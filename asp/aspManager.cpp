//////////////////////////////////////////////////////////////////////////////////////////
#include "aspManager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

//////////////////////////////////////////////////////////////////////////////////////////
// === ENEMY AI IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

EnemyAI::EnemyAI(SharedState* sharedState, int enemyIndex) {
    this->sharedState = sharedState;
    this->enemyIndex = enemyIndex;
}

int EnemyAI::pickTarget() {
    int minHp = -1;
    int targetIdx = -1;
    for (int i = 0; i < sharedState->playerCount; i++) {
        EntityData* entity = &sharedState->players[i].entity;
        if (!entity->isAlive) 
            continue;
        if (targetIdx == -1 || entity->currentHp < minHp) {
            minHp = entity->currentHp;
            targetIdx = i;
        }
    }
    return targetIdx;
}

int EnemyAI::pickAction(int targetIndex) {
    if (targetIndex < 0) 
        return actionEnemySkip;
    return actionEnemyStrike;
}

//////////////////////////////////////////////////////////////////////////////////////////
// === ENEMY CONTROLLER IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

EnemyController::EnemyController(SharedState* sharedState, int enemyIdx) : ai(sharedState, enemyIdx) {
    this->sharedState = sharedState;
    this->enemyIdx = enemyIdx;
    this->running = false;
}

EnemyController::~EnemyController() {}

bool EnemyController::isMyTurn() {
    return sharedState->enemies[enemyIdx].entity.hasTurn;
}

bool EnemyController::isAlive() {
    return sharedState->enemies[enemyIdx].entity.isAlive;
}

void EnemyController::handleStun() {
    logAction(sharedState, "Enemy stunned - waiting 3s", getpid(), pthread_self());

    sleep(stunDuration);

    sem_wait(&sharedState->shmLock);
    sharedState->enemies[enemyIdx].entity.isStunned = false;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);

    logAction(sharedState, "Enemy stun expired - resuming", getpid(), pthread_self());
}

void EnemyController::animateTargetSearch(int finalTarget) {
    
    if (finalTarget < 0 || finalTarget >= sharedState->playerCount) return;
    
    int playerCount = sharedState->playerCount;
    if (playerCount <= 1) return;

    int steps = 5;          
    int current = 0;
    for (int i = 0; i < steps && !sharedState->shouldExit && !sharedState->returnToMenu; i++) {
        
        sem_wait(&sharedState->shmLock);
        sharedState->enemies[enemyIdx].actionTarget = current;
        sharedState->sequence++;
        sem_post(&sharedState->shmLock);
        usleep(200000); //200ms

        int next = current;
        for (int j = 0; j < playerCount; j++) {
            next = (next + 1) % playerCount;
            if (sharedState->players[next].entity.isAlive) {
                current = next;
                break;
            }
        }
    }
    
    sem_wait(&sharedState->shmLock);
    sharedState->enemies[enemyIdx].actionTarget = finalTarget;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);
}

void EnemyController::submitAction(int actionType, int targetIndex) {
    sem_wait(&sharedState->shmLock);
    
    EnemyData* enemy = &sharedState->enemies[enemyIdx];
    enemy->actionType = actionType;
    enemy->actionTarget = targetIndex;
    enemy->entity.actionReady = true;
    sharedState->sequence++;

    sem_post(&sharedState->shmLock);
    sem_post(&sharedState->actionSem);

    char msg[128];
    snprintf(msg, 128, "E%d action:%d target:%d", enemyIdx, actionType, targetIndex);
    logAction(sharedState, msg, getpid(), pthread_self());
}

void EnemyController::checkGlobalInput() {
    int myIdx = maxPlayers + enemyIdx;
    sem_wait(&sharedState->keyboardInput.inputLock);
    if (sharedState->keyboardInput.newInput && !sharedState->keyboardInput.consumed[myIdx]) {
        sharedState->keyboardInput.consumed[myIdx] = true;
    }
    sem_post(&sharedState->keyboardInput.inputLock);
}

