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
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include <ns3/buildings-module.h>
#include "ns3/building-position-allocator.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-stream-helper.h"
#include "ns3/tcp-stream-interface.h"
#include "ns3/mmwave-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/config-store.h"
#include "ns3/mmwave-point-to-point-epc-helper.h"

template <typename T>
std::string ToString(T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}

using namespace ns3;
using namespace mmwave;

NS_LOG_COMPONENT_DEFINE ("LteTcpStreamExample");

/* 
 * This code runs, but no log files are generated
 * I think we need to check for the following reasons:
 * 
 * routing table implementation (used the one in mmwave-simple-epc instead of tcp-stream)
 * dash implementation may not work with the mobility model (might require a building?)
 * are the ports set correctly?
 * TODO: replace UdpHelpers with TcpStreamHelpers (currently have both)
 *    This is probably why it's not working but it's also 2am so i'll do this tomorrow
 */

int
main (int argc, char *argv[])
{
   const double MIN_DISTANCE = 10.0; // eNB-UE distance in meters
   const double MAX_DISTANCE = 150.0; // eNB-UE distance in meters
   double simTime = 1.0;
   double interPacketInterval = 100;

   LogComponentEnable ("LteTcpStreamExample", LOG_LEVEL_INFO);
   LogComponentEnable ("TcpStreamClientApplication", LOG_LEVEL_INFO);
   LogComponentEnable ("TcpStreamServerApplication", LOG_LEVEL_INFO);

   // Get input
   uint64_t segmentDuration;
   uint32_t simulationId;
   uint32_t numberOfClients;
   std::string adaptationAlgo;
   std::string segmentSizeFilePath;

   CommandLine cmd;
   cmd.Usage ("Simulation of streaming with DASH.\n");
   cmd.AddValue ("simulationId", "The simulation's index (for logging purposes)", simulationId);
   cmd.AddValue ("numberOfClients", "The number of clients", numberOfClients);
   cmd.AddValue ("segmentDuration", "The duration of a video segment in microseconds", segmentDuration);
   cmd.AddValue ("adaptationAlgo", "The adaptation algorithm that the client uses for the simulation", adaptationAlgo);
   cmd.AddValue ("segmentSizeFile", "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes", segmentSizeFilePath);
   cmd.Parse (argc, argv);

   Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
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
   p2p.SetDeviceAttribute ("DataRate", StringValue ("100000kb/s")); // This must not be more than the maximum throughput in 802.11n
   p2p.SetDeviceAttribute ("Mtu", UintegerValue (1500));
   p2p.SetChannelAttribute ("Delay", StringValue ("45ms"));

   // Connect pgw and remoteHost
   NetDeviceContainer internetDevices = p2p.Install (pgw, remoteHost);

   NS_LOG_INFO ("Configuring Ipv4.");
   // Configure Ipv4
   Ipv4AddressHelper ipv4h;
   ipv4h.SetBase ("76.1.1.0", "255.255.255.0");
   // Interface
   Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
   // interface 0 is localhost, 1 is the p2p device
   Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

   NS_LOG_INFO ("Routing.");
   // Set up routing
   // TODO: check if this works correctly
   // mmwave-simple-epc.cc routing:
   Ipv4StaticRoutingHelper ipv4RoutingHelper;
   Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
   remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

   // tcp-stream.cc routing: (this produces error for some reason)
//   Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
//   uint16_t port = 9;

   NS_LOG_INFO ("Setting up eNBs and UEs.");
   // Create UEs and eNBs
   NodeContainer enbNodes;
   NodeContainer ueNodes;
   // create 1 enb and for now
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
//   uePositionAlloc->Add (Vector (1.5, 1.5, 1.5));
   Ptr<UniformRandomVariable> distRv = CreateObject<UniformRandomVariable> ();

   // We have 3 UE's for now
   for (unsigned i = 0; i < numberOfClients; i++)
     {
       double dist = distRv->GetValue (MIN_DISTANCE, MAX_DISTANCE);
       uePositionAlloc->Add (Vector (dist, 0.0, 0.0));
     }
   uemobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
   uemobility.SetPositionAllocator (uePositionAlloc);
   uemobility.Install (ueNodes);

   uemobility.Install (remoteHost); // added this

   NS_LOG_INFO ("Installing mmWave devices.");
   // Install mmWave Devices to the nodes
   NetDeviceContainer uemmWaveDevs = mmwaveHelper->InstallUeDevice (ueNodes);
   NetDeviceContainer enbmmWaveDevs = mmwaveHelper->InstallEnbDevice (enbNodes);

   // Install IP stack onto UE's
   internet.Install (ueNodes);
   Ipv4InterfaceContainer ueIpIface;
   ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (uemmWaveDevs));

   NS_LOG_INFO ("Assigning IP addresses, installing applications.");
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

