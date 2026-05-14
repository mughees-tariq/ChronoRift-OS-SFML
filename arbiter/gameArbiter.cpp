//////////////////////////////////////////////////////////////////////////////////////////
#include "gameArbiter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <math.h>
#include <string>

using namespace sf;

//////////////////////////////////////////////////////////////////////////////////////////
// === ANIMATION IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

Animation::Animation(int numAnim, const char** paths, int spWidth, 
    int* totalWidths, bool** conds, int spHeight) {
    
    numAnimations = numAnim;
    spriteWidth = spWidth;
    spriteHeight = spHeight;
    currentX = 0;
    currentAnimation = 0;
    isOneShot = false;
    finished = false;

    textures = new Texture[numAnimations];
    sprites = new Sprite[numAnimations];
    sheetWidths = new int[numAnimations];
    conditions = new bool*[numAnimations];
    for (int i = 0; i < numAnimations; i++)
        conditions[i] = conds[i];

    for (int i = 0; i < numAnimations; i++) {
        if (!textures[i].loadFromFile(paths[i])) {
            fprintf(stderr, "failed to load: %s\n", paths[i]);
        }
        sheetWidths[i] = totalWidths[i];
        sprites[i].setTexture(textures[i]);
        sprites[i].setTextureRect(IntRect(0, 0, spriteWidth, spriteHeight));
    }
    activeSprite = &sprites[0];
}

Animation::~Animation() {
    delete[] textures;
    delete[] sprites;
    delete[] sheetWidths;
    delete[] conditions;
}

Sprite* Animation::animate(float offsetX, float posX, float posY) {
    if (finished) {
        if (activeSprite)
            activeSprite->setPosition(posX - offsetX, posY);
        return activeSprite;
    }

    if (animClock.getElapsedTime().asSeconds() > 0.25f) {
        animClock.restart();
        for (int i = 0; i < numAnimations; i++) {
            if (!*conditions[i]) continue;

            if (currentAnimation == i) {
                currentX += spriteWidth;

                if (isOneShot && currentX >= sheetWidths[i]) {
                    currentX = sheetWidths[i] - spriteWidth;
                    finished = true;
                } 
                else if (currentX >= sheetWidths[i]) {
                    currentX = 0;
                }
            } 
            else {
                currentAnimation = i;
                currentX = 0;
            }

            activeSprite = &sprites[i];
            activeSprite->setTextureRect(IntRect(currentX, 0, spriteWidth, spriteHeight));
        }
    }
    if (activeSprite)
        activeSprite->setPosition(posX - offsetX, posY);
    return activeSprite;
}

void Animation::setOneShot(bool val) {
    isOneShot = val;
    finished  = false;
}

bool Animation::isFinished() {
    return finished;
}

// void Animation::reset() {
//     currentX = 0;
//     finished = false;
//     animClock.restart();
// }

// int Animation::getCurrentAnimation() {
//     return currentAnimation;
// }

Sprite* Animation::getActiveSprite() {
    return activeSprite;
}

//////////////////////////////////////////////////////////////////////////////////////////

AnimationManager::AnimationManager() {
    count = 0;
    for (int i = 0; i < maxEntries; i++) {
        entries[i].animation = nullptr;
        entries[i].active = false;
        memset(entries[i].tag, 0, 32);
    }
}

AnimationManager::~AnimationManager() {
    for (int i = 0; i < maxEntries; i++) {
        if (entries[i].animation) {
            delete entries[i].animation;
            entries[i].animation = nullptr;
        }
    }
}

int AnimationManager::findEntry(const char* tag) {
    for (int i = 0; i < maxEntries; i++) {
        if (entries[i].active && strncmp(entries[i].tag, tag, 31) == 0)
            return i;
    }
    return -1;
}

void AnimationManager::play(const char* tag, const AnimationDetails& spec, bool useAttack,
    Vector2f pos, bool oneShot, bool** conditions) {
    
    stop(tag);

    int slot = -1;
    for (int i = 0; i < maxEntries; i++) {
        if (!entries[i].active) { 
            slot = i; 
            break; 
        }
    }
    
    if (slot < 0) {
        fprintf(stderr, "no free slots\n");
        return;
    }

    const char* path;
    int frameW, frameH, frameCount;

    if (useAttack) {
        path = spec.attackPath;
        frameW = spec.attackFrameWidth;
        frameH = spec.attackFrameHeight;
        frameCount = spec.attackFrames;
    } 
    else {
        path = spec.idlePath;
        frameW = spec.idleFrameWidth;
        frameH = spec.idleFrameHeight;
        frameCount = spec.idleFrames;
    }

    const char* paths[1] = { path };
    int widths[1] = { frameW * frameCount };

    Animation* anim = new Animation(1, paths, frameW, widths, conditions, frameH);
    anim->setOneShot(oneShot);

    strncpy(entries[slot].tag, tag, 31);
    entries[slot].tag[31] = '\0';
    entries[slot].animation = anim;
    entries[slot].pos = pos;
    entries[slot].active = true;
    entries[slot].isOneShot = oneShot;
}

void AnimationManager::update(float dt) {
    (void)dt;
    for (int i = 0; i < maxEntries; i++) {
        if (!entries[i].active) continue;
        if (entries[i].isOneShot && entries[i].animation->isFinished()) {
            stop(entries[i].tag);
        }
    }
}

void AnimationManager::draw(RenderWindow& window) {
    for (int i = 0; i < maxEntries; i++) {
        if (!entries[i].active || !entries[i].animation) 
            continue;

        Sprite* sprite = entries[i].animation->animate(0.0f, entries[i].pos.x, entries[i].pos.y);
        if (sprite) 
            window.draw(*sprite);
    }
}

bool AnimationManager::isPlaying(const char* tag) {
    return findEntry(tag) >= 0;
}

void AnimationManager::stop(const char* tag) {
    int idx = findEntry(tag);
    if (idx < 0) 
        return;
    
    delete entries[idx].animation;
    entries[idx].animation = nullptr;
    entries[idx].active = false;
}

void AnimationManager::stopAll() {
    for (int i = 0; i < maxEntries; i++) {
        if (!entries[i].active) 
            continue;
        
        delete entries[i].animation;
        entries[i].animation = nullptr;
        entries[i].active = false;
    }
}

void AnimationManager::setPosition(const char* tag, Vector2f pos) {
    int idx = findEntry(tag);
    if (idx < 0) 
        return;
    
    entries[idx].pos = pos;
}

void AnimationManager::setScale(const char* tag, float sx, float sy) {
    int idx = findEntry(tag);
    if (idx < 0 || !entries[idx].animation) 
        return;
    
    Sprite* sprite = entries[idx].animation->getActiveSprite();
    if (sprite) 
        sprite->setScale(sx, sy);
}

//////////////////////////////////////////////////////////////////////////////////////////
// === TURN SCHEDULER IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

TurnScheduler::TurnScheduler(SharedState* sharedState) {
    this->sharedState = sharedState;
}

bool TurnScheduler::isReady(int entityId) {
    if (entityId < maxPlayers) {
        EntityData* entity = &sharedState->players[entityId].entity;
        return entity->currentStamina >= entity->maxStamina;
    }
    int enemyIdx = entityId - maxPlayers;
    EntityData* entity = &sharedState->enemies[enemyIdx].entity;
    return entity->currentStamina >= entity->maxStamina;
}

bool TurnScheduler::isEligible(int entityId) {
    if (entityId < maxPlayers) {
        EntityData* entity = &sharedState->players[entityId].entity;
        return entity->isAlive && !entity->isStunned && !entity->hasTurn;
    }
    
    int enemyIdx = entityId - maxPlayers;
    EntityData* entity = &sharedState->enemies[enemyIdx].entity;
    return entity->isAlive && !entity->isStunned && !entity->hasTurn && !sharedState->ultimateActive;
}

int TurnScheduler::getEntityPid(int entityId) {
    if (entityId < maxPlayers)
        return sharedState->players[entityId].entity.pid;
    
    int enemyIdx = entityId - maxPlayers;
    return sharedState->enemies[enemyIdx].entity.pid;
}

void TurnScheduler::enqueueReady(int entityId) {
    
    if (entityId < 0 || entityId >= maxEntities) return;
    if (sharedState->inReadyQueue[entityId]) return;
    if (sharedState->readyQueueCount >= maxEntities) return;

    sharedState->readyQueue[sharedState->readyQueueTail] = entityId;
    sharedState->readyQueueTail = (sharedState->readyQueueTail + 1) % maxEntities;
    sharedState->readyQueueCount++;
    sharedState->inReadyQueue[entityId] = true;
}

int TurnScheduler::pickNext() {
    sem_wait(&sharedState->shmLock);

    if (sharedState->readyQueueCount == 0) {
        for (int i = 0; i < sharedState->playerCount; i++) {
            if (isEligible(i) && isReady(i))
                enqueueReady(i);
        }
        for (int i = 0; i < sharedState->enemyCount; i++) {
            int entityId = maxPlayers + i;
            if (isEligible(entityId) && isReady(entityId))
                enqueueReady(entityId);
        }
    }

    while (sharedState->readyQueueCount > 0) {
        int entityId = sharedState->readyQueue[sharedState->readyQueueHead];
        sharedState->readyQueue[sharedState->readyQueueHead] = noEntity;
        sharedState->readyQueueHead = (sharedState->readyQueueHead + 1) % maxEntities;
        sharedState->readyQueueCount--;
        if (entityId >= 0 && entityId < maxEntities)
            sharedState->inReadyQueue[entityId] = false;

        if (isEligible(entityId) && isReady(entityId)) {
            sem_post(&sharedState->shmLock);
            return entityId;
        }
    }

    sem_post(&sharedState->shmLock);
    return noEntity;
}

/*
void TurnScheduler::swapPlayerPriority(int playerIndexA, int playerIndexB) {
    if (playerIndexA < 0 || playerIndexA >= sharedState->playerCount) 
        return;
    if (playerIndexB < 0 || playerIndexB >= sharedState->playerCount) 
        return;
    if (playerIndexA == playerIndexB) 
        return;

    sem_wait(&sharedState->shmLock);
    int staminaA = sharedState->players[playerIndexA].entity.currentStamina;
    int staminaB = sharedState->players[playerIndexB].entity.currentStamina;
    sharedState->players[playerIndexA].entity.currentStamina = staminaB;
    sharedState->players[playerIndexB].entity.currentStamina = staminaA;
    sharedState->sequence++;
    
    char msg[128];
    snprintf(msg, 128, "priority swap: player %d <-> player %d", playerIndexA, playerIndexB);
    logAction(sharedState, msg, getpid(), pthread_self());
    sem_post(&sharedState->shmLock);
}

bool TurnScheduler::anyReady() {
    for (int i = 0; i < sharedState->playerCount; i++)
        if (isEligible(i) && isReady(i)) 
            return true;
    
    for (int i = 0; i < sharedState->enemyCount; i++) {
        int entityId = maxPlayers + i;
        if (isEligible(entityId) && isReady(entityId)) 
            return true;
    }
    return false;
}
*/

void TurnScheduler::clearTurnFlags(int entityId) {
    sem_wait(&sharedState->shmLock);
    if (entityId < maxPlayers) {
        sharedState->players[entityId].entity.hasTurn = false;
        sharedState->players[entityId].entity.actionReady = false;
    } 
    else {
        int enemyIdx = entityId - maxPlayers;
        if (enemyIdx < sharedState->enemyCount) {
            sharedState->enemies[enemyIdx].entity.hasTurn = false;
            sharedState->enemies[enemyIdx].entity.actionReady = false;
        }
    }
    if (entityId >= 0 && entityId < maxEntities)
        sharedState->inReadyQueue[entityId] = false;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);
}

//////////////////////////////////////////////////////////////////////////////////////////
// === STAMINA TIMER IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

StaminaTimer::StaminaTimer(SharedState* sharedState) {
    this->sharedState = sharedState;
    running = false;
}

void StaminaTimer::stop() { 
    running = false; 
}

bool StaminaTimer::isRunning() { 
    return running; 
}

void StaminaTimer::enqueueReady(int entityId) {

    if (entityId < 0 || entityId >= maxEntities) return;
    if (sharedState->inReadyQueue[entityId]) return;
    if (sharedState->readyQueueCount >= maxEntities) return;

    sharedState->readyQueue[sharedState->readyQueueTail] = entityId;
    sharedState->readyQueueTail = (sharedState->readyQueueTail + 1) % maxEntities;
    sharedState->readyQueueCount++;
    sharedState->inReadyQueue[entityId] = true;
}

void StaminaTimer::tick() {
    sem_wait(&sharedState->shmLock);

    for (int i = 0; i < sharedState->playerCount; i++) {
        
        EntityData* entity = &sharedState->players[i].entity;
        if (!entity->isAlive || entity->isStunned) continue;

        entity->currentStamina += entity->speed;
        if (entity->currentStamina > entity->maxStamina)
            entity->currentStamina = entity->maxStamina;

        if (entity->currentStamina >= entity->maxStamina && !entity->hasTurn)
            enqueueReady(i);
    }

    for (int i = 0; i < sharedState->enemyCount; i++) {
        EntityData* entity = &sharedState->enemies[i].entity;
        
        if (!entity->isAlive || entity->isStunned) 
            continue;
        
        if (sharedState->ultimateActive) 
            continue;
        
        entity->currentStamina += entity->speed;
        if (entity->currentStamina > entity->maxStamina)
            entity->currentStamina = entity->maxStamina;

        int entityId = maxPlayers + i;
        if (entity->currentStamina >= entity->maxStamina && !entity->hasTurn)
            enqueueReady(entityId);
    }

    sharedState->sequence++;
    sem_post(&sharedState->shmLock);
}

void StaminaTimer::start() {
    running = true;
    logAction(sharedState, "stamina timer started", getpid(), pthread_self());

    while (running && !sharedState->shouldExit && !sharedState->returnToMenu) {
        if (sharedState->gamePaused || sharedState->waveTransition || sharedState->gameOver) {
            for (int i = 0; i < 20 && !sharedState->shouldExit; i++) {
                usleep(50000); //50 ms
                if (!sharedState->gamePaused) break;
            }
            continue;
        }
        sleep(1);
        tick();
    }
    logAction(sharedState, "stamina timer stopped", getpid(), pthread_self());
}

