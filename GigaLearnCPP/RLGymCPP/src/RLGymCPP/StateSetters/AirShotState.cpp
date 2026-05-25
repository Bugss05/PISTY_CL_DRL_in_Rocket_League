#include "AirShotState.h"
#include "../Math.h"

using RocketSim::Math::RandFloat;
using RLGC::Math::RandVec;

void RLGC::AirShotState::ResetArena(Arena* arena) {
	arena->ResetToRandomKickoff();

	// Ball starts lower but has upward velocity, and can be on the sides
	float ballX = RandFloat(-3000.f, 3000.f);
	float ballY = RandFloat(-2000.f, 2000.f);
	float ballZ = RandFloat(200.f, 800.f);

	Vec targetPos = Vec(0.f, 5120.f, 0.f); // Opponent goal for BLUE

	{
		BallState bs = {};
		bs.pos = Vec(ballX, ballY, ballZ);
		
		// Calculate direction towards goal
		Vec velDirGoal = (targetPos - bs.pos);
		velDirGoal.z = 0;
		velDirGoal = velDirGoal.Normalized();

		// Slower speed towards goal, high upward speed to give the bot time
		bs.vel = velDirGoal * RandFloat(1200.f, 2200.f);
		bs.vel.z = RandFloat(700.f, 1300.f); // Menos forca vertical (antes: 1200 a 2000) e mais forca para a frente
		bs.vel.x += RandFloat(-300.f, 300.f); // Adds some horizontal variation

		bs.angVel = Vec(0, 0, 0);
		arena->ball->SetState(bs);
	}

	for (Car* car : arena->_cars) {
		CarState cs = {};

		// Spawn car behind the ball (closer to their own goal side)
		float carX = ballX + RandFloat(-400.f, 400.f);
		float carY = ballY - RandFloat(1000.f, 1800.f);
		
		// Map boundaries check
		if (carX > 4000.f) carX = 4000.f;
		if (carX < -4000.f) carX = -4000.f;
		if (carY < -5000.f) carY = -5000.f;

		cs.pos = Vec(carX, carY, 17.f); 
		
		// Car pointing roughly towards the goal/ball
		Vec dir = (targetPos - cs.pos).Normalized();

		// Approx yaw calculation
		float yaw = atan2(dir.y, dir.x);
		Angle angle = Angle(yaw, 0.f, 0.f);
		cs.rotMat = angle.ToRotMat();

		cs.vel = dir * RandFloat(500.f, 1500.f);
		cs.angVel = Vec(0, 0, 0);
		cs.boost = 100.f;

		car->SetState(cs);
	}
}