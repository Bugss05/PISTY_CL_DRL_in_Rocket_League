/* IMPORTANT!!!!!!!!!!!!!!!!!!!
Just replace this with your original examplemain.cpp file in your GigalearnCPP-Leak/src file
	*/

#include <GigaLearnCPP/Learner.h>
#include "config.h"

#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/TerminalConditions/TimeoutCondition.h>
#include <RLGymCPP/ObsBuilders/DefaultObsPadded.h>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/StateSetters/GoalShotState.h>
#include <RLGymCPP/StateSetters/AttackerMidfieldState.h>
#include <RLGymCPP/StateSetters/CombinedState.h>
#include <RLGymCPP/StateSetters/AirShotState.h>
#include <RLGymCPP/StateSetters/FallingBallApproachState.h>
#include <RLGymCPP/StateSetters/PassState.h>
#include <RLGymCPP/StateSetters/StaticAerialState.h>
#include <RLGymCPP/StateSetters/CrossState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>
#include <RLGymCPP/StateSetters/WallDragState.h>
//include all of our directories to compile the Gigalearnbot.exe
#include "TrackedStates.h"
#include "SchedulableState.h"
#include "Scheduler.h"



using namespace GGL;

using namespace RLGC;



/* Helper to randomly choose team size (1v1, 2v2, or 3v3)
int GetRandomTeamSize() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dist(0, 2);
    int r = dist(gen);
    return r == 0 ? 1 : (r == 1 ? 2 : 3);
}*/

EnvCreateResult EnvCreateFunc(int index) {
	
    // Nome (usado no SchedulerConfig) + reward + peso inicial. A ordem é fixa em todas as
    // arenas, por isso o Scheduler casa nome->índice de forma robusta (ver g_rewardNames).
    struct NamedReward { std::string name; Reward* reward; float weight; };
    std::vector<NamedReward> namedRewards = {
        { "VelocityPlayerToBall", new VelocityPlayerToBallReward(),          0.5f },
        { "ConstantPenalty",      new ConstantReward(),                     -0.3f }, // Penalidade constante fixa por tick vivido
        { "BallTouchGround",      new BallTouchGroundPenalty(-10.0f),        0.01f }, // Penalidade brusca ao tocar a bola no chão
        { "VelocityBallToGoal",   new VelocityBallToGoalReward(false),       0.8f },
        { "Goal",                 new ZeroSumReward(new GoalReward(), 1.0f, 1.0f), 100.0f },
        { "TouchBallAerial",      new TouchBallAerialReward(),               3.0f }, // 3x built-in aerial multiplier → ground=3, aéreo=9
        { "AirAlignment",         new AirAlignmentReward(),                  0.4f }, // trajetória eficiente até à bola
        { "AerialDistance",       new AerialDistanceReward(),                0.4f }, // altura do contacto
        { "Air",                  new AirReward(),                           0.05f },
    };

    std::vector<WeightedReward> rewards;
    std::vector<std::string> rewardNames;
    for (auto& nr : namedRewards) {
        rewards.push_back({ nr.reward, nr.weight });
        rewardNames.push_back(nr.name);
    }
    RegisterRewardNames(rewardNames); // regista uma vez (call_once); o Scheduler usa para o matching



	std::vector<TerminalCondition*> terminalConditions = {
		new NoTouchCondition(2),
		new GoalScoreCondition(),
		new TimeoutCondition(60.0f) // Termina/reseta se o jogo rolar por 1 minuto (60 segs) sem golo
	};


    // Just 1 player training the shot
    int playersPerTeam = 1;
    auto arena = Arena::Create(GameMode::SOCCAR);

    MutatorConfig mutator = MutatorConfig(GameMode::SOCCAR);
    mutator.boostUsedPerSecond = 0.0f;
    mutator.carSpawnBoostAmount = 100.0f;
    arena->SetMutatorConfig(mutator);
    
    // Auto add a car to BLUE only, so only 1 car exists in the arena
    arena->AddCar(Team::BLUE, CAR_CONFIG_PLANK);

    // SchedulableState: as labels ("AirShot", "WallDrag", ...) são o que referencias em
    // SchedulerConfig.h (stateWeights). O Scheduler muda estes pesos por fase em runtime.
    SchedulableState* combinedSetter = new SchedulableState({
        { "AirShot",      new AirShotState(),                    0.0f }, // Foco principal em remates aéreos!
        { "GoalShot",     new TrackedGoalShotState(),            0.0f },
        { "Random",       new TrackedRandomState(true,false,true), 0.0f },
        { "FallingBall",  new TrackedFallingBallApproachState(), 0.0f }, // Bola a cair no meio campo adversário, carro no chão
        { "Pass",         new PassState(),                       0.0f }, // Passe: bola a meia altura com vel horizontal, carro aleatório
        { "Kickoff",      new KickoffState(),                    0.0f },
        { "StaticAerial", new StaticAerialState(),               0.0f },
        { "Cross",        new CrossState(),                      0.0f },
        { "WallDrag",     new WallDragState(),                   1.0f },
    }, /*stochastic*/ true); // pesos iniciais; o Scheduler sobrepõe-se conforme a fase

    // Padded observation builder for up to 3 players per team, if your just training 1v1, just use advanced obs and only train ones, just remove the state setter
    auto obsBuilder = new DefaultObsPadded(1);
    auto actionParser = new DefaultAction();

    EnvCreateResult result = {};
    result.actionParser = actionParser;
    result.obsBuilder = obsBuilder;
    result.stateSetter = combinedSetter;
    result.terminalConditions = terminalConditions;
    result.rewards = rewards;
    result.arena = arena;

    return result;
}

extern std::atomic<int> g_totalGoals;

// Scheduler do currículo — aplica os parâmetros da fase atual (PPO + rewards + state setters).
Scheduler g_scheduler;

