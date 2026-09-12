#pragma once

#include "audio/AudioSystem.h"
#include "core/Assets.h"
#include "core/Config.h"
#include "core/SaveGame.h"
#include "effects/ParticleSystem.h"
#include "entities/Boss.h"
#include "entities/Enemy.h"
#include "entities/Player.h"
#include "entities/Projectile.h"
#include "ui/Hud.h"
#include "ui/MainMenu.h"
#include "ui/StoryScreen.h"
#include "ui/UpgradeScreen.h"
#include "upgrades/Stats.h"
#include "upgrades/UpgradeTree.h"
#include "world/Arena.h"
#include "world/DepthTheme.h"
#include "world/LevelManager.h"
#include "world/Seabed.h"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Keyboard.hpp>

#include <optional>
#include <vector>

// Owns the window and drives the main loop. The campaign runs as a small state
// machine: a level is announced, fought, cleared, then spent on upgrades.
class Game
{
public:
    Game();

    void run();

private:
    enum class State
    {
        Menu,       // the screen the game opens on
        Controls,   // what the keys and the mouse do
        Bestiary,   // the logbook: what has been met, and what it was
        Options,    // sound and difficulty
        Story,      // an interlude between depths
        LevelIntro, // title card, nothing spawns yet
        Fighting,
        LevelClear, // objective met, the water settles
        Upgrading,  // the tree, between levels
        GameOver,
        Victory,    // the last authored level is behind you
        Credits,
    };

    // What a level cost and what it paid, totalled while it is being fought
    // and shown once it is over. Reset by beginLevel, so a repeat of a level
    // reads as its own attempt rather than the sum of every attempt.
    struct RunTally
    {
        int   kills = 0;
        int   salvage = 0;
        int   shotsFired = 0;
        int   shotsHit = 0;
        int   damageTaken = 0;
        float seconds = 0.0f;
    };

    // An escort drone: orbits, picks its own targets, fires on its own clock.
    struct Drone
    {
        sf::Sprite sprite;
        float angle;
        float fireTimer;
    };

    // An expanding ring left where something was hit. Purely feedback: it is
    // the difference between "did that land" and seeing it land.
    struct HitRing
    {
        sf::Vector2f position;
        float        age;
        float        life;
        float        radius;
        sf::Color    colour;
    };

    // A dropped mine: arms after a moment so it cannot detonate in your face.
    struct Mine
    {
        sf::Sprite   sprite;
        sf::Vector2f position;
        float        life;
        float        arm;
    };

    // A crack in the floor that breathes. It glows brighter and brighter while
    // it builds pressure, then blows a column of scalding water that hurts
    // whatever is standing on it, the player and the creatures alike. The arena
    // stops being only a backdrop: there are places in it you cannot stand.
    struct Vent
    {
        sf::Vector2f position;
        float        radius;
        float        timer;   // seconds until it blows
        float        interval;
        float        erupting; // seconds of eruption left, zero while dormant
    };

    // A clutch of egg cases cemented to rock out near the reef. It hatches
    // until it is killed, it does not move, and it is not in sight from the
    // middle of the basin - which is the point of it. A level built round
    // these is a level you have to go through rather than stand in.
    // A salvage pod. It sits where it was dropped until something swims into
    // it, then it is towed behind the stern - slower, louder, and let go of the
    // moment anything lands a hit. Delivering one means reaching the mouth the
    // dive started at, which is usually the far side of the basin.
    struct Cargo
    {
        sf::Sprite   sprite;
        sf::Vector2f position;
        bool         carried;
        bool         delivered;
        float        bob;
    };

    struct Nest
    {
        sf::Sprite   sprite;
        sf::Vector2f position;
        int          health;
        int          maxHealth;
        float        spawnTimer;
        float        flash;
        bool         alive;
    };

    void processEvents();
    void update(float deltaTime);
    void render();

    void createWindow(bool fullscreen);
    void toggleFullscreen();
    void applyLetterboxView();
    void updateCamera(float deltaTime);
    sf::Vector2f randomVisiblePoint() const;

    // --- campaign flow ---
    void beginLevel(int number);
    void onLevelComplete();
    void advanceLevel();
    void startNewRun();
    void diveIntoSelected();
    void onPlayerDied();
    void persist();
    int  getDepth() const;

    // --- combat ---
    // The jet, and two guns that are not the jet with different numbers.
    // Cycled in the water rather than chosen in the hub, because which one is
    // right changes between one room and the next.
    enum class FireMode { Jet, Harpoon, Pulse };

    int      modeCount() const;
    FireMode modeAt(int slot) const;
    void     cycleFireMode();
    static const char* modeName(FireMode mode);

    void fireHarpoon();
    void firePulse();
    void updateHarpoon(float deltaTime);
    void drawHarpoon();

