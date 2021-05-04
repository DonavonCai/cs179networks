#include "fastScan.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("FastScanAlgorithm");

NS_OBJECT_ENSURE_REGISTERED (FastScanAlgorithm);

FastScanAlgorithm::FastScanAlgorithm ( const videoData &videoData,
                                       const playbackData &playbackData,
                                       const bufferData &bufferData,
                                       const throughputData &throughput) :
  AdaptationAlgorithm (videoData, playbackData, bufferData, throughput),
  m_highestRepIndex (videoData.averageBitrate.size () - 1),
  m_W (30),
  m_S (-1),
  m_C (m_W)
{
  NS_LOG_INFO (this);
  NS_ASSERT_MSG (m_highestRepIndex >= 0, "highestRepIndex < 0");

  for (unsigned int i = 0; i < videoData.segmentSize[0].size(); i++)
  {
    m_n.push_back (0); // Initialize decisions to 0
    m_d.push_back (0); // Initialize stall time to 0
    m_bandwidth.push_back (0); // Initialize bandwidth to 0
    m_remainingBandwidth.push_back (0);
    m_timeslots.push_back (0);
  }

  // Initialize deadlines
  m_deadlines.push_back(0);
  for (unsigned int i = 1; i < videoData.segmentSize[0].size(); i++)
  {
    m_deadlines.push_back((i - 1) * videoData.segmentDuration); // deadline(i) = s + (i - 1)L, add s later, when we have the startup time
  }
}

// Returns total stall time in microseconds for all segments that have been played so far
int64_t
FastScanAlgorithm::GetTotalStallTime ()
{
  const int64_t duration = m_videoData.segmentDuration;
  
  int64_t totalStall = 0;
  int64_t firstSegTime, secondSegTime;
  for (unsigned int i = 0; i < m_playbackData.playbackStart.size () - 1; i++)
    {
      firstSegTime = m_playbackData.playbackStart.at (i);
      secondSegTime = m_playbackData.playbackStart.at (i + 1);

      if (firstSegTime + duration < secondSegTime)
        {
          totalStall += (secondSegTime - (firstSegTime + duration));
        }
    }

  return totalStall;
}

// output: d(i) stall time of chunk i
int64_t
FastScanAlgorithm::Level0Forward (const int64_t i)
{
  // s = startup time + total stall time so far
  int64_t s = m_S + GetTotalStallTime();

  // bf = total buffer length right now
  //int64_t bf = m_bufferData.bufferLevelNew.back();

  // while chunk C is not fetched
    if (i > 1 && m_d.at (i) < m_d.at (i - 1)) 
      {
        // Assume minimum stall time for i = stall time for i - 1
        m_d.at (i) = m_d.at (i - 1);
        // Then deadline for i = total playtime before i + total stall time before i + stall time for i
        m_deadlines.at (i) = (i - 1) * (m_videoData.segmentDuration) + s + m_d.at (i);
      }
    //if ()

  return 0;
}

void
FastScanAlgorithm::Level0Backward()
{

}

void
FastScanAlgorithm::LevelNForward()
{

}

void
FastScanAlgorithm::LevelNBackward()
{

}

algorithmReply
FastScanAlgorithm::GetNextRep (const int64_t segmentCounter, int64_t clientId) 
{ 
  //const int64_t timeNow = Simulator::Now ().GetMicroSeconds ();
  algorithmReply answer;
  answer.nextRepIndex = 0;
  answer.nextDownloadDelay = 0;
  answer.decisionTime = Simulator::Now ().GetMicroSeconds ();
  answer.decisionCase = 0;
  answer.delayDecisionCase = 0;

  // Initialize
  if (segmentCounter == 0) 
  {
    return answer;
  }

  m_C = segmentCounter + m_W;

  if(m_playbackData.playbackIndex.size() == 1)
  {
    // Get startup time if we haven't already
    if (m_S == -1)
    {
      m_S = m_playbackData.playbackStart.at(0) - m_throughput.transmissionRequested.at(0);
      // Add startup time to deadlines
      for (unsigned int i = 0; i < m_deadlines.size(); i++)
      {
        m_deadlines.at (i) = m_deadlines.at (i) + m_S;
      }
    }
  }
  // By now startup time should be known.
  // Find c(j)
  int64_t sumOfBandwidth = 0;
  for (unsigned int i = 0; i < m_bandwidth.size(); i++)
  {
    sumOfBandwidth += m_bandwidth.at (i);
  }

  // Level 0 forward
  m_n.at (segmentCounter) = 0;
  m_deadlines.at (segmentCounter) = Level0Forward(segmentCounter);

  // For every quality level, level n forward and backward
  return answer;
}
}

