#ifndef FASTSCAN_H
#define FASTSCAN_H

#include "tcp-stream-adaptation-algorithm.h"

namespace ns3 {
/**
 * \ingroup tcpStream
 * \brief Implementation of a new adaptation algorithm
 */
class FastScanAlgorithm : public AdaptationAlgorithm
{
public:

FastScanAlgorithm ( const videoData &videoData,
                    const playbackData &playbackData,
			              const bufferData &bufferData,
			              const throughputData &throughput );

algorithmReply GetNextRep ( const int64_t segmentCounter, const int64_t clientId );

private:
int64_t Level0Forward(const int64_t);
void Level0Backward();
void LevelNFoward();
void LevelNBackward();
void LevelNForward();
int64_t GetTotalStallTime();
const int64_t m_highestRepIndex;
const int64_t m_W; // Size of window
int64_t m_S; // Time it takes from requesting seg 0 to playing seg 0
int64_t m_C; // Index of last segment in window
std::vector<int64_t> m_n; // Quality level for each segment
std::vector<int64_t> m_d; // Total stall time before segment i played, ONLY for the current window (d for delay)
std::vector<int64_t> m_deadlines; // How many more microseconds we have to download segment without stalling
std::vector<int64_t> m_bandwidth; // Bandwidth up to segment i
std::vector<int64_t> m_remainingBandwidth; // Remaining bandwidth after after all non-skipped segments are fetched
std::vector<int64_t> m_timeslots; // First timeslots in which segment i can be fetched
};
} // namespace ns3
#endif /* NEW_ALGORITHM_H */