/*
   std::ofstream clientPosLog;
   std::string clientPos = dashLogDirectory + "/" + adaptationAlgo + "/" + ToString (numberOfClients) + "/" + "sim" + ToString (simulationId) + "_"  + "clientPos.txt";
   clientPosLog.open (clientPos.c_str());
   NS_ASSERT_MSG (clientPosLog.is_open(), "Couldn't open clientPosLog file");
   //////////////////////////////////////////////////////////////////////////////////////////////////
   //// Set up Building
   //////////////////////////////////////////////////////////////////////////////////////////////////
     double roomHeight = 3;
     double roomLength = 6;
     double roomWidth = 5;
     uint32_t xRooms = 8;
     uint32_t yRooms = 3;
     uint32_t nFloors = 6;

     Ptr<Building> b = CreateObject <Building> ();
     b->SetBoundaries (Box ( 0.0, xRooms * roomWidth,
                             0.0, yRooms * roomLength,
                             0.0, nFloors * roomHeight));
     b->SetBuildingType (Building::Office);
     b->SetExtWallsType (Building::ConcreteWithWindows);
     b->SetNFloors (6);
     b->SetNRoomsX (8);
     b->SetNRoomsY (3);

     BuildingsHelper::Install (enbNodes);
     BuildingsHelper::Install (ueNodes);
     BuildingsHelper::Install (remoteHost);
*/
     /*
     Vector posAp = Vector ( 1.0, 1.0, 1.0);
     // give the server node any position, it does not have influence on the simulation, it has to be set though,
     // because when we do: mobility.Install (networkNodes);, there has to be a position as place holder for the server
     // because otherwise the first client would not get assigned the desired position.
     Vector posServer = Vector (1.5, 1.5, 1.5);

     // Set up positions of nodes (AP and server)
     Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
     positionAlloc->Add (posAp);
     positionAlloc->Add (posServer);
     */
/*
     Ptr<RandomRoomPositionAllocator> randPosAlloc = CreateObject<RandomRoomPositionAllocator> ();
     randPosAlloc->AssignStreams (simulationId);

     // FIXME: Do we need to log client positions for the sim to run?
     // allocate clients to positions
     for (uint i = 0; i < numberOfClients; i++)
       {
         Vector pos = Vector (randPosAlloc->GetNext());
         uePositionAlloc->Add (pos);

         // log client positions
         clientPosLog << ToString(pos.x) << ", " << ToString(pos.y) << ", " << ToString(pos.z) << "\n";
         clientPosLog.flush ();
       }
*/
   NS_LOG_INFO ("Installing applications on UEs and remoteHost.");
   // Install and start applications on UEs and remote host
   uint16_t dlPort = 1234;
   uint16_t ulPort = 2000;
   uint16_t otherPort = 3000;
   ApplicationContainer clientApps;
   ApplicationContainer serverApps;


   for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
     {
       ++ulPort;
       ++otherPort;
       PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
       PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
       PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), otherPort));
       serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get (u)));
       serverApps.Add (ulPacketSinkHelper.Install (remoteHost));
       serverApps.Add (packetSinkHelper.Install (ueNodes.Get (u)));

       UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
       dlClient.SetAttribute ("Interval", TimeValue (MicroSeconds (interPacketInterval)));
       dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000));

       UdpClientHelper ulClient (remoteHostAddr, ulPort);
       ulClient.SetAttribute ("Interval", TimeValue (MicroSeconds (interPacketInterval)));
       ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000));

       clientApps.Add (dlClient.Install (remoteHost));
       clientApps.Add (ulClient.Install (ueNodes.Get(u)));
     }
   serverApps.Start (Seconds (0.1));
//   clientApps.Start (Seconds (0.1));
//   mmwaveHelper->EnableTraces ();
   // Uncomment to enable PCAP tracing
//   p2p.EnablePcapAll ("mmwave-epc-simple");

   /* Install TCP Receiver on the access point */
   TcpStreamServerHelper serverHelper (ulPort/*port*/);
   ApplicationContainer serverApp = serverHelper.Install (remoteHost);
   serverApp.Start (Seconds (0.1));
//   clientApps.Start (Seconds (0.1));
   /* Install TCP/UDP Transmitter on the station */
   TcpStreamClientHelper clientHelper (remoteHostAddr, ulPort);
   clientHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
   clientHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
   clientHelper.SetAttribute ("NumberOfClients", UintegerValue(numberOfClients));
   clientHelper.SetAttribute ("SimulationId", UintegerValue (simulationId));

   for (uint i = 0; i < ueNodes.GetN (); i++)
     {
       double startTime = 2.0 + ((i * 3) / 100.0);
       clientApps.Get (i)->SetStartTime (Seconds (startTime));
     }

   NS_LOG_INFO ("Run Simulation.");
   Simulator::Stop (Seconds (simTime + 30.0));
   //NS_LOG_INFO ("Sim: " << simulationId << "Clients: " << numberOfClients);
   Simulator::Run ();
   Simulator::Destroy ();
   NS_LOG_INFO ("Done.");
   return 0;
}
