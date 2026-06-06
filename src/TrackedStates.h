#pragma once
#include <RLGymCPP/StateSetters/GoalShotState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/StateSetters/StateSetter.h>
#include <RLGymCPP/StateSetters/FallingBallApproachState.h>
#include <mutex>
#include <unordered_set>
#include <atomic>
#include <cstdio>
#include <cmath>

// Usamos "inline" (C++17) para podermos definir e instanciar as globais no header
inline std::mutex g_arenaMutex;
inline std::unordered_set<RocketSim::Arena*> g_goalShotArenas;

inline bool IsArenaGoalShot(RocketSim::Arena* arena) {
    if (!arena) return false;
    std::lock_guard<std::mutex> lock(g_arenaMutex);
    return g_goalShotArenas.count(arena) > 0;
}

class TrackedGoalShotState : public RLGC::GoalShotState {
public:
    void ResetArena(RocketSim::Arena* arena) override {
        {
            std::lock_guard<std::mutex> lock(g_arenaMutex);
            g_goalShotArenas.insert(arena);
        }
        RLGC::GoalShotState::ResetArena(arena);
    }
};

class TrackedRandomState : public RLGC::RandomState {
public:
    TrackedRandomState(bool a, bool b, bool c) : RLGC::RandomState(a, b, c) {}

    void ResetArena(RocketSim::Arena* arena) override {
        {
            std::lock_guard<std::mutex> lock(g_arenaMutex);
            g_goalShotArenas.erase(arena);
        }
        RLGC::RandomState::ResetArena(arena);
    }
};

// --- FallingBallApproachState tracking ---
inline std::unordered_set<RocketSim::Arena*> g_fallingBallArenas;
inline std::atomic<int> g_fallingBallGoals{0};
inline std::atomic<int> g_fallingBallSteps{0};

inline bool IsArenaFallingBall(RocketSim::Arena* arena) {
    if (!arena) return false;
    std::lock_guard<std::mutex> lock(g_arenaMutex);
    return g_fallingBallArenas.count(arena) > 0;
}

class TrackedFallingBallApproachState : public RLGC::FallingBallApproachState {
public:
    void ResetArena(RocketSim::Arena* arena) override {
        {
            std::lock_guard<std::mutex> lock(g_arenaMutex);
            g_fallingBallArenas.insert(arena);
        }

        RLGC::FallingBallApproachState::ResetArena(arena);
    }
};