void EnemyController::start() {
    
    running = true;
    logAction(sharedState, "Enemy thread started", getpid(), pthread_self());

    bool loopExit = false, loopReturn = false;
    while (running && !loopExit && !loopReturn) {
        
        sem_wait(&sharedState->shmLock);
        loopExit = sharedState->shouldExit;
        loopReturn = sharedState->returnToMenu;
        bool paused = sharedState->gamePaused;
        bool wave = sharedState->waveTransition;
        sem_post(&sharedState->shmLock);

        if (loopExit || loopReturn) break;

        if (!isMyTurn()) {
            usleep(16000);
            continue;
        }

        if (!isAlive()) {
            sem_wait(&sharedState->shmLock);
            sharedState->enemies[enemyIdx].entity.hasTurn = false;
            sharedState->enemies[enemyIdx].entity.actionReady = true; //marking as done
            sharedState->sequence++;
            sem_post(&sharedState->shmLock);
            sem_post(&sharedState->actionSem);  //notifying Arbiter
            logAction(sharedState, "Enemy turn skipped because entity is dead", getpid(), pthread_self());
            continue;  
        }

        if (sharedState->enemies[enemyIdx].entity.isStunned) {
            sem_wait(&sharedState->shmLock);
            sharedState->enemies[enemyIdx].entity.hasTurn = false;
            sharedState->enemies[enemyIdx].entity.actionReady = false;
            sharedState->sequence++;
            sem_post(&sharedState->shmLock);
            sem_post(&sharedState->actionSem);
            logAction(sharedState, "Enemy skipped turn (stunned)", getpid(), pthread_self());
            continue;
        }

        if (paused || wave) {
            usleep(50000);
            continue;
        }

        //pick action and submit:
        int target = ai.pickTarget();
        int action = ai.pickAction(target);

        //revalidating target before animation (in case it became invalid):
        if (action == actionEnemyStrike && target >= 0) {
            sem_wait(&sharedState->shmLock);
            bool alive = (target < sharedState->playerCount &&  sharedState->players[target].entity.isAlive);
            sem_post(&sharedState->shmLock);
            if (!alive) {
                target = -1;
                action = actionEnemySkip;
            } else {
                animateTargetSearch(target);
            }
        }

        submitAction(action, target);
    }
    logAction(sharedState, "Enemy thread exiting", getpid(), pthread_self());
}

void* EnemyController::threadFunc(void* arg) {
    EnemyController* ec = (EnemyController*)arg;
    ec->start();
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
// === ASP MANAGER IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

ASPManager* ASPManager::instance = nullptr;

ASPManager::ASPManager(int roll) {
    rollNo = roll;
    shmId = -1;
    sharedState = nullptr;
    running = false;
    instance = this;

    for (int i = 0; i < maxEnemies; i++) {
        enemyThreads[i] = 0;
        controllers[i] = nullptr;
    }
}

ASPManager::~ASPManager() {
    cleanup();
}

bool ASPManager::initialize() {
    if (!attachShm()) 
        return false;
    
    setupSignals();
    registerPid();
    return true;
}

bool ASPManager::attachShm() {
    int retries = 10;
    while (retries-- > 0) {
        shmId = attachStateToSharedMemory(&sharedState);
        if (shmId >= 0) 
            return true;
        usleep(200000);
    }
    fprintf(stderr, "[ASP] shm attach failed after retries\n");
    return false;
}

void ASPManager::registerPid() {
    sem_wait(&sharedState->shmLock);
    sharedState->aspPid = getpid();
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);
    logAction(sharedState, "ASP attached", getpid(), 0);
}

void ASPManager::spawnEnemyThreads() {
    int count = sharedState->enemyCount;

    for (int i = 0; i < count; i++) {
        if (enemyThreads[i] != 0) 
            continue;

        controllers[i] = new EnemyController(sharedState, i);

        if (pthread_create(&enemyThreads[i], NULL, EnemyController::threadFunc, controllers[i]) != 0) {
            perror("[ASP] enemy thread create failed");
            enemyThreads[i] = 0;
        } 
        else {
            sem_wait(&sharedState->shmLock);
            sharedState->enemies[i].entity.threadId = enemyThreads[i];
            sharedState->enemies[i].entity.pid = getpid();
            sharedState->sequence++;
            sem_post(&sharedState->shmLock);

            char msg[64];
            snprintf(msg, 64, "ASP spawned thread for enemy %d", i);
            logAction(sharedState, msg, getpid(), (long)enemyThreads[i]);
        }
    }
}

