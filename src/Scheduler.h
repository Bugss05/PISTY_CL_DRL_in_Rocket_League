#pragma once
#include <GigaLearnCPP/Learner.h>
#include "SchedulerConfig.h"
#include "SchedulerCore.h"
#include <string>
#include <vector>

// Nomes das rewards, na MESMA ordem do vetor de rewards criado no EnvCreateFunc.
// O Scheduler casa os nomes do SchedulerConfig -> índice através desta lista, em vez de
// usar Reward::GetName() (que no GCC devolve nomes "mangled" e quebraria o matching).
// Chama RegisterRewardNames() uma vez, dentro do EnvCreateFunc.
extern std::vector<std::string> g_rewardNames;
void RegisterRewardNames(const std::vector<std::string>& names);

/*
	Scheduler do currículo de treino.

	Uso na main:
	    Scheduler g_scheduler;                  // global
	    void StepCallback(Learner* learner, ...) {
	        g_scheduler.Update(learner);        // 1ª linha
	        ...
	    }

	O Update() é barato e idempotente: corre o seu trabalho no máximo uma vez por iteração
	PPO (deteta a mudança via learner->totalIterations), por isso podes chamá-lo a cada step.

	GARANTIA: alterar parâmetros do PPO NÃO recria modelos nem optimizer. As redes vivem em
	ppo->models (criadas uma só vez no construtor) e SetLearningRates() apenas ajusta o LR dos
	param_groups in-place — o estado do Adam e os pesos aprendidos são preservados entre fases.
*/
class Scheduler {
public:
	Scheduler() : cfg(GetSchedulerConfig()) {}

	void Update(GGL::Learner* learner);

private:
	// ResolvedPPO e a lógica pura (forward-fill, interpolação) vivem em SchedulerCore.h.
	SchedulerConfig cfg;
	std::vector<ResolvedPPO> resolved; // 1:1 com cfg.phases, construído na 1ª Update()

	bool   initialized = false;
	int    lastPhase = -1;
	int64_t lastIteration = -1;
	float  lastPolicyLR = -1.0f, lastCriticLR = -1.0f; // evita log spam do SetLearningRates

	int  PhaseFor(uint64_t timesteps) const;
	void ResolveBaseline(GGL::Learner* learner);
	void ApplyPhaseWeights(GGL::Learner* learner, const TrainingPhase& p); // rewards + setters (degrau)
	void ApplyPPO(GGL::Learner* learner, uint64_t timesteps, int phaseIdx); // numéricos (degrau ou interp.)
};
