#pragma once
#include "Reward.h"
#include "../Math.h"
#include <unordered_map>

namespace RLGC {

	template<bool PlayerEventState::* VAR, bool NEGATIVE>
	class PlayerDataEventReward : public Reward {
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			bool val =  player.eventState.*VAR;

			if (NEGATIVE) {
				return -(float)val;
			} else {
				return (float)val;
			}
		}
	};

	typedef PlayerDataEventReward<&PlayerEventState::goal, false> PlayerGoalReward; // NOTE: Given only to the player who last touched the ball on the opposing team
	typedef PlayerDataEventReward<&PlayerEventState::assist, false> AssistReward;
	typedef PlayerDataEventReward<&PlayerEventState::shot, false> ShotReward;
	typedef PlayerDataEventReward<&PlayerEventState::shotPass, false> ShotPassReward;
	typedef PlayerDataEventReward<&PlayerEventState::save, false> SaveReward;
	typedef PlayerDataEventReward<&PlayerEventState::bump, false> BumpReward;
	typedef PlayerDataEventReward<&PlayerEventState::bumped, true> BumpedPenalty;
	typedef PlayerDataEventReward<&PlayerEventState::demo, false> DemoReward;
	typedef PlayerDataEventReward<&PlayerEventState::demoed, true> DemoedPenalty;

	// Recompensa um golo, com bónus opcionais de VELOCIDADE e de ALTURA de entrada.
	//   reward = 1 + speedScale * speedFrac + heightScale * heightFrac
	//   speedFrac  = velocidade da bola ao cruzar a linha / BALL_MAX_SPEED.
	//   heightFrac = 0 do chão até METADE da baliza; de meia-baliza ao TOPO sobe até 1.
	//                (z <= GOAL_HEIGHT/2 -> 0; z >= GOAL_HEIGHT -> 1).
	//   speedScale = heightScale = 0 -> comportamento antigo (golo = 1).
	//   concedeScale aplica-se a quem SOFRE o golo (-1 = simétrico).
	class GoalReward : public Reward {
	public:
		float concedeScale; // multiplicador para a equipa que sofre o golo
		float speedScale;   // quanto a velocidade de entrada amplifica a reward
		float heightScale;  // quanto a altura de entrada (acima de meia-baliza) amplifica a reward

		GoalReward(float concedeScale = -1, float speedScale = 0, float heightScale = 0)
			: concedeScale(concedeScale), speedScale(speedScale), heightScale(heightScale) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			if (!state.goalScored)
				return 0;

			// Bónus pela velocidade da bola ao cruzar a linha (normalizado por BALL_MAX_SPEED).
			float speedFrac = RS_CLAMP(state.ball.vel.Length() / CommonValues::BALL_MAX_SPEED, 0, 1);

			// Bónus pela altura: 0 do chão até meia-baliza; de meia-baliza ao topo sobe até 1.
			float halfGoal = CommonValues::GOAL_HEIGHT * 0.5f;
			float heightFrac = RS_CLAMP(
				(state.ball.pos.z - halfGoal) / (CommonValues::GOAL_HEIGHT - halfGoal), 0, 1);

			float reward = 1.0f + speedScale * speedFrac + heightScale * heightFrac;

			bool scored = (player.team != RS_TEAM_FROM_Y(state.ball.pos.y));
			return scored ? reward : concedeScale * reward;
		}
	};

	// https://github.com/AechPro/rocket-league-gym-sim/blob/main/rlgym_sim/utils/reward_functions/common_rewards/misc_rewards.py
	class VelocityReward : public Reward {
	public:
		bool isNegative;
		VelocityReward(bool isNegative = false) : isNegative(isNegative) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			return player.vel.Length() / CommonValues::CAR_MAX_SPEED * (1 - 2 * isNegative);
		}
	};

	// https://github.com/AechPro/rocket-league-gym-sim/blob/main/rlgym_sim/utils/reward_functions/common_rewards/ball_goal_rewards.py
	class VelocityBallToGoalReward : public Reward {
	public:
		bool ownGoal = false;
		VelocityBallToGoalReward(bool ownGoal = false) : ownGoal(ownGoal) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			bool targetOrangeGoal = player.team == Team::BLUE;
			if (ownGoal)
				targetOrangeGoal = !targetOrangeGoal;

			Vec targetPos = targetOrangeGoal ? CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;

			Vec ballToGoal = targetPos - state.ball.pos;
			float dist = ballToGoal.Length();
			if (dist < 1e-6f) return 0.0f;
			Vec ballDirToGoal = ballToGoal / dist;
			return ballDirToGoal.Dot(state.ball.vel / CommonValues::BALL_MAX_SPEED);
		}
	};

	// https://github.com/AechPro/rocket-league-gym-sim/blob/main/rlgym_sim/utils/reward_functions/common_rewards/player_ball_rewards.py
	class VelocityPlayerToBallReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			Vec diff = state.ball.pos - player.pos;
			float dist = diff.Length();
			if (dist < 1e-6f) return 0.0f;
			Vec dirToBall = diff / dist;
			Vec normVel = player.vel / CommonValues::CAR_MAX_SPEED;
			return dirToBall.Dot(normVel);
		}
	};

	// https://github.com/AechPro/rocket-league-gym-sim/blob/main/rlgym_sim/utils/reward_functions/common_rewards/player_ball_rewards.py
	class FaceBallReward : public Reward {
	public:
		float retreatScale; // peso da penalização por se afastar (multiplica a vel de recuo normalizada)

		// Enquanto se APROXIMA da bola, dá o "apontar à bola" normal (forward · dirToBall).
		// Quando se AFASTA, em vez de 0 leva penalização proporcional à velocidade de recuo.
		// Assim deixa de compensar recuar para estabilizar o apontar perto da bola.
		FaceBallReward(float retreatScale = 1.0f) : retreatScale(retreatScale) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			Vec diff = state.ball.pos - player.pos;
			float dist = diff.Length();
			if (dist < 1e-6f) return 0.0f;
			Vec dirToBall = diff / dist;

			float faceDot  = player.rotMat.forward.Dot(dirToBall); // [-1,1] apontar à bola
			float approach = player.vel.Dot(dirToBall);            // >0 aproxima, <0 afasta

			if (approach >= 0.0f)
				return faceDot;                                    // a aproximar-se: FaceBall normal

			// a afastar-se: penaliza (negativo) proporcional à velocidade de recuo, em vez de 0
			return retreatScale * (approach / CommonValues::CAR_MAX_SPEED);
		}
	};


	class TouchBallReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			return player.ballTouchedStep;
		}
	};


	// Penaliza a bola a cair no chão — mas só quando é uma bola FÁCIL de atingir:
	// pouca velocidade horizontal (não vai a lado nenhum) e a cair de altura (vz alto).
	// O castigo (negativo) escala por dois fatores em [0,1]:
	//   horizFactor: 1 quando horiz=0; desce até 0 ao chegar a horizThresh (bola rápida
	//                na horizontal -> não é sitter -> não penaliza).
	//   vertFactor:  0 abaixo de zThresh (mal cai, quase no chão -> não interessa);
	//                sobe até 1 em zMax (cai de muito alto -> pior).
	// Usa a velocidade da bola ANTES do toque (state.prev) = velocidade de queda real.
	// Devolve magnitude em [0,1] -> dá-lhe um PESO NEGATIVO na main (como o WhiffReward).
	class BallTouchGroundPenalty : public Reward {
	public:
		float horizThresh; // velocidade horizontal a partir da qual NÃO penaliza
		float zThresh;     // velocidade vertical abaixo da qual NÃO penaliza
		float zMax;        // velocidade vertical para castigo máximo

		BallTouchGroundPenalty(float horizThresh = 1000.f, float zThresh = 500.f, float zMax = 2000.f)
			: horizThresh(horizThresh), zThresh(zThresh), zMax(zMax) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			if (!state.prev) return 0.0f;

			bool wasOnGround = state.prev->ball.pos.z <= 105.0f;
			bool isOnGround = state.ball.pos.z <= 105.0f;

			// Só no momento em que a bola bate no chão.
			if (wasOnGround || !isOnGround) return 0.0f;

			// Velocidade da bola ANTES do impacto (a velocidade da queda).
			Vec bvel = state.prev->ball.vel;
			float horiz = sqrtf(bvel.x * bvel.x + bvel.y * bvel.y);
			float vz = fabsf(bvel.z);

			// Horizontal: pior quanto mais perto de 0; nada acima de horizThresh.
			float horizFactor = RS_CLAMP(1.0f - horiz / horizThresh, 0.0f, 1.0f);
			// Vertical: nada abaixo de zThresh; sobe até 1 em zMax (cai de mais alto = pior).
			float vertFactor = RS_CLAMP((vz - zThresh) / (zMax - zThresh), 0.0f, 1.0f);

			return horizFactor * vertFactor; // [0,1] -> peso negativo na main
		}
	};

	// Penaliza WHIFFS: o carro chega muito perto da bola e AFASTA-SE sem lhe tocar (falhou o toque).
	// Binário, SEM escalas: devolve 1.0 no instante do whiff, 0 caso contrário.
	// É um castigo -> dá-lhe um PESO NEGATIVO no SchedulerConfig (como o ConstantPenalty).
	class WhiffReward : public Reward {
	public:
		float whiffDist; // distância (uu) abaixo da qual se considera "esteve perto o suficiente para tocar"

		WhiffReward(float whiffDist = 250.0f) : whiffDist(whiffDist) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev || !state.prev) return 0.0f;

			// Não conta se tocou agora ou no passo anterior (afastar-se após o toque não é whiff)
			if (player.ballTouchedStep || player.prev->ballTouchedStep) return 0.0f;

			float distNow  = (state.ball.pos - player.pos).Length();
			float distPrev = (state.prev->ball.pos - player.prev->pos).Length();

			// Whiff = esteve dentro de whiffDist e agora está a AFASTAR-SE sem ter tocado
			if (distPrev < whiffDist && distNow > distPrev)
				return 1.0f;

			return 0.0f;
		}
	};

	class SpeedReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			return player.vel.Length() / CommonValues::CAR_MAX_SPEED;
		}
	};

	class WavedashReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			if (!player.prev)
				return 0;

			if (player.isOnGround && (player.prev->isFlipping && !player.prev->isOnGround)) {
				return 1;
			} else {
				return 0;
			}
		}
	};

	class PickupBoostReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev)
				return 0;

			if (player.boost > player.prev->boost) {
				return sqrtf(player.boost / 100.f) - sqrtf(player.prev->boost / 100.f);
			} else {
				return 0;
			}
		}
	};

	// https://github.com/AechPro/rocket-league-gym-sim/blob/main/rlgym_sim/utils/reward_functions/common_rewards/misc_rewards.py
	class SaveBoostReward : public Reward {
	public:
		float exponent;
		SaveBoostReward(float exponent = 0.5f) : exponent(exponent) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			return RS_CLAMP(powf(player.boost / 100, exponent), 0, 1);
		}
	};


	class AirReward : public Reward {
	public:
		float heightThresh; // x: altura (z do carro) a partir da qual dá reward total
		float lowScale;     // fração da reward enquanto z está entre 0 e heightThresh
		float noTouchTime;  // segundos sem tocar na bola a partir dos quais a reward decai
		float noTouchScale; // multiplicador da reward depois de noTouchTime (0 = desliga)

		std::unordered_map<uint32_t, float> timeSinceTouch; // tempo sem tocar na bola, por carro

		// Premeia estar no ar, por patamares de altura:
		//   z entre 0 e heightThresh -> lowScale (ex.: 30%)
		//   z >= heightThresh        -> 1.0 (100%)
		// Após noTouchTime segundos SEM tocar na bola, a reward é multiplicada por
		// noTouchScale (evita ficar no ar a farmar sem ir à bola).
		AirReward(float heightThresh = 500.f, float lowScale = 0.3f,
			float noTouchTime = 3.0f, float noTouchScale = 0.0f)
			: heightThresh(heightThresh), lowScale(lowScale),
			  noTouchTime(noTouchTime), noTouchScale(noTouchScale) {}

		virtual void Reset(const GameState& initialState) override {
			timeSinceTouch.clear();
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// Atualiza o tempo desde o último toque na bola (por carro).
			float& t = timeSinceTouch[player.carId];
			if (player.ballTouchedStep) t = 0.0f;
			else                        t += state.deltaTime;

			if (player.isOnGround)
				return 0.0f;

			float reward = (player.pos.z >= heightThresh) ? 1.0f : lowScale;

			// Decai se passou demasiado tempo sem tocar na bola.
			if (t > noTouchTime)
				reward *= noTouchScale;

			return reward;
		}
	};

	// EXPERIMENTAL: incentiva a técnica de double jump + boost dirigido à bola — como os
	// jogadores fazem para chegar a aéreos mais depressa (dois saltos com boost intermitente).
	// Só conta quando TODAS se verificam:
	//   - está no ar e JÁ fez double jump (hasDoubleJumped; flips NÃO contam)
	//   - está a premir boost (input prevAction.boost == 1; deteta o INPUT, não o consumo,
	//     porque com boostUsedPerSecond = 0 o boost é infinito e nunca diminui)
	//   - a bola está mais alta que o robô (ou só um pouco abaixo: z_bola >= z_robô - belowTolerance)
	//   - a bola está minimamente elevada do chão (z_bola >= minBallHeight)
	// Reward fixa (1.0) quando as condições se cumprem — aproximar à bola é trabalho
	// de OUTRAS rewards; esta só premeia a técnica (double jump + boost para aéreo).
	class DoubleJumpBoostReward : public Reward {
	public:
		float belowTolerance; // quanto a bola pode estar ABAIXO do robô e ainda contar (uu)
		float minBallHeight;  // a bola tem de estar pelo menos a esta altura do chão (uu)

		DoubleJumpBoostReward(float belowTolerance = 100.f, float minBallHeight = 300.f)
			: belowTolerance(belowTolerance), minBallHeight(minBallHeight) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (player.isOnGround) return 0.0f;
			if (!player.hasDoubleJumped) return 0.0f;          // precisa do 2º salto (flip não conta)

			// Está a premir boost? (input, não consumo — boost é infinito neste setup)
			if (player.prevAction.boost < 0.5f) return 0.0f;

			// A bola tem de estar elevada do chão
			if (state.ball.pos.z < minBallHeight) return 0.0f;
			// A bola tem de estar mais alta que o robô (ou só um pouco abaixo)
			if (state.ball.pos.z < player.pos.z - belowTolerance) return 0.0f;

			// Condições cumpridas: premeia a técnica (não a aproximação à bola).
			return 1.0f;
		}
	};

	// Mostly based on the classic Necto rewards
	// Total reward output for speeding the ball up to MAX_REWARDED_BALL_SPEED is 1.0
	// The bot can do this slowly (putting) or quickly (shooting)
	class TouchAccelReward : public Reward {
	public:
		constexpr static float MAX_REWARDED_BALL_SPEED = RLGC::Math::KPHToVel(110);

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!state.prev)
				return 0;

			if (player.ballTouchedStep) {
				float prevSpeedFrac = RS_MIN(1, state.prev->ball.vel.Length() / MAX_REWARDED_BALL_SPEED);
				float curSpeedFrac = RS_MIN(1, state.ball.vel.Length() / MAX_REWARDED_BALL_SPEED);

				if (curSpeedFrac > prevSpeedFrac) {
					return (curSpeedFrac - prevSpeedFrac);
				} else {
					// Not speeding up the ball so we don't care
					return 0;
				}
			} else {
				return 0;
			}
		}
	};

	class StrongTouchReward : public Reward {
	public:
		float minRewardedVel, maxRewardedVel;
		StrongTouchReward(float minSpeedKPH = 20, float maxSpeedKPH = 130) {
			minRewardedVel = RLGC::Math::KPHToVel(minSpeedKPH);
			maxRewardedVel = RLGC::Math::KPHToVel(maxSpeedKPH);
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!state.prev)
				return 0;

			if (player.ballTouchedStep) {
				float hitForce = (state.ball.vel - state.prev->ball.vel).Length();
				if (hitForce < minRewardedVel)
					return 0;

				return RS_MIN(1, hitForce / maxRewardedVel);
			} else {
				return 0;
			}
		}
	};



	// Air-touch reward adaptada a BOOST INFINITO: como o carro pode ficar no ar
	// indefinidamente, escalar pela fração de tempo no ar é gameable. Em vez disso
	// é um GATE: tocar na bola estando no ar há pelo menos minAirTime E com a bola
	// a uma altura mínima minBallHeight. Reward fixa (1.0) quando tudo se cumpre.
	class AirTouchReward : public Reward {
	public:
		float minAirTime;    // tempo mínimo no ar (s) para contar (ex.: 0.5)
		float minBallHeight; // altura mínima da bola (uu, centro da bola) para contar

		AirTouchReward(float minAirTime = 0.5f, float minBallHeight = 600.f)
			: minAirTime(minAirTime), minBallHeight(minBallHeight) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.ballTouchedStep) return 0.0f;
			if (player.airTime < minAirTime) return 0.0f;        // tempo mínimo no ar
			if (state.ball.pos.z < minBallHeight) return 0.0f;   // altura mínima da bola
			return 1.0f;
		}
	};


	class AirDribbleReward : public Reward {
	public:
		int consecutiveAirTouches = 0;

		/**
		 * Rewards multiple consecutive touches in the air without the ball or player hitting the ground.
		 * Higher rewards are given for longer chains of air touches.
		 */
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (player.isOnGround || state.ball.pos.z < 200.0f) {
				consecutiveAirTouches = 0;
				return 0.0f;
			}

			if (player.ballTouchedStep) {
				consecutiveAirTouches++;
				// Exponential reward for maintaining the dribble: 1st touch = 1, 2nd = 2, 3rd = 4...
				// Cap at 64 touches to prevent overflow to Inf (0 * Inf = NaN)
				int capped = RS_MIN(consecutiveAirTouches, 64);
				return powf(2.0f, (float)capped - 1.0f);
			}

			return 0.0f;
		}

		virtual void Reset(const GameState& initialState) override {
			consecutiveAirTouches = 0;
		}
	};

	class WallLaunchReward : public Reward {
	public:
		/**
		 * Rewards jumping off the side walls and touching the ball shortly after.
		 * This encourages the bot to transition from wall-riding to aerial play.
		 */
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!state.prev) return 0.0f;

			// Check if player just left the ground/wall
			bool justLeftSurface = !player.isOnGround && state.prev->players[player.index].isOnGround;
			
			// Check if player is near the side walls (X-axis limits are approx +/- 4096)
			bool nearWall = fabsf(player.pos.x) > (CommonValues::SIDE_WALL_X - 200.0f);

			if (justLeftSurface && nearWall) {
				return 1.0f; // Reward the launch itself
			}
			
			// Bonus if they touch the ball while in the air after a wall launch
			if (player.ballTouchedStep && !player.isOnGround && nearWall) {
				return 2.0f;
			}

			return 0.0f;
		}
	};

	class LandAllFoursReward : public Reward {
	public:
		/**
		 * Rewards the player for landing on the ground with all four wheels (up-vector alignment).
		 * This prevents the bot from landing on its side or roof, losing speed.
		 */
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!state.prev) return 0.0f;

			// Detect the exact moment of landing
			bool justLanded = player.isOnGround && !state.prev->players[player.index].isOnGround;

			if (justLanded) {
				// Check if the car's UP vector is aligned with the World UP vector (0,0,1)
				// Using the rotation matrix forward/up/right vectors
				float alignment = player.rotMat.up.Dot(Vec(0, 0, 1));
				
				// Reward is 1.0 if perfectly upright, 0.0 if sideways or upside down
				return RS_MAX(0.0f, alignment);
			}

			return 0.0f;
		}
	};
		

    class ConstantReward : public Reward {
    public:
        virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
            return 1.0f;
        }
    };


	// ====================================================
	// Zeelan Rewards
	// ====================================================

	class GoalBonusReward : public Reward {
	public:
		float concedeScale; // multiplicador do castigo ao marcar na PRÓPRIA baliza (-1 = simétrico; 0 = sem castigo)
		GoalBonusReward(float concedeScale = -1.0f) : concedeScale(concedeScale) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!state.goalScored)
				return 0;


			float baseReward = 1.0f; // base reward for scoring a goal
			// Bonus based on velocity of the ball when it crosses the goal line (up to 25% of the reward)
			float ballSpeed = state.ball.vel.Length();
			float speedBonus = RS_CLAMP(ballSpeed / CommonValues::BALL_MAX_SPEED, 0, 1);
			
			// Bonus based on how close the ball is to the nearest goal corner when it crosses the line (up to 25% of the reward)
			bool targetOrangeGoal = player.team == Team::BLUE;
			Vec targetPos = targetOrangeGoal ? CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;
			Vec goalCorners[4] = {
				targetPos + Vec(-CommonValues::GOAL_WIDTH / 2, 0, 0) + Vec(0, 0, CommonValues::GOAL_HEIGHT), // Top left
				targetPos + Vec(CommonValues::GOAL_WIDTH / 2, 0, 0) + Vec(0, 0, CommonValues::GOAL_HEIGHT), // Top right
				targetPos + Vec(-CommonValues::GOAL_WIDTH / 2, 0, 0), // Bottom left
				targetPos + Vec(CommonValues::GOAL_WIDTH / 2, 0, 0) // Bottom right
			};
			float closestDist = FLT_MAX;

			for (int i = 0; i < 4; i++) {
				float dist = (state.ball.pos - goalCorners[i]).Length();
				if (dist < closestDist) {
					closestDist = dist;
				}
			}
			float cornerBonus = RS_CLAMP(1 - (closestDist / CommonValues::GOAL_WIDTH), 0, 1);

			float bonus = speedBonus * 1.0f + cornerBonus * 0.25f; // up to 0.5

			baseReward += bonus;
			// Give +bonus to scoring players, concedeScale*bonus to others (zero-sum)
			bool scored = (player.team != RS_TEAM_FROM_Y(state.ball.pos.y));
			return scored ? baseReward : concedeScale * baseReward;

			// this reward is [0, 1.5]. if we want to keep between [0, 1] we can divide by 1.5, 
			// but I think it's fine to have some rewards above 1 as long as they are not too high 
			// compared to other rewards in the vector
		}
	};

	// Recompensa de FLICK (dribble -> dodge para rematar a bola no ar).
	//
	// Fluxo (como pedido):
	//   1) Pequena reward no INSTANTE em que SALTA — incentiva o build-up do flick.
	//   2) Grande reward quando, NO AR e depois de ter feito um FLIP (dodge),
	//      TOCA na bola. O valor escala com dois fatores, ambos argumentos:
	//        - velScale    * (velocidade da bola APÓS o toque / BALL_MAX_SPEED)
	//        - heightScale * (altura do CENTRO da bola / CEILING_Z)
	//
	// Notas:
	//   - velFrac usa a velocidade da bola já depois do toque (igual ao resto do
	//     sistema: módulo da velocidade normalizado por BALL_MAX_SPEED, em [0,1]).
	//   - heightFrac usa ball.pos.z, que JÁ é o centro da bola, normalizado por CEILING_Z.
	//   - Exige um FLIP PARA A FRENTE (isFlipping + flipRelTorque.y > forwardThresh):
	//     obriga a rematar a bola com um flip para a frente, não lateral/trás/double jump.
	//   - Output até ~ (velScale + heightScale); ajusta os scalers + o peso no vetor.
	class FlickReward : public Reward {
	public:
		float velScale, heightScale, jumpReward, forwardThresh;

		FlickReward(float velScale = 1.0f, float heightScale = 1.0f, float jumpReward = 0.05f,
			float forwardThresh = 0.5f)
			: velScale(velScale), heightScale(heightScale), jumpReward(jumpReward),
			  forwardThresh(forwardThresh) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev) return 0.0f;

			// 1) Pequena reward no instante do salto (transição: agora saltou, antes não).
			bool justJumped = player.hasJumped && !player.prev->hasJumped;
			float reward = justJumped ? jumpReward : 0.0f;

			// 2) FLICK: no ar e a tocar na bola DURANTE um flip ATIVO PARA A FRENTE.
			// flipRelTorque.y > forwardThresh -> dodge para a frente (não lateral/trás).
			if (player.ballTouchedStep && !player.isOnGround && player.isFlipping
				&& player.flipRelTorque.y > forwardThresh) {
				float velFrac    = RS_MIN(1.0f, state.ball.vel.Length() / CommonValues::BALL_MAX_SPEED);
				float heightFrac = RS_CLAMP(state.ball.pos.z / CommonValues::CEILING_Z, 0.0f, 1.0f);
				reward += velScale * velFrac + heightScale * heightFrac;
			}

			return reward;
		}
	};

	// FlickTowardsBall: premeia um FLIP (dodge) que termina mais perto da bola.
	// Usa airTime (tempo no ar) para detetar o salto (airTime passa a > 0) e a
	// aterragem (airTime volta a 0). Guarda a distância à bola no salto; ao aterrar,
	// se ficou mais perto da bola, dá reward escalada pela velocidade EM DIREÇÃO à bola.
	//   - NÃO conta double jumps: exige hasFlipped (dodge) E airTime curto. Um airTime
	//     alto significa que subiu muito (double jump/aéreo) -> não é um flick rasteiro.
	//   - Exige FLIP PARA A FRENTE (flipRelTorque.y > forwardThresh): obriga a flipar
	//     para a frente (em direção à bola), não lateral/trás.
	//   - Estado por carro (distância no salto), limpo em cada Reset.
	class FlickTowardsBallReward : public Reward {
	public:
		float maxAirTime;    // tempo máximo no ar (s) para contar como flick (acima -> subiu demais)
		float forwardThresh; // flipRelTorque.y mínimo para o dodge contar como "para a frente"

		FlickTowardsBallReward(float maxAirTime = 0.6f, float forwardThresh = 0.5f)
			: maxAirTime(maxAirTime), forwardThresh(forwardThresh) {}

		std::unordered_map<uint32_t, float> distAtJump; // distância à bola quando saiu do chão

		virtual void Reset(const GameState& initialState) override {
			distAtJump.clear();
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev) return 0.0f;

			// Saiu do chão: airTime começou a contar.
			if (!player.isOnGround && player.prev->isOnGround)
				distAtJump[player.carId] = (state.ball.pos - player.pos).Length();

			// Aterrou: airTime voltou a 0 -> o flip acabou, avalia.
			if (player.isOnGround && !player.prev->isOnGround) {
				// Só conta se houve um FLIP (dodge); double jump não define hasFlipped.
				if (!player.prev->hasFlipped) return 0.0f;

				// O dodge tem de ter sido PARA A FRENTE (em direção à bola), não lateral/trás.
				if (player.prev->flipRelTorque.y <= forwardThresh) return 0.0f;

				// airTime alto = subiu muito (double jump/aéreo) -> não é flick rasteiro.
				if (player.prev->airTime > maxAirTime) return 0.0f;

				auto it = distAtJump.find(player.carId);
				if (it == distAtJump.end()) return 0.0f;

				float dist = (state.ball.pos - player.pos).Length();
				float gained = it->second - dist;   // quanto se aproximou durante o flip (>0 = mais perto)
				if (gained <= 0.0f) return 0.0f;     // não ficou mais perto da bola

				// Escala pela velocidade MÉDIA de aproximação durante o flip, não pela
				// velocidade instantânea na aterragem (que muitas vezes já é ~0 ou negativa).
				// avgClosing = distância ganha / tempo de voo.
				float airT = RS_MAX(player.prev->airTime, 1e-3f);
				float avgClosing = gained / airT;
				return RS_CLAMP(avgClosing / CommonValues::CAR_MAX_SPEED, 0.0f, 1.0f);
			}

			return 0.0f;
		}
	};

	// Reward para o bot APRENDER o SPEED FLIP para a frente (SEM estado/maps).
	// Um dodge para a frente puro faz faceplant (nariz no chão) e MATA a velocidade;
	// o speed flip cancela e fica DIREITO, mantendo o boost a empurrar para a frente.
	// Premeia VELOCIDADE PARA A FRENTE × VERTICALIDADE, enquanto está no ar após um
	// dodge para a frente:
	//   - hasFlipped: true desde o dodge até aterrar (campo do próprio carro).
	//   - flipRelTorque.y > forwardThresh: o dodge foi para a frente.
	//   - reward = speedFrac × max(0, up.z) × alignment.
	//       speedFrac  = (vel · nariz)/CAR_MAX_SPEED  -> rápido para a frente.
	//       up.z       -> verticalidade (faceplant=baixo, cancelado/direito=alto).
	//       alignment  = (vel · nariz)/|vel| = cos(slip) -> nariz ALINHADO com a velocidade.
	//                    "traseira para o lado" (slip alto) -> alignment baixo -> mata a reward.
	// Notas: up.z evita o exploit de rodar; alignment força a forma boa (sem derrapar).
	class ForwardFlipReward : public Reward {
	public:
		float forwardThresh; // flipRelTorque.y mínimo para o dodge contar como "para a frente"

		ForwardFlipReward(float forwardThresh = 0.5f) : forwardThresh(forwardThresh) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// Só após um dodge PARA A FRENTE, enquanto ainda no ar (hasFlipped reinicia ao aterrar).
			if (!player.hasFlipped) return 0.0f;
			if (player.flipRelTorque.y <= forwardThresh) return 0.0f;

			float fwdSpeed = player.vel.Dot(player.rotMat.forward); // velocidade na direção do nariz
			if (fwdSpeed <= 0.0f) return 0.0f;

			float speedFrac = RS_CLAMP(fwdSpeed / CommonValues::CAR_MAX_SPEED, 0.0f, 1.0f);
			float upright   = RS_MAX(0.0f, player.rotMat.up.z);     // 1 = direito, 0/neg = faceplant

			// Alinhamento nariz<->velocidade (cos do ângulo de derrapagem). 1 = perfeito.
			float speed = player.vel.Length();
			float alignment = (speed > 1e-6f) ? RS_CLAMP(fwdSpeed / speed, 0.0f, 1.0f) : 0.0f;

			return speedFrac * upright * alignment;
		}
	};

}
