#pragma once
#include <GigaLearnCPP/Learner.h>
#include <GigaLearnCPP/Util/Report.h>
#include "SchedulerConfig.h"
#include "SchedulerCore.h"
#include "SchedulableState.h"
#include "SchedulableTerminal.h"
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

	Fase de transição (opcional, ver TransitionConfig no SchedulerConfig):
	  Ao atingir o limiar, em vez de saltar logo para a fase seguinte, corre uma
	  janela de transição de N timesteps a interpolar linearmente os pesos das
	  rewards e/ou da distribuição de estados (cada um com a sua flag). No fim da
	  janela, a fase seguinte é aplicada por inteiro. Durante a transição não há
	  novo avanço por win rate.

	Persistência: salva currentPhaseIdx em checkpoints/scheduler_phase.txt para
	retomar a fase correta ao continuar de um checkpoint. A transição em curso NÃO
	é persistida: ao retomar a meio de uma transição, retoma-se na fase de origem
	(que volta a atingir o limiar e a re-disparar a transição — inócuo).
*/
class Scheduler {
public:
	Scheduler() : cfg(GetSchedulerConfig()) {}

	// Chamado na 1ª linha de StepCallback. Corre no máximo 1× por iteração PPO.
	void Update(GGL::Learner* learner, GGL::Report& report);

	// Chamado em StepCallback quando state.goalScored == true.
	void OnGoalScored(RocketSim::Arena* arena);

	// Chamado em StepCallback quando qualquer jogador toca na bola (e goal não foi marcado).
	// Conta como vitória se o terminal "BallTouch" estiver ativo nessa arena.
	void OnBallTouched(RocketSim::Arena* arena);

private:
	static constexpr int MIN_EPISODES_FOR_CHECK = 30;

	struct StateStats {
		std::unordered_map<std::string, int>   episodes;
		std::unordered_map<std::string, int>   goals;
		std::unordered_map<std::string, float> winRate;
	}; // episódios mínimos para contar um estado

	SchedulerConfig cfg;
	std::vector<ResolvedPPO> resolved;

	bool    initialized      = false;
	int     currentPhaseIdx  = 0;
	int     consecutiveIters = 0;  // iters consecutivas onde TODOS os estados ativos > limiar
	int64_t lastIteration    = -1;
	float   lastPolicyLR     = -1.0f;
	float   lastCriticLR     = -1.0f;

	// --- Fase de transição (interpolação de pesos entre fases) ---
	bool             inTransition        = false;
	uint64_t         transitionStartTs   = 0;
	uint64_t         transitionEndTs     = 0;
	int              transitionTargetIdx = -1;   // fase de destino (currentPhaseIdx + 1)
	TransitionConfig transitionCfg;              // config resolvida da transição ativa
	std::unordered_map<std::string, float> transitionStartRewardW;  // pesos no início (por nome)
	std::unordered_map<std::string, float> transitionStartStateW;   // pesos no início (por nome)

	std::unordered_map<RocketSim::Arena*, SchedulableState*>    arenaToState;
	std::unordered_map<RocketSim::Arena*, SchedulableTerminal*> arenaToTerminal;

	// Logging
	std::ofstream logFile;
	std::chrono::system_clock::time_point trainStart;

	void  Initialize(GGL::Learner* learner);
	StateStats CollectPerStateStats(GGL::Learner* learner, GGL::Report& report);
	bool  ShouldAdvance(const StateStats& stats, uint64_t ts);
	void  ApplyPhase(GGL::Learner* learner, int phaseIdx);
	void  ApplyPhaseWeights(GGL::Learner* learner, const TrainingPhase& p);
	void  ApplyPPO(GGL::Learner* learner, uint64_t ts, int phaseIdx);

	// Transição entre fases
	TransitionConfig ResolveTransition(int phaseIdx) const;
	void  BeginTransition(GGL::Learner* learner, uint64_t ts);
	void  UpdateTransition(GGL::Learner* learner, uint64_t ts);
	void  FinalizeTransition(GGL::Learner* learner, uint64_t ts, int64_t iter);
	void  SavePhase(GGL::Learner* learner);
	void  LoadPhase(GGL::Learner* learner);
	void  ResolveBaseline(GGL::Learner* learner);
	void  OpenLogFile(GGL::Learner* learner);
	void  LogPhaseEvent(const std::string& event, int phaseIdx, uint64_t ts, int64_t iter);
};
