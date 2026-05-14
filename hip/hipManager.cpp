//////////////////////////////////////////////////////////////////////////////////////////
#include "hipManager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

//////////////////////////////////////////////////////////////////////////////////////////
// === INTERFACE STATE IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

InterfaceState::InterfaceState() {
    resetPhases();
    selectedPlayer = 0;
    selectedTarget = 0;
    selectedAction = 0;
    playerCount = 0;
    npcCount = 0;
    swapRequested = false;
    swapFromIndex = -1;
    swapToIndex = -1;
    escapeMenuSelection = 0;
}

void InterfaceState::resetPhases() {
    isMainMenu = false;
    isPartySelect = false;
    isBattle = false;
    isPlayerSelect = false;
    isTargetSelect = false;
    isActionPopup = false;
    isEscapeMenu = false;
    isWaveTransition = false;
    isWinScreen = false;
    isLoseScreen = false;
    isDebugOverlay = false;
}

void InterfaceState::setPhase(bool& phase) {
    resetPhases();
    phase = true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// === PLAYER CONTROLLER IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

PlayerController::PlayerController(SharedState* sharedState, int pIndex, int hipNum) {
    this->sharedState = sharedState;
    this->playerIndex = pIndex;
    this->hipNumber = hipNum;
    this->running = false;
}

PlayerController::~PlayerController() {}

void PlayerController::waitForInput(bool& up, bool& down, bool& left, bool& right, bool& confirm, bool& back) {
    
    sem_wait(&sharedState->keyboardInput.inputLock);
    sharedState->keyboardInput.consumed[playerIndex] = true;
    sem_post(&sharedState->keyboardInput.inputLock);
   
    while (!sharedState->shouldExit) {
        bool canRead = sharedState->players[playerIndex].entity.hasTurn ||
            (sharedState->weaponDropActive && sharedState->weaponDropOwner == playerIndex);
        
        if (!canRead) {
            usleep(8000);
            continue;
        }

        sem_wait(&sharedState->keyboardInput.inputLock);
        if (sharedState->keyboardInput.newInput && !sharedState->keyboardInput.consumed[playerIndex]) {
            up = sharedState->keyboardInput.isUp;
            down = sharedState->keyboardInput.isDown;
            left = sharedState->keyboardInput.isLeft;
            right = sharedState->keyboardInput.isRight;
            confirm = sharedState->keyboardInput.isConfirm;
            back = sharedState->keyboardInput.isBack;
            sharedState->keyboardInput.consumed[playerIndex] = true;
            sharedState->keyboardInput.newInput = false;

            sem_post(&sharedState->keyboardInput.inputLock);
            return;
        }
        sem_post(&sharedState->keyboardInput.inputLock);
        usleep(8000);
    }
    up = down = left = right = confirm = back = false;
}

bool PlayerController::isMyTurn() {
    return sharedState->players[playerIndex].entity.hasTurn;
}

int PlayerController::topPriorityPlayer() {
    for (int i = 0; i < sharedState->playerCount; i++)
        if (sharedState->players[i].entity.hasTurn) 
            return i;
    return playerIndex;
}

int PlayerController::aliveEnemyCount() {
    int count = 0;
    for (int i = 0; i < sharedState->enemyCount; i++)
        if (sharedState->enemies[i].entity.isAlive) 
            count++;
    return count;
}

int PlayerController::nextAliveEnemy(int current, int dir) {
    int count = sharedState->enemyCount;
    int next = current;
    for (int i = 0; i < count; i++) {
        next = (next + dir + count) % count;
        if (sharedState->enemies[next].entity.isAlive) 
            return next;
    }
    return current;
}

int PlayerController::nextAlivePlayer(int current, int dir) {
    int count = sharedState->playerCount;
    int next  = current;
    for (int i = 0; i < count; i++) {
        next = (next + dir + count) % count;
        if (sharedState->players[next].entity.isAlive) 
            return next;
    }
    return current;
}

bool PlayerController::hasWeaponInInventory(int weaponId) {
    InventoryData* inv = &sharedState->players[playerIndex].inventory;
    for (int i = 0; i < inventorySlots; i++)
        if (inv->slots[i] == weaponId) 
            return true;
    return false;
}

int PlayerController::getInventoryWeapons(int* weaponIds, int  maxCount) {
    InventoryData* inv = &sharedState->players[playerIndex].inventory;
    int count = 0;
    int i  = 0;
    while (i < inventorySlots && count < maxCount) {
        if (inv->slots[i] == noWeapon) { 
            i++; 
            continue; 
        }
        int wid = inv->slots[i];
        weaponIds[count++] = wid;
        i += weaponTable[wid].slotSize;
    }
    return count;
}

void PlayerController::requestPrioritySwap(int fromIdx, int toIdx) {
    sem_wait(&sharedState->shmLock);
    int staminaA = sharedState->players[fromIdx].entity.currentStamina;
    int staminaB = sharedState->players[toIdx].entity.currentStamina;
    
    sharedState->players[fromIdx].entity.currentStamina = staminaB;
    sharedState->players[toIdx].entity.currentStamina = staminaA;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);

    char msg[64];
    snprintf(msg, 64, "priority swap P%d<->P%d", fromIdx, toIdx);
    logAction(sharedState, msg, getpid(), pthread_self());
}

