#pragma once
#include "StateSetter.h"

namespace RLGC {
	class GoalShotState : public StateSetter {
	public:
		float radius;
		GoalShotState(float radius = 1000.f) : radius(radius) {}

		virtual void ResetArena(Arena* arena) override;
	};
}