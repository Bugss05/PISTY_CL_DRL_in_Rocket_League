#pragma once
#include "SchedulerTypes.h"   // TransitionConfig, TrainingPhase, SchedulerConfig (o schema)
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

	Parâmetros (além do peso): para as rewards/setters/terminais que implementam SetParam,
	podes mudar argumentos do construtor por fase via rewardParams / stateParams / terminalParams. Ex.:
	  .rewardParams   = { { "StrongTouch", { {"minSpeedKPH", 40.f}, {"maxSpeedKPH", 150.f} } } }
	  .stateParams    = { { "GoalShot",    { {"radius", 1600.f}, {"minHeight", 92.75f}, {"maxHeight", 550.f} } } }
	  .terminalParams = { { "Timeout",     { {"seconds", 20.f} } } }
	As unidades são as MESMAS dos argumentos do construtor. Chaves desconhecidas são ignoradas.
	Quais aceitam params: ver os override de SetParam em CommonRewards.h, nos StateSetters
	(ex.: GoalShotState radius/minHeight/maxHeight) e em TimeoutCondition (seconds).
	É atualização PARCIAL (forward-fill como os pesos).

	interpolatePPOParams (false por defeito com fases por métricas):
	  - false : cada parâmetro PPO muda em DEGRAU quando a fase avança.
	  - true  : interpola LINEARMENTE entre o valor atual e o da fase seguinte,
	            ao longo dos timesteps entre as duas fases. Só faz sentido se
	            advanceMaxTimesteps estiver definido nas fases (senão não há
	            ponto de chegada para interpolar). Para treino normal deixa false.

	FASE DE TRANSIÇÃO (transition):
	  Quando a condição de avanço de uma fase é atingida, em vez de saltar logo
	  para a fase seguinte podes correr uma fase intermédia de `lengthTimesteps`
	  timesteps. Durante essa janela, os pesos são interpolados LINEARMENTE dos
	  valores atuais (fim da fase atual) para os da fase seguinte. Cada
	  comportamento é ligado/desligado pela sua flag:
	    - lerpRewards      : interpola os pesos das rewards.
	    - lerpStateWeights : interpola os pesos dos state setters (distribuição de cenários).
	  Durante a transição NÃO se verifica win rate (não há novo avanço) e os
	  parâmetros PPO, params, terminais e flag stochastic ficam nos valores da
	  fase de origem; a fase seguinte é aplicada por inteiro (em degrau) no fim da
	  transição. lengthTimesteps == 0 (ou ambas as flags false) = salto imediato
	  (comportamento clássico). Define o default global em SchedulerConfig::transition
	  e/ou override por fase em TrainingPhase::transition (transição AO SAIR dessa fase).
*/

// As definições de TransitionConfig / TrainingPhase / SchedulerConfig vivem em SchedulerTypes.h.
// Aqui só preenches a data (o currículo) em GetSchedulerConfig().

