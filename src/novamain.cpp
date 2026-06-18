/* IMPORTANT!!!!!!!!!!!!!!!!!!!
Just replace this with your original examplemain.cpp file in your GigalearnCPP-Leak/src file
	*/

#include <GigaLearnCPP/Learner.h>
#include "config.h"

#include <ctime>
#include <string>
#include <filesystem>
#include <cstdio>

#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/Rewards/GoalDirectionReward.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/TerminalConditions/TimeoutCondition.h>
#include <RLGymCPP/ObsBuilders/DefaultObsPadded.h>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/StateSetters/WallDragState.h>
#include <RLGymCPP/StateSetters/PassingState.h>
#include <RLGymCPP/StateSetters/CrossingState.h>
#include <RLGymCPP/StateSetters/FallingBallState.h>
#include <RLGymCPP/StateSetters/ShootingState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>
#include <RLGymCPP/ActionParsers/NoMoveAction.h>
#include "SchedulableState.h"

using namespace GGL;
using namespace RLGC;

// Mete a TRUE à mão para os carros NÃO se mexerem (usa NoMoveAction em vez de
// DefaultAction). Ex.: observar o cenário em render com os bots parados.
bool g_freezeBots = false;

// Mete a TRUE para o BACKUP NOTURNO: a cada g_nightBackupInterval iterações copia a
// pasta inteira de checkpoints para run_noite/<iteracao>/.
bool g_nightBackup = true;
int  g_nightBackupInterval = 150;

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
        { new ZeroSumReward(new GoalReward(-1, 2.0f, 2.5f), 0.0f, 0.8f),        70.0f }, // GoalReward(concedeScale, speedScale, heightScale): golo escalado pela velocidade e ALTURA de entrada
        { new ZeroSumReward(new VelocityBallToGoalReward(false), 0.0f, 0.8f),   5.0f }, // bola->golo = progresso ofensivo
        { new GoalDirectionReward(new TouchAccelReward(), true, false),         1.5f }, // GoalDirectionReward(child, onlyAttackHalf, checkHeight): metade de ataque, NÃO penaliza por cima da barra
        { new GoalDirectionReward(new StrongTouchReward(30,100), true, false),  1.0f }, // idem: powershot p/ a baliza (sem penalizar por cima)
        { new VelocityPlayerToBallReward(),                                     0.25f },
        { new TouchBallReward(),                                                0.00f },
        { new FaceBallReward(),                                                 0.15f },
        { new AirReward(350.f, 0.3f, 3.5f, 0.3f),                               0.7f }, // AirReward(heightThresh, lowScale, noTouchTime, noTouchScale): ar por altura; decai após 3s sem tocar
        { new WallLaunchReward(),                                               0.2f },
        { new SpeedReward(),                                                    0.08f }, // ter velocidade
        // --- Mecânica aérea (novas) ---

        { new AirTouchReward(0.3f, 450.f),                                      9.0f }, // AirTouchReward(minAirTime, minBallHeight): toque aéreo com tempo+altura mínimos
        { new DoubleJumpBoostReward(25.f, 425.f),                              0.40f }, // DoubleJumpBoostReward(belowTolerance, minBallHeight): double jump + boost p/ aéreo
        { new GoalDirectionReward(new FlickReward(1.0f, 5.0f, 0.05f, 0.7f), true, false), 10.0f }, // flick PARA A FRENTE; na metade de ataque só conta se for p/ a baliza
        { new ForwardFlipReward(0.7f),                                            0.7f }, // ForwardFlipReward(forwardThresh): flip PARA A FRENTE (não lateral/trás)
        { new AirProximityReward(400.f, 2500.f, 300.f),                          1.0f }, // AirProximityReward(minBallHeight, maxDist, wallMargin): perto da bola no ar × altura × velocidade
        { new ConsecutiveAirTouchReward(300.f, 10, 2.0f),                         8.0f }, // ConsecutiveAirTouchReward(minBallHeight, maxCount, maxGap): contagem × tempo desde o toque anterior

        // --- Eventos do jogo (preenchidos pelo GameEventTracker) ---
        { new ZeroSumReward(new ShotReward(),0.0f,0.2f),                        6.5f }, // remate à baliza
        { new ZeroSumReward(new SaveReward(),0.0f,0.4f),                        7.0f }, // defesa

        { new WhiffReward(),                                                    -0.25f }, // falhar a bola (erro próprio)
        { new WallMissPenalty(300.f),                                           -4.0f }, // WallMissPenalty(minHeight): bola na parede adversária abaixo da barra, fora da baliza (último a tocar); peso NEGATIVO
        //{ new BallTouchGroundPenalty(1000.f, 500.f, 2000.f),                    -1.0f }, // BallTouchGroundPenalty(horizThresh, zThresh, zMax): bola a cair (sitter); peso NEGATIVO
        { new ZeroSumReward(new BumpReward(),0.0f,0.5f),                        4.0f }, // bump no adversário
        { new ZeroSumReward(new DemoReward(),0.0f,0.5f),                        3.0f }, // demo no adversário
        { new LandAllFoursReward(),                                             3.5f }, // aterrar nas 4 rodas
        
        { new WavedashReward(),                                                 2.5f }, // wavedash (dodge+land rápido)


    }; 

    std::vector<TerminalCondition*> terminals = {
        new GoalScoreCondition(),
        new TimeoutCondition(30.f),
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
        // Base do treino 1v1
        { "Kickoff",  new KickoffState(),                   0.4f },
        { "Random",   new RandomState(true, false, true),   1.0f },

        // Cenários novos (atacante + defensor já incluídos no próprio state setter)
        { "Passing",  new PassingState(),                   1.0f }, // passe
        { "Crossing", new CrossingState(),                  3.0f }, // CrossingState(minHeight, maxHeight, minSpeed, maxSpeed)
        { "Shooting", new ShootingState(),                  0.2f }, // remate
        { "fallingBall", new FallingBallState(),            1.0f }, // bola a cair do céu (sem rebote)
        // Mecânica avançada de parede (wall dribble/launch)
        { "WallDrag", new WallDragState(),                  0.0f },
    }, /*stochastic*/ true);

    auto obsBuilder   = new DefaultObsPadded(1);
    ActionParser* actionParser = g_freezeBots
        ? (ActionParser*)new NoMoveAction()   // bots parados (flag manual)
        : (ActionParser*)new DefaultAction();

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

    // BACKUP NOTURNO (flag g_nightBackup): a cada g_nightBackupInterval iterações copia
    // a pasta inteira de checkpoints para run_noite/<iteracao>/ (recuperar bots se piorarem).
    if (g_nightBackup && !learner->config.renderMode) {
        static uint64_t lastBackupIter = 0;
        uint64_t it = learner->totalIterations;
        if (it > 0 && (it % (uint64_t)g_nightBackupInterval) == 0 && it != lastBackupIter) {
            lastBackupIter = it;
            namespace fs = std::filesystem;
            std::error_code ec;
            fs::path src = learner->config.checkpointFolder; // default "checkpoints"
            if (!src.empty() && fs::exists(src)) {
                fs::path dst = fs::path("run_noite") / std::to_string(it);
                fs::create_directories(dst.parent_path(), ec);
                fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
                if (ec) printf("[run_noite] ERRO ao copiar checkpoints: %s\n", ec.message().c_str());
                else    printf("[run_noite] checkpoints guardados em %s\n", dst.string().c_str());
            }
        }
    }

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
    cfg.ppo.gaeGamma       = 0.996f;
    cfg.ppo.policyLR       = 1.0e-4f;
    cfg.ppo.criticLR       = 1.0e-4f;
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
