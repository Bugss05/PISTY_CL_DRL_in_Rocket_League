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
	Nomes de terminais: a string que puseste nas entries do SchedulableTerminal em EnvCreateFunc.

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

	// --- Condições terminais ativas (atualização parcial; vazio = sem alteração) ---
	// Nomes: os que puseste nas entries do SchedulableTerminal em EnvCreateFunc.
	std::unordered_map<std::string, bool> terminalActive;
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

		// ══════════════════════════════════════════════════════════════
		// FASES DE REMATE (chão)
		// ══════════════════════════════════════════════════════════════

		// -------- Fase 1: tocar a bola --------
		// Objetivo: aproximar e tocar. Episódio termina ao toque.
		// Rewards: aproximação + toque com velocidade. Sem direção à baliza (ainda não interessa).
		TrainingPhase{
			.name               = "1-TocarBola",
			.advanceWinRate     = 0.85f,
			.advanceItersNeeded = 8,

			.policyLR     = 2e-4f,
			.criticLR     = 2e-4f,
			.entropyScale = 0.035f,
			.epochs       = 1,
			.gaeGamma     = 0.99f,

			.rewardWeights = {
				{ "VelocityPlayerToBall", 5.0f  },  // aproximação à bola
				{ "TouchAccel",        200.0f },  // tocar com velocidade = win
				{ "VelocityBallToGoal",   0.0f  },
				{ "GoalBonus",            0.0f  },
				{ "FaceBall",             1.0f  },  // NOVO: recompensar alinhamento com a bola
				{ "LandAllFours",         0.3f  },  // NOVO: recompensar aterragem estável após toque aéreo}
				{ "Velocity",             0.2f  },  // NOVO: recompensa geral por velocidade, para incentivar movimento mesmo que ainda não acerte na bola
				{ "ConstantPenalty",     -0.2f },  // NOVO: pequeno castigo por cada passo, para incentivar a resolver o episódio rápido
			},
			.stochastic   = true,
			.stateWeights = {
				{ "Random",      0.25f },
				{ "GoalShot",    0.30f },
				{ "FallingBall", 0.45f },
			},
			.terminalActive = {
				{ "BallTouch", true  },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
		},

		// -------- Fase 2: GoalShot — 1º toque direcionado --------
		// Objetivo: no toque, enviar a bola para a baliza.
		// Rewards: direção à baliza é agora a principal. Toque forte como secundário.
		// VelocityPlayerToBall cai muito (já está perto no GoalShot).
		TrainingPhase{
			.name               = "2-GoalShot 1toque",
			.advanceWinRate     = 0.90f,
			.advanceItersNeeded = 8,


			.rewardWeights = {
				{ "VelocityPlayerToBall", 3.0f  },
				{ "TouchAccel",           20.0f },
				{ "VelocityBallToGoal",   8.0f  },  // shaping: direcionar bola à baliza
				{ "FaceBall",             1.0f  },
				{ "LandAllFours",         0.3f  },
				{ "Velocity",             0.2f  },
				{ "ConstantPenalty",     -0.4f  },
				{ "Goal",                 200.0f},
				{ "GoalBonus",            0.0f  },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "Random",      0.0f },
				{ "GoalShot",    1.0f },
				{ "FallingBall", 0.0f },
				{ "Pass",        0.0f },
			},
			.terminalActive = {
				{ "BallTouch", false }, // desativa terminal de toque para permitir múltiplos toques e foco na direção},
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
		},

		// -------- Fase 3b: Pass — bola com velocidade dirigida ao carro --------
		// Objetivo: temporizar o remate numa bola em movimento.
		// A bola VEM ao carro → VelocityPlayerToBall irrelevante.
		// Rewards: força do toque + direção pós-toque.
		TrainingPhase{
			.name               = "3b-Pass",
			.advanceWinRate     = 0.80f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "VelocityPlayerToBall", 0.5f  },  // bola vem ao carro, pouca navegação necessária
				{ "TouchAccel",           3.0f  },
				{ "VelocityBallToGoal",   8.0f  },  // direção pós-toque é crítica
				{ "GoalBonus",            150.0f},
				{ "FaceBall",             0.5f  },
				{ "LandAllFours",         0.3f  },
				{ "Velocity",             0.2f  },
				{ "Wavedash",                0.5f  },  // recompensa por wavedash, para incentivar movimentação avançada
				{ "ConstantPenalty",     -0.4f  },
				{ "Goal",                 0.0f  },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "GoalShot",    0.0f },
				{ "FallingBall", 0.0f },
				{ "Pass",        1.0f },
			},
			.terminalActive = {
				{ "BallTouch", false },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
		},

		// -------- Fase 4: FallingBall — navegar e rematar --------
		// Objetivo: aproximar-se de uma bola aleatória e marcar.
		// A bola NÃO vem ao carro → VelocityPlayerToBall volta a ser importante.
		// Rewards: aproximação + direção + golo.
		TrainingPhase{
			.name               = "4-FallingBall",
			.advanceWinRate     = 0.75f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "VelocityPlayerToBall", 4.0f  },  // navegar até à bola
				{ "TouchAccel",        4.0f  },
				{ "StrongTouch",          0.0f  },  // desativa — foco na navegação
				{ "VelocityBallToGoal",   5.0f  },
				{ "GoalBonus",            500.0f},
				{ "GoalDistancePotential",2.0f  },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "GoalShot",    0.0f },
				{ "FallingBall", 1.0f },
				{ "Pass",        0.0f },
			},
			.terminalActive = {
				{ "BallTouch", false },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
		},

		// -------- Fase 5: consolidação chão --------
		// Objetivo: generalizar os três estados. Rewards equilibradas.
		TrainingPhase{
			.name               = "5-Consolidacao",
			.advanceWinRate     = 0.80f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "VelocityPlayerToBall", 2.0f  },
				{ "TouchAccel",           3.0f  },
				{ "VelocityBallToGoal",   4.0f  },
				{ "GoalBonus",            500.0f},
			},
			.stochastic   = true,
			.stateWeights = {
				{ "GoalShot",    0.2f },
				{ "Pass",        0.3f },
				{ "FallingBall", 0.5f },
			},
		},

		// ══════════════════════════════════════════════════════════════
		// FASES AÉREAS
		// ══════════════════════════════════════════════════════════════

		// -------- Fase 6: tocar bola aérea --------
		// Objetivo: chegar à bola enquanto no ar.
		// Rewards: estar no ar + aproximação aérea + alinhar com a bola.
		// Sem VelocityBallToGoal — foco total em CHEGAR à bola, não na direção.
		TrainingPhase{
			.name               = "6-Aerio toque",
			.advanceWinRate     = 0.75f,
			.advanceItersNeeded = 8,

			.policyLR     = 1.5e-4f,
			.criticLR     = 1.5e-4f,
			.entropyScale = 0.030f,
			.epochs       = 2,
			.gaeGamma     = 0.993f,

			.rewardWeights = {
				{ "Air",                  2.0f  },  // estar no ar
				{ "AirCloserToBall",      5.0f  },  // aproximação aérea à bola
				{ "AirAlignment",         3.0f  },  // alinhar corpo com bola
				{ "VelocityPlayerToBall", 1.0f  },  // aproximação geral
				{ "TouchAccel",        8.0f  },  // tocar a bola no ar
				{ "VelocityBallToGoal",   0.0f  },  // ainda não importa a direção
				{ "GoalBonus",            200.0f},  // bónus se marcar mas não é o foco
				{ "GoalDistancePotential",0.0f  },
				{ "StrongTouch",          0.0f  },
				{ "ConstantPenalty",      -0.02f},  // pequena penalidade por ineficiência
			},
			.stochastic   = true,
			.stateWeights = {
				{ "GoalShot",     0.0f },
				{ "FallingBall",  0.3f },
				{ "StaticAerial", 0.7f },
				{ "Pass",         0.0f },
			},
			.terminalActive = {
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
		},

		// -------- Fase 7: marcar aéreo --------
		// Objetivo: tocar a bola no ar E direcioná-la para a baliza.
		// Rewards: aproximação aérea + direção pós-toque + golo.
		TrainingPhase{
			.name               = "7-Aerio marcar",
			.advanceWinRate     = 0.65f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "Air",                  1.0f  },
				{ "AirCloserToBall",      3.0f  },
				{ "AirAlignment",         2.0f  },
				{ "VelocityPlayerToBall", 0.5f  },
				{ "TouchAccel",        5.0f  },
				{ "VelocityBallToGoal",   6.0f  },  // NOVO: direção à baliza
				{ "GoalBonus",            800.0f},  // marcar é agora o objetivo
				{ "ConstantPenalty",      -0.02f},
			},
			.stochastic   = true,
			.stateWeights = {
				{ "FallingBall",  0.1f },
				{ "StaticAerial", 0.1f },
				{ "Cross",        0.8f },  // cross: ângulo + velocidade
			},
		},

		// -------- Fase 8: aperfeiçoamento --------
		// Objetivo: consolidar tudo. Rewards equilibradas para não regredir.
		TrainingPhase{
			.name         = "8-Aperfeicoamento",
			.policyLR     = 0.8e-4f,
			.criticLR     = 0.8e-4f,
			.entropyScale = 0.025f,
			.epochs       = 2,
			.gaeGamma     = 0.997f,

			.rewardWeights = {
				{ "Air",                  0.5f  },
				{ "AirCloserToBall",      1.0f  },
				{ "AirAlignment",         1.0f  },
				{ "VelocityPlayerToBall", 0.5f  },
				{ "TouchAccel",           2.0f  },
				{ "VelocityBallToGoal",   2.0f  },
				{ "GoalBonus",            1000.0f},
				{ "WallLaunch",           1.0f  },
				{ "ConstantPenalty",      -0.02f},
			},
			.stochastic   = true,
			.stateWeights = {
				{ "FallingBall",  0.0f },
				{ "StaticAerial", 0.1f },
				{ "Cross",        0.5f },
				{ "WallDrag",     0.3f },
				{ "Random",       0.1f },
			},
		}
	};

	return cfg;
}
