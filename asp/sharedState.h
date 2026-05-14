//////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <semaphore.h>
#include <time.h>
#include <sys/types.h>

//////////////////////////////////////////////////////////////////////////////////////////

static const int maxPlayers = 4;
static const int maxEnemies = 9;
static const int inventorySlots = 20;
static const int longTermStorageSlots = 20;
static const int logSize = 32;
static const int maxEntities = 13; 

static const int shmKey = 245826;

//actions:
static const int actionStrike = 0;
static const int actionExhaust = 1;
static const int actionUseWeapon = 2;
static const int actionSwapIn = 3;
static const int actionHeal = 4;
static const int actionSkip = 5;
static const int actionUltimate = 6;

static const int actionEnemyStrike = 0;
static const int actionEnemySkip = 1;

//artifacts:
static const int solarCoreArtifact = 0;
static const int lunarBladeArtifact = 1;
static const int eclipseRelicArtifact = 2;

//sentinel values:
static const int noHolder = -1;
static const int noWeapon = -1;
static const int noEntity = -1;
static const int noTarget = -1;

//game rules:
static const int winKillCount = 10;
static const int stunDuration = 3; 
static const int ultimateDuration = 10;
static const int enemyTimeout = 3;
static const int playerMaxStamina = 100;
static const int enemyMaxStamina = 150;
static const int healPercent = 10;
static const int skipStamina = 50;

//////////////////////////////////////////////////////////////////////////////////////////

struct WeaponData {
    char name[32];
    int slotSize;
    int damage;
    int id;
};

static const int weaponCount = 9;

static const WeaponData weaponTable[weaponCount] = {
    {"Solar Core", 10, 95, 0},
    {"Lunar Blade", 10, 90, 1},
    {"Iron Halberd", 7, 55, 2},
    {"Venom Dagger", 4, 30, 3},
    {"Thunderstaff", 6, 50, 4},
    {"Obsidian Axe", 5, 45, 5},
    {"Frostbow", 6, 48, 6},
    {"Splinter Stick", 2, 12, 7},
    {"Eclipse Rune", 10, 100, 8}
};

//////////////////////////////////////////////////////////////////////////////////////////

struct InventoryData {
    int slots[inventorySlots];
    int longTermStorage[longTermStorageSlots];
    int longTermStorageCount; 
};

//////////////////////////////////////////////////////////////////////////////////////////

struct EntityData {
    int id;                 //0-3 for players, 4-12 for enemies
    char name[32];          //display name
    int currentHp;    
    int maxHp;
    int currentStamina;
    int maxStamina;
    int speed;
    int damage;
    bool isAlive;
    bool isStunned;
    bool hasTurn;
    bool actionReady;
    pid_t pid;              //process id of controlling process
    pthread_t threadId;     //thread id of controlling thread (for logging)
};

//////////////////////////////////////////////////////////////////////////////////////////

struct PlayerData {
    EntityData entity;
    InventoryData inventory;
    int actionType;    //choosen action
    int actionTarget;  //index into enemies
    int weaponUsed;    //will store weapon id if actionUseWeapon
    int hipOwner;      //0 = HIP1, 1 = HIP2
    int spriteType;    //0 or 1, set by arbiter, read by renderer
};

//////////////////////////////////////////////////////////////////////////////////////////

struct EnemyData {
    EntityData entity;
    int weaponId;     //no weapon if none
    int actionType;   //chosen action
    int actionTarget; //index into players
    int spriteType;   //0-8 random per wave
};

//////////////////////////////////////////////////////////////////////////////////////////

struct ResourceTable {
    int holder[3];                //entity id holding artifact
    bool exists[3];               //exists[2] = eclipse relic spawned
    int waitingFor[maxEntities];  //entity i waiting for artifact id
};

//////////////////////////////////////////////////////////////////////////////////////////

struct ActionLogEntry {
    char message[128];
    pid_t pid;
    pthread_t threadId;
    time_t timestamp;
    bool valid;
};

