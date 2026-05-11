#pragma once
#include "TerminalCondition.h"

namespace RLGC {
	class TimeoutCondition : public TerminalCondition {
	public:
		float totalTime = 0;
		float maxTime;

		TimeoutCondition(float maxTime) : maxTime(maxTime) {}

		virtual void Reset(const GameState& initialState) override {
			totalTime = 0;
		}

		virtual bool IsTerminal(const GameState& currentState) override {
			// Conta o tempo a cada tick
			totalTime += currentState.deltaTime;
			return totalTime >= maxTime;
		}

		// Importante: atingir o limite de tempo é considerado "Truncation"
		// no PPO para garantir que ele não se pune a achar que morreu, mas
		// apenas que o tempo acabou sem finalidade útil
		virtual bool IsTruncation() override {
			return true;
		}
	};
}