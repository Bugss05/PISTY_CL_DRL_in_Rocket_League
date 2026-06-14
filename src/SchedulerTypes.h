#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/*
	Definições de tipos do Scheduler (o "schema" do currículo).
	NÃO precisas de editar este ficheiro — o currículo concreto vive em SchedulerConfig.h.
	Ver SchedulerConfig.h para a explicação de cada campo e como preencher as fases.
*/

// Comportamento da fase de transição entre uma fase e a seguinte.
struct TransitionConfig {
	uint64_t lengthTimesteps = 0;   // 0 = sem transição (salto imediato)
	bool lerpRewards      = false;  // interpola pesos das rewards (atual -> seguinte)
	bool lerpStateWeights = false;  // interpola pesos dos state setters (distribuição)
};

struct TrainingPhase {
	std::string name;

	// --- Condição de avanço para a PRÓXIMA fase ---
	std::optional<float>    advanceWinRate;       // limiar de golos/episódios (0.0-1.0)
	std::optional<int>      advanceItersNeeded;   // iterações consecutivas acima do limiar
	std::optional<uint64_t> advanceMaxTimesteps;  // safety net: avança de qualquer forma

	// --- Parâmetros PPO (vazio = manter valor anterior) ---
	std::optional<float> policyLR;
	std::optional<float> criticLR;
	std::optional<float> entropyScale;
	std::optional<float> clipRange;
	std::optional<float> policyTemperature;
	std::optional<int>   epochs;
	std::optional<float> gaeGamma;
	std::optional<float> gaeLambda;
	std::optional<float> rewardClipRange;

	// --- Pesos das rewards, por nome (atualização parcial) ---
	std::unordered_map<std::string, float> rewardWeights;

	// --- Parâmetros das rewards, por nome -> (param -> valor) (atualização parcial) ---
	// Só aplica às rewards que implementam Reward::SetParam (ver CommonRewards.h).
	// Ex.: { "StrongTouch", { {"minSpeedKPH", 40.f}, {"maxSpeedKPH", 150.f} } }
	// Unidades = as MESMAS dos argumentos do construtor da reward.
	std::unordered_map<std::string, std::unordered_map<std::string, float>> rewardParams;

	// --- State setters ---
	std::optional<bool> stochastic;
	std::unordered_map<std::string, float> stateWeights;

	// --- Parâmetros dos state setters, por nome -> (param -> valor) (atualização parcial) ---
	// Só aplica aos setters que implementam StateSetter::SetParam (ex.: GoalShotState "radius").
	// Ex.: { "GoalShot", { {"radius", 1600.f} } }
	std::unordered_map<std::string, std::unordered_map<std::string, float>> stateParams;

	// --- Condições terminais ativas (atualização parcial; vazio = sem alteração) ---
	// Nomes: os que puseste nas entries do SchedulableTerminal em EnvCreateFunc.
	std::unordered_map<std::string, bool> terminalActive;

	// --- Parâmetros das condições terminais, por nome -> (param -> valor) (atualização parcial) ---
	// Só aplica às condições que implementam TerminalCondition::SetParam (ex.: Timeout "seconds").
	// Ex.: { "Timeout", { {"seconds", 20.f} } }
	std::unordered_map<std::string, std::unordered_map<std::string, float>> terminalParams;

	// --- Transição AO SAIR desta fase (override; vazio = usa SchedulerConfig::transition) ---
	std::optional<TransitionConfig> transition;
};

struct SchedulerConfig {
	// false = mudança em degrau ao transitar de fase (recomendado com fases por métrica)
	// true  = interpolação linear entre fases — só útil se advanceMaxTimesteps estiver definido
	bool interpolatePPOParams = false;

	// Transição por defeito aplicada a TODAS as fases (a menos que a fase tenha override).
	TransitionConfig transition;

	std::vector<TrainingPhase> phases;
};
