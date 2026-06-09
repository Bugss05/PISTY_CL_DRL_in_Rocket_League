#pragma once
#include <RLGymCPP/TerminalConditions/TerminalCondition.h>

// Termina o episódio quando qualquer jogador toca na bola.
// Útil em fases iniciais de currículo para dar feedback imediato ao primeiro toque.
class BallTouchCondition : public RLGC::TerminalCondition {
public:
	bool IsTerminal(const RLGC::GameState& currentState) override {
		for (const auto& player : currentState.players)
			if (player.ballTouchedStep)
				return true;
		return false;
	}

	bool IsTruncation() override { return false; }
};
