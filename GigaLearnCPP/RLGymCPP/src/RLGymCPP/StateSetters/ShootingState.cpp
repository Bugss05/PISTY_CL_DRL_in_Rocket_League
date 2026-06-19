#include "ShootingState.h"
#include "../Math.h"
#include <cmath>

using RocketSim::Math::RandFloat;

namespace RLGC {

void ShootingState::ResetArena(Arena* arena) {
    arena->ResetToRandomKickoff();

    // ---------------------------------------------------------
    // 0. MIRRORING (Choose which team attacks)
    // ---------------------------------------------------------
    float goalY = RandFloat(0.f, 1.f) > 0.5f ? 5120.f : -5120.f;

    // Direction pointing to the center of the field from the target goal
    float dirMid = goalY > 0.f ? -1.f : 1.f;

    // ---------------------------------------------------------
    // 1. BALL SPAWN AREA
    // ---------------------------------------------------------
    float r_ball = RandFloat(2000.f, 4000.f);
    float angle_ball_deg = RandFloat(-70.f, 70.f);
    float angle_ball_rad = angle_ball_deg * (M_PI / 180.f); // Convert to radians

    float ballX = r_ball * sinf(angle_ball_rad);
    float ballY = goalY + dirMid * r_ball * cosf(angle_ball_rad);
    float ballZ = CommonValues::BALL_RADIUS; // Keep the ball on the ground

    {
        BallState bs = {};
        bs.pos = Vec(ballX, ballY, ballZ);
        bs.vel = Vec(0, 0, 0);
        bs.angVel = Vec(0, 0, 0);
        arena->ball->SetState(bs);
    }

    // The attacking team is the one attacking the goal closest to the ball.
    float distToBlueGoal = fabsf(ballY + 5120.f);
    float distToOrangeGoal = fabsf(ballY - 5120.f);
    bool blueAttacks = distToOrangeGoal <= distToBlueGoal;

    // ---------------------------------------------------------
    // 2. ATTACKER SPAWN AREA
    // ---------------------------------------------------------
    float d_att = RandFloat(500.f, 1000.f);
    float angle_att_deg = RandFloat(-30.f, 30.f);
    float angle_att_rad = angle_att_deg * (M_PI / 180.f);

    // Direction vector [Goal -> Ball]
    float gbX = ballX - 0.f;
    float gbY = ballY - goalY;
    float magGB = sqrtf(gbX*gbX + gbY*gbY);
    float normGBx = gbX / magGB;
    float normGBy = gbY / magGB;

    float spawnDirX = normGBx * cosf(angle_att_rad) - normGBy * sinf(angle_att_rad);
    float spawnDirY = normGBx * sinf(angle_att_rad) + normGBy * cosf(angle_att_rad);

    float attX = ballX + spawnDirX * d_att;
    float attY = ballY + spawnDirY * d_att;
    float attZ = 17.f; 

    float attYaw = atan2f(ballY - attY, ballX - attX);

    // ---------------------------------------------------------
    // 3. DEFENDER SPAWN AREA
    // ---------------------------------------------------------
    float lateralDist = RandFloat(500.f, 700.f);
    float toBallX = ballX - attX;
    float toBallY = ballY - attY;
    float toBallMag = sqrtf(toBallX * toBallX + toBallY * toBallY);
    float lateralX = -toBallY / toBallMag;
    float lateralY = toBallX / toBallMag;

    if (RandFloat(0.f, 1.f) > 0.5f) {
        lateralX = -lateralX;
        lateralY = -lateralY;
    }

    float defX = attX + lateralX * lateralDist;
    float defY = attY + lateralY * lateralDist;
    float defZ = 17.f;

    // Map boundaries (keep consistent with GoalShotState)
    float MAX_X = 4000.f;
    float MAX_Y = 5000.f;

    // Clamp attacker
    if (attX > MAX_X) attX = MAX_X;
    if (attX < -MAX_X) attX = -MAX_X;
    if (attY > MAX_Y) attY = MAX_Y;
    if (attY < -MAX_Y) attY = -MAX_Y;

    // Clamp defender
    if (defX > MAX_X) defX = MAX_X;
    if (defX < -MAX_X) defX = -MAX_X;
    if (defY > MAX_Y) defY = MAX_Y;
    if (defY < -MAX_Y) defY = -MAX_Y;

    float defYaw = atan2f(ballY - defY, ballX - defX);

    // ---------------------------------------------------------
    // 4. APPLY TO THE CARS (Separate the teams)
    // ---------------------------------------------------------
    for (Car* car : arena->_cars) {
        CarState cs = {};
        cs.vel = Vec(0, 0, 0);
        cs.angVel = Vec(0, 0, 0);
        cs.boost = 100.f;

        bool isAttacker = (car->team == Team::BLUE && blueAttacks) || 
                          (car->team == Team::ORANGE && !blueAttacks);

        if (isAttacker) {
            cs.pos = Vec(attX, attY, attZ);
            float zDeg = RandFloat(-30.f, 30.f);
            float zRad = zDeg * (M_PI / 180.f);
            cs.rotMat = Angle(attYaw + zRad, 0.f, 0.f).ToRotMat();
        } else {
            cs.pos = Vec(defX, defY, defZ);
            float zDeg = RandFloat(-30.f, 30.f);
            float zRad = zDeg * (M_PI / 180.f);
            cs.rotMat = Angle(defYaw + zRad, 0.f, 0.f).ToRotMat();
        }

        car->SetState(cs);
    }
}

} // namespace RLGC