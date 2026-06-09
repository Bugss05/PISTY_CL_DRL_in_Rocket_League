#include "GoalShotState.h"
#include "../Math.h"

using RocketSim::Math::RandFloat;

void RLGC::GoalShotState::ResetArena(Arena* arena) {
	arena->ResetToRandomKickoff();

	float ballX = RandFloat(-890.f, 890.f);
	float ballY = 5020.f;

	{ // Set up the ball
		BallState bs = {};
		bs.pos = Vec(ballX, ballY, CommonValues::BALL_RADIUS);
		bs.vel = Vec(0, 0, 0);
		bs.angVel = Vec(0, 0, 0);
		arena->ball->SetState(bs);
	}

	for (Car* car : arena->_cars) {
		CarState cs = {};

		float theta = RandFloat(0.f, M_PI);
		float carX = ballX + radius * cosf(theta);
		float carY = ballY - radius * sinf(theta);
		
		// Map Boundaries check so the car doesn't spawn outside the arena walls
		float MAX_X = 4000.f;
		float MAX_Y = 5000.f;
		if (carX > MAX_X) carX = MAX_X;
		if (carX < -MAX_X) carX = -MAX_X;
		if (carY > MAX_Y) carY = MAX_Y;
		if (carY < -MAX_Y) carY = -MAX_Y;

		cs.pos = Vec(carX, carY, 17.f); // safely on the ground
		cs.vel = Vec(0, 0, 0);
		cs.angVel = Vec(0, 0, 0);

		// Dá ao carro uma rotação (yaw) completamente aleatória em vez de estar sempre virado para a bola
		float yaw = RandFloat(-M_PI, M_PI);

		Angle angle = Angle(yaw, 0.f, 0.f);
		cs.rotMat = angle.ToRotMat();
		cs.boost = 100.f;

		car->SetState(cs);
	}
}