// ------------------------------------------------------------------------------------------
//  Define o teu currículo aqui. As fases são percorridas em ordem; a última é permanente.
//  Ajusta advanceWinRate e advanceItersNeeded ao ritmo do teu treino.
// ------------------------------------------------------------------------------------------
inline SchedulerConfig GetSchedulerConfig() {
	SchedulerConfig cfg;
	cfg.interpolatePPOParams = false;

	// Transição suave por defeito entre fases: ao atingir o limiar de win rate, corre
	// 5M timesteps a interpolar LINEARMENTE os pesos das rewards + a distribuição de estados
	// antes de fixar a fase seguinte. As alturas (stateParams), o timeout (terminalParams), os
	// terminais e o PPO mudam em DEGRAU no fim da janela. lengthTimesteps = 0 = salto imediato.
	cfg.transition = TransitionConfig{
		.lengthTimesteps  = 5'000'000,  // pesos das rewards mudam pouco a pouco ao longo de 5M steps
		.lerpRewards      = true,
		.lerpStateWeights = false,
	};

	// Notas de altura (uu): teto do campo CEILING_Z = 2044 -> metade ≈ 1022.
	//   - GoalShot: limitado por código a ≈550 (barra GOAL_HEIGHT-BALL_RADIUS) p/ a bola não
	//     passar por cima do golo. "no chão" = 92.75.
	//   - "um pouco acima de metade do campo" -> 1200 (topo do Pass alto / FallingBall).
	// Win rate por fase: contabiliza TOQUE enquanto BallTouch estiver ON; passa a contar GOLOS
	// quando BallTouch fica OFF (ver Scheduler::OnBallTouched / OnGoalScored).

	cfg.phases = {

		// ══════════════════════════════════════════════════════════════
		// BLOCO A — TOCAR NA BOLA (win = toque; BallTouch ON)
		// Sobe a bola pouco a pouco (GoalShot do chão até à barra) e encurta o episódio.
		// ══════════════════════════════════════════════════════════════

		// -------- Fase 1: tocar a bola (como está hoje) --------
		// GoalShot no chão, FallingBall nos valores normais. Só aproximar e tocar.
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
				{ "GoalBonus",            0.0f   },
				{ "VelocityBallToGoal",   0.0f   },
				{ "VelocityPlayerToBall", 5.0f   },  // aproximação à bola
				{ "FaceBall",             1.0f   },  // alinhar com a bola
				{ "TouchBallAerial",      0.0f   },				
				{ "TouchAccel",         200.0f   },  // tocar com velocidade = win
				{ "StrongTouch",         30.0f   },				
				{ "AerialDistance",       0.0f   },			
				{"HeightMatch",           0.0f   },  // sem foco em elevar a bola nesta fase	
				{ "Air",                  0.3f   },
				{ "Speed",                0.4f   },
				{ "BallTouchGround",     -2.0f   },
				{ "Wavedash",             0.3f   },
				{ "LandAllFours",         0.3f   },  // aterrar estável
				{ "ConstantPenalty",     -0.1f   },  // resolver o episódio rápido
				{ "VelocityTouch",         0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       0.0f   },
				{ "Whiff",                 0.0f   },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "Random",      0.25f },
				{ "GoalShot",    0.30f },
				{ "FallingBall", 0.45f },
			},
			.stateParams = {
				{ "GoalShot",    { { "minHeight", 92.75f }, { "maxHeight", 92.75f } } }, // bola no chão
				{ "FallingBall", { { "minHeight", 400.f  }, { "maxHeight", 700.f  } } }, // valores normais
			},
			.terminalActive = {
				{ "BallTouch", true  },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 40.f } } },
			},
		},


		TrainingPhase{
			.name               = "1.2-Marcar golo simples",
			.advanceWinRate     = 0.85f,
			.advanceItersNeeded = 8,


			.rewardWeights = {
				{ "GoalBonus",            500.0f   },
				{ "VelocityBallToGoal",   3.0f   },
				{ "VelocityPlayerToBall", 2.0f   },  // aproximação à bola
				{ "FaceBall",             1.0f   },  // alinhar com a bola
				{ "TouchBallAerial",      3.0f   },				
				{ "TouchAccel",           2.0f   },  // tocar com velocidade = win
				{ "StrongTouch",          5.0f   },				
				{ "AerialDistance",       0.0f   },			
				{"HeightMatch",           0.0f   },  // sem foco em elevar a bola nesta fase	
				{ "Air",                  0.7f   },
				{ "Speed",                0.4f   },
				{ "BallTouchGround",     -2.0f   },
				{ "Wavedash",             1.0f   },
				{ "LandAllFours",         0.8f   },  // aterrar estável
				{ "ConstantPenalty",     -0.1f   },  // resolver o episódio rápido
				{ "VelocityTouch",         0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       0.8f   },
				{ "Whiff",                 0.0f   },
			},
			.rewardParams = {
    			{ "GoalBonus", { { "concedeScale", -0.1f } } },  // -1 = atual (-500) | -0.1 = ameno (~-50) | 0 = sem castigo
			},
			.stochastic   = true,
			.stateWeights = {
				{ "Random",      0.5f },
				{ "GoalShot",    0.0f },
				{ "FallingBall", 0.5f },
			},
			.stateParams = {
				{ "GoalShot",    { { "minHeight", 92.75f }, { "maxHeight", 92.75f } } }, // bola no chão
				{ "FallingBall", { { "minHeight", 400.f  }, { "maxHeight", 700.f  } } }, // valores normais
			},
			.terminalActive = {
				{ "BallTouch", true  },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 30.0f } } },
			},
		},

		TrainingPhase{
			.name               = "2-ElevarBola",
			.advanceWinRate     = 0.80f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "GoalBonus",            500.0f   },
				{ "VelocityBallToGoal",   3.0f   },
				{ "VelocityPlayerToBall", 2.0f   },  // aproximação à bola
				{ "FaceBall",             1.0f   },  // alinhar com a bola
				{ "TouchBallAerial",      10.0f   },				
				{ "TouchAccel",           1.0f   },  // tocar com velocidade = win
				{ "StrongTouch",          2.0f   },				
				{ "AerialDistance",       0.0f   },			
				{"HeightMatch",           0.0f   },  // sem foco em elevar a bola nesta fase	
				{ "Air",                  0.7f   },
				{ "Speed",                0.2f   },
				{ "BallTouchGround",     -4.0f   },
				{ "Wavedash",             1.0f   },
				{ "LandAllFours",         0.8f   },  // aterrar estável
				{ "ConstantPenalty",     -0.1f   },  // resolver o episódio rápido
				{ "VelocityTouch",         0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       1.0f   },
				{ "Whiff",                 -5.0f   },
			},
			.rewardParams = {
    			{ "GoalBonus", { { "concedeScale", -0.1f } } },  // -1 = atual (-500) | -0.1 = ameno (~-50) | 0 = sem castigo
			},
			.stochastic   = true,
			.stateWeights = {
				{ "Random",      0.2f },
				{ "GoalShot",    0.6f },
				{ "FallingBall", 0.2f },
			},
			.stateParams = {
				{ "GoalShot",    { { "minHeight", 92.75f }, { "maxHeight", 550.f } } }, // bola no chão
				{ "FallingBall", { { "minHeight", 400.f  }, { "maxHeight", 700.f  } } }, // valores normais
			},
			.terminalActive = {
				{ "BallTouch", true  },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 30.0f } } },
			},
		},
		// -------- Fase 2: elevar a bola --------
		// Sobe o GoalShot do chão até à barra (≈550) e o FallingBall um pouco. Começam os
		// "aéreos pequenos": recompensa aproximação aérea para ele aprender a mover-se no ar.
		TrainingPhase{
			.name               = "2-ElevarBola",
			.advanceWinRate     = 0.80f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "GoalBonus",            500.0f   },
				{ "VelocityBallToGoal",   5.0f   },
				{ "VelocityPlayerToBall", 5.0f   },
				{ "FaceBall",             1.0f   },
				{ "TouchBallAerial",      4.0f   },				
				{ "TouchAccel",           2.0f   },
				{ "StrongTouch",          5.0f   },
				{"HeightMatch",           0.0f   },  // sem foco em elevar a bola nesta fase	
				{ "BallTouchGround",     -2.0f   }, // penaliza tocar a bola no chão (ajuda a elevar)				
				{ "AerialDistance",       0.0f   },				
				{ "Air",                  0.7f   },  
				{ "Speed",                0.4f   },
				{ "Wavedash",             2.0f   },
				{ "LandAllFours",         0.6f   },
				{ "ConstantPenalty",     -0.2f   },
				{ "VelocityTouch",         0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       3.0f   },
				{ "Whiff",                 -3.0f   },
			},
			.rewardParams = {
    			{ "GoalBonus", { { "concedeScale", -0.1f } } },  // -1 = atual (-500) | -0.1 = ameno (~-50) | 0 = sem castigo
			},
			.stochastic   = true,
			.stateWeights = {
				{ "Random",      0.25f },
				{ "GoalShot",    0.25f },
				{ "FallingBall", 0.5f },
			},
			.stateParams = {
				{ "GoalShot",    { { "minHeight", 92.75f }, { "maxHeight", 550.f } } }, // até à barra
				{ "FallingBall", { { "minHeight", 400.f }, { "maxHeight", 1200.f } } }, // range máximo
			},
			.terminalActive = {
				{ "BallTouch", false },  // toque = win (explícito p/ ser correto ao retomar nesta fase)
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 40.f } } },
			},
		},
