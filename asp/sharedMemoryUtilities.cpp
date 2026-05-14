//////////////////////////////////////////////////////////////////////////////////////////

#include "sharedMemoryUtilities.h"
#include <sys/shm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

//////////////////////////////////////////////////////////////////////////////////////////

int initializeSharedMemory(SharedState** out) {
    printf("[shm] Initializing shared memory...\n");
    fflush(stdout);
    
    //removing stale segment if exists from previous crash:
    int old = shmget(shmKey, sizeof(SharedState), 0666);
    if (old >= 0) {
        printf("[shm] Removing stale segment %d\n", old);
        shmctl(old, IPC_RMID, NULL);
    }

    int shmId = shmget(shmKey, sizeof(SharedState), 0666 | IPC_CREAT);
    if (shmId < 0) {
        perror("shmget create failed");
        return -1;
    }
    printf("[shm] Shared memory created with ID %d\n", shmId);
    fflush(stdout);

    SharedState* sharedState = (SharedState*)shmat(shmId, NULL, 0);
    if (sharedState == (SharedState*)-1) {
        perror("shmat failed");
        return -1;
    }
    printf("[shm] Shared memory attached at %p\n", sharedState);
    fflush(stdout);

    memset(sharedState, 0, sizeof(SharedState));
    printf("[shm] Shared memory zeroed\n");
    fflush(stdout);

    //initializing semaphores with error checking:
    printf("[shm] Initializing semaphores...\n");
    fflush(stdout);
    
    if (sem_init(&sharedState->shmLock, 1, 1) != 0) {
        perror("sem_init shmLock");
        return -1;
    }
    if (sem_init(&sharedState->turnSem, 1, 0) != 0) {
        perror("sem_init turnSem");
        return -1;
    }
    if (sem_init(&sharedState->actionSem, 1, 0) != 0) {
        perror("sem_init actionSem");
        return -1;
    }
    if (sem_init(&sharedState->artifactLock, 1, 1) != 0) {
        perror("sem_init artifactLock");
        return -1;
    }
    if (sem_init(&sharedState->logLock, 1, 1) != 0) {
        perror("sem_init logLock");
        return -1;
    }
    if (sem_init(&sharedState->keyboardInput.inputLock, 1, 1) != 0) {
        perror("sem_init inputLock");
        return -1;
    }
    if (sem_init(&sharedState->hipRequest.hipRequestLock, 1, 1) != 0) {
        perror("sem_init hipRequestLock");
        return -1;
    }
    sharedState->hipRequest.requestPause = false;
    
    printf("[shm] Semaphores initialized\n");
    fflush(stdout);

    //initializing all other fields:
    sharedState->keyboardInput.newInput = false;
    sharedState->keyboardInput.isUp = false;
    sharedState->keyboardInput.isDown = false;
    sharedState->keyboardInput.isLeft = false;
    sharedState->keyboardInput.isRight = false;
    sharedState->keyboardInput.isConfirm = false;
    sharedState->keyboardInput.isBack = false;
    sharedState->keyboardInput.isEscape = false;
    sharedState->keyboardInput.menuSelection = 0;
    sharedState->keyboardInput.menuConfirmed = false;
    sharedState->keyboardInput.partySize = 0;
    sharedState->keyboardInput.gameStartRequested = false;
    
    for (int i = 0; i < maxEntities; i++)
        sharedState->keyboardInput.consumed[i] = true;

    sharedState->isMainMenu = true;
    sharedState->isPartySelect = false;
    sharedState->isBattle = false;
    sharedState->isGameOver = false;

    for (int i = 0; i < 3; i++) {
        sharedState->artifacts.holder[i] = noHolder;
        sharedState->artifacts.exists[i] = false;
    }
    sharedState->artifacts.exists[solarCoreArtifact]  = true;
    sharedState->artifacts.exists[lunarBladeArtifact] = true;
    for (int i = 0; i < maxEntities; i++)
        sharedState->artifacts.waitingFor[i] = -1;

    for (int p = 0; p < maxPlayers; p++) {
        for (int i = 0; i < inventorySlots; i++)
            sharedState->players[p].inventory.slots[i] = noWeapon;
        for (int i = 0; i < longTermStorageSlots; i++)
            sharedState->players[p].inventory.longTermStorage[i] = noWeapon;

        sharedState->players[p].inventory.longTermStorageCount = 0;
        
        //initializing entity data with safe defaults:
        sharedState->players[p].entity.maxHp = 0;  
        sharedState->players[p].entity.currentHp = 0;
        sharedState->players[p].entity.id = p;
        snprintf(sharedState->players[p].entity.name, 32, "Player%d", p + 1);
    }

    sharedState->hip2Pid = -1;
    sharedState->activeHipCount = 1;
    sharedState->enemyEnabled = true;
    sharedState->waveNumber = 1;
    sharedState->sequence = 0;
    sharedState->logHead = 0;
    sharedState->logCount = 0;
    sharedState->gameOver = false;
    sharedState->playerWon = false;
    sharedState->gamePaused = false;
    sharedState->shouldExit = false;
    sharedState->returnToMenu = false;
    sharedState->ultimateActive = false;
    sharedState->eclipseRelicSpawned = false;
    sharedState->waveTransition = false;
    sharedState->teleportActive = false;
    sharedState->teleportPlayAttack = false;
    sharedState->attackAnimationInProgress = false;
    sharedState->weaponDropActive = false;
    sharedState->weaponDropResolved = false;
    sharedState->weaponDropAccepted = false;
    sharedState->weaponDropId = noWeapon;
    sharedState->weaponDropOwner = noEntity;
    sharedState->currentTurnEntityId = noEntity;
    sharedState->readyQueueHead = 0;
    sharedState->readyQueueTail = 0;
    sharedState->readyQueueCount = 0;
    for (int i = 0; i < maxEntities; i++) {
        sharedState->readyQueue[i] = noEntity;
        sharedState->inReadyQueue[i] = false;
    }
    sharedState->stunTargetPid = -1;
    sharedState->stunTargetEntityId = noEntity;
    sharedState->gameReady = false;  

    printf("[shm] All fields initialized\n");
    fflush(stdout);
    
    *out = sharedState;
    return shmId;
}

