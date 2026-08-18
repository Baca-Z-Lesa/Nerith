#include "core/Game.h"

#include "core/Config.h"
#include "core/Random.h"

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    const sf::Vector2f WorldCentre{ config::WindowWidth / 2.0f, config::WindowHeight / 2.0f };

    // Where a ray from the middle of the screen leaves a rectangle a little
    // larger than the window. Every point on it is off screen in every
    // direction, which an ellipse would not guarantee near the diagonals.
    sf::Vector2f offScreenPoint(sf::Vector2f viewCentre, float angleDegrees)
    {
        const float radians = sf::degrees(angleDegrees).asRadians();
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);

        const float halfWidth = config::WindowWidth / 2.0f + config::SpawnMarginX;
        const float halfHeight = config::WindowHeight / 2.0f + config::SpawnMarginY;

        const float toSide = halfWidth / std::max(std::abs(cosine), 0.0001f);
        const float toTop = halfHeight / std::max(std::abs(sine), 0.0001f);

        // Measured from where the camera is looking, not from the middle of the
        // basin, because the basin is now far wider than the window.
        return viewCentre + sf::Vector2f{ cosine, sine } * std::min(toSide, toTop);
    }

    float angleGap(float a, float b)
    {
        const float delta = std::fmod(std::abs(a - b), 360.0f);
        return delta > 180.0f ? 360.0f - delta : delta;
    }

    sf::Vector2f unit(sf::Angle angle)
    {
        return { std::cos(angle.asRadians()), std::sin(angle.asRadians()) };
    }

    // Banners fade in and back out rather than snapping, so a title card never
    // feels like a stutter.
    float bannerAlpha(float remaining, float total)
    {
        const float elapsed = total - remaining;
        return std::min({ 1.0f, elapsed / 0.35f, remaining / 0.45f });
    }
}

Game::Game()
    : m_cameraCentre(WorldCentre)
    , m_fullscreen(false)
    , m_arena(WorldCentre, config::ArenaRadius)
    , m_seabed(m_assets, WorldCentre, config::ArenaRadius)
    , m_player(m_assets, m_playerStats)
    , m_ambience(m_assets.particle)
    , m_effects(m_assets.particle)
    , m_state(State::LevelIntro)
    , m_stateTimer(config::LevelIntroDuration)
    , m_salvage(0)
    , m_showUpgrades(false)
    , m_lastSpawnAngle(0.0f)
    , m_snowTimer(0.0f)
    , m_bubbleTimer(0.0f)
    , m_thrusterTimer(0.0f)
    , m_mineTimer(0.0f)
    , m_sonarTimer(0.0f)
    , m_sonarSweep(0.0f)
    , m_sonarRing(10.0f, 64)
    , m_sonarMark(9.0f, 3)
    , m_deathOverlay({ static_cast<float>(config::WindowWidth), static_cast<float>(config::WindowHeight) })
{
    m_deathOverlay.setFillColor(config::DeathOverlayColor);

    m_sonarRing.setFillColor(sf::Color::Transparent);
    m_sonarRing.setOutlineThickness(2.0f);
    m_sonarMark.setFillColor(sf::Color::Transparent);
    m_sonarMark.setOutlineThickness(2.0f);
    m_sonarMark.setOrigin({ 9.0f, 9.0f });

    createWindow(false);

    // Pick up where the last session stopped, if there was one.
    m_save = SaveGame::load();
    m_salvage = m_save.salvage;
    m_tree.setUnlockedIds(m_save.unlocked);
    applyUpgrades();
    m_player.reset();

    beginLevel(m_save.level);

    // Seed the water so the arena is not empty on the first frame.
    for (int i = 0; i < 260; ++i)
        effects::marineSnow(m_ambience, randomVisiblePoint());

    m_audio.startAmbient();
}

void Game::createWindow(bool fullscreen)
{
    const sf::ContextSettings settings{ .antiAliasingLevel = 8 };

    if (fullscreen)
    {
        m_window.create(sf::VideoMode::getDesktopMode(), config::WindowTitle,
                        sf::Style::None, sf::State::Fullscreen, settings);
    }
    else
    {
        m_window.create(sf::VideoMode({ config::WindowWidth, config::WindowHeight }),
                        config::WindowTitle, sf::Style::Titlebar | sf::Style::Close,
                        sf::State::Windowed, settings);
    }

    m_fullscreen = fullscreen;
    m_window.setFramerateLimit(config::FramerateLimit);
    applyLetterboxView();
}

void Game::toggleFullscreen()
{
    createWindow(!m_fullscreen);
}

