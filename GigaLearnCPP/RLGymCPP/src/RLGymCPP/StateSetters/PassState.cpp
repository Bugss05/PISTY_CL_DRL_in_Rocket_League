#include "PassState.h"
#include "../Math.h"
#include <cmath>

using RocketSim::Math::RandFloat;
using RLGC::Math::RandVec;

void RLGC::PassState::ResetArena(Arena* arena) {
	arena->ResetToRandomKickoff();

	// Bola: meia altura, velocidade horizontal que simula um passe
	float ballX = RandFloat(-2000.f, 2000.f);
	float ballY = RandFloat(0.f, 4000.f);     // meio campo adversário
	float ballZ = RandFloat(300.f, 900.f);    // meia altura

	{
		BallState bs = {};
		bs.pos = Vec(ballX, ballY, ballZ);

		// Velocidade horizontal pequena em direção aleatória (passe)
		float passSpeed = RandFloat(800.f, 2000.f);
		float passDir   = RandFloat(-(9*M_PI)/8, (9*M_PI)/8);
		bs.vel = Vec(
			cosf(passDir) * passSpeed,
			sinf(passDir) * passSpeed,
			RandFloat(-150.f, 0.f)  // ligeira descida, não sobe
		);
		bs.angVel = RandVec(Vec(-2.f, -2.f, -2.f), Vec(2.f, 2.f, 2.f));
		arena->ball->SetState(bs);
	}

	for (Car* car : arena->_cars) {
		CarState cs = {};

		// Posição: atrás da bola (carY < ballY), do meio do próprio meio campo até à posição da bola
		float carX = RandFloat(-3500.f, 3500.f);
		float carY = RandFloat(-2560.f, ballY - 100.f);  // sempre atrás da bola

		if (carX >  4000.f) carX =  4000.f;
		if (carX < -4000.f) carX = -4000.f;

		cs.pos = Vec(carX, carY, 17.f);

		// Orientação aleatória no plano horizontal
		float yaw = RandFloat(-M_PI, M_PI);
		cs.rotMat = Angle(yaw, 0.f, 0.f).ToRotMat();

		// Velocidade baixa no chão
		float speed = RandFloat(0.f, 800.f);
		cs.vel = Vec(
			cosf(yaw) * speed + RandFloat(-200.f, 200.f),
			sinf(yaw) * speed + RandFloat(-200.f, 200.f),
			0.f
		);
		cs.angVel = Vec(0.f, 0.f, 0.f);
		cs.boost = 100.f;

		car->SetState(cs);
	}
}
