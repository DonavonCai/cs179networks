#include "newAdaptationAlgorithm.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("newAdaptationAlgorithm");

NS_OBJECT_ENSURE_REGISTERED (newAdaptationAlgorithm);

newAdaptationAlgorithm::newAdaptationAlgorithm (  const videoData &videoData,
                                  const playbackData & playbackData,
                                  const bufferData & bufferData,
                                  const throughputData & throughput) :
  AdaptationAlgorithm (videoData, playbackData, bufferData, throughput),
  m_highestRepIndex (videoData.averageBitrate.size () - 1)
{
  NS_LOG_INFO (this);
  NS_ASSERT_MSG (m_highestRepIndex >= 0, "highestRepIndex < 0");
}

algorithmReply
newAdaptationAlgorithm::GetNextRep (const int64_t segmentCounter, int64_t clientId) 
{
  algorithmReply answer;
  answer.nextRepIndex = 1;
  answer.nextDownloadDelay = 0;
  answer.decisionTime = Simulator::Now ().GetMicroSeconds ();
  answer.decisionCase = 0;
  answer.delayDecisionCase = 0;
  return answer;
}
}