void Game::applyLetterboxView()
{
    // The game always thinks in 1280x720. Whatever the real window is, the view
    // is letterboxed into it so nothing stretches and aiming stays honest.
    const sf::Vector2f target(static_cast<float>(config::WindowWidth),
                              static_cast<float>(config::WindowHeight));
    const sf::Vector2f windowSize(m_window.getSize());

    if (windowSize.x <= 0.0f || windowSize.y <= 0.0f)
        return;

    const float targetRatio = target.x / target.y;
    const float windowRatio = windowSize.x / windowSize.y;

    sf::Vector2f size{ 1.0f, 1.0f };
    sf::Vector2f offset{ 0.0f, 0.0f };

    if (windowRatio > targetRatio)
    {
        size.x = targetRatio / windowRatio;   // bars on the left and right
        offset.x = (1.0f - size.x) / 2.0f;
    }
    else
    {
        size.y = windowRatio / targetRatio;   // bars on the top and bottom
        offset.y = (1.0f - size.y) / 2.0f;
    }

    const sf::FloatRect viewport(offset, size);

    // Two views share the letterbox: one that travels with the sub and one
    // pinned to the screen for the HUD, the vignette and every menu.
    m_ui = sf::View(sf::FloatRect({ 0.0f, 0.0f }, target));
    m_ui.setViewport(viewport);

    m_camera.setSize(target);
    m_camera.setCenter(m_cameraCentre);
    m_camera.setViewport(viewport);

    m_window.setView(m_camera);
}

void Game::updateCamera(float deltaTime)
{
    // Follow the sub, but never let the camera wander so far that the reef
    // leaves the frame entirely.
    sf::Vector2f target = m_player.getPosition();
    const sf::Vector2f offset = target - WorldCentre;

    if (offset.length() > config::CameraClamp)
        target = WorldCentre + offset.normalized() * config::CameraClamp;

    const float blend = std::min(1.0f, config::CameraLerp * deltaTime);
    m_cameraCentre += (target - m_cameraCentre) * blend;
    m_camera.setCenter(m_cameraCentre);
}

sf::Vector2f Game::randomVisiblePoint() const
{
    // Ambience is seeded around what the player can actually see; scattering it
    // across the whole basin would leave the screen empty.
    return m_cameraCentre + sf::Vector2f{ rng::range(-760.0f, 760.0f),
                                          rng::range(-470.0f, 470.0f) };
}

// ---------------------------------------------------------------------------
//  Campaign flow
// ---------------------------------------------------------------------------

int Game::getDepth() const
{
    return m_level.getDepth();
}

void Game::beginLevel(int number)
{
    m_level.start(number);

    // The depth is the mood: the lamp, the sand, the darkness and the drone all
    // change together, so descending is felt rather than announced.
    const DepthTheme& theme = themeFor(m_level.getDepth());
    m_arena.applyTheme(theme);
    m_seabed.applyTheme(theme);
    m_audio.setDepth(theme.ambientPitch, theme.ambientVolume);

    m_playerShots.clear();
    m_torpedoes.clear();
    m_enemyShots.clear();
    m_enemies.clear();
    m_mines.clear();
    m_boss.reset();
    m_lastSpawnAngle = rng::range(0.0f, 360.0f);
    m_mineTimer = config::MineInterval;
    m_sonarTimer = 0.5f;
    m_sonarSweep = 0.0f;

    m_player.onLevelStart();

    // Escorts are rebuilt each level so the count always matches the tree.
    m_drones.clear();
    const int wanted = m_playerStats.has(Trait::Drones)
                           ? std::max(0, m_playerStats.getInt(Stat::DroneCount, 0))
                           : 0;
    for (int i = 0; i < wanted; ++i)
    {
        Drone drone{ sf::Sprite(m_assets.forPart(ShipPart::Drone)),
                     360.0f * static_cast<float>(i) / static_cast<float>(std::max(1, wanted)),
                     rng::range(0.0f, config::DroneFireCooldown) };
        drone.sprite.setOrigin(sf::Vector2f(m_assets.forPart(ShipPart::Drone).getSize()) / 2.0f);
        m_drones.push_back(std::move(drone));
    }

    if (m_level.isBoss())
    {
        // Every depth has its own creature, and it arrives out of the dark like
        // everything else rather than blinking into the middle of the arena.
        m_boss.emplace(m_assets, bossForDepth(m_level.getDepth()), pickSpawnPosition(),
                       m_level.getScaling());
        m_audio.play(Sfx::BossRoar);
    }

    m_state = State::LevelIntro;
    m_stateTimer = config::LevelIntroDuration;
    m_showUpgrades = false;
}

void Game::onLevelComplete()
{
    for (const Enemy& enemy : m_enemies)
        effects::enemyDeath(m_effects, enemy.getPosition());

    m_enemies.clear();
    m_enemyShots.clear();

    m_salvage += config::SalvagePerLevel;
    m_audio.play(Sfx::LevelClear);

    m_state = State::LevelClear;
    m_stateTimer = config::LevelClearDuration;

    persist();
}

void Game::advanceLevel()
{
    beginLevel(m_level.getNumber() + 1);
    persist();
}

void Game::persist()
{
    m_save.level = m_state == State::LevelClear || m_state == State::Upgrading
                       ? m_level.getNumber() + 1
                       : m_level.getNumber();
    m_save.salvage = m_salvage;
    m_save.unlocked = m_tree.getUnlockedIds();
    m_save.bestLevel = std::max(m_save.bestLevel, m_level.getNumber());
    m_save.bestDepth = std::max(m_save.bestDepth, m_level.getDepth());
    m_save.save();
}

void Game::onPlayerDied()
{
    m_state = State::GameOver;
    m_showUpgrades = false;

    effects::playerDeath(m_effects, m_player.getPosition());
    m_audio.play(Sfx::PlayerHurt, 1.4f);

    // The records survive, the run does not.
    m_save.bestLevel = std::max(m_save.bestLevel, m_level.getNumber());
    m_save.bestDepth = std::max(m_save.bestDepth, m_level.getDepth());
    m_save.level = 1;
    m_save.salvage = 0;
    m_save.unlocked.clear();
    m_save.save();
}

