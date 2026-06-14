#pragma once
#include "StateSetter.h"

namespace RLGC {
	class StaticAerialState : public StateSetter {
	public:
		float minHeight; // altura mínima do centro da bola (uu)
		float maxHeight; // altura máxima do centro da bola (uu)

		// minHeight/maxHeight: intervalo de altura da bola parada no ar (altura de aerial).
		StaticAerialState(float minHeight = 600.f, float maxHeight = 1500.f)
			: minHeight(minHeight), maxHeight(maxHeight) {}

		// Schedulable params: "minHeight", "maxHeight" (mesmas unidades do construtor).
		virtual void SetParam(const std::string& key, float value) override {
			if (key == "minHeight") minHeight = value;
			if (key == "maxHeight") maxHeight = value;
		}

		virtual void ResetArena(Arena* arena) override;
	};
}
