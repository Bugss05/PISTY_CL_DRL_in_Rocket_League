#pragma once
#include "../Gamestates/GameState.h"
#include <string>

namespace RLGC {

	enum TerminalType {
		NOT_TERMINAL,

		NORMAL,
		TRUNCATED
	};

	class TerminalCondition {
	public:
		virtual void Reset(const GameState& initialState) {};
		virtual bool IsTerminal(const GameState& currentState) = 0;

		// If this terminal condition truncates episode
		// You should use truncation if the terminal condition is not part of the game (such as timeout conditions)
		virtual bool IsTruncation() = 0;

		// Optional runtime-tunable parameters (e.g. TimeoutCondition's "seconds"), so the
		// Scheduler can change them per training phase. Unknown keys are ignored; conditions
		// without parameters keep the default no-op.
		virtual void SetParam(const std::string& key, float value) {}
	};
}