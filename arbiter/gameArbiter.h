//////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "sharedState.h"
#include "sharedMemoryUtilities.h"
#include "stats.h"
#include <SFML/Graphics.hpp>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <execinfo.h>

using namespace sf;

//////////////////////////////////////////////////////////////////////////////////////////
// === ANIMATION STRUCTURES ===
//////////////////////////////////////////////////////////////////////////////////////////

struct AnimationDetails {
    const char* idlePath;
    int idleFrameWidth;
    int idleFrameHeight;
    int idleFrames;

    const char* attackPath;
    int attackFrameWidth;
    int attackFrameHeight;
    int attackFrames;
};

//Player sprites: 4 distinct characters mapped by spriteType % 4
static const AnimationDetails animationPlayer1 = {
    "arbiter/assets/players/crono_idle.png", 22, 35, 5,
    "arbiter/assets/players/crono_attack.png", 31, 48, 5
};
static const AnimationDetails animationPlayer2 = {
    "arbiter/assets/players/frog_idle.png", 28, 37, 4,
    "arbiter/assets/players/frog_attack.png", 32, 45, 5
};
static const AnimationDetails animationPlayer3 = {
    "arbiter/assets/players/magus_idle.png", 30, 33, 2,
    "arbiter/assets/players/magus_attack.png", 32, 32, 4
};
static const AnimationDetails animationPlayer4 = {
    "arbiter/assets/players/slash_idle.png", 16, 48, 3,
    "arbiter/assets/players/slash_attack.png", 42, 42, 3
};

//Enemy sprites: 3 types
static const AnimationDetails animationEnemyFrog = {
    "arbiter/assets/enemies/cybot_idle.png", 36, 56, 3,
    "arbiter/assets/enemies/cybot_attack.png", 60, 60, 4
};
static const AnimationDetails animationEnemyMagus = {
    "arbiter/assets/enemies/goblin1_idle.png", 27, 32, 3,
    "arbiter/assets/enemies/goblin1_attack.png", 51, 34, 3
};
static const AnimationDetails animationEnemySlash = {
    "arbiter/assets/enemies/macabre_idle.png", 34, 32, 3,
    "arbiter/assets/enemies/macabre_attack.png", 55, 48, 3
};

//player lookup: index by spriteType % 4
static const AnimationDetails* playerAnimation[4] = {
    &animationPlayer1, &animationPlayer2,
    &animationPlayer3, &animationPlayer4
};

//enemy lookup: index by spriteType % 3
static const AnimationDetails* enemyAnimation[3] = {
    &animationEnemyFrog,
    &animationEnemyMagus,
    &animationEnemySlash
};

// Display sizes on screen
static const float playerDisplayWidth = 64.f;
static const float playerDisplayHeight = 64.f;
static const float enemyDisplayWidth = 64.f;
static const float enemyDisplayHeight = 64.f;

//////////////////////////////////////////////////////////////////////////////////////////
// === ANIMATION SYSTEM ===
//////////////////////////////////////////////////////////////////////////////////////////

class Animation {
private:
    int numAnimations;
    Texture* textures;
    Sprite* sprites;
    Sprite* activeSprite;
    int spriteWidth;
    int spriteHeight;
    int* sheetWidths;
    bool** conditions;
    int currentX;
    int currentAnimation;
    Clock animClock;
    bool isOneShot;
    bool finished;

public:
    Animation(int numAnimations, const char** paths, int spriteWidth, 
        int* totalWidths, bool** conditions, int spriteHeight = 64);
    ~Animation();

    Sprite* animate(float offsetX, float posX, float posY);
    void setOneShot(bool val);
    bool isFinished();
    // void reset();
    // int getCurrentAnimation();
    Sprite* getActiveSprite();
};

//////////////////////////////////////////////////////////////////////////////////////////

class AnimationManager {
private:
    static const int maxEntries = 64;
    
    struct AnimationEntry {
        Animation* animation;
        Vector2f pos;
        bool active;
        bool isOneShot;
        char tag[32];
    };
    
