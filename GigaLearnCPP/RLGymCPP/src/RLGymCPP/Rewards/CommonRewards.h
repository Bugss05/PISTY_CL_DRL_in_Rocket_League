#pragma once
#include "Reward.h"
#include "../Math.h"

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

	// Rewards a goal by anyone on the team
	// NOTE: Already zero-sum
	class GoalReward : public Reward {
	public:
		float concedeScale;
		GoalReward(float concedeScale = -1) : concedeScale(concedeScale) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			if (!state.goalScored)
				return 0;

			bool scored = (player.team != RS_TEAM_FROM_Y(state.ball.pos.y));
			return scored ? 1 : concedeScale;
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
			
			Vec ballDirToGoal = (targetPos - state.ball.pos).Normalized();
			return ballDirToGoal.Dot(state.ball.vel / CommonValues::BALL_MAX_SPEED);
		}
	};

	// https://github.com/AechPro/rocket-league-gym-sim/blob/main/rlgym_sim/utils/reward_functions/common_rewards/player_ball_rewards.py
	class VelocityPlayerToBallReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			Vec dirToBall = (state.ball.pos - player.pos).Normalized();
			Vec normVel = player.vel / CommonValues::CAR_MAX_SPEED;
			return dirToBall.Dot(normVel);
		}
	};

	// https://github.com/AechPro/rocket-league-gym-sim/blob/main/rlgym_sim/utils/reward_functions/common_rewards/player_ball_rewards.py
	class FaceBallReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			Vec dirToBall = (state.ball.pos - player.pos).Normalized();
			return player.rotMat.forward.Dot(dirToBall);
		}
	};

	/*
	class TouchBallReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			return player.ballTouchedStep;
		}
	};
	*/

	class BallTouchGroundPenalty : public Reward {
	public:
		float penalty;
		BallTouchGroundPenalty(float penalty = -5.0f) : penalty(penalty) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			// If the ball is touching the ground (z <= 100 roughly corresponds to ball radius touching the floor)
			if (state.ball.pos.z <= 105.0f && state.ball.vel.z <= 10.0f) {
				return penalty;
			}
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
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			return !player.isOnGround;
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

// by Diogo Amaral
	class BallBetweenPlayerAndGoalReward : public Reward {
	public: 
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// This reward incentives the agent to position itself such that the ball is between itself and the goal, 
			// which is a typical good position for shooting.


			// Get the goal we are attacking
			bool targetOrangeGoal = player.team == Team::BLUE;
			Vec targetPos = targetOrangeGoal ? CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;

			// Get the angle between the player->ball and player->goal vectors. The smaller the angle, the better
			Vec playerToBall = state.ball.pos - player.pos;
			Vec playerToGoal = targetPos - player.pos;

			float angle = acosf(playerToBall.Normalized().Dot(playerToGoal.Normalized()));

			// Reward is 1 when the ball is perfectly between the player and the goal, and approaches 0 as the angle increases, reaching 0 at 180 degrees
			// Returns: [0, 1]
			return (1 - angle / M_PI);
		}
	};

	class ShotHitsTargetinGoalReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// This reward incentives the agent to hit the ball towards the corners of the goal, 
			// which are harder for the opponent to save. 

			// THIS IS ONLY REWARDED TO THE AGENT IF THE SCORES!!!
			if (!state.goalScored) {
            	return 0.0f;
        	}
			bool scored = (player.team != RS_TEAM_FROM_Y(state.ball.pos.y));
			if (!scored) {
				return 0.0f;
			}

			// Get goal that we are attacking
			bool targetOrangeGoal = player.team == Team::BLUE;
			Vec targetPos = targetOrangeGoal ? CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;

			// Get all 4 goal corners (desired ball hit locations)
			Vec goalCorners[4] = {
				targetPos + Vec(-CommonValues::GOAL_WIDTH / 2, 0, 0) + Vec(0, 0, CommonValues::GOAL_HEIGHT), // Top left
				targetPos + Vec(CommonValues::GOAL_WIDTH / 2, 0, 0) + Vec(0, 0, CommonValues::GOAL_HEIGHT), // Top right
				targetPos + Vec(-CommonValues::GOAL_WIDTH / 2, 0, 0), // Bottom left
				targetPos + Vec(CommonValues::GOAL_WIDTH / 2, 0, 0) // Bottom right
			};

			// Get the closest corner to the ball
			float closestDist = FLT_MAX;
			for (int i = 0; i < 4; i++) {
				float dist = (state.ball.pos - goalCorners[i]).Length();
				if (dist < closestDist) {
					closestDist = dist;
				}
			}

			// Reward is between 0 and 1, approaching 1 as the ball hits the goal closer to the corners, and approaching 0 as it hits closer to the center. 
			// A hit in the center is still rewarded by the GoalReward, just less than if it hit in the corners
			return 1 - (closestDist / CommonValues::GOAL_WIDTH);
		}
	};

	class SaveGoalReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// This reward incetives the agent to save goals and clear danger.
			// It calculates if the ball was moving towards the agent's own goal, 
			// and if after the agent touched the ball and it is no longer moving towards the goal, 
			// then we consider that a "save" and rewards the agent based on the distance to the goal.

			// Get the goal we are defending
			bool defendingOrangeGoal = player.team == Team::ORANGE;
			Vec defendingGoalPos = defendingOrangeGoal ? CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;

			// Check if the ball was moving towards our goal in the previous step
			if (!state.prev)
				return 0;

			Vec ballToGoalPrev = defendingGoalPos - state.prev->ball.pos;
			bool wasMovingTowardsGoal = ballToGoalPrev.Dot(state.prev->ball.vel) > 0;
			if (!wasMovingTowardsGoal)
				return 0;
			
			// Check if the agent touched the ball in this step
			if (!player.ballTouchedStep)
				return 0;

			// Check if the ball is no longer moving towards the goal after the touch
			Vec ballToGoalCurrent = defendingGoalPos - state.ball.pos;
			bool isNoLongerMovingTowardsGoal = ballToGoalCurrent.Dot(state.ball.vel) <= 0;
			if (!isNoLongerMovingTowardsGoal)
				return 0;

			// Reward is based on the distance of the ball to our goal, the closer the ball is to the goal, 
			// the higher the reward, with a max of 1 when the ball is at the goal line. Returns [0, 1].
			float distToGoal = ballToGoalCurrent.Length();
			return RS_CLAMP(1 - (distToGoal / CommonValues::FIELD_LENGTH), 0, 1);

		}
	};

	class TouchBallReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// Returns a reward for touching the ball, with a multiplier for aerial touches.
			if (!player.ballTouchedStep) {
				return 0.0f;
			}
			float reward = 1.0f;
			const float DOUBLE_JUMP_HEIGHT = 500.0f;
			if (!player.isOnGround && player.pos.z > DOUBLE_JUMP_HEIGHT) {
				reward *= 3.0f;
			}
			return reward;
		}
	};

	class AirAlignmentReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (player.isOnGround || player.vel.Length() < 100.0f) return 0.0f;

			Vec dirToBall = (state.ball.pos - player.pos).Normalized();
			Vec velDir = player.vel.Normalized();
			
			float alignment = velDir.Dot(dirToBall); // 1.0 se estiver perfeito
			return RS_MAX(0.0f, alignment);
		}
	};

	class HeightMatchReward : public Reward {
	public:
		/**
		 * Incentivizes the player to match the ball's height (Z-axis) as they get closer horizontally.
		 * This helps prevent "whiffing" by flying over or under the ball.
		 */
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// Calculate horizontal distance (XY plane only)
			float dist2D = (Vec(player.pos.x, player.pos.y, 0) - Vec(state.ball.pos.x, state.ball.pos.y, 0)).Length();
			
			// Calculate height difference
			float heightDiff = fabsf(player.pos.z - state.ball.pos.z);
			
			// We only care about height matching when the player is relatively close (e.g., within 1500 units)
			float proximityFactor = RS_CLAMP(1.0f - (dist2D / 1500.0f), 0.0f, 1.0f);
			
			// Reward is higher when heightDiff is small and proximity is high
			// Max height of the arena is ~2000
			float heightFactor = RS_CLAMP(1.0f - (heightDiff / 1000.0f), 0.0f, 1.0f);
			
			return proximityFactor * heightFactor;
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
				return powf(2.0f, (float)consecutiveAirTouches - 1.0f);
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
		
	class CmonDoSomethingReward : public Reward {
    public:
        int stepsSinceLastTouch = 0;

        /**
         * Penalizes the agent quadratically for going too long without touching the ball.
         * The penalty starts small (giving the bot time to rotate or get boost) 
         * but escalates aggressively the longer the bot stays inactive.
         */
        virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
            if (player.ballTouchedStep) {
                // Reset the counter immediately upon contact
                stepsSinceLastTouch = 0;
                return 0.0f;
            } else {
                stepsSinceLastTouch++;
            }

            // Convert steps to seconds (assuming 120Hz physics)
            float secondsInactive = (float)stepsSinceLastTouch / 120.0f;
            return secondsInactive * secondsInactive;
        }

        virtual void Reset(const GameState& initialState) override {
            stepsSinceLastTouch = 0;
        }
    };

	class TeremMoffiReward : public Reward {
    public:
        int stepsSinceLastGoal = 0;

        /*
         * Penalizes the agent for going too long without scoring a goal (Moffi ball).
         * This implements a Time-to-Goal Penalty using log-space, making sure the agent 
         * actively searches for the fastest path (brachistochrone) to score.
         * The penalty grows logarithmically to prevent gradient saturation in long episodes.
         */
        virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
            // 1. Check if our team scored in this step
            bool scored = state.goalScored && (player.team != RS_TEAM_FROM_Y(state.ball.pos.y));

            if (scored) {
                // Reset the timer when a goal is scored
                stepsSinceLastGoal = 0;
                return 0.0f;
            } else {
                stepsSinceLastGoal++;
            }

            // Convert steps to seconds (assuming 120Hz physics)
            float secondsSinceGoal = (float)stepsSinceLastGoal / 120.0f;

            // Log-Space Penalty calculation:
            // We use log1pf (log(1 + x)) to ensure the result is 0.0f at 0 seconds,
            // and grows smoothly afterwards.
            float logPenalty = log1pf(secondsSinceGoal);

            // This returns a positive value designed to be used with a NEGATIVE weight in the main vector
            return logPenalty;
        }

        virtual void Reset(const GameState& initialState) override {
            stepsSinceLastGoal = 0;
        }
    };

	class GoalDistancePotentialReward : public Reward {
    public:
        /**
         * Calculates the difference in potential (distance from ball to target goal)
         * between the current step and the previous step.
         * Reward = Potential(t) - Potential(t-1)
         */
        virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
            if (!state.prev) return 0.0f;

            // Define target goal based on team
            bool targetOrangeGoal = player.team == Team::BLUE;
            Vec targetPos = targetOrangeGoal ? CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;

            // Calculate distances for current and previous frame
            float distCurrent = (state.ball.pos - targetPos).Length();
            float distPrev = (state.prev->ball.pos - targetPos).Length();

            // We divide by a normalization factor (e.g., max field length ~10200) 
            // so the step difference is on a well-behaved scale.
            float potentialCurrent = -distCurrent / CommonValues::FIELD_LENGTH;
            float potentialPrev = -distPrev / CommonValues::FIELD_LENGTH;

            // Return the potential difference (shaping)
            return potentialCurrent - potentialPrev;
        }
    };

	class VeloAlignmentReward : public Reward {
    public:
        /**
         * Computes the Scalar Projection of the car's velocity onto the direction vector to the ball.
         * Ensures that driving fast is only rewarded if it is directed towards the target.
         */
        virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
            Vec playerToBall = state.ball.pos - player.pos;
            if (playerToBall.Length() < 1e-5f) return 0.0f;

            Vec dirToBall = playerToBall.Normalized();
            
            // Scalar projection (Dot product between raw velocity and normalized direction)
            // Normalized by CAR_MAX_SPEED to keep the reward range strictly within [-1.0, 1.0]
            float scalarProjection = player.vel.Dot(dirToBall) / CommonValues::CAR_MAX_SPEED;

            return scalarProjection;
        }
    };

	class ActionSmoothingPenalty : public Reward {
    public:
        /**
         * Penalizes sudden changes in control inputs between consecutive frames.
         * This forces the agent to behave with human-like inertia and smoothness.
         */
        virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
            if (!player.prev) return 0.0f;

            // Calculate squared differences in steer, pitch, and yaw inputs
            float diffSteer = player.prevAction.steer - player.prev->prevAction.steer;
            float diffPitch = player.prevAction.pitch - player.prev->prevAction.pitch;
            float diffYaw = player.prevAction.yaw - player.prev->prevAction.yaw;

            float squaredDiffSum = (diffSteer * diffSteer) + (diffPitch * diffPitch) + (diffYaw * diffYaw);

            // Returns a positive penalty scale to be multiplied by a negative weight in the vector
            return squaredDiffSum;
        }
    };

    class ConstantReward : public Reward {
    public:
        virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
            return 1.0f;
        }
    };
}
