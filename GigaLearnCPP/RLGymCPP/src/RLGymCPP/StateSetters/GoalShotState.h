#pragma once
#include "StateSetter.h"

namespace RLGC {
	class GoalShotState : public StateSetter {
	public:
		float radius;
		float minHeight; // altura mínima do centro da bola (uu)
		float maxHeight; // altura máxima do centro da bola (uu) — limitada à barra em ResetArena

		// radius: distância carro->bola no spawn.
		// minHeight/maxHeight: intervalo de altura da bola. maxHeight é SEMPRE limitado em
		// ResetArena a (GOAL_HEIGHT - BALL_RADIUS) ≈ 550 para a bola nunca ficar acima da
		// barra — senão passa por cima do golo ("phase") e não conta, o que prejudica o treino.
		GoalShotState(float radius = 1000.f, float minHeight = 92.75f, float maxHeight = 550.f)
			: radius(radius), minHeight(minHeight), maxHeight(maxHeight) {}

		// Schedulable params: "radius", "minHeight", "maxHeight" (mesmas unidades do construtor).
		virtual void SetParam(const std::string& key, float value) override {
			if (key == "radius")    radius    = value;
			if (key == "minHeight") minHeight = value;
			if (key == "maxHeight") maxHeight = value;
		}

		virtual void ResetArena(Arena* arena) override;
	};
}
