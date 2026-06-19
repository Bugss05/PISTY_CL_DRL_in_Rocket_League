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
#include <RLGymCPP/StateSetters/PassingState.h>
#include <RLGymCPP/StateSetters/CrossingState.h>
#include <RLGymCPP/StateSetters/FallingBallState.h>
#include <RLGymCPP/StateSetters/ShootingState.h>
#include <RLGymCPP/StateSetters/WallDragState.h>
#include <RLGymCPP/StateSetters/CombinedState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>
#include <RLGymCPP/ActionParsers/NoMoveAction.h>

using namespace GGL;
using namespace RLGC;

// Set to TRUE by hand to make the cars NOT move (uses NoMoveAction instead of
// DefaultAction). E.g.: watch the scenario in render with the bots standing still.
bool g_freezeBots = false;

// Set to TRUE for the NIGHTLY BACKUP: every g_nightBackupInterval iterations it copies
// the entire checkpoints folder to run_noite/<iteration>/.
bool g_nightBackup = true;
int  g_nightBackupInterval = 300;

EnvCreateResult EnvCreateFunc(int index) {
    // ====================================================================
    // REWARD WEIGHTS - EDIT BY HAND. Each line format: { reward, weight }.
    //
    // Two WRAPPERS are used several times below. A wrapper wraps another
    // reward (the "child") and transforms the value it returns:
    //
    //   ZeroSumReward(child, teamSpirit, opponentScale)
    //     Makes the reward ZERO-SUM: what one gains, the opponent "loses".
    //     player_reward = own*(1-teamSpirit) + avgTeam*teamSpirit - opponentScale*avgOpponent
    //       - teamSpirit   = fraction shared with teammates. In 1v1 it has no
    //                        effect (1 player per team) -> we use 0.0.
    //       - opponentScale= weight of the punishment for what the opponent gains.
    //                        1.0 -> full zero-sum; <1.0 -> aggressiveness bias
    //                        (conceding costs less than scoring).
    //     Philosophy (rewards.md): zero-sum is only worth it when it is ADVANTAGEOUS
    //     for the opponent to PREVENT it (goals, ball->goal, powershots, bumps/demos, shot/save).
    //     Personal mechanics/tuning (air, facing the ball, aerial, wall, whiff) does
    //     NOT get zero-sum - there it would only add noise.
    //
    //   GoalDirectionReward(child, onlyAttackHalf, checkHeight)
    //     DIRECTIONAL filter: only lets the child's reward through when the touch sends
    //     the ball TOWARD the opponent's GOAL, scaling by the alignment to the goal.
    //       - onlyAttackHalf = true  -> in own half it passes the full reward;
    //                                    only filters in the attacking half.
    //                          false -> requires direction to the goal across the WHOLE field.
    //       - checkHeight    = false -> does NOT penalize going over the crossbar (posts only);
    //                          true  -> also requires passing below the crossbar.
    // ====================================================================
    std::vector<WeightedReward> rewards = {
        // --- ZERO-SUM (the opponent wants to prevent it) ---

        { new ZeroSumReward(new GoalReward(-1, 2.0f, 2.5f), 0.0f, 0.8f),                  70.0f },  // GoalReward(concedeScale, speedScale, heightScale): goal; concedeScale=mult for the one who CONCEDES (-1=symmetric), speedScale=bonus for entry speed, heightScale=bonus for entry height
        { new ZeroSumReward(new VelocityBallToGoalReward(false), 0.0f, 0.8f),             5.0f },  // VelocityBallToGoalReward(ownGoal): ball->goal = offensive progress; ownGoal=false -> measured toward the OPPONENT's goal
        { new GoalDirectionReward(new TouchAccelReward(), true, false),                   1.5f },  // TouchAccelReward() (no args): rewards ACCELERATING the ball on touch. Here filtered toward the goal (onlyAttackHalf=true, checkHeight=false)
        { new GoalDirectionReward(new StrongTouchReward(30,100), true, false),            1.0f },  // StrongTouchReward(minSpeedKPH, maxSpeedKPH): powershot; only counts above minSpeedKPH, saturates at maxSpeedKPH (+height mult). Filtered toward the goal
        { new VelocityPlayerToBallReward(),                                               0.25f },  // VelocityPlayerToBallReward() (no args): moving toward the ball
        { new TouchBallReward(),                                                          0.00f },  // TouchBallReward() (no args): 1 on the step it touches the ball, otherwise 0
        { new FaceBallReward(),                                                           0.15f },  // FaceBallReward(retreatScale=1.0): point the nose at the ball; when moving away it penalizes × retreatScale
        { new AirReward(350.f, 0.3f, 3.5f, 0.3f),                                         0.7f },  // AirReward(heightThresh, lowScale, noTouchTime, noTouchScale): in the air; lowScale below heightThresh, 1.0 above; after noTouchTime s without touching it is × noTouchScale
        { new WallLaunchReward(),                                                         0.2f },  // WallLaunchReward() (no args): jump off the side wall (1.0) and touch the ball right after in the air (2.0)
        { new SpeedReward(),                                                              0.08f },  // SpeedReward() (no args): having speed (|vel| / car max speed)
        // --- Aerial mechanics (new) ---

        { new AirTouchReward(0.3f, 450.f),                                                9.0f },  // AirTouchReward(minAirTime, minBallHeight): GATE - touch the ball while airborne for >= minAirTime s and with the ball above minBallHeight (uu)
        { new DoubleJumpBoostReward(25.f, 425.f),                                         0.40f },  // DoubleJumpBoostReward(belowTolerance, minBallHeight): double jump + boost for aerial; ball up to belowTolerance below the car and above minBallHeight (uu)
        { new GoalDirectionReward(new FlickReward(1.0f, 5.0f, 0.05f, 0.7f), true, false), 10.0f },  // FlickReward(velScale, heightScale, jumpReward, forwardThresh): flick with a FORWARD flip (flipRelTorque.y>forwardThresh); scales with velScale*vel + heightScale*height; jumpReward at the moment of the jump. Filtered toward the goal
        { new ForwardFlipReward(0.7f),                                                    0.7f },  // ForwardFlipReward(forwardThresh): FORWARD flip (not sideways/backwards); only counts if flipRelTorque.y>forwardThresh
        { new AirProximityReward(400.f, 2500.f, 300.f),                                   1.0f },  // AirProximityReward(minBallHeight, maxDist, wallMargin): airborne and near the ball; ball above minBallHeight, proximity up to maxDist, ignored if within wallMargin of a wall (uu)
        { new ConsecutiveAirTouchReward(300.f, 10, 2.0f),                                 8.0f },  // ConsecutiveAirTouchReward(minBallHeight, maxCount, maxGap): consecutive aerial touches above minBallHeight; count up to maxCount × time factor since the previous touch (up to maxGap s)

        // --- Game events (filled in by the GameEventTracker) ---
        { new ZeroSumReward(new ShotReward(),0.0f,0.2f),                                  6.5f },  // ShotReward() (no args): shot on the opponent's goal
        { new ZeroSumReward(new SaveReward(),0.0f,0.4f),                                  7.0f },  // SaveReward() (no args): save of a shot

        { new WhiffReward(),                                                              -0.25f },  // WhiffReward(whiffDist=250): whiffing the ball - got within < whiffDist (uu) and moved away without touching. NEGATIVE WEIGHT (penalty)
        { new WallMissPenalty(300.f),                                                     -4.0f },  // WallMissPenalty(minHeight): ball hits the opponent's wall below the crossbar and outside the goal (last toucher); minHeight = tolerance. NEGATIVE WEIGHT
        //{ new BallTouchGroundPenalty(1000.f, 500.f, 2000.f),                            -1.0f },  // BallTouchGroundPenalty(horizThresh, zThresh, zMax): "sitter" ball falling to the ground; horizThresh=horizontal speed above which it does NOT punish, zThresh/zMax=falling-speed window. NEGATIVE WEIGHT
        { new ZeroSumReward(new BumpReward(),0.0f,0.5f),                                  4.0f }, // BumpReward() (no args): bump the opponent
        { new ZeroSumReward(new DemoReward(),0.0f,0.5f),                                  3.0f }, // DemoReward() (no args): demolish (demo) the opponent
        { new LandAllFoursReward(),                                                       3.5f }, // LandAllFoursReward() (no args): land on all 4 wheels (car upright), scales with alignment to vertical


        { new WavedashReward(),                                                           2.5f }, // WavedashReward() (no args): wavedash - land right after a flip (quick dodge+land)


    };

    std::vector<TerminalCondition*> terminals = {
        new GoalScoreCondition(),
        new TimeoutCondition(30.f),
    };

    // 1v1: one BLUE car and one ORANGE car
    auto arena = Arena::Create(GameMode::SOCCAR);

    MutatorConfig mutator = MutatorConfig(GameMode::SOCCAR);
    mutator.boostUsedPerSecond = 1.0f;
    mutator.carSpawnBoostAmount = 0.0f;
    arena->SetMutatorConfig(mutator);

    arena->AddCar(Team::BLUE,   CAR_CONFIG_PLANK);
    arena->AddCar(Team::ORANGE, CAR_CONFIG_PLANK);

    // ====================================================================
    // STATE SETTERS - initial scenario of each episode. Format: { setter, weight }.
    //
    // CombinedState picks ONE setter per arena reset, at random, weighted by the
    // weights (weight 0 = never picked). There is NO scheduler/phases: each
    // setter's weights and parameters are fixed (the defaults here).
    // In the gameplay scenarios (Passing/Crossing/...) ORANGE ALWAYS spawns in a
    // defensive position, embedded in the setter itself.
    // ====================================================================
    auto stateSetter = new CombinedState({
        // 1v1 training base
        { new KickoffState(),                   0.4f },  // KickoffState() (no args): standard random kickoff (boost forced to 100)
        { new RandomState(true, false, true),   1.0f },  // RandomState(randBallSpeed, randCarSpeed, carsOnGround): here ball with random speed, cars stationary and on the ground

        // Additional scenarios
        { new PassingState(),                   1.0f },  // PassingState() (no args): pass - attacker + defender embedded
        { new CrossingState(),                  3.0f },  // CrossingState(minHeight, maxHeight, minSpeed, maxSpeed): arcing aerial cross; ball's initial Z and horizontal speed (defaults 300/700, 1000/1700)
        { new ShootingState(),                  0.2f },  // ShootingState() (no args): shot-on-goal scenario
        { new FallingBallState(),               1.0f },  // FallingBallState() (no args): ball falling from the sky (aerial) vs goalkeeper
        { new WallDragState(),                  0.0f },  // WallDragState() (no args): advanced wall mechanic - DISABLED (weight 0; raise the weight to enable)
    });

    auto obsBuilder   = new DefaultObsPadded(1);
    ActionParser* actionParser = g_freezeBots
        ? (ActionParser*)new NoMoveAction()   // bots standing still (manual flag)
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

    // NIGHTLY BACKUP (flag g_nightBackup): every g_nightBackupInterval iterations it copies
    // the entire checkpoints folder to run_noite/<iteration>/ (to recover bots if they get worse).
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

    // RENDER: prints each car's actions live and HIGHLIGHTS the flips (dodges).
    if (learner->config.renderMode && !states.empty()) {
        const GameState& gs = states[0]; // arena 0 (render = 1 game)
        for (auto& player : gs.players) {
            const Action& a = player.prevAction;
            const char* teamStr = player.team == Team::BLUE ? "BLUE" : "ORANGE";

            // Start of a FLIP (dodge) this step -> very visible alert.
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
    cfg.deviceType  = LearnerDeviceType::GPU_CUDA; // or LearnerDeviceType::CPU to force CPU (AMD/Windows), or auto for automatic
    cfg.tickSkip    = 8;
    cfg.actionDelay = cfg.tickSkip - 1;
    cfg.numGames    = 768;  // WARNING: more parallel games = more VRAM and more CPU. Lower it if the machine can't handle it.

    cfg.ppo.tsPerItr       = 262144;
    cfg.ppo.batchSize      = 262144;  // WARNING: bigger batch = more VRAM and more CPU per update. Lower it if you run out of memory.
    cfg.ppo.miniBatchSize  = 131072;
    cfg.ppo.epochs         = 2;
    cfg.ppo.entropyScale   = 0.035f;
    cfg.ppo.gaeGamma       = 0.996f;
    cfg.ppo.policyLR       = 1.0e-4f;
    cfg.ppo.criticLR       = 1.0e-4f;
    // ATTENTION: if you are going to CONTINUE OUR bot (load our checkpoint), you MUST
    // keep EXACTLY these network sizes - changing them invalidates the saved weights.
    cfg.ppo.sharedHead.layerSizes = { 1024, 1024, 1024 };
    cfg.ppo.policy.layerSizes     = { 256, 256, 256 };
    cfg.ppo.critic.layerSizes     = { 256, 256, 256 };

    cfg.ppo.sharedHead.activationType = ModelActivationType::LEAKY_RELU;
    cfg.ppo.policy.activationType     = ModelActivationType::LEAKY_RELU;
    cfg.ppo.critic.activationType     = ModelActivationType::LEAKY_RELU;

    cfg.ppo.sharedHead.addLayerNorm = true;
    cfg.ppo.policy.addLayerNorm     = true;
    cfg.ppo.critic.addLayerNorm     = true;

    // Half-precision is only worth it on a CUDA GPU; on CPU (AMD/Windows) it is slower.
    // Set back to true if you train on a CUDA GPU.
    cfg.ppo.useHalfPrecision = false;

    // ELO + self-play against previous versions
    cfg.skillTracker.enabled        = true;
    cfg.skillTracker.numArenas      = 8;
    cfg.skillTracker.simTime        = 45;
    cfg.skillTracker.updateInterval = 16;
    cfg.skillTracker.ratingInc      = 5;
    cfg.skillTracker.initialRating  = 0;
    cfg.trainAgainstOldVersions     = true; // trains against old snapshots (league), not just mirror

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
