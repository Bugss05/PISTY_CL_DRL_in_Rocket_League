#include "AttackerMidfieldState.h"
#include "../Math.h"

// MathTypes defines M_PI, but we can also use Math::Constants if present
// Assuming M_PI is available via RocketSim's math
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

using RocketSim::Math::RandFloat;

namespace RLGC {
	void AttackerMidfieldState::ResetArena(Arena* arena) {
		arena->ResetToRandomKickoff();

		// A bola instanciada dentro da area (adversária)
		// Área de penalty ronda X: [-1000, 1000] e Y [4000, 5000] (assumindo baliza +Y)
		float ballX = RandFloat(-1024.f, 1024.f);
		float ballY = RandFloat(4096.f, 5100.f); 

		// A começar a meia altura
		float ballZ = RandFloat(500.f, 1000.f);

		// Um pouco de velocidade para a frente sem apontar para a baliza.
		// A baliza está em X = 0. Para não apontar muito diretamente, adicionamos X vel
		float ballVx = (ballX > 0) ? RandFloat(400.f, 800.f) : RandFloat(-800.f, -400.f);
		float ballVy = RandFloat(300.f, 800.f); // Velocidade para +Y (frente)

		{ // Set up the ball
			BallState bs = {};
			bs.pos = Vec(ballX, ballY, ballZ);
			bs.vel = Vec(ballVx, ballVy, RandFloat(100.f, 300.f)); // Um ligeiro arco em Z
			bs.angVel = Vec(0.f, 0.f, 0.f);
			arena->ball->SetState(bs);
		}

		for (Car* car : arena->_cars) { 
			CarState cs = {};
			
			// O robô perto do meio do meio campo adversário, 
			// nascimento random nessa linha
			float carX = RandFloat(-3000.f, 3000.f);
			float carY = 2560.f; // Metade do campo adversário (5120 / 2)
			
			cs.pos = Vec(carX, carY, 17.f);
			cs.vel = Vec(0, 0, 0);
			cs.angVel = Vec(0, 0, 0);

			// Robô sempre a apontar para o eixo do y (yaw = 90 graus / pi/2)
			// Isso aponta o carro em direcção ao campo adversário (+Y).
			Angle angle = Angle(M_PI / 2.f, 0.f, 0.f);
			cs.rotMat = angle.ToRotMat();
			cs.boost = 100.f;

			car->SetState(cs);
		}
	}
}