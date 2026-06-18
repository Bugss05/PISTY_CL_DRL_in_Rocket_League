#include "CrossingState.h"
#include "../Math.h"
#include <cmath>

using RocketSim::Math::RandFloat;

void RLGC::CrossingState::ResetArena(Arena* arena) {
	arena->ResetToRandomKickoff();

	// Lado da asa de onde vem o cruzamento: +1 = parede direita, -1 = parede esquerda.
	float side = (RandFloat(0.f, 1.f) > 0.5f) ? 1.f : -1.f;

	// ---------------------------------------------------------------------
	// ZONA DA BOLA (BLUE ataca +Y; baliza ORANGE em Y=+5120)
	//   X: na asa, perto da parede lateral (±4096) mas sem encostar
	//   Y: entra no meio-campo adversário, com espaço para arquear para a frente
	//   Z: já no ar (parametrizável via minHeight/maxHeight)
	// ---------------------------------------------------------------------
	float ballX = side * RandFloat(2400.f, 3700.f);
	float ballY = RandFloat(3000.f, 4000.f);
	float ballZ = RandFloat(minHeight, maxHeight);

	// ---------------------------------------------------------------------
	// DIREÇÃO HORIZONTAL DO CRUZAMENTO: ângulo da velocidade = ângulo do vetor
	// bola->baliza + um desvio. Num extremo (desvio 0) a bola vai DIRETA à baliza;
	// no outro (desvio 90°) sai PERPENDICULAR ao vetor bola->baliza, mas sempre
	// rodada para LONGE da parede (para o interior do campo), nunca contra a parede.
	//
	// O sentido da rotação depende do lado de onde vem o cruzamento (side):
	//   asa direita (side=+1, vai da direita p/ esquerda): roda 0..+90°
	//   asa esquerda (side=-1, vai da esquerda p/ direita): roda 0..-90°
	// ARCO (vertical): velZ alto -> a bola sobe e cai = bola alta que exige aéreo.
	// ---------------------------------------------------------------------
	float horizSpeed = RandFloat(minSpeed, maxSpeed/1.5f); // velocidade horizontal do cruzamento (parametrizável via minSpeed/maxSpeed)

	Vec goal = Vec(0.f, 5120.f, 0.f); // baliza ORANGE (alvo do BLUE)
	float goalAng = atan2f(goal.y - ballY, goal.x - ballX); // ângulo do vetor bola->baliza (XY)

	constexpr float MAX_ANG = (float)M_PI / 3.f;
	float velAng = goalAng + side * RandFloat(0.f, MAX_ANG); // 0..±60°, sempre p/ longe da parede

	float velX = cosf(velAng) * horizSpeed;
	float velY = sinf(velAng) * horizSpeed;
	float velZ = RandFloat(650.f, 1050.f);  // o ARCO (sobe primeiro)

	{
		BallState bs = {};
		bs.pos    = Vec(ballX, ballY, ballZ);
		bs.vel    = Vec(velX, velY, velZ);
		bs.angVel = Vec(RandFloat(-2.f, 2.f), RandFloat(-2.f, 2.f), RandFloat(-2.f, 2.f));
		arena->ball->SetState(bs);
	}

	// ---------------------------------------------------------------------
	// PAPÉIS FIXOS: BLUE = ATACANTE, ORANGE = DEFENSOR (guarda-redes).
	//   BLUE  -> posição de ataque (área frontal OU meio-campo), rotação aleatória,
	//            boost cheio. Vai disputar o cruzamento.
	//   ORANGE-> guarda-redes junto à baliza +Y que defende, virado para o campo (-Y),
	//            boost limitado (realista).
	// ---------------------------------------------------------------------
	for (Car* car : arena->_cars) {
		CarState cs = {};

		if (car->team == Team::BLUE) {
			// ===== POSIÇÃO DO ATACANTE (5 passos) =====
			// 1) vetor bola->baliza (normalizado)
			Vec v1 = Vec(goal.x - ballX, goal.y - ballY, 0.f).Normalized();

			// 2) vetor de velocidade horizontal (X,Y) da bola
			Vec v2 = Vec(velX, velY, 0.f);
			Vec v2dir = v2.Normalized();

			// Referência ESTÁVEL de "dentro do campo": vetor da bola para o CENTRO.
			// Não depende da direção da bola -> NÃO inverte se a bola for p/ a nossa baliza.
			Vec toCenter = Vec(-ballX, -ballY, 0.f);

			// 3) v2 rodado 90°, escolhido para o lado do CENTRO do campo (interior).
			Vec v3 = Vec(-v2dir.y, v2dir.x, 0.f);          // perpendicular a v2
			if (v3.Dot(toCenter) < 0.f) v3 = v3 * -1.f;    // garante que aponta para dentro

			// 4) RETÂNGULO de zonas possíveis a partir da bola: deslocamento ao longo de
			//    v2 (direção da bola) e de v3 (perpendicular p/ dentro). Min/max escalados
			//    com a velocidade da bola; o do v3 é MENOR que o do v2.
			float speedScale = RS_CLAMP(horizSpeed / maxSpeed, 0.f, 1.f);
			float offV2 = RandFloat(1700.f, 2300.f);   // ao longo da direção da bola
			float offV3 = RandFloat(1000.f, 1500.f);   // perpendicular p/ dentro

			cs.pos = Vec(ballX, ballY, 17.f) + (v2dir * offV2 + v3 * offV3);

			// 5) ORIENTAÇÃO = v1 (bola->baliza) rodado 90° PARA DENTRO do campo (mediante o lado)
			Vec ori = Vec(-v1.y, v1.x, 0.f);
			if (ori.Dot(toCenter) < 0.f) ori = ori * -1.f;  // aponta para dentro (lado do centro)
			float yaw = atan2f(ori.y, ori.x);
			cs.rotMat = Angle(-yaw, 0.f, 0.f).ToRotMat();

			// Velocidade na direção em que está virado (já a correr para o lance)
			float speed = RandFloat(700.f, 700.f);
			cs.vel   = Vec(cosf(-yaw) * speed, sinf(-yaw) * speed, 0.f);
			cs.boost = 100.f;
		} else {
			// DEFENSOR (ORANGE): em qualquer ponto à largura do campo, desde que a
			// <=300 uu da sua parede da baliza (+Y, em 5120).
			float x   = RandFloat(-3100.f, 3100.f);               // qualquer ponto à largura
			float y   = RandFloat(4520.f, 5050.f);                // dentro de 300 uu da parede (sem clipar)
			// Aponta para o CENTRO do campo (origem): fica virado para o campo, nunca
			// para a parede de fundo (+Y) nem de costas para o centro.
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
