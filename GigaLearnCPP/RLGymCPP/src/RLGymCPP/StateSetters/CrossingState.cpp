#include "CrossingState.h"
#include "../Math.h"
#include <cmath>

using RocketSim::Math::RandFloat;

void RLGC::CrossingState::ResetArena(Arena* arena) {
	arena->ResetToRandomKickoff();

	// Wing side the cross comes from: +1 = right wall, -1 = left wall.
	float side = (RandFloat(0.f, 1.f) > 0.5f) ? 1.f : -1.f;

	// ---------------------------------------------------------------------
	// BALL ZONE (BLUE attacks +Y; ORANGE goal at Y=+5120)
	//   X: on the wing, near the side wall (±4096) but not touching
	//   Y: enters the opponent's half, with room to arc forward
	//   Z: already in the air (parametrizable via minHeight/maxHeight)
	// ---------------------------------------------------------------------
	float ballX = side * RandFloat(2400.f, 3700.f);
	float ballY = RandFloat(3000.f, 4000.f);
	float ballZ = RandFloat(minHeight, maxHeight);

	// ---------------------------------------------------------------------
	// HORIZONTAL DIRECTION OF THE CROSS: velocity angle = angle of the
	// ball->goal vector + an offset. At one extreme (offset 0) the ball goes STRAIGHT to the goal;
	// at the other (offset 90°) it leaves PERPENDICULAR to the ball->goal vector, but always
	// rotated AWAY from the wall (toward the field interior), never against the wall.
	//
	// The rotation direction depends on the side the cross comes from (side):
	//   right wing (side=+1, goes from right to left): rotates 0..+90°
	//   left wing (side=-1, goes from left to right): rotates 0..-90°
	// ARC (vertical): high velZ -> the ball rises and falls = high ball that demands an aerial.
	// ---------------------------------------------------------------------
	float horizSpeed = RandFloat(minSpeed, maxSpeed/1.5f); // horizontal speed of the cross (parametrizable via minSpeed/maxSpeed)

	Vec goal = Vec(0.f, 5120.f, 0.f); // ORANGE goal (BLUE's target)
	float goalAng = atan2f(goal.y - ballY, goal.x - ballX); // angle of the ball->goal vector (XY)

	constexpr float MAX_ANG = (float)M_PI / 3.f;
	float velAng = goalAng + side * RandFloat(0.f, MAX_ANG); // 0..±60°, always away from the wall

	float velX = cosf(velAng) * horizSpeed;
	float velY = sinf(velAng) * horizSpeed;
	float velZ = RandFloat(650.f, 1050.f);  // the ARC (rises first)

	{
		BallState bs = {};
		bs.pos    = Vec(ballX, ballY, ballZ);
		bs.vel    = Vec(velX, velY, velZ);
		bs.angVel = Vec(RandFloat(-2.f, 2.f), RandFloat(-2.f, 2.f), RandFloat(-2.f, 2.f));
		arena->ball->SetState(bs);
	}

	// ---------------------------------------------------------------------
	// FIXED ROLES: BLUE = ATTACKER, ORANGE = DEFENDER (goalkeeper).
	//   BLUE  -> attacking position (front box OR midfield), random rotation,
	//            full boost. Will contest the cross.
	//   ORANGE-> goalkeeper next to the +Y goal it defends, facing the field (-Y),
	//            limited boost (realistic).
	// ---------------------------------------------------------------------
	for (Car* car : arena->_cars) {
		CarState cs = {};

		if (car->team == Team::BLUE) {
			// ===== ATTACKER POSITION (5 steps) =====
			// 1) ball->goal vector (normalized)
			Vec v1 = Vec(goal.x - ballX, goal.y - ballY, 0.f).Normalized();

			// 2) ball's horizontal velocity vector (X,Y)
			Vec v2 = Vec(velX, velY, 0.f);
			Vec v2dir = v2.Normalized();

			// STABLE reference of "inside the field": vector from the ball to the CENTER.
			// Does not depend on the ball's direction -> does NOT flip if the ball goes toward our goal.
			Vec toCenter = Vec(-ballX, -ballY, 0.f);

			// 3) v2 rotated 90°, chosen toward the CENTER of the field (interior).
			Vec v3 = Vec(-v2dir.y, v2dir.x, 0.f);          // perpendicular to v2
			if (v3.Dot(toCenter) < 0.f) v3 = v3 * -1.f;    // ensures it points inward

			// 4) RECTANGLE of possible zones from the ball: offset along
			//    v2 (ball direction) and v3 (perpendicular inward). Min/max scaled
			//    with the ball speed; the v3 one is SMALLER than the v2 one.
			float speedScale = RS_CLAMP(horizSpeed / maxSpeed, 0.f, 1.f);
			float offV2 = RandFloat(1700.f, 2300.f);   // along the ball direction
			float offV3 = RandFloat(1000.f, 1500.f);   // perpendicular inward

			cs.pos = Vec(ballX, ballY, 17.f) + (v2dir * offV2 + v3 * offV3);

			// 5) ORIENTATION = v1 (ball->goal) rotated 90° INWARD into the field (depending on side)
			Vec ori = Vec(-v1.y, v1.x, 0.f);
			if (ori.Dot(toCenter) < 0.f) ori = ori * -1.f;  // points inward (center side)
			float yaw = atan2f(ori.y, ori.x);
			cs.rotMat = Angle(-yaw, 0.f, 0.f).ToRotMat();

			// Velocity in the facing direction (already running toward the play)
			float speed = RandFloat(700.f, 700.f);
			cs.vel   = Vec(cosf(-yaw) * speed, sinf(-yaw) * speed, 0.f);
			cs.boost = 100.f;
		} else {
			// DEFENDER (ORANGE): anywhere across the field width, as long as it is
			// <=300 uu from its own goal wall (+Y, at 5120).
			float x   = RandFloat(-3100.f, 3100.f);               // anywhere across the width
			float y   = RandFloat(4520.f, 5050.f);                // within 300 uu of the wall (without clipping)
			// Points to the CENTER of the field (origin): stays facing the field, never
			// toward the back wall (+Y) nor with its back to the center.
			float yaw = atan2f(0.f - y, 0.f - x) + RandFloat(-0.15f, 0.15f);

			cs.pos    = Vec(x, y, 17.f);
			cs.rotMat = Angle(-yaw, 0.f, 0.f).ToRotMat();
			cs.vel    = Vec(0.f, 0.f, 0.f);
			cs.boost  = RandFloat(20.f, 50.f);
		}

		cs.angVel = Vec(0.f, 0.f, 0.f);
		car->SetState(cs);
	}
}
