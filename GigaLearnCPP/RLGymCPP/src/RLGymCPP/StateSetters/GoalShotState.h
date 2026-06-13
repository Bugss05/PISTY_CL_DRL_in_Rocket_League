#pragma once
#include "StateSetter.h"

namespace RLGC {
	class GoalShotState : public StateSetter {
	public:
		float radius;
		GoalShotState(float radius = 1000.f) : radius(radius) {}

		// Schedulable param: "radius" (same units as the constructor).
		virtual void SetParam(const std::string& key, float value) override {
			if (key == "radius") radius = value;
		}

		virtual void ResetArena(Arena* arena) override;
	};
}