    AnimationEntry entries[maxEntries];
    int count;

    int findEntry(const char* tag);

public:
    AnimationManager();
    ~AnimationManager();

    void play(const char* tag, const AnimationDetails& spec, bool useAttack, 
        Vector2f pos, bool isOneShot, bool** conditions);
    void update(float dt);
    void draw(RenderWindow& window);
    bool isPlaying(const char* tag);
    void stop(const char* tag);
    void stopAll();
    void setPosition(const char* tag, Vector2f pos);
    void setScale(const char* tag, float sx, float sy);
};

//////////////////////////////////////////////////////////////////////////////////////////
// === TURN SCHEDULER ===
//////////////////////////////////////////////////////////////////////////////////////////

class TurnScheduler {
private:
    SharedState* sharedState;

    bool isReady(int entityId);
    bool isEligible(int entityId);
    int getEntityPid(int entityId);
    void enqueueReady(int entityId);

public:
    TurnScheduler(SharedState* s);
    int pickNext();
    // void swapPlayerPriority(int playerIndexA, int playerIndexB);
    void clearTurnFlags(int entityId);
    // bool anyReady();
};

//////////////////////////////////////////////////////////////////////////////////////////
// === STAMINA TIMER ===
//////////////////////////////////////////////////////////////////////////////////////////

class StaminaTimer {
private:
    SharedState* sharedState;
    bool running;
    void tick();
    void enqueueReady(int entityId);

public:
    StaminaTimer(SharedState* sharedState);

    void start();
    void stop();
    bool isRunning();

    static void* threadFunc(void* arg);
};

//////////////////////////////////////////////////////////////////////////////////////////
// === INVENTORY ALLOCATOR ===
//////////////////////////////////////////////////////////////////////////////////////////

class InventoryAllocator {
private:
    SharedState* sharedState;

    int findFreeBlock(InventoryData* inv, int size);
    int findSmallestWeapon(InventoryData* inv);
    int getWeaponSize(int weaponId);
    int getWeaponStart(InventoryData* inv, int weaponId);
    void clearSlots(InventoryData* inv, int slotStart, int size);
    void writeSlots(InventoryData* inv, int slotStart, int weaponId, int size);
    bool toLongTermStorage(InventoryData* inv, int slotStart);

public:
    InventoryAllocator(SharedState* sharedState);

    bool allocateWeapon(int playerIndex, int weaponId);
    bool swapIn(int playerIndex, int weaponId);
    void removeWeapon(int playerIndex, int weaponId);
    bool hasWeapon(int playerIndex, int weaponId);
    bool hasUltimateWeapons(int playerIndex);
    void logInventory(int playerIndex);
};

//////////////////////////////////////////////////////////////////////////////////////////
// === DEADLOCK MONITOR ===
//////////////////////////////////////////////////////////////////////////////////////////

class DeadlockMonitor {
private:
    SharedState* sharedState;
    InventoryAllocator* inventoryAllocator;
    bool running;

    int detectDeadlock();
    bool dfsHasCycle(int entityId, int* visited, int* parent);
    int pickVictim(int* parent, int cycleNode);
    void resolveDeadlock(int victimEntityId);
    int getHeldArtifact(int entityId);
    int getEntityPid(int entityId);

public:
    DeadlockMonitor(SharedState* sharedState, InventoryAllocator* inventoryAllocator);

    void start();
    void stop();
    bool isRunning();

    static void* threadFunc(void* arg);
};

//////////////////////////////////////////////////////////////////////////////////////////
// === RENDERER ===
//////////////////////////////////////////////////////////////////////////////////////////

static const int windowWidth = 1366;
static const int windowHeight = 720;
static const int hudHeight = 50;
static const int logPanelHeight = 160;
static const int battleYStart = hudHeight;
static const int battleHeight = windowHeight - hudHeight - logPanelHeight;

