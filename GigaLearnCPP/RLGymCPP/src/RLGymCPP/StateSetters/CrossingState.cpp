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
	float horizSpeed = RandFloat(minSpeed, maxSpeed);

	Vec goal = Vec(0.f, 5120.f, 0.f); // baliza ORANGE (alvo do BLUE)
	float goalAng = atan2f(goal.y - ballY, goal.x - ballX); // ângulo do vetor bola->baliza (XY)

	constexpr float MAX_ANG = (float)M_PI / 3.f;
	float velAng = goalAng + side * RandFloat(0.f, MAX_ANG); // 0..±60°, sempre p/ longe da parede

	float velX = cosf(velAng) * horizSpeed;
	float velY = sinf(velAng) * horizSpeed;
	float velZ = RandFloat(650.f, 1250.f);  // o ARCO (sobe primeiro)

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
			// ATACANTE: PERPENDICULAR à trajetória XY da bola.
			// Escolhe-se um ponto P na trajetória (à frente da bola). Como P está na
			// linha, P->bola é paralelo à velocidade; deslocando o atacante na
			// PERPENDICULAR a partir de P, o ângulo (P->bola, P->atacante) = 90° por
			// construção, e a distância (>=100 uu) à trajetória é esse deslocamento.
			Vec d = Vec(velX, velY, 0.f).Normalized();    // direção da trajetória XY
			Vec n = Vec(-d.y, d.x, 0.f);                   // perpendicular (90°)
			// A trajetória divide o campo em dois lados. O atacante fica na metade
			// MAIS LONGE da baliza (lado oposto ao da baliza relativamente à linha).
			Vec ballToGoal = Vec(goal.x - ballX, goal.y - ballY, 0.f);
			if (ballToGoal.Dot(n) > 0.f) n = n * -1.f;     // se n aponta p/ a baliza, inverte

			float along = RandFloat(700.f, 2200.f);        // ponto P, à frente da bola
			float perp  = RandFloat(400.f, 2000.f);        // distância à trajetória (>=100)

			Vec P = Vec(ballX, ballY, 0.f) + d * along;
			Vec a = P + n * perp;

			// Mantém dentro do campo (margem das paredes/baliza)
			float carX = RS_CLAMP(a.x, -3800.f, 3800.f);
			float carY = RS_CLAMP(a.y, -2500.f, 4400.f);
			cs.pos = Vec(carX, carY, 17.f);

			// Virado para o ponto P da trajetória, com desvio aleatório de ±45°
			float baseYaw = atan2f(P.y - carY, P.x - carX);
			float yaw = baseYaw + RandFloat(-(float)M_PI / 4.f, (float)M_PI / 4.f);
			cs.rotMat = Angle(yaw, 0.f, 0.f).ToRotMat();

			// Velocidade baixa na direção em que está virado (pronto, não comprometido)
			float speed = RandFloat(0.f, 500.f);
			cs.vel   = Vec(cosf(yaw) * speed, sinf(yaw) * speed, 0.f);
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
			cs.rotMat = Angle(yaw, 0.f, 0.f).ToRotMat();
			cs.vel    = Vec(0.f, 0.f, 0.f);
			cs.boost  = RandFloat(20.f, 50.f);
		}

		cs.angVel = Vec(0.f, 0.f, 0.f);
		car->SetState(cs);
	}
}