void ASPManager::joinEnemyThreads() {
    for (int i = 0; i < maxEnemies; i++) {
        if (enemyThreads[i] != 0) {
            pthread_join(enemyThreads[i], NULL);
            enemyThreads[i] = 0;
        }
        if (controllers[i]) {
            delete controllers[i];
            controllers[i] = nullptr;
        }
    }
}

void ASPManager::onNewWave() {
    for (int i = 0; i < maxEnemies; i++) {
        if (enemyThreads[i] != 0) {
            pthread_join(enemyThreads[i], NULL);
            enemyThreads[i] = 0;
        }
        if (controllers[i]) {
            delete controllers[i];
            controllers[i] = nullptr;
        }
    }
    spawnEnemyThreads();
    logAction(sharedState, "ASP new wave threads spawned", getpid(), 0);
}

void ASPManager::run() {
    running = true;
    
    printf("[ASP] run() started\n");
    fflush(stdout);
    
    usleep(200000);
    
    printf("[ASP] waiting for enemyCount > 0...\n");
    fflush(stdout);

    int waitCount = 0;
    while (sharedState->enemyCount == 0 && !sharedState->shouldExit && !sharedState->returnToMenu) {
        usleep(100000);
        waitCount++;
        if (waitCount > 100) {
            printf("[ASP] timeout waiting for enemyCount\n");
            break;
        }
    }
    printf("[ASP] enemyCount = %d\n", sharedState->enemyCount);
    fflush(stdout);
    
    printf("[ASP] waiting for player data initialization...\n");
    fflush(stdout);
    while ((sharedState->playerCount == 0 || sharedState->players[0].entity.maxHp == 0) 
    && !sharedState->shouldExit && !sharedState->returnToMenu) {
        usleep(100000);
    }
    printf("[ASP] player data ready, playerCount = %d\n", sharedState->playerCount);
    fflush(stdout);
    
    spawnEnemyThreads();
    
    int lastWave = sharedState->waveNumber;
    while (running && !sharedState->shouldExit && !sharedState->returnToMenu) {
        usleep(100000);
        
        if (sharedState->waveNumber != lastWave &&
            !sharedState->waveTransition) {
            lastWave = sharedState->waveNumber;
            onNewWave();
        }
        
        if (sharedState->gameOver) running = false;
    }
    
    joinEnemyThreads();
    logAction(sharedState, "ASP exiting", getpid(), 0);
}

void ASPManager::cleanup() {
    joinEnemyThreads();
    detachFromSharedMemory(sharedState);
    sharedState = nullptr;
    shmId = -1;
}

void ASPManager::setupSignals() {
    struct sigaction sa = {};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = onSigterm;
    sigaction(SIGTERM, &sa, nullptr);

    sa.sa_handler = onSigusr1;
    sigaction(SIGUSR1, &sa, nullptr);

    sa.sa_handler = onSigusr2;
    sigaction(SIGUSR2, &sa, nullptr);

    sa.sa_handler = onSigalrm;
    sigaction(SIGALRM, &sa, nullptr);
}

void ASPManager::onSigterm(int sig) {
    (void)sig;
    if (instance) 
        instance->running = false;
}

void ASPManager::onSigusr1(int sig) {
    (void)sig;
    if (!instance || !instance->sharedState) return;
    int targetId = instance->sharedState->stunTargetEntityId;

    if (targetId >= maxPlayers && targetId < maxPlayers + maxEnemies) {
        int enemyIdx = targetId - maxPlayers;
        instance->sharedState->enemies[enemyIdx].entity.isStunned = true;
        alarm(stunDuration);
    }
}

void ASPManager::onSigusr2(int sig) {
    (void)sig;
}

void ASPManager::onSigalrm(int sig) {
    (void)sig;
    if (!instance || !instance->sharedState) return;
    
    int targetId = instance->sharedState->stunTargetEntityId;
    if (targetId >= maxPlayers && targetId < maxPlayers + maxEnemies) {
        int enemyIdx = targetId - maxPlayers;
        instance->sharedState->enemies[enemyIdx].entity.isStunned = false;
        instance->sharedState->sequence++;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
