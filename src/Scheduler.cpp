#include "Scheduler.h"
#include "SchedulableState.h"

#include <cstdio>
#include <cmath>
#include <mutex>

using namespace GGL;

std::vector<std::string> g_rewardNames;
static std::once_flag g_rewardNamesFlag;

void RegisterRewardNames(const std::vector<std::string>& names) {
	std::call_once(g_rewardNamesFlag, [&] { g_rewardNames = names; });
}

int Scheduler::PhaseFor(uint64_t ts) const {
	return SchedulerPhaseFor(cfg, ts);
}

// Lê a baseline (config atual do learner) e resolve as fases via lógica pura partilhada.
void Scheduler::ResolveBaseline(Learner* learner) {
	const PPOLearnerConfig& live = learner->GetLivePPOConfig(); // o que o Learn() usa
	ResolvedPPO baseline;
	baseline.policyLR          = live.policyLR;
	baseline.criticLR          = live.criticLR;
	baseline.entropyScale      = live.entropyScale;
	baseline.clipRange         = live.clipRange;
	baseline.policyTemperature = live.policyTemperature;
	baseline.epochs            = live.epochs;
	// GAE: o loop lê de learner->config.ppo, não do config do PPOLearner.
	baseline.gaeGamma          = learner->config.ppo.gaeGamma;
	baseline.gaeLambda         = learner->config.ppo.gaeLambda;
	baseline.rewardClipRange   = learner->config.ppo.rewardClipRange;

	resolved = SchedulerResolvePhases(cfg, baseline);
}

// Pesos de reward + state setters — mudam sempre em DEGRAU, na fronteira da fase.
void Scheduler::ApplyPhaseWeights(Learner* learner, const TrainingPhase& p) {
	// Rewards: casa nome -> índice via g_rewardNames (ordem fixa, igual em todas as arenas).
	for (auto& arenaRewards : learner->envSet->rewards) {
		for (size_t i = 0; i < arenaRewards.size() && i < g_rewardNames.size(); i++) {
			auto it = p.rewardWeights.find(g_rewardNames[i]);
			if (it != p.rewardWeights.end())
				arenaRewards[i].weight = it->second;
		}
	}

	// State setters: só os que são SchedulableState reagem.
	for (auto* s : learner->envSet->stateSetters) {
		auto* ss = dynamic_cast<SchedulableState*>(s);
		if (!ss) continue;
		if (p.stochastic.has_value())
			ss->SetStochastic(*p.stochastic);
		if (!p.stateWeights.empty())
			ss->SetWeights(p.stateWeights);
	}
}

// Params numéricos do PPO. Interpola entre a fase atual e a próxima se interpolatePPOParams.
// Nunca recria modelos/optimizer — só ajusta floats e o LR in-place.
void Scheduler::ApplyPPO(Learner* learner, uint64_t ts, int phaseIdx) {
	// Valores efetivos (degrau ou interpolados) via lógica pura partilhada.
	ResolvedPPO eff = SchedulerInterpolate(cfg, resolved, ts, phaseIdx);

	// LR via ponte exportada: ajusta param_groups in-place (preserva pesos e estado Adam).
	// Só chamamos se mudou de forma percetível, para não inundar o log.
	if (std::fabs(eff.policyLR - lastPolicyLR) > 1e-9f || std::fabs(eff.criticLR - lastCriticLR) > 1e-9f) {
		learner->SetLearningRates(eff.policyLR, eff.criticLR);
		lastPolicyLR = eff.policyLR;
		lastCriticLR = eff.criticLR;
	}

	// Loss params: escritos no config vivo lido por Learn() a cada iteração.
	PPOLearnerConfig& live = learner->GetLivePPOConfig();
	live.entropyScale      = eff.entropyScale;
	live.clipRange         = eff.clipRange;
	live.policyTemperature = eff.policyTemperature;
	live.epochs            = eff.epochs;

	// GAE params: lidos frescos de learner->config.ppo no passo GAE.
	learner->config.ppo.gaeGamma        = eff.gaeGamma;
	learner->config.ppo.gaeLambda       = eff.gaeLambda;
	learner->config.ppo.rewardClipRange = eff.rewardClipRange;
}

void Scheduler::Update(Learner* learner) {
	if (cfg.phases.empty() || learner->config.renderMode)
		return;

	if (!initialized) {
		ResolveBaseline(learner);
		initialized = true;
	}

	// Trabalha no máximo uma vez por iteração PPO (totalIterations só muda entre iterações).
	int64_t it = (int64_t)learner->totalIterations;
	if (it == lastIteration)
		return;
	lastIteration = it;

	uint64_t ts = learner->totalTimesteps;
	int phase = PhaseFor(ts);
	bool phaseChanged = (phase != lastPhase);

	if (phaseChanged) {
		const TrainingPhase& p = cfg.phases[phase];
		printf("[Scheduler] -> fase \"%s\" @ %llu timesteps\n",
			p.name.c_str(), (unsigned long long)ts);
		ApplyPhaseWeights(learner, p);
		lastPhase = phase;
	}

	// PPO: interpola todas as iterações; em modo degrau só na transição.
	if (cfg.interpolatePPOParams || phaseChanged)
		ApplyPPO(learner, ts, phase);
}