static const int playerAreaX = 0;
static const int playerAreaW = (int)(windowWidth * 0.4f);
static const int enemyAreaX = (int)(windowWidth * 0.6f);
static const int enemyAreaW = (int)(windowWidth * 0.4f);
static const int centerX = (int)(windowWidth * 0.4f);
static const int centerW = (int)(windowWidth * 0.2f);

static const int barWidth = 60;
static const int hpBarHeight = 8;
static const int staminaBarHeight = 5;
static const int barYOffsetHp = -20;
static const int barYOffsetSp = -10;

static const float circleRadius = 24.f;
static const float circleThickness = 4.f;

static const int logFontSize = 14;
static const int logMaxVisible = (logPanelHeight - 24) / (logFontSize + 3);

struct Particle {
    CircleShape shape;
    Vector2f velocity;
    float lifetime;
    float maxLifetime;
    bool active;
};
static const int maxParticles = 200;

struct EntityAnimationState {
    bool isIdle;
    bool isAttacking;
    bool isDead;
    bool attackTriggered;
    bool deathTriggered;
};

struct TeleportState {
    bool active;
    Vector2f startPos;
    Vector2f targetPos;
    Vector2f currentPos;
    float progress;
    bool isPlayer;
    int entityId;
    int spriteType;
    bool returning;
    bool playAttack;
};

class Renderer {
private:
    SharedState* sharedState;
    RenderWindow window;
    Font font;
    AnimationManager animationManager;

    Texture backgroundTexture;
    Sprite backgroundSprite;

    bool debugOverlay;
    bool showActionLog;
    int lastSequence;

    EntityAnimationState animStates[maxEntities];
    bool idleConditions[maxEntities];

    Particle particles[maxParticles];
    TeleportState teleport;

    int menuSelected;
    int partySelected;
    int escapeMenuSelection;

    struct RenderSnapshot {
        
        int waveNumber, enemiesDefeated, playersAlive, playerCount, enemyCount;
        
        PlayerData players[maxPlayers];
        EnemyData  enemies[maxEnemies];
        
        bool gamePaused, waveTransition, gameOver, playerWon, gameReady;
        bool escapeMenuOpen, weaponDropActive;
        int weaponDropId, weaponDropOwner;
        
        int currentTurnEntityId, combatPhase;
        int combatActingPlayer, combatSelectedTarget, combatSelectedAction;
        
        bool teleportActive;
        int teleportAttackerId, teleportAttackerSpriteType, teleportTargetId;
        bool teleportAttackerIsPlayer, teleportTargetIsPlayer, teleportPlayAttack;
        
        bool eclipseRelicPickupActive;
        
        int sequence;
        ResourceTable artifacts;
        ActionLogEntry log[logSize];
        int logCount, logHead;
    };
    RenderSnapshot snap;
    void captureSnapshot();

    Vector2f playerScreenPos(int playerIndex);
    Vector2f enemyScreenPos(int enemyIndex);

    void buildIdleTag(char* buf, bool isPlayer, int index);
    void buildAttackTag(char* buf, bool isPlayer, int index);
    void buildDeathTag(char* buf, bool isPlayer, int index);

    void updateAnimStates();
    void triggerAttack(int entityId, bool isPlayer, int spriteType);
    void triggerDeath(int entityId, bool isPlayer, int spriteType);

    void triggerTeleport(int attackerEntityId, bool attackerIsPlayer, int attackerSpriteType, int targetEntityId, bool targetIsPlayer, bool playAttack);
    void updateTeleport(float dt);
    void drawTeleport();

    void spawnDeathParticles(Vector2f pos, bool isPlayer);
    void updateParticles(float dt);
    void drawParticles();

