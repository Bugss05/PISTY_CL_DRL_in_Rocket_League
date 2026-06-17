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
        { new ZeroSumReward(new GoalReward(-1, 1.2f, 2.5f), 0.0f, 1.0f),        40.0f }, // GoalReward(concedeScale, speedScale, heightScale): golo escalado pela velocidade e ALTURA de entrada
        { new ZeroSumReward(new VelocityBallToGoalReward(false), 0.0f, 0.8f),   3.0f }, // bola->golo = progresso ofensivo
        { new TouchAccelReward(),                                               1.5f }, // toca na bola! (sinal dominante)
        { new StrongTouchReward(30,100),                                        0.85f }, // StrongTouchReward(minSpeedKPH, maxSpeedKPH): powershot
        { new VelocityPlayerToBallReward(),                                     0.25f },
        { new TouchBallReward(),                                                0.05f },
        { new FaceBallReward(),                                                 0.3f },
        { new AirReward(300.f, 0.3f, 3.0f, 0.3f),                               1.0f }, // AirReward(heightThresh, lowScale, noTouchTime, noTouchScale): ar por altura; decai após 3s sem tocar
        { new WallLaunchReward(),                                               0.2f },
        { new SpeedReward(),                                                    0.1f }, // ter velocidade
        // --- Mecânica aérea (novas) ---

        { new AirTouchReward(0.3f, 500.f),                                      10.0f }, // AirTouchReward(minAirTime, minBallHeight): toque aéreo com tempo+altura mínimos
        { new DoubleJumpBoostReward(25.f, 350.f),                              0.25f }, // DoubleJumpBoostReward(belowTolerance, minBallHeight): double jump + boost p/ aéreo
        { new FlickReward(1.0f, 2.0f, 0.05f, 0.65f),                             8.5f }, // FlickReward(velScale, heightScale, jumpReward, forwardThresh): flick PARA A FRENTE na bola
        { new FlickTowardsBallReward(0.6f, 0.65f),                               3.5f }, // FlickTowardsBallReward(maxAirTime, forwardThresh): flip rasteiro p/ a frente que acaba mais perto da bola

        // --- Eventos do jogo (preenchidos pelo GameEventTracker) ---
        { new ZeroSumReward(new ShotReward(),0.0f,0.2f),                        2.5f }, // remate à baliza
        { new ZeroSumReward(new SaveReward(),0.0f,0.2f),                        10.0f }, // defesa

        { new WhiffReward(),                                                    -0.25f }, // falhar a bola (erro próprio)
        //{ new BallTouchGroundPenalty(1000.f, 500.f, 2000.f),                    -1.0f }, // BallTouchGroundPenalty(horizThresh, zThresh, zMax): bola a cair (sitter); peso NEGATIVO
        { new ZeroSumReward(new BumpReward(),0.0f,0.5f),                        4.0f }, // bump no adversário
        { new ZeroSumReward(new DemoReward(),0.0f,0.5f),                        3.0f }, // demo no adversário
        { new LandAllFoursReward(),                                             0.8f }, // aterrar nas 4 rodas
        
        { new WavedashReward(),                                                 1.0f }, // wavedash (dodge+land rápido)


    };

    std::vector<TerminalCondition*> terminals = {
        new GoalScoreCondition(),
        new TimeoutCondition(40.f),
    };

    // 1v1: um carro BLUE e um carro ORANGE
    auto arena = Arena::Create(GameMode::SOCCAR);

    MutatorConfig mutator = MutatorConfig(GameMode::SOCCAR);
    mutator.boostUsedPerSecond = 1.0f;
    mutator.carSpawnBoostAmount = 0.0f;
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
        { "Pass", new DefenderState(new PassState(300.f, 1600.f), DefenderState::BEHIND_BALL), 1.0f },

        // Aerial (substitui o StaticAerial): bola a cair de mais alto, com
        // guarda-redes na baliza. "valores mais altos" = 700..1600.
        { "FallingBall", new DefenderState(new FallingBallApproachState(700.f, 1600.f), DefenderState::GOAL), 1.0f },

        // Cruzamento: bola colada à parede a cruzar para a área, guarda-redes na baliza.
        { "Cross", new DefenderState(new CrossState(), DefenderState::GOAL), 1.0f },

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

    // RENDER: imprime em live as ações de cada carro e SALIENTA os flips (dodges).
    if (learner->config.renderMode && !states.empty()) {
        const GameState& gs = states[0]; // arena 0 (render = 1 jogo)
        for (auto& player : gs.players) {
            const Action& a = player.prevAction;
            const char* teamStr = player.team == Team::BLUE ? "BLUE" : "ORANGE";

            // Início de um FLIP (dodge) neste step -> alerta bem visível.
            bool flipStarted = player.prev && player.isFlipping && !player.prev->isFlipping;
            if (flipStarted) {
                const char* dir = player.flipRelTorque.y > 0.5f ? "FRENTE"
                                : player.flipRelTorque.y < -0.5f ? "TRAS" : "LADO";
                printf(">>>>>>>>>>  FLIP! Car %u %s  dir=%s  flipRelTorque.y=%+.2f  <<<<<<<<<<\n",
                    player.carId, teamStr, dir, player.flipRelTorque.y);
            }

            printf("[Car %u %s] thr=%+.1f steer=%+.1f pitch=%+.1f yaw=%+.1f roll=%+.1f jump=%.0f boost=%.0f hb=%.0f %s\n",
                player.carId, teamStr, a.throttle, a.steer, a.pitch, a.yaw, a.roll,
                a.jump, a.boost, a.handbrake, player.isFlipping ? "[FLIPPING]" : "");
        }
    }

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
    cfg.numGames    = 768;

    cfg.ppo.tsPerItr       = 262144;
    cfg.ppo.batchSize      = 262144;  // guia: = tsPerItr
    cfg.ppo.miniBatchSize  = 131072;   // guia: 25k-50k (porção pequena do batch, poupa VRAM/RAM)
    cfg.ppo.epochs         = 2;       // guia: 2-3 (era 1, abaixo do recomendado)
    cfg.ppo.entropyScale   = 0.035f;
    cfg.ppo.gaeGamma       = 0.995f;
    cfg.ppo.policyLR       = 1.35e-4f;
    cfg.ppo.criticLR       = 1.35e-4f;
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
    cfg.ppo.useHalfPrecision = false;

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
        std::string runName = "1v1-selfplay";//"1v1-selfplay"
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
