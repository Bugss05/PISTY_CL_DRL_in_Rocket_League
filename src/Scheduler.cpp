#include "Scheduler.h"
#include "SchedulableState.h"

#include <cstdio>
#include <cmath>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <ctime>

using namespace GGL;

// ---------------------------------------------------------------------------
// Registo de nomes das rewards (chamado uma vez em EnvCreateFunc)
// ---------------------------------------------------------------------------
std::vector<std::string> g_rewardNames;
static std::once_flag g_rewardNamesFlag;

void RegisterRewardNames(const std::vector<std::string>& names) {
	std::call_once(g_rewardNamesFlag, [&] { g_rewardNames = names; });
}

// ---------------------------------------------------------------------------
// Inicialização (1ª chamada a Update)
// ---------------------------------------------------------------------------
void Scheduler::ResolveBaseline(Learner* learner) {
	const PPOLearnerConfig& live = learner->GetLivePPOConfig();
	ResolvedPPO baseline;
	baseline.policyLR          = live.policyLR;
	baseline.criticLR          = live.criticLR;
	baseline.entropyScale      = live.entropyScale;
	baseline.clipRange         = live.clipRange;
	baseline.policyTemperature = live.policyTemperature;
	baseline.epochs            = live.epochs;
	baseline.gaeGamma          = learner->config.ppo.gaeGamma;
	baseline.gaeLambda         = learner->config.ppo.gaeLambda;
	baseline.rewardClipRange   = learner->config.ppo.rewardClipRange;
	resolved = SchedulerResolvePhases(cfg, baseline);
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
static std::string FormatTime(std::chrono::system_clock::time_point tp) {
	std::time_t t = std::chrono::system_clock::to_time_t(tp);
	std::tm tm_buf;
#ifdef _WIN32
	localtime_s(&tm_buf, &t);
#else
	localtime_r(&t, &tm_buf);
#endif
	std::ostringstream ss;
	ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
	return ss.str();
}

static std::string FormatElapsed(std::chrono::system_clock::duration d) {
	auto total = std::chrono::duration_cast<std::chrono::seconds>(d).count();
	long long h = total / 3600, m = (total % 3600) / 60, s = total % 60;
	std::ostringstream ss;
	ss << std::setfill('0')
	   << std::setw(2) << h << "h"
	   << std::setw(2) << m << "m"
	   << std::setw(2) << s << "s";
	return ss.str();
}

void Scheduler::OpenLogFile(Learner* learner) {
	trainStart = std::chrono::system_clock::now();

	// Cria pasta logs/ junto ao executável (diretório de trabalho)
	std::filesystem::path logsDir = "logs";
	std::filesystem::create_directories(logsDir);

	// Nome do ficheiro = data+hora de início
	std::time_t t = std::chrono::system_clock::to_time_t(trainStart);
	std::tm tm_buf;
#ifdef _WIN32
	localtime_s(&tm_buf, &t);
#else
	localtime_r(&t, &tm_buf);
#endif
	std::ostringstream name;
	name << std::put_time(&tm_buf, "%Y-%m-%d_%H-%M-%S") << ".log";
	auto logPath = logsDir / name.str();

	logFile.open(logPath, std::ios::out | std::ios::app);
	if (!logFile.is_open()) {
		printf("[Scheduler] AVISO: nao foi possivel criar o ficheiro de log em %s\n",
			logPath.string().c_str());
		return;
	}

	logFile << "========================================================\n";
	logFile << "  Treino iniciado: " << FormatTime(trainStart) << "\n";
	logFile << "  Fases: " << cfg.phases.size() << "\n";
	for (int i = 0; i < (int)cfg.phases.size(); i++)
		logFile << "    [" << i << "] " << cfg.phases[i].name << "\n";
	logFile << "========================================================\n\n";
	logFile.flush();

	printf("[Scheduler] Log: %s\n", logPath.string().c_str());
}

void Scheduler::LogPhaseEvent(const std::string& event, int phaseIdx,
                               uint64_t ts, int64_t iter) {
	if (!logFile.is_open()) return;
	auto now     = std::chrono::system_clock::now();
	auto elapsed = now - trainStart;
	logFile << "[" << FormatTime(now) << "] "
	        << "[+" << FormatElapsed(elapsed) << "] "
	        << event << "\n"
	        << "  Fase       : [" << phaseIdx << "] "
	        << cfg.phases[phaseIdx].name << "\n"
	        << "  Timesteps  : " << ts << "\n"
	        << "  Iteracao   : " << iter << "\n\n";
	logFile.flush();
}

// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------
void Scheduler::Initialize(Learner* learner) {
	for (int i = 0; i < (int)learner->envSet->arenas.size(); i++) {
		Arena* arena = learner->envSet->arenas[i];

		auto* ss = dynamic_cast<SchedulableState*>(learner->envSet->stateSetters[i]);
		if (ss)
			arenaToState[arena] = ss;

		for (auto* tc : learner->envSet->terminalConditions[i]) {
			auto* st = dynamic_cast<SchedulableTerminal*>(tc);
			if (st) { arenaToTerminal[arena] = st; break; }
		}
	}

	ResolveBaseline(learner);
	OpenLogFile(learner);
	LoadPhase(learner);

	initialized   = true;
	lastIteration = (int64_t)learner->totalIterations;

	ApplyPhase(learner, currentPhaseIdx);
	printf("[Scheduler] Inicializado na fase %d: \"%s\"\n",
		currentPhaseIdx, cfg.phases[currentPhaseIdx].name.c_str());

	LogPhaseEvent("Inicio do treino", currentPhaseIdx,
		learner->totalTimesteps, (int64_t)learner->totalIterations);
}

// ---------------------------------------------------------------------------
// OnGoalScored / OnBallTouched — chamados de StepCallback
// ---------------------------------------------------------------------------
void Scheduler::OnGoalScored(RocketSim::Arena* arena) {
	auto it = arenaToState.find(arena);
	if (it != arenaToState.end())
		it->second->NotifyGoal();
}

void Scheduler::OnBallTouched(RocketSim::Arena* arena) {
	// Só conta como vitória se o terminal "BallTouch" estiver ativo nessa arena
	auto termIt = arenaToTerminal.find(arena);
	if (termIt == arenaToTerminal.end()) return;
	if (!termIt->second->IsConditionActive("BallTouch")) return;

	auto stateIt = arenaToState.find(arena);
	if (stateIt != arenaToState.end())
		stateIt->second->NotifyGoal();
}

// ---------------------------------------------------------------------------
// CollectPerStateStats — agrega de TODAS as arenas, adiciona ao Report
// ---------------------------------------------------------------------------
Scheduler::StateStats
Scheduler::CollectPerStateStats(Learner* learner, Report& report) {
	StateStats out;
	for (auto* setter : learner->envSet->stateSetters) {
		auto* ss = dynamic_cast<SchedulableState*>(setter);
		if (!ss) continue;
		auto stats = ss->GetAndResetStats();
		for (int i = 0; i < (int)stats.names.size(); i++) {
			out.episodes[stats.names[i]] += stats.episodes[i];
			out.goals[stats.names[i]]    += stats.goals[i];
		}
	}

	for (auto& [name, eps] : out.episodes) {
		if (eps < MIN_EPISODES_FOR_CHECK) continue;
		float wr = (float)out.goals[name] / eps;
		out.winRate[name] = wr;
		report.Add("WinRate/" + name, wr);
	}

	return out;
}

// ---------------------------------------------------------------------------
// ShouldAdvance — verifica se TODOS os estados ativos passaram o limiar
// ---------------------------------------------------------------------------
bool Scheduler::ShouldAdvance(const StateStats& stats, uint64_t ts) {
	if (currentPhaseIdx >= (int)cfg.phases.size() - 1)
		return false;

	const TrainingPhase& p = cfg.phases[currentPhaseIdx];

	// Safety net
	if (p.advanceMaxTimesteps && ts >= *p.advanceMaxTimesteps) {
		printf("[Scheduler] Safety net (%llu ts) — avanca para proxima fase.\n",
			(unsigned long long)ts);
		consecutiveIters = 0;
		return true;
	}

	if (!p.advanceWinRate)
		return false;

	float threshold = *p.advanceWinRate;
	int   needed    = p.advanceItersNeeded.value_or(5);

	bool allAbove   = true;
	bool anyChecked = false;

	printf("[Scheduler]   %-18s  %6s  %6s  %7s  %s\n",
		"Estado", "EPs", "Wins", "WinRate", "OK?");
	printf("[Scheduler]   %-18s  %6s  %6s  %7s  %s\n",
		"------", "---", "----", "-------", "---");

	for (auto& [name, weight] : p.stateWeights) {
		if (weight <= 0.0f) continue;

		auto epsIt = stats.episodes.find(name);
		int  eps   = (epsIt != stats.episodes.end()) ? epsIt->second : 0;
		int  wins  = 0;
		auto gIt   = stats.goals.find(name);
		if (gIt != stats.goals.end()) wins = gIt->second;

		if (eps < MIN_EPISODES_FOR_CHECK) {
			printf("[Scheduler]   %-18s  %6d  %6s  %7s  (poucos dados)\n",
				name.c_str(), eps, "-", "-");
			continue;
		}

		anyChecked = true;
		float wr = stats.winRate.at(name);
		bool  ok = (wr >= threshold);
		printf("[Scheduler]   %-18s  %6d  %6d  %6.1f%%  %s\n",
			name.c_str(), eps, wins, wr * 100.f, ok ? "OK" : "NOK");
		if (!ok) allAbove = false;
	}

	// Se não há estados ativos com dados suficientes, não avança
	if (!anyChecked) {
		printf("[Scheduler]   (nenhum estado com dados suficientes)\n");
		return false;
	}

	if (allAbove) {
		consecutiveIters++;
		printf("[Scheduler]   TODOS acima de %.0f%% — %d/%d iteracoes consecutivas.\n",
			threshold * 100.f, consecutiveIters, needed);
	} else {
		if (consecutiveIters > 0)
			printf("[Scheduler]   Algum estado abaixo — contador reset (%d -> 0).\n",
				consecutiveIters);
		consecutiveIters = 0;
	}

	return consecutiveIters >= needed;
}

// ---------------------------------------------------------------------------
// ApplyPhase — aplica rewards + setters + PPO da fase indicada
// ---------------------------------------------------------------------------
void Scheduler::ApplyPhase(Learner* learner, int phaseIdx) {
	ApplyPhaseWeights(learner, cfg.phases[phaseIdx]);
	ApplyPPO(learner, learner->totalTimesteps, phaseIdx);
}

void Scheduler::ApplyPhaseWeights(Learner* learner, const TrainingPhase& p) {
	for (auto& arenaRewards : learner->envSet->rewards) {
		for (size_t i = 0; i < arenaRewards.size() && i < g_rewardNames.size(); i++) {
			auto it = p.rewardWeights.find(g_rewardNames[i]);
			if (it != p.rewardWeights.end())
				arenaRewards[i].weight = it->second;
		}
	}
	for (auto* s : learner->envSet->stateSetters) {
		auto* ss = dynamic_cast<SchedulableState*>(s);
		if (!ss) continue;
		if (p.stochastic.has_value())
			ss->SetStochastic(*p.stochastic);
		if (!p.stateWeights.empty())
			ss->SetWeights(p.stateWeights);
	}

	if (!p.terminalActive.empty()) {
		for (auto& [arena, st] : arenaToTerminal)
			st->SetActive(p.terminalActive);

		printf("[Scheduler]   Terminal conditions atualizadas:\n");
		for (auto& [name, active] : p.terminalActive)
			printf("[Scheduler]     %-16s -> %s\n", name.c_str(), active ? "ON" : "OFF");
	}
}

void Scheduler::ApplyPPO(Learner* learner, uint64_t ts, int phaseIdx) {
	ResolvedPPO eff = SchedulerEffective(cfg, resolved, ts, phaseIdx);

	if (std::fabs(eff.policyLR - lastPolicyLR) > 1e-9f ||
	    std::fabs(eff.criticLR - lastCriticLR) > 1e-9f) {
		learner->SetLearningRates(eff.policyLR, eff.criticLR);
		lastPolicyLR = eff.policyLR;
		lastCriticLR = eff.criticLR;
	}

	PPOLearnerConfig& live = learner->GetLivePPOConfig();
	live.entropyScale      = eff.entropyScale;
	live.clipRange         = eff.clipRange;
	live.policyTemperature = eff.policyTemperature;
	live.epochs            = eff.epochs;

	learner->config.ppo.gaeGamma        = eff.gaeGamma;
	learner->config.ppo.gaeLambda       = eff.gaeLambda;
	learner->config.ppo.rewardClipRange = eff.rewardClipRange;
}

// ---------------------------------------------------------------------------
// Persistência do índice de fase
// ---------------------------------------------------------------------------
void Scheduler::SavePhase(Learner* learner) {
	if (learner->config.checkpointFolder.empty()) return;
	auto path = learner->config.checkpointFolder / "scheduler_phase.txt";
	std::ofstream f(path);
	if (f) f << currentPhaseIdx;
}

void Scheduler::LoadPhase(Learner* learner) {
	if (learner->config.checkpointFolder.empty()) return;
	auto path = learner->config.checkpointFolder / "scheduler_phase.txt";
	std::ifstream f(path);
	if (!f) return;
	int idx;
	if (f >> idx && idx >= 0 && idx < (int)cfg.phases.size()) {
		currentPhaseIdx = idx;
		printf("[Scheduler] Fase %d (\"%s\") carregada do checkpoint.\n",
			idx, cfg.phases[idx].name.c_str());
	}
}

// ---------------------------------------------------------------------------
// Update — ponto de entrada principal
// ---------------------------------------------------------------------------
void Scheduler::Update(Learner* learner, Report& report) {
	if (cfg.phases.empty() || learner->config.renderMode)
		return;

	if (!initialized) {
		Initialize(learner);
		return;
	}

	// Corre no máximo uma vez por iteração PPO
	int64_t it = (int64_t)learner->totalIterations;
	if (it == lastIteration)
		return;
	lastIteration = it;

	uint64_t ts = learner->totalTimesteps;

	printf("\n[Scheduler] === Iteracao %lld | Fase %d: \"%s\" | %llu ts ===\n",
		(long long)it,
		currentPhaseIdx,
		cfg.phases[currentPhaseIdx].name.c_str(),
		(unsigned long long)ts);

	// 1. Recolhe stats por estado (e adiciona ao report para wandb)
	auto stats = CollectPerStateStats(learner, report);

	// 2. Verifica se avança de fase
	if (ShouldAdvance(stats, ts)) {
		currentPhaseIdx++;
		consecutiveIters = 0;
		printf("[Scheduler] *** AVANCA para fase %d: \"%s\" @ %llu ts ***\n",
			currentPhaseIdx,
			cfg.phases[currentPhaseIdx].name.c_str(),
			(unsigned long long)ts);
		ApplyPhase(learner, currentPhaseIdx);
		SavePhase(learner);
		LogPhaseEvent("Transicao de fase", currentPhaseIdx, ts, it);
	} else if (cfg.interpolatePPOParams) {
		ApplyPPO(learner, ts, currentPhaseIdx);
	}

	printf("[Scheduler] ================================================\n");
}