void Game::startNewRun()
{
    m_tree.reset();
    m_salvage = 0;
    applyUpgrades();
    m_player.reset();

    beginLevel(1);
    persist();
}

// ---------------------------------------------------------------------------
//  Upgrades
// ---------------------------------------------------------------------------

void Game::applyUpgrades()
{
    // Rebuilt from the tree every time, so nothing can be counted twice.
    m_tree.applyTo(m_playerStats);
    m_player.onStatsChanged();
}

void Game::handleUpgradeClick()
{
    // The tree is inert once the run is over. Buying here would both heal a dead
    // sub and write the already wiped run back to disk.
    if (m_state == State::GameOver)
        return;

    const std::optional<std::size_t> hovered = m_upgradeScreen.getHovered();
    if (!hovered)
        return;

    if (m_tree.tryUnlock(*hovered, m_salvage, getDepth()))
    {
        applyUpgrades();
        m_audio.play(Sfx::LevelClear, 0.5f);
        persist();
    }
}

bool Game::upgradesVisible() const
{
    return m_state == State::Upgrading || m_showUpgrades;
}

// ---------------------------------------------------------------------------
//  Input
// ---------------------------------------------------------------------------

void Game::processEvents()
{
    while (const std::optional<sf::Event> event = m_window.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            m_window.close();
        }
        else if (event->is<sf::Event::Resized>())
        {
            applyLetterboxView();
        }
        else if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (upgradesVisible() && mousePressed->button == sf::Mouse::Button::Left)
                handleUpgradeClick();
            else if (!upgradesVisible() && m_state == State::Fighting
                     && mousePressed->button == sf::Mouse::Button::Right)
                fireTorpedo();
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            switch (keyPressed->scancode)
            {
                case sf::Keyboard::Scancode::Escape:
                    if (m_showUpgrades)
                        m_showUpgrades = false;
                    else
                        m_window.close();
                    break;

                case sf::Keyboard::Scancode::Tab:
                    // Between levels the tree is the screen, so there is nothing
                    // to toggle; after a death there is nothing left to spend.
                    if (m_state != State::Upgrading && m_state != State::GameOver)
                    {
                        m_showUpgrades = !m_showUpgrades;
                        m_upgradeScreen.clearCursor();
                m_upgradeScreen.recentre();
                    }
                    break;

                case sf::Keyboard::Scancode::LShift:
                case sf::Keyboard::Scancode::RShift:
                    if (m_state == State::Fighting && !m_showUpgrades)
                        performDash();
                    break;

                case sf::Keyboard::Scancode::Enter:
                case sf::Keyboard::Scancode::Space:
                    if (m_state == State::Upgrading)
                        advanceLevel();
                    break;

                case sf::Keyboard::Scancode::R:
                    if (m_state == State::GameOver)
                        startNewRun();
                    break;

                case sf::Keyboard::Scancode::M:
                    m_audio.setMuted(!m_audio.isMuted());
                    break;

                case sf::Keyboard::Scancode::F11:
                    toggleFullscreen();
                    break;

                default:
                    break;
            }
        }
    }
}

sf::Vector2f Game::getMouseWorldPosition() const
{
    // Through the camera, so aiming survives the letterbox and the scrolling world.
    return m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window), m_camera);
}

// ---------------------------------------------------------------------------
//  Player weapons
// ---------------------------------------------------------------------------

int Game::getJetDamage() const
{
    int damage = std::max(1, m_playerStats.getInt(Stat::ProjectileDamage, config::WaterJetDamage));

    // Predator Eye: the sub bites hardest when it is nearly dead.
    if (m_playerStats.has(Trait::PredatorInstinct)
        && static_cast<float>(m_player.getHealth())
               <= static_cast<float>(m_player.getMaxHealth()) * config::PredatorThreshold)
    {
        damage = std::max(1, static_cast<int>(std::lround(
                                 static_cast<float>(damage) * (1.0f + config::PredatorBonus))));
    }

    return damage;
}

