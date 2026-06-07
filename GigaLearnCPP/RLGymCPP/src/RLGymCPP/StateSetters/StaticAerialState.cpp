#include "StaticAerialState.h"
#include "../Math.h"
#include <cmath>

using RocketSim::Math::RandFloat;

void RLGC::StaticAerialState::ResetArena(Arena* arena) {
	arena->ResetToRandomKickoff();

	// Bola parada no ar a altura de aerial (não flip reset)
	// Flip reset seria Z≈17-100; aerial começa a partir de ~500
	float ballX = RandFloat(-2000.f, 2000.f);
	float ballY = RandFloat(0.f, 4000.f);    // meio campo adversário
	float ballZ = RandFloat(600.f, 1500.f);  // altura de aerial claro: salto + aerial

	{
		BallState bs = {};
		bs.pos    = Vec(ballX, ballY, ballZ);
		bs.vel    = Vec(0,0,0); // quase parada
		bs.angVel = Vec(0,0,0);
		arena->ball->SetState(bs);
	}

	for (Car* car : arena->_cars) {
		CarState cs = {};

		// Atrás da bola, no chão
		float carX = RandFloat(-3500.f, 3500.f);
		float carY = RandFloat(-2560.f, ballY - 100.f); // sempre atrás da bola

		if (carX >  4000.f) carX =  4000.f;
		if (carX < -4000.f) carX = -4000.f;

		cs.pos = Vec(carX, carY, 17.f);

		float yaw = RandFloat(-M_PI, M_PI);
		cs.rotMat = Angle(yaw, 0.f, 0.f).ToRotMat();

		float speed = RandFloat(0.f, 800.f);
		cs.vel = Vec(
			cosf(yaw) * speed + RandFloat(-200.f, 200.f),
			sinf(yaw) * speed + RandFloat(-200.f, 200.f),
			0.f
		);
		cs.angVel = Vec(0.f, 0.f, 0.f);
		cs.boost  = 100.f;

		car->SetState(cs);
	}
}
