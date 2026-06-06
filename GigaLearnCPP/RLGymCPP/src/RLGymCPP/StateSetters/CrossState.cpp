#include "CrossState.h"
#include "../Math.h"
#include <cmath>

using RocketSim::Math::RandFloat;

void RLGC::CrossState::ResetArena(Arena* arena) {
	arena->ResetToRandomKickoff();

	// Lado aleatório: +1 = parede direita, -1 = parede esquerda
	float side = (RandFloat(0.f, 1.f) > 0.5f) ? 1.f : -1.f;

	// Bola perto da parede no meio campo adversário
	float ballX = side * RandFloat(3000.f, 4000.f);
	float ballY = RandFloat(1000.f, 4000.f);
	float ballZ = RandFloat(100.f, 400.f);

	// Ângulo de saída [π/5, π] medido a partir do eixo +Y em direção ao centro do campo
	// π/5 ≈ 36°: cruzamento para a frente (vai para a área)
	// π/2 = 90°: cruzamento perpendicular
	// π = 180°: cruzamento para trás
	float exitAngle = RandFloat(float(M_PI) / 8.f, (5*float(M_PI)) / 8.f);
	float crossSpeed = RandFloat(800.f, 2000.f);

	// Componentes: X vai para o centro (oposto ao lado), Y segue o ângulo
	float velX = -side * sinf(exitAngle) * crossSpeed;
	float velY  =        cosf(exitAngle) * crossSpeed;

	{
		BallState bs = {};
		bs.pos    = Vec(ballX, ballY, ballZ);
		bs.vel    = Vec(velX, velY, RandFloat(1000.f, 1500.f));
		bs.angVel = Vec(RandFloat(-2.f, 2.f), RandFloat(-2.f, 2.f), RandFloat(-2.f, 2.f));
		arena->ball->SetState(bs);
	}

	for (Car* car : arena->_cars) {
		CarState cs = {};

		// Perto do centro, um pouco antes da bola em Y
		float carX = RandFloat(-700.f, 700.f);
		float carY = RandFloat(ballY - 1600.f, ballY - 800.f);
		if (carY < -2560.f) carY = -2560.f;

		cs.pos = Vec(carX, carY, 17.f);

		// Yaw: [π/5, 3π/5] — aponta para a baliza adversária (yaw=π/2 = +Y)
		float yaw = RandFloat(float(M_PI) / 5.f, 3.f * float(M_PI) / 5.f);
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
