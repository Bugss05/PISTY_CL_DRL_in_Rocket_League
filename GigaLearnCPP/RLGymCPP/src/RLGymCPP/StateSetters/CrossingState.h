#pragma once
#include "StateSetter.h"

namespace RLGC {
	// "Crossing": HIGH ARCING ball coming from a wing toward the front zone of the
	// opponent's goal, forcing the attacker to prepare for an AERIAL.
	//
	// Different from CrossState (low/mid-height cross glued to the wall):
	// here the ball rises (high velZ) and arcs inward.
	//
	// FIXED ROLES, embedded in the state itself (no DefenderState needed):
	//   BLUE   = ATTACKER -> PERPENDICULAR to the ball's XY trajectory (>=100 uu from it),
	//            in the half of the field FARTHER from the goal, facing point P of the
	//            trajectory (±45°), full boost.
	//   ORANGE = DEFENDER -> anywhere within <=300 uu of its own goal wall (+Y),
	//            facing the center of the field.
	//
	// Convention: BLUE attacks the ORANGE goal (+Y, at Y=+5120).
	//
	// Schedulable parameters (SetParam): "minHeight"/"maxHeight" (ball's initial Z),
	// "minSpeed"/"maxSpeed" (horizontal speed of the cross).
	class CrossingState : public StateSetter {
	public:
		float minHeight, maxHeight;   // ball's initial Z (uu)
		float minSpeed, maxSpeed;     // horizontal speed of the cross (uu/s)

		CrossingState(float minHeight = 300.f, float maxHeight = 700.f,
		              float minSpeed = 1000.f, float maxSpeed = 1700.f)
			: minHeight(minHeight), maxHeight(maxHeight),
			  minSpeed(minSpeed), maxSpeed(maxSpeed) {}

		void SetParam(const std::string& key, float value) override {
			if      (key == "minHeight") minHeight = value;
			else if (key == "maxHeight") maxHeight = value;
			else if (key == "minSpeed")  minSpeed  = value;
			else if (key == "maxSpeed")  maxSpeed  = value;
		}

		void ResetArena(Arena* arena) override;
	};
}
