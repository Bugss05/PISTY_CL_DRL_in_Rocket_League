#pragma once
#include "StateSetter.h"
#include "../Math.h"
#include <cmath>

namespace RLGC {

// Envolve um state setter de cenário (FallingBall, Pass, Cross, ...) e
// reposiciona o carro ORANGE como DEFENSOR, de forma específica ao cenário.
//
// O state setter base posiciona a bola e TODOS os carros (modo ataque). Depois
// este wrapper sobrepõe apenas o carro ORANGE para criar um 1v1 realista:
// atacante (BLUE) contra defensor (ORANGE).
//
// Modos:
//   GOAL        — guarda-redes: junto à baliza laranja (Y ≈ 4600..5100),
//                 dentro da largura da baliza, virado para o campo (-Y).
//                 Usar em FallingBall (aerial shot) e Cross (cruzamento).
//   BEHIND_BALL — defensor na TRAJETÓRIA horizontal da bola, bem à frente dela
//                 (do lado da baliza), com X variado e virado para a bola.
//                 NÃO fica preso à baliza. A bola viaja e vai ter com o
//                 defensor. Usar em Pass.
//
// Convenção yaw (igual a AirShotState.cpp): yaw = atan2(dir.y, dir.x).
//   yaw = +π/2 → virado para +Y (baliza ORANGE)
//   yaw = -π/2 → virado para -Y (campo / defesa ORANGE)
class DefenderState : public StateSetter {
public:
	enum Mode { GOAL, BEHIND_BALL };

	DefenderState(StateSetter* base, Mode mode) : base(base), mode(mode) {}

	// Encaminha parâmetros agendáveis (ex.: "minHeight"/"maxHeight") para o base.
	void SetParam(const std::string& key, float value) override {
		base->SetParam(key, value);
	}

	void ResetArena(Arena* arena) override {
		base->ResetArena(arena); // posiciona bola + TODOS os carros

		// Bola já definida pelo base (lemos o estado interno diretamente,
		// como o resto do código acede a arena->_cars).
		const Vec ballPos = arena->ball->_internalState.pos;
		const Vec ballVel = arena->ball->_internalState.vel;

		for (Car* car : arena->_cars) {
			if (car->team != Team::ORANGE) continue;

			CarState cs = {};
			float x, y, yaw;

			if (mode == GOAL) {
				// Guarda-redes: junto à linha de golo laranja, dentro da baliza.
				x   = Math::RandFloat(-700.f, 700.f);          // largura da baliza (±892) com margem
				y   = Math::RandFloat(4600.f, 5100.f);         // junto à linha de golo (+5120)
				yaw = -(float)M_PI / 2.f + Math::RandFloat(-0.30f, 0.30f); // vira para -Y (campo)
			} else { // BEHIND_BALL
				// Defensor na trajetória horizontal da bola, bem à frente dela.
				Vec flat = Vec(ballVel.x, ballVel.y, 0.f);
				Vec dir;
				if (flat.Length() < 100.f) dir = Vec(0.f, 1.f, 0.f); // bola quase parada -> assume +Y
				else                       dir = flat.Normalized();

				float dist = Math::RandFloat(1500.f, 2600.f);  // consideravelmente à frente da bola
				x = ballPos.x + dir.x * dist + Math::RandFloat(-700.f, 700.f); // X variado
				y = ballPos.y + dir.y * dist;

				// Mantém dentro do campo, sem encostar à baliza
				if (x >  3500.f) x =  3500.f;
				if (x < -3500.f) x = -3500.f;
				if (y >  4800.f) y =  4800.f;
				if (y < -4800.f) y = -4800.f;

				// Vira para a bola (que vem na sua direção)
				yaw = atan2f(ballPos.y - y, ballPos.x - x);
			}

			cs.pos    = Vec(x, y, 17.f);
			cs.vel    = {};
			cs.angVel = {};
			cs.rotMat = Angle(yaw, 0.f, 0.f).ToRotMat();
			cs.boost  = Math::RandFloat(20.f, 50.f); // boost limitado, realista

			car->SetState(cs);
		}
	}

private:
	StateSetter* base;
	Mode mode;
};

} // namespace RLGC
