#pragma once
#include "SchedulerTypes.h"   // só precisa dos tipos (TrainingPhase, SchedulerConfig), não da data
#include <vector>
#include <cstdint>

/*
	Lógica PURA do scheduler — sem dependências de torch, Learner ou estado.
	Vive separada para poder ser testada isoladamente.
*/

// Valores PPO concretos resolvidos (forward-fill dos optionals) por fase.
struct ResolvedPPO {
	float policyLR, criticLR, entropyScale, clipRange, policyTemperature;
	int   epochs;
	float gaeGamma, gaeLambda, rewardClipRange;
};

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

// Valores PPO efetivos para `phaseIdx`.
// Se cfg.interpolatePPOParams == true E advanceMaxTimesteps estiver definido nas duas fases,
// interpola linearmente entre o valor atual e o da fase seguinte.
// epochs muda sempre em degrau (é inteiro).
inline ResolvedPPO SchedulerEffective(const SchedulerConfig& cfg, const std::vector<ResolvedPPO>& resolved,
                                      uint64_t ts, int phaseIdx) {
	ResolvedPPO out = resolved[phaseIdx];

	if (cfg.interpolatePPOParams && phaseIdx + 1 < (int)cfg.phases.size()) {
		const TrainingPhase& cur = cfg.phases[phaseIdx];
		const TrainingPhase& nxt = cfg.phases[phaseIdx + 1];

		// Interpolação só faz sentido se ambas as fases têm um ponto final definido
		if (cur.advanceMaxTimesteps && nxt.advanceMaxTimesteps) {
			uint64_t a = *cur.advanceMaxTimesteps;
			uint64_t b = *nxt.advanceMaxTimesteps;
			if (b > a) {
				double t = (double)(ts - a) / (double)(b - a);
				t = t < 0 ? 0 : (t > 1 ? 1 : t);
				auto L = [&](float x, float y) { return (float)(x + (y - x) * t); };
				const ResolvedPPO& r_cur = resolved[phaseIdx];
				const ResolvedPPO& r_nxt = resolved[phaseIdx + 1];
				out.policyLR          = L(r_cur.policyLR,          r_nxt.policyLR);
				out.criticLR          = L(r_cur.criticLR,          r_nxt.criticLR);
				out.entropyScale      = L(r_cur.entropyScale,      r_nxt.entropyScale);
				out.clipRange         = L(r_cur.clipRange,         r_nxt.clipRange);
				out.policyTemperature = L(r_cur.policyTemperature, r_nxt.policyTemperature);
				out.gaeGamma          = L(r_cur.gaeGamma,          r_nxt.gaeGamma);
				out.gaeLambda         = L(r_cur.gaeLambda,         r_nxt.gaeLambda);
				out.rewardClipRange   = L(r_cur.rewardClipRange,   r_nxt.rewardClipRange);
				// out.epochs fica em degrau
			}
		}
	}

	return out;
}