void* StaminaTimer::threadFunc(void* arg) {
    StaminaTimer* timer = (StaminaTimer*)arg;
    timer->start();
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
// === DEADLOCK MONITOR IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

DeadlockMonitor::DeadlockMonitor(SharedState* sharedState, InventoryAllocator* inventoryAllocator) {
    this->sharedState = sharedState;
    this->inventoryAllocator = inventoryAllocator;
    running = false;
}

void DeadlockMonitor::stop() { 
    running = false; 
}

bool DeadlockMonitor::isRunning() { 
    return running; 
}

bool DeadlockMonitor::dfsHasCycle(int entityId, int* visited, int* parent) {
    visited[entityId] = 1;

    int waitingFor = sharedState->artifacts.waitingFor[entityId];
    if (waitingFor == -1) { 
        visited[entityId] = 2; 
        return false; 
    }

    int holder = sharedState->artifacts.holder[waitingFor];
    if (holder == noHolder) { 
        visited[entityId] = 2; 
        return false; 
    }

    int next = holder;
    parent[next] = entityId;

    if (visited[next] == 1) 
        return true;
    
    if (visited[next] == 0) {
        if (dfsHasCycle(next, visited, parent)) 
            return true;
    }

    visited[entityId] = 2;
    return false;
}

int DeadlockMonitor::detectDeadlock() {
    int visited[maxEntities];
    int parent[maxEntities];

    for (int i = 0; i < maxEntities; i++)
        visited[i] = 0;
    
    for (int i = 0; i < maxEntities; i++) 
        parent[i] = noEntity;

    for (int i = 0; i < maxEntities; i++) {
        if (visited[i] != 0) 
            continue;
        
        if (sharedState->artifacts.waitingFor[i] == -1) 
            continue;

        if (dfsHasCycle(i, visited, parent)) {
            for (int j = 0; j < maxEntities; j++) {
                if (visited[j] == 1)
                    return pickVictim(parent, j);
            }
        }
    }
    return noEntity;
}

int DeadlockMonitor::pickVictim(int* parent, int cycleNode) {
    
    bool inCycle[maxEntities] = {};
    int cycleNodes[maxEntities];
    int cycleLen = 0;

    int cur = cycleNode;
    for (int steps = 0; steps < maxEntities; steps++) {
        if (cur == noEntity || inCycle[cur]) break;
        inCycle[cur] = true;
        cycleNodes[cycleLen++] = cur;
        cur = parent[cur];
    }

    int victim = cycleNodes[0];
    pthread_t victimTid = 0;

    for (int i = 0; i < cycleLen; i++) {
        int node = cycleNodes[i];
        pthread_t tid = 0;
        if (node < maxPlayers)
            tid = sharedState->players[node].entity.threadId;
        else {
            int ei = node - maxPlayers;
            if (ei < sharedState->enemyCount)
                tid = sharedState->enemies[ei].entity.threadId;
        }
        if (tid > victimTid) {
            victimTid = tid;
            victim = node;
        }
    }
    return victim;
}

int DeadlockMonitor::getHeldArtifact(int entityId) {
    
    for (int a = 0; a < 3; a++) {
        if (!sharedState->artifacts.exists[a]) 
            continue;
        if (sharedState->artifacts.holder[a] == entityId) 
            return a;
    }
    return -1;
}

int DeadlockMonitor::getEntityPid(int entityId) {
    if (entityId < maxPlayers)
        return sharedState->players[entityId].entity.pid;
    
    int enemyIdx = entityId - maxPlayers;
    if (enemyIdx < sharedState->enemyCount)
        return sharedState->enemies[enemyIdx].entity.pid;
    
    return -1;
}

void DeadlockMonitor::resolveDeadlock(int victimEntityId) {
    sem_wait(&sharedState->artifactLock);

    int victimArtifact = getHeldArtifact(victimEntityId);
    if (victimArtifact == -1) {
        sharedState->artifacts.waitingFor[victimEntityId] = -1;
        sharedState->sequence++;
        sem_post(&sharedState->artifactLock);
        return;
    }
    int winnerEntityId = noHolder;
    pthread_t winnerTid = (pthread_t)-1;

    for (int i = 0; i < maxEntities; i++) {
        if (i == victimEntityId) continue;
        if (sharedState->artifacts.waitingFor[i] != victimArtifact) continue;

        pthread_t tid = 0;
        if (i < maxPlayers)
            tid = sharedState->players[i].entity.threadId;
        else {
            int ei = i - maxPlayers;
            if (ei < sharedState->enemyCount)
                tid = sharedState->enemies[ei].entity.threadId;
        }
        if (winnerEntityId == noHolder || tid < winnerTid) {
            winnerTid = tid;
            winnerEntityId = i;
        }
    }

    sharedState->artifacts.holder[victimArtifact] = (winnerEntityId != noHolder) ? winnerEntityId : noHolder;

    sharedState->artifacts.waitingFor[victimEntityId] = -1;
    if (winnerEntityId != noHolder)
        sharedState->artifacts.waitingFor[winnerEntityId] = -1;

    for (int i = 0; i < maxEntities; i++) {
        if (sharedState->artifacts.waitingFor[i] == victimArtifact)
            sharedState->artifacts.waitingFor[i] = -1;
    }
    sharedState->sequence++;

    char msg[128];
    snprintf(msg, 128, "deadlock resolved: P%d(victim,high tid) lost art%d -> P%d(winner,low tid) now holds both",
             victimEntityId, victimArtifact, winnerEntityId != noHolder ? winnerEntityId : -1);
    logAction(sharedState, msg, getpid(), pthread_self());

    int victimPid = getEntityPid(victimEntityId);
    sem_post(&sharedState->artifactLock);

    if (inventoryAllocator != nullptr && victimEntityId >= 0 && victimEntityId < maxPlayers &&
        winnerEntityId != noHolder && winnerEntityId >= 0 && winnerEntityId < maxPlayers) {

        sem_wait(&sharedState->shmLock);

        InventoryData* victimInv = &sharedState->players[victimEntityId].inventory;
        for (int s = 0; s < inventorySlots; s++) {
            if (victimInv->slots[s] == victimArtifact) {
                int sz = weaponTable[victimArtifact].slotSize;
                for (int k = s; k < s + sz && k < inventorySlots; k++)
                    victimInv->slots[k] = noWeapon;
                break;
            }
        }

        for (int i = 0; i < victimInv->longTermStorageCount; i++) {
            if (victimInv->longTermStorage[i] == victimArtifact) {
                for (int j = i; j < victimInv->longTermStorageCount - 1; j++)
                    victimInv->longTermStorage[j] = victimInv->longTermStorage[j + 1];
                victimInv->longTermStorage[--victimInv->longTermStorageCount] = noWeapon;
                break;
            }
        }

        InventoryData* winnerInv = &sharedState->players[winnerEntityId].inventory;
        bool alreadyHas = false;
        for (int s = 0; s < inventorySlots; s++) {
            if (winnerInv->slots[s] == victimArtifact) { 
                alreadyHas = true; 
                break; 
            }
        }
        if (!alreadyHas) {
            int sz = weaponTable[victimArtifact].slotSize;
            int freeStart = -1;

            for (int s = 0; s <= inventorySlots - sz; s++) {
                bool ok = true;
                for (int k = s; k < s + sz; k++)
                    if (winnerInv->slots[k] != noWeapon) { 
                        ok = false; 
                        break; 
                    }
                if (ok) { 
                    freeStart = s; 
                    break; 
                }
            }
            if (freeStart >= 0) {
                for (int k = freeStart; k < freeStart + sz && k < inventorySlots; k++)
                    winnerInv->slots[k] = victimArtifact;
            } 
            else {
                int evictSlot = -1, evictSz = inventorySlots + 1;
                for (int s = 0; s < inventorySlots; s++) {
                    int wid = winnerInv->slots[s];
                    if (wid == noWeapon) continue;
                    
                    int wsz = weaponTable[wid].slotSize;
                    if (wsz < evictSz && wsz < sz) { 
                        evictSz = wsz; 
                        evictSlot = s; 
                    }
                }
                if (evictSlot >= 0) {
                    int evictWid = winnerInv->slots[evictSlot];
                    if (winnerInv->longTermStorageCount < longTermStorageSlots)
                        winnerInv->longTermStorage[winnerInv->longTermStorageCount++] = evictWid;
                    
                    for (int k = evictSlot; k < evictSlot + weaponTable[evictWid].slotSize && k < inventorySlots; k++)
                        winnerInv->slots[k] = noWeapon;

                    for (int k = evictSlot; k < evictSlot + sz && k < inventorySlots; k++)
                        winnerInv->slots[k] = victimArtifact;
                }
            }
        }

        sharedState->sequence++;
        sem_post(&sharedState->shmLock);
    }

    if (victimPid > 0) kill(victimPid, SIGUSR2);
}

void DeadlockMonitor::start() {
    running = true;
    logAction(sharedState, "deadlock monitor started", getpid(), pthread_self());

    while (running && !sharedState->shouldExit && !sharedState->returnToMenu) {
        usleep(500000); //0.5s 

        if (sharedState->gamePaused || sharedState->waveTransition ||
            sharedState->gameOver || !sharedState->enemyEnabled)
            continue;

        sem_wait(&sharedState->artifactLock);
        bool anyWaiting = false;
        for (int i = 0; i < maxEntities; i++) {
            if (sharedState->artifacts.waitingFor[i] != -1) {
                anyWaiting = true;
                break;
            }
        }
        if (!anyWaiting) {
            sem_post(&sharedState->artifactLock);
            continue;
        }

        int victim = detectDeadlock();
        sem_post(&sharedState->artifactLock);

        if (victim != noEntity) {
            char msg[128];
            snprintf(msg, 128, "deadlock detected -- victim P%d (high tid)", victim);
            logAction(sharedState, msg, getpid(), pthread_self());
            resolveDeadlock(victim);
        }
    }

    logAction(sharedState, "deadlock monitor stopped", getpid(), pthread_self());
}

void* DeadlockMonitor::threadFunc(void* arg) {
    DeadlockMonitor* monitor = (DeadlockMonitor*)arg;
    monitor->start();
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
// === INVENTORY ALLOCATOR IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

InventoryAllocator::InventoryAllocator(SharedState* sharedState) {
    this->sharedState = sharedState;
}

int InventoryAllocator::getWeaponSize(int weaponId) {
    if (weaponId < 0 || weaponId >= weaponCount) 
        return 0;
    return weaponTable[weaponId].slotSize;
}

int InventoryAllocator::findFreeBlock(InventoryData* inv, int size) {
    int consecutive = 0;
    int blockStart = -1;

    for (int i = 0; i < inventorySlots; i++) {
        if (inv->slots[i] == noWeapon) {
            if (consecutive == 0) 
                blockStart = i;
            consecutive++;
            
            if (consecutive >= size) 
                return blockStart;
        } 
        else {
            consecutive = 0;
            blockStart  = -1;
        }
    }
    return -1;
}

int InventoryAllocator::findSmallestWeapon(InventoryData* inv) {
    int smallestSize = inventorySlots + 1;
    int smallestStart = -1;
    int i = 0;
    while (i < inventorySlots) {
        if (inv->slots[i] == noWeapon) { 
            i++; 
            continue; 
        }
        
        int wid = inv->slots[i];
        int wsize = getWeaponSize(wid);
        if (wsize < smallestSize) {
            smallestSize  = wsize;
            smallestStart = i;
        }
        i += wsize;
    }
    return smallestStart;
}

int InventoryAllocator::getWeaponStart(InventoryData* inv, int weaponId) {
    for (int i = 0; i < inventorySlots; i++)
        if (inv->slots[i] == weaponId) 
            return i;
    return -1;
}

void InventoryAllocator::clearSlots(InventoryData* inv, int slotStart, int size) {
    for (int i = slotStart; i < slotStart + size && i < inventorySlots; i++)
        inv->slots[i] = noWeapon;
}

void InventoryAllocator::writeSlots(InventoryData* inv, int slotStart, int weaponId, int size) {
    for (int i = slotStart; i < slotStart + size && i < inventorySlots; i++)
        inv->slots[i] = weaponId;
}

bool InventoryAllocator::toLongTermStorage(InventoryData* inv, int slotStart) {
    if (slotStart < 0) 
        return false;
    
    if (inv->longTermStorageCount >= longTermStorageSlots) 
        return false;

    int weaponId = inv->slots[slotStart];
    if (weaponId == noWeapon) 
        return false;

    int weaponSize = getWeaponSize(weaponId);
    inv->longTermStorage[inv->longTermStorageCount++] = weaponId;
    clearSlots(inv, slotStart, weaponSize);
    return true;
}

bool InventoryAllocator::allocateWeapon(int playerIndex, int weaponId) {
    if (playerIndex < 0 || playerIndex >= sharedState->playerCount)
        return false;
    
    if (weaponId < 0 || weaponId >= weaponCount)
        return false;

    InventoryData* inv = &sharedState->players[playerIndex].inventory;
    int size = getWeaponSize(weaponId);
    sem_wait(&sharedState->shmLock);

    int slot = findFreeBlock(inv, size);
    if (slot >= 0) {
        writeSlots(inv, slot, weaponId, size);
        sharedState->sequence++;
        sem_post(&sharedState->shmLock);
        char msg[128];
        snprintf(msg, 128, "P%d alloc weapon [%s] slot %d", playerIndex, weaponTable[weaponId].name, slot);
        logAction(sharedState, msg, getpid(), pthread_self());
        return true;
    }

    int attempts = 0;
    while (attempts < inventorySlots) {
        int evictSlot = findSmallestWeapon(inv);
        if (evictSlot < 0)
            break;

        int evictedWid = inv->slots[evictSlot];
        if (!toLongTermStorage(inv, evictSlot))
            break;

        char msg[128];
        snprintf(msg, 128, "P%d Swap Out [%s] -> LTS", playerIndex,
            (evictedWid >= 0 && evictedWid < weaponCount) ? weaponTable[evictedWid].name : "?");
        logAction(sharedState, msg, getpid(), pthread_self());

        slot = findFreeBlock(inv, size);
        if (slot >= 0) {
            writeSlots(inv, slot, weaponId, size);
            sharedState->sequence++;
            sem_post(&sharedState->shmLock);
            snprintf(msg, 128,"P%d alloc weapon [%s] slot %d after evict", 
                playerIndex, weaponTable[weaponId].name, slot);
            logAction(sharedState, msg, getpid(), pthread_self());
            return true;
        }
        attempts++;
    }

    sem_post(&sharedState->shmLock);
    char msg[128];
    snprintf(msg, 128, "P%d alloc weapon [%s] FAILED", playerIndex, weaponTable[weaponId].name);
    logAction(sharedState, msg, getpid(), pthread_self());
    return false;
}

bool InventoryAllocator::swapIn(int playerIndex, int weaponId) {
    if (playerIndex < 0 || playerIndex >= sharedState->playerCount)
        return false;

    InventoryData* inv = &sharedState->players[playerIndex].inventory;

    int ltsIdx = -1;
    for (int i = 0; i < inv->longTermStorageCount; i++) {
        if (inv->longTermStorage[i] == weaponId) { 
            ltsIdx = i; 
            break; 
        }
    }
    if (ltsIdx < 0) return false;

    sem_wait(&sharedState->shmLock);
    for (int i = ltsIdx; i < inv->longTermStorageCount - 1; i++)
        inv->longTermStorage[i] = inv->longTermStorage[i + 1];

    inv->longTermStorage[--inv->longTermStorageCount] = noWeapon;
    sem_post(&sharedState->shmLock);

    bool ok = allocateWeapon(playerIndex, weaponId);
    if (!ok) {
        sem_wait(&sharedState->shmLock);
        inv->longTermStorage[inv->longTermStorageCount++] = weaponId;
        sem_post(&sharedState->shmLock);
    }
    return ok;
}

void InventoryAllocator::removeWeapon(int playerIndex, int weaponId) {
    if (playerIndex < 0 || playerIndex >= sharedState->playerCount)
        return;
    
    InventoryData* inv = &sharedState->players[playerIndex].inventory;
    int start = getWeaponStart(inv, weaponId);
    if (start < 0) 
        return;

    sem_wait(&sharedState->shmLock);
    clearSlots(inv, start, getWeaponSize(weaponId));
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);
}

bool InventoryAllocator::hasWeapon(int playerIndex, int weaponId) {
    if (playerIndex < 0 || playerIndex >= sharedState->playerCount)
        return false;

    InventoryData* inv = &sharedState->players[playerIndex].inventory;
    return getWeaponStart(inv, weaponId) >= 0;
}

bool InventoryAllocator::hasUltimateWeapons(int playerIndex) {
    bool hasSC = false, hasLB = false, hasER = false;
    InventoryData* inv = &sharedState->players[playerIndex].inventory;
    int i = 0;
    while (i < inventorySlots) {
        int wid = inv->slots[i];
        if (wid != noWeapon) {
            if (wid == 0) hasSC = true;
            if (wid == 1) hasLB = true;
            if (wid == 8) hasER = true;
            i += weaponTable[wid].slotSize;
        }
        else {
            i++;
        }
    }
    return (hasSC && hasLB) || (hasSC && hasER) || (hasLB && hasER);
}

void InventoryAllocator::logInventory(int playerIndex) {
    if (playerIndex < 0 || playerIndex >= sharedState->playerCount)
        return;
    
    InventoryData* inv = &sharedState->players[playerIndex].inventory;
    char msg[128];
    snprintf(msg, 128, "P%d inventory:", playerIndex);
    logAction(sharedState, msg, getpid(), pthread_self());

    int i = 0;
    while (i < inventorySlots) {
        if (inv->slots[i] == noWeapon) { 
            i++; 
            continue; 
        }

        int wid = inv->slots[i];
        snprintf(msg, 128, "  slot %d: [%s] size %d", i, weaponTable[wid].name, getWeaponSize(wid));
        logAction(sharedState, msg, getpid(), pthread_self());
        i += getWeaponSize(wid);
    }
    snprintf(msg, 128, "P%d LTS: %d items", playerIndex, inv->longTermStorageCount);
    logAction(sharedState, msg, getpid(), pthread_self());
}

//////////////////////////////////////////////////////////////////////////////////////////
// === RENDERER IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

static const char* backgroundImage = "arbiter/assets/background/background.png";
static const char* fontPath = "arbiter/assets/fonts/font.ttf";

Renderer::Renderer(SharedState* sharedState) {
    this->sharedState = sharedState;
    debugOverlay = false;
    showActionLog = true;
    lastSequence = -1;
    escapeMenuSelection = 0;

    for (int i = 0; i < maxEntities; i++) {
        idleConditions[i] = true;
        animStates[i].isIdle = true;
        animStates[i].isAttacking = false;
        animStates[i].isDead = false;
        animStates[i].attackTriggered = false;
        animStates[i].deathTriggered = false;
    }

    for (int i = 0; i < maxParticles; i++)
        particles[i].active = false;

    teleport.active = false;
}

Renderer::~Renderer() {
    if (window.isOpen()) 
        window.close();
}

bool Renderer::initialize() {
    
    printf("[renderer] creating window...\n");
    fflush(stdout);
    
    try {
        window.create(VideoMode(windowWidth, windowHeight), "Chrono Rift", Style::Titlebar | Style::Resize | Style::Close);
        window.setFramerateLimit(60);
    } 
    catch (const std::exception& e) {
        fprintf(stderr, "[renderer] SFML exception: %s\n", e.what());
        return false;
    }
    
    if (!font.loadFromFile(fontPath)) {
        fprintf(stderr, "failed to load font: %s\n", fontPath);
    }

    if (!backgroundTexture.loadFromFile(backgroundImage)) {
        fprintf(stderr, "background not found: %s\n", backgroundImage);
    } 
    else {
        backgroundSprite.setTexture(backgroundTexture);
        backgroundSprite.setScale(
            (float)windowWidth / backgroundTexture.getSize().x,
            (float)windowHeight / backgroundTexture.getSize().y);
    }
    
    printf("[renderer] initialization complete\n");
    fflush(stdout);
    return true;
}

bool Renderer::isOpen() { 
    return window.isOpen(); 
}

void Renderer::close() { 
    window.close(); 
}

bool Renderer::isGameReadyForDrawing() {
    if (!sharedState) return false;
    if (!sharedState->gameReady) return false;
    if (sharedState->playerCount == 0) return false;
    if (sharedState->players[0].entity.maxHp == 0) return false;
    if (sharedState->enemyCount == 0) return false;
    return true;
}

Vector2f Renderer::playerScreenPos(int idx) {
    int col = idx % 2;
    int row = idx / 2;
    int cellWidth = playerAreaW / 2;
    int cellHeight = battleHeight / 2;
    
    float x = playerAreaX + col * cellWidth + cellWidth / 2.0 - 32;
    float y = battleYStart + row * cellHeight + cellHeight / 2.0 - 32;
    return Vector2f(x, y);
}

Vector2f Renderer::enemyScreenPos(int idx) {

    int cols = 3;
    if (snap.enemyCount <= 3) 
        cols = snap.enemyCount;

    if (cols == 0)
        cols = 1;

    int rows = (snap.enemyCount + cols - 1) / cols;
    int col = idx % cols;
    int row = idx / cols;
    int cellWidth = enemyAreaW / cols;
    int cellHeight = battleHeight / (rows > 0 ? rows : 1);

    float x = enemyAreaX + col * cellWidth + cellWidth / 2.0 - 32;
    float y = battleYStart + row * cellHeight + cellHeight / 2.0 - 32;
    return Vector2f(x, y);
}

void Renderer::buildIdleTag(char* buf, bool isPlayer, int index) {
    snprintf(buf, 32, "%s_idle_%d", isPlayer ? "p" : "e", index);
}

void Renderer::buildAttackTag(char* buf, bool isPlayer, int index) {
    snprintf(buf, 32, "%s_atk_%d", isPlayer ? "p" : "e", index);
}

void Renderer::buildDeathTag(char* buf, bool isPlayer, int index) {
    snprintf(buf, 32, "%s_death_%d", isPlayer ? "p" : "e", index);
}

void Renderer::updateAnimStates() {
    for (int i = 0; i < snap.playerCount; i++) {
        EntityData* entity = &snap.players[i].entity;
        EntityAnimationState* eas = &animStates[i];

        if (!entity->isAlive) {
            char idleTag[32], atkTag[32];
            buildIdleTag(idleTag, true, i);
            buildAttackTag(atkTag, true, i);
            animationManager.stop(idleTag);
            animationManager.stop(atkTag);

            if (!eas->deathTriggered) triggerDeath(entity->id, true, 0);
            eas->isDead = true;
            eas->isIdle = false;
            eas->isAttacking = false;
            eas->deathTriggered = true;
            continue;
        }
        if (entity->isAlive && !eas->isDead && !eas->isAttacking) {
            eas->isIdle = true;
        }
    }

    for (int i = 0; i < snap.enemyCount; i++) {
        EntityData* entity = &snap.enemies[i].entity;
        EntityAnimationState* eas = &animStates[maxPlayers + i];

        if (entity->isAlive && eas->isDead) {
            char idleTag[32], atkTag[32];
            buildIdleTag(idleTag, false, i);
            buildAttackTag(atkTag, false, i);
            animationManager.stop(idleTag);
            animationManager.stop(atkTag);
            eas->isDead = false;
            eas->deathTriggered = false;
            eas->isIdle = false;
            eas->isAttacking = false;
            eas->attackTriggered = false;
        }

        if (!entity->isAlive) {
            char idleTag[32], atkTag[32];
            buildIdleTag(idleTag, false, i);
            buildAttackTag(atkTag, false, i);
            animationManager.stop(idleTag);
            animationManager.stop(atkTag);

            if (!eas->deathTriggered) triggerDeath(entity->id, false, snap.enemies[i].spriteType);
            eas->isDead = true;
            eas->isIdle = false;
            eas->isAttacking = false;
            eas->deathTriggered = true;
            continue;
        }
        if (entity->isAlive && !eas->isDead && !eas->isAttacking) {
            eas->isIdle = true;
        }
    }
}

void Renderer::triggerAttack(int entityId, bool isPlayer, int spriteType) {
    
    char tag[32];
    int idx = isPlayer ? entityId : entityId - maxPlayers;
    buildAttackTag(tag, isPlayer, idx);

    const AnimationDetails* details = isPlayer
        ? playerAnimation[snap.players[idx].spriteType % 4]
        : enemyAnimation[spriteType % 3];

    char idleTag[32];
    buildIdleTag(idleTag, isPlayer, idx);
    animationManager.stop(idleTag);

    int stateIdx = isPlayer ? entityId : maxPlayers + idx;
    animStates[stateIdx].isAttacking = true;
    animStates[stateIdx].isIdle = false;

    bool* cond[1] = { &animStates[stateIdx].isAttacking };
    animationManager.play(tag, *details, true, isPlayer ? playerScreenPos(idx) : enemyScreenPos(idx), true, cond);

    float sx = playerDisplayWidth / details->attackFrameWidth;
    float sy = playerDisplayHeight / details->attackFrameHeight;
    animationManager.setScale(tag, sx, sy);

    animStates[stateIdx].attackTriggered = true;
}

void Renderer::triggerDeath(int entityId, bool isPlayer, int spriteType) {
    (void)spriteType;
    int idx = isPlayer ? entityId : entityId - maxPlayers;

    char idleTag[32], atkTag[32];
    buildIdleTag(idleTag, isPlayer, idx);
    buildAttackTag(atkTag, isPlayer, idx);
    animationManager.stop(idleTag);
    animationManager.stop(atkTag);

    Vector2f pos = isPlayer ? playerScreenPos(idx) : enemyScreenPos(idx);
    spawnDeathParticles(pos, isPlayer);
}

void Renderer::triggerTeleport(int attackerEntityId, bool attackerIsPlayer,
    int attackerSpriteType, int targetEntityId, bool targetIsPlayer, bool playAttack) {

    teleport.active = true;
    teleport.isPlayer = attackerIsPlayer;
    teleport.entityId = attackerEntityId;
    teleport.spriteType = attackerSpriteType;
    teleport.returning = false;
    teleport.progress = 0.f;
    teleport.playAttack = playAttack;

    int aIdx = attackerIsPlayer ? attackerEntityId : attackerEntityId - maxPlayers;
    int tIdx = targetIsPlayer ? targetEntityId : targetEntityId  - maxPlayers;

    teleport.startPos = attackerIsPlayer ? playerScreenPos(aIdx) : enemyScreenPos(aIdx);
    teleport.targetPos = targetIsPlayer ? playerScreenPos(tIdx) : enemyScreenPos(tIdx);
    teleport.currentPos = teleport.startPos;

    if (playAttack) {
        char idleTag[32];
        buildIdleTag(idleTag, attackerIsPlayer, aIdx);
        animationManager.stop(idleTag);

        int stateIdx = attackerIsPlayer ? attackerEntityId : maxPlayers + aIdx;
        animStates[stateIdx].isIdle      = false;
        animStates[stateIdx].isAttacking = true;
    }
}

void Renderer::updateTeleport(float dt) {
    if (!teleport.active) return;

    static const float teleportSpeed = 1800.f;
    Vector2f from = teleport.returning ? teleport.targetPos : teleport.startPos;
    Vector2f to = teleport.returning ? teleport.startPos : teleport.targetPos;
    Vector2f delta = to - from;

    float dist = sqrtf(delta.x * delta.x + delta.y * delta.y);
    if (dist <= 0.001f) {
        teleport.progress = 1.f;
    }
    else {
        teleport.progress += (teleportSpeed * dt) / dist;
    }

    if (teleport.progress >= 1.f) {
        teleport.progress = 0.f;

        if (!teleport.returning) {
            teleport.currentPos = teleport.targetPos;
            teleport.returning = true;
            if (teleport.playAttack) {
                triggerAttack(teleport.entityId, teleport.isPlayer, teleport.spriteType);
            }
        }
        else {
            teleport.active = false;
            teleport.currentPos = teleport.startPos;

            int idx = teleport.isPlayer ? teleport.entityId : (teleport.entityId - maxPlayers);
            int stateIdx = teleport.isPlayer ? teleport.entityId : maxPlayers + idx;

            char atkTag[32];
            buildAttackTag(atkTag, teleport.isPlayer, idx);
            animationManager.stop(atkTag);

            animStates[stateIdx].isAttacking = false;
            animStates[stateIdx].attackTriggered = false;
            animStates[stateIdx].isIdle = true;

            sem_wait(&sharedState->shmLock);
            sharedState->attackAnimationInProgress = false;
            sharedState->sequence++;
            sem_post(&sharedState->shmLock);
        }
    } 
    else {
        teleport.currentPos = from + (to - from) * teleport.progress;
    }
}

void Renderer::drawTeleport() {
    if (!teleport.active) return;

    RectangleShape body(Vector2f(64.f, 64.f));
    body.setPosition(teleport.currentPos);
    Color bodyColor = teleport.isPlayer ? Color(80, 180, 255, 230) : Color(255, 80,  80, 230);
    body.setFillColor(bodyColor);
    body.setOutlineColor(Color::White);
    body.setOutlineThickness(3.f);
    window.draw(body);

    for (int i = 1; i <= 2; i++) {
        Vector2f from = teleport.returning ? teleport.startPos : teleport.targetPos;
        Vector2f to = teleport.returning ? teleport.targetPos : teleport.startPos;
        float trailT = teleport.progress - i * 0.12;
        if (trailT < 0.f) continue;

        Vector2f trailPos = to + (from - to) * trailT;
        RectangleShape trail(Vector2f(64.0, 64.0));
        trail.setPosition(trailPos);
        Color tc = bodyColor;
        tc.a = (Uint8)(80 / i);
        trail.setFillColor(tc);
        window.draw(trail);
    }
}

void Renderer::spawnDeathParticles(Vector2f pos, bool isPlayer) {
    Color color = isPlayer ? Color(100, 180, 255) : Color(255,  80,  80);

    int spawned = 0;
    for (int i = 0; i < maxParticles && spawned < 20; i++) {
        if (particles[i].active) continue;

        float angle = (rand() % 360) * (22/7.0) / 180.0;
        float speed = 40 + rand() % 80;

        particles[i].shape.setRadius(4.0 + rand() % 6);
        particles[i].shape.setFillColor(color);
        particles[i].shape.setPosition(pos.x + 32.0, pos.y + 32.0);
        particles[i].velocity = Vector2f(cosf(angle) * speed, sinf(angle) * speed);
        particles[i].maxLifetime = 0.6f + (rand() % 40) / 100.0;
        particles[i].lifetime = particles[i].maxLifetime;
        particles[i].active = true;
        spawned++;
    }
}

void Renderer::updateParticles(float dt) {
    for (int i = 0; i < maxParticles; i++) {
        if (!particles[i].active) continue;
        particles[i].lifetime -= dt;

        if (particles[i].lifetime <= 0.f) {
            particles[i].active = false;
            continue;
        }

        Vector2f pos = particles[i].shape.getPosition();
        pos += particles[i].velocity * dt;
        particles[i].shape.setPosition(pos);

        float alpha = (particles[i].lifetime / particles[i].maxLifetime) * 255.f;
        Color color = particles[i].shape.getFillColor();
        color.a = (Uint8)alpha;
        particles[i].shape.setFillColor(color);

        particles[i].velocity *= 0.95f;
    }
}

void Renderer::drawParticles() {
    for (int i = 0; i < maxParticles; i++) {
        if (!particles[i].active) 
            continue;
        window.draw(particles[i].shape);
    }
}

void Renderer::drawHealthBar(Vector2f pos, int hp, int maxHp, int spriteH) {
    (void)spriteH;
    if (maxHp <= 0) return;

    float fillRatio = hp / (float)maxHp;
    if (fillRatio < 0.f) fillRatio = 0.f;
    if (fillRatio > 1.f) fillRatio = 1.f;

    float barY = pos.y + barYOffsetHp;

    RectangleShape bg(Vector2f(barWidth, hpBarHeight));
    bg.setPosition(pos.x, barY);
    bg.setFillColor(Color(80, 0, 0));
    window.draw(bg);

    RectangleShape fill(Vector2f(barWidth * fillRatio, hpBarHeight));
    fill.setPosition(pos.x, barY);
    fill.setFillColor(Color(220, 30, 30));
    window.draw(fill);
}

void Renderer::drawStaminaBar(Vector2f pos, int stamina, int maxStamina, int spriteH) {
    (void)spriteH;
    if (maxStamina <= 0) return;

    float fillRatio = stamina / (float)maxStamina;
    if (fillRatio < 0.0) fillRatio = 0.0;
    if (fillRatio > 1.0) fillRatio = 1.0;

    float barY = pos.y + barYOffsetSp;

    RectangleShape bg(Vector2f(barWidth, staminaBarHeight));
    bg.setPosition(pos.x, barY);
    bg.setFillColor(Color(0, 0, 80));
    window.draw(bg);

    RectangleShape fill(Vector2f(barWidth * fillRatio, staminaBarHeight));
    fill.setPosition(pos.x, barY);
    fill.setFillColor(Color(30, 100, 220));
    window.draw(fill);
}

void Renderer::drawSelectionCircle(Vector2f pos, Color color, int spriteH) {
    CircleShape circle(circleRadius);
    circle.setFillColor(Color::Transparent);
    circle.setOutlineColor(color);
    circle.setOutlineThickness(circleThickness);
    circle.setPosition(pos.x + spriteH / 2.0 - circleRadius, pos.y + spriteH - circleRadius * 0.5);
    window.draw(circle);
}

void Renderer::drawStunIndicator(Vector2f pos, int spriteH) {
    (void)spriteH;
    Text stunText;
    stunText.setFont(font);
    stunText.setString("***");
    stunText.setCharacterSize(14);
    stunText.setFillColor(Color::Yellow);
    stunText.setPosition(pos.x, pos.y - 30.f);
    window.draw(stunText);
}

void Renderer::drawEntity(EntityData* entity, Vector2f pos, bool isPlayer, 
    int spriteType, bool yellowCircle, bool redCircle) {
    
    if (!entity) return;
    if (!snap.gameReady) return;
    if (entity->maxHp <= 0) return;
    if (!entity->isAlive) return;

    int spriteH = (int)playerDisplayHeight;
    int idx = isPlayer ? entity->id : entity->id - maxPlayers;

    if (idx < 0) return;
    if (isPlayer && idx >= maxPlayers) return;
    if (!isPlayer && idx >= maxEnemies) return;

    const AnimationDetails* details = nullptr;
    if (isPlayer) {
        int pSprite = snap.players[idx].spriteType % 4;
        details = playerAnimation[pSprite];
    } 
    else {
        details = enemyAnimation[spriteType % 3];
    }
    
    if (!details) return;
    
    char idleTag[32];
    if (isPlayer) {
        snprintf(idleTag, 32, "p_idle_%d", idx);
    } 
    else {
        snprintf(idleTag, 32, "e_idle_%d", idx);
    }
    
    try {
        if (!animationManager.isPlaying(idleTag) &&
            !animStates[isPlayer ? entity->id : maxPlayers + idx].isAttacking) {
            
            bool* cond[1] = { &idleConditions[entity->id] };
            animationManager.play(idleTag, *details, false, pos, false, cond);
            
            float sx = playerDisplayWidth / details->idleFrameWidth;
            float sy = playerDisplayHeight / details->idleFrameHeight;
            animationManager.setScale(idleTag, sx, sy);
        } 
        else {
            animationManager.setPosition(idleTag, pos);
        }
    } catch (...) {
        RectangleShape rect(Vector2f(64, 64));
        rect.setPosition(pos);
        rect.setFillColor(isPlayer ? Color::Blue : Color::Red);
        window.draw(rect);
        return;
    }
    
    drawHealthBar(pos, entity->currentHp, entity->maxHp, spriteH);
    drawStaminaBar(pos, entity->currentStamina, entity->maxStamina, spriteH);
    
    Text nameText;
    nameText.setFont(font);
    nameText.setString(entity->name);
    nameText.setCharacterSize(10);
    nameText.setFillColor(Color::White);
    nameText.setPosition(pos.x, pos.y - 35);
    window.draw(nameText);
    
    if (entity->isStunned) drawStunIndicator(pos, spriteH);
    if (yellowCircle) drawSelectionCircle(pos, Color::Yellow, spriteH);
    if (redCircle) drawSelectionCircle(pos, Color::Red, spriteH);
}

void Renderer::drawBackground() {
    if (backgroundTexture.getSize().x > 0)
        window.draw(backgroundSprite);
    else
        window.clear(Color(20, 20, 35));
}

void Renderer::drawHUD() {
    RectangleShape hudBackground(Vector2f(windowWidth, hudHeight));
    hudBackground.setFillColor(Color(15, 15, 25, 220));
    window.draw(hudBackground);

    Text waveText;
    waveText.setFont(font);
    char waveBuf[32];
    snprintf(waveBuf, 32, "Wave %d", snap.waveNumber);
    waveText.setString(waveBuf);
    waveText.setCharacterSize(18);
    waveText.setFillColor(Color::White);
    waveText.setPosition(windowWidth / 2.0 - 40, 10);
    window.draw(waveText);

    Text killText;
    killText.setFont(font);
    char killBuf[32];
    snprintf(killBuf, 32, "Defeated: %d / %d", snap.enemiesDefeated, winKillCount);
    killText.setString(killBuf);
    killText.setCharacterSize(14);
    killText.setFillColor(Color(220, 80, 80));
    killText.setPosition(windowWidth / 2.0 + 80, 15);
    window.draw(killText);

    Text aliveText;
    aliveText.setFont(font);
    char aliveBuf[32];
    snprintf(aliveBuf, 32, "Players: %d / %d", snap.playersAlive, snap.playerCount);
    aliveText.setString(aliveBuf);
    aliveText.setCharacterSize(14);
    aliveText.setFillColor(Color(80, 180, 80));
    aliveText.setPosition(20, 15);
    window.draw(aliveText);
}

void Renderer::drawCenterPanel() {
    RectangleShape leftDiv(Vector2f(2.0, (float)battleHeight));
    leftDiv.setPosition((float)centerX, (float)battleYStart);
    leftDiv.setFillColor(Color(80, 80, 120, 160));
    window.draw(leftDiv);

    RectangleShape rightDiv(Vector2f(2.0, (float)battleHeight));
    rightDiv.setPosition((float)(centerX + centerW), (float)battleYStart);
    rightDiv.setFillColor(Color(80, 80, 120, 160));
    window.draw(rightDiv);
}

void Renderer::drawPlayers() {
    if (snap.playerCount == 0) return;
    if (!snap.gameReady) return;

    for (int i = 0; i < snap.playerCount; i++) {
        if (snap.players[i].entity.maxHp == 0) continue;
        if (!snap.players[i].entity.isAlive) continue;

        EntityData* entity = &snap.players[i].entity;
        Vector2f pos = playerScreenPos(i);

        bool showTurnCircle = entity->hasTurn && !snap.gamePaused && !snap.waveTransition 
        && !snap.gameOver && (snap.currentTurnEntityId >= maxPlayers || snap.combatPhase != 0);
        bool redCircle = false;

        if (!snap.gamePaused && !snap.waveTransition && !snap.gameOver && snap.currentTurnEntityId >= maxPlayers) {
            int enemyIdx = snap.currentTurnEntityId - maxPlayers;
            if (enemyIdx < snap.enemyCount)
                redCircle = (snap.enemies[enemyIdx].actionTarget == i);
        }
        drawEntity(entity, pos, true, 0, showTurnCircle, redCircle);
    }
}

void Renderer::drawEnemies() {
    if (snap.enemyCount == 0) return;
    if (!snap.gameReady) return;

    for (int i = 0; i < snap.enemyCount; i++) {
        if (snap.enemies[i].entity.maxHp == 0) continue;
        if (!snap.enemies[i].entity.isAlive) continue;

        EntityData* entity = &snap.enemies[i].entity;
        Vector2f pos = enemyScreenPos(i);
        bool showTurnCircle = entity->hasTurn && !snap.gamePaused && !snap.waveTransition && !snap.gameOver;
        drawEntity(entity, pos, false, snap.enemies[i].spriteType, showTurnCircle, false);
    }
}

void Renderer::drawActionLog() {
    float panelY = (float)(windowHeight - logPanelHeight);

    RectangleShape panel(Vector2f((float)windowWidth, (float)logPanelHeight));
    panel.setPosition(0, panelY);
    panel.setFillColor(Color(10, 10, 20, 210));
    window.draw(panel);

    Text title;
    title.setFont(font);
    title.setString("Action Log");
    title.setCharacterSize(logFontSize);
    title.setFillColor(Color(150, 150, 200));
    title.setPosition(8, panelY + 4);
    window.draw(title);

    int count = snap.logCount < logMaxVisible ? snap.logCount : logMaxVisible;

    float textStartY = panelY + logFontSize + 8;
    for (int i = 0; i < count; i++) {
        int slot = (snap.logHead - count + i + logSize * 2) % logSize;
        const ActionLogEntry* entry = &snap.log[slot];
        if (!entry->valid) 
            continue;

        Text logText;
        logText.setFont(font);
        char buf[160];
        snprintf(buf, 160, "[pid:%d tid:%ld] %s", entry->pid, entry->threadId, entry->message);
        logText.setString(buf);
        logText.setCharacterSize(logFontSize);
        logText.setFillColor(Color(200, 200, 200));
        logText.setPosition(8, textStartY + i * (logFontSize + 3));
        window.draw(logText);
    }
}

void Renderer::drawLine(const char* txt, float x, float& y) {
    Text text;
    text.setFont(font);
    text.setString(txt);
    text.setCharacterSize(11);
    text.setFillColor(Color::Green);
    text.setPosition(x, y);
    window.draw(text);
    y += 14.0;
}

void Renderer::drawDebugOverlay() {
    RectangleShape background(Vector2f(320, 320));
    background.setFillColor(Color(0, 0, 0, 180));
    background.setPosition(10.0, (float)hudHeight + 10);
    window.draw(background);

    float y = (float)hudHeight + 15;
    float x = 15;

    char buf[128];
    snprintf(buf, 128, "SHM seq: %d", snap.sequence);
    drawLine(buf, x, y);
    snprintf(buf, 128, "Wave: %d  Killed: %d", snap.waveNumber, snap.enemiesDefeated);
    drawLine(buf, x, y);
    snprintf(buf, 128, "Turn entity: %d", snap.currentTurnEntityId);
    drawLine(buf, x, y);

    snprintf(buf, 128, "Artifacts SC=%d LB=%d ER=%d", snap.artifacts.holder[0],
        snap.artifacts.holder[1], snap.artifacts.holder[2]);
    drawLine(buf, x, y);

    drawLine("-- Players --", x, y);
    for (int i = 0; i < snap.playerCount; i++) {
        const EntityData* entity = &snap.players[i].entity;
        snprintf(buf, 128, "P%d hp:%d st:%d/%d pid:%d",
                 i, entity->currentHp, entity->currentStamina, entity->maxStamina, entity->pid);
        drawLine(buf, x, y);
    }

    drawLine("-- Enemies --", x, y);
    for (int i = 0; i < snap.enemyCount; i++) {
        const EntityData* entity = &snap.enemies[i].entity;
        snprintf(buf, 128, "E%d hp:%d st:%d/%d alive:%d",
                 i, entity->currentHp, entity->currentStamina, entity->maxStamina, (int)entity->isAlive);
        drawLine(buf, x, y);
    }
}

void Renderer::drawEscapeMenu() {
    
    RectangleShape overlay(Vector2f((float)windowWidth, (float)windowHeight));
    overlay.setFillColor(Color(0, 0, 0, 160));
    window.draw(overlay);

    float boxW = 320, boxH = 160;
    float boxX = windowWidth / 2 - boxW / 2;
    float boxY = windowHeight / 2 - boxH / 2;

    RectangleShape box(Vector2f(boxW, boxH));
    box.setFillColor(Color(20, 20, 40, 240));
    box.setOutlineColor(Color(100, 100, 180));
    box.setOutlineThickness(2);
    box.setPosition(boxX, boxY);
    window.draw(box);

    const char* items[2] = {
        "Resume", "Return to Desktop"
    };

    for (int i = 0; i < 2; i++) {
        Text item;
        item.setFont(font);
        item.setString(items[i]);
        item.setCharacterSize(16);
        item.setFillColor(i == escapeMenuSelection ? Color::Yellow : Color::White);
        item.setPosition(boxX + 44, boxY + 40 + i * 50);
        window.draw(item);

        if (i == escapeMenuSelection) {
            Text arrow;
            arrow.setFont(font);
            arrow.setString(">");
            arrow.setCharacterSize(16);
            arrow.setFillColor(Color::Yellow);
            arrow.setPosition(boxX + 22, boxY + 40 + i * 50);
            window.draw(arrow);
        }
    }

    Text menuHint;
    menuHint.setFont(font);
    menuHint.setString("Up/Down: Navigate   Enter: Confirm   Esc: Resume");
    menuHint.setCharacterSize(11);
    menuHint.setFillColor(Color(140, 140, 140));
    menuHint.setPosition(boxX + 14, boxY + boxH - 24);
    window.draw(menuHint);
}

void Renderer::drawWaveTransition() {
    RectangleShape overlay(Vector2f((float)windowWidth, (float)windowHeight));
    overlay.setFillColor(Color(0, 0, 0, 200));
    window.draw(overlay);

    Text txt;
    txt.setFont(font);
    char buf[64];
    snprintf(buf, 64, "Wave %d Complete!", snap.waveNumber - 1);
    txt.setString(buf);
    txt.setCharacterSize(32);
    txt.setFillColor(Color::White);
    txt.setPosition(windowWidth / 2 - 130, windowHeight / 2 - 60);
    window.draw(txt);

    Text sub;
    sub.setFont(font);
    char subBuf[64];
    snprintf(subBuf, 64, "Enemies Defeated: %d / %d", snap.enemiesDefeated, winKillCount);
    sub.setString(subBuf);
    sub.setCharacterSize(18);
    sub.setFillColor(Color(200, 200, 100));
    sub.setPosition(windowWidth / 2 - 110, windowHeight / 2 + 10);
    window.draw(sub);

    Text next;
    next.setFont(font);
    next.setString("Next wave incoming...");
    next.setCharacterSize(14);
    next.setFillColor(Color(150, 150, 150));
    next.setPosition(windowWidth / 2 - 90, windowHeight / 2 + 50);
    window.draw(next);
}

void Renderer::drawWinScreen() {
    RectangleShape overlay(Vector2f((float)windowWidth, (float)windowHeight));
    overlay.setFillColor(Color(0, 0, 0, 210));
    window.draw(overlay);

    Text txt;
    txt.setFont(font);
    txt.setString("VICTORY");
    txt.setCharacterSize(48);
    txt.setFillColor(Color(255, 215, 0));
    txt.setPosition(windowWidth / 2 - 100, windowHeight / 2 - 60);
    window.draw(txt);

    Text sub;
    sub.setFont(font);
    sub.setString("All enemies defeated!");
    sub.setCharacterSize(18);
    sub.setFillColor(Color::White);
    sub.setPosition(windowWidth / 2 - 90, windowHeight / 2 + 10);
    window.draw(sub);
}

void Renderer::drawLoseScreen() {
    RectangleShape overlay(Vector2f((float)windowWidth, (float)windowHeight));
    overlay.setFillColor(Color(0, 0, 0, 210));
    window.draw(overlay);

    Text txt;
    txt.setFont(font);
    txt.setString("DEFEATED");
    txt.setCharacterSize(48);
    txt.setFillColor(Color(180, 30, 30));
    txt.setPosition(windowWidth / 2 - 110, windowHeight / 2 - 60);
    window.draw(txt);

    Text sub;
    sub.setFont(font);
    sub.setString("All players have fallen.");
    sub.setCharacterSize(18);
    sub.setFillColor(Color::White);
    sub.setPosition(windowWidth / 2 - 100, windowHeight / 2 + 10);
    window.draw(sub);
}

void Renderer::drawCombatUI() {

    int phase = snap.combatPhase;
    bool paused = snap.gamePaused || snap.waveTransition || snap.gameOver;
    int turnEntity = snap.currentTurnEntityId;
    
    bool playerTurnActive = !paused && turnEntity >= 0 && 
    turnEntity < snap.playerCount && snap.players[turnEntity].entity.hasTurn;

    if (phase != 0) {
        int actingPlayer = snap.combatActingPlayer;

        if (actingPlayer >= 0 && actingPlayer < snap.playerCount) {
            Vector2f pos = playerScreenPos(actingPlayer);

            Text turnText;
            turnText.setFont(font);
            turnText.setString("YOUR TURN");
            turnText.setCharacterSize(12);
            turnText.setFillColor(Color::Yellow);
            turnText.setPosition(pos.x - 2.0, pos.y - 52.0);
            window.draw(turnText);

            const char* hintStr = "";
            if (phase == 2) hintStr = "Arrow Keys: Select target   Enter: Confirm   Esc: Menu";
            if (phase == 3) hintStr = "Up/Down: Select action   Enter: Confirm   Esc: Back";
            if (phase == 4) hintStr = "Enter: Pick up weapon   Esc: Leave";
            if (phase == 5) hintStr = "Up/Down: Select weapon   Enter: Swap In   Esc: Back";
            if (phase == 6) hintStr = "Up/Down: Select weapon   Enter: Use   Esc: Back";

            Text hint;
            hint.setFont(font);
            hint.setString(hintStr);
            hint.setCharacterSize(12);
            hint.setFillColor(Color(220, 220, 100));
            hint.setPosition(20.f, (float)(windowHeight - logPanelHeight - 22));
            window.draw(hint);
        }

        if (phase == 4) {
            
            int wid = snap.weaponDropId;
            const char* wname = "Unknown";
            if (wid >= 0 && wid < weaponCount) wname = weaponTable[wid].name;
            
            RectangleShape box(Vector2f(360.0, 120.0));
            box.setFillColor(Color(10, 10, 30, 230));
            box.setOutlineColor(Color(120, 120, 200));
            box.setOutlineThickness(2.f);
            box.setPosition(windowWidth / 2.0 - 180, windowHeight / 2.0 - 90);
            window.draw(box);

            Text prompt;
            prompt.setFont(font);
            prompt.setString(std::string("Enemy dropped: ") + wname);
            prompt.setCharacterSize(16);
            prompt.setFillColor(Color::White);
            prompt.setPosition(windowWidth / 2.0 - 150, windowHeight / 2.0 - 70);
            window.draw(prompt);

            Text options;
            options.setFont(font);
            options.setString("Enter: Pick up    Esc: Leave");
            options.setCharacterSize(13);
            options.setFillColor(Color(200, 200, 200));
            options.setPosition(windowWidth / 2.0 - 140, windowHeight / 2.0 - 40);
            window.draw(options);
        }
    }

    if (playerTurnActive && phase != 4) {
        int selT = snap.combatSelectedTarget;
        if (selT < 0 || selT >= snap.enemyCount || !snap.enemies[selT].entity.isAlive) {
            selT = -1;
            for (int i = 0; i < snap.enemyCount; i++) {
                if (snap.enemies[i].entity.isAlive) {
                    selT = i;
                    break;
                }
            }
        }

        if (selT >= 0 && selT < snap.enemyCount) {
            Vector2f pos = enemyScreenPos(selT);
            static const float ringRadius = 36.f;
            CircleShape ring(ringRadius);
            ring.setFillColor(Color::Transparent);
            ring.setOutlineColor(Color::Cyan);
            ring.setOutlineThickness(4.f);
            float ringOffset = ringRadius - 32.f;
            ring.setPosition(pos.x - ringOffset, pos.y - ringOffset);
            window.draw(ring);
        }
    }

    if (phase == 3) drawActionMenu();
    if (phase == 5) drawSwapInMenu();
    if (phase == 6) drawWeaponSelectMenu();
}

void Renderer::drawActionMenu() {

    static const char* actionNames[7] = {
        "Strike", "Exhaust", "Use Weapon", "Swap In", "Heal", "Skip", "Ultimate Ability"
    };

    float boxW = 260, boxH = 295;
    float boxX = windowWidth / 2.0 - boxW / 2.0;
    float boxY = windowHeight / 2.0 - boxH / 2.0 - 40;

    RectangleShape bg(Vector2f(boxW, boxH));
    bg.setFillColor(Color(10, 10, 30, 235));
    bg.setOutlineColor(Color(120, 120, 200));
    bg.setOutlineThickness(2.f);
    bg.setPosition(boxX, boxY);
    window.draw(bg);

    Text title;
    title.setFont(font);
    title.setString("SELECT ACTION");
    title.setCharacterSize(14);
    title.setFillColor(Color(160, 160, 255));
    title.setPosition(boxX + 20.f, boxY + 10.f);
    window.draw(title);

    //checking if acting player has any two of three artifacts in inventory for ultimate:
    bool canUltimate = false;
    int actingPlayer = snap.combatActingPlayer;
    if (actingPlayer >= 0 && actingPlayer < snap.playerCount) {
        int artifactCount = 0;
        InventoryData* inv = &snap.players[actingPlayer].inventory;
        int i = 0;
        while (i < inventorySlots) {
            int wid = inv->slots[i];
            if (wid != noWeapon) {
                if (wid == 0 || wid == 1 || wid == 8) artifactCount++;
                i += weaponTable[wid].slotSize;
            } 
            else {
                i++;
            }
        }
        canUltimate = (artifactCount >= 2);
    }

    int sel = snap.combatSelectedAction;
    for (int i = 0; i < 7; i++) {
        
        bool isUlt = (i == actionUltimate);
        Color textColor;
        if (i == sel)
            textColor = isUlt ? (canUltimate ? Color::Yellow : Color(100, 100, 0)) : Color::Yellow;
        else
            textColor = isUlt ? (canUltimate ? Color(200, 150, 255) : Color(80, 60, 100)) : Color::White;

        Text item;
        item.setFont(font);
        item.setString(actionNames[i]);
        item.setCharacterSize(17);
        item.setFillColor(textColor);
        item.setPosition(boxX + 44, boxY + 38 + i * 30);
        window.draw(item);

        if (i == sel) {
            Text arrow;
            arrow.setFont(font);
            arrow.setString(">");
            arrow.setCharacterSize(17);
            arrow.setFillColor(textColor);
            arrow.setPosition(boxX + 22, boxY + 38 + i * 30);
            window.draw(arrow);
        }
    }

    Text hint;
    hint.setFont(font);
    hint.setString("Up/Down: Select   Enter: Confirm   Esc: Back");
    hint.setCharacterSize(11);
    hint.setFillColor(Color(140, 140, 140));
    hint.setPosition(boxX + 10, boxY + boxH - 22);
    window.draw(hint);
}

void Renderer::drawSwapInMenu() {

    int actingPlayer = snap.combatActingPlayer;
    if (actingPlayer < 0 || actingPlayer >= snap.playerCount)
        return;

    InventoryData* inv = &snap.players[actingPlayer].inventory;
    int count = inv->longTermStorageCount;

    float boxW = 280, boxH = 60 + count * 30 + 30;
    if (boxH < 110) boxH = 110;
    float boxX = windowWidth / 2.0 - boxW / 2.0;
    float boxY = windowHeight / 2.0 - boxH / 2.0 - 40;

    RectangleShape bg(Vector2f(boxW, boxH));
    bg.setFillColor(Color(10, 10, 30, 235));
    bg.setOutlineColor(Color(120, 200, 120));
    bg.setOutlineThickness(2.0);
    bg.setPosition(boxX, boxY);
    window.draw(bg);

    Text title;
    title.setFont(font);
    title.setString("SWAP IN FROM STORAGE");
    title.setCharacterSize(14);
    title.setFillColor(Color(160, 255, 160));
    title.setPosition(boxX + 14, boxY + 10);
    window.draw(title);

    if (count == 0) {
        Text empty;
        empty.setFont(font);
        empty.setString("(No weapons in storage)");
        empty.setCharacterSize(14);
        empty.setFillColor(Color(180, 180, 180));
        empty.setPosition(boxX + 20, boxY + 38);
        window.draw(empty);
    } 
    else {
        int sel = snap.combatSelectedAction;
        if (sel < 0) sel = 0;
        if (sel >= count) sel = count - 1;

        for (int i = 0; i < count; i++) {
            
            int wid = inv->longTermStorage[i];
            const WeaponData& w = weaponTable[wid];
            char buf[128];
            snprintf(buf, sizeof(buf), "%s  [dmg:%d]  [size:%d]", w.name, w.damage, w.slotSize);
            
            Text item;
            item.setFont(font);
            item.setString(buf);
            item.setCharacterSize(15);
            item.setFillColor(i == sel ? Color::Yellow : Color::White);
            item.setPosition(boxX + 44, boxY + 38 + i * 28);
            window.draw(item);

            if (i == sel) {
                Text arrow;
                arrow.setFont(font);
                arrow.setString(">");
                arrow.setCharacterSize(16);
                arrow.setFillColor(Color::Yellow);
                arrow.setPosition(boxX + 22, boxY + 38 + i * 28);
                window.draw(arrow);
            }
        }
    }

    Text hint;
    hint.setFont(font);
    hint.setString("Up/Down: Select   Enter: Confirm   Esc: Back");
    hint.setCharacterSize(11);
    hint.setFillColor(Color(140, 140, 140));
    hint.setPosition(boxX + 10, boxY + boxH - 22);
    window.draw(hint);
}

void Renderer::drawWeaponSelectMenu() {

    int actingPlayer = snap.combatActingPlayer;
    if (actingPlayer < 0 || actingPlayer >= snap.playerCount)
        return;

    InventoryData* inv = &snap.players[actingPlayer].inventory;

    int weapons[inventorySlots];
    int count = 0;
    int i = 0;
    while (i < inventorySlots && count < inventorySlots) {
        
        if (inv->slots[i] == noWeapon) { 
            i++;
            continue; 
        }
        int wid = inv->slots[i];
        weapons[count++] = wid;
        i += weaponTable[wid].slotSize;
    }

    float boxW = 280, boxH = 60 + count * 30 + 30;
    if (boxH < 110) boxH = 110;
    float boxX = windowWidth / 2.0 - boxW / 2.0;
    float boxY = windowHeight / 2.0 - boxH / 2.0 - 40;

    RectangleShape bg(Vector2f(boxW, boxH));
    bg.setFillColor(Color(10, 10, 30, 235));
    bg.setOutlineColor(Color(200, 160, 60));
    bg.setOutlineThickness(2);
    bg.setPosition(boxX, boxY);
    window.draw(bg);

    Text title;
    title.setFont(font);
    title.setString("SELECT WEAPON");
    title.setCharacterSize(14);
    title.setFillColor(Color(255, 200, 80));
    title.setPosition(boxX + 14, boxY + 10);
    window.draw(title);

    if (count == 0) {
        Text empty;
        empty.setFont(font);
        empty.setString("(No weapons in inventory)");
        empty.setCharacterSize(14);
        empty.setFillColor(Color(180, 180, 180));
        empty.setPosition(boxX + 20, boxY + 38);
        window.draw(empty);
    } 
    else {
        int sel = snap.combatSelectedAction;
        if (sel < 0) sel = 0;
        if (sel >= count) sel = count - 1;

        for (int j = 0; j < count; j++) {
            int wid = weapons[j];
            const char* wname = (wid >= 0 && wid < weaponCount) ? weaponTable[wid].name : "?";
            char buf[64];
            snprintf(buf, 64, "%s [dmg:%d] [space:%d]", wname, 
                (wid >= 0 && wid < weaponCount) ? weaponTable[wid].damage : 0,
                (wid >= 0 && wid < weaponCount) ? weaponTable[wid].slotSize : 0);

            Text item;
            item.setFont(font);
            item.setString(buf);
            item.setCharacterSize(15);
            item.setFillColor(j == sel ? Color::Yellow : Color::White);
            item.setPosition(boxX + 44, boxY + 38 + j * 28);
            window.draw(item);

            if (j == sel) {
                Text arrow;
                arrow.setFont(font);
                arrow.setString(">");
                arrow.setCharacterSize(15);
                arrow.setFillColor(Color::Yellow);
                arrow.setPosition(boxX + 22, boxY + 38 + j * 28);
                window.draw(arrow);
            }
        }
    }

    Text hint;
    hint.setFont(font);
    hint.setString("Up/Down: Select   Enter: Use   Esc: Back");
    hint.setCharacterSize(11);
    hint.setFillColor(Color(140, 140, 140));
    hint.setPosition(boxX + 10, boxY + boxH - 22);
    window.draw(hint);
}

void Renderer::drawEclipseRelicPickup() {
    
    float boxW = 400, boxH = 180;
    float boxX = windowWidth / 2.0 - boxW / 2.0;
    float boxY = windowHeight / 2.0 - boxH / 2.0 - 40;

    RectangleShape bg(Vector2f(boxW, boxH));
    bg.setFillColor(Color(15, 10, 35, 240));
    bg.setOutlineColor(Color(180, 80, 255));
    bg.setOutlineThickness(2);
    bg.setPosition(boxX, boxY);
    window.draw(bg);

    Text title;
    title.setFont(font);
    title.setString("Eclipse Relic Appeared!");
    title.setCharacterSize(16);
    title.setFillColor(Color(200, 120, 255));
    title.setPosition(boxX + 18, boxY + 10);
    window.draw(title);

    Text sub;
    sub.setFont(font);
    sub.setString("Press player number to pick it up:");
    sub.setCharacterSize(13);
    sub.setFillColor(Color(200, 200, 200));
    sub.setPosition(boxX + 18, boxY + 38);
    window.draw(sub);

    //showing alive player numbers:
    float px = boxX + 18;
    for (int i = 0; i < snap.playerCount; i++) {
        if (!snap.players[i].entity.isAlive) continue;

        char buf[16];
        snprintf(buf, 16, "[%d] P%d", i + 1, i + 1);
        Text pt;
        pt.setFont(font);
        pt.setString(buf);
        pt.setCharacterSize(15);
        pt.setFillColor(Color(255, 220, 80));
        pt.setPosition(px, boxY + 68);
        window.draw(pt);
        px += 80.f;
    }

    Text esc;
    esc.setFont(font);
    esc.setString("ESC: Leave (enemy picks it up)");
    esc.setCharacterSize(12);
    esc.setFillColor(Color(160, 160, 160));
    esc.setPosition(boxX + 18, boxY + boxH - 28);
    window.draw(esc);
}

void Renderer::drawMainMenu() {
    if (backgroundTexture.getSize().x > 0)
        window.draw(backgroundSprite);

    RectangleShape overlay(Vector2f((float)windowWidth, (float)windowHeight));
    overlay.setFillColor(Color(0, 0, 0, 140));
    window.draw(overlay);

    Text title;
    title.setFont(font);
    title.setString("CHRONO  RIFT");
    title.setCharacterSize(64);
    title.setFillColor(Color(255, 210, 120));
    FloatRect titleBounds = title.getLocalBounds();
    title.setOrigin(titleBounds.left + titleBounds.width / 2.0, titleBounds.top + titleBounds.height / 2.0);
    title.setPosition(windowWidth / 2.0, 200);
    window.draw(title);

    Text sub;
    sub.setFont(font);
    sub.setString("A Multi-Process Tactical RPG");
    sub.setCharacterSize(22);
    sub.setFillColor(Color(210, 180, 140));
    FloatRect subBounds = sub.getLocalBounds();
    sub.setOrigin(subBounds.left + subBounds.width / 2.0, subBounds.top + subBounds.height / 2.0);
    sub.setPosition(windowWidth / 2.0, 260);
    window.draw(sub);

    const char* labels[2] = { "New Game", "Exit" };
    for (int i = 0; i < 2; i++) {
        Text item;
        item.setFont(font);
        item.setString(labels[i]);
        item.setCharacterSize(34);
        item.setFillColor(i == menuSelected ? Color(255, 200, 120) : Color(200, 200, 200));
        FloatRect itemBounds = item.getLocalBounds();
        item.setOrigin(itemBounds.left + itemBounds.width / 2.0, itemBounds.top + itemBounds.height / 2.0);
        float itemY = 380.0 + i * 60.0;
        item.setPosition(windowWidth / 2.0, itemY);
        window.draw(item);

        if (i == menuSelected) {
            Text arrow;
            arrow.setFont(font);
            arrow.setString(">");
            arrow.setCharacterSize(34);
            arrow.setFillColor(Color(255, 200, 120));
            FloatRect arrowBounds = arrow.getLocalBounds();
            arrow.setOrigin(arrowBounds.left + arrowBounds.width / 2.0, arrowBounds.top + arrowBounds.height / 2.0);
            arrow.setPosition(windowWidth / 2.0 - itemBounds.width / 2.0 - 26.0, itemY);
            window.draw(arrow);
        }
    }

    Text hint;
    hint.setFont(font);
    hint.setString("Arrow Keys: Navigate    Enter: Confirm");
    hint.setCharacterSize(18);
    hint.setFillColor(Color(170, 170, 170));
    FloatRect hintBounds = hint.getLocalBounds();
    hint.setOrigin(hintBounds.left + hintBounds.width / 2.0, hintBounds.top + hintBounds.height / 2.0);
    hint.setPosition(windowWidth / 2.0, windowHeight - 60.0);
    window.draw(hint);
}

void Renderer::drawPartySelect() {
    if (backgroundTexture.getSize().x > 0)
        window.draw(backgroundSprite);

    RectangleShape overlay(Vector2f((float)windowWidth, (float)windowHeight));
    overlay.setFillColor(Color(0, 0, 0, 140));
    window.draw(overlay);

    Text title;
    title.setFont(font);
    title.setString("Select Party Size");
    title.setCharacterSize(42);
    title.setFillColor(Color(255, 210, 120));
    FloatRect titleBounds = title.getLocalBounds();
    title.setOrigin(titleBounds.left + titleBounds.width / 2.0, titleBounds.top + titleBounds.height / 2.0);
    title.setPosition(windowWidth / 2.0, 220);
    window.draw(title);

    for (int i = 0; i < maxPlayers; i++) {
        char buf[32];
        snprintf(buf, 32, "%d Player%s", i + 1, i == 0 ? "" : "s");

        Text item;
        item.setFont(font);
        item.setString(buf);
        item.setCharacterSize(30);
        item.setFillColor(i == partySelected
            ? Color(255, 200, 120) : Color(200, 200, 200));
        FloatRect itemBounds = item.getLocalBounds();
        item.setOrigin(itemBounds.left + itemBounds.width / 2.0, itemBounds.top + itemBounds.height / 2.0);
        float itemY = 340.0 + i * 60.0;
        item.setPosition(windowWidth / 2.0, itemY);
        window.draw(item);

        if (i == partySelected) {
            Text arrow;
            arrow.setFont(font);
            arrow.setString(">");
            arrow.setCharacterSize(30);
            arrow.setFillColor(Color(255, 200, 120));
            FloatRect arrowBounds = arrow.getLocalBounds();
            arrow.setOrigin(arrowBounds.left + arrowBounds.width / 2.0, arrowBounds.top + arrowBounds.height / 2.0);
            arrow.setPosition(windowWidth / 2.0 - itemBounds.width / 2.0 - 26, itemY);
            window.draw(arrow);
        }
    }

    Text hint;
    hint.setFont(font);
    hint.setString("Arrow Keys: Navigate    Enter: Confirm    Escape: Back");
    hint.setCharacterSize(18);
    hint.setFillColor(Color(170, 170, 170));
    FloatRect hintBounds = hint.getLocalBounds();
    hint.setOrigin(hintBounds.left + hintBounds.width / 2.0, hintBounds.top + hintBounds.height / 2.0);
    hint.setPosition(windowWidth / 2.0, windowHeight - 60);
    window.draw(hint);
}

void Renderer::handleBattleInput(Event& event) {

    if (event.type != Event::KeyPressed) 
        return;
    
    sem_wait(&sharedState->keyboardInput.inputLock);

    sharedState->keyboardInput.isUp = false;
    sharedState->keyboardInput.isDown = false;
    sharedState->keyboardInput.isLeft = false;
    sharedState->keyboardInput.isRight = false;
    sharedState->keyboardInput.isConfirm = false;
    sharedState->keyboardInput.isBack = false;
    sharedState->keyboardInput.isEscape = false;

    switch (event.key.code) {
        case Keyboard::W:
        case Keyboard::Up:     
            sharedState->keyboardInput.isUp = true; 
            break;

        case Keyboard::S:
        case Keyboard::Down:   
            sharedState->keyboardInput.isDown = true; 
            break;

        case Keyboard::A:
        case Keyboard::Left:   
            sharedState->keyboardInput.isLeft = true; 
            break;

        case Keyboard::D:
        case Keyboard::Right:  
            sharedState->keyboardInput.isRight = true; 
            break;

        case Keyboard::Return: 
            sharedState->keyboardInput.isConfirm = true; 
            break;
            
        case Keyboard::Escape: 
            sharedState->keyboardInput.isBack = true;
            sharedState->keyboardInput.isEscape = true; 
            break;

        case Keyboard::Tab:    
            debugOverlay = !debugOverlay;               
            break;

        default: 
            break;
    }

    for (int i = 0; i < maxEntities; i++)
        sharedState->keyboardInput.consumed[i] = false;
    
    sharedState->keyboardInput.newInput = true;
    sem_post(&sharedState->keyboardInput.inputLock);
}

void Renderer::captureSnapshot() {
    
    sem_wait(&sharedState->shmLock);

    snap.waveNumber = sharedState->waveNumber;
    snap.enemiesDefeated = sharedState->enemiesDefeated;
    snap.playersAlive = sharedState->playersAlive;
    snap.playerCount = sharedState->playerCount;
    snap.enemyCount = sharedState->enemyCount;

    for (int i = 0; i < maxPlayers; i++) snap.players[i] = sharedState->players[i];
    for (int i = 0; i < maxEnemies; i++) snap.enemies[i] = sharedState->enemies[i];

    snap.gamePaused = sharedState->gamePaused;
    snap.waveTransition = sharedState->waveTransition;
    snap.gameOver = sharedState->gameOver;
    snap.playerWon = sharedState->playerWon;
    snap.gameReady = sharedState->gameReady;
    snap.escapeMenuOpen = sharedState->escapeMenuOpen;
    snap.weaponDropActive = sharedState->weaponDropActive;
    snap.weaponDropId = sharedState->weaponDropId;
    snap.weaponDropOwner = sharedState->weaponDropOwner;
    snap.currentTurnEntityId = sharedState->currentTurnEntityId;
    snap.combatPhase = sharedState->combatPhase;
    snap.combatActingPlayer = sharedState->combatActingPlayer;
    snap.combatSelectedTarget = sharedState->combatSelectedTarget;
    snap.combatSelectedAction = sharedState->combatSelectedAction;
    snap.sequence = sharedState->sequence;
    snap.artifacts = sharedState->artifacts;

    snap.eclipseRelicPickupActive = sharedState->eclipseRelicPickupActive;

    snap.teleportActive = sharedState->teleportActive;
    if (snap.teleportActive) {
        snap.teleportAttackerId = sharedState->teleportAttackerId;
        snap.teleportAttackerIsPlayer = sharedState->teleportAttackerIsPlayer;
        snap.teleportAttackerSpriteType = sharedState->teleportAttackerSpriteType;
        snap.teleportTargetId = sharedState->teleportTargetId;
        snap.teleportTargetIsPlayer = sharedState->teleportTargetIsPlayer;
        snap.teleportPlayAttack = sharedState->teleportPlayAttack;
        sharedState->teleportActive = false;
        sharedState->teleportPlayAttack = false;
        sharedState->sequence++;
    }

    sem_post(&sharedState->shmLock);

    sem_wait(&sharedState->logLock);
    memcpy(snap.log, sharedState->log, sizeof(snap.log));
    snap.logCount = sharedState->logCount;
    snap.logHead = sharedState->logHead;
    sem_post(&sharedState->logLock);
}

void Renderer::loop() {
    if (!initialize()) {
        fprintf(stderr, "[renderer] init failed\n");
        return;
    }

    printf("[renderer] Entering main loop\n");
    fflush(stdout);

    menuSelected = 0;
    partySelected = 1;

    Clock frameClock;

    while (window.isOpen() && !sharedState->shouldExit) {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) {
                sharedState->shouldExit = true;
                window.close();
                return;
            }
            
            if (event.type == Event::KeyPressed) {
                if (sharedState->isBattle) {
                    if (event.key.code == Keyboard::Tab) debugOverlay = !debugOverlay;
                    if (event.key.code == Keyboard::D) showActionLog = !showActionLog;
                }
            }
        }

        //reading shared keyboard input for menus / escape menu
        if (sharedState->isMainMenu || sharedState->isPartySelect) {
            sem_wait(&sharedState->keyboardInput.inputLock);
            bool up = sharedState->keyboardInput.isUp;
            bool down = sharedState->keyboardInput.isDown;
            bool confirm = sharedState->keyboardInput.isConfirm;
            bool back = sharedState->keyboardInput.isBack;
            bool newInput = sharedState->keyboardInput.newInput;
            
            if (newInput) sharedState->keyboardInput.newInput = false;   
            sem_post(&sharedState->keyboardInput.inputLock);

            if (!newInput) {
                //nothing to do
            }
            else if (sharedState->isMainMenu) {
                
                if (up) menuSelected = (menuSelected - 1 + 2) % 2;
                if (down) menuSelected = (menuSelected + 1) % 2;
                if (confirm) {
                    if (menuSelected == 0) {
                        sem_wait(&sharedState->shmLock);
                        sharedState->isMainMenu = false;
                        sharedState->isPartySelect = true;
                        sem_post(&sharedState->shmLock);
                        partySelected = 1;
                    } 
                    else {
                        sharedState->shouldExit = true;
                        kill(sharedState->arbiterPid, SIGTERM);
                        window.close();
                        return;
                    }
                }
            }
            else if (sharedState->isPartySelect) {
                
                if (up) partySelected = (partySelected - 1 + maxPlayers) % maxPlayers;
                if (down) partySelected = (partySelected + 1) % maxPlayers;
                if (confirm) {
                    int count = partySelected + 1;
                    sem_wait(&sharedState->shmLock);
                    sharedState->playerCount = count;
                    sharedState->isPartySelect = false;
                    sharedState->isBattle = true;
                    sem_post(&sharedState->shmLock);
                }
                if (back) {
                    sem_wait(&sharedState->shmLock);
                    sharedState->isPartySelect = false;
                    sharedState->isMainMenu = true;
                    sem_post(&sharedState->shmLock);
                    menuSelected = 0;
                }
            }
        }

        if (sharedState->escapeMenuOpen) {
            sem_wait(&sharedState->keyboardInput.inputLock);
            bool up = sharedState->keyboardInput.isUp;
            bool down = sharedState->keyboardInput.isDown;
            bool confirm = sharedState->keyboardInput.isConfirm;
            bool back = sharedState->keyboardInput.isBack;
            bool newInput = sharedState->keyboardInput.newInput;
            
            if (newInput) sharedState->keyboardInput.newInput = false;
            sem_post(&sharedState->keyboardInput.inputLock);

            if (newInput) {
                
                if (up) escapeMenuSelection = (escapeMenuSelection - 1 + 2) % 2;
                if (down) escapeMenuSelection = (escapeMenuSelection + 1) % 2;
                if (confirm) {
                    sem_wait(&sharedState->shmLock);
                    sharedState->escapeMenuOpen = false;
                    sharedState->gamePaused = false;

                    if (escapeMenuSelection == 1) sharedState->shouldExit = true;
                    sem_post(&sharedState->shmLock);
                    escapeMenuSelection = 0;
                } 
                else if (back) {
                    sem_wait(&sharedState->shmLock);
                    sharedState->escapeMenuOpen = false;
                    sharedState->gamePaused = false;
                    sem_post(&sharedState->shmLock);
                    escapeMenuSelection = 0;
                }
            }
        }

        float dt = frameClock.restart().asSeconds();

        if (sharedState->returnToMenu) {
            animationManager.stopAll();
            for (int i = 0; i < maxParticles; i++) particles[i].active = false;
            teleport.active = false;
            for (int i = 0; i < maxEntities; i++) {
                idleConditions[i] = true;
                animStates[i].isIdle = true;
                animStates[i].isAttacking = false;
                animStates[i].isDead = false;
                animStates[i].attackTriggered = false;
                animStates[i].deathTriggered = false;
            }
            menuSelected = 0;
            partySelected = 1;
            escapeMenuSelection = 0;
            while (sharedState->returnToMenu && !sharedState->shouldExit)
                usleep(16000);
        }

        window.clear(Color(10, 10, 20));

        if (sharedState->isMainMenu) {
            drawMainMenu();
        }
        else if (sharedState->isPartySelect) {
            drawPartySelect();
        }
        else if (sharedState->isBattle) {
            if (!isGameReadyForDrawing()) {
                Text loading;
                loading.setFont(font);
                loading.setString("Initializing battle...");
                loading.setCharacterSize(24);
                loading.setFillColor(Color::White);
                loading.setPosition(windowWidth / 2.0 - 110, windowHeight / 2.0);
                window.draw(loading);
            } 
            else {
                captureSnapshot();

                if (snap.teleportActive)
                    triggerTeleport(snap.teleportAttackerId, snap.teleportAttackerIsPlayer,
                                    snap.teleportAttackerSpriteType, snap.teleportTargetId,
                                    snap.teleportTargetIsPlayer, snap.teleportPlayAttack);

                updateAnimStates();
                updateTeleport(dt);
                updateParticles(dt);
                animationManager.update(dt);

                drawBackground();
                drawCenterPanel();
                drawHUD();
                drawPlayers();
                drawEnemies();
                animationManager.draw(window);
                drawTeleport();
                drawParticles();
                drawCombatUI();
                if (showActionLog) drawActionLog();

                if (snap.eclipseRelicPickupActive) drawEclipseRelicPickup();
                if (snap.escapeMenuOpen) drawEscapeMenu();
                if (snap.waveTransition)  drawWaveTransition();
                if (snap.gameOver) {
                    if (snap.playerWon) drawWinScreen();
                    else drawLoseScreen();
                }
                if (debugOverlay) drawDebugOverlay();
            }
        }

        window.display();
        sleep(milliseconds(10));
    }

    printf("[renderer] Exiting main loop\n");
    fflush(stdout);
}

