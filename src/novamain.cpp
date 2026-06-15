/* IMPORTANT!!!!!!!!!!!!!!!!!!!
Just replace this with your original examplemain.cpp file in your GigalearnCPP-Leak/src file
	*/

#include <GigaLearnCPP/Learner.h>
#include "config.h"

#include <ctime>
#include <string>

#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/TerminalConditions/TimeoutCondition.h>
#include <RLGymCPP/ObsBuilders/DefaultObsPadded.h>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/StateSetters/WallDragState.h>
#include <RLGymCPP/StateSetters/PassState.h>
#include <RLGymCPP/StateSetters/FallingBallApproachState.h>
#include <RLGymCPP/StateSetters/CrossState.h>
#include <RLGymCPP/StateSetters/DefenderState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>
#include "SchedulableState.h"

using namespace GGL;
using namespace RLGC;

EnvCreateResult EnvCreateFunc(int index) {
    // Rewards — pesos iniciais 0; o scheduler aplica a Fase 0 na primeira iteração.
    // A ORDEM e os NOMES têm de coincidir com PhaseConfig.h.
    //
    // ZeroSumReward(child, teamSpirit, opponentScale=1):
    //   player_reward = own*(1-ts) + avgTeam*ts - opponentScale*avgOpponent
    //   - teamSpirit = 0 em 1v1 (1 jogador por equipa -> não tem efeito; guia: começar baixo)
    //   - opponentScale = 1.0 -> zero-sum total
    //   - opponentScale < 1.0 -> viés de agressividade (sofrer custa menos do que marcar)
    //
    // Filosofia (rewards.md): só zero-sum se for VANTAJOSO o adversário IMPEDIR.
    //   ZERO-SUM  -> golos, bola->golo, powershots/toques fortes, velocidade.
    //   NÃO zero-sum -> mecânica/movimento/afinação (ar, encarar bola, aerial,
    //                   parede, whiff): zero-sum aqui só adicionaria ruído.
    // ===== PESOS DAS REWARDS — EDITA AQUI À MÃO (sem scheduler) =====
    // Formato: { reward, peso }.
    // ZeroSumReward(child, teamSpirit, opponentScale):
    //   teamSpirit=0 em 1v1; opponentScale=1.0 -> zero-sum total;
    //   opponentScale<1.0 -> viés de agressividade (sofrer custa menos que marcar).
    std::vector<WeightedReward> rewards = {
        // --- ZERO-SUM (o adversário quer impedir) ---
        { new ZeroSumReward(new GoalBonusReward(),               0.0f, 0.8f),  0.0f }, // golo (viés agressivo 0.8)
        { new ZeroSumReward(new VelocityBallToGoalReward(false), 0.0f, 1.0f),  0.0f }, // bola->golo = progresso ofensivo
        { new ZeroSumReward(new TouchAccelReward(),              0.0f, 1.0f), 50.0f }, // toca na bola! (sinal dominante)
        { new ZeroSumReward(new StrongTouchReward(),             0.0f, 1.0f),  0.0f }, // powershot
        { new ZeroSumReward(new VelocityTouchReward(),           0.0f, 1.0f),  0.0f }, // powershot
        // --- NÃO zero-sum (mecânica/movimento/afinação) ---
        { new VelocityPlayerToBallReward(),                                    5.0f }, // mover-se para a bola
        { new FaceBallReward(),                                               1.0f }, // não conduzir de costas
        { new AirReward(),                                                    0.15f }, // NAO ESQUECER DE SALTAR
        { new AerialDistanceReward(),                                         0.0f }, // shaping de aerial
        { new TouchBallAerialReward(),                                        0.0f }, // shaping de aerial
        { new WallLaunchReward(),                                             0.0f }, // mecânica de parede
        // --- ZERO-SUM (velocidade -> adversário quer impedir) ---
        { new ZeroSumReward(new SpeedReward(),                   0.0f, 1.0f),  0.0f }, // ter velocidade
        // --- NÃO zero-sum (penalização própria; peso NEGATIVO para punir) ---
        { new WhiffReward(),                                                  0.0f }, // falhar a bola (erro próprio)
    };

    std::vector<TerminalCondition*> terminals = {
        new GoalScoreCondition(),
        new TimeoutCondition(60.f),
    };

    // 1v1: um carro BLUE e um carro ORANGE
    auto arena = Arena::Create(GameMode::SOCCAR);

    MutatorConfig mutator = MutatorConfig(GameMode::SOCCAR);
    mutator.boostUsedPerSecond = 0.0f;
    mutator.carSpawnBoostAmount = 100.0f;
    arena->SetMutatorConfig(mutator);

    arena->AddCar(Team::BLUE,   CAR_CONFIG_PLANK);
    arena->AddCar(Team::ORANGE, CAR_CONFIG_PLANK);

    // State setters — pesos iniciais definidos aqui; o scheduler sobrepõe por fase.
    // Nomes têm de coincidir EXACTAMENTE com PhaseConfig.h (stateWeights).
    //
    // Cada cenário (Pass/FallingBall/Cross) tem SEMPRE o ORANGE em posição
    // defensiva (DefenderState). WallDrag é mecânica avançada -> só fase final.
    auto stateSetter = new SchedulableState({
        // Sempre activos — base do treino 1v1
        { "Kickoff",  new KickoffState(),                   1.0f },
        { "Random",   new RandomState(true, false, true),   1.0f }, // SEMPRE activo em todas as fases

        // Passe rasteiro/meia-altura: defensor na TRAJETÓRIA da bola, à frente
        // dela e com X variado (não preso à baliza). A bola vai ter com ele.
        { "Pass", new DefenderState(new PassState(300.f, 900.f), DefenderState::BEHIND_BALL), 0.0f },

        // Aerial (substitui o StaticAerial): bola a cair de mais alto, com
        // guarda-redes na baliza. "valores mais altos" = 700..1600.
        { "FallingBall", new DefenderState(new FallingBallApproachState(700.f, 1600.f), DefenderState::GOAL), 0.0f },

        // Cruzamento: bola colada à parede a cruzar para a área, guarda-redes na baliza.
        { "Cross", new DefenderState(new CrossState(), DefenderState::GOAL), 0.0f },

        // Mecânica avançada de parede (wall dribble/launch) — só nas fases finais,
        // requer a reward WallLaunch activa. Drill puro, sem defensor.
        { "WallDrag", new WallDragState(), 0.0f },
    }, /*stochastic*/ true);

    auto obsBuilder   = new DefaultObsPadded(1);
    auto actionParser = new DefaultAction();

    EnvCreateResult result = {};
    result.actionParser       = actionParser;
    result.obsBuilder         = obsBuilder;
    result.stateSetter        = stateSetter;
    result.terminalConditions = terminals;
    result.rewards            = rewards;
    result.arena              = arena;

    return result;
}

