#include "WallDragState.h"
#include "../Math.h"
#include <cmath>

using RocketSim::Math::RandFloat;

void RLGC::WallDragState::ResetArena(Arena* arena) {
	arena->ResetToRandomKickoff();

	// Lado aleatório: +1 = parede direita, -1 = parede esquerda
	float side = (RandFloat(0.f, 1.f) > 0.5f) ? 1.f : -1.f;

	// Bola no chão, colada à parede lateral
	// Y: um pouco antes do meio campo até quase à baliza adversária
	float ballX = side * RandFloat(3200.f, 3200.f);
	float ballY = RandFloat(-300.f, 3000.f);
	float ballZ = 93.f;  // raio da bola — a tocar o chão

	// Ângulo θ ∈ [2π/5, π/2] a partir do eixo +Y (frente), no plano XY:
	// [-π/2, -2π/5] do utilizador → magnitude [2π/5, π/2] em direção à parede
	// sin(θ) → componente X para a parede (dominante ~95–100%)
	// cos(θ) → componente Y para a frente (pequena ~0–31%)
	float theta = RandFloat(2.f * float(M_PI) / 5.f, float(M_PI) / 2.f);
	float speed = RandFloat(2100.f, 2100.f);

	float velX = side * sinf(theta) * speed;  // para a parede (dominante)
	float velY = cosf(theta) * speed;          // avanço para a frente (pequeno)

	{
		BallState bs = {};
		bs.pos    = Vec(ballX, ballY, ballZ);
		bs.vel    = Vec(velX, velY, 0);
		bs.angVel = Vec(RandFloat(-3.f, 3.f), RandFloat(-3.f, 3.f), RandFloat(-3.f, 3.f));
		arena->ball->SetState(bs);
	}

	for (Car* car : arena->_cars) {
		CarState cs = {};

		// Atrás da bola, perto da parede, no chão — como se estivesse a segui-la
		float carX = ballX - side * RandFloat(200.f, 600.f);  // atrás da bola em X, mesmo lado
		float carY = RandFloat(ballY - 600.f, ballY - 300.f);
		if (carY < -2560.f) carY = -2560.f;

		cs.pos = Vec(carX, carY, 17.f);

		// Apontar para a bola
		float dx = ballX - carX;
		float dy = ballY - carY;
		float yaw = atan2f(dy, dx);
		cs.rotMat = Angle(yaw, 0.f, 0.f).ToRotMat();

		// Velocidade baixa — a seguir a bola
		float carSpeed = RandFloat(0.f, 600.f);
		cs.vel = Vec(
			cosf(yaw) * carSpeed + RandFloat(-100.f, 100.f),
			sinf(yaw) * carSpeed + RandFloat(-100.f, 100.f),
			0.f
		);
		cs.angVel = Vec(0.f, 0.f, 0.f);
		cs.boost  = 100.f;

		car->SetState(cs);
	}
}