void* Renderer::threadFunc(void* arg) {
    Renderer* renderer = (Renderer*)arg;
    renderer->loop();
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
// === GAME ARBITER IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

GameArbiter* GameArbiter::instance = nullptr;

void signal_handler(int sig) {
    fprintf(stderr, "Signal %d caught\n", sig);
    exit(1);
}

GameArbiter::GameArbiter(int rollNumber) {
    rollNo = rollNumber;
    sharedState = nullptr;
    shmId = -1;
    hip1Pid = -1;
    hip2Pid = -1;
    aspPid = -1;
    running = false;
    staminaTimer = nullptr;
    deadlockMonitor = nullptr;
    turnScheduler = nullptr;
    inventoryAllocator = nullptr;
    renderer = nullptr;
    staminaThread = 0;
    deadlockThread = 0;
    renderThread = 0;
    gameLoopThread = 0;
    initThread = 0;
    monitorThread = 0;
    instance = this;
}

GameArbiter::~GameArbiter() {
    cleanup();
}

bool GameArbiter::initialize() {
    
    struct sigaction sa_crash = {};
    sigemptyset(&sa_crash.sa_mask);
    sa_crash.sa_flags = SA_RESTART;
    sa_crash.sa_handler = signal_handler;
    sigaction(SIGSEGV, &sa_crash, nullptr);
    sigaction(SIGABRT, &sa_crash, nullptr);

    printf("[arbiter] initialize() started\n");
    fflush(stdout);

    srand(rollNo);

    shmId = initializeSharedMemory(&sharedState);
    if (shmId < 0) {
        fprintf(stderr, "[arbiter] shm creation failed\n");
        return false;
    }

    sharedState->arbiterPid = getpid();
    setupSignals();

    staminaTimer = new StaminaTimer(sharedState);
    inventoryAllocator = new InventoryAllocator(sharedState);
    deadlockMonitor = new DeadlockMonitor(sharedState, inventoryAllocator);
    turnScheduler = new TurnScheduler(sharedState);
    renderer = new Renderer(sharedState);

    logAction(sharedState, "arbiter init complete", getpid(), 0);
    printf("[arbiter] initialize() done -- renderer will run on main thread\n");
    fflush(stdout);
    return true;
}

bool GameArbiter::spawnASP() {
    if (aspPid > 0) {
        if (kill(aspPid, 0) == 0) {
            printf("[arbiter] ASP already running with pid %d\n", aspPid);
            return true;
        }
    }
    
    pid_t pid = fork();
    if (pid < 0) { 
        perror("[arbiter] fork asp"); 
        return false; 
    }
    
    if (pid == 0) {
        printf("[ASP] process starting...\n");
        fflush(stdout);
        execl("./asps", "asps", NULL);
        perror("[arbiter] execl asps");
        exit(1);
    }

    aspPid = pid;
    sharedState->aspPid = pid;
    printf("[arbiter] ASP spawned with pid %d\n", aspPid);
    fflush(stdout);
    logAction(sharedState, "ASP spawned", getpid(), 0);
    return true;
}

bool GameArbiter::spawnHIP(int hipNumber) {
    pid_t existingPid = (hipNumber == 1) ? hip1Pid : hip2Pid;
    if (existingPid > 0) {
        if (kill(existingPid, 0) == 0) {
            printf("[arbiter] HIP%d already running with pid %d\n", hipNumber, existingPid);
            return true;
        }
    }
    
    pid_t pid = fork();
    if (pid < 0) { 
        perror("[arbiter] fork hip"); 
        return false; 
    }
    
    if (pid == 0) {
        printf("[HIP%d] process starting...\n", hipNumber);
        fflush(stdout);
        execl("./hips", "hips", NULL);
        perror("[arbiter] execl hips");
        exit(1);
    }
    
    if (hipNumber == 1) {
        hip1Pid = pid;
        sharedState->hip1Pid = pid;
        printf("[arbiter] HIP1 spawned with pid %d\n", hip1Pid);
    } 
    else {
        hip2Pid = pid;
        sharedState->hip2Pid = pid;
        printf("[arbiter] HIP2 spawned with pid %d\n", hip2Pid);
    }
    fflush(stdout);
    logAction(sharedState, hipNumber == 1 ? "HIP1 spawned" : "HIP2 spawned", getpid(), 0);
    return true;
}

void GameArbiter::killChildren() {
    if (hip1Pid > 0) { 
        kill(hip1Pid, SIGTERM); 
        hip1Pid = -1; 
    }

    if (hip2Pid > 0) { 
        kill(hip2Pid, SIGTERM); 
        hip2Pid = -1; 
    }

    if (aspPid  > 0) { 
        kill(aspPid, SIGTERM); 
        aspPid = -1; 
    }
}

void GameArbiter::waitForChildren() {
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);
}

