#pragma once
#include "SchedulerConfig.h"
#include <vector>
#include <cstdint>

/*
	Lógica PURA do scheduler — sem dependências de torch, Learner ou estado.
	Vive separada para poder ser testada isoladamente (ver tests/scheduler_test.cpp),
	garantindo que o teste exercita exatamente o mesmo código que corre em produção.
*/

// Valores PPO concretos resolvidos (forward-fill dos optionals) por fase.
struct ResolvedPPO {
	float policyLR, criticLR, entropyScale, clipRange, policyTemperature;
	int   epochs;
	float gaeGamma, gaeLambda, rewardClipRange;
};

// Índice da fase ativa para `ts`. Assume fases ordenadas por startTimestep crescente.
inline int SchedulerPhaseFor(const SchedulerConfig& cfg, uint64_t ts) {
	int idx = 0;
	for (int i = 0; i < (int)cfg.phases.size(); i++) {
		if (cfg.phases[i].startTimestep <= ts)
			idx = i;
		else
			break;
	}
	return idx;
}

// Resolve os optionals de cada fase em valores concretos, fazendo forward-fill:
// um campo vazio herda o valor da fase anterior; a fase 0 herda de `baseline`.
inline std::vector<ResolvedPPO> SchedulerResolvePhases(const SchedulerConfig& cfg, const ResolvedPPO& baseline) {
	std::vector<ResolvedPPO> resolved(cfg.phases.size());
	ResolvedPPO prev = baseline;
	for (size_t i = 0; i < cfg.phases.size(); i++) {
		const TrainingPhase& p = cfg.phases[i];
		ResolvedPPO r = prev;
		if (p.policyLR)          r.policyLR          = *p.policyLR;
		if (p.criticLR)          r.criticLR          = *p.criticLR;
		if (p.entropyScale)      r.entropyScale      = *p.entropyScale;
		if (p.clipRange)         r.clipRange         = *p.clipRange;
		if (p.policyTemperature) r.policyTemperature = *p.policyTemperature;
		if (p.epochs)            r.epochs            = *p.epochs;
		if (p.gaeGamma)          r.gaeGamma          = *p.gaeGamma;
		if (p.gaeLambda)         r.gaeLambda         = *p.gaeLambda;
		if (p.rewardClipRange)   r.rewardClipRange   = *p.rewardClipRange;
		resolved[i] = r;
		prev = r;
	}
	return resolved;
}

// Valores PPO efetivos em `ts`, dentro da fase `phaseIdx`.
// Se cfg.interpolatePPOParams, interpola linearmente os floats até à fase seguinte.
// `epochs` é sempre em degrau (inteiro, fica o da fase atual).
inline ResolvedPPO SchedulerInterpolate(const SchedulerConfig& cfg, const std::vector<ResolvedPPO>& resolved,
                                        uint64_t ts, int phaseIdx) {
	ResolvedPPO out = resolved[phaseIdx];

	if (cfg.interpolatePPOParams && phaseIdx + 1 < (int)cfg.phases.size()) {
		const ResolvedPPO& cur = resolved[phaseIdx];
		const ResolvedPPO& nxt = resolved[phaseIdx + 1];
		uint64_t a = cfg.phases[phaseIdx].startTimestep;
		uint64_t b = cfg.phases[phaseIdx + 1].startTimestep;
		if (b > a) {
			double t = (double)(ts - a) / (double)(b - a);
			t = t < 0 ? 0 : (t > 1 ? 1 : t);
			auto L = [&](float x, float y) { return (float)(x + (y - x) * t); };
			out.policyLR          = L(cur.policyLR, nxt.policyLR);
			out.criticLR          = L(cur.criticLR, nxt.criticLR);
			out.entropyScale      = L(cur.entropyScale, nxt.entropyScale);
			out.clipRange         = L(cur.clipRange, nxt.clipRange);
			out.policyTemperature = L(cur.policyTemperature, nxt.policyTemperature);
			out.gaeGamma          = L(cur.gaeGamma, nxt.gaeGamma);
			out.gaeLambda         = L(cur.gaeLambda, nxt.gaeLambda);
			out.rewardClipRange   = L(cur.rewardClipRange, nxt.rewardClipRange);
			// out.epochs fica em degrau (cur.epochs)
		}
	}

	return out;
}