void Game::handleShooting()
{
    if (!m_window.hasFocus() || !sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        return;

    if (!m_player.tryFire())
        return;

    const sf::Vector2f muzzle = m_player.getMuzzlePosition();
    const sf::Vector2f direction = m_player.getAimDirection();

    const config::ProjectileStats jet{
        m_playerStats.get(Stat::ProjectileSpeed, config::WaterJet.speed),
        config::WaterJet.radius,
        config::WaterJet.scale
    };

    m_playerShots.emplace_back(m_assets.waterJet, muzzle, direction, jet, getJetDamage(),
                               m_playerStats.getInt(Stat::Pierce, 0));
    effects::muzzleBurst(m_effects, muzzle, direction);
    m_audio.play(Sfx::JetFire);
}

void Game::fireTorpedo()
{
    if (!m_player.tryFireTorpedo())
        return;

    const sf::Vector2f muzzle = m_player.getMuzzlePosition();
    const sf::Vector2f direction = m_player.getAimDirection();
    const int damage = std::max(1, m_playerStats.getInt(Stat::TorpedoDamage, config::TorpedoBaseDamage));

    m_torpedoes.emplace_back(m_assets.torpedo, muzzle, direction, config::Torpedo, damage);
    effects::muzzleBurst(m_effects, muzzle, direction);
    m_audio.play(Sfx::Impact, 0.7f);
}

void Game::performDash()
{
    if (!m_player.tryDash())
        return;

    effects::thrusterBubbles(m_effects, m_player.getPosition(), -m_player.getAimDirection());
    m_audio.play(Sfx::Impact, 0.6f);

    // Pressure Burst turns the dash itself into a weapon.
    if (m_playerStats.has(Trait::PressureBurst))
    {
        areaDamage(m_player.getPosition(), config::PressureBurstRadius, config::PressureBurstDamage);
        effects::impact(m_effects, m_player.getPosition(), m_player.getAimDirection());
    }
}

void Game::spawnSplit(sf::Vector2f at, sf::Vector2f incoming)
{
    const int count = std::max(0, m_playerStats.getInt(Stat::SplitCount, 0));
    if (count <= 0)
        return;

    const config::ProjectileStats fragment{ config::SplitFragmentSpeed,
                                            config::WaterJet.radius * 0.7f,
                                            config::WaterJet.scale * config::SplitFragmentScale };

    // Fragments spray forward from the impact, never straight back at you.
    const sf::Angle base = incoming.angle();
    const int damage = std::max(1, getJetDamage() / 2);

    for (int i = 0; i < count; ++i)
    {
        const float step = config::SplitSpread / static_cast<float>(std::max(1, count - 1));
        const sf::Angle angle = base + sf::degrees(-config::SplitSpread / 2.0f
                                                   + step * static_cast<float>(i));
        m_playerShots.emplace_back(m_assets.waterJet, at, unit(angle), fragment, damage, 0);
    }
}

void Game::areaDamage(sf::Vector2f centre, float radius, int damage)
{
    for (Enemy& enemy : m_enemies)
    {
        if (!enemy.isAlive())
            continue;

        const float reach = radius + enemy.getRadius();
        if ((enemy.getPosition() - centre).lengthSquared() > reach * reach)
            continue;

        enemy.takeDamage(damage);
        if (!enemy.isAlive())
            killEnemy(enemy);
    }

    if (m_boss)
    {
        const Boss::Hit hit = m_boss->tryHit(centre, radius, damage,
                                             m_playerStats.get(Stat::WeakPointDamage, 0.0f));
        if (hit.destroyedPart)
            m_audio.play(Sfx::PartBreak);
    }

    effects::impact(m_effects, centre, { 1.0f, 0.0f });
}

// ---------------------------------------------------------------------------
//  Owned systems
// ---------------------------------------------------------------------------

void Game::emitThrusterTrail(float deltaTime)
{
    if (!m_player.isMoving())
        return;

    m_thrusterTimer -= deltaTime;
    if (m_thrusterTimer > 0.0f)
        return;

    m_thrusterTimer = config::PlayerBubbleInterval;
    effects::thrusterBubbles(m_ambience, m_player.getSternPosition(), -m_player.getAimDirection());
}

void Game::updateDrones(float deltaTime)
{
    for (Drone& drone : m_drones)
    {
        drone.angle += config::DroneOrbitSpeed * deltaTime;
        const sf::Vector2f offset = unit(sf::degrees(drone.angle)) * config::DroneOrbitRadius;
        const sf::Vector2f position = m_player.getPosition() + offset;

        drone.sprite.setPosition(position);

        // Pick whatever is nearest and inside its reach; drones do not lead
        // targets, which is what keeps them a supplement rather than a gun.
        const Enemy* target = nullptr;
        float best = config::DroneRange * config::DroneRange;

        for (const Enemy& enemy : m_enemies)
        {
            if (!enemy.isAlive())
                continue;

            const float distance = (enemy.getPosition() - position).lengthSquared();
            if (distance < best)
            {
                best = distance;
                target = &enemy;
            }
        }

        sf::Vector2f facing = m_player.getAimDirection();
        if (target != nullptr)
            facing = (target->getPosition() - position).normalized();
        else if (m_boss && (m_boss->getPosition() - position).length() < config::DroneRange)
            facing = (m_boss->getPosition() - position).normalized();
        else
            target = nullptr;

        drone.sprite.setRotation(facing.angle());

        drone.fireTimer -= deltaTime;
        if (drone.fireTimer > 0.0f)
            continue;

        const bool hasTarget = target != nullptr
                               || (m_boss && (m_boss->getPosition() - position).length() < config::DroneRange);
        if (!hasTarget)
            continue;

        drone.fireTimer = config::DroneFireCooldown;
        m_playerShots.emplace_back(m_assets.waterJet, position, facing,
                                   config::WaterJet, config::DroneDamage, 0);
    }
}

void Game::updateMines(float deltaTime)
{
    if (m_playerStats.has(Trait::Mines) && m_state == State::Fighting)
    {
        m_mineTimer -= deltaTime;
        if (m_mineTimer <= 0.0f)
        {
            m_mineTimer = config::MineInterval;

            Mine mine{ sf::Sprite(m_assets.mine), m_player.getSternPosition(),
                       config::MineLifetime, 0.6f };
            mine.sprite.setOrigin(sf::Vector2f(m_assets.mine.getSize()) / 2.0f);
            mine.sprite.setPosition(mine.position);
            m_mines.push_back(std::move(mine));
        }
    }

    const int damage = std::max(1, m_playerStats.getInt(Stat::MineDamage, config::MineBaseDamage));

    for (Mine& mine : m_mines)
    {
        mine.life -= deltaTime;
        if (mine.arm > 0.0f)
        {
            mine.arm -= deltaTime;
            continue;
        }

        for (const Enemy& enemy : m_enemies)
        {
            if (!enemy.isAlive())
                continue;

            const float reach = config::MineTriggerRadius + enemy.getRadius();
            if ((enemy.getPosition() - mine.position).lengthSquared() > reach * reach)
                continue;

            areaDamage(mine.position, config::MineBlastRadius, damage);
            m_audio.play(Sfx::PartBreak, 0.6f);
            mine.life = 0.0f;
            break;
        }
    }

    std::erase_if(m_mines, [](const Mine& mine) { return mine.life <= 0.0f; });
}

void Game::updateSonar(float deltaTime)
{
    if (!m_playerStats.has(Trait::Sonar))
        return;

    if (m_sonarSweep > 0.0f)
    {
        m_sonarSweep -= deltaTime;
        return;
    }

    m_sonarTimer -= deltaTime;
    if (m_sonarTimer <= 0.0f)
    {
        m_sonarTimer = config::SonarInterval;
        m_sonarSweep = config::SonarSweepTime;
    }
}

void Game::spawnAmbience(float deltaTime)
{
    m_snowTimer -= deltaTime;
    if (m_snowTimer <= 0.0f)
    {
        m_snowTimer = config::AmbientSnowInterval;
        effects::marineSnow(m_ambience, randomVisiblePoint());
    }

    m_bubbleTimer -= deltaTime;
    if (m_bubbleTimer <= 0.0f)
    {
        m_bubbleTimer = config::AmbientBubbleInterval;
        effects::ambientBubble(m_ambience, randomVisiblePoint());
    }
}

// ---------------------------------------------------------------------------
//  Spawning and movement
// ---------------------------------------------------------------------------

sf::Vector2f Game::pickSpawnPosition()
{
    sf::Vector2f best = offScreenPoint(m_cameraCentre, m_lastSpawnAngle + 180.0f);
    float bestAngle = m_lastSpawnAngle + 180.0f;
    float bestDistance = -1.0f;

    for (int attempt = 0; attempt < config::SpawnPlacementAttempts; ++attempt)
    {
        const float angle = rng::range(0.0f, 360.0f);
        const sf::Vector2f candidate = offScreenPoint(m_cameraCentre, angle);
        const float distance = (candidate - m_player.getPosition()).length();

        if (distance > bestDistance)
        {
            bestDistance = distance;
            best = candidate;
            bestAngle = angle;
        }

        const bool tooClose = distance < config::MinSpawnDistanceFromPlayer;
        const bool sameSide = angleGap(angle, m_lastSpawnAngle) < config::MinSpawnAngleSeparation;

        if (!tooClose && !sameSide)
        {
            best = candidate;
            bestAngle = angle;
            break;
        }
    }

    m_lastSpawnAngle = bestAngle;
    return best;
}

void Game::spawnEnemy(EnemyType type)
{
    m_enemies.emplace_back(type, m_assets.forEnemy(type), pickSpawnPosition(),
                           m_level.getScaling());
}

void Game::updateProjectiles(float deltaTime)
{
    const float bound = config::ArenaRadius + 420.0f;

    const auto step = [deltaTime, bound](std::vector<Projectile>& shots) {
        for (Projectile& shot : shots)
        {
            shot.update(deltaTime);

            if ((shot.getPosition() - WorldCentre).lengthSquared() > bound * bound)
                shot.kill();
        }
    };

    step(m_playerShots);
    step(m_enemyShots);
}

void Game::updateTorpedoes(float deltaTime)
{
    const float bound = config::ArenaRadius + 420.0f;
    const bool homing = m_playerStats.has(Trait::HomingTorpedoes);

    for (Projectile& torpedo : m_torpedoes)
    {
        if (homing)
        {
            // Nearest thing wins. A torpedo that reconsiders every frame reads
            // as hunting rather than as a guided missile.
            const sf::Vector2f from = torpedo.getPosition();
            float best = config::TorpedoSeekRange * config::TorpedoSeekRange;
            const sf::Vector2f* target = nullptr;
            sf::Vector2f bossPosition;

            for (const Enemy& enemy : m_enemies)
            {
                if (!enemy.isAlive())
                    continue;
                const float distance = (enemy.getPosition() - from).lengthSquared();
                if (distance < best)
                {
                    best = distance;
                    bossPosition = enemy.getPosition();
                    target = &bossPosition;
                }
            }

            if (m_boss && m_boss->isVulnerable())
            {
                const float distance = (m_boss->getPosition() - from).lengthSquared();
                if (distance < best)
                {
                    bossPosition = m_boss->getPosition();
                    target = &bossPosition;
                }
            }

            if (target != nullptr)
                torpedo.steerTowards(*target, config::TorpedoTurnRate, deltaTime);
        }

        torpedo.update(deltaTime);

        if ((torpedo.getPosition() - WorldCentre).lengthSquared() > bound * bound)
            torpedo.kill();
    }
}

void Game::updateEnemies(float deltaTime)
{
    const float roam = m_arena.getRadius() + config::EnemyRoamMargin;

    for (Enemy& enemy : m_enemies)
    {
        const bool wasAlive = enemy.isAlive();

        enemy.update(deltaTime, m_player.getPosition());
        enemy.confine(m_arena.getCenter(), roam);

        // Bleeding can finish something between shots, and that still counts.
        if (wasAlive && !enemy.isAlive())
        {
            killEnemy(enemy);
            continue;
        }

        for (const sf::Vector2f& direction : enemy.takeShots())
        {
            const sf::Vector2f muzzle = enemy.getPosition() + direction * (enemy.getRadius() + 8.0f);
            m_enemyShots.emplace_back(m_assets.enemyOrb, muzzle, direction, config::EnemyOrb,
                                      config::EnemyOrbDamage + m_level.getScaling().bonusDamage);
            effects::hostileMuzzle(m_effects, muzzle, direction);
        }
    }
}

void Game::updateBoss(float deltaTime)
{
    if (!m_boss)
        return;

    m_boss->update(deltaTime, m_player.getPosition());
    m_boss->confine(m_arena.getCenter(), config::BossRoamRadius);
    m_level.notifyBossCore(m_boss->getCoreFraction());

    const int orbDamage = config::EnemyOrbDamage + m_level.getScaling().bonusDamage;

    for (const BossShot& shot : m_boss->takeShots())
    {
        m_enemyShots.emplace_back(m_assets.enemyOrb, shot.position, shot.direction,
                                  config::EnemyOrb, orbDamage);
        effects::hostileMuzzle(m_effects, shot.position, shot.direction);
    }
}

// ---------------------------------------------------------------------------
//  Collisions
// ---------------------------------------------------------------------------

void Game::killEnemy(Enemy& enemy)
{
    effects::enemyDeath(m_effects, enemy.getPosition());
    m_audio.play(Sfx::EnemyDie);

    const float bonus = 1.0f + m_playerStats.get(Stat::SalvageBonus, 0.0f);
    m_salvage += std::max(1, static_cast<int>(std::lround(
                                 static_cast<float>(enemy.getSalvage()) * bonus)));

    m_level.notifyEnemyKilled(enemy.getType());
}

void Game::hurtPlayer(int amount)
{
    const Player::DamageResult result = m_player.takeDamage(amount);

    if (result.shieldBroke)
    {
        m_audio.play(Sfx::PartBreak, 0.8f);
        effects::impact(m_effects, m_player.getPosition(), { 1.0f, 0.0f });

        if (m_playerStats.has(Trait::ReactiveArmour))
            areaDamage(m_player.getPosition(), config::ReactiveBurstRadius, config::ReactiveBurstDamage);
        return;
    }

    if (!result.applied)
        return;

    effects::playerHurt(m_effects, m_player.getPosition());
    m_audio.play(Sfx::PlayerHurt, result.ventSaved ? 1.3f : 1.0f);
}

void Game::handleCollisions()
{
    const float weakPoint = m_playerStats.get(Stat::WeakPointDamage, 0.0f);
    const bool bleeds = m_playerStats.has(Trait::Bleed);
    const float bleedRate = m_playerStats.get(Stat::BleedDamage, 0.0f);
    const float bleedBonus = m_playerStats.get(Stat::BleedTargetDamage, 0.0f);
    const bool splits = m_playerStats.has(Trait::SplitShot);
    const bool chains = m_playerStats.has(Trait::ChainReaction);

    // Player shots and torpedoes share every rule except what they do on impact.
    const auto resolve = [&](std::vector<Projectile>& shots, bool torpedo) {
        for (Projectile& shot : shots)
        {
            if (!shot.isAlive())
                continue;

            bool spent = false;

            for (Enemy& enemy : m_enemies)
            {
                if (!enemy.isAlive())
                    continue;

                const float reach = shot.getRadius() + enemy.getRadius();
                if ((enemy.getPosition() - shot.getPosition()).lengthSquared() > reach * reach)
                    continue;

                int damage = shot.getDamage();
                if (enemy.isBleeding())
                {
                    damage = std::max(1, static_cast<int>(std::lround(
                                             static_cast<float>(damage) * (1.0f + bleedBonus))));
                }

                enemy.takeDamage(damage);
                if (bleeds && bleedRate > 0.0f)
                    enemy.applyBleed(bleedRate, config::BleedDuration);

                effects::impact(m_effects, shot.getPosition(), shot.getDirection());
                m_audio.play(Sfx::Impact);

                if (!enemy.isAlive())
                    killEnemy(enemy);

                if (torpedo)
                {
                    if (m_playerStats.has(Trait::ClusterTorpedoes))
                    {
                        for (int i = 0; i < config::TorpedoClusterShots; ++i)
                        {
                            const sf::Angle angle = sf::degrees(
                                360.0f * static_cast<float>(i) / config::TorpedoClusterShots);
                            m_playerShots.emplace_back(m_assets.waterJet, shot.getPosition(),
                                                       unit(angle), config::WaterJet,
                                                       std::max(1, shot.getDamage() / 2), 0);
                        }
                    }
                    shot.kill();
                    spent = true;
                    break;
                }

                if (splits)
                    spawnSplit(shot.getPosition(), shot.getDirection());

                if (shot.spendPierce())
                {
                    spent = true;
                    break;
                }
            }

            if (spent || !shot.isAlive() || !m_boss)
                continue;

            const Boss::Hit hit = m_boss->tryHit(shot.getPosition(), shot.getRadius(),
                                                 shot.getDamage(), weakPoint);
            if (!hit.hit)
                continue;

            effects::impact(m_effects, shot.getPosition(), shot.getDirection());

            if (hit.blocked)
            {
                // Hide and plating turn shots away. The spark is the lesson.
                m_audio.play(Sfx::Impact, 0.5f);
                shot.kill();
                continue;
            }

            if (hit.destroyedPart)
            {
                effects::enemyDeath(m_effects, hit.at);
                m_audio.play(Sfx::PartBreak);

                if (chains)
                    m_boss->splashNeighbours(hit.partIndex, config::BossChainDamage);
            }
            else
            {
                m_audio.play(Sfx::Impact);
            }

            if (hit.killedBoss || !m_boss->isAlive())
            {
                effects::playerDeath(m_effects, hit.at); // the biggest burst we have
                m_audio.play(Sfx::BossRoar, 1.2f);

                const float bonus = 1.0f + m_playerStats.get(Stat::SalvageBonus, 0.0f);
                m_salvage += std::max(1, static_cast<int>(std::lround(
                                             static_cast<float>(m_boss->getSalvage()) * bonus)));

                m_level.notifyBossDefeated();
                m_boss.reset();
                shot.kill();
                continue;
            }

            if (torpedo || shot.spendPierce())
                shot.kill();
        }
    };

    resolve(m_playerShots, false);
    resolve(m_torpedoes, true);

    // Hostile orbs against the sub.
    for (Projectile& shot : m_enemyShots)
    {
        if (!shot.isAlive())
            continue;

        const float reach = shot.getRadius() + config::PlayerRadius;
        if ((m_player.getPosition() - shot.getPosition()).lengthSquared() > reach * reach)
            continue;

        shot.kill();
        effects::impact(m_effects, shot.getPosition(), shot.getDirection());
        hurtPlayer(shot.getDamage());
    }

    // Contact damage. The invulnerability window is what stops a pile-up from
    // draining the whole bar in a single second.
    for (const Enemy& enemy : m_enemies)
    {
        if (!enemy.isAlive())
            continue;

        const float reach = enemy.getRadius() + config::PlayerRadius;
        if ((enemy.getPosition() - m_player.getPosition()).lengthSquared() > reach * reach)
            continue;

        hurtPlayer(enemy.getContactDamage());
    }

    if (m_boss && m_boss->isVulnerable() && m_boss->touches(m_player.getPosition(), config::PlayerRadius))
        hurtPlayer(m_boss->getContactDamage());
}

void Game::removeDeadEntities()
{
    const auto dead = [](const Projectile& shot) { return !shot.isAlive(); };

    std::erase_if(m_playerShots, dead);
    std::erase_if(m_torpedoes, dead);
    std::erase_if(m_enemyShots, dead);
    std::erase_if(m_enemies, [](const Enemy& enemy) { return !enemy.isAlive(); });
}

// ---------------------------------------------------------------------------
//  Loop
// ---------------------------------------------------------------------------

void Game::run()
{
    m_clock.restart();

    while (m_window.isOpen())
    {
        const float deltaTime = std::min(m_clock.restart().asSeconds(), config::MaxDeltaTime);

        processEvents();
        update(deltaTime);
        render();
    }
}

void Game::update(float deltaTime)
{
    // The world keeps breathing in every state. Only the fight pauses.
    m_arena.update(deltaTime);
    m_seabed.update(deltaTime);
    updateCamera(deltaTime);
    spawnAmbience(deltaTime);

    if (upgradesVisible())
    {
        // The tree lives in screen space, so the cursor has to be mapped through
        // the fixed view rather than the camera that follows the sub.
        m_upgradeScreen.update(deltaTime);
        m_upgradeScreen.setCursor(m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window), m_ui),
                                  m_tree);
    }

    switch (m_state)
    {
        case State::LevelIntro:
            m_stateTimer -= deltaTime;
            if (m_stateTimer <= 0.0f)
                m_state = State::Fighting;
            break;

        case State::Fighting:
            if (!m_showUpgrades)
            {
                m_player.update(deltaTime, getMouseWorldPosition(), m_arena);
                handleShooting();
                emitThrusterTrail(deltaTime);

                updateProjectiles(deltaTime);
                updateTorpedoes(deltaTime);
                m_level.update(deltaTime);

                if (const std::optional<EnemyType> type = m_level.takeSpawn(m_enemies.size()))
                    spawnEnemy(*type);

                updateEnemies(deltaTime);
                updateBoss(deltaTime);
                updateDrones(deltaTime);
                updateMines(deltaTime);
                updateSonar(deltaTime);

                handleCollisions();
                removeDeadEntities();

                if (!m_player.isAlive())
                    onPlayerDied();
                else if (m_level.isComplete())
                    onLevelComplete();
            }
            break;

        case State::LevelClear:
            m_stateTimer -= deltaTime;
            if (m_stateTimer <= 0.0f)
            {
                m_state = State::Upgrading;
                m_showUpgrades = false;
                m_upgradeScreen.clearCursor();
                m_upgradeScreen.recentre();
            }
            break;

        case State::Upgrading:
        case State::GameOver:
            break;
    }

    m_ambience.update(deltaTime);
    m_effects.update(deltaTime);
}