bool GameArbiter::startStaminaThread() {
    if (pthread_create(&staminaThread, NULL, staminaThreadFunc, this) != 0) {
        perror("[arbiter] stamina thread");
        return false;
    }
    return true;
}

bool GameArbiter::startDeadlockThread() {
    if (pthread_create(&deadlockThread, NULL, deadlockThreadFunc, this) != 0) {
        perror("[arbiter] deadlock thread");
        return false;
    }
    return true;
}

bool GameArbiter::startRenderThread() {
    if (pthread_create(&renderThread, NULL, renderThreadFunc, this) != 0) {
        perror("[arbiter] render thread"); 
        return false;
    }
    pthread_detach(renderThread);
    return true;
}

void GameArbiter::initializeEntities() {
    printf("[arbiter] initializeEntities: starting\n");
    fflush(stdout);
    
    int pCount = sharedState->playerCount;
    printf("[arbiter] initializeEntities: playerCount = %d\n", pCount);
    fflush(stdout);
    
    for (int i = 0; i < pCount; i++) {
        printf("[arbiter] initializeEntities: initializing player %d\n", i);
        fflush(stdout);
        
        PlayerData* player = &sharedState->players[i];
        EntityData* entity = &player->entity;
        
        entity->id = i;
        snprintf(entity->name, 32, "Player%d", i + 1);
        entity->currentHp = generatePlayerHp(rollNo);
        entity->maxHp = entity->currentHp;
        
        printf("[arbiter] initializeEntities: player %d - HP = %d\n", i, entity->maxHp);
        fflush(stdout);
        
        entity->maxStamina = playerMaxStamina;
        entity->currentStamina = 0;
        entity->speed = generatePlayerSpeed(pCount);
        
        printf("[arbiter] initializeEntities: player %d - speed = %d\n", i, entity->speed);
        fflush(stdout);
        
        entity->damage = generatePlayerDamage(rollNo);
        
        printf("[arbiter] initializeEntities: player %d - damage = %d\n", i, entity->damage);
        fflush(stdout);
        
        entity->isAlive = true;
        entity->isStunned = false;
        entity->hasTurn = false;
        entity->actionReady = false;
        entity->pid = -1;
        entity->threadId = -1;

        player->actionType = actionSkip;
        player->actionTarget = noTarget;
        player->weaponUsed = noWeapon;
        player->hipOwner = 0;
        player->spriteType = i % 4;

        printf("[arbiter] initializeEntities: player %d - complete\n", i);
        fflush(stdout);
    }

    sharedState->readyQueueHead = 0;
    sharedState->readyQueueTail = 0;
    sharedState->readyQueueCount = 0;
    for (int i = 0; i < maxEntities; i++) {
        sharedState->readyQueue[i] = noEntity;
        sharedState->inReadyQueue[i] = false;
    }

    sharedState->combatPhase = 0;
    sharedState->combatActingPlayer = -1;
    sharedState->combatSelectedTarget = 0;
    sharedState->combatSelectedAction = 0;

    sharedState->playersAlive = pCount;
    printf("[arbiter] initializeEntities: complete, playersAlive=%d\n", sharedState->playersAlive);
    fflush(stdout);
}

