#include "newAdaptationAlgorithm.h"

namespace ns3 {
    NS_LOG_COMPONENT_DEFINE("newAdaptationAlgorithm");

    NS_OBJECT_ENSURE_REGISTERED(newAdaptationAlgorithm);

    newAdaptationAlgorithm::newAdaptationAlgorithm(const videoData& videoData,
        const playbackData& playbackData,
        const bufferData& bufferData,
        const throughputData& throughput) :
        AdaptationAlgorithm(videoData, playbackData, bufferData, throughput),
        m_highestRepIndex(videoData.averageBitrate.size() - 1)
    {
        NS_LOG_INFO(this);
        NS_ASSERT_MSG(m_highestRepIndex >= 0, "highestRepIndex < 0");
    }

    algorithmReply newAdaptationAlgorithm::GetNextRep(const int64_t segmentCounter, int64_t clientId)
    {
        int64_t timeNow = Simulator::Now().GetMicroSeconds();
        algorithmReply answer;
        answer.decisionTime = timeNow;
        answer.nextDownloadDelay = 0;
        answer.delayDecisionCase = 0;
        if (segmentCounter == 0)
        {
            answer.nextRepIndex = 0;
            answer.decisionCase = 0;
            return answer;
        }
        if (m_throughput.transmissionEnd.size() > 0)
        {
            int64_t bufferNow = m_bufferData.bufferLevelNew.back() - (timeNow - m_throughput.transmissionEnd.back());
            double sft = 1.0 * (m_throughput.transmissionEnd.back() - m_throughput.transmissionRequested.back());
            double mi = double(1.0 * m_videoData.segmentDuration) / sft;
            double epsilon = 0.0;
            int i;
            int rates_size = m_videoData.averageBitrate.size();
            for (i = 0; i < rates_size - 1; i++)
            {
                epsilon = std::max(epsilon, (m_videoData.averageBitrate[i + 1] - m_videoData.averageBitrate[i]) / m_videoData.averageBitrate[i]);
            }          
            int cur_quality = m_lastVideoIndex;
            int rateInd = m_lastVideoIndex;
            int nextRepIndex = m_lastVideoIndex;
            if (mi > 1 + epsilon) // Switch Up
            {
                if (rateInd < rates_size - 1)
                {
                    nextRepIndex = rateInd + 1;
                }
            }
            else if (mi < gamma_d_) // Switch down
            {
                bool found = false;
                for (i = cur_quality - 1; i >= 0; i--)
                {
                    if (m_videoData.averageBitrate[i] < mi * m_videoData.averageBitrate[m_lastVideoIndex])
                    {
                        nextRepIndex = i;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    nextRepIndex = 0;
                }
            }
            double temp = m_videoData.averageBitrate.at(m_lastVideoIndex) * (m_videoData.segmentDuration) / m_videoData.averageBitrate[0];
            int64_t delay = bufferNow - t_min_ms_ - (int)temp;
            if (delay > 0)
            {
                answer.nextDownloadDelay = delay;
            }
            answer.nextRepIndex = nextRepIndex;
        }
        answer.decisionCase = 1;
        return answer;
    }
}