void PlayerController::submitAction(int actionType, int targetIndex, int weaponId) {
    sem_wait(&sharedState->shmLock);
    sharedState->combatPhase = 0;
    sharedState->combatActingPlayer = -1;
    sharedState->combatSelectedTarget = 0;
    sharedState->combatSelectedAction = 0;
    
    PlayerData* player = &sharedState->players[playerIndex];
    player->actionType = actionType;
    player->actionTarget = targetIndex;
    player->weaponUsed = weaponId;
    player->entity.actionReady = true;
    sharedState->sequence++;

    sem_post(&sharedState->shmLock);
    sem_post(&sharedState->actionSem);

    char msg[128];
    snprintf(msg, 128, "P%d action:%d target:%d weapon:%d", playerIndex, actionType, targetIndex, weaponId);
    logAction(sharedState, msg, getpid(), pthread_self());
}

void PlayerController::phasePlayerSelect() {
    interface.resetPhases();
    interface.isPlayerSelect = true;
    interface.selectedPlayer = topPriorityPlayer();

    sharedState->combatPhase = 1;
    sharedState->combatActingPlayer = playerIndex;

    if (sharedState->playersAlive <= 1) {
        interface.selectedPlayer = playerIndex;
        interface.setPhase(interface.isTargetSelect);
        return;
    }

    while (interface.isPlayerSelect && !sharedState->shouldExit) {
        bool up, down, left, right, confirm, back;
        waitForInput(up, down, left, right, confirm, back);

        if (back) {
            interface.setPhase(interface.isEscapeMenu);
            phaseEscapeMenu();
            if (!interface.isBattle) 
                return;
            interface.setPhase(interface.isPlayerSelect);
            continue;
        }

        int dir = 0;
        if (right || down) dir =  1;
        if (left || up) dir = -1;

        if (dir != 0) {
            int next = nextAlivePlayer(interface.selectedPlayer, dir);
            if (next != interface.selectedPlayer) {
                EntityData* entity = &sharedState->players[next].entity;
                if (entity->currentStamina >= entity->maxStamina) {
                    requestPrioritySwap(interface.selectedPlayer, next);
                    interface.selectedPlayer = next;
                }
            }
        }

        if (confirm) {
            interface.setPhase(interface.isTargetSelect);
            return;
        }
    }
}

