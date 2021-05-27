/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2016 Technische Universitaet Berlin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include "ns3/mmwave-helper.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"

#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include <ns3/buildings-module.h>
#include "ns3/building-position-allocator.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store.h"

#include "ns3/tcp-stream-helper.h"
#include "ns3/tcp-stream-interface.h"

using namespace ns3;
using namespace mmwave;

template <typename T>
std::string ToString(T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}


NS_LOG_COMPONENT_DEFINE ("LteTcpStreamExample");

static void
CourseChange (std::string context, Ptr<const MobilityModel> mobility)
{
  Vector pos = mobility->GetPosition ();
  Vector vel = mobility->GetVelocity ();
  std::cout << Simulator::Now () << ", model=" << mobility << ", POS: x=" << pos.x << ", y=" << pos.y
            << ", z=" << pos.z << "; VEL:" << vel.x << ", y=" << vel.y
            << ", z=" << vel.z << std::endl;
}

/* 
 * FIXME: ServerApp and ClientApps start, but run slowly.
 * Multiple clients doesn't seem to run correctly. Not sure what's wrong.
 */

int
main (int argc, char *argv[])
{
   const double MIN_DISTANCE = 10.0; // eNB-UE distance in meters
   const double MAX_DISTANCE = 150.0; // eNB-UE distance in meters
   const double simTime = 120.0; // time in seconds to run simulation for
   const double segmentSize = 1446;
//   double interPacketInterval = 100;

   LogComponentEnable ("LteTcpStreamExample", LOG_LEVEL_INFO);
//   LogComponentEnable ("MmWaveHelper", LOG_LEVEL_ALL);
//   LogComponentEnable ("TcpStreamClientApplication", LOG_LEVEL_ALL);
//   LogComponentEnable ("TcpStreamServerApplication", LOG_LEVEL_ALL);

//   LogComponentEnable ("PacketSink", LOG_LEVEL_ALL);
//   LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);

//   LogComponentEnable ("MobilityHelper", LOG_LEVEL_ALL);
//   LogComponentEnable ("RandomDirection2dMobilityModel", LOG_LEVEL_ALL);

   // Get input
   uint64_t segmentDuration = 2000000;
   uint32_t simulationId = 1;
   uint32_t numberOfClients = 1;
   std::string adaptationAlgo;
   std::string segmentSizeFilePath = "contrib/dash/segmentSizes.txt";

   CommandLine cmd;
   cmd.Usage ("Simulation of streaming with DASH.\n");
   cmd.AddValue ("adaptationAlgo", "The adaptation algorithm that the client uses for the simulation", adaptationAlgo);
   cmd.Parse (argc, argv);

   Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (segmentSize));
   Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (524288));
   Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (524288));

   NS_LOG_INFO ("Creating mmWaveHelper.");
   Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper> ();
   mmwaveHelper->SetSchedulerType ("ns3::MmWaveFlexTtiMacScheduler");
   Ptr<MmWavePointToPointEpcHelper>  epcHelper = CreateObject<MmWavePointToPointEpcHelper> ();
   mmwaveHelper->SetEpcHelper (epcHelper);
   mmwaveHelper->SetHarqEnabled (true/*harqEnabled*/);

   NS_LOG_INFO ("Creating RemoteHost.");
   // Create a RemoteHost
   NodeContainer remoteHostContainer;
   remoteHostContainer.Create (1);
   Ptr<Node> remoteHost = remoteHostContainer.Get (0);

   NS_LOG_INFO ("Creating InternetStackHelper.");
   // Install internet stack onto remoteHost
   // We need to do this before doing Ipv4AddressHelper::Assign()
   InternetStackHelper internet;
   internet.Install (remoteHostContainer);

   // Create pgw. This connects the UEs to the RemoteHost.
   Ptr<Node> pgw = epcHelper->GetPgwNode ();

   NS_LOG_INFO ("Creating P2PHelper.");
   // P2P helper
   PointToPointHelper p2p;
   p2p.SetDeviceAttribute ("DataRate", StringValue ("100Gb/s"));
   p2p.SetDeviceAttribute ("Mtu", UintegerValue (segmentSize));
   p2p.SetChannelAttribute ("Delay", StringValue ("45ms"));

   // Connect pgw and remoteHost
   NetDeviceContainer internetDevices = p2p.Install (pgw, remoteHost);

   NS_LOG_INFO ("Configuring Ipv4.");
   // Configure Ipv4
   Ipv4AddressHelper ipv4h;
   ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
   // Interface
   Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
   // interface 0 is localhost, 1 is the p2p device
   Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

   NS_LOG_INFO ("Routing.");
   // Set up routing
   // TODO: check if this works correctly
   Ipv4StaticRoutingHelper ipv4RoutingHelper;
   Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
   remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);


   NS_LOG_INFO ("Setting up eNBs and UEs.");
   // Create UEs and eNBs
   NodeContainer enbNodes;
   NodeContainer ueNodes;
   // create 1 enb for now
   enbNodes.Create (1/*numEnb*/);
   ueNodes.Create (numberOfClients);

   // Install eNB mobility model (eNB's don't move)
   Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator> ();
   enbPositionAlloc->Add (Vector (0.0, 0.0, 0.0));

   MobilityHelper enbmobility;
   enbmobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
   enbmobility.SetPositionAllocator (enbPositionAlloc);
   enbmobility.Install (enbNodes);

   // Install UE mobility model (I think ue's are randomly placed and static)
   MobilityHelper uemobility;
   Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator> ();
   Ptr<UniformRandomVariable> distRv = CreateObject<UniformRandomVariable> ();

   // We have 3 UE's for now
   for (unsigned i = 0; i < numberOfClients; i++)
     {
       double dist = distRv->GetValue (MIN_DISTANCE, MAX_DISTANCE);
       uePositionAlloc->Add (Vector (dist, 0.0, 0.0));
     }
