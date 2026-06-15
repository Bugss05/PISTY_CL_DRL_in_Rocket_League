#pragma once
#include <GigaLearnCPP/Learner.h>
#include <GigaLearnCPP/Util/Report.h>
#include "SchedulableState.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdio>

// ---------------------------------------------------------------------------
// Registo de nomes das rewards.
// Chama RegisterRewardNames() UMA VEZ no EnvCreateFunc, com os nomes na
// mesma ordem do vetor de rewards. O scheduler usa-os para mapear nome->índice.
// ---------------------------------------------------------------------------
inline std::vector<std::string> g_rewardNames;

inline void RegisterRewardNames(const std::vector<std::string>& names) {
    static bool done = false;
    if (!done) { g_rewardNames = names; done = true; }
}

// ---------------------------------------------------------------------------
// Configuração de uma fase — define em PhaseConfig.h
// ---------------------------------------------------------------------------
struct Phase {
    std::string name;
    uint64_t    startTimesteps;                              // ativa quando totalTimesteps >= este valor

    std::unordered_map<std::string, float> rewardWeights;   // nome -> peso da reward
    std::unordered_map<std::string, float> stateWeights;    // nome -> peso do state setter (vazio = sem mudança)

    // Parâmetros PPO opcionais — omitir = mantém o valor atual
    std::optional<float> policyLR;
    std::optional<float> criticLR;
    std::optional<float> entropyScale;
    std::optional<float> gaeGamma;
};

// Implementada em PhaseConfig.h — só editas esse ficheiro
std::vector<Phase> GetPhases();

// ---------------------------------------------------------------------------
// Scheduler simples por timesteps
//
// Uso em StepCallback (primeira linha):
//     g_scheduler.Update(learner, report);
//
// Quando totalTimesteps ultrapassa o startTimesteps de uma fase, aplica:
//   - Pesos das rewards em todas as arenas (via g_rewardNames)
//   - Pesos dos state setters em todas as arenas (via SchedulableState::SetWeights)
//   - Parâmetros PPO definidos na fase
// ---------------------------------------------------------------------------
class SimpleScheduler {
public:
    void Update(GGL::Learner* learner, GGL::Report& report) {
        int64_t curIter = (int64_t)learner->totalIterations;
        if (curIter == lastIteration) return;
        lastIteration = curIter;

        if (!initialized) {
            phases      = GetPhases();
            initialized = true;
        }

        TryAdvance(learner, report);
    }

private:
    std::vector<Phase> phases;
    int     currentPhaseIdx = -1;
    int64_t lastIteration   = -1;
    bool    initialized     = false;

    void TryAdvance(GGL::Learner* learner, GGL::Report& report) {
        uint64_t ts = learner->totalTimesteps;

        int target = -1;
        for (int i = (int)phases.size() - 1; i >= 0; --i) {
            if (ts >= phases[i].startTimesteps) { target = i; break; }
        }
        if (target < 0 || target == currentPhaseIdx) return;

        currentPhaseIdx = target;
        const Phase& p  = phases[target];

        // --- Rewards ---
        for (auto& arenaRewards : learner->envSet->rewards) {
            for (int i = 0; i < (int)arenaRewards.size(); ++i) {
                if (i >= (int)g_rewardNames.size()) break;
                auto it = p.rewardWeights.find(g_rewardNames[i]);
                if (it != p.rewardWeights.end())
                    arenaRewards[i].weight = it->second;
            }
        }

        // --- State setters ---
        if (!p.stateWeights.empty()) {
            for (auto* setter : learner->envSet->stateSetters) {
                auto* ss = dynamic_cast<SchedulableState*>(setter);
                if (ss) ss->SetWeights(p.stateWeights);
            }
        }

        // --- PPO ---
        auto& ppo = learner->config.ppo;
        if (p.policyLR)     ppo.policyLR     = *p.policyLR;
        if (p.criticLR)     ppo.criticLR     = *p.criticLR;
        if (p.entropyScale) ppo.entropyScale  = *p.entropyScale;
        if (p.gaeGamma)     ppo.gaeGamma      = *p.gaeGamma;

        printf("[Scheduler] Fase %d: \"%s\" @ %llu steps\n",
            target, p.name.c_str(), (unsigned long long)ts);

        report.Add("Scheduler/Phase", (float)target);
    }
};
