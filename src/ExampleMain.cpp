/* IMPORTANT!!!!!!!!!!!!!!!!!!!
Just replace this with your original examplemain.cpp file in your GigalearnCPP-Leak/src file
	*/

#include <GigaLearnCPP/Learner.h>

#include <RLGymCPP/Rewards/CommonRewards.h>
#include <RLGymCPP/Rewards/ZeroSumReward.h>
#include <RLGymCPP/TerminalConditions/NoTouchCondition.h>
#include <RLGymCPP/TerminalConditions/GoalScoreCondition.h>
#include <RLGymCPP/ObsBuilders/DefaultObsPadded.h>
#include <RLGymCPP/StateSetters/KickoffState.h>
#include <RLGymCPP/StateSetters/RandomState.h>
#include <RLGymCPP/StateSetters/GoalShotState.h>
#include <RLGymCPP/StateSetters/CombinedState.h>
#include <RLGymCPP/ActionParsers/DefaultAction.h>
//include all of our directories to compile the Gigalearnbot.exe
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
	
	std::vector<WeightedReward> rewards = {

        /*
        // These are ok rewards that will produce a scoring bot in ~100m steps

        // Movement
        { new AirReward(), 0.25f },

        // Player-ball
        { new FaceBallReward(), 0.25f },
        { new VelocityPlayerToBallReward(), 4.f },
        { new StrongTouchReward(20, 100), 60 },

        // Ball-goal
        { new ZeroSumReward(new VelocityBallToGoalReward(), 1), 2.0f },

        // Boost
        { new PickupBoostReward(), 10.f },
        { new SaveBoostReward(), 0.2f },

        // Game events
        { new ZeroSumReward(new BumpReward(), 0.5f), 20 },
        { new ZeroSumReward(new DemoReward(), 0.5f), 80 },
        { new GoalReward(), 150 }

        */

        // Positioning rewarding
        { new FaceBallReward(), 1.0f },                       // Facing the ball
        { new BallBetweenPlayerAndGoalReward(), 1.5f },       // Being in between the ball and the goal
		{ new LandAllFoursReward(), 10.0f },                   // Rewards clean landings to maintain momentum and recovery speed

        // Momemtum rewarding
        { new VelocityPlayerToBallReward(), 3.0f },           // Moving towards the ball
        { new SpeedReward(), 0.5f },                          // Moving at all (to prevent camping)
        { new WavedashReward(), 5.0f },                       // Wavedashing (ground movement technique that is faster than rolling)
        
        // Touch rewarding
        { new StrongTouchReward(30, 120), 50.0f },            // Hulk Mode on the ball
        { new TouchBallReward(), 10.0f },                     // Reward for touching the ball (Includes x3 multiplier for high aerials under the hood)
        { new TouchAccelReward(), 30.0f },                    // Reward for accelerating the ball

        // Boost rewarding
        // Boost stealing is zero-sum. You get it, opponents lose an opportunity.
        { new ZeroSumReward(new PickupBoostReward(), 0.0f, 1.0f), 10.0f },              // Picking up boost
        { new SaveBoostReward(), 0.1f },                                                // Saving boost

        // Score & Defensive Rewarding 
        // We use ZeroSumReward(..., 1.0f, 1.0f) so the whole team benefits/suffers equally.
        // Positive behavior automatically punishes the enemy team, and vice-versa!
        { new ZeroSumReward(new VelocityBallToGoalReward(), 1.0f, 1.0f), 20.0f },       // Going towards the goal with the ball
        { new ZeroSumReward(new GoalReward(), 1.0f, 1.0f), 200.0f },                    // Scoring (BAMM)   
        { new ZeroSumReward(new ShotHitsTargetinGoalReward(), 1.0f, 1.0f), 50.0f },     // Na coruja (DOUBLE BAMM)
        { new ZeroSumReward(new SaveGoalReward(), 1.0f, 1.0f), 150.0f },                // Saving a goal (to encourage goal conservation)

        // Aerial & Advanced Mechanics rewarding 
        { new AirReward(), 0.2f },                            // Leave the ground
        { new HeightMatchReward(), 2.0f },                    // Incentivizes matching ball height
        { new WallLaunchReward(), 5.0f },                     // Rewards jumping off the wall to transition into aerial plays
        { new AirDribbleReward(), 15.0f },                    // High reward for chaining consecutive aerial touches (Exponential scaling)
    };

	std::vector<TerminalCondition*> terminalConditions = {
		new NoTouchCondition(10),
		new GoalScoreCondition()
	};


    // Just 1 player training the shot
    int playersPerTeam = 1;
    auto arena = Arena::Create(GameMode::SOCCAR);
    
    // Auto add a car to BLUE only, so only 1 car exists in the arena
    arena->AddCar(Team::BLUE, CAR_CONFIG_PLANK);

    std::vector<std::pair<StateSetter*, float>> weightedSetters = {
        { new GoalShotState(), 1.0f },
    }; //state setters, kickoff and randomstate weights go tune them yourself
    CombinedState* combinedSetter = new CombinedState(weightedSetters);

    // Padded observation builder for up to 3 players per team, if your just training 1v1, just use advanced obs and only train ones, just remove the state setter
    auto obsBuilder = new DefaultObsPadded(3);
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