//   uemobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
   uemobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue ("Distance"),
                              "Distance", StringValue ("100.0"),
                              "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=50.0]"),
                              "Bounds", StringValue ("0|200|0|200"));

   uemobility.SetPositionAllocator (uePositionAlloc);
   uemobility.Install (ueNodes);

   MobilityHelper remoteHostMobility;
   remoteHostMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
   remoteHostMobility.Install (remoteHost); // added this

   NS_LOG_INFO ("Installing mmWave devices.");
   // Install mmWave Devices to the nodes
   NetDeviceContainer uemmWaveDevs = mmwaveHelper->InstallUeDevice (ueNodes);
   NetDeviceContainer enbmmWaveDevs = mmwaveHelper->InstallEnbDevice (enbNodes);

   // Install IP stack onto UE's
   internet.Install (ueNodes);
   Ipv4InterfaceContainer ueIpIface;
   ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (uemmWaveDevs));

   NS_LOG_INFO ("Assigning IP addresses.");
   // Assign IP address to UEs, and install applications
   for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
     {
       Ptr<Node> ueNode = ueNodes.Get (u);
       // Set the default gateway for the UE
       Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
       ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
     }

   NS_LOG_INFO ("Attaching UEs to eNBs.");
   mmwaveHelper->AttachToClosestEnb (uemmWaveDevs, enbmmWaveDevs);

   NS_LOG_INFO ("Creating folder for log files.");
   // create folder so we can log the positions of the clients
   const char * mylogsDir = dashLogDirectory.c_str();
   mkdir (mylogsDir, 0775);
   std::string algodirstr (dashLogDirectory +  adaptationAlgo );
   const char * algodir = algodirstr.c_str();
   mkdir (algodir, 0775);
   std::string dirstr (dashLogDirectory + adaptationAlgo + "/" + ToString (numberOfClients) + "/");
   const char * dir = dirstr.c_str();
   mkdir(dir, 0775);


   std::ofstream clientPosLog;
   std::string clientPos = dashLogDirectory + "/" + adaptationAlgo + "/" + ToString (numberOfClients) + "/" + "sim" + ToString (simulationId) + "_"  + "clientPos.txt";
   clientPosLog.open (clientPos.c_str());
   NS_ASSERT_MSG (clientPosLog.is_open(), "Couldn't open clientPosLog file");


   // Do we need to log client positions for the sim to run?
   // Log client positions
   for (uint i = 0; i < numberOfClients; i++)
     {
	   Vector pos = Vector (uePositionAlloc->GetNext());
	   // log client positions
	   clientPosLog << ToString(pos.x) << ", " << ToString(pos.y) << ", " << ToString(pos.z) << "\n";
	   clientPosLog.flush ();
     }

   ApplicationContainer serverApp;
   ApplicationContainer clientApps;

   NS_LOG_INFO ("Installing server application on remote host.");
   uint16_t port = 2000;
   // Install TCP Receiver on the remote host
   // dash:
   TcpStreamServerHelper serverHelper (port);

   // basic ns-3:
//   PacketSinkHelper serverHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));

  serverApp = serverHelper.Install (remoteHost);

  serverApp.Start (Seconds (0.1));

   NS_LOG_INFO("Installing client applications on UE nodes.");

   // Installing applications on UE nodes
   // dash:
   TcpStreamClientHelper clientHelper (remoteHostAddr, port);
   clientHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
   clientHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
   clientHelper.SetAttribute ("NumberOfClients", UintegerValue(numberOfClients));
   clientHelper.SetAttribute ("SimulationId", UintegerValue (simulationId));

   // Determine client nodes for object creation with client helper class
   std::vector <std::pair <Ptr<Node>, std::string> > clients;
   for (NodeContainer::Iterator i = ueNodes.Begin (); i != ueNodes.End (); ++i)
     {
       std::pair <Ptr<Node>, std::string> client (*i, adaptationAlgo);
       clients.push_back (client);
     }
   clientApps = clientHelper.Install (clients);

   // basic ns-3:
//   UdpClientHelper clientHelper (remoteHostAddr, port);
//   clientHelper.SetAttribute ("Interval", TimeValue (MicroSeconds (100)));
//   clientHelper.SetAttribute ("MaxPackets", UintegerValue (1000));
//   clientApps = clientHelper.Install(ueNodes);

   clientApps.Start (Seconds (1.0));
   /*
   for (uint i = 0; i < clientApps.GetN (); i++)
     {
       double startTime = 0.2 + ((i * 3) / 100.0);
       clientApps.Get (i)->SetStartTime (Seconds (startTime));
     }
   */
   //mmwaveHelper->EnableTraces ();
   // Uncomment to enable PCAP tracing
   //p2p.EnablePcapAll ("lte-dash");

   Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
                    MakeCallback (&CourseChange));

   NS_LOG_INFO ("Run Simulation.");
   //NS_LOG_INFO ("Sim: " << simulationId << "Clients: " << numberOfClients);
   Simulator::Stop (Seconds (simTime));
   Simulator::Run ();
   Simulator::Destroy ();
   NS_LOG_INFO ("Done.");
   return 0;
}