void PlayerController::phaseTargetSelect() {
    interface.isTargetSelect = true;
    interface.selectedTarget = 0;

    for (int i = 0; i < sharedState->enemyCount; i++) {
        if (sharedState->enemies[i].entity.isAlive) {
            interface.selectedTarget = i;
            break;
        }
    }

    sharedState->combatPhase = 2;
    sharedState->combatActingPlayer = playerIndex;
    sharedState->combatSelectedTarget = interface.selectedTarget;

    while (interface.isTargetSelect && !sharedState->shouldExit) {
        if (sharedState->waveTransition || sharedState->gameOver || !isMyTurn()) {
            interface.setPhase(interface.isBattle);
            return;
        }

        int cur = interface.selectedTarget;
        if (cur < 0 || cur >= sharedState->enemyCount ||
            !sharedState->enemies[cur].entity.isAlive) {
            interface.selectedTarget = nextAliveEnemy(cur < 0 ? 0 : cur, 1);
            sharedState->combatSelectedTarget = interface.selectedTarget;
        }

        bool up, down, left, right, confirm, back;
        waitForInput(up, down, left, right, confirm, back);

        //exiting immediately if turn has ended or a weapon drop is waiting:
        if (!isMyTurn() ||
            (sharedState->weaponDropActive && sharedState->weaponDropOwner == playerIndex)) {
            interface.setPhase(interface.isBattle);
            return;
        }

        if (back) {
            interface.setPhase(interface.isPlayerSelect);
            return;
        }

        int dir = 0;
        if (right || down) dir =  1;
        if (left || up) dir = -1;

        if (dir != 0) {
            interface.selectedTarget = nextAliveEnemy(interface.selectedTarget, dir);
            sharedState->combatSelectedTarget = interface.selectedTarget;
        }

        if (confirm) {
            interface.setPhase(interface.isActionPopup);
            return;
        }
    }
}

int PlayerController::phaseWeaponSelect() {
    int weapons[inventorySlots];
    int wCount = getInventoryWeapons(weapons, inventorySlots);
    if (wCount == 0)
        return noWeapon;

    int selected = 0;

    sharedState->combatPhase = 6;
    sharedState->combatActingPlayer = playerIndex;
    sharedState->combatSelectedAction = selected;

    while (!sharedState->shouldExit) {
        if (sharedState->waveTransition || sharedState->gameOver || !isMyTurn())
            return noWeapon;
        
        bool up, down, left, right, confirm, back;
        waitForInput(up, down, left, right, confirm, back);

        if (back) return noWeapon;
        if (up) {
            selected = (selected - 1 + wCount) % wCount;
            sharedState->combatSelectedAction = selected;
        }
        if (down) {
            selected = (selected + 1) % wCount;
            sharedState->combatSelectedAction = selected;
        }
        if (confirm) return weapons[selected];
    }

    return noWeapon;
}

int PlayerController::phaseSwapInSelect() {

    InventoryData* inv = &sharedState->players[playerIndex].inventory;
    if (inv->longTermStorageCount == 0)
        return noWeapon;

    int selected = 0;

    sharedState->combatPhase = 5;
    sharedState->combatActingPlayer = playerIndex;
    sharedState->combatSelectedAction = selected;

    while (!sharedState->shouldExit) {
        if (sharedState->waveTransition || sharedState->gameOver || !isMyTurn())
            return noWeapon;
        bool up, down, left, right, confirm, back;
        waitForInput(up, down, left, right, confirm, back);

        if (back) return noWeapon;
        if (up) {
            selected = (selected - 1 + inv->longTermStorageCount) % inv->longTermStorageCount;
            sharedState->combatSelectedAction = selected;
        }
        if (down) {
            selected = (selected + 1) % inv->longTermStorageCount;
            sharedState->combatSelectedAction = selected;
        }
        if (confirm) return inv->longTermStorage[selected];
    }
    return noWeapon;
}

