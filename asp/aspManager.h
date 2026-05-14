//////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../arbiter/sharedState.h"
#include "../arbiter/sharedMemoryUtilities.h"
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

//////////////////////////////////////////////////////////////////////////////////////////
// === ENEMY AI ===
//////////////////////////////////////////////////////////////////////////////////////////

class EnemyAI {
private:
    SharedState* sharedState;
    int enemyIndex;

public:
    EnemyAI(SharedState* sharedState, int enemyIndex);

    int pickTarget();
    int pickAction(int targetIndex);
};

//////////////////////////////////////////////////////////////////////////////////////////
// === ENEMY CONTROLLER ===
//////////////////////////////////////////////////////////////////////////////////////////

class EnemyController {
private:
    SharedState* sharedState;
    int enemyIdx;
    bool running;
    EnemyAI ai;

    bool isMyTurn();
    bool isAlive();

    void checkGlobalInput();
    void submitAction(int actionType, int targetIndex);
    void handleStun();
    void animateTargetSearch(int finalTarget);

public:
    EnemyController(SharedState* sharedState, int enemyIdx);
    ~EnemyController();

    void start();
    static void* threadFunc(void* arg);
};

//////////////////////////////////////////////////////////////////////////////////////////
// === ASP MANAGER ===
//////////////////////////////////////////////////////////////////////////////////////////

class ASPManager {
private:
    SharedState* sharedState;
    int shmId;
    int rollNo;

    pthread_t enemyThreads[maxEnemies];
    EnemyController* controllers[maxEnemies];
    bool running;

    bool attachShm();
    void registerPid();
    void spawnEnemyThreads();
    void joinEnemyThreads();
    void onNewWave();

    void setupSignals();
    static void onSigterm(int sig);
    static void onSigusr1(int sig);
    static void onSigusr2(int sig);
    static void onSigalrm(int sig);

    static ASPManager* instance;

public:
    ASPManager(int roll);
    ~ASPManager();

    bool initialize();
    void run();
    void cleanup();
};

//////////////////////////////////////////////////////////////////////////////////////////
