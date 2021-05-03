#ifndef NEW_ALGORITHM_H
#define NEW_ALGORITHM_H

#include "tcp-stream-adaptation-algorithm.h"

namespace ns3 {
	/**
	 * \ingroup tcpStream
	 * \brief Implementation of a new adaptation algorithm
	 */
	class newAdaptationAlgorithm : public AdaptationAlgorithm
	{
	public:

		newAdaptationAlgorithm(const videoData& videoData,
			const playbackData& playbackData,
			const bufferData& bufferData,
			const throughputData& throughput);

		algorithmReply GetNextRep(const int64_t segmentCounter, const int64_t clientId);

	private:
		const int64_t m_highestRepIndex;
		int t_min_ms_ = 9000;
		int64_t m_lastVideoIndex;
		double gamma_d_ = 0.67; //Switch down factor
	};
} // namespace ns3
#endif /* NEW_ALGORITHM_H */