//////////////////////////////////////////////////////////////////////////////////////////

int attachStateToSharedMemory(SharedState** out) {
    
    int shmId = shmget(shmKey, sizeof(SharedState), 0666);
    if (shmId < 0) {
        perror("shmget attach failed");
        return -1;
    }

    SharedState* sharedState = (SharedState*)shmat(shmId, NULL, 0);
    if (sharedState == (SharedState*)-1) {
        perror("shmat attach failed");
        return -1;
    }

    *out = sharedState;
    return shmId;
}

//////////////////////////////////////////////////////////////////////////////////////////

void detachFromSharedMemory(SharedState* sharedState) {
    if (sharedState) 
        shmdt(sharedState);
}

//////////////////////////////////////////////////////////////////////////////////////////

void destroySharedMemory(int shmId, SharedState* sharedState) {
    
    if (sharedState) {
        sem_destroy(&sharedState->shmLock);
        sem_destroy(&sharedState->turnSem);
        sem_destroy(&sharedState->actionSem);
        sem_destroy(&sharedState->artifactLock);
        sem_destroy(&sharedState->logLock);
        sem_destroy(&sharedState->keyboardInput.inputLock);
        sem_destroy(&sharedState->hipRequest.hipRequestLock);
        shmdt(sharedState);
    }

    if (shmId >= 0)
        shmctl(shmId, IPC_RMID, NULL);
}

//////////////////////////////////////////////////////////////////////////////////////////

void logAction(SharedState* sharedState, const char* msg, pid_t pid, pthread_t tid) {
    
    sem_wait(&sharedState->logLock);

    ActionLogEntry* entry = &sharedState->log[sharedState->logHead];
    strncpy(entry->message, msg, 127);
    entry->message[127] = '\0';
    entry->pid = pid;
    entry->threadId = tid;
    entry->timestamp = time(NULL);
    entry->valid = true;

    sharedState->logHead = (sharedState->logHead + 1) % logSize;
    if (sharedState->logCount < logSize) 
        sharedState->logCount++;
    
    sharedState->sequence++;
    sem_post(&sharedState->logLock);
}

//////////////////////////////////////////////////////////////////////////////////////////