    void drawBackground();
    void drawHUD();
    void drawCenterPanel();
    void drawPlayers();
    void drawEnemies();
    void drawEntity(EntityData* entity, Vector2f pos, bool isPlayer, int spriteType, bool yellowCircle, bool redCircle);
    void drawHealthBar(Vector2f pos, int hp, int maxHp, int spriteHeight);
    void drawStaminaBar(Vector2f pos, int stamina, int maxStamina, int spriteHeight);
    void drawSelectionCircle(Vector2f pos, Color color, int spriteHeight);
    void drawStunIndicator(Vector2f pos, int spriteHeight);
    void drawActionLog();
    void drawDebugOverlay();
    void drawLine(const char* txt, float x, float& y);

    void drawEscapeMenu();
    void drawWaveTransition();
    void drawWinScreen();
    void drawLoseScreen();

    void drawCombatUI();
    void drawActionMenu();
    void drawSwapInMenu();
    void drawWeaponSelectMenu();
    void drawEclipseRelicPickup();

    void drawMainMenu();
    void drawPartySelect();

    void handleMenuInput(Event& event);
    void handlePartySelectInput(Event& event);
    void handleBattleInput(Event& event);

    bool isGameReadyForDrawing();

public:
    Renderer(SharedState* s);
    ~Renderer();

    bool initialize();
    void loop();
    void close();
    bool isOpen();

    static void* threadFunc(void* arg);
};

//////////////////////////////////////////////////////////////////////////////////////////
// === GAME ARBITER ===
//////////////////////////////////////////////////////////////////////////////////////////

class GameArbiter {
private:
    SharedState* sharedState;
    int shmId;
    int rollNo;

    pid_t hip1Pid;
    pid_t hip2Pid;
    pid_t aspPid;

    pthread_t staminaThread;
    pthread_t deadlockThread;
    pthread_t renderThread;
    pthread_t gameLoopThread;
    pthread_t initThread;
    pthread_t monitorThread;

    bool running;

    StaminaTimer* staminaTimer;
    DeadlockMonitor* deadlockMonitor;
    TurnScheduler* turnScheduler;
    InventoryAllocator* inventoryAllocator;
    Renderer* renderer;

    //process management
    bool spawnHIP(int hipNumber);
    bool spawnASP();
    void waitForChildren();
    void killChildren();

    //thread launchers
    bool startStaminaThread();
    bool startDeadlockThread();
    bool startRenderThread();

    //turn logic
    void grantTurn(int entityId);
    bool waitForAction();
    void commitAction(int entityId);
    void handlePlayerAction(int playerIndex);
    void handleEnemyAction(int enemyIndex);
    void resolveWeaponDrop();
    void triggerWeaponDrop(int killerPlayerIndex);
    int pickRandomAliveEnemy();

    //game flow
    void initializeEntities();
    void initializeWave();
    void checkWinLoss();
    void handleWaveEnd();
    void handleGameOver(bool won);

    //artifact system
    bool tryAcquireArtifact(int playerIndex, int artifactId);
    void releaseArtifact(int playerIndex, int artifactId);
    void checkEclipseRelicSpawn();
    void resolveEclipseRelicPickup();
    bool isUniqueWeaponInCirculation(int weaponId);

    //hip request processing
    void processHipRequests();

    //stun delivery
    void deliverStun(int targetEntityId);

    //signal setup
    void setupSignals();
    static void onSigterm(int sig);
    static void onSigalrm(int sig);
    static void onSigusr2(int sig);

    static void* staminaThreadFunc(void* arg);
    static void* deadlockThreadFunc(void* arg);
    static void* renderThreadFunc(void* arg);
    static void* gameLoopThreadFunc(void* arg);
    static void* initThreadFunc(void* arg);

    static GameArbiter* instance;

    struct TimeoutArgs { 
        SharedState* state; 
        int entityId; 
    };
    struct MonitorArgs { 
        GameArbiter* arbiter; 
    };
    static void* monitorThreadFunc(void* arg);
    static void* handleEnemyTimeout(void* arg);

public:
    GameArbiter(int rollNumber);
    ~GameArbiter();

    bool initialize();
    void run();
    void cleanup();
    void resetForNewGame();
};

//////////////////////////////////////////////////////////////////////////////////////////
