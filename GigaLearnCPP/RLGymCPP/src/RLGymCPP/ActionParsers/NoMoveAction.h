#pragma once
#include "DefaultAction.h"

namespace RLGC {
	// Action parser IMÓVEL: os carros NÃO se mexem (ação sempre a zero).
	// Mantém o MESMO número de ações do DefaultAction (delega contagem/máscara), para
	// o modelo treinado continuar a carregar sem mismatch; só o ParseAction devolve
	// uma ação nula. Útil em RENDER para observar o cenário com os bots parados.
	class NoMoveAction : public ActionParser {
	public:
		DefaultAction inner; // só para a contagem/máscara coincidirem com o treino

		virtual Action ParseAction(int index, const Player& player, const GameState& state) override {
			return Action(0, 0, 0, 0, 0, 0, 0, 0); // nada: sem throttle/steer/pitch/yaw/roll/jump/boost/handbrake
		}

		virtual int GetActionAmount() override {
			return inner.GetActionAmount();
		}

		virtual std::vector<uint8_t> GetActionMask(const Player& player, const GameState& state) override {
			return inner.GetActionMask(player, state);
		}
	};
}
