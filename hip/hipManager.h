//////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "../arbiter/sharedState.h"
#include "../arbiter/sharedMemoryUtilities.h"
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <SFML/Graphics.hpp>

using namespace sf;

//////////////////////////////////////////////////////////////////////////////////////////
// === INTERFACE STATE ===
//////////////////////////////////////////////////////////////////////////////////////////

struct InterfaceState {
    bool isMainMenu;
    bool isPartySelect;
    bool isBattle;
    bool isPlayerSelect;
    bool isTargetSelect;
    bool isActionPopup;
    bool isEscapeMenu;
    bool isWaveTransition;
    bool isWinScreen;
    bool isLoseScreen;
    bool isDebugOverlay;

    int selectedPlayer;
    int selectedTarget;
    int selectedAction;
    int playerCount;
    int npcCount;

    bool swapRequested;
    int swapFromIndex;
    int swapToIndex;

    int escapeMenuSelection;

    InterfaceState();
    void resetPhases();
    void setPhase(bool& phase);
};

//////////////////////////////////////////////////////////////////////////////////////////
// === PLAYER CONTROLLER ===
//////////////////////////////////////////////////////////////////////////////////////////

class PlayerController {
private:
    SharedState* sharedState;
    int playerIndex;
    int hipNumber;
    InterfaceState interface;
    bool running;

    void phasePlayerSelect();
    void phaseTargetSelect();
    void phaseActionPopup();
    void phaseEscapeMenu();
    void phaseWeaponPickup();

    int phaseWeaponSelect();
    int phaseSwapInSelect();

    void submitAction(int actionType, int targetIndex, int weaponId);
    void requestPrioritySwap(int fromIndex, int toIndex);

    bool isMyTurn();
    int topPriorityPlayer();
    int aliveEnemyCount();
    int nextAliveEnemy(int current, int direction);
    int nextAlivePlayer(int current, int direction);
    bool hasWeaponInInventory(int weaponId);
    int getInventoryWeapons(int* weaponIds, int maxCount);

    void waitForInput(bool& up, bool& down, bool& left, bool& right, bool& confirm, bool& back);

public:
    PlayerController(SharedState* sharedState, int pIndex, int hipNum/*, InputState* input*/);
    ~PlayerController();

    void start();
    static void* threadFunc(void* arg);
};

//////////////////////////////////////////////////////////////////////////////////////////
// === HIP MANAGER ===
//////////////////////////////////////////////////////////////////////////////////////////

class HIPManager {
private:
    SharedState* sharedState;
    int shmId;
    int rollNo;
    int hipNumber;
    int playerOffset;
    int playerCount;

    pthread_t playerThreads[maxPlayers];
    PlayerController* controllers[maxPlayers];
    bool running;

    static void* inputThreadWrapper(void* arg);
    RenderWindow hiddenWindow;       
    bool initializeWindow();            


    bool attachShm();
    void registerPid();
    void spawnPlayerThreads();
    void eventLoop();

    void setupSignals();
    static void onSigterm(int sig);
    static void onSigusr1(int sig);
    static void onSigusr2(int sig);
    static void onSigalrm(int sig);

    static HIPManager* instance;

public:
    HIPManager(int hipNumber, int roll);
    ~HIPManager();

    bool initialize();
    void run();
    void cleanup();
};

//////////////////////////////////////////////////////////////////////////////////////////