void StepCallback(Learner* learner, const std::vector<GameState>& states, Report& report) {

    bool doExpensiveMetrics = (rand() % 4) == 0;

    for (auto& state : states) {
        if (doExpensiveMetrics) {
            for (auto& player : state.players) {
                report.AddAvg("Player/In Air Ratio",      !player.isOnGround);
                report.AddAvg("Player/Ball Touch Ratio",   player.ballTouchedStep);
                report.AddAvg("Player/Demoed Ratio",       player.isDemoed);
                report.AddAvg("Player/Speed",              player.vel.Length());
                Vec dirToBall = (state.ball.pos - player.pos).Normalized();
                report.AddAvg("Player/Speed Towards Ball", RS_MAX(0, player.vel.Dot(dirToBall)));
                report.AddAvg("Player/Boost",              player.boost);
                if (player.ballTouchedStep)
                    report.AddAvg("Player/Touch Height",   state.ball.pos.z);
            }
        }
        if (state.goalScored)
            report.AddAvg("Game/Goal Speed", state.ball.vel.Length());
    }
}

int main(int argc, char* argv[]) {
    RocketSim::Init(CONFIG_COLLISION_MESHES);

    LearnerConfig cfg = {};
    // AMD (9070 XT) NÃO tem CUDA. Este framework só suporta CUDA ou CPU.
    // AUTO -> usa GPU se torch::cuda::is_available() (CUDA/ROCm-Linux), senão CPU.
    // No Windows com AMD, isto corre em CPU.
    cfg.deviceType  = LearnerDeviceType::GPU_CUDA; // ou LearnerDeviceType::CPU para forçar CPU (AMD/Windows)
    cfg.tickSkip    = 8;
    cfg.actionDelay = cfg.tickSkip - 1;
    cfg.numGames    = 576;

    cfg.ppo.tsPerItr       = 262144;
    cfg.ppo.batchSize      = 262144;  // guia: = tsPerItr
    cfg.ppo.miniBatchSize  = 131072;   // guia: 25k-50k (porção pequena do batch, poupa VRAM/RAM)
    cfg.ppo.epochs         = 1;       // guia: 2-3 (era 1, abaixo do recomendado)
    cfg.ppo.entropyScale   = 0.035f;
    cfg.ppo.gaeGamma       = 0.99f;
    cfg.ppo.policyLR       = 2e-4f;
    cfg.ppo.criticLR       = 2e-4f;
    cfg.ppo.sharedHead.layerSizes = { 1024, 1024, 1024 }; // guia: 512-1024 (mais profundo = mais lento, mas melhor)
    cfg.ppo.policy.layerSizes     = { 256, 256, 256 };
    cfg.ppo.critic.layerSizes     = { 256, 256, 256 };

    cfg.ppo.sharedHead.activationType = ModelActivationType::LEAKY_RELU;
    cfg.ppo.policy.activationType     = ModelActivationType::LEAKY_RELU;
    cfg.ppo.critic.activationType     = ModelActivationType::LEAKY_RELU;

    cfg.ppo.sharedHead.addLayerNorm = true;
    cfg.ppo.policy.addLayerNorm     = true;
    cfg.ppo.critic.addLayerNorm     = true;

    // Half-precision só compensa em GPU CUDA; em CPU (AMD/Windows) é mais lento.
    // Volta a true se treinares numa GPU CUDA.
    cfg.ppo.useHalfPrecision = true;

    // ELO + self-play contra versoes anteriores
    cfg.skillTracker.enabled        = true;
    cfg.skillTracker.numArenas      = 8;
    cfg.skillTracker.simTime        = 45;
    cfg.skillTracker.updateInterval = 16;
    cfg.skillTracker.ratingInc      = 5;
    cfg.skillTracker.initialRating  = 0;
    cfg.trainAgainstOldVersions     = true; // treina contra snapshots antigos (league), não só mirror

    bool renderMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--render") {
            renderMode = true;
            break;
        }
    }

    if (renderMode) {
        cfg.renderMode        = true;
        cfg.renderTimeScale   = 1.0f;
        cfg.sendMetrics       = false;
        cfg.numGames          = 1;
        cfg.ppo.deterministic = true;
        cfg.skillTracker.enabled = false;
    } else {
        cfg.sendMetrics = true;
    }

    cfg.randomSeed = 123;

    {
        std::string runName = "1v1-selfplay";
        std::time_t t = std::time(nullptr);
        std::tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        char stamp[32];
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%Hh", &tm_buf);
        cfg.metricsRunName = runName + "_" + stamp;
    }

    Learner* learner = new Learner(EnvCreateFunc, cfg, StepCallback);
    learner->Start();

    return EXIT_SUCCESS;
}
