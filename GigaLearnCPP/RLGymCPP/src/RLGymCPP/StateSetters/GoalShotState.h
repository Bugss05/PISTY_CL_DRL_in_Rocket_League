#pragma once
#include "StateSetter.h"
#include <atomic>

extern std::atomic<int> g_totalEpisodes;
extern std::atomic<int> g_totalGoals;
extern std::atomic<float> g_currentRadius;

namespace RLGC {
	class GoalShotState : public StateSetter {
	public:
		GoalShotState() {}

		virtual void ResetArena(Arena* arena) override;
	};
}