#include "GoalDirectionReward.h"
#include "../Math.h"
#include <cmath>

float RLGC::GoalDirectionReward::GetReward(const Player& player, const GameState& state, bool isFinal) {
	float base = child->GetReward(player, state, isFinal);
	if (base == 0.0f) return 0.0f;

	bool targetOrange = player.team == Team::BLUE; // BLUE -> baliza ORANGE (+Y)
	// Metade de ataque: BLUE -> bola em y>0 ; ORANGE -> bola em y<0.
	bool inAttackHalf = targetOrange ? (state.ball.pos.y > 0.0f) : (state.ball.pos.y < 0.0f);

	// Na própria metade não há restrição (só faz gate na metade de ataque).
	if (onlyAttackHalf && !inAttackHalf)
		return base;

	float goalY = targetOrange ? CommonValues::BACK_WALL_Y : -CommonValues::BACK_WALL_Y;
	float vy = state.ball.vel.y;

	// A bola tem de ir NA DIREÇÃO do alvo; senão não está a apontar à baliza -> penaliza.
	bool towardGoal = targetOrange ? (vy > 1e-3f) : (vy < -1e-3f);
	if (!towardGoal) return -base;

	// Projeta a trajetória (linha reta) até ao plano da linha de golo e vê se passa
	// DENTRO da baliza: entre os postes (largura) e abaixo da barra (altura).
	// NOTA: ignora a gravidade no Z -> exato em remates rasos; aproximado em bolas altas.
	float t = (goalY - state.ball.pos.y) / vy;
	float xCross = state.ball.pos.x + state.ball.vel.x * t;
	float zCross = state.ball.pos.z + state.ball.vel.z * t;

	bool onTargetX = fabsf(xCross) <= CommonValues::GOAL_WIDTH_FROM_CENTER;
	// checkHeight=false: ignora a barra (ir por CIMA não penaliza). true: exige abaixo da barra.
	bool onTargetZ = !checkHeight || (zCross >= 0.0f && zCross <= CommonValues::GOAL_HEIGHT);
	bool onTarget = onTargetX && onTargetZ;

	// Aponta para a baliza -> dá a reward; falha a baliza -> penaliza (negativo).
	return onTarget ? base : -base;
}
