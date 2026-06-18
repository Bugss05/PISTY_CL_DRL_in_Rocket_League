#pragma once
#include "RewardWrapper.h"

namespace RLGC {
	// Wrapper DIRECIONAL: filtra a reward do child conforme a bola vai PARA A BALIZA
	// adversária (lógica idêntica ao VelocityBallToGoalReward, ciente da equipa).
	//   - Na metade de ATAQUE (à frente do meio campo, do lado da baliza adversária):
	//     só deixa passar se o toque mandar a bola PARA A BALIZA, escalando pelo
	//     alinhamento (quanto mais direcionada à baliza, maior — "perto disso").
	//   - Na própria metade (defesa): passa a reward inteira (se onlyAttackHalf = true).
	//   - onlyAttackHalf = false: exige direção à baliza em TODO o campo (ex.: air shot).
	// Tem em conta a EQUIPA: BLUE ataca +Y (baliza ORANGE); ORANGE ataca -Y (baliza BLUE).
	class GoalDirectionReward : public RewardWrapper {
	public:
		bool onlyAttackHalf;

		GoalDirectionReward(Reward* child, bool onlyAttackHalf = true)
			: RewardWrapper(child), onlyAttackHalf(onlyAttackHalf) {}

	protected:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override;
	};
}