void PlayerController::phaseActionPopup() {
    
    interface.isActionPopup = true;
    interface.selectedAction = 0;
    static const int actionCount = 7;  

    sharedState->combatPhase = 3;
    sharedState->combatActingPlayer = playerIndex;
    sharedState->combatSelectedAction = 0;

    while (interface.isActionPopup && !sharedState->shouldExit) {
        if (sharedState->waveTransition || sharedState->gameOver || !isMyTurn()) {
            interface.setPhase(interface.isBattle);
            return;
        }
        bool up, down, left, right, confirm, back;
        waitForInput(up, down, left, right, confirm, back);

        if (back) {
            interface.setPhase(interface.isTargetSelect);
            return;
        }

        if (up) {
            interface.selectedAction = (interface.selectedAction - 1 + actionCount) % actionCount;
            sharedState->combatSelectedAction = interface.selectedAction;
        }
        if (down) {
            interface.selectedAction = (interface.selectedAction + 1) % actionCount;
            sharedState->combatSelectedAction = interface.selectedAction;
        }

        if (confirm) {
            int weaponId = noWeapon;

            if (interface.selectedAction == actionUltimate) {
                
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
                if (artifactCount < 2) continue;

                // Trigger ultimate
                kill(sharedState->arbiterPid, SIGUSR2);
                logAction(sharedState, "ULTIMATE triggered - sent SIGUSR2 to Arbiter", getpid(), pthread_self());
                submitAction(actionUltimate, interface.selectedTarget, noWeapon);
                interface.setPhase(interface.isBattle);
                return;
            }

            if (interface.selectedAction == actionUseWeapon) {
                weaponId = phaseWeaponSelect();
                sharedState->combatPhase = 3;
                sharedState->combatActingPlayer = playerIndex;
                sharedState->combatSelectedAction = interface.selectedAction;
                if (weaponId == noWeapon)
                    continue;
            }

            if (interface.selectedAction == actionSwapIn) {
                weaponId = phaseSwapInSelect();
                sharedState->combatPhase = 3;
                sharedState->combatActingPlayer = playerIndex;
                sharedState->combatSelectedAction = interface.selectedAction;
                if (weaponId == noWeapon)
                    continue;
            }

            submitAction(interface.selectedAction, interface.selectedTarget, weaponId);
            interface.setPhase(interface.isBattle);
            return;
        }
    }
}

void PlayerController::phaseEscapeMenu() {
    interface.isEscapeMenu = true;

    sem_wait(&sharedState->hipRequest.hipRequestLock);
    sharedState->hipRequest.requestPause = true;
    sem_post(&sharedState->hipRequest.hipRequestLock);

    while (!sharedState->escapeMenuOpen && !sharedState->shouldExit)
        usleep(5000);

    while (sharedState->escapeMenuOpen && !sharedState->shouldExit && !sharedState->returnToMenu)
        usleep(5000);

    if (!sharedState->shouldExit && !sharedState->returnToMenu)
        interface.setPhase(interface.isBattle);
    else
        interface.resetPhases();
}

void PlayerController::phaseWeaponPickup() {
    
    sem_wait(&sharedState->shmLock);
    if (!sharedState->weaponDropActive || sharedState->weaponDropOwner != playerIndex) {
        sem_post(&sharedState->shmLock);
        return;
    }
    sharedState->combatPhase = 4;
    sharedState->combatActingPlayer = playerIndex;
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);

    while (!sharedState->shouldExit) {
        bool up, down, left, right, confirm, back;
        waitForInput(up, down, left, right, confirm, back);
        (void)up; (void)down; (void)left; (void)right;

        if (confirm || back) {
            sem_wait(&sharedState->shmLock);
            sharedState->weaponDropResolved = true;
            sharedState->weaponDropAccepted = confirm;
            sharedState->combatPhase = 0;
            sharedState->sequence++;
            sem_post(&sharedState->shmLock);
            return;
        }
    }
}

