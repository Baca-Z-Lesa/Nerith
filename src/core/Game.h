#pragma once

#include "audio/AudioSystem.h"
#include "core/Assets.h"
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
        Story,      // an interlude between depths
        LevelIntro, // title card, nothing spawns yet
        Fighting,
        LevelClear, // objective met, the water settles
        Upgrading,  // the tree, between levels
        GameOver,
    };

    // An escort drone: orbits, picks its own targets, fires on its own clock.
    struct Drone
    {
        sf::Sprite sprite;
        float angle;
        float fireTimer;
    };

    // A dropped mine: arms after a moment so it cannot detonate in your face.
    struct Mine
    {
        sf::Sprite   sprite;
        sf::Vector2f position;
        float        life;
        float        arm;
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
    void handleShooting();
    void fireTorpedo();
    void performDash();
    void emitThrusterTrail(float deltaTime);
    void spawnEnemy(EnemyType type);
    sf::Vector2f pickSpawnPosition();
    void updateProjectiles(float deltaTime);
    void updateTorpedoes(float deltaTime);
    void updateDrones(float deltaTime);
    void updateMines(float deltaTime);
    void updateSonar(float deltaTime);
    void updateEnemies(float deltaTime);
    void updateBoss(float deltaTime);
    void spawnAmbience(float deltaTime);
    void handleCollisions();
    void hurtPlayer(int amount);
    void killEnemy(Enemy& enemy);
    void flushPendingShots();
    void removeDeadEntities();

    // Shared by the pressure burst, reactive plating, mines and clusters.
    void areaDamage(sf::Vector2f centre, float radius, int damage);
    void spawnSplit(sf::Vector2f at, sf::Vector2f incoming);
    int  getJetDamage() const;

    // Shows a chapter if it has never been seen, otherwise goes straight on.
    void maybePlayStory(int chapter, State next);

    void openMenu();
    void activateMenuChoice();

    void applyUpgrades();
    void handleUpgradeClick();
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
    std::optional<Boss>     m_boss;

    // Two layers: ambience sits under the entities, impacts sit over them.
    ParticleSystem m_ambience;
    ParticleSystem m_effects;

    SaveGame m_save;
    State    m_state;
    float    m_stateTimer;
    float    m_menuTime;
    State    m_afterStory;

    int   m_salvage;
    bool  m_showUpgrades;
    float m_lastSpawnAngle;
    float m_snowTimer;
    float m_bubbleTimer;
    float m_thrusterTimer;
    float m_mineTimer;
    float m_sonarTimer;
    float m_sonarSweep; // 0 when idle, counts down through a ping

    sf::CircleShape    m_sonarRing;
    sf::CircleShape    m_sonarMark;
    sf::RectangleShape m_deathOverlay;
};
