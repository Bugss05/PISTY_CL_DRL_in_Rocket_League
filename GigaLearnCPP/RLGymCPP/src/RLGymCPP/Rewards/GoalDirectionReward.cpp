#include "GoalDirectionReward.h"
#include "../Math.h"

float RLGC::GoalDirectionReward::GetReward(const Player& player, const GameState& state, bool isFinal) {
	float base = child->GetReward(player, state, isFinal);
	if (base == 0.0f) return 0.0f;

	bool targetOrange = player.team == Team::BLUE; // BLUE -> baliza ORANGE (+Y)
	// Metade de ataque: BLUE -> bola em y>0 ; ORANGE -> bola em y<0.
	bool inAttackHalf = targetOrange ? (state.ball.pos.y > 0.0f) : (state.ball.pos.y < 0.0f);

	// Na própria metade não há restrição (só gate na metade de ataque).
	if (onlyAttackHalf && !inAttackHalf)
		return base;

	// Alinhamento da velocidade da bola com a direção à baliza adversária.
	Vec goalPos = targetOrange ? CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;
	Vec ballToGoal = goalPos - state.ball.pos;
	float d = ballToGoal.Length();
	float ballSpeed = state.ball.vel.Length();
	if (d < 1e-6f || ballSpeed < 1e-6f) return 0.0f;

	float align = RS_CLAMP((ballToGoal / d).Dot(state.ball.vel / ballSpeed), 0.3f, 1.0f);
	return base * align;
}