void GameArbiter::initializeWave() {
    printf("[arbiter] initializeWave: starting\n");
    fflush(stdout);
    
    int nCount = generateEnemyCount();
    printf("[arbiter] initializeWave: enemyCount = %d\n", nCount);
    fflush(stdout);

    sharedState->enemyCount = nCount;
    sharedState->totalEnemiesThisWave = nCount;
    sharedState->enemiesAlive = nCount;

    sharedState->readyQueueHead = 0;
    sharedState->readyQueueTail = 0;
    sharedState->readyQueueCount = 0;
    for (int i = 0; i < maxEntities; i++) {
        sharedState->readyQueue[i] = noEntity;
        sharedState->inReadyQueue[i] = false;
    }

    int spriteAssign[maxEnemies];
    for (int i = 0; i < nCount; i++) spriteAssign[i] = i % 3;
    for (int i = nCount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = spriteAssign[i]; spriteAssign[i] = spriteAssign[j]; spriteAssign[j] = tmp;
    }

    printf("[arbiter] initializeWave: looping to create %d enemies\n", nCount);
    fflush(stdout);

    for (int i = 0; i < nCount; i++) {
        printf("[arbiter] initializeWave: creating enemy %d\n", i);
        fflush(stdout);
        
        EnemyData* enemy = &sharedState->enemies[i];
        EntityData* entity = &enemy->entity;
        
        entity->id = maxPlayers + i;
        snprintf(entity->name, 32, "Enemy%d", i + 1);
        entity->maxHp = generateEnemyHp(rollNo);
        entity->currentHp = entity->maxHp;
        
        printf("[arbiter] initializeWave: enemy %d - setting stamina\n", i);
        entity->maxStamina = enemyMaxStamina;
        entity->currentStamina = 0;
        
        entity->speed = generateEnemySpeed();
        entity->damage = generateEnemyDamage(rollNo);
        
        printf("[arbiter] initializeWave: enemy %d - damage = %d\n", i, entity->damage);
        fflush(stdout);
        
        entity->isAlive = true;
        entity->isStunned = false;
        entity->hasTurn = false;
        entity->actionReady = false;
        entity->pid = -1;
        entity->threadId = -1;
        
        enemy->weaponId = noWeapon;
        enemy->actionType = actionEnemyStrike;
        enemy->actionTarget = noTarget;
        enemy->spriteType = spriteAssign[i];

        printf("[arbiter] initializeWave: enemy %d complete\n", i);
        fflush(stdout);
    }
    
    printf("[arbiter] initializeWave: complete, enemyCount=%d\n", sharedState->enemyCount);
    fflush(stdout);
}

