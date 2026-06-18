#include "FallingBallState.h"
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
    }

    void FallingBallState::ResetArena(Arena* arena) {
        arena->ResetToRandomKickoff();

        const bool attackBlue = RandFloat(0.f, 1.f) > 0.5f;
        const Team attackerTeam = attackBlue ? Team::BLUE : Team::ORANGE;
        
        const float targetY = attackBlue ? CommonValues::BACK_WALL_Y : -CommonValues::BACK_WALL_Y;
        const Vec targetGoal = Vec(0.f, targetY, 0.f);

        // --- 1. SPAWN DA BOLA (CORREDOR CENTRAL - MARGEM DE 1500) ---
        // Alterado para 1500.f de cada lado para prender a bola no centro do campo
        float ballX = RandFloat(-CommonValues::SIDE_WALL_X + 1500.f, CommonValues::SIDE_WALL_X - 1500.f);
        float ballY = 0.f;

        if (attackBlue) {
            ballY = RandFloat(200.f, 3000.f); 
        } else {
            ballY = RandFloat(-3000.f, -200.f);
        }
        
        Vec ballPos(ballX, ballY, RandFloat(1200.f, 1800.f));
        ballPos = ClampToField(ballPos, CommonValues::BALL_RADIUS, CommonValues::BALL_RADIUS);
        
        BallState bs = {};
        bs.pos = ballPos;

        // --- PERTURBAÇÃO CIRÚRGICA NA VELOCIDADE DA BOLA ---
        float fallSpeedZ = RandFloat(-2000.f, -1000.f);
        float perturbAngle = RandFloat(0.f, 3.f) * kDegToRad; 
        float perturbTheta = RandFloat(-kPi, kPi); 
        
        float horizontalMag = std::abs(fallSpeedZ) * std::tan(perturbAngle);
        float velX = horizontalMag * cosf(perturbTheta);
        float velY = horizontalMag * sinf(perturbTheta);

        bs.vel = Vec(velX, velY, fallSpeedZ); 
        bs.angVel = Vec(0.f, 0.f, 0.f);
        arena->ball->SetState(bs);

        // --- 2. POSIÇÃO DO ATACANTE ---
        Vec ballToGoal = FlatNormalized(targetGoal - ballPos, Vec(0.f, attackBlue ? 1.f : -1.f, 0.f));
        Vec behindBall = ballToGoal * -1.f;

        float angleOffset = RandFloat(-15.f * kDegToRad, 15.f * kDegToRad);
        Vec spawnDir = Rotate2D(behindBall, angleOffset);

        float dist = RandFloat(1500.f, 2000.f);
        Vec attackerPos = ballPos + spawnDir * dist;
        attackerPos.z = 17.f;

        Vec attackerVel = FlatNormalized(ballPos - attackerPos, ballToGoal) * RandFloat(800.f, 1000.f);

        float attBaseYaw = atan2f(ballPos.y - attackerPos.y, ballPos.x - attackerPos.x);
        float attVisualError = RandFloat(-15.f * kDegToRad, 15.f * kDegToRad);
        float attackerYaw = attBaseYaw + attVisualError;

        // --- 3. APLICAR AOS CARROS ---
        for (Car* car : arena->_cars) {
            CarState cs = {};
            
            if (car->team == attackerTeam) {
                cs.pos = ClampToField(attackerPos, 180.f, 180.f);
                cs.vel = attackerVel;
                cs.angVel = Vec(0.f, 0.f, 0.f);
                cs.boost = 100.f;
                cs.rotMat = Angle(attackerYaw, 0.f, 0.f).ToRotMat();
            } else {
                const float goalHalfWidth = (CommonValues::GOAL_WIDTH * 0.5f) - 160.f;
                float defX = RandFloat(-goalHalfWidth, goalHalfWidth);
                
                float depthOffset = RandFloat(0.f, 120.f);
                float defY = attackBlue ? (targetY - depthOffset) : (targetY + depthOffset);
                
                Vec defenderPos = Vec(defX, defY, 17.f);

                cs.pos = ClampToField(defenderPos, 120.f, 120.f);
                cs.vel = Vec(0.f, 0.f, 0.f);
                cs.angVel = Vec(0.f, 0.f, 0.f);
                cs.boost = 100.f;
                
                float defBaseYaw = atan2f(ballPos.y - cs.pos.y, ballPos.x - cs.pos.x);
                float defVisualError = RandFloat(-15.f * kDegToRad, 15.f * kDegToRad);
                float defenderYaw = defBaseYaw + defVisualError;
                
                cs.rotMat = Angle(-defenderYaw, 0.f, 0.f).ToRotMat();
            }

            car->SetState(cs);
        }
    }

} // namespace RLGC