void PlayerController::start() {
    running = true;

    sem_wait(&sharedState->shmLock);
    sharedState->players[playerIndex].entity.threadId = pthread_self();
    sharedState->players[playerIndex].entity.pid = getpid();
    sem_post(&sharedState->shmLock);

    logAction(sharedState, "player thread started", getpid(), pthread_self());

    interface.resetPhases();
    interface.setPhase(interface.isBattle);
    interface.playerCount = sharedState->playerCount;

    while (running && !sharedState->shouldExit && !sharedState->returnToMenu) {
        if (sharedState->weaponDropActive && sharedState->weaponDropOwner == playerIndex &&
            !sharedState->weaponDropResolved && !sharedState->waveTransition) {
            phaseWeaponPickup();
            continue;
        }

        if (!isMyTurn()) {
            usleep(16000);
            continue;
        }

        if (sharedState->players[playerIndex].entity.actionReady) {
            usleep(8000);
            continue;
        }

        if (sharedState->waveTransition || sharedState->gameOver) {
            usleep(100000);
            continue;
        }

        interface.npcCount = sharedState->enemyCount;

        phaseTargetSelect();
        if (sharedState->shouldExit || sharedState->returnToMenu)
            break;

        phaseActionPopup();
        if (sharedState->shouldExit || sharedState->returnToMenu)
            break;
    }

    logAction(sharedState, "player thread exiting", getpid(), pthread_self());
}