void GameArbiter::grantTurn(int entityId) {
    sem_wait(&sharedState->shmLock);
    sharedState->currentTurnEntityId = entityId;

    if (entityId < maxPlayers) {
        sharedState->players[entityId].entity.hasTurn = true;
        sharedState->players[entityId].entity.actionReady = false;
        sharedState->players[entityId].actionType = actionSkip;
        sharedState->players[entityId].actionTarget = noTarget;
        sharedState->players[entityId].weaponUsed = noWeapon;
    }
    else {
        int idx = entityId - maxPlayers;
        sharedState->enemies[idx].entity.hasTurn = true;
        sharedState->enemies[idx].entity.actionReady = false;
        sharedState->enemies[idx].actionType = actionEnemySkip;
        sharedState->enemies[idx].actionTarget = noTarget;
    }

    sharedState->sequence++;
    sem_post(&sharedState->shmLock);
    sem_post(&sharedState->turnSem);

    char msg[64];
    snprintf(msg, 64, "turn granted to entity %d", entityId);
    logAction(sharedState, msg, getpid(), 0);
}

void* GameArbiter::handleEnemyTimeout(void* arg) {
    
    TimeoutArgs* ta = (TimeoutArgs*)arg;
    SharedState* s = ta->state;
    int eid = ta->entityId;
    delete ta; 

    sleep(enemyTimeout);

    sem_wait(&s->shmLock);
    if (s->currentTurnEntityId == eid && eid >= maxPlayers) {
        int idx = eid - maxPlayers;
        if (!s->enemies[idx].entity.actionReady) {
            s->enemies[idx].actionType = actionEnemySkip;
            s->enemies[idx].entity.actionReady = true;
            sem_post(&s->actionSem);
        }
    }
    sem_post(&s->shmLock);
    return nullptr;
}

bool GameArbiter::waitForAction() {
 
    int val;
    while (sem_getvalue(&sharedState->actionSem, &val) == 0 && val > 0)
        sem_trywait(&sharedState->actionSem);
    
    if (sharedState->shouldExit) return false;

    int entityId = sharedState->currentTurnEntityId;
    bool isEnemy = (entityId >= maxPlayers);

    if (isEnemy) {
        pthread_t timeoutThread;
        TimeoutArgs* args = new TimeoutArgs{sharedState, entityId};
        
        pthread_create(&timeoutThread, NULL, handleEnemyTimeout, args);
        pthread_detach(timeoutThread);
    }

    const struct timespec ts = {0, 100000000}; 
    while (!sharedState->shouldExit) {
        if (sharedState->gamePaused) {
            usleep(50000);
            continue;
        }
        
        if (sem_timedwait(&sharedState->actionSem, &ts) == 0) {
            return true; 
        }
    }
    return false;
}

void GameArbiter::commitAction(int entityId) {
    
    sem_wait(&sharedState->shmLock);
    if (sharedState->gamePaused || sharedState->waveTransition || sharedState->gameOver) {
        
        turnScheduler->clearTurnFlags(entityId);
        sharedState->currentTurnEntityId = noEntity;
        sharedState->sequence++;
        sem_post(&sharedState->shmLock);
        logAction(sharedState, "Action skipped because game is paused", getpid(), 0);
        return;
    }

    bool ready = false;
    if (entityId < maxPlayers)
        ready = sharedState->players[entityId].entity.actionReady;
    else {
        int idx = entityId - maxPlayers;
        if (idx < sharedState->enemyCount)
            ready = sharedState->enemies[idx].entity.actionReady;
    }

    if (!ready) {
        sem_post(&sharedState->shmLock);
        logAction(sharedState, "commitAction: actionReady false -- re-waiting", getpid(), 0);
        sem_wait(&sharedState->actionSem);
        sem_wait(&sharedState->shmLock);
    }

    if (entityId < maxPlayers)
        handlePlayerAction(entityId);
    else
        handleEnemyAction(entityId - maxPlayers);

    sharedState->currentTurnEntityId = noEntity;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);

    turnScheduler->clearTurnFlags(entityId);
}

void GameArbiter::handlePlayerAction(int playerIndex) {
    PlayerData* player = &sharedState->players[playerIndex];
    EntityData* entity = &player->entity;

    switch (player->actionType) {
        case actionStrike: {
            int ti = player->actionTarget;

            if (ti < 0 || ti >= sharedState->enemyCount || !sharedState->enemies[ti].entity.isAlive) {
                for (int j = 0; j < sharedState->enemyCount; j++) {
                    if (sharedState->enemies[j].entity.isAlive) { 
                        ti = j; 
                        break; 
                    }
                }
            }

            bool enemyKilled = false;
            if (ti >= 0 && ti < sharedState->enemyCount && sharedState->enemies[ti].entity.isAlive) {
                sharedState->enemies[ti].entity.currentHp -= entity->damage;

                if (sharedState->enemies[ti].entity.currentHp <= 0) {
                    sharedState->enemies[ti].entity.currentHp = 0;
                    sharedState->enemies[ti].entity.isAlive = false;
                    sharedState->enemiesAlive--;
                    sharedState->enemiesDefeated++;
                    enemyKilled = true;
                }
            }

            entity->currentStamina = 0;

            if (ti >= 0 && ti < sharedState->enemyCount) {
                sharedState->teleportActive = true;
                sharedState->attackAnimationInProgress = true;
                sharedState->teleportAttackerId = playerIndex;
                sharedState->teleportAttackerIsPlayer = true;
                sharedState->teleportAttackerSpriteType = 0;
                sharedState->teleportTargetId = maxPlayers + ti;
                sharedState->teleportTargetIsPlayer = false;
                sharedState->teleportPlayAttack = false;
            }

            char msg[128];
            snprintf(msg, 128, "P%d Strike->E%d dmg:%d", playerIndex, ti, entity->damage);
            logAction(sharedState, msg, entity->pid, entity->threadId);

            if (enemyKilled) triggerWeaponDrop(playerIndex);
            break;
        }

        case actionExhaust: {
            int ti = player->actionTarget;
            if (ti < 0 || ti >= sharedState->enemyCount || !sharedState->enemies[ti].entity.isAlive) {
                for (int j = 0; j < sharedState->enemyCount; j++) {
                    if (sharedState->enemies[j].entity.isAlive) { 
                        ti = j; 
                        break; 
                    }
                }
            }
            if (ti >= 0 && ti < sharedState->enemyCount && sharedState->enemies[ti].entity.isAlive) {
                sharedState->enemies[ti].entity.currentStamina -= entity->damage * 5;
                if (sharedState->enemies[ti].entity.currentStamina < 0)
                    sharedState->enemies[ti].entity.currentStamina = 0;
            }
            entity->currentStamina = 0;

            //exhaust always stuns the enemy target asynchronously via SIGUSR1:
            if (ti >= 0 && ti < sharedState->enemyCount && sharedState->enemies[ti].entity.isAlive
                && !sharedState->enemies[ti].entity.isStunned) {
                sem_post(&sharedState->shmLock);
                deliverStun(maxPlayers + ti);
                sem_wait(&sharedState->shmLock);
            }

            char msg[128];
            snprintf(msg, 128, "P%d Exhaust->E%d (stun)", playerIndex, ti);
            logAction(sharedState, msg, entity->pid, entity->threadId);
            break;
        }

        case actionUseWeapon: {
            int wid = player->weaponUsed;
            int ti = player->actionTarget;
            bool enemyKilled = false;

            if (ti < 0 || ti >= sharedState->enemyCount || !sharedState->enemies[ti].entity.isAlive) {
                for (int j = 0; j < sharedState->enemyCount; j++) {
                    if (sharedState->enemies[j].entity.isAlive) { 
                        ti = j; 
                        break; 
                    }
                }
            }

            if (wid == 0 || wid == 1) {
                if (sharedState->artifacts.holder[wid] != playerIndex) {
                    sem_post(&sharedState->shmLock);
                    bool ok = tryAcquireArtifact(playerIndex, wid);
                    sem_wait(&sharedState->shmLock);

                    if (!ok) {
                        entity->currentStamina = entity->maxStamina * skipStamina / 100;
                        logAction(sharedState, "artifact busy -- skip", entity->pid, entity->threadId);
                        break;
                    }
                }
            }

            if (wid >= 0 && wid < weaponCount && ti >= 0 && ti < sharedState->enemyCount &&
                sharedState->enemies[ti].entity.isAlive) {
                
                sharedState->enemies[ti].entity.currentHp -= weaponTable[wid].damage;
                if (sharedState->enemies[ti].entity.currentHp <= 0) {
                    sharedState->enemies[ti].entity.currentHp = 0;
                    sharedState->enemies[ti].entity.isAlive = false;
                    sharedState->enemiesAlive--;
                    sharedState->enemiesDefeated++;
                    enemyKilled = true;
                }
            }

            entity->currentStamina = 0;
            if (ti >= 0 && ti < sharedState->enemyCount) {
                sharedState->teleportActive = true;
                sharedState->attackAnimationInProgress = true;
                sharedState->teleportAttackerId = playerIndex;
                sharedState->teleportAttackerIsPlayer = true;
                sharedState->teleportAttackerSpriteType = 0;
                sharedState->teleportTargetId = maxPlayers + ti;
                sharedState->teleportTargetIsPlayer = false;
                sharedState->teleportPlayAttack = true;
            }
            
            char msg[128];
            snprintf(msg, 128, "P%d UseWeapon[%s]->E%d", playerIndex, 
                wid >= 0 ? weaponTable[wid].name : "?", ti);
            logAction(sharedState, msg, entity->pid, entity->threadId);

            if (enemyKilled)
                triggerWeaponDrop(playerIndex);
            break;
        }

        case actionHeal: {
            int healAmt = entity->maxHp * healPercent / 100;
            entity->currentHp += healAmt;

            if (entity->currentHp > entity->maxHp) entity->currentHp = entity->maxHp;
            entity->currentStamina = 0;

            char msg[128];
            snprintf(msg, 128, "P%d Heal +%d hp", playerIndex, healAmt);
            logAction(sharedState, msg, entity->pid, entity->threadId);
            break;
        }

        case actionSwapIn: {
            int wid = player->weaponUsed;
            entity->currentStamina = 0;
            sem_post(&sharedState->shmLock);

            bool swapOk = false;
            if (wid >= 0 && wid < weaponCount)
                swapOk = inventoryAllocator->swapIn(playerIndex, wid);
            sem_wait(&sharedState->shmLock);
            
            char swapMsg[128];
            snprintf(swapMsg, 128, "P%d Swap In [%s] %s", playerIndex,
                (wid >= 0 && wid < weaponCount) ? weaponTable[wid].name : "?",
                swapOk ? "success" : "failed");
                logAction(sharedState, swapMsg, entity->pid, entity->threadId);
            break;
        }

        case actionSkip: {
            entity->currentStamina = entity->maxStamina * skipStamina / 100;
            logAction(sharedState, "player Skip", entity->pid, entity->threadId);
            break;
        }

        case actionUltimate: {
            
            int artifactCount = 0;
            InventoryData* inv = &sharedState->players[playerIndex].inventory;
            int i = 0;
            while (i < inventorySlots) {
                int wid = inv->slots[i];
                if (wid != noWeapon) {
                    if (wid == 0 || wid == 1 || wid == 8)
                        artifactCount++;
                    i += weaponTable[wid].slotSize;
                } 
                else {
                    i++;
                }
            }
            if (artifactCount < 2) {
                entity->currentStamina = entity->maxStamina * skipStamina / 100;
                logAction(sharedState, "Ultimate failed as 2 artifacts required", entity->pid, entity->threadId);
                break;
            }

            entity->currentStamina = 0;
            char msg[128];
            snprintf(msg, 128, "P%d Ultimate Ability used", playerIndex);
            logAction(sharedState, msg, entity->pid, entity->threadId);
            break;
        }
    }
}

