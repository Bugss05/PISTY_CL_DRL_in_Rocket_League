#pragma once
#include "StateSetter.h"

namespace RLGC {
	// "Crossing": bola ALTA em ARCO vinda de uma asa em direção à zona frontal da
	// baliza adversária, forçando o atacante a preparar-se para um AÉRIO.
	//
	// Diferente do CrossState (cruzamento rasteiro/meia-altura colado à parede):
	// aqui a bola sobe (velZ alto) e arqueia para dentro.
	//
	// PAPÉIS FIXOS, embutidos no próprio estado (não precisa de DefenderState):
	//   BLUE   = ATACANTE -> PERPENDICULAR à trajetória XY da bola (>=100 uu dela),
	//            na metade do campo mais LONGE da baliza, virado para o ponto P da
	//            trajetória (±45°), boost cheio.
	//   ORANGE = DEFENSOR -> em qualquer ponto a <=300 uu da sua parede da baliza (+Y),
	//            virado para o centro do campo.
	//
	// Convenção: BLUE ataca a baliza ORANGE (+Y, em Y=+5120).
	//
	// Parâmetros agendáveis (SetParam): "minHeight"/"maxHeight" (Z inicial da bola),
	// "minSpeed"/"maxSpeed" (velocidade horizontal do cruzamento).
	class CrossingState : public StateSetter {
	public:
		float minHeight, maxHeight;   // Z inicial da bola (uu)
		float minSpeed, maxSpeed;     // velocidade horizontal do cruzamento (uu/s)

		CrossingState(float minHeight = 300.f, float maxHeight = 700.f,
		              float minSpeed = 1000.f, float maxSpeed = 1700.f)
			: minHeight(minHeight), maxHeight(maxHeight),
			  minSpeed(minSpeed), maxSpeed(maxSpeed) {}

		void SetParam(const std::string& key, float value) override {
			if      (key == "minHeight") minHeight = value;
			else if (key == "maxHeight") maxHeight = value;
			else if (key == "minSpeed")  minSpeed  = value;
			else if (key == "maxSpeed")  maxSpeed  = value;
		}

		void ResetArena(Arena* arena) override;
	};
}