    void handleShooting(float deltaTime);
    void fireJet(sf::Vector2f muzzle, sf::Vector2f direction, float chargeFactor);
    void fireTorpedo();
    void performDash();
    void emitThrusterTrail(float deltaTime);
    void spawnEnemy(EnemyType type);
    sf::Vector2f pickSpawnPosition();
    void updateProjectiles(float deltaTime);
    void updateTorpedoes(float deltaTime);
    void updateDrones(float deltaTime);
    void updateMines(float deltaTime);
    void updateOrbits(float deltaTime);
    void drawOrbits();
    void placeVents();
    void updateVents(float deltaTime);
    void drawVents();

    void placeCargo();
    void updateCargo(float deltaTime);
    void drawCargo();
    void dropCargo();

    void placeNests();
    void updateNests(float deltaTime);
    void drawNests();

    // Running dark. The lamp is what lets a creature find you from across the
    // basin; with it off you are nearly blind but so is everything else, and
    // firing gives you away anyway. It is the other way to play a level.
    void toggleLamp();
    float noticeRange() const;
    void updateSonar(float deltaTime);
    void updateEnemies(float deltaTime);
    void updateBoss(float deltaTime);
    void spawnAmbience(float deltaTime);

    // Something big, a long way off, on a long random timer. Never anything
    // the player can shoot: it is there to make a quiet stretch feel wrong.
    void callFromTheDark(float deltaTime);
    void handleCollisions();
    void hurtPlayer(int amount);
    void killEnemy(Enemy& enemy);
    void flushPendingShots();
    void removeDeadEntities();

    // Shared by the pressure burst, reactive plating, mines and clusters.
    // One door for every payout, so the salvage bonus and the repeat-water
    // penalty cannot be forgotten at one of the call sites.
    void awardSalvage(int base);

    // Moves the dive's hold into the bank. Surfacing is what makes salvage
    // yours: clearing a level does it, walking out of one on purpose does it,
    // and drowning does the opposite.
    void bankSalvage();

    // Feel, rather than rules. A jolt of the camera and a few frames of frozen
    // time are what make a torpedo land like a torpedo; the red wash is how the
    // player finds out they have been bitten without reading the health pips.
    void addShake(float strength);
    void addHitStop(float seconds);
    void updateFeel(float deltaTime);

    // The three things a moment gets made out of when it matters: the world
    // slows down, the screen is washed in a colour, and a ring goes out.
    // Ordinary hits use none of them; a boss changing its mind uses all three.
    void addSlowMotion(float seconds, float scale);
    void addFlash(sf::Color colour, float strength);
    void bigMoment(sf::Vector2f at, sf::Color colour, float shake, float slow);

    void onBossActChanged(int act);

    // The moves that are not shots. The creature asks; this is where the
    // world actually does it.
    void applyBossMove(const BossMove& move);
    void updateBossMoves(float deltaTime);
    void drawBossMoves();

    // The ring that closes on a creature about to commit, and the line it is
    // about to take. Drawn here rather than in Boss for the same reason the hit
    // rings are: the creature is the thing, this is the game telling you about
    // the thing.
    void drawBossTell();

    // Small triangles at the edge of the screen for whatever is hunting from
    // outside it. The basin is far larger than the view, so without these a
    // creature can cross the whole arena and bite before it is ever seen.
    void drawThreatArrows();

    // The basin drawn small in the corner. With a generated map there is no
    // longer a shape the player can hold in their head, and a hunt across one
    // without a map is not tense, it is tedious.
    void drawMinimap();
    void drawPauseOverlay();

    // The three flat screens hanging off the title: what the controls are, what
    // can be turned, and who to blame. None of them owns any state beyond a row
    // cursor, so they live here rather than in classes of their own.
    void drawControls();
    void drawBestiary();
    void drawOptions();
    void drawVictory();
    void drawCredits();

    void handleOptionsKey(sf::Keyboard::Scancode key);
    void applySettings();

    const config::DifficultyDef& difficulty() const;

    // What a creature sounds like when it is hit and when it stops. A basin
    // full of different animals used to sound like one animal being killed
    // over and over.
    static Sfx deathVoice(EnemyType type);
    static Sfx hitVoice(EnemyType type);

    void areaDamage(sf::Vector2f centre, float radius, int damage);
    void addHitRing(sf::Vector2f at, float radius, sf::Color colour);
    void updateHitRings(float deltaTime);
    void spawnSplit(sf::Vector2f at, sf::Vector2f incoming, int fromEnemyId);
    int  getJetDamage() const;

    // Shows a chapter if it has never been seen, otherwise goes straight on.
    void maybePlayStory(int chapter, State next);

    void openMenu();
    void activateMenuChoice();

    void applyUpgrades();
    void handleUpgradeClick();
    void stepSelectedLevel(int delta);
    void stripHull();
    bool upgradesVisible() const;

    sf::Vector2f getMouseWorldPosition() const;

    sf::RenderWindow m_window;
    sf::View         m_camera; // follows the sub through a basin larger than the screen
    sf::View         m_ui;     // fixed 1280x720 for the HUD and the vignette
    sf::Vector2f     m_cameraCentre;
    sf::Clock        m_clock;
    bool             m_fullscreen;

    // Declared before anything holding a sprite: sprites only borrow their
    // texture, so the assets have to outlive them.
    Assets      m_assets;
    AudioSystem m_audio;

    // Stats before the player, because the player keeps a reference to them.
    Stats       m_playerStats;
    UpgradeTree m_tree;

    Arena         m_arena;
    Seabed        m_seabed;
    LevelManager  m_level;
    Player        m_player;
    Hud           m_hud;
    MainMenu      m_menu;
    StoryScreen   m_story;
    UpgradeScreen m_upgradeScreen;

    std::vector<Projectile> m_playerShots;
    std::vector<Projectile> m_torpedoes;
    std::vector<Projectile> m_enemyShots;

    // Anything spawned while a shot list is being walked goes here first and is
    // appended at the end of the frame. Pushing straight into the live vector
    // reallocates it and leaves the loop holding a dangling reference.
    std::vector<Projectile> m_pendingPlayerShots;
    std::vector<Projectile> m_pendingEnemyShots;
    std::vector<Enemy>      m_enemies;
    std::vector<Drone>      m_drones;
    std::vector<Mine>       m_mines;
    std::vector<Vent>       m_vents;
    std::vector<Nest>       m_nests;
    std::vector<Cargo>      m_cargo;
    std::vector<HitRing>    m_hitRings;
    std::optional<Boss>     m_boss;

    // The harpoon is kept apart from the shot list: there is only ever one, it
    // has its own collision rules, and while it is dragging the sub it is not
    // a projectile at all.
    std::optional<Projectile> m_harpoon;
    sf::Vector2f              m_harpoonFrom;
    sf::Vector2f              m_harpoonTo;
    float                     m_harpoonPull = 0.0f;

    int   m_fireSlot = 0;     // which fitted mode is selected
    float m_modeTimer = 0.0f; // the cooldown the non-jet modes run on

    // Two layers: ambience sits under the entities, impacts sit over them.
    ParticleSystem m_ambience;
    ParticleSystem m_effects;

    SaveGame m_save;
    State    m_state;
    float    m_stateTimer;
    float    m_menuTime;
    State    m_afterStory;

    int   m_salvage;      // banked, and safe
    int   m_diveSalvage;  // earned this dive, and lost if you drown with it
    int   m_lostSalvage;  // what the last death cost, for the banner
    bool  m_showUpgrades;

    // Water that has already been cleared pays a fraction, so the level being
    // a repeat is something the whole run has to know about.
    bool  m_rerun;

    // A stripped hull cannot be un-stripped, so the key has to be pressed
    // twice: the first press only puts the warning on the screen.
    bool  m_respecArmed;

    // The fight stops; the water does not.
    bool  m_paused;

    RunTally m_tally;

    float m_shake;    // pixels of camera jitter left, decaying
    float m_hitStop;  // seconds the simulation is held still
    float m_hurtFlash; // 1 at the moment of a bite, fading to 0
    float m_charge;    // seconds the fire button has been held down
    float m_orbitAngle; // where the defensive motes are in their circle

    bool  m_lampOn;    // the sub's own light, and whether anything can see you
    float m_noiseTimer; // seconds of having given yourself away by firing

    float     m_slowTimer;   // seconds of slow motion left
    float     m_slowScale;   // how slow, while it lasts
    float     m_flash;       // 1 at the moment, fading to 0
    sf::Color m_flashColour;
    int       m_bossAct;     // the act the boss was in last frame

    // Everything a boss special leaves behind for a few seconds. Held here
    // rather than in Boss because every one of them is about the world and the
    // sub rather than about the creature.
    sf::Vector2f m_pushTo;
    float        m_pushTime = 0.0f;
    sf::Vector2f m_wellAt;
    float        m_wellTimer = 0.0f;
    sf::Vector2f m_eruptAt;
    float        m_eruptTimer = 0.0f;
    float        m_siltTimer = 0.0f;
    float        m_grabArmed = 0.0f;  // a lunge that will grab if it connects
    float        m_grabTimer = 0.0f;  // seconds it has you for
    float        m_grabBite = 0.0f;
    bool      m_bossTelling; // whether it was already announcing an attack
    int       m_streak;      // kills since the last time something touched you
    int       m_bestStreak;
    int       m_blocks;      // hits that did not land, for the summary
    int       m_carrying;    // which pod is under tow, or -1
    int       m_hullLost;    // how many systems this dive has already cost
    float     m_sirenTimer;  // the hull alarm, once there is almost none left

    int   m_optionRow;    // which line of the options screen is selected
    int   m_beastRow;     // which animal the logbook is open at
    float m_creditsScroll;
    bool  m_justBeatCampaign; // the clear that ends the campaign, seen once

    float m_lastSpawnAngle;
    float m_snowTimer;
    float m_bubbleTimer;
    float m_thrusterTimer;
    float m_mineTimer;
    float m_navTimer = 0.0f;  // when the flow field was last swept
    float m_sonarTimer;
    float m_sonarSweep; // 0 when idle, counts down through a ping

    // Seconds until something out in the dark calls. Deliberately long.
    float m_callTimer;

    sf::CircleShape    m_sonarRing;
    sf::CircleShape    m_sonarMark;
    sf::RectangleShape m_deathOverlay;
};