void StepCallback(Learner* learner, const std::vector<GameState>& states, Report& report) {

    g_scheduler.Update(learner); // barato e idempotente: corre 1x por iteração

    bool doExpensiveMetrics = (rand() % 4) == 0;
    int fbSteps = 0, fbGoals = 0;

    for (auto& state : states) {
        if (state.goalScored) {
            if (IsArenaGoalShot(state.lastArena))
                g_totalGoals++;
        }

        if (IsArenaFallingBall(state.lastArena)) {
            fbSteps++;
            if (state.goalScored) fbGoals++;
        }

        if (doExpensiveMetrics) {
            for (auto& player : state.players) {
                report.AddAvg("Player/In Air Ratio", !player.isOnGround);
                report.AddAvg("Player/Ball Touch Ratio", player.ballTouchedStep);
                report.AddAvg("Player/Demoed Ratio", player.isDemoed);
                report.AddAvg("Player/Speed", player.vel.Length());
                Vec dirToBall = (state.ball.pos - player.pos).Normalized();
                report.AddAvg("Player/Speed Towards Ball", RS_MAX(0, player.vel.Dot(dirToBall)));
                report.AddAvg("Player/Boost", player.boost);
                if (player.ballTouchedStep)
                    report.AddAvg("Player/Touch Height", state.ball.pos.z);
            }
        }
        if (state.goalScored)
            report.AddAvg("Game/Goal Speed", state.ball.vel.Length());
    }

    // Uma vez por iteração PPO (262144 steps totais)
    g_fallingBallGoals += fbGoals;
    int totalSteps = (g_fallingBallSteps += fbSteps);
    if (totalSteps >= 262144) {
        int actual = g_fallingBallSteps.exchange(0);
        if (actual >= 262144) {
            int goals = g_fallingBallGoals.exchange(0);
            printf("[FallingBall] %d golos em %d steps | %.1f%% win rate\n",
                   goals, actual, (float)goals / actual * 100.f);
        }
    }
}

int main(int argc, char* argv[]) {
    RocketSim::Init(CONFIG_COLLISION_MESHES);

    LearnerConfig cfg = {};
    cfg.deviceType = LearnerDeviceType::GPU_CUDA;
    cfg.tickSkip = 8; //tick skip, if you change this you should change gamma
    cfg.actionDelay = cfg.tickSkip - 1;
    cfg.numGames = 576; // inference ≈ env_step: ajustar com formula numGames*(envStep/inferenceTime)

    cfg.ppo.tsPerItr = 262144;
    cfg.ppo.batchSize = 262144;
    cfg.ppo.miniBatchSize = 131072; // 262144 / 131072 = 2 — divisivel; 9070 XT tem 16GB VRAM
    cfg.ppo.epochs = 1; // usa cada batch de dados duas vezes — mais aprendizagem por iteracao
    cfg.ppo.entropyScale = 0.035f; //this is a good starting point, lower it if your bot is like very good, or you just want to refine what it already knows                              
    cfg.ppo.gaeGamma = 0.99f; //start with .99, then up to .993 once it can hit ball and shoot ball on net, then .995 once it learns dribbles and powershots, .997 once it gets better than necto.
    cfg.ppo.policyLR = 2e-4f;
    cfg.ppo.criticLR = 2e-4f; //learning rates. start out high, then lower to 1.5e-4 when it learns to shoot and touch ball, then 1e-4 once it learns dribbles, then 0.8e-4 once it is around nexto level.
    cfg.ppo.sharedHead.layerSizes = { 1024,1024, 512, 512 }; // power-of-2 → kernels GPU ideais; 768 = 3×256, desalinhado, throughput pior
    cfg.ppo.policy.layerSizes = { 256, 256, 256 };
    cfg.ppo.critic.layerSizes = { 256, 256, 256 }; //these are pretty good, DO NOT INCREASE IT FURTHER

    cfg.ppo.sharedHead.activationType = ModelActivationType::LEAKY_RELU; //leakly relu prevent nuerons from going dead, but slows down training a bit
    cfg.ppo.policy.activationType = ModelActivationType::LEAKY_RELU; 
    cfg.ppo.critic.activationType = ModelActivationType::LEAKY_RELU;

    cfg.ppo.sharedHead.addLayerNorm = true; // if you decide not to use sharedhead(why would you not?) set this to false
    cfg.ppo.policy.addLayerNorm = true; // dont touch
    cfg.ppo.critic.addLayerNorm = true; // dont touch

    cfg.ppo.useHalfPrecision = true; // FP16 inference — 9070 XT (RDNA4) tem 2x throughput em FP16 vs FP32, treino continua em FP32

    cfg.skillTracker.enabled = false; // Desativado para parar os test matches
    cfg.skillTracker.numArenas = 8;
    cfg.skillTracker.simTime = 45;
    cfg.skillTracker.updateInterval = 16;
    cfg.skillTracker.ratingInc = 5;
    cfg.skillTracker.initialRating = 0;


    bool renderMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--render") { //render tag, you use it like Gigalearnbot.exe --render
            renderMode = true;
            break;
        }
    }

    if (renderMode) { //render stuff
        cfg.renderMode = true;
        cfg.renderTimeScale = 1.0f;
        cfg.sendMetrics = false;
        cfg.numGames = 1;
        cfg.ppo.deterministic = true;
        cfg.skillTracker.enabled = false;
    } else {
        cfg.sendMetrics = true; //whether to send metrics 
    }

    cfg.randomSeed = 123; // use -1 for random seed for viewing, this doesnt matter tho

    Learner* learner = new Learner(EnvCreateFunc, cfg, StepCallback);
    learner->Start(); // START LEARNING BOYYYSSS

    return EXIT_SUCCESS;
}