void* PlayerController::threadFunc(void* arg) {
    PlayerController* controller = (PlayerController*)arg;
    controller->start();
    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
// === HIP MANAGER IMPLEMENTATION ===
//////////////////////////////////////////////////////////////////////////////////////////

HIPManager* HIPManager::instance = nullptr;

HIPManager::HIPManager(int hipNum, int roll) {
    hipNumber = hipNum;
    rollNo = roll;
    playerOffset = (hipNum == 1) ? 0 : 2;
    shmId = -1;
    sharedState = nullptr;
    running = false;
    playerCount = 0;
    instance = this;

    for (int i = 0; i < maxPlayers; i++) {
        controllers[i] = nullptr;
        playerThreads[i] = 0;
    }
}

HIPManager::~HIPManager() {
    cleanup();
}

bool HIPManager::initialize() {
    if (!attachShm())  
        return false;
    if (!initializeWindow()) 
        return false;
    
    setupSignals();
    registerPid();
    return true;
}

bool HIPManager::attachShm() {
    int retries = 10;
    while (retries-- > 0) {
        shmId = attachStateToSharedMemory(&sharedState);
        if (shmId >= 0) return true;
        usleep(200000);
    }
    fprintf(stderr, "[HIP%d] shm attach failed\n", hipNumber);
    return false;
}

void HIPManager::registerPid() {
    sem_wait(&sharedState->shmLock);
    if (hipNumber == 1)
        sharedState->hip1Pid = getpid();
    else
        sharedState->hip2Pid = getpid();
    
    sharedState->sequence++;
    sem_post(&sharedState->shmLock);

    char msg[64];
    snprintf(msg, 64, "HIP%d attached pid:%d", hipNumber, getpid());
    logAction(sharedState, msg, getpid(), 0);
}

bool HIPManager::initializeWindow() {
    
    hiddenWindow.create(VideoMode(1, 1), "HIP Input", Style::None);
    hiddenWindow.setVisible(false);
    return true;
}

void HIPManager::spawnPlayerThreads() {
    for (int i = 0; i < playerCount; i++) {
        int globalIdx = playerOffset + i;
        controllers[i] = new PlayerController(sharedState, globalIdx, hipNumber/*, &input->state*/);

        if (pthread_create(&playerThreads[i], NULL, PlayerController::threadFunc, controllers[i]) != 0) {
            perror("[HIP] player thread create failed");
        } 
        else {
            char msg[64];
            snprintf(msg, 64, "HIP%d spawned thread for player %d", hipNumber, globalIdx);
            logAction(sharedState, msg, getpid(), playerThreads[i]);
        }
    }
}

void HIPManager::eventLoop() {
    
    static bool prevUp = false, prevDown = false, prevLeft = false, prevRight = false;
    static bool prevConfirm = false, prevBack = false;
    static bool prevNum1 = false, prevNum2 = false, prevNum3 = false, prevNum4 = false;
    static bool prevEscape = false;

    while (running && !sharedState->shouldExit && !sharedState->returnToMenu) {
        
        bool curUp = Keyboard::isKeyPressed(Keyboard::Up) || Keyboard::isKeyPressed(Keyboard::W);
        bool curDown = Keyboard::isKeyPressed(Keyboard::Down) || Keyboard::isKeyPressed(Keyboard::S);
        bool curLeft = Keyboard::isKeyPressed(Keyboard::Left) || Keyboard::isKeyPressed(Keyboard::A);
        bool curRight = Keyboard::isKeyPressed(Keyboard::Right);
        bool curConfirm = Keyboard::isKeyPressed(Keyboard::Return);
        bool curBack = Keyboard::isKeyPressed(Keyboard::Escape);
        bool curNum1 = Keyboard::isKeyPressed(Keyboard::Num1);
        bool curNum2 = Keyboard::isKeyPressed(Keyboard::Num2);
        bool curNum3 = Keyboard::isKeyPressed(Keyboard::Num3);
        bool curNum4 = Keyboard::isKeyPressed(Keyboard::Num4);
        bool curEscape = Keyboard::isKeyPressed(Keyboard::Escape);

        bool upPressed = curUp && !prevUp;
        bool downPressed = curDown && !prevDown;
        bool leftPressed = curLeft && !prevLeft;
        bool rightPressed = curRight && !prevRight;
        bool confirmPressed = curConfirm && !prevConfirm;
        bool backPressed = curBack && !prevBack;
        bool num1Pressed = curNum1 && !prevNum1;
        bool num2Pressed = curNum2 && !prevNum2;
        bool num3Pressed = curNum3 && !prevNum3;
        bool num4Pressed = curNum4 && !prevNum4;
        bool escapePressed = curEscape && !prevEscape;  

        prevUp = curUp;
        prevDown = curDown;
        prevLeft = curLeft;
        prevRight = curRight;
        prevConfirm = curConfirm;
        prevBack = curBack;
        prevNum1 = curNum1;
        prevNum2 = curNum2;
        prevNum3 = curNum3;
        prevNum4 = curNum4;
        prevEscape = curEscape;

        //if no key was pressed this frame then sleep and continue:
        if (!upPressed && !downPressed && !leftPressed && !rightPressed && !confirmPressed && !backPressed 
            && !num1Pressed && !num2Pressed && !num3Pressed && !num4Pressed && !escapePressed) {
            usleep(10000);
            continue;
        }

        //reading current game phase:
        sem_wait(&sharedState->shmLock);
        bool mainMenu = sharedState->isMainMenu;
        bool partySelect = sharedState->isPartySelect;
        bool battle = sharedState->isBattle;
        bool eclipsePickup = sharedState->eclipseRelicPickupActive && !sharedState->eclipseRelicPickupResolved;
        bool escapeOpen = sharedState->escapeMenuOpen;
        int weaponOwner = sharedState->weaponDropOwner;
        int turnEntity = sharedState->currentTurnEntityId;
        sem_post(&sharedState->shmLock);

        //Eclipse Relic Pickup (number keys 1-4, Escape):
        if (eclipsePickup) {
            int chosen = -1;
            if (num1Pressed) chosen = 0;
            else if (num2Pressed) chosen = 1;
            else if (num3Pressed) chosen = 2;
            else if (num4Pressed) chosen = 3;
            else if (escapePressed) chosen = -2;  

            if (chosen != -1) {
                sem_wait(&sharedState->shmLock);
                if (chosen >= 0 && chosen < sharedState->playerCount && sharedState->players[chosen].entity.isAlive)
                    sharedState->eclipseRelicPickupPlayer = chosen;

                else if (chosen == -2)
                    sharedState->eclipseRelicPickupPlayer = -1;
                
                sharedState->eclipseRelicPickupResolved = true;
                sharedState->combatPhase = 0;
                sharedState->sequence++;
                sem_post(&sharedState->shmLock);
                continue;
            }
        }

        //Main Menu / Party Select:
        if (mainMenu || partySelect) {
            sem_wait(&sharedState->keyboardInput.inputLock);
            sharedState->keyboardInput.isUp = upPressed;
            sharedState->keyboardInput.isDown = downPressed;
            sharedState->keyboardInput.isConfirm = confirmPressed;
            sharedState->keyboardInput.isBack = backPressed;
            sharedState->keyboardInput.newInput = true;
            for (int i = 0; i < maxEntities; ++i) sharedState->keyboardInput.consumed[i] = true;
            sem_post(&sharedState->keyboardInput.inputLock);
            continue;
        }

        if (battle) {

            int targetPlayer = -1;
            if (weaponOwner >= 0 && weaponOwner < sharedState->playerCount)
                targetPlayer = weaponOwner;
            else if (turnEntity >= 0 && turnEntity < sharedState->playerCount)
                targetPlayer = turnEntity;

            //determining if we are inside a sub-menu where Escape should be "back"
            bool isInSubMenu = (sharedState->combatPhase == 3 ||   //action popup
                                sharedState->combatPhase == 4 ||   //weapon drop prompt
                                sharedState->combatPhase == 5 ||   //swap-in select
                                sharedState->combatPhase == 6);    //weapon select

            //escape menu already open so forward navigation keys:
            if (escapeOpen) {
                sem_wait(&sharedState->keyboardInput.inputLock);
                sharedState->keyboardInput.isUp = upPressed;
                sharedState->keyboardInput.isDown = downPressed;
                sharedState->keyboardInput.isConfirm = confirmPressed;
                sharedState->keyboardInput.isBack = backPressed;
                sharedState->keyboardInput.newInput = true;
                for (int i = 0; i < maxEntities; ++i)
                    sharedState->keyboardInput.consumed[i] = true;
                sem_post(&sharedState->keyboardInput.inputLock);
            }
            //inside a sub-menu and forwarding Escape as "back" to the player:
            else if (targetPlayer >= 0 && isInSubMenu) {
                sem_wait(&sharedState->keyboardInput.inputLock);
                sharedState->keyboardInput.isUp = upPressed;
                sharedState->keyboardInput.isDown = downPressed;
                sharedState->keyboardInput.isLeft = leftPressed;
                sharedState->keyboardInput.isRight = rightPressed;
                sharedState->keyboardInput.isConfirm = confirmPressed;
                sharedState->keyboardInput.isBack = backPressed;   //escape = back
                sharedState->keyboardInput.newInput = true;
                for (int i = 0; i < maxEntities; ++i) sharedState->keyboardInput.consumed[i] = true;
                sharedState->keyboardInput.consumed[targetPlayer] = false;
                sem_post(&sharedState->keyboardInput.inputLock);
            }
            //normal player turn and not in a sub-menu so forward directional keys (no Escape):
            else if (targetPlayer >= 0) {
                sem_wait(&sharedState->keyboardInput.inputLock);
                sharedState->keyboardInput.isUp = upPressed;
                sharedState->keyboardInput.isDown = downPressed;
                sharedState->keyboardInput.isLeft = leftPressed;
                sharedState->keyboardInput.isRight = rightPressed;
                sharedState->keyboardInput.isConfirm = confirmPressed;
                sharedState->keyboardInput.isBack = false;
                sharedState->keyboardInput.newInput = true;
                for (int i = 0; i < maxEntities; ++i) sharedState->keyboardInput.consumed[i] = true;
                sharedState->keyboardInput.consumed[targetPlayer] = false;
                sem_post(&sharedState->keyboardInput.inputLock);
            }
            //no active player turn and not in sub-menu so open escape menu:
            else if (!isInSubMenu) {
                if (backPressed) {
                    sem_wait(&sharedState->hipRequest.hipRequestLock);
                    sharedState->hipRequest.requestPause = true;
                    sem_post(&sharedState->hipRequest.hipRequestLock);
                }
            }
        }
        usleep(10000);  //small delay to avoid 100% CPU
    }
}

void* HIPManager::inputThreadWrapper(void* arg) {
    HIPManager* self = (HIPManager*)arg;
    self->eventLoop();
    return nullptr;
}

void HIPManager::run() {
    running = true;
    
    printf("[HIP%d] run() started\n", hipNumber);
    fflush(stdout);
    usleep(200000);

    //starting the input capture thread immediately:
    pthread_t inputThread;
    pthread_create(&inputThread, NULL, inputThreadWrapper, this);
    pthread_detach(inputThread);  

    //waiting for playerCount (will be set after party selection):
    if (hipNumber == 1) {
        printf("[HIP%d] waiting for playerCount from renderer...\n", hipNumber);
        fflush(stdout);
        while (sharedState->playerCount == 0 && !sharedState->shouldExit && !sharedState->returnToMenu)
            usleep(100000);
        playerCount = sharedState->playerCount;
        printf("[HIP%d] playerCount = %d\n", hipNumber, playerCount);
        fflush(stdout);
    } 
    else {
        while (sharedState->playerCount == 0) 
            usleep(100000);
        playerCount = maxPlayers - sharedState->playerCount;
    }
    
    //waiting for entity initialization:
    printf("[HIP%d] waiting for entity initialization (maxHp > 0)...\n", hipNumber);
    fflush(stdout);
    int waitCount = 0;
    while ((sharedState->players[0].entity.maxHp == 0) && !sharedState->shouldExit && !sharedState->returnToMenu) {
        usleep(100000);
        waitCount++;
        if (waitCount > 100) {
            printf("[HIP%d] timeout waiting for entity initialization\n", hipNumber);
            break;
        }
    }
    printf("[HIP%d] entity initialization detected, maxHp = %d\n", hipNumber, sharedState->players[0].entity.maxHp);
    fflush(stdout);
    
    //spawning player threads (each will read input via keyboardInput):
    spawnPlayerThreads();
    
    //wait for player threads to finish (input thread continues independently):
    for (int i = 0; i < playerCount; i++) {
        if (playerThreads[i] != 0)
            pthread_join(playerThreads[i], NULL);
    }
    
    logAction(sharedState, "HIP exiting", getpid(), 0);
}

void HIPManager::cleanup() {
    for (int i = 0; i < maxPlayers; i++) {
        if (controllers[i]) {
            delete controllers[i];
            controllers[i] = nullptr;
        }
    }

    detachFromSharedMemory(sharedState);
    sharedState = nullptr;
    shmId = -1;
}

void HIPManager::setupSignals() {
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

void HIPManager::onSigterm(int sig) {
    (void)sig;
    if (instance) 
        instance->running = false;
}

void HIPManager::onSigusr1(int sig) {
    (void)sig;
    if (!instance || !instance->sharedState)
        return;

    int targetId = instance->sharedState->stunTargetEntityId;
    if (targetId < 0 || targetId >= maxPlayers)
        return;

    instance->sharedState->players[targetId].entity.isStunned = true;
    instance->sharedState->sequence++;

    alarm(stunDuration);
}

void HIPManager::onSigusr2(int sig) {
    (void)sig;
}

void HIPManager::onSigalrm(int sig) {
    (void)sig;
    if (!instance || !instance->sharedState)
        return;

    int targetId = instance->sharedState->stunTargetEntityId;
    if (targetId < 0 || targetId >= maxPlayers)
        return;

    instance->sharedState->players[targetId].entity.isStunned = false;
    instance->sharedState->sequence++;
}

//////////////////////////////////////////////////////////////////////////////////////////
