#pragma once
#include "SimpleScheduler.h"

// ===========================================================================
// EDITA AQUI — currículo de treino por timesteps
//
// NOMES DAS REWARDS (ordem = novamain.cpp):
//   "GoalBonus", "VelocityBallToGoal",
//   "TouchAccel", "StrongTouch", "VelocityTouch",
//   "VelocityPlayerToBall", "FaceBall",
//   "Air", "AerialDistance", "TouchBallAerial", "WallLaunch",
//   "Speed", "Whiff"
//
// NOMES DOS STATE SETTERS (ordem = novamain.cpp):
//   "Kickoff", "Random"  (SEMPRE activos)
//   "Pass"        — passe vs defensor na trajetória da bola (X variado, fora da baliza)
//   "FallingBall" — aerial (bola a cair de alto) vs guarda-redes na baliza
//   "Cross"       — cruzamento da parede para a área vs guarda-redes
//   "WallDrag"    — mecânica avançada de parede (só fases finais, precisa de WallLaunch)
//
// Progressão: tocar -> marcar/passar no chão -> aerials -> cruzamentos + parede
// ===========================================================================
inline std::vector<Phase> GetPhases() {
    return {

        // ====================================================================
        // FASE 0 — Bronze/Silver: Tocar na bola. Não esquecer de saltar.
        // 0 – 200M steps
        //
        // O bot aprende a perseguir e TOCAR na bola. Mais nada.
        // - TouchAccel domina: grande recompensa por tocar.
        // - GoalBonus pequeno mas presente — marcar é sempre o objetivo.
        // - Air=0.15 OBRIGATÓRIO — sem isto o bot para de saltar.
        // - Só Kickoff + Random. O ambiente 1v1 já força competição.
        // ====================================================================
        {
            "Bronze-Tocar",
            /* startTimesteps */ 0,
            /* rewardWeights  */ {
                // Guia (early stage): SÓ tocar na bola + não esquecer de saltar.
                // SEM rewards de golo nem bola->golo — só adicionam ruído agora.
                { "GoalBonus",            0.0f  },  // guia: nada de golo no early stage
                { "VelocityBallToGoal",   0.0f  },  // guia: nada de bola->golo no early stage
                { "TouchAccel",           50.0f },  // recompensa GIGANTE por tocar (domina tudo)
                { "StrongTouch",          0.0f  },  // força do toque é middle-stage
                { "VelocityTouch",        0.0f  },
                { "VelocityPlayerToBall", 5.0f  },  // mover-se para a bola
                { "FaceBall",             1.0f  },  // não conduzir de costas para a bola
                { "Air",                  0.15f },  // NAO ESQUECER DE SALTAR
                { "AerialDistance",       0.0f  },
                { "TouchBallAerial",      0.0f  },
                { "WallLaunch",           0.0f  },
                { "Speed",                0.0f  },
                { "Whiff",                0.0f  },
            },
            /* stateWeights */ {
                { "Kickoff",     1.0f },
                { "Random",      1.0f },
                { "Pass",        0.0f },
                { "FallingBall", 0.0f },
                { "Cross",       0.0f },
                { "WallDrag",    0.0f },
            },
        },

        // ====================================================================
        // FASE 1 — Gold: Marcar golos + jogo de chão contra defensor
        // 300M – 500M steps
        //
        // Bot já toca com frequência. Introduz GoalBonus moderado (5, não 100+).
        // VelocityBallToGoal > VelocityPlayerToBall (guia: "a fair bit stronger").
        // Introduz o Pass: bola a meia-altura/rasteira com um DEFENSOR na sua
        // trajetória — o bot aprende a marcar contra alguém posicionado.
        // Ainda sem aerials, cruzamentos ou parede.
        // ====================================================================
        {
            "Gold-GoloEPasse",
            /* startTimesteps */ 400'000'000,
            /* rewardWeights  */ {
                // Guia (learning to score): baixar MUITO o touch, golo MODERADO (~20, nunca 100),
                // VelocityBallToGoal "a fair bit stronger" que VelocityPlayerToBall.
                { "GoalBonus",            20.0f },  // guia: ~20 é razoável; 100 afoga tudo
                { "VelocityBallToGoal",   5.0f  },  // bem mais forte que VelocityPlayerToBall
                { "TouchAccel",           5.0f  },  // reduzido (era dominante na fase 0)
                { "StrongTouch",          4.0f  },  // começa a premiar powershots
                { "VelocityTouch",        0.0f  },
                { "VelocityPlayerToBall", 1.0f  },  // mover para a bola, já secundário
                { "FaceBall",             0.1f  },
                { "Air",                  0.3f  },  // manter hábito de saltar
                { "AerialDistance",       0.0f  },
                { "TouchBallAerial",      2.0f  },  // baby aerials começam a aparecer
                { "WallLaunch",           0.0f  },
                { "Speed",                0.1f  },
                { "Whiff",               -0.5f  },
            },
            /* stateWeights */ {
                { "Kickoff",     1.0f },
                { "Random",      1.0f },  // sempre activo
                { "Pass",        0.5f },  // marcar contra defensor posicionado
                { "FallingBall", 0.0f },
                { "Cross",       0.0f },
                { "WallDrag",    0.0f },
            },
            // ExampleMain: "lower to 1.5e-4 when it learns to shoot and touch ball"
            // "then up to .993 once it can hit ball and shoot ball on net"
            /* policyLR  */ 1.5e-4f,
            /* criticLR  */ 1.5e-4f,
            /* entropy   */ {},       // mantém 0.035
            /* gaeGamma  */ 0.993f,
        },

        // ====================================================================
        // FASE 2 — Plat: Aerials contra guarda-redes
        // 500M – 1.5B steps
        //
        // Bot já marca golos no chão. Hora dos aerials:
        // - FallingBall (700-1600 UU): bola a cair de alto, com guarda-redes
        //   na baliza. Substitui o antigo StaticAerial ("valores mais altos").
        // - Pass continua, agora a peso máximo.
        // AerialDistance/TouchBallAerial entram para premiar bons aerials.
        // LR começa a descer (guia: "decrease LR to around 1e-4 now").
        // ====================================================================
        {
            "Plat-Aerials",
            /* startTimesteps */ 900'000'000,
            /* rewardWeights  */ {
                // Guia (middle stage): golo moderado, shaping de aerial forte, resto a descer.
                { "GoalBonus",            20.0f },  // mantém moderado
                { "VelocityBallToGoal",   5.0f  },
                { "TouchAccel",           2.0f  },
                { "StrongTouch",          3.0f  },  // powershots (zero-sum: adversário quer impedir)
                { "VelocityTouch",        0.0f  },
                { "VelocityPlayerToBall", 0.3f  },
                { "FaceBall",             0.05f },
                { "Air",                  0.5f  },
                { "AerialDistance",       5.0f  },  // aerial bem executado
                { "TouchBallAerial",      8.0f  },  // toque no ar = bom
                { "WallLaunch",           0.0f  },  // parede só na fase seguinte
                { "Speed",                0.05f },
                { "Whiff",               -0.1f  },
            },
            /* stateWeights */ {
                { "Kickoff",     0.5f },
                { "Random",      1.0f },  // sempre activo
                { "Pass",        0.4f },  // peso máximo
                { "FallingBall", 1.0f },  // aerial vs guarda-redes
                { "Cross",       0.5f },
                { "WallDrag",    0.0f },
            },
            // ExampleMain: "then 1e-4 once it learns dribbles"
            // "then .995 once it learns dribbles and powershots"
            /* policyLR     */ 1e-4f,
            /* criticLR     */ 1e-4f,
            /* entropyScale */ 0.025f,  // ligeiramente abaixo do default
            /* gaeGamma     */ 0.995f,
        },

        // ====================================================================
        // FASE 3 — Diamond: Cruzamentos + mecânica de parede + jogo completo
        // 1.5B+ steps
        //
        // Bot tem aerial e marca contra defensor. Agora o jogo completo:
        // - Cross: cruzamentos da parede para a área, com guarda-redes.
        // - WallDrag: mecânica AVANÇADA de parede. Só agora, com WallLaunch
        //   activo (precisa de reward específica para fazer sentido).
        // Pass e FallingBall mantêm-se a peso alto.
        // Shaping rewards ao mínimo — confia no GoalBonus (10).
        // LR: 0.8e-4, entropia: 0.015 (refinar, não explorar), gamma: 0.997.
        // ====================================================================
        {
            "Diamond-CrossEParede",
            /* startTimesteps */ 2'000'000'000,
            /* rewardWeights  */ {
                // Guia (later stage): confia no golo moderado + zero-sum, shaping ao mínimo.
                { "GoalBonus",            20.0f },  // nunca massivo, mesmo no fim
                { "VelocityBallToGoal",   3.0f  },
                { "TouchAccel",           1.0f  },
                { "StrongTouch",          3.0f  },
                { "VelocityTouch",        0.0f  },
                { "VelocityPlayerToBall", 0.1f  },
                { "FaceBall",             0.0f  },
                { "Air",                  0.3f  },
                { "AerialDistance",       3.0f  },
                { "TouchBallAerial",      3.0f  },  // aerial shot bem executado
                { "WallLaunch",           2.0f  },  // mecânica de parede agora premiada
                { "Speed",                0.02f },
                { "Whiff",               -1.0f  },
            },
            /* stateWeights */ {
                { "Kickoff",     0.5f },
                { "Random",      1.0f },  // sempre activo
                { "Pass",        1.0f },
                { "FallingBall", 1.0f },
                { "Cross",       0.7f },  // cruzamentos vs guarda-redes
                { "WallDrag",    0.5f },  // mecânica avançada de parede
            },
            // ExampleMain: "then 0.8e-4 once it is around nexto level"
            // "then .997 once it gets better than necto"
            /* policyLR     */ 0.8e-4f,
            /* criticLR     */ 0.8e-4f,
            /* entropyScale */ 0.015f,  // bot já bom, refinar em vez de explorar
            /* gaeGamma     */ 0.997f,
        },
    };
}