void StepCallback(Learner* learner, const std::vector<GameState>& states, Report& report) {

    bool doExpensiveMetrics = (rand() % 4) == 0;
    for (auto& state : states) {
        if (state.goalScored) {
            g_totalGoals++; // We track goals for dynamic distance scaling
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
} //them metrics, I dont use metrics tho

int main(int argc, char* argv[]) {
    RocketSim::Init("/home/bugss/Desktop/Robotica/collision_meshes"); //INCLUDE YOUR COLLISION MESHES

    LearnerConfig cfg = {};
    cfg.deviceType = LearnerDeviceType::GPU_CUDA;
    cfg.tickSkip = 8; //tick skip, if you change this you should change gamma
    cfg.actionDelay = cfg.tickSkip - 1;
    cfg.numGames =758; //adjust to how good your cpu is, mine is a i7-12700k and 192 games is optimal for me. The better your cpu is, the more games you should have.

    cfg.ppo.tsPerItr = 262144;  
    cfg.ppo.batchSize = 262144; //how much your bot trains at a time
    cfg.ppo.miniBatchSize = 131072; //minibatch size. if you have small pc, 25k is good, if you have a powerful pc (4070ti or better) 75k might be optimal
    cfg.ppo.epochs = 1; //start out with one epoch, and once your bot gets better increase this to two
    cfg.ppo.entropyScale = 0.035f; //this is a good starting point, lower it if your bot is like very good, or you just want to refine what it already knows                              
    cfg.ppo.gaeGamma = 0.99f; //start with .99, then up to .993 once it can hit ball and shoot ball on net, then .995 once it learns dribbles and powershots, .997 once it gets better than necto.
    cfg.ppo.policyLR = 2e-4f;
    cfg.ppo.criticLR = 2e-4f; //learning rates. start out high, then lower to 1.5e-4 when it learns to shoot and touch ball, then 1e-4 once it learns dribbles, then 0.8e-4 once it is around nexto level.

    cfg.ppo.sharedHead.layerSizes = { 1024, 1024, 1024}; //your bot has a shared head, both the cpu and critic learn from this, this should be big sizes
    cfg.ppo.policy.layerSizes = { 256, 256, 256 };
    cfg.ppo.critic.layerSizes = { 256, 256, 256 }; //these are pretty good, DO NOT INCREASE IT FURTHER

    cfg.ppo.sharedHead.activationType = ModelActivationType::LEAKY_RELU; //leakly relu prevent nuerons from going dead, but slows down training a bit
    cfg.ppo.policy.activationType = ModelActivationType::LEAKY_RELU; 
    cfg.ppo.critic.activationType = ModelActivationType::LEAKY_RELU;

    cfg.ppo.sharedHead.addLayerNorm = true; // if you decide not to use sharedhead(why would you not?) set this to false
    cfg.ppo.policy.addLayerNorm = true; // dont touch
    cfg.ppo.critic.addLayerNorm = true; // dont touch

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