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

	Cada TrainingPhase fica ativa até a condição de avanço ser cumprida:
	  - advanceWinRate + advanceItersNeeded : avança quando a win rate global (golos/episódios)
	    ficar ACIMA do limiar durante N iterações PPO CONSECUTIVAS.
	  - advanceMaxTimesteps : safety net — avança mesmo que o limiar nunca seja atingido.
	  - Deixar ambos vazios = fase permanente (última fase do currículo).

	Regras de preenchimento dos campos PPO (std::optional):
	  - Vazio ({}) = mantém o valor da fase anterior (forward-fill).
	  - A fase 0 herda os valores do ExampleMain.cpp (o que passaste ao Learner).

	Nomes de rewards: a string que puseste em namedRewards dentro de EnvCreateFunc.
	Nomes de setters: a label que deste no SchedulableState dentro de EnvCreateFunc.

	interpolatePPOParams (false por defeito com fases por métricas):
	  - false : cada parâmetro PPO muda em DEGRAU quando a fase avança.
	  - true  : interpola LINEARMENTE entre o valor atual e o da fase seguinte,
	            ao longo dos timesteps entre as duas fases. Só faz sentido se
	            advanceMaxTimesteps estiver definido nas fases (senão não há
	            ponto de chegada para interpolar). Para treino normal deixa false.
*/

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

	// --- State setters ---
	std::optional<bool> stochastic;
	std::unordered_map<std::string, float> stateWeights;
};

struct SchedulerConfig {
	// false = mudança em degrau ao transitar de fase (recomendado com fases por métrica)
	// true  = interpolação linear entre fases — só útil se advanceMaxTimesteps estiver definido
	bool interpolatePPOParams = false;
	std::vector<TrainingPhase> phases;
};

// ------------------------------------------------------------------------------------------
//  Define o teu currículo aqui. As fases são percorridas em ordem; a última é permanente.
//  Ajusta advanceWinRate e advanceItersNeeded ao ritmo do teu treino.
// ------------------------------------------------------------------------------------------
inline SchedulerConfig GetSchedulerConfig() {
	SchedulerConfig cfg;
	cfg.interpolatePPOParams = false;

	cfg.phases = {

		// -------- Fase 1: aprender a tocar a bola (WallDrag) --------
		// Avança quando o bot marca em >70% dos episódios durante 5 iterações seguidas.
		// Safety: avança de qualquer forma aos 80M steps.
		TrainingPhase{
			.name                = "1-TocarBola",
			.advanceWinRate      = 0.7f,
			.advanceItersNeeded  = 5,
			.advanceMaxTimesteps = 80'000'000ULL,

			.policyLR        = 2e-4f,
			.criticLR        = 2e-4f,
			.entropyScale    = 0.035f,
			.epochs          = 1,
			.gaeGamma        = 0.99f,

			.rewardWeights = {
				{ "VelocityPlayerToBall", 0.5f  },
				{ "VelocityBallToGoal",   0.8f  },
				{ "Goal",               100.0f  },
				{ "TouchBallAerial",      1.0f  },
				{ "Air",                  0.02f },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "WallDrag",  1.0f },
				{ "GoalShot",  0.0f },
				{ "AirShot",   0.0f },
				{ "Kickoff",   0.0f },
			},
		},

		// -------- Fase 2: remates ao primeiro toque (Kickoff + GoalShot) --------
		// Avança quando o bot marca em >80% dos episódios durante 5 iterações seguidas.
		// Safety: avança aos 200M steps.
		TrainingPhase{
			.name                = "2-Remates",
			.advanceWinRate      = 0.8f,
			.advanceItersNeeded  = 5,
			.advanceMaxTimesteps = 200'000'000ULL,

			.policyLR     = 1.5e-4f,
			.criticLR     = 1.5e-4f,
			.entropyScale = 0.025f,
			.epochs       = 2,
			.gaeGamma     = 0.993f,

			.rewardWeights = {
				{ "VelocityPlayerToBall", 0.2f  },
				{ "VelocityBallToGoal",   1.5f  },
				{ "Goal",                80.0f  },
				{ "TouchBallAerial",      1.5f  },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "WallDrag", 0.0f },
				{ "GoalShot", 0.5f },
				{ "Kickoff",  0.5f },
				{ "AirShot",  0.0f },
			},
		},

		// -------- Fase 3: remates aéreos --------
		// Última fase — permanente (sem condição de avanço).
		TrainingPhase{
			.name = "3-Aereos",

			.policyLR     = 1e-4f,
			.criticLR     = 1e-4f,
			.entropyScale = 0.018f,
			.epochs       = 2,
			.gaeGamma     = 0.995f,

			.rewardWeights = {
				{ "VelocityBallToGoal", 1.5f },
				{ "Goal",              80.0f },
				{ "TouchBallAerial",    6.0f },
				{ "AirAlignment",       0.8f },
				{ "AerialDistance",     0.8f },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "GoalShot", 0.2f },
				{ "AirShot",  0.8f },
				{ "Kickoff",  0.0f },
				{ "WallDrag", 0.0f },
			},
		},
	};

	return cfg;
}