/*
		// -------- Fase 3: encurtar episódio para 30s --------
		// Mesma distribuição; mais pressão temporal (ConstantPenalty maior).
		TrainingPhase{
			.name               = "3-Timeout30",
			.advanceWinRate     = 0.80f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "GoalBonus",            0.0f   },
				{ "VelocityBallToGoal",   0.0f   },
				{ "VelocityPlayerToBall", 5.0f   },
				{ "FaceBall",             1.0f   },
				{ "TouchBallAerial",      0.0f   },				
				{ "TouchAccel",         500.0f   },
				{ "StrongTouch",         30.0f   },	
				{"HeightMatch",          10.0f   },  			
				{ "AerialDistance",       0.0f   },				
				{ "Air",                  0.7f   },
				{ "Speed",                0.4f   },
				{ "Wavedash",             0.8f   },
				{ "LandAllFours",         0.3f   },
				{ "ConstantPenalty",     -0.5f   },  // mais pressão temporal
				{ "VelocityTouch",         0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       3.0f   },
				{ "Whiff",                 0.0f   },
				{ "BallTouchGround",       0.0f   },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "Random",      0.0f },
				{ "GoalShot",    0.8f },
				{ "FallingBall", 0.2f },
			},
			.rewardParams = {
    			{ "GoalBonus", { { "concedeScale", -0.1f } } },  // -1 = atual (-500) | -0.1 = ameno (~-50) | 0 = sem castigo
			},
			.stateParams = {
				{ "GoalShot",    { { "minHeight", 92.75f }, { "maxHeight", 550.f } } }, // até à barra
				{ "FallingBall", { { "minHeight", 400.f }, { "maxHeight", 1200.f } } }, // range máximo
			},
			.terminalActive = {
				{ "BallTouch", true  },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 30.f } } },
			},

		},

		// -------- Fase 4: encurtar episódio para 20s --------
		TrainingPhase{
			.name               = "4-Timeout20",
			.advanceWinRate     = 0.80f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "GoalBonus",            0.0f   },
				{ "VelocityBallToGoal",   0.0f   },
				{ "VelocityPlayerToBall", 4.0f   },
				{ "FaceBall",             1.0f   },
				{ "TouchBallAerial",      0.0f   },				
				{ "TouchAccel",         500.0f   },
				{ "StrongTouch",         30.0f   },		
				{"HeightMatch",          10.0f   },		
				{ "AerialDistance",       0.0f   },				
				{ "Air",                  0.7f   },
				{ "Speed",                0.4f   },
				{ "Wavedash",             0.8f   },	
				{ "LandAllFours",         0.3f   },
				{ "ConstantPenalty",     -0.5f   },
				{ "VelocityTouch",         0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       3.0f   },
				{ "Whiff",                 0.0f   },
				{ "BallTouchGround",       0.0f   },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "Random",      0.0f },
				{ "GoalShot",    0.55f },
				{ "FallingBall", 0.45f },
			},
			.terminalActive = {
				{ "BallTouch", true  },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 20.f } } },
			},
		},

		// ══════════════════════════════════════════════════════════════
		// BLOCO B — MARCAR GOLOS (win = golo; BallTouch OFF)
		// ══════════════════════════════════════════════════════════════

		// -------- Fase 5: GoalShot — marcar com alturas aleatórias --------
		// Só GoalShot, bola em qualquer altura do range (chão até barra). Direção à baliza
		// passa a ser o foco. A partir daqui conta GOLOS (BallTouch desligado).
		TrainingPhase{
			.name               = "5-GoalShot-Marcar",
			.advanceWinRate     = 0.80f,
			.advanceItersNeeded = 8,
			.gaeGamma	 = 0.995f,

			.rewardWeights = {

				{ "GoalBonus",            200.0f   },
				{ "VelocityBallToGoal",   8.0f   },  // direção à baliza
				{ "VelocityPlayerToBall", 3.0f   },
				{ "FaceBall",             1.0f   },
				{ "TouchBallAerial",      0.0f   },				
				{ "TouchAccel",          20.0f   },
				{ "StrongTouch",         20.0f   },				
				{ "AerialDistance",       0.0f   },	
				{"HeightMatch",          10.0f   },			
				{ "Air",                  0.5f   },
				{ "Speed",                0.4f   },
				{ "Wavedash",             0.5f   },	
				{ "LandAllFours",         0.3f   },
				{ "ConstantPenalty",     -0.4f   },
				{ "VelocityTouch",         0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       0.0f   },
				{ "Whiff",                 0.0f   },
				{ "BallTouchGround",       0.0f   },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "GoalShot",    1.0f },
				{ "FallingBall", 0.0f },
				{ "Pass",        0.0f },
				{ "Random",      0.0f },
			},
			.stateParams = {
				{ "GoalShot", { { "minHeight", 92.75f }, { "maxHeight", 550.f } } }, // range completo
			},
			.terminalActive = {
				{ "BallTouch", false },  // a partir daqui o win é GOLO
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 20.f } } },
			},
		},

		// -------- Fase 6: Passe baixo (valores normais) --------
		// Bola vem ao carro com velocidade. Começa nos valores normais de altura.
		TrainingPhase{
			.name               = "6-Passe-Baixo",
			.advanceWinRate     = 0.80f,
			.advanceItersNeeded = 8,
 
			.rewardWeights = {
				{ "GoalBonus",          400.0f   },
				{ "VelocityBallToGoal",   8.0f   },  // direção pós-toque
				{ "VelocityPlayerToBall", 0.5f   },  // bola vem ao carro
				{ "FaceBall",             0.5f   },
				{ "TouchBallAerial",      1.0f   },				
				{ "TouchAccel",           3.0f   },
				{ "StrongTouch",          1.0f   },		
				{"HeightMatch",           5.0f   },		
				{ "AerialDistance",       2.0f   },				
				{ "Air",                  0.3f   },
				{ "Speed",                0.2f   },
				{ "Wavedash",             0.5f   },
				{ "LandAllFours",         0.1f   },
				{ "ConstantPenalty",     -0.4f   },
				{ "VelocityTouch",         0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       0.0f   },
				{ "Whiff",                 0.0f   },
				{ "BallTouchGround",       0.0f   },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "GoalShot",    0.0f },
				{ "FallingBall", 0.0f },
				{ "Pass",        1.0f },
			},
			.stateParams = {
				{ "Pass", { { "minHeight", 300.f }, { "maxHeight", 900.f } } }, // normais
			},
			.terminalActive = {
				{ "BallTouch", false },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 20.f } } },
			},
		},

		// -------- Fase 7: Passe médio --------
		// Sobe a altura do passe; mais ênfase aérea (alinhar no ar).
		TrainingPhase{
			.name               = "7-Passe-Medio",
			.advanceWinRate     = 0.75f,
			.advanceItersNeeded = 8,
			.policyLR     = 1.5e-4f,  // refinar: baixar LR na consolidação
			.criticLR     = 1.5e-4f,

			.rewardWeights = {
				{ "GoalBonus",          400.0f   },
				{ "VelocityBallToGoal",   7.0f   },
				{ "VelocityPlayerToBall", 0.5f   },
				{ "FaceBall",             0.5f   },
				{ "TouchBallAerial",      3.0f   },				
				{ "TouchAccel",           1.0f   },
				{ "StrongTouch",          0.2f   },				
				{ "AerialDistance",       2.0f   },				
				{ "Air",                  0.5f   },
				{ "Speed",                0.2f   },
				{ "Wavedash",             0.5f   },
				{ "LandAllFours",         0.0f   },
				{ "ConstantPenalty",     -0.4f   },
				{ "VelocityTouch",         0.0f   },
				{ "HeightMatch",           0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       0.0f   },
				{ "Whiff",                 0.0f   },
				{ "BallTouchGround",       0.0f   },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "Pass", 1.0f },
			},
			.stateParams = {
				{ "Pass", { { "minHeight", 400.f }, { "maxHeight", 1050.f } } },
			},
			.terminalActive = {
				{ "BallTouch", false },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 20.f } } },
			},
		},

		// -------- Fase 8: Passe alto (um pouco acima de metade do campo) --------
		TrainingPhase{
			.name               = "8-Passe-Alto",
			.advanceWinRate     = 0.75f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "GoalBonus",          400.0f   },
				{ "VelocityBallToGoal",   6.0f   },
				{ "VelocityPlayerToBall", 0.5f   },
				{ "FaceBall",             0.3f   },
				{ "TouchBallAerial",      3.0f   },				
				{ "TouchAccel",           1.0f   },
				{ "StrongTouch",          0.2f   },				
				{ "AerialDistance",       4.0f   },				
				{ "Air",                  0.6f   },
				{ "Speed",                0.2f   },
				{ "Wavedash",             0.3f   },
				{ "LandAllFours",         0.1f   },
				{ "ConstantPenalty",     -0.4f   },
				{ "VelocityTouch",         0.0f   },
				{ "HeightMatch",           0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       0.0f   },
				{ "Whiff",                 0.0f   },
				{ "BallTouchGround",       0.0f   },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "Pass", 1.0f },
			},
			.stateParams = {
				{ "Pass", { { "minHeight", 500.f }, { "maxHeight", 1200.f } } }, // ≈ acima de metade
			},
			.terminalActive = {
				{ "BallTouch", false },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 20.f } } },
			},
		},

		// -------- Fase 9: FallingBall 70% + Passe 30% (ranges máximos) --------
		// Navegar até bola alta a cair + temporizar passes. Maior range de alturas.
		TrainingPhase{
			.name               = "9-FallingBall+Passe",
			.advanceWinRate     = 0.75f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "GoalBonus",          400.0f   },
				{ "VelocityBallToGoal",   5.0f   },
				{ "VelocityPlayerToBall", 2.0f   },  // navegar (FallingBall não vem ao carro)
				{ "FaceBall",             0.3f   },
				{ "TouchBallAerial",      3.0f   },				
				{ "TouchAccel",           1.0f   },
				{ "StrongTouch",          0.2f   },				
				{ "AerialDistance",       4.0f   },				
				{ "Air",                  0.6f   },
				{ "Speed",                0.2f   },
				{ "Wavedash",             0.3f   },
				{ "LandAllFours",         0.1f   },
				{ "ConstantPenalty",     -0.4f   },
				{ "VelocityTouch",         0.0f   },
				{ "HeightMatch",           0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       0.0f   },
				{ "Whiff",                 0.0f   },
				{ "BallTouchGround",       0.0f   },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "FallingBall", 0.7f },
				{ "Pass",        0.3f },
				{ "GoalShot",    0.0f },
			},
			.stateParams = {
				{ "FallingBall", { { "minHeight", 400.f }, { "maxHeight", 1200.f } } }, // range máximo
				{ "Pass",        { { "minHeight", 500.f }, { "maxHeight", 1200.f } } },
			},
			.terminalActive = {
				{ "BallTouch", false },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 20.f } } },
			},
		},

		// ══════════════════════════════════════════════════════════════
		// BLOCO C — CONSOLIDAÇÃO (3 estados, ranges máximos de altura)
		// ══════════════════════════════════════════════════════════════

		// -------- Fase 10: consolidação a 20s --------
		// GoalShot + Pass + FallingBall, todos no range máximo de altura. Timeout 20.
		TrainingPhase{
			.name               = "10-Consolidacao-20",
			.advanceWinRate     = 0.85f,
			.advanceItersNeeded = 8,

			.policyLR     = 1.0e-4f,  // refinar: baixar LR na consolidação
			.criticLR     = 1.0e-4f,

			.rewardWeights = {
				{ "GoalBonus",          300.0f   },
				{ "VelocityBallToGoal",   4.0f   },
				{ "VelocityPlayerToBall", 2.0f   },
				{ "FaceBall",             0.3f   },
				{ "TouchBallAerial",      4.0f   },				
				{ "TouchAccel",           1.0f   },
				{ "StrongTouch",          0.3f   },				
				{ "AerialDistance",       4.0f   },				
				{ "Air",                  0.5f   },
				{ "Speed",                0.2f   },
				{ "Wavedash",             0.3f   },
				{ "LandAllFours",         0.1f   },
				{ "ConstantPenalty",     -0.3f   },
				{ "VelocityTouch",         0.0f   },
				{ "HeightMatch",           0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       0.0f   },
				{ "Whiff",                 0.0f   },
				{ "BallTouchGround",       0.0f   },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "GoalShot",    0.34f },
				{ "Pass",        0.33f },
				{ "FallingBall", 0.33f },
			},
			.stateParams = {
				{ "GoalShot",    { { "minHeight", 92.75f }, { "maxHeight", 550.f  } } },
				{ "Pass",        { { "minHeight", 300.f  }, { "maxHeight", 1200.f } } },
				{ "FallingBall", { { "minHeight", 400.f  }, { "maxHeight", 1200.f } } },
			},
			.terminalActive = {
				{ "BallTouch", false },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 20.f } } },
			},
		},

		// -------- Fase 11: consolidação final a 15s (permanente) --------
		// Última fase: mesmo mix, timeout encurtado para 15s. Sem condição de avanço
		// (advance* vazios) => fica aqui até ao fim do treino.
		TrainingPhase{
			.name         = "11-Consolidacao-15",

			.policyLR     = 1.2e-4f,
			.criticLR     = 1.2e-4f,
			.gaeGamma	 = 0.995f,

			.rewardWeights = {
				{ "GoalBonus",          800.0f  },
				{ "VelocityBallToGoal",   4.0f   },
				{ "VelocityPlayerToBall", 2.0f   },
				{ "FaceBall",             0.3f   },
				{ "TouchBallAerial",      4.0f   },
				{ "TouchAccel",           1.0f   },
				{ "StrongTouch",          0.2f   },
				{ "AerialDistance",       4.0f   },				
				{ "Air",                  0.5f   },
				{ "Speed",                0.2f   },
				{ "Wavedash",             0.3f   },
				{ "LandAllFours",         0.1f   },
				{ "ConstantPenalty",     -0.4f   },
				{ "VelocityTouch",         0.0f   },
				{ "HeightMatch",           0.0f   },
				{ "WallLaunch",            0.0f   },
				{ "DoubleJumpBoost",       0.0f   },
				{ "Whiff",                 0.0f   },
				{ "BallTouchGround",       0.0f   },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "GoalShot",    0.34f },
				{ "Pass",        0.33f },
				{ "FallingBall", 0.33f },
			},
			.stateParams = {
				{ "GoalShot",    { { "minHeight", 92.75f }, { "maxHeight", 550.f  } } },
				{ "Pass",        { { "minHeight", 300.f  }, { "maxHeight", 1200.f } } },
				{ "FallingBall", { { "minHeight", 400.f  }, { "maxHeight", 1200.f } } },
			},
			.terminalActive = {
				{ "BallTouch", false },
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
			.terminalParams = {
				{ "Timeout", { { "seconds", 15.f } } },
			},
		},

		// ══════════════════════════════════════════════════════════════
		// TREINO AÉREO — COMENTADO (ativar quando o treino de chão estiver consolidado).
		// Para reativar: descomenta e usa os state setters AirShot / StaticAerial / Cross.
		// ══════════════════════════════════════════════════════════════
		
		// -------- Aéreo 1: tocar bola aérea --------
		TrainingPhase{
			.name               = "A1-Aerio toque",
			.advanceWinRate     = 0.75f,
			.advanceItersNeeded = 8,

			.policyLR     = 1.5e-4f,
			.criticLR     = 1.5e-4f,
			.entropyScale = 0.030f,
			.epochs       = 2,
			.gaeGamma     = 0.993f,

			.rewardWeights = {
				{ "Air",                  2.0f  },
				{ "AirAlignment",         3.0f  },
				{ "VelocityPlayerToBall", 1.0f  },
				{ "TouchAccel",           8.0f  },
				{ "VelocityBallToGoal",   0.0f  },
				{ "GoalBonus",          200.0f  },
				{ "ConstantPenalty",     -0.02f },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "FallingBall",  0.3f },
				{ "StaticAerial", 0.7f },
			},
			.terminalActive = {
				{ "NoTouch",   false },
				{ "GoalScore", true  },
				{ "Timeout",   true  },
			},
		},

		// -------- Aéreo 2: marcar aéreo --------
		TrainingPhase{
			.name               = "A2-Aerio marcar",
			.advanceWinRate     = 0.65f,
			.advanceItersNeeded = 8,

			.rewardWeights = {
				{ "Air",                  1.0f  },
				{ "AirAlignment",         2.0f  },
				{ "VelocityPlayerToBall", 0.5f  },
				{ "TouchAccel",           5.0f  },
				{ "VelocityBallToGoal",   6.0f  },
				{ "GoalBonus",          800.0f  },
				{ "ConstantPenalty",     -0.02f },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "FallingBall",  0.1f },
				{ "StaticAerial", 0.1f },
				{ "Cross",        0.8f },
			},
		},

		// -------- Aéreo 3: aperfeiçoamento --------
		TrainingPhase{
			.name         = "A3-Aperfeicoamento",
			.policyLR     = 0.8e-4f,
			.criticLR     = 0.8e-4f,
			.entropyScale = 0.025f,
			.epochs       = 2,
			.gaeGamma     = 0.997f,

			.rewardWeights = {
				{ "Air",                  0.5f  },
				{ "AirAlignment",         1.0f  },
				{ "VelocityPlayerToBall", 0.5f  },
				{ "TouchAccel",           2.0f  },
				{ "VelocityBallToGoal",   2.0f  },
				{ "GoalBonus",         1000.0f  },
				{ "WallLaunch",           1.0f  },
				{ "ConstantPenalty",     -0.02f },
			},
			.stochastic   = true,
			.stateWeights = {
				{ "StaticAerial", 0.1f },
				{ "Cross",        0.5f },
				{ "WallDrag",     0.3f },
				{ "Random",       0.1f },
			},
		},
		*/
	};

	return cfg;
}
