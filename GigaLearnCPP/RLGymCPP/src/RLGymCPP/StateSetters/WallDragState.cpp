#include "WallDragState.h"
#include "../Math.h"
#include <cmath>

using RocketSim::Math::RandFloat;

void RLGC::WallDragState::ResetArena(Arena* arena) {
	arena->ResetToRandomKickoff();

	// Random side: +1 = right wall, -1 = left wall
	float side = (RandFloat(0.f, 1.f) > 0.5f) ? 1.f : -1.f;

	// Ball on the ground, glued to the side wall
	// Y: a bit before midfield up to almost the opponent's goal
	float ballX = side * RandFloat(3200.f, 3200.f);
	float ballY = RandFloat(-300.f, 3000.f);
	float ballZ = 93.f;  // ball radius — touching the ground

	// Angle θ ∈ [2π/5, π/2] from the +Y axis (forward), in the XY plane:
	// [-π/2, -2π/5] from the user → magnitude [2π/5, π/2] toward the wall
	// sin(θ) → X component toward the wall (dominant ~95–100%)
	// cos(θ) → Y component forward (small ~0–31%)
	float theta = RandFloat(2.f * float(M_PI) / 5.f, float(M_PI) / 2.f);
	float speed = RandFloat(2100.f, 2100.f);

	float velX = side * sinf(theta) * speed;  // toward the wall (dominant)
	float velY = cosf(theta) * speed;          // forward advance (small)

	{
		BallState bs = {};
		bs.pos    = Vec(ballX, ballY, ballZ);
		bs.vel    = Vec(velX, velY, 0);
		bs.angVel = Vec(RandFloat(-3.f, 3.f), RandFloat(-3.f, 3.f), RandFloat(-3.f, 3.f));
		arena->ball->SetState(bs);
	}

	for (Car* car : arena->_cars) {
		CarState cs = {};

		if (car->team == Team::BLUE) {
			// ATTACKER: behind the ball, near the wall, on the ground — as if following it.
			float carX = ballX - side * RandFloat(200.f, 600.f);  // behind the ball in X, same side
			float carY = RandFloat(ballY - 600.f, ballY - 300.f);
			if (carY < -2560.f) carY = -2560.f;

			cs.pos = Vec(carX, carY, 17.f);

			// Point at the ball
			float dx = ballX - carX;
			float dy = ballY - carY;
			float yaw = atan2f(dy, dx);
			cs.rotMat = Angle(yaw, 0.f, 0.f).ToRotMat();

			// Low speed — following the ball
			float carSpeed = RandFloat(0.f, 600.f);
			cs.vel = Vec(
				cosf(yaw) * carSpeed + RandFloat(-100.f, 100.f),
				sinf(yaw) * carSpeed + RandFloat(-100.f, 100.f),
				0.f
			);
			cs.boost = 100.f;
		} else {
			// DEFENDER (ORANGE): goalkeeper next to the +Y goal (the ball advances toward +Y).
			float x   = RandFloat(-3100.f, 3100.f);   // anywhere across the width
			float y   = RandFloat(4520.f, 5050.f);    // within 300 uu of the goal wall
			float yaw = atan2f(0.f - y, 0.f - x) + RandFloat(-0.15f, 0.15f); // facing the field

			cs.pos    = Vec(x, y, 17.f);
			cs.rotMat = Angle(yaw, 0.f, 0.f).ToRotMat();
			cs.vel    = Vec(0.f, 0.f, 0.f);
			cs.boost  = RandFloat(20.f, 50.f);
		}

		cs.angVel = Vec(0.f, 0.f, 0.f);
		car->SetState(cs);
	}
}
