#include "FallingBallApproachState.h"
#include "../Math.h"
#include <cmath>

using RocketSim::Math::RandFloat;

void RLGC::FallingBallApproachState::ResetArena(Arena* arena) {
	arena->ResetToRandomKickoff();

	// Bola: meio campo adversário (Y > 0 para Blue), cai por gravidade sem impulso inicial
	float ballX = RandFloat(-1500.f, 1500.f);
	float ballY = RandFloat(1500.f, 3500.f);  // centro do meio campo adversário ≈ Y 2560
	float ballZ = RandFloat(400.f, 700.f);    // alcançável com um salto simples, não aerial

	{
		BallState bs = {};
		bs.pos    = Vec(ballX, ballY, ballZ);
		bs.vel    = Vec(0.f, 0.f, RandFloat(-300.f, -100.f)); // cai desde o início
		bs.angVel = Vec(0.f, 0.f, 0.f);
		arena->ball->SetState(bs);
	}

	for (Car* car : arena->_cars) {
		CarState cs = {};

		float carX = RandFloat(-3000.f, 3000.f);
		float carY = RandFloat(-2000.f, 0.f);   // próprio meio campo até linha central

		if (carX >  4000.f) carX =  4000.f;
		if (carX < -4000.f) carX = -4000.f;

		cs.pos = Vec(carX, carY, 17.f);

		// Orientação completamente aleatória
		float yaw = RandFloat(-M_PI, M_PI);
		cs.rotMat = Angle(yaw, 0.f, 0.f).ToRotMat();

		// Velocidade aleatória no chão
		float speed = RandFloat(0.f, 1800.f);
		cs.vel = Vec(
			cosf(yaw) * speed + RandFloat(-300.f, 300.f),
			sinf(yaw) * speed + RandFloat(-300.f, 300.f),
			0.f
		);
		cs.angVel = Vec(0.f, 0.f, 0.f);
		cs.boost  = 100.f;

		car->SetState(cs);
	}
}
