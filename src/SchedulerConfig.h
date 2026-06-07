#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

/*
	==========================================================================================
	  CURRÍCULO DE TREINO  —  é AQUI que defines as fases. Não precisas de mexer no resto.
	==========================================================================================

	Cada TrainingPhase fica ativa a partir de `startTimestep` (em totalTimesteps). A fase
	atual é determinada pelo Scheduler a cada iteração a partir de learner->totalTimesteps,
	por isso retoma a fase certa automaticamente ao continuar de um checkpoint.

	Regras de preenchimento:
	  - PPO: campos std::optional. Se deixares vazio ({}), mantém o valor que vinha de trás.
	  - rewardWeights / stateWeights: mapa por NOME. Termos não listados ficam INALTERADOS.
	      * nome da reward  = reward->GetName()  (ex.: "VelocityPlayerToBallReward").
	        Atenção: ZeroSumReward(new GoalReward()) reporta-se como "ZeroSumReward".
	      * nome do setter  = a label que deste no SchedulableState no EnvCreateFunc.
	  - stochastic: vazio = mantém. true = pesos são probabilidades (amostragem aleatória).
	                false = pesos são percentagens (distribuição determinística exata).

	A flag global `interpolatePPOParams` (true por defeito):
	  - true : os params NUMÉRICOS do PPO (LR, entropy, clip, temperature, gae*) são
	           INTERPOLADOS linearmente entre o valor da fase atual e o da próxima fase,
	           ao longo dos timesteps. Transição suave (recomendado para LR/entropy).
	  - false: os params do PPO mudam em DEGRAU na fronteira da fase.
	  Nota: `epochs`, os pesos de reward e os pesos de state setters mudam SEMPRE em degrau.
*/

struct TrainingPhase {
	std::string name;
	uint64_t startTimestep = 0;

	// --- Parâmetros PPO (vazio = manter valor anterior) ---
	std::optional<float> policyLR;
	std::optional<float> criticLR;
	std::optional<float> entropyScale;
	std::optional<float> clipRange;
	std::optional<float> policyTemperature;
	std::optional<int>   epochs;            // sempre em degrau (inteiro)
	std::optional<float> gaeGamma;
	std::optional<float> gaeLambda;
	std::optional<float> rewardClipRange;

	// --- Pesos das rewards, por nome (atualização parcial) ---
	std::unordered_map<std::string, float> rewardWeights;

	// --- State setters ---
	std::optional<bool> stochastic;                       // vazio = manter
	std::unordered_map<std::string, float> stateWeights;  // por nome (atualização parcial)
};

struct SchedulerConfig {
	bool interpolatePPOParams = true;
	std::vector<TrainingPhase> phases;
};

// ------------------------------------------------------------------------------------------
//  Define o teu currículo aqui. As fases TÊM de estar ordenadas por startTimestep crescente.
//  Os valores abaixo são PLACEHOLDERS de exemplo — ajusta-os ao teu treino.
// ------------------------------------------------------------------------------------------
inline SchedulerConfig GetSchedulerConfig() {
	SchedulerConfig cfg;
	cfg.interpolatePPOParams = true;

	cfg.phases = {
		// -------- Fase 1: aprender a ir à bola / tocar --------
		TrainingPhase{
			/*name*/             "1-TocarBola",
			/*startTimestep*/    0,
			/*policyLR*/         2e-4f,
			/*criticLR*/         2e-4f,
			/*entropyScale*/     0.035f,
			/*clipRange*/        {},
			/*policyTemperature*/{},
			/*epochs*/           1,
			/*gaeGamma*/         0.99f,
			/*gaeLambda*/        {},
			/*rewardClipRange*/  {},
			/*rewardWeights*/    {
				{ "VelocityPlayerToBall", 0.5f },
				{ "VelocityBallToGoal",   0.8f },
				{ "Goal",               100.0f },
				{ "TouchBallAerial",      3.0f },
				{ "Air",                  0.05f },
			},
			/*stochastic*/       true,
			/*stateWeights*/     {
				{ "WallDrag", 1.0f },
				{ "Kickoff",  0.0f },
				{ "AirShot",  0.0f },
			},
		},

		// -------- Fase 2: remates / direcionar a bola à baliza --------
		TrainingPhase{
			/*name*/             "2-Remates",
			/*startTimestep*/    50'000'000,
			/*policyLR*/         1.5e-4f,
			/*criticLR*/         1.5e-4f,
			/*entropyScale*/     0.025f,
			/*clipRange*/        {},
			/*policyTemperature*/{},
			/*epochs*/           {},
			/*gaeGamma*/         0.993f,
			/*gaeLambda*/        {},
			/*rewardClipRange*/  {},
			/*rewardWeights*/    {
				{ "VelocityPlayerToBall", 0.2f },
				{ "VelocityBallToGoal",   1.5f },
			},
			/*stochastic*/       true,
			/*stateWeights*/     {
				{ "WallDrag", 0.3f },
				{ "Kickoff",  0.4f },
				{ "AirShot",  0.3f },
			},
		},

		// -------- Fase 3: aéreos --------
		TrainingPhase{
			/*name*/             "3-Aereos",
			/*startTimestep*/    150'000'000,
			/*policyLR*/         1e-4f,
			/*criticLR*/         1e-4f,
			/*entropyScale*/     0.018f,
			/*clipRange*/        {},
			/*policyTemperature*/{},
			/*epochs*/           {},
			/*gaeGamma*/         0.995f,
			/*gaeLambda*/        {},
			/*rewardClipRange*/  {},
			/*rewardWeights*/    {
				{ "TouchBallAerial",  6.0f },
				{ "AirAlignment",     0.8f },
				{ "AerialDistance",   0.8f },
			},
			/*stochastic*/       true,
			/*stateWeights*/     {
				{ "AirShot",  1.0f },
				{ "WallDrag", 0.0f },
				{ "Kickoff",  0.0f },
			},
		},
	};

	return cfg;
}
