#pragma once
#include "StateSetter.h"

namespace RLGC {
	class PassState : public StateSetter {
	public:
		float minHeight; // altura mínima do centro da bola (uu)
		float maxHeight; // altura máxima do centro da bola (uu)

		// minHeight/maxHeight: intervalo de altura da bola (passe a meia altura com vel horizontal).
		PassState(float minHeight = 300.f, float maxHeight = 900.f)
			: minHeight(minHeight), maxHeight(maxHeight) {}

		// Schedulable params: "minHeight", "maxHeight" (mesmas unidades do construtor).
		virtual void SetParam(const std::string& key, float value) override {
			if (key == "minHeight") minHeight = value;
			if (key == "maxHeight") maxHeight = value;
		}

		virtual void ResetArena(Arena* arena) override;
	};
}
