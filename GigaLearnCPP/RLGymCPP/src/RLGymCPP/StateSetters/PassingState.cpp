#include "PassingState.h"
#include "../CommonValues.h" 
#include <algorithm>
#include <cmath>

using RocketSim::Math::RandFloat;

namespace RLGC {

    namespace {
        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kDegToRad = kPi / 180.f;

        Vec Rotate2D(const Vec& vec, float angle) {
            float c = cosf(angle);
            float s = sinf(angle);
            return Vec(
                vec.x * c - vec.y * s,
                vec.x * s + vec.y * c,
                0.f
            );
        }

        Vec FlatNormalized(Vec vec, const Vec& fallback) {
            vec.z = 0.f;
            float len = vec.Length();
            if (len < 1e-6f) return fallback;
            return vec / len;
        }

        Vec ClampToField(Vec pos, float marginX, float marginY) {
            pos.x = std::clamp(pos.x, -CommonValues::SIDE_WALL_X + marginX, CommonValues::SIDE_WALL_X - marginX);
            pos.y = std::clamp(pos.y, -CommonValues::BACK_WALL_Y + marginY, CommonValues::BACK_WALL_Y - marginY);
            return pos;
        }

        Vec GoalDirection(bool attackBlue) {
            return attackBlue ? Vec(0.f, -1.f, 0.f) : Vec(0.f, 1.f, 0.f);
        }
    }

    void PassingState::ResetArena(Arena* arena) {
        arena->ResetToRandomKickoff();

        const bool attackBlue = RandFloat(0.f, 1.f) > 0.5f;
        const Team attackerTeam = attackBlue ? Team::BLUE : Team::ORANGE;
        const Vec goalPos = attackBlue ? CommonValues::ORANGE_GOAL_BACK : CommonValues::BLUE_GOAL_BACK;
        const Vec towardField = GoalDirection(attackBlue);

        // --- 1. BALL SPAWN ---
        const float ballRadius = RandFloat(2500.f, 4500.f);
        const float ballAngle = RandFloat(-60.f * kDegToRad, 60.f * kDegToRad);
        Vec ballPos = goalPos + Rotate2D(towardField, ballAngle) * ballRadius;
        ballPos = ClampToField(ballPos, CommonValues::BALL_RADIUS, CommonValues::BALL_RADIUS);

        // --- 2. ATTACKER SPAWN ---
        Vec attackerPos;
        bool validSpawn = false;
        
        for (int i = 0; i < 50; ++i) {
            float ax = RandFloat(-CommonValues::SIDE_WALL_X + 180.f, CommonValues::SIDE_WALL_X - 180.f);
            float ay = attackBlue 
                ? RandFloat(0.f, std::max(0.f, ballPos.y)) 
                : RandFloat(std::min(0.f, ballPos.y), 0.f);

            attackerPos = Vec(ax, ay, 17.f);
            
            if ((attackerPos - ballPos).Length() >= 1500.f) {
                validSpawn = true;
                break;
            }
        }

        if (!validSpawn) {
            attackerPos = ballPos + towardField * 1500.f;
            attackerPos.z = 17.f;
            attackerPos = ClampToField(attackerPos, 180.f, 180.f);
        }

        // --- 3. PASS (BALL SPEED AND HEIGHT) ---
        Vec ballToAttacker = FlatNormalized(attackerPos - ballPos, towardField);
        float errorAngle = RandFloat(-5.f * kDegToRad, 5.f * kDegToRad);
        Vec passDir = Rotate2D(ballToAttacker, errorAngle);

        BallState bs = {};

        // NEW: Sets the ball's initial height (on the ground or slightly in the air up to 200)
        float spawnHeight = RandFloat(CommonValues::BALL_RADIUS, 200.f);
        bs.pos = Vec(ballPos.x, ballPos.y, spawnHeight);

        // NEW: Keeps the horizontal speed, but adds a "pop" on the Z axis so the ball bounces or floats a bit
        bs.vel = passDir * RandFloat(1000.f, 2500.f);
        bs.vel.z = RandFloat(0.f, 350.f); // Slight chip pass / bounce

        bs.angVel = Vec(0.f, 0.f, 0.f);
        arena->ball->SetState(bs);

        // --- 4. CARS SPAWN ---
        for (Car* car : arena->_cars) {
            CarState cs = {};
            Vec carPos;

            if (car->team == attackerTeam) {
                carPos = attackerPos;
            } else {
                const float goalHalfWidth = CommonValues::GOAL_WIDTH * 0.5f - 120.f;
                const float goalFront = CommonValues::BACK_WALL_Y - 520.f;
                const float goalBack = CommonValues::BACK_WALL_Y - 40.f;

                carPos.x = RandFloat(-goalHalfWidth, goalHalfWidth);
                carPos.y = attackBlue
                    ? RandFloat(goalFront, goalBack)
                    : RandFloat(-goalBack, -goalFront);
                carPos = ClampToField(carPos, 120.f, 120.f);
            }

            carPos.z = 17.f;
            cs.pos = carPos;
            cs.vel = Vec(0.f, 0.f, 0.f);
            cs.angVel = Vec(0.f, 0.f, 0.f);
            cs.boost = 100.f;
            cs.rotMat = Angle(atan2f(ballPos.y - carPos.y, ballPos.x - carPos.x), 0.f, 0.f).ToRotMat();

            car->SetState(cs);
        }
    }

}