void GameArbiter::handleEnemyAction(int enemyIndex) {
    EnemyData* enemy = &sharedState->enemies[enemyIndex];
    EntityData* entity = &enemy->entity;

    if (enemy->actionType == actionEnemyStrike) {
        int ti = enemy->actionTarget;
        if (ti < 0 || ti >= sharedState->playerCount || !sharedState->players[ti].entity.isAlive) {
            int minHp = -1, newTarget = -1;
            
            for (int i = 0; i < sharedState->playerCount; i++) {
                EntityData* p = &sharedState->players[i].entity;
                if (!p->isAlive) continue;
                if (newTarget == -1 || p->currentHp < minHp) {
                    minHp = p->currentHp;
                    newTarget = i;
                }
            }
            enemy->actionTarget = newTarget;
            if (newTarget < 0) enemy->actionType = actionEnemySkip;
        }
    }

    switch (enemy->actionType) {
        case actionEnemyStrike: {
            int ti = enemy->actionTarget;
            bool playerKilled = false;
            if (ti >= 0 && ti < sharedState->playerCount && sharedState->players[ti].entity.isAlive) {
                
                sharedState->players[ti].entity.currentHp -= entity->damage * 1000;

                if (sharedState->players[ti].entity.currentHp <= 0) {
                    sharedState->players[ti].entity.currentHp = 0;
                    sharedState->players[ti].entity.isAlive = false;
                    sharedState->playersAlive--;
                    playerKilled = true;
                }
            }

            entity->currentStamina = 0;
            sharedState->teleportActive = true;
            sharedState->attackAnimationInProgress = true;
            sharedState->teleportAttackerId = maxPlayers + enemyIndex;
            sharedState->teleportAttackerIsPlayer = false;
            sharedState->teleportAttackerSpriteType = enemy->spriteType;
            sharedState->teleportTargetId = enemy->actionTarget;
            sharedState->teleportTargetIsPlayer = true;
            sharedState->teleportPlayAttack = (enemy->weaponId != noWeapon);

            //25% chance of heavy enemy strike stunning the player:
            if (!playerKilled && ti >= 0 && ti < sharedState->playerCount && sharedState->players[ti].entity.isAlive
                && !sharedState->players[ti].entity.isStunned && (rand() % 4 == 0)) {
                sem_post(&sharedState->shmLock);
                deliverStun(ti);
                sem_wait(&sharedState->shmLock);
            }

            char msg[128];
            snprintf(msg, 128, "E%d Strike->P%d dmg:%d", enemyIndex, enemy->actionTarget, entity->damage);
            logAction(sharedState, msg, entity->pid, entity->threadId);
            break;
        }

        case actionEnemySkip: {
            entity->currentStamina = entity->maxStamina * skipStamina / 100;
            logAction(sharedState, "enemy Skip", entity->pid, entity->threadId);
            break;
        }
    }
}

bool GameArbiter::isUniqueWeaponInCirculation(int weaponId) {
    
    //Solar Core (0) and Lunar Blade (1) and Eclipse Relic (8) are unique so only one instance allowed:
    if (weaponId != 0 && weaponId != 1 && weaponId != 8)
        return false;

    for (int p = 0; p < sharedState->playerCount; p++) {
        InventoryData* inv = &sharedState->players[p].inventory;
        for (int s = 0; s < inventorySlots; s++) {
            if (inv->slots[s] == weaponId)
                return true;
        }
        for (int s = 0; s < inv->longTermStorageCount; s++) {
            if (inv->longTermStorage[s] == weaponId)
                return true;
        }
    }

    for (int e = 0; e < sharedState->enemyCount; e++) {
        if (sharedState->enemies[e].weaponId == weaponId)
            return true;
    }

    return false;
}

void GameArbiter::triggerWeaponDrop(int killerPlayerIndex) {
    if (sharedState->weaponDropActive)
        return;
    if (killerPlayerIndex < 0 || killerPlayerIndex >= sharedState->playerCount)
        return;

    int roll = rand() % 100;
    if (roll >= 50) return;

    int weaponId = rand() % weaponCount;

    // weaponId = rand() % 3;
    // if (weaponId == 2){
    //     weaponId = 8;
    // }
    // int tries = 10;
    // while (isUniqueWeaponInCirculation(weaponId) && tries > 0) {
    //     weaponId = rand() % 3;
    //     if (weaponId == 2){
    //         weaponId = 8;
    //     }
    //     tries--;
    // }

    // SC/LB/ER are unique so if already in any player's possession, then pick a different weapon:
    while ((weaponId == 0 || weaponId == 1 || weaponId == 8) && isUniqueWeaponInCirculation(weaponId)) {
        weaponId = rand() % weaponCount;
    }
    sharedState->weaponDropActive = true;
    sharedState->weaponDropResolved = false;
    sharedState->weaponDropAccepted = false;
    sharedState->weaponDropOwner = killerPlayerIndex;
    sharedState->weaponDropId = weaponId;
    sharedState->combatPhase = 4;
    sharedState->combatActingPlayer = killerPlayerIndex;
    sharedState->sequence++;

    char msg[128];
    snprintf(msg, 128, "Enemy dropped weapon [%s] for P%d", weaponTable[weaponId].name, killerPlayerIndex);
    logAction(sharedState, msg, getpid(), 0);
}


int GameArbiter::pickRandomAliveEnemy() {
    int alive[maxEnemies];
    int count = 0;

    sem_wait(&sharedState->shmLock);
    for (int i = 0; i < sharedState->enemyCount; i++) {
        if (sharedState->enemies[i].entity.isAlive)
            alive[count++] = i;
    }
    sem_post(&sharedState->shmLock);

    if (count == 0) return -1;
    return alive[rand() % count];
}

void GameArbiter::resolveWeaponDrop() {
    int weaponId = noWeapon;
    int owner = noEntity;
    bool accepted = false;

    sem_wait(&sharedState->shmLock);
    if (!sharedState->weaponDropActive || !sharedState->weaponDropResolved) {
        sem_post(&sharedState->shmLock);
        return;
    }
    weaponId = sharedState->weaponDropId;
    owner = sharedState->weaponDropOwner;
    accepted = sharedState->weaponDropAccepted;

    sharedState->weaponDropActive = false;
    sharedState->weaponDropResolved = false;
    sharedState->weaponDropAccepted = false;
    sharedState->weaponDropOwner = noEntity;
    sharedState->weaponDropId = noWeapon;
    sharedState->combatPhase = 0;
    sharedState->combatActingPlayer = -1;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);

    bool pickedUp = false;
    if (accepted && owner >= 0 && owner < sharedState->playerCount &&
        weaponId >= 0 && weaponId < weaponCount) {
        pickedUp = inventoryAllocator->allocateWeapon(owner, weaponId);
        if (pickedUp && (weaponId == solarCoreArtifact || weaponId == lunarBladeArtifact || weaponId == eclipseRelicArtifact)) {
            int artifactId = (weaponId == 8) ? eclipseRelicArtifact : weaponId;
            tryAcquireArtifact(owner, artifactId);
        }
        char msg[128];
        snprintf(msg, 128, "P%d Weapon Pickup [%s]%s", owner,
            weaponTable[weaponId].name,
            pickedUp ? "" : " FAILED");
        logAction(sharedState, msg, getpid(), 0);
    }

    if (!accepted && owner >= 0 && owner < sharedState->playerCount &&
        weaponId >= 0 && weaponId < weaponCount) {
        char dropMsg[128];
        snprintf(dropMsg, 128, "P%d Drop Weapon [%s]", owner, weaponTable[weaponId].name);
        logAction(sharedState, dropMsg, getpid(), 0);
    }

    if (!pickedUp && weaponId >= 0 && weaponId < weaponCount) {
        
        // SC/LB: player declined so weapon is truly dropped from circulation (no enemy pickup)
        // so it can respawn on a future enemy death
        bool isUnique = (weaponId == solarCoreArtifact || weaponId == lunarBladeArtifact);
        if (!isUnique) {
            int enemyIdx = pickRandomAliveEnemy();
            if (enemyIdx >= 0) {
                sem_wait(&sharedState->shmLock);
                sharedState->enemies[enemyIdx].weaponId = weaponId;
                sharedState->sequence++;
                sem_post(&sharedState->shmLock);

                char msg[128];
                snprintf(msg, 128, "E%d picked up weapon [%s]", enemyIdx, weaponTable[weaponId].name);
                logAction(sharedState, msg, getpid(), 0);
            }
        } 
        else {
            char msg[128];
            snprintf(msg, 128, "[%s] dropped - back in circulation", weaponTable[weaponId].name);
            logAction(sharedState, msg, getpid(), 0);
        }
    }
}

bool GameArbiter::tryAcquireArtifact(int playerIndex, int artifactId) {
    
    sem_wait(&sharedState->artifactLock);
    if (sharedState->artifacts.holder[artifactId] == playerIndex) {
        sharedState->artifacts.waitingFor[playerIndex] = -1;
        sem_post(&sharedState->artifactLock);
        return true;
    }

    if (sharedState->artifacts.exists[artifactId] && sharedState->artifacts.holder[artifactId] == noHolder) {

        sharedState->artifacts.holder[artifactId] = playerIndex;
        sharedState->sequence++;

        char msg[64];
        snprintf(msg, 64, "P%d acquired artifact %d", playerIndex, artifactId);
        logAction(sharedState, msg, getpid(), 0);

        int otherArtifact = -1;
        if (artifactId == solarCoreArtifact) otherArtifact = lunarBladeArtifact;
        else if (artifactId == lunarBladeArtifact) otherArtifact = solarCoreArtifact;

        if (otherArtifact != -1) {
            
            int otherHolder = sharedState->artifacts.holder[otherArtifact];
            if (otherHolder != noHolder && otherHolder != playerIndex) {
                sharedState->artifacts.waitingFor[playerIndex] = otherArtifact;
                sharedState->artifacts.waitingFor[otherHolder] = artifactId;

                char dmsg[128];
                snprintf(dmsg, 128,
                    "DEADLOCK MANUFACTURED: P%d holds art%d wants art%d | P%d holds art%d wants art%d",
                    playerIndex, artifactId, otherArtifact, otherHolder, otherArtifact, artifactId);
                logAction(sharedState, dmsg, getpid(), 0);
            } 
            else {
                sharedState->artifacts.waitingFor[playerIndex] = -1;
            }
        } 
        else {
            sharedState->artifacts.waitingFor[playerIndex] = -1;
        }

        sem_post(&sharedState->artifactLock);
        return true;
    }

    sharedState->artifacts.waitingFor[playerIndex] = artifactId;
    sharedState->sequence++;
    char msg[64];
    snprintf(msg, 64, "P%d waiting for artifact %d (held by %d)",
        playerIndex, artifactId, sharedState->artifacts.holder[artifactId]);
    logAction(sharedState, msg, getpid(), 0);

    sem_post(&sharedState->artifactLock);
    return false;
}

void GameArbiter::releaseArtifact(int playerIndex, int artifactId) {
    sem_wait(&sharedState->artifactLock);
    if (sharedState->artifacts.holder[artifactId] == playerIndex) {
        sharedState->artifacts.holder[artifactId] = noHolder;
        sharedState->artifacts.waitingFor[playerIndex] = -1;
        sharedState->sequence++;

        char msg[64];
        snprintf(msg, 64, "P%d released artifact %d", playerIndex, artifactId);
        logAction(sharedState, msg, getpid(), 0);
    }
    sem_post(&sharedState->artifactLock);
}

void GameArbiter::checkEclipseRelicSpawn() {
    (void)0;
}

void GameArbiter::resolveEclipseRelicPickup() {
    
    sem_wait(&sharedState->shmLock);
    int pickedPlayer = sharedState->eclipseRelicPickupPlayer;
    sharedState->eclipseRelicPickupActive = false;
    sharedState->eclipseRelicPickupResolved = false;
    sharedState->eclipseRelicPickupPlayer = -1;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);

    const int eclipseWeaponId = 8;
    if (pickedPlayer >= 0 && pickedPlayer < sharedState->playerCount &&
        
        sharedState->players[pickedPlayer].entity.isAlive) {
        bool allocated = inventoryAllocator->allocateWeapon(pickedPlayer, eclipseWeaponId);
        if (!allocated) {
            logAction(sharedState, "Eclipse Relic pickup failed (inventory full)", getpid(), 0);
            return;
        }
        tryAcquireArtifact(pickedPlayer, eclipseRelicArtifact);
        char msg[64];
        snprintf(msg, 64, "P%d picked up Eclipse Relic", pickedPlayer);
        logAction(sharedState, msg, getpid(), 0);
    } 
    else {
        
        int enemyIdx = pickRandomAliveEnemy();
        if (enemyIdx >= 0) {
            
            sem_wait(&sharedState->shmLock);
            sharedState->enemies[enemyIdx].weaponId = eclipseWeaponId;
            
            sem_wait(&sharedState->artifactLock);
            sharedState->artifacts.holder[eclipseRelicArtifact] = maxPlayers + enemyIdx;
            sem_post(&sharedState->artifactLock);
            
            sharedState->sequence++;
            sem_post(&sharedState->shmLock);

            char msg[64];
            snprintf(msg, 64, "E%d picked up Eclipse Relic", enemyIdx);
            logAction(sharedState, msg, getpid(), 0);
        } 
        else {    
            logAction(sharedState, "Eclipse Relic unclaimed", getpid(), 0);
        }
    }
}

void GameArbiter::processHipRequests() {
    sem_wait(&sharedState->hipRequest.hipRequestLock);
    if (!sharedState->hipRequest.requestPause) {
        sem_post(&sharedState->hipRequest.hipRequestLock);
        return;
    }
    sharedState->hipRequest.requestPause = false;
    sem_post(&sharedState->hipRequest.hipRequestLock);

    sem_wait(&sharedState->shmLock);
    sharedState->escapeMenuOpen = true;
    sharedState->gamePaused = true;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);
    logAction(sharedState, "HIP requested pause -- escape menu opened by Arbiter", getpid(), 0);
}

void GameArbiter::deliverStun(int targetEntityId) {
    int targetPid = -1;

    sem_wait(&sharedState->shmLock);
    sharedState->stunTargetEntityId = targetEntityId;
    sharedState->stunTargetPid = (targetEntityId < maxPlayers)
        ? sharedState->players[targetEntityId].entity.pid
        : sharedState->enemies[targetEntityId - maxPlayers].entity.pid;
    
    targetPid = sharedState->stunTargetPid;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);

    if (targetPid > 0) {
        kill(targetPid, SIGUSR1);
        char msg[64];
        snprintf(msg, 64, "stun delivered to entity %d pid %d", targetEntityId, targetPid);
        logAction(sharedState, msg, getpid(), 0);
    }
}

void GameArbiter::checkWinLoss() {
    if (sharedState->playersAlive <= 0) {
        handleGameOver(false);
        return;
    }
    if (sharedState->enemiesDefeated >= winKillCount) {
        handleGameOver(true);
        return;
    }
    if (sharedState->enemiesAlive <= 0) {
        handleWaveEnd();
    }
}

void GameArbiter::handleWaveEnd() {
    sem_wait(&sharedState->shmLock);
    sharedState->waveTransition = true;
    sharedState->currentTurnEntityId = noEntity;
    sharedState->combatPhase = 0;
    sharedState->combatActingPlayer = -1;
    sharedState->combatSelectedTarget = 0;
    sharedState->combatSelectedAction = 0;

    sharedState->weaponDropActive = false;
    sharedState->weaponDropResolved = false;
    sharedState->weaponDropAccepted = false;
    sharedState->weaponDropOwner = noEntity;
    sharedState->weaponDropId = noWeapon;

    for (int i = 0; i < sharedState->playerCount; i++) {
        sharedState->players[i].entity.hasTurn = false;
        sharedState->players[i].entity.actionReady = false;
    }
    for (int i = 0; i < sharedState->enemyCount; i++) {
        sharedState->enemies[i].entity.hasTurn = false;
        sharedState->enemies[i].entity.actionReady = false;
    }

    sharedState->readyQueueHead = 0;
    sharedState->readyQueueTail = 0;
    sharedState->readyQueueCount = 0;
    for (int i = 0; i < maxEntities; i++) {
        sharedState->readyQueue[i] = noEntity;
        sharedState->inReadyQueue[i] = false;
    }

    sharedState->waveNumber++;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);

    checkEclipseRelicSpawn();
    logAction(sharedState, "wave complete - transition", getpid(), 0);

    sleep(3);
    initializeWave();

    sem_wait(&sharedState->shmLock);
    sharedState->waveTransition = false;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);
}

void GameArbiter::handleGameOver(bool won) {
    sem_wait(&sharedState->shmLock);
    sharedState->gameOver  = true;
    sharedState->playerWon = won;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);

    logAction(sharedState, won ? "GAME OVER: WIN" : "GAME OVER: LOSS", getpid(), 0);
    sleep(3);
    running = false;
}

void GameArbiter::setupSignals() {
    struct sigaction sa = {};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    sa.sa_handler = onSigterm;
    sigaction(SIGTERM, &sa, nullptr);

    sa.sa_handler = onSigalrm;
    sigaction(SIGALRM, &sa, nullptr);

    sa.sa_handler = onSigusr2;
    sigaction(SIGUSR2, &sa, nullptr);
}

void GameArbiter::onSigterm(int sig) {
    (void)sig;
    if (instance) {
        instance->sharedState->shouldExit = true;
        instance->running = false;
    }
}

void GameArbiter::onSigalrm(int sig) {
    (void)sig;
    if (!instance || !instance->sharedState) return;
    
    SharedState* s = instance->sharedState;
    s->ultimateActive = false;
    if (s->aspPid > 0) {
        kill(s->aspPid, SIGCONT);
        logAction(s, "Ultimate ability: ASP process resumed (SIGCONT)", getpid(), 0);
    }
}

void GameArbiter::onSigusr2(int sig) {
    (void)sig;
    if (!instance || !instance->sharedState)
        return;
    
    SharedState* s = instance->sharedState;
    s->ultimateActive = true;
    if (s->aspPid > 0) {
        kill(s->aspPid, SIGSTOP);
        alarm(ultimateDuration);  
        logAction(s, "Ultimate ability: ASP process stopped (SIGSTOP)", getpid(), 0);
    }
}

void* GameArbiter::staminaThreadFunc(void* arg) {
    GameArbiter* arbiter = (GameArbiter*)arg;
    return StaminaTimer::threadFunc(arbiter->staminaTimer);
}

void* GameArbiter::deadlockThreadFunc(void* arg) {
    GameArbiter* arbiter = (GameArbiter*)arg;
    return DeadlockMonitor::threadFunc(arbiter->deadlockMonitor);
}

void* GameArbiter::renderThreadFunc(void* arg) {
    GameArbiter* arbiter = (GameArbiter*)arg;
    return Renderer::threadFunc(arbiter->renderer);
}

void* GameArbiter::gameLoopThreadFunc(void* arg) {
    
    GameArbiter* arbiter = (GameArbiter*)arg;
    bool ready = false, exit = false;
    while (!ready && !exit) {
        sem_wait(&arbiter->sharedState->shmLock);
        ready = arbiter->sharedState->gameReady;
        exit = arbiter->sharedState->shouldExit;
        sem_post(&arbiter->sharedState->shmLock);
        if (!ready && !exit) 
            usleep(10000);
    }

    bool loopExit = false, loopReturn = false;
    while (arbiter->running && !loopExit && !loopReturn) {
        
        arbiter->processHipRequests();

        sem_wait(&arbiter->sharedState->shmLock);
        loopExit = arbiter->sharedState->shouldExit;
        loopReturn = arbiter->sharedState->returnToMenu;
        bool paused = arbiter->sharedState->gamePaused;
        bool wave = arbiter->sharedState->waveTransition;
        bool over = arbiter->sharedState->gameOver;
        sem_post(&arbiter->sharedState->shmLock);

        if (loopExit || loopReturn) break;
        if (paused || wave || over) {
            usleep(50000);
            continue;
        }

        if (arbiter->sharedState->weaponDropActive) {
            if (arbiter->sharedState->weaponDropResolved) arbiter->resolveWeaponDrop();
            else {
                usleep(10000);
                continue;
            }
        }

        if (!arbiter->sharedState->eclipseRelicSpawned &&
            !arbiter->sharedState->artifacts.exists[eclipseRelicArtifact] &&
            !arbiter->sharedState->eclipseRelicPickupActive &&
            !arbiter->sharedState->waveTransition && (rand() % 10000 == 0)) {
            
            sem_wait(&arbiter->sharedState->artifactLock);
            arbiter->sharedState->artifacts.exists[eclipseRelicArtifact] = true;
            arbiter->sharedState->artifacts.holder[eclipseRelicArtifact] = noHolder;
            arbiter->sharedState->eclipseRelicSpawned = true;
            sem_post(&arbiter->sharedState->artifactLock);

            sem_wait(&arbiter->sharedState->shmLock);
            arbiter->sharedState->eclipseRelicPickupActive = true;
            arbiter->sharedState->eclipseRelicPickupResolved = false;
            arbiter->sharedState->eclipseRelicPickupPlayer = -1;
            arbiter->sharedState->combatPhase = 7;
            arbiter->sharedState->sequence++;
            sem_post(&arbiter->sharedState->shmLock);

            logAction(arbiter->sharedState, "Eclipse Relic appeared!", getpid(), 0);
        }

        if (arbiter->sharedState->eclipseRelicPickupActive) {
            if (arbiter->sharedState->eclipseRelicPickupResolved) arbiter->resolveEclipseRelicPickup();
            else {
                usleep(10000);
                continue;
            }
        }

        if (arbiter->sharedState->attackAnimationInProgress) {
            usleep(10000);
            continue;
        }

        int entityId = arbiter->turnScheduler->pickNext();
        if (entityId == noEntity) {
            usleep(10000);
            continue;
        }

        arbiter->grantTurn(entityId);
        if (!arbiter->waitForAction()) break;
        arbiter->commitAction(entityId);
        arbiter->checkWinLoss();
    }

    if (!arbiter->sharedState->returnToMenu) arbiter->sharedState->shouldExit = true;
    arbiter->running = false;
    return nullptr;
}

void* GameArbiter::initThreadFunc(void* arg) {
    GameArbiter* arbiter = (GameArbiter*)arg;
    SharedState* s = arbiter->sharedState;

    printf("[initThread] waiting for party selection...\n");
    fflush(stdout);
    while (s->playerCount == 0 && !s->shouldExit)
        usleep(100000);

    if (s->shouldExit) return nullptr;

    printf("[initThread] playerCount=%d -- initialising entities\n", s->playerCount);
    fflush(stdout);

    arbiter->initializeEntities();
    arbiter->initializeWave();

    s->gameReady = true;
    s->sequence++;
    printf("[initThread] gameReady set\n");
    fflush(stdout);

    usleep(200000);

    if (!arbiter->spawnASP()) {
        fprintf(stderr, "[initThread] failed to spawn ASP\n");
        s->shouldExit = true;
        return nullptr;
    }
    usleep(100000);

    arbiter->startStaminaThread();
    arbiter->startDeadlockThread();

    printf("[initThread] done\n");
    fflush(stdout);
    return nullptr;
}

void GameArbiter::resetForNewGame() {
    
    killChildren();
    waitForChildren();
    
    if (staminaTimer) { 
        staminaTimer->stop();    
        delete staminaTimer;    
        staminaTimer = new StaminaTimer(sharedState); 
    }

    if (deadlockMonitor) { 
        deadlockMonitor->stop();  
        delete deadlockMonitor; 
        inventoryAllocator = new InventoryAllocator(sharedState);
        deadlockMonitor = new DeadlockMonitor(sharedState, inventoryAllocator); 
    }

    usleep(1200000);
    
    sem_wait(&sharedState->shmLock);

    sharedState->returnToMenu = false;
    sharedState->shouldExit = false;
    sharedState->gamePaused = false;
    sharedState->escapeMenuOpen = false;
    sharedState->gameOver = false;
    sharedState->playerWon = false;
    sharedState->gameReady = false;
    sharedState->ultimateActive = false;
    sharedState->eclipseRelicSpawned = false;
    sharedState->waveTransition = false;
    sharedState->multiplayerMode = false;
    sharedState->pvpMode = false;
    sharedState->enemyEnabled = true;
    sharedState->activeHipCount = 0;
    sharedState->attackAnimationInProgress = false;
    sharedState->teleportActive = false;
    sharedState->weaponDropActive = false;
    sharedState->weaponDropResolved = false;
    sharedState->weaponDropAccepted = false;
    sharedState->weaponDropOwner = noEntity;
    sharedState->weaponDropId = noWeapon;

    sharedState->eclipseRelicPickupActive = false;
    sharedState->eclipseRelicPickupResolved = false;
    sharedState->eclipseRelicPickupPlayer = -1;

    sharedState->playerCount = 0;
    sharedState->enemyCount = 0;
    sharedState->playersAlive = 0;
    sharedState->enemiesAlive = 0;
    sharedState->totalEnemiesThisWave = 0;
    sharedState->enemiesDefeated = 0;
    sharedState->waveNumber = 1;
    sharedState->currentTurnEntityId = noEntity;
    sharedState->sequence++;

    sharedState->readyQueueHead = 0;
    sharedState->readyQueueTail = 0;
    sharedState->readyQueueCount = 0;
    for (int i = 0; i < maxEntities; i++) {
        sharedState->readyQueue[i] = noEntity;
        sharedState->inReadyQueue[i] = false;
    }

    sharedState->combatPhase = 0;
    sharedState->combatActingPlayer = -1;
    sharedState->combatSelectedTarget = 0;
    sharedState->combatSelectedAction = 0;

    for (int a = 0; a < 3; a++) {
        sharedState->artifacts.holder[a] = noHolder;
        sharedState->artifacts.exists[a] = false;
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

        sharedState->players[p].entity.id = p;
        sharedState->players[p].entity.maxHp = 0;
        sharedState->players[p].entity.currentHp = 0;
        sharedState->players[p].entity.maxStamina = 0;
        sharedState->players[p].entity.currentStamina = 0;
        sharedState->players[p].entity.speed = 0;
        sharedState->players[p].entity.damage = 0;
        sharedState->players[p].entity.isAlive = false;
        sharedState->players[p].entity.isStunned = false;
        sharedState->players[p].entity.hasTurn = false;
        sharedState->players[p].entity.actionReady = false;
        sharedState->players[p].entity.pid = -1;
        sharedState->players[p].entity.threadId = 0;
        snprintf(sharedState->players[p].entity.name, 32, "Player%d", p + 1);

        sharedState->players[p].actionType = actionSkip;
        sharedState->players[p].actionTarget = noTarget;
        sharedState->players[p].weaponUsed = noWeapon;
        sharedState->players[p].hipOwner = 0;
        sharedState->players[p].spriteType = p % 4;
    }

    for (int i = 0; i < maxEnemies; i++) {
        sharedState->enemies[i].entity.id = maxPlayers + i;
        sharedState->enemies[i].entity.maxHp = 0;
        sharedState->enemies[i].entity.currentHp = 0;
        sharedState->enemies[i].entity.maxStamina = 0;
        sharedState->enemies[i].entity.currentStamina = 0;
        sharedState->enemies[i].entity.speed = 0;
        sharedState->enemies[i].entity.damage = 0;
        sharedState->enemies[i].entity.isAlive = false;
        sharedState->enemies[i].entity.isStunned = false;
        sharedState->enemies[i].entity.hasTurn = false;
        sharedState->enemies[i].entity.actionReady = false;
        sharedState->enemies[i].entity.pid = -1;
        sharedState->enemies[i].entity.threadId = 0;
        snprintf(sharedState->enemies[i].entity.name, 32, "Enemy%d", i + 1);

        sharedState->enemies[i].weaponId = noWeapon;
        sharedState->enemies[i].actionType = actionEnemySkip;
        sharedState->enemies[i].actionTarget = noTarget;
        sharedState->enemies[i].spriteType = 0;
    }

    sharedState->keyboardInput.isUp = false;
    sharedState->keyboardInput.isDown = false;
    sharedState->keyboardInput.isLeft = false;
    sharedState->keyboardInput.isRight = false;
    sharedState->keyboardInput.isConfirm = false;
    sharedState->keyboardInput.isBack = false;
    sharedState->keyboardInput.isEscape = false;
    sharedState->keyboardInput.newInput = false;
    
    for (int i = 0; i < maxEntities; i++)
        sharedState->keyboardInput.consumed[i] = true;   
    
    sharedState->keyboardInput.menuSelection = 0;
    sharedState->keyboardInput.menuConfirmed = false;
    sharedState->keyboardInput.partySize = 0;
    sharedState->keyboardInput.gameStartRequested = false;

    sharedState->teleportPlayAttack = false;
    sharedState->activeHipCount = 1;

    // stun coordination:
    sharedState->stunTargetPid = -1;
    sharedState->stunTargetEntityId = noEntity;

    //hip requests:
    sem_wait(&sharedState->hipRequest.hipRequestLock);
    sharedState->hipRequest.requestPause = false;
    sem_post(&sharedState->hipRequest.hipRequestLock);

    //child pids:
    sharedState->hip1Pid = -1;
    sharedState->hip2Pid = -1;
    sharedState->aspPid = -1;

    //action log:
    sharedState->logHead = 0;
    sharedState->logCount = 0;
    for (int i = 0; i < logSize; i++)
        sharedState->log[i].valid = false;

    //back to main menu:
    sharedState->isMainMenu = true;
    sharedState->isPartySelect = false;
    sharedState->isBattle = false;
    sharedState->isGameOver = false;

    sem_post(&sharedState->shmLock);

    running = true;
    hip1Pid = -1;
    hip2Pid = -1;
    aspPid = -1;

    logAction(sharedState, "reset complete -- back at main menu", getpid(), 0);
    printf("[arbiter] reset complete -- waiting for new game start\n");
    fflush(stdout);
}

void* GameArbiter::monitorThreadFunc(void* arg) {
    MonitorArgs* ma = (MonitorArgs*)arg;
    GameArbiter* ga = ma->arbiter;
    delete ma;

    while (!ga->sharedState->shouldExit) {
        usleep(50000);
        
        if (!ga->sharedState->returnToMenu) continue;
        logAction(ga->sharedState, "monitor: returning to main menu", getpid(), pthread_self());
        
        ga->resetForNewGame();
        ga->running = true;

        pthread_t gl, it;
        if (pthread_create(&gl, NULL, GameArbiter::gameLoopThreadFunc, ga) == 0)
            pthread_detach(gl);
        if (pthread_create(&it, NULL, GameArbiter::initThreadFunc, ga) == 0)
            pthread_detach(it);

        ga->spawnHIP(1);
    }
    return nullptr;
}

void GameArbiter::run() {
    running = true;

    if (pthread_create(&gameLoopThread, NULL, gameLoopThreadFunc, this) != 0) {
        perror("[arbiter] game loop thread");
        running = false;
        return;
    }

    if (pthread_create(&initThread, NULL, initThreadFunc, this) != 0) {
        perror("[arbiter] init thread");
        running = false;
        sharedState->shouldExit = true;
        return;
    }

    //spawning HIP1 immediately so input is captured from the start:
    if (!spawnHIP(1)) {
        fprintf(stderr, "[arbiter] failed to spawn HIP1\n");
        sharedState->shouldExit = true;
    }

    //monitor thread:
    MonitorArgs* margs = new MonitorArgs{this};
    if (pthread_create(&monitorThread, NULL, monitorThreadFunc, margs) != 0) {
        perror("[arbiter] monitor thread");
        delete margs;
    }

    logAction(sharedState, "game loop started", getpid(), 0);
    printf("[arbiter] renderer starting on main thread\n");
    fflush(stdout);

    renderer->loop();  //blocks until window close or shouldExit

    sharedState->shouldExit = true;
    running = false;

    logAction(sharedState, "game loop ended", getpid(), 0);
    usleep(50000);
    killChildren();
    usleep(50000); 

    if (hip1Pid > 0) kill(hip1Pid, SIGKILL);
    if (hip2Pid > 0) kill(hip2Pid, SIGKILL);
    if (aspPid > 0) kill(aspPid, SIGKILL);
    waitForChildren();
}

void GameArbiter::cleanup() {
    killChildren();
    waitForChildren();

    //signaling all threads to stop:
    if (sharedState) sharedState->shouldExit = true;
    running = false;

    
    if (staminaTimer) staminaTimer->stop();
    if (deadlockMonitor) deadlockMonitor->stop();

    if (gameLoopThread) { 
        pthread_join(gameLoopThread, NULL); 
        gameLoopThread = 0; 
    }
    if (initThread) { 
        pthread_join(initThread, NULL); 
        initThread = 0; 
    }
    if (monitorThread) { 
        pthread_join(monitorThread, NULL); 
        monitorThread = 0; 
    }
    if (staminaThread) { 
        pthread_join(staminaThread, NULL); 
        staminaThread  = 0; 
    }
    if (deadlockThread) { 
        pthread_join(deadlockThread, NULL); 
        deadlockThread = 0; 
    }

    if (staminaTimer) { 
        delete staminaTimer;   
        staminaTimer = nullptr; 
    }
    if (deadlockMonitor) { 
        delete deadlockMonitor; 
        deadlockMonitor = nullptr; 
    }
    if (turnScheduler) { 
        delete turnScheduler;   
        turnScheduler = nullptr; 
    }
    if (inventoryAllocator) { 
        delete inventoryAllocator; 
        inventoryAllocator = nullptr; 
    }
    if (renderer) { 
        renderer->close(); 
        delete renderer; 
        renderer = nullptr; 
    }

    destroySharedMemory(shmId, sharedState);
    sharedState = nullptr;
    shmId = -1;
}

//////////////////////////////////////////////////////////////////////////////////////////
