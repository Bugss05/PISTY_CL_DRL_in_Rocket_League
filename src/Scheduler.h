#pragma once
#include <GigaLearnCPP/Learner.h>
#include <GigaLearnCPP/Util/Report.h>
#include "SchedulerConfig.h"
#include "SchedulerCore.h"
#include "SchedulableState.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <chrono>

// Nomes das rewards, na MESMA ordem do vetor de rewards criado no EnvCreateFunc.
// O Scheduler casa os nomes do SchedulerConfig -> índice através desta lista.
// Chama RegisterRewardNames() uma vez, dentro do EnvCreateFunc.
extern std::vector<std::string> g_rewardNames;
void RegisterRewardNames(const std::vector<std::string>& names);

/*
	Scheduler do currículo de treino — transições por win rate POR ESTADO.

	Uso:
	    Scheduler g_scheduler;
	    void StepCallback(Learner* learner, ..., Report& report) {
	        g_scheduler.Update(learner, report);           // 1ª linha
	        for (auto& state : states)
	            if (state.goalScored)
	                g_scheduler.OnGoalScored(state.lastArena);
	    }

	Condição de avanço (por estado individual):
	  Todos os state setters com peso > 0 na fase atual e com episódios suficientes
	  têm de ter win rate >= advanceWinRate durante advanceItersNeeded iterações
	  CONSECUTIVAS. Se QUALQUER estado ativo descer abaixo do limiar, o contador
	  reinicia para zero.

	Persistência: salva currentPhaseIdx em checkpoints/scheduler_phase.txt para
	retomar a fase correta ao continuar de um checkpoint.
*/
class Scheduler {
public:
	Scheduler() : cfg(GetSchedulerConfig()) {}

	// Chamado na 1ª linha de StepCallback. Corre no máximo 1× por iteração PPO.
	void Update(GGL::Learner* learner, GGL::Report& report);

	// Chamado em StepCallback quando state.goalScored == true.
	void OnGoalScored(RocketSim::Arena* arena);

private:
	static constexpr int MIN_EPISODES_FOR_CHECK = 30; // episódios mínimos para contar um estado

	SchedulerConfig cfg;
	std::vector<ResolvedPPO> resolved;

	bool    initialized      = false;
	int     currentPhaseIdx  = 0;
	int     consecutiveIters = 0;  // iters consecutivas onde TODOS os estados ativos > limiar
	int64_t lastIteration    = -1;
	float   lastPolicyLR     = -1.0f;
	float   lastCriticLR     = -1.0f;

	std::unordered_map<RocketSim::Arena*, SchedulableState*> arenaToState;

	// Logging
	std::ofstream logFile;
	std::chrono::system_clock::time_point trainStart;

	void  Initialize(GGL::Learner* learner);
	std::unordered_map<std::string, float> CollectPerStateWinRate(
		GGL::Learner* learner, GGL::Report& report);
	bool  ShouldAdvance(const std::unordered_map<std::string, float>& perStateWR, uint64_t ts);
	void  ApplyPhase(GGL::Learner* learner, int phaseIdx);
	void  ApplyPhaseWeights(GGL::Learner* learner, const TrainingPhase& p);
	void  ApplyPPO(GGL::Learner* learner, uint64_t ts, int phaseIdx);
	void  SavePhase(GGL::Learner* learner);
	void  LoadPhase(GGL::Learner* learner);
	void  ResolveBaseline(GGL::Learner* learner);
	void  OpenLogFile(GGL::Learner* learner);
	void  LogPhaseEvent(const std::string& event, int phaseIdx, uint64_t ts, int64_t iter);
};
