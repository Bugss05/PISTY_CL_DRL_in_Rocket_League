#pragma once
#include "../Gamestates/GameState.h"
#include <string>

namespace RLGC {
	class StateSetter {
	public:
		virtual void ResetArena(Arena* arena) = 0;

		// Optional runtime-tunable parameters. State setters that accept constructor
		// arguments (e.g. GoalShotState's radius) override this so the Scheduler can
		// change them per training phase. Unknown keys are ignored; setters without
		// parameters keep the default no-op.
		virtual void SetParam(const std::string& key, float value) {}
	};
}