void Game::render()
{
    m_window.clear(themeFor(m_level.getDepth()).background);

    // Everything that lives in the basin is drawn through the camera.
    m_window.setView(m_camera);

    m_seabed.draw(m_window);
    m_arena.drawLight(m_window, m_player.getPosition());

    m_ambience.draw(m_window);

    for (const Mine& mine : m_mines)
        m_window.draw(mine.sprite);

    if (m_boss)
        m_boss->draw(m_window);

    for (const Enemy& enemy : m_enemies)
        enemy.draw(m_window);

    for (const Projectile& shot : m_enemyShots)
        shot.draw(m_window);

    for (const Projectile& shot : m_playerShots)
        shot.draw(m_window);

    for (const Projectile& torpedo : m_torpedoes)
        torpedo.draw(m_window);

    for (const Drone& drone : m_drones)
        m_window.draw(drone.sprite);

    if (m_player.isAlive())
        m_player.draw(m_window);

    m_effects.draw(m_window);

    // Sonar: an expanding ring plus a marker on everything it finds, including
    // creatures still out in the gloom where the light does not reach.
    if (m_sonarSweep > 0.0f)
    {
        const float progress = 1.0f - m_sonarSweep / config::SonarSweepTime;
        const float range = m_playerStats.get(Stat::SonarRange, config::SonarBaseRange);
        const float radius = range * progress;
        const auto fade = static_cast<std::uint8_t>((1.0f - progress) * 170.0f);

        sf::Color ping = config::SonarPingColor;
        ping.a = fade;

        m_sonarRing.setRadius(radius);
        m_sonarRing.setOrigin({ radius, radius });
        m_sonarRing.setPosition(m_player.getPosition());
        m_sonarRing.setOutlineColor(ping);
        m_window.draw(m_sonarRing);

        for (const Enemy& enemy : m_enemies)
        {
            if ((enemy.getPosition() - m_player.getPosition()).length() > range)
                continue;

            m_sonarMark.setPosition(enemy.getPosition());
            m_sonarMark.setOutlineColor(ping);
            m_window.draw(m_sonarMark);
        }
    }

    // Everything from here is pinned to the screen: the darkness closing in is
    // a vignette around the viewer, not a ring painted on the seabed.
    m_window.setView(m_ui);
    m_arena.drawGloom(m_window);

    if (m_state == State::GameOver)
        m_window.draw(m_deathOverlay);

    if (upgradesVisible())
    {
        const std::string prompt = m_state == State::Upgrading
                                       ? "ENTER TO DIVE TO LEVEL "
                                             + std::to_string(m_level.getNumber() + 1)
                                       : "TAB TO CLOSE";
        m_upgradeScreen.draw(m_window, m_tree, m_salvage, getDepth(), prompt);
    }
    else
    {
        Hud::LevelInfo info;
        info.depth = m_level.getDepth();
        info.levelInDepth = m_level.getLevelInDepth();
        info.number = m_level.getNumber();
        info.objective = m_level.getObjectiveLabel();
        info.progress = m_level.getProgress();
        info.isBoss = m_level.isBoss();
        info.shieldCharges = m_player.getShieldCharges();
        info.maxShieldCharges = m_player.getMaxShieldCharges();
        info.dashReady = m_player.getDashCooldownFraction();
        info.hasDash = m_playerStats.has(Trait::Dash);

        m_hud.draw(m_window, m_player.getHealth(), m_player.getMaxHealth(), info, m_salvage);

        if (m_boss)
            m_hud.drawBossBar(m_window, *m_boss);

        const LevelDef& def = m_level.getDef();

        if (m_state == State::LevelIntro)
        {
            const std::string label = "DEPTH " + std::to_string(m_level.getDepth()) + "  "
                                      + depthName(m_level.getDepth()) + "  -  LEVEL "
                                      + std::to_string(m_level.getNumber());

            // A boss level announces the creature itself, in its own words.
            const char* title = m_boss ? m_boss->getName() : def.title;
            const char* brief = m_boss ? m_boss->getBrief() : def.brief;

            m_hud.drawBanner(m_window, label, title, brief,
                             m_level.isBoss() ? config::HudBossColor : config::HudTextColor,
                             bannerAlpha(m_stateTimer, config::LevelIntroDuration));
        }
        else if (m_state == State::LevelClear)
        {
            m_hud.drawBanner(m_window, "LEVEL " + std::to_string(m_level.getNumber()),
                             m_level.isBoss() ? "DEPTH CLEARED" : "CLEAR",
                             "SALVAGE " + std::to_string(m_salvage),
                             config::HudSalvageColor,
                             bannerAlpha(m_stateTimer, config::LevelClearDuration));
        }
        else if (m_state == State::GameOver)
        {
            m_hud.drawBanner(m_window, "LOST AT LEVEL " + std::to_string(m_level.getNumber()),
                             "THE DARK TOOK YOU",
                             "R TO DIVE AGAIN   -   BEST LEVEL "
                                 + std::to_string(m_save.bestLevel),
                             config::HudBossColor, 1.0f);
        }
    }

    m_window.display();
}
