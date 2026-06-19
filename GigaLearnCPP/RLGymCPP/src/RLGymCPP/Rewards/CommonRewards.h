#pragma once
#include "Reward.h"
#include "../Math.h"
#include <unordered_map>
#include <vector>

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

	// Rewards a goal, with optional bonuses for entry SPEED and HEIGHT.
	//   reward = 1 + speedScale * speedFrac + heightScale * heightFrac
	//   speedFrac  = ball speed when crossing the line / BALL_MAX_SPEED.
	//   heightFrac = 0 from the ground up to HALF the goal; from half-goal to the TOP it rises to 1.
	//                (z <= GOAL_HEIGHT/2 -> 0; z >= GOAL_HEIGHT -> 1).
	//   speedScale = heightScale = 0 -> old behaviour (goal = 1).
	//   concedeScale applies to whoever CONCEDES the goal (-1 = symmetric).
	class GoalReward : public Reward {
	public:
		float concedeScale; // multiplier for the team that concedes the goal
		float speedScale;   // how much the entry speed amplifies the reward
		float heightScale;  // how much the entry height (above half-goal) amplifies the reward

		GoalReward(float concedeScale = -1, float speedScale = 0, float heightScale = 0)
			: concedeScale(concedeScale), speedScale(speedScale), heightScale(heightScale) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			if (!state.goalScored)
				return 0;

			// Bonus for the ball speed when crossing the line (normalized by BALL_MAX_SPEED).
			float speedFrac = RS_CLAMP(state.ball.vel.Length() / CommonValues::BALL_MAX_SPEED, 0, 1);

			// Height bonus: 0 from the ground to half-goal; from half-goal to the top it rises to 1.
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
		float retreatScale; // weight of the penalty for moving away (multiplies the normalized retreat speed)

		// While APPROACHING the ball, gives the normal "facing the ball" (forward · dirToBall).
		// When MOVING AWAY, instead of 0 it gets a penalty proportional to the retreat speed.
		// This way retreating no longer pays off as a way to stabilize the aim near the ball.
		FaceBallReward(float retreatScale = 1.0f) : retreatScale(retreatScale) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			Vec diff = state.ball.pos - player.pos;
			float dist = diff.Length();
			if (dist < 1e-6f) return 0.0f;
			Vec dirToBall = diff / dist;

			float faceDot  = player.rotMat.forward.Dot(dirToBall); // [-1,1] facing the ball
			float approach = player.vel.Dot(dirToBall);            // >0 approaching, <0 moving away

			if (approach >= 0.0f)
				return faceDot;                                    // approaching: normal FaceBall

			// moving away: penalizes (negative) proportional to the retreat speed, instead of 0
			return retreatScale * (approach / CommonValues::CAR_MAX_SPEED);
		}
	};


	class TouchBallReward : public Reward {
	public:
		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			return player.ballTouchedStep;
		}
	};


	// Penalizes the ball falling to the ground — but only when it is an EASY ball to reach:
	// little horizontal speed (going nowhere) and falling from height (high vz).
	// The penalty (negative) scales by two factors in [0,1]:
	//   horizFactor: 1 when horiz=0; drops to 0 on reaching horizThresh (ball fast
	//                horizontally -> not a sitter -> no penalty).
	//   vertFactor:  0 below zThresh (barely falling, almost on the ground -> doesn't matter);
	//                rises to 1 at zMax (falling from very high -> worse).
	// Uses the ball speed BEFORE the touch (state.prev) = the real falling speed.
	// Returns magnitude in [0,1] -> give it a NEGATIVE WEIGHT in main (like WhiffReward).
	class BallTouchGroundPenalty : public Reward {
	public:
		float horizThresh; // horizontal speed at/above which it does NOT penalize
		float zThresh;     // vertical speed below which it does NOT penalize
		float zMax;        // vertical speed for maximum penalty

		BallTouchGroundPenalty(float horizThresh = 1000.f, float zThresh = 500.f, float zMax = 2000.f)
			: horizThresh(horizThresh), zThresh(zThresh), zMax(zMax) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) {
			if (!state.prev) return 0.0f;

			bool wasOnGround = state.prev->ball.pos.z <= 105.0f;
			bool isOnGround = state.ball.pos.z <= 105.0f;

			// Only at the moment the ball hits the ground.
			if (wasOnGround || !isOnGround) return 0.0f;

			// Ball speed BEFORE the impact (the falling speed).
			Vec bvel = state.prev->ball.vel;
			float horiz = sqrtf(bvel.x * bvel.x + bvel.y * bvel.y);
			float vz = fabsf(bvel.z);

			// Horizontal: worse the closer to 0; nothing above horizThresh.
			float horizFactor = RS_CLAMP(1.0f - horiz / horizThresh, 0.0f, 1.0f);
			// Vertical: nothing below zThresh; rises to 1 at zMax (falling from higher = worse).
			float vertFactor = RS_CLAMP((vz - zThresh) / (zMax - zThresh), 0.0f, 1.0f);

			return horizFactor * vertFactor; // [0,1] -> negative weight in main
		}
	};

	// Penalizes WHIFFS: the car gets very close to the ball and MOVES AWAY without touching it (missed the touch).
	// Binary, NO scaling: returns 1.0 at the moment of the whiff, 0 otherwise.
	// It is a penalty -> give it a NEGATIVE WEIGHT in SchedulerConfig (like ConstantPenalty).
	class WhiffReward : public Reward {
	public:
		float whiffDist; // distance (uu) below which it is considered "was close enough to touch"

		WhiffReward(float whiffDist = 250.0f) : whiffDist(whiffDist) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev || !state.prev) return 0.0f;

			// Doesn't count if it touched now or on the previous step (moving away after the touch is not a whiff)
			if (player.ballTouchedStep || player.prev->ballTouchedStep) return 0.0f;

			float distNow  = (state.ball.pos - player.pos).Length();
			float distPrev = (state.prev->ball.pos - player.prev->pos).Length();

			// Whiff = was within whiffDist and is now MOVING AWAY without having touched
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
		float heightThresh; // height (car z) at/above which it gives the full reward
		float lowScale;     // fraction of the reward while z is between 0 and heightThresh
		float noTouchTime;  // seconds without touching the ball after which the reward decays
		float noTouchScale; // reward multiplier after noTouchTime (0 = off)

		std::unordered_map<uint32_t, float> timeSinceTouch; // time without touching the ball, per car

		// Rewards being in the air, by height tiers:
		//   z between 0 and heightThresh -> lowScale (e.g.: 30%)
		//   z >= heightThresh            -> 1.0 (100%)
		// After noTouchTime seconds WITHOUT touching the ball, the reward is multiplied by
		// noTouchScale (avoids staying in the air farming without going to the ball).
		AirReward(float heightThresh = 500.f, float lowScale = 0.3f,
			float noTouchTime = 3.0f, float noTouchScale = 0.0f)
			: heightThresh(heightThresh), lowScale(lowScale),
			  noTouchTime(noTouchTime), noTouchScale(noTouchScale) {}

		virtual void Reset(const GameState& initialState) override {
			timeSinceTouch.clear();
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// Updates the time since the last ball touch (per car).
			float& t = timeSinceTouch[player.carId];
			if (player.ballTouchedStep) t = 0.0f;
			else                        t += state.deltaTime;

			if (player.isOnGround)
				return 0.0f;

			float reward = (player.pos.z >= heightThresh) ? 1.0f : lowScale;

			// Decays if too much time passed without touching the ball.
			if (t > noTouchTime)
				reward *= noTouchScale;

			return reward;
		}
	};

	// EXPERIMENTAL: encourages the double jump + boost-toward-the-ball technique — like
	// players do to reach aerials faster (two jumps with intermittent boost).
	// Only counts when ALL hold:
	//   - is in the air and has ALREADY double jumped (hasDoubleJumped; flips do NOT count)
	//   - is pressing boost (input prevAction.boost == 1; detects the INPUT, not the consumption,
	//     because with boostUsedPerSecond = 0 the boost is infinite and never decreases)
	//   - the ball is higher than the car (or just slightly below: z_ball >= z_car - belowTolerance)
	//   - the ball is minimally raised off the ground (z_ball >= minBallHeight)
	// Fixed reward (1.0) when the conditions are met — getting close to the ball is the job
	// of OTHER rewards; this one only rewards the technique (double jump + boost for aerial).
	class DoubleJumpBoostReward : public Reward {
	public:
		float belowTolerance; // how far the ball can be BELOW the car and still count (uu)
		float minBallHeight;  // the ball must be at least this high off the ground (uu)

		DoubleJumpBoostReward(float belowTolerance = 100.f, float minBallHeight = 300.f)
			: belowTolerance(belowTolerance), minBallHeight(minBallHeight) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (player.isOnGround) return 0.0f;
			if (!player.hasDoubleJumped) return 0.0f;          // needs the 2nd jump (flip doesn't count)

			// Is it pressing boost? (input, not consumption — boost is infinite in this setup)
			if (player.prevAction.boost < 0.5f) return 0.0f;

			// The ball must be raised off the ground
			if (state.ball.pos.z < minBallHeight) return 0.0f;
			// The ball must be higher than the car (or just slightly below)
			if (state.ball.pos.z < player.pos.z - belowTolerance) return 0.0f;

			// Conditions met: rewards the technique (not the approach to the ball).
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
		float heightForDouble; // height (uu) at which the height multiplier reaches 2
		StrongTouchReward(float minSpeedKPH = 20, float maxSpeedKPH = 130,
			float heightForDouble = CommonValues::CEILING_Z) {
			minRewardedVel = RLGC::Math::KPHToVel(minSpeedKPH);
			maxRewardedVel = RLGC::Math::KPHToVel(maxSpeedKPH);
			this->heightForDouble = heightForDouble;
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!state.prev)
				return 0;

			if (player.ballTouchedStep) {
				float hitForce = (state.ball.vel - state.prev->ball.vel).Length();
				if (hitForce < minRewardedVel)
					return 0;

				float base = RS_MIN(1, hitForce / maxRewardedVel);
				// HEIGHT multiplier in [1, 2]: 1 on the ground, rises to 2 at heightForDouble.
				float heightMult = 1.0f + RS_CLAMP(state.ball.pos.z / heightForDouble, 0.0f, 1.0f);
				return base * heightMult;
			} else {
				return 0;
			}
		}
	};



	// Air-touch reward adapted to INFINITE BOOST: since the car can stay in the air
	// indefinitely, scaling by the fraction of time in the air is gameable. Instead
	// it is a GATE: touch the ball while airborne for at least minAirTime AND with the ball
	// at a minimum height minBallHeight. Fixed reward (1.0) when everything holds.
	class AirTouchReward : public Reward {
	public:
		float minAirTime;    // minimum time in the air (s) to count (e.g.: 0.5)
		float minBallHeight; // minimum ball height (uu, ball center) to count

		AirTouchReward(float minAirTime = 0.5f, float minBallHeight = 600.f)
			: minAirTime(minAirTime), minBallHeight(minBallHeight) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.ballTouchedStep) return 0.0f;
			if (player.airTime < minAirTime) return 0.0f;        // minimum time in the air
			if (state.ball.pos.z < minBallHeight) return 0.0f;   // minimum ball height
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


	// FLICK reward (dribble -> dodge to hit the ball in the air).
	//
	// Flow (as requested):
	//   1) Small reward at the MOMENT it JUMPS — encourages the flick build-up.
	//   2) Big reward when, IN THE AIR and after having done a FLIP (dodge),
	//      it TOUCHES the ball. The value scales with two factors, both arguments:
	//        - velScale    * (ball speed AFTER the touch / BALL_MAX_SPEED)
	//        - heightScale * (height of the ball CENTER / CEILING_Z)
	//
	// Notes:
	//   - velFrac uses the ball speed after the touch (like the rest of the
	//     system: speed magnitude normalized by BALL_MAX_SPEED, in [0,1]).
	//   - heightFrac uses ball.pos.z, which IS already the ball center, normalized by CEILING_Z.
	//   - Requires a FORWARD FLIP (isFlipping + flipRelTorque.y > forwardThresh):
	//     forces hitting the ball with a forward flip, not sideways/backwards/double jump.
	//   - Output up to ~ (velScale + heightScale); tune the scalers + the weight in the vector.
	class FlickReward : public Reward {
	public:
		float velScale, heightScale, jumpReward, forwardThresh;

		FlickReward(float velScale = 1.0f, float heightScale = 1.0f, float jumpReward = 0.05f,
			float forwardThresh = 0.5f)
			: velScale(velScale), heightScale(heightScale), jumpReward(jumpReward),
			  forwardThresh(forwardThresh) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev) return 0.0f;

			// 1) Small reward at the moment of the jump (transition: jumped now, not before).
			bool justJumped = player.hasJumped && !player.prev->hasJumped;
			float reward = justJumped ? jumpReward : 0.0f;

			// 2) FLICK: in the air and touching the ball DURING an ACTIVE FORWARD flip.
			// flipRelTorque.y > forwardThresh -> forward dodge (not sideways/backwards).
			if (player.ballTouchedStep && !player.isOnGround && player.isFlipping
				&& player.flipRelTorque.y > forwardThresh) {
				float velFrac    = RS_MIN(1.0f, state.ball.vel.Length() / CommonValues::BALL_MAX_SPEED);
				float heightFrac = RS_CLAMP(state.ball.pos.z / CommonValues::CEILING_Z, 0.0f, 1.0f);
				reward += velScale * velFrac + heightScale * heightFrac;
			}

			return reward;
		}
	};

	// FlickTowardsBall: rewards a FLIP (dodge) that ends closer to the ball.
	// Uses airTime (time in the air) to detect the jump (airTime becomes > 0) and the
	// landing (airTime returns to 0). Stores the distance to the ball at the jump; on landing,
	// if it got closer to the ball, gives a reward scaled by the speed TOWARD the ball.
	//   - Does NOT count double jumps: requires hasFlipped (dodge) AND a short airTime. A high
	//     airTime means it went up a lot (double jump/aerial) -> not a low flick.
	//   - Requires a FORWARD FLIP (flipRelTorque.y > forwardThresh): forces flipping
	//     forward (toward the ball), not sideways/backwards.
	//   - Per-car state (distance at the jump), cleared on each Reset.
	class FlickTowardsBallReward : public Reward {
	public:
		float maxAirTime;    // maximum time in the air (s) to count as a flick (above -> went up too much)
		float forwardThresh; // minimum flipRelTorque.y for the dodge to count as "forward"

		FlickTowardsBallReward(float maxAirTime = 0.6f, float forwardThresh = 0.5f)
			: maxAirTime(maxAirTime), forwardThresh(forwardThresh) {}

		std::unordered_map<uint32_t, float> distAtJump; // distance to the ball when it left the ground

		virtual void Reset(const GameState& initialState) override {
			distAtJump.clear();
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (!player.prev) return 0.0f;

			// Left the ground: airTime started counting.
			if (!player.isOnGround && player.prev->isOnGround)
				distAtJump[player.carId] = (state.ball.pos - player.pos).Length();

			// Landed: airTime returned to 0 -> the flip ended, evaluate.
			if (player.isOnGround && !player.prev->isOnGround) {
				// Only counts if there was a FLIP (dodge); double jump does not set hasFlipped.
				if (!player.prev->hasFlipped) return 0.0f;

				// The dodge must have been FORWARD (toward the ball), not sideways/backwards.
				if (player.prev->flipRelTorque.y <= forwardThresh) return 0.0f;

				// high airTime = went up a lot (double jump/aerial) -> not a low flick.
				if (player.prev->airTime > maxAirTime) return 0.0f;

				auto it = distAtJump.find(player.carId);
				if (it == distAtJump.end()) return 0.0f;

				float dist = (state.ball.pos - player.pos).Length();
				float gained = it->second - dist;   // how much it got closer during the flip (>0 = closer)
				if (gained <= 0.0f) return 0.0f;     // did not get closer to the ball

				// Scales by the AVERAGE closing speed during the flip, not by the
				// instantaneous speed on landing (which is often already ~0 or negative).
				// avgClosing = distance gained / flight time.
				float airT = RS_MAX(player.prev->airTime, 1e-3f);
				float avgClosing = gained / airT;
				return RS_CLAMP(avgClosing / CommonValues::CAR_MAX_SPEED, 0.0f, 1.0f);
			}

			return 0.0f;
		}
	};

	// Reward for the bot to LEARN the forward SPEED FLIP (NO state/maps).
	// A pure forward dodge faceplants (nose on the ground) and KILLS the speed;
	// the speed flip cancels and stays UPRIGHT, keeping the boost pushing forward.
	// Rewards FORWARD SPEED × UPRIGHTNESS, while in the air after a
	// forward dodge:
	//   - hasFlipped: true from the dodge until landing (field of the car itself).
	//   - flipRelTorque.y > forwardThresh: the dodge was forward.
	//   - reward = speedFrac × max(0, up.z) × alignment.
	//       speedFrac  = (vel · nose)/CAR_MAX_SPEED  -> fast forward.
	//       up.z       -> uprightness (faceplant=low, cancelled/upright=high).
	//       alignment  = (vel · nose)/|vel| = cos(slip) -> nose ALIGNED with the velocity.
	//                    "rear sideways" (high slip) -> low alignment -> kills the reward.
	// Notes: up.z avoids the spinning exploit; alignment forces the good form (no sliding).
	class ForwardFlipReward : public Reward {
	public:
		float forwardThresh; // minimum flipRelTorque.y for the dodge to count as "forward"

		ForwardFlipReward(float forwardThresh = 0.5f) : forwardThresh(forwardThresh) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			// Only after a FORWARD dodge, while still in the air (hasFlipped resets on landing).
			if (!player.hasFlipped) return 0.0f;
			if (player.flipRelTorque.y <= forwardThresh) return 0.0f;

			float fwdSpeed = player.vel.Dot(player.rotMat.forward); // speed in the nose direction
			if (fwdSpeed <= 0.0f) return 0.0f;

			float speedFrac = RS_CLAMP(fwdSpeed / (CommonValues::CAR_MAX_SPEED/2.0f), 0.0f, 1.0f);
			float upright   = RS_MAX(0.0f, player.rotMat.up.z);     // 1 = upright, 0/neg = faceplant

			// Nose<->velocity alignment (cos of the slip angle). 1 = perfect.
			float speed = player.vel.Length();
			float alignment = (speed > 1e-6f) ? RS_CLAMP(fwdSpeed / speed, 0.0f, 1.0f) : 0.0f;

			return speedFrac * upright * alignment;
		}
	};

	// 1) AIR PROXIMITY: when the car is IN THE AIR (not on the ground nor on a wall) and the
	// ball is at a considerable height, rewards being NEAR the ball. Scales with
	// proximity × car height × car speed.
	//   - !isOnGround already excludes ground AND wall (wheels on a wall -> isOnGround true).
	//   - wallMargin excludes being glued to a wall (avoids wall-launch).
	class AirProximityReward : public Reward {
	public:
		float minBallHeight; // the ball must be above this height (uu) to count
		float maxDist;       // distance to the ball (uu) at/above which the proximity is 0
		float wallMargin;    // ignores if the car is within this of a wall (uu)

		AirProximityReward(float minBallHeight = 500.f, float maxDist = 2500.f, float wallMargin = 300.f)
			: minBallHeight(minBallHeight), maxDist(maxDist), wallMargin(wallMargin) {}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (player.isOnGround) return 0.0f;                       // must be in the air
			if (state.ball.pos.z < minBallHeight) return 0.0f;        // ball at a considerable height

			// Not glued to a wall (avoids counting wall-launches).
			if (fabsf(player.pos.x) > CommonValues::SIDE_WALL_X - wallMargin) return 0.0f;
			if (fabsf(player.pos.y) > CommonValues::BACK_WALL_Y - wallMargin) return 0.0f;

			float dist = (state.ball.pos - player.pos).Length();
			float proximity  = RS_CLAMP(1.0f - dist / maxDist, 0.0f, 1.0f);              // close = 1
			float heightFrac = RS_CLAMP(player.pos.z / CommonValues::CEILING_Z, 0.5f, 1.5f);
			return proximity * heightFrac;
		}
	};

	// 3) CONSECUTIVE AERIAL TOUCHES: each time it touches the ball IN THE AIR (above
	// minBallHeight) WITHOUT having landed, the reward grows (1×, 2×, 3×, ...). Resets on
	// landing. Encourages controlling/juggling the ball in the air.
	// It also SCALES with the TIME it took to touch the ball (interval since the
	// previous touch, or since it jumped for the 1st touch). Factor [1, 2] -> only INCREASES,
	// never decreases: more time until the touch -> larger the factor (up to maxGap seconds).
	// Per-player state via vectors indexed by player.index (cheap, no hashing).
	class ConsecutiveAirTouchReward : public Reward {
	public:
		float minBallHeight; // minimum ball height (uu) for the touch to count
		int   maxCount;      // cap of the count multiplier (avoids blowing up)
		float maxGap;        // time (s) up to which the time factor reaches 2

		ConsecutiveAirTouchReward(float minBallHeight = 400.f, int maxCount = 5, float maxGap = 1.0f)
			: minBallHeight(minBallHeight), maxCount(maxCount), maxGap(maxGap) {}

		std::vector<int>   touchCount;     // consecutive aerial touches, per player.index
		std::vector<float> timeSinceTouch; // time (s) since the last aerial touch, per player.index

		virtual void Reset(const GameState& initialState) override {
			touchCount.clear();
			timeSinceTouch.clear();
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			int idx = player.index;
			if ((int)touchCount.size() <= idx) {
				touchCount.resize(idx + 1, 0);
				timeSinceTouch.resize(idx + 1, 0.f);
			}

			// Accumulates the time since the last aerial touch.
			timeSinceTouch[idx] += state.deltaTime;

			// Landed -> resets the count and the timer.
			if (player.isOnGround) {
				touchCount[idx] = 0;
				timeSinceTouch[idx] = 0.f;
				return 0.0f;
			}

			// Touch in the air with minimum height -> increments and rewards.
			if (player.ballTouchedStep && state.ball.pos.z >= minBallHeight) {
				if (touchCount[idx] < maxCount) touchCount[idx]++;

				// Factor of the TIME it took to touch: [1, 2], only INCREASES (never < 1).
				float timeFrac = RS_CLAMP(timeSinceTouch[idx] / maxGap, 0.0f, 1.0f);
				float scale = 1.0f + timeFrac;

				float reward = (float)touchCount[idx] * scale; // count × time factor

				// Resets the timer until the next touch.
				timeSinceTouch[idx] = 0.f;
				return reward;
			}

			return 0.0f;
		}
	};

	// Penalizes the ball HITTING THE OPPONENT'S WALL (target's goal line) BELOW the
	// CROSSBAR but OUTSIDE the goal — the attacker reached the goal line and missed to the side.
	//   - Assigned ONLY to the LAST one to touch the ball (the attacker who shot).
	//   - Does NOT count if inside the goal bounds (would be a goal).
	//   - Does NOT count ABOVE the crossbar (only below).
	//   - minHeight: minimum tolerance height — very low hits don't count.
	//   - Returns 1.0 at the moment of the hit -> give it a NEGATIVE WEIGHT in main.
	// Uses PreStep to know who was the last to touch (per-arena state).
	class WallMissPenalty : public Reward {
	public:
		float minHeight; // minimum ball height (uu) for the hit to count (tolerance)

		WallMissPenalty(float minHeight = 100.f) : minHeight(minHeight) {}

		int  lastToucherId = -1;            // carId of the last to touch the ball (per arena)
		Team lastToucherTeam = Team::BLUE;

		virtual void Reset(const GameState& initialState) override {
			lastToucherId = -1;
		}

		// Updates who was the last to touch (1x per step, before the rewards).
		virtual void PreStep(const GameState& state) override {
			for (const auto& p : state.players)
				if (p.ballTouchedStep) { lastToucherId = (int)p.carId; lastToucherTeam = p.team; }
		}

		virtual float GetReward(const Player& player, const GameState& state, bool isFinal) override {
			if (state.goalScored) return 0.0f;                 // was a goal -> no penalty
			if (!state.prev) return 0.0f;
			if (lastToucherId < 0) return 0.0f;
			// Only the LAST to touch (the attacker who shot) receives the penalty.
			if ((int)player.carId != lastToucherId) return 0.0f;

			// OPPONENT's wall = the goal that the last touch ATTACKS (BLUE -> +Y, ORANGE -> -Y).
			bool targetOrange = lastToucherTeam == Team::BLUE;
			float wallY = targetOrange ? CommonValues::BACK_WALL_Y : -CommonValues::BACK_WALL_Y;
			float yWall = wallY - (targetOrange ? CommonValues::BALL_RADIUS : -CommonValues::BALL_RADIUS);

			// The ball touches the target's back wall ON THIS step (crosses yWall while approaching).
			float prevY = state.prev->ball.pos.y;
			float curY  = state.ball.pos.y;
			bool reachedWall = targetOrange ? (prevY < yWall && curY >= yWall)
			                                : (prevY > yWall && curY <= yWall);
			if (!reachedWall) return 0.0f;

			float bz = state.ball.pos.z;
			float bx = fabsf(state.ball.pos.x);

			bool belowBar    = bz <= CommonValues::GOAL_HEIGHT;             // below the crossbar
			bool aboveMin    = bz >= minHeight;                            // height tolerance
			bool outsideGoal = bx > CommonValues::GOAL_WIDTH_FROM_CENTER;  // OUTSIDE the goal (not a goal)

			return (belowBar && aboveMin && outsideGoal) ? 1.0f : 0.0f;
		}
	};

}