//////////////////////////////////////////////////////////////////////////////////////////

struct HIPRequest {
    bool requestPause;    //HIP asks Arbiter to open escape menu and pause the game
    sem_t hipRequestLock;
};

//////////////////////////////////////////////////////////////////////////////////////////

struct KeyboardInput {
    bool isUp;
    bool isDown;
    bool isLeft;
    bool isRight;
    bool isConfirm;
    bool isBack;
    bool isEscape;
    bool newInput;
    bool consumed[maxEntities];
    sem_t inputLock;

    int menuSelection;
    bool menuConfirmed;
    int partySize;
    bool gameStartRequested;
};

//////////////////////////////////////////////////////////////////////////////////////////

struct SharedState {

    //semaphores:
    sem_t shmLock;       //general state guard
    sem_t turnSem;       //arbiter->entity: your turn 
    sem_t actionSem;     //entity->arbiter: done 
    sem_t artifactLock;  //resource table guard
    sem_t logLock;       //action log guard

    
    //entity data:
    PlayerData players[maxPlayers];
    EnemyData enemies[maxEnemies];
    int playerCount;
    int enemyCount;


    //turn tracking:
    int currentTurnEntityId;
    int readyQueue[maxEntities];
    int readyQueueHead;
    int readyQueueTail;
    int readyQueueCount;
    bool inReadyQueue[maxEntities];

    
    //game progress tracking:
    int enemiesDefeated;
    int waveNumber;
    int playersAlive;
    int enemiesAlive;
    int totalEnemiesThisWave;

    
    //artifact tracking:
    ResourceTable artifacts;

    
    //control flags:
    bool shouldExit;
    bool returnToMenu;   
    bool gamePaused;
    bool escapeMenuOpen;
    bool ultimateActive;
    bool waveTransition;
    bool gameOver;
    bool playerWon;
    bool gameReady;
    bool eclipseRelicSpawned;


    //stun coordination:
    pid_t stunTargetPid;
    int stunTargetEntityId;

    
    //process tracking for cleanup:
    pid_t arbiterPid;
    pid_t hip1Pid;
    pid_t hip2Pid;      //-1 if single player
    pid_t aspPid;


    //mode flags:
    bool multiplayerMode;
    bool pvpMode;
    bool enemyEnabled;
    int activeHipCount;


    //teleportation state:
    bool teleportActive;
    int teleportAttackerId;
    bool teleportAttackerIsPlayer;
    int teleportAttackerSpriteType;
    int teleportTargetId;
    bool teleportTargetIsPlayer;
    bool teleportPlayAttack;

    //attack animation coordination:
    bool attackAnimationInProgress;
    
    //weapon drop selection:
    bool weaponDropActive;
    int weaponDropId;
    int weaponDropOwner; // player index
    bool weaponDropResolved;
    bool weaponDropAccepted;

    //eclipse relic dynamic pickup:
    bool eclipseRelicPickupActive;
    bool eclipseRelicPickupResolved;
    int eclipseRelicPickupPlayer;  //player index who picks it up; -1 = declined (enemy gets it)

    
    //action log:
    ActionLogEntry log[logSize];
    int logHead;
    int logCount;


    //sequence number for log entries to maintain order:
    int sequence;


    bool isMainMenu;
    bool isPartySelect;
    bool isBattle;
    bool isGameOver;

    //Combat UI state which is written by HIP player thread, read by renderer (display only)
    int combatPhase;          // 0=idle, 1=player_sel, 2=target_sel, 3=action_popup
    int combatActingPlayer;   // player index whose turn it is
    int combatSelectedTarget; // enemy index currently highlighted
    int combatSelectedAction; // action index highlighted in popup

    KeyboardInput keyboardInput;

    //HIP -> Arbiter control requests (HIP writes, Arbiter applies to game state)
    HIPRequest hipRequest;
};

//////////////////////////////////////////////////////////////////////////////////////////
