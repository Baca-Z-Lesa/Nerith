#include "core/Game.h"

#include "core/Config.h"
#include "core/Random.h"
#include "ui/PixelFont.h"

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
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
    , m_state(State::Menu)
    , m_stateTimer(config::LevelIntroDuration)
    , m_menuTime(0.0f)
    , m_salvage(0)
    , m_showUpgrades(false)
    , m_rerun(false)
    , m_respecArmed(false)
    , m_paused(false)
    , m_shake(0.0f)
    , m_hitStop(0.0f)
    , m_hurtFlash(0.0f)
    , m_charge(0.0f)
    , m_orbitAngle(0.0f)
    , m_lampOn(true)
    , m_noiseTimer(0.0f)
    , m_slowTimer(0.0f)
    , m_slowScale(1.0f)
    , m_flash(0.0f)
    , m_flashColour(sf::Color::White)
    , m_bossAct(0)
    , m_sirenTimer(0.0f)
    , m_optionRow(0)
    , m_creditsScroll(0.0f)
    , m_justBeatCampaign(false)
    , m_lastSpawnAngle(0.0f)
    , m_snowTimer(0.0f)
    , m_bubbleTimer(0.0f)
    , m_thrusterTimer(0.0f)
    , m_mineTimer(0.0f)
    , m_sonarTimer(0.0f)
    , m_sonarSweep(0.0f)
    , m_callTimer(0.0f)
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
    applySettings();
    m_player.reset();

    if (m_save.fullscreen)
        toggleFullscreen();

    beginLevel(m_save.selectedLevel);

    // Seed the water so the arena is not empty on the first frame.
    for (int i = 0; i < 260; ++i)
        effects::marineSnow(m_ambience, randomVisiblePoint());

    // The world is built and waiting behind the title, so Continue drops the
    // player straight back into the level the save left off at.
    openMenu();

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

    // The jolt is applied to the view rather than to m_cameraCentre, so it
    // never drags the follow target along with it and never fights the clamp.
    sf::Vector2f jolt{ 0.0f, 0.0f };
    if (m_shake > 0.05f)
        jolt = { rng::range(-m_shake, m_shake), rng::range(-m_shake, m_shake) };

    m_camera.setCenter(m_cameraCentre + jolt);
}

void Game::addShake(float strength)
{
    // Takes the larger of the two rather than adding: a torpedo landing in a
    // swarm would otherwise stack into a screen nobody can read.
    m_shake = std::min(config::ShakeMax, std::max(m_shake, strength));
}

void Game::addHitStop(float seconds)
{
    m_hitStop = std::max(m_hitStop, seconds);
}

void Game::addSlowMotion(float seconds, float scale)
{
    // Takes the stronger of the two rather than adding, so two events landing
    // together cannot leave the game crawling for ten seconds.
    m_slowTimer = std::max(m_slowTimer, seconds);
    m_slowScale = std::min(m_slowScale, std::clamp(scale, 0.05f, 1.0f));
}

void Game::addFlash(sf::Color colour, float strength)
{
    m_flash = std::max(m_flash, std::clamp(strength, 0.0f, 1.0f));
    m_flashColour = colour;
}

void Game::bigMoment(sf::Vector2f at, sf::Color colour, float shake, float slow)
{
    // Everything a moment is made of, in one call, so a new one cannot end up
    // half dressed: the jolt, the held breath, the slowed water, the wash of
    // colour, and a ring going out from wherever it happened.
    addShake(shake);
    addHitStop(config::HitStopBossPart);
    addSlowMotion(slow, config::SlowMotionScale);
    addFlash(colour, 0.85f);
    addHitRing(at, config::BigMomentRing, colour);

    effects::playerDeath(m_effects, at);
}

void Game::onBossActChanged(int act)
{
    // The fight turning is the single loudest thing in the game. It has to be
    // unmissable, because it is the player's only warning that everything they
    // had worked out a second ago is about to stop being true.
    const sf::Color colour = act >= 2 ? config::HudBossColor : config::VentGlowColor;

    bigMoment(m_boss ? m_boss->getPosition() : m_player.getPosition(),
              colour, config::ShakeDeath, config::SlowMotionBossAct);

    m_audio.play(Sfx::BossPhase);
    m_audio.play(Sfx::BossRoar, 1.3f);
}

void Game::updateFeel(float deltaTime)
{
    m_flash = std::max(0.0f, m_flash - config::FlashFade * deltaTime);

    if (m_slowTimer > 0.0f)
    {
        m_slowTimer -= deltaTime;
        if (m_slowTimer <= 0.0f)
            m_slowScale = 1.0f;
    }

    // The hull alarm, on a loop, once there is almost nothing left of the sub.
    // It is the difference between reading a number and being told.
    const bool critical = m_state == State::Fighting && m_player.isAlive()
                          && m_player.getHealth() <= 1;

    if (critical)
    {
        m_sirenTimer -= deltaTime;
        if (m_sirenTimer <= 0.0f)
        {
            m_sirenTimer = config::SirenInterval;
            m_audio.play(Sfx::Siren);
        }
    }
    else
    {
        m_sirenTimer = 0.0f;
    }

    m_noiseTimer = std::max(0.0f, m_noiseTimer - deltaTime);
    m_shake *= std::exp(-config::ShakeDecay * deltaTime);
    if (m_shake < 0.05f)
        m_shake = 0.0f;

    m_hurtFlash = std::max(0.0f, m_hurtFlash - config::HurtFlashFade * deltaTime);
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

    // Anything shallower than the deepest water opened so far has been beaten
    // before, and beating it again is worth a fraction of the first time.
    m_rerun = number < m_save.unlockedLevel;

    m_tally = RunTally{};
    m_paused = false;
    m_bossAct = 0;
    m_slowTimer = 0.0f;
    m_slowScale = 1.0f;
    m_flash = 0.0f;
    m_sirenTimer = 0.0f;
    m_shake = 0.0f;
    m_hitStop = 0.0f;
    m_hurtFlash = 0.0f;

    // The depth is the mood: the lamp, the sand, the darkness and the drone all
    // change together, so descending is felt rather than announced.
    const DepthTheme& theme = themeFor(m_level.getDepth());
    m_arena.applyTheme(theme);
    m_seabed.applyTheme(theme);
    m_audio.setDepth(theme.ambientPitch, theme.ambientVolume);

    m_playerShots.clear();
    m_pendingPlayerShots.clear();
    m_pendingEnemyShots.clear();
    m_torpedoes.clear();
    m_enemyShots.clear();
    m_enemies.clear();
    m_mines.clear();
    m_hitRings.clear();
    placeVents();
    placeNests();
    m_callTimer = rng::range(6.0f, config::CreatureCallMax);
    m_boss.reset();
    m_lastSpawnAngle = rng::range(0.0f, 360.0f);
    m_mineTimer = config::MineInterval;
    m_sonarTimer = 0.5f;
    m_sonarSweep = 0.0f;

    m_player.onLevelStart();

    // The refit crew works between dives, not during one.
    if (m_playerStats.has(Trait::Refit))
        m_player.heal(m_playerStats.getInt(Stat::RepairPerLevel, 0));

    // A bought lamp is worth the same at every depth, so it is applied on top
    // of the theme rather than replacing it.
    m_lampOn = true;
    m_noiseTimer = 0.0f;
    m_arena.setLamp(m_playerStats.get(Stat::LampRadius, 0.0f), 1.0f);

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

    // The bonus is for opening water, so a repeat pays nothing for the clear
    // itself and only a fraction for what was in it.
    if (!m_rerun)
    {
        m_salvage += config::FirstClearBase
                     + config::FirstClearPerLevel * m_level.getNumber();
    }

    m_audio.play(Sfx::LevelClear);

    if (m_level.getNumber() >= authoredLevelCount() && !m_save.campaignBeaten)
    {
        m_save.campaignBeaten = true;
        m_justBeatCampaign = true;
    }

    // Clearing a level is the only thing that opens the next one. Beating it
    // again later is always allowed, which is what makes grinding possible.
    m_save.unlockedLevel = std::max(m_save.unlockedLevel, m_level.getNumber() + 1);
    m_save.selectedLevel = m_save.unlockedLevel;

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
    // selectedLevel and unlockedLevel are owned by whoever changed them; this
    // only writes out the things that are always true.
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

    m_stateTimer = config::DeathBannerDuration;

    effects::playerDeath(m_effects, m_player.getPosition());
    m_audio.play(Sfx::PlayerHurt, 1.4f);
    addShake(config::ShakeDeath);
    m_hurtFlash = 1.0f;

    // Nothing is taken away. The salvage stays, the tree stays, and the level
    // is still there to be tried again from the hub. Losing an attempt is the
    // whole cost of dying: the difficulty is in beating a level at all.
    m_save.selectedLevel = m_level.getNumber();
    persist();
}

void Game::startNewRun()
{
    // Only reachable from the menu's explicit reset. Everything else in the
    // game is additive.
    m_tree.reset();
    m_salvage = 0;

    // Settings belong to the player rather than to the run, so scuttling the
    // sub does not also reset the volume and the difficulty.
    const SaveGame kept = m_save;
    m_save = SaveGame{};
    m_save.difficulty = kept.difficulty;
    m_save.sfxVolume = kept.sfxVolume;
    m_save.musicVolume = kept.musicVolume;
    m_save.muted = kept.muted;
    m_save.fullscreen = kept.fullscreen;

    applyUpgrades();
    m_player.reset();

    beginLevel(1);
    persist();
}

void Game::diveIntoSelected()
{
    const int level = std::clamp(m_save.selectedLevel, 1, m_save.unlockedLevel);

    m_player.reset();
    beginLevel(level);
    persist();
}

// ---------------------------------------------------------------------------
//  Upgrades
// ---------------------------------------------------------------------------

void Game::maybePlayStory(int chapter, State next)
{
    const int bit = 1 << chapter;

    // Each chapter is shown once ever. Replaying a level should not replay the
    // words that came with it the first time.
    if (chapter < 0 || chapter >= StoryScreen::chapterCount() || (m_save.storySeen & bit) != 0)
    {
        m_state = next;
        return;
    }

    m_save.storySeen |= bit;
    persist();

    m_story.begin(chapter);
    m_afterStory = next;
    m_state = State::Story;
}

void Game::openMenu()
{
    m_state = State::Menu;
    m_showUpgrades = false;

    // Continue is only offered when the save actually holds a run worth
    // resuming, so a fresh install never shows a dead option.
    m_menu.build(m_save.hasRun(), m_save.selectedLevel, m_save.bestLevel, m_save.bestDepth);
}

void Game::activateMenuChoice()
{
    switch (m_menu.getSelected())
    {
        case MainMenu::Choice::Dive:
            // Straight into the hub: the level and what to spend salvage on are
            // the same decision, so they share a screen. The prologue only
            // stands between them the very first time.
            m_showUpgrades = false;
            m_upgradeScreen.clearCursor();
            m_upgradeScreen.recentre();
            maybePlayStory(0, State::Upgrading);
            break;

        case MainMenu::Choice::Controls:
            m_state = State::Controls;
            break;

        case MainMenu::Choice::Options:
            m_optionRow = 0;
            m_state = State::Options;
            break;

        case MainMenu::Choice::Credits:
            m_creditsScroll = 0.0f;
            m_state = State::Credits;
            break;

        case MainMenu::Choice::Reset:
            startNewRun();
            openMenu();
            break;

        case MainMenu::Choice::Quit:
            m_window.close();
            break;
    }
}

const config::DifficultyDef& Game::difficulty() const
{
    return config::Difficulties[static_cast<std::size_t>(
        std::clamp(m_save.difficulty, 0, static_cast<int>(config::Difficulties.size()) - 1))];
}

void Game::applySettings()
{
    // One place that pushes the saved settings out into the systems that care,
    // so loading a save and changing a setting take the same path.
    m_audio.setSfxVolume(static_cast<float>(m_save.sfxVolume) / 10.0f);
    m_audio.setMusicVolume(static_cast<float>(m_save.musicVolume) / 10.0f);
    m_audio.setMuted(m_save.muted);
    m_level.setDifficulty(m_save.difficulty);
}

void Game::handleOptionsKey(sf::Keyboard::Scancode key)
{
    constexpr int RowCount = 5;

    const auto step = [&](int delta) {
        switch (m_optionRow)
        {
            case 0:
                m_save.difficulty = std::clamp(m_save.difficulty + delta, 0,
                                               static_cast<int>(config::Difficulties.size()) - 1);
                break;
            case 1: m_save.sfxVolume = std::clamp(m_save.sfxVolume + delta, 0, 10); break;
            case 2: m_save.musicVolume = std::clamp(m_save.musicVolume + delta, 0, 10); break;
            case 3:
                m_save.muted = delta > 0;
                break;
            case 4:
                if ((delta > 0) != m_fullscreen)
                {
                    toggleFullscreen();
                    m_save.fullscreen = m_fullscreen;
                }
                break;
            default: break;
        }

        applySettings();
        m_save.save();
    };

    switch (key)
    {
        case sf::Keyboard::Scancode::Up:
        case sf::Keyboard::Scancode::W:
            m_optionRow = (m_optionRow + RowCount - 1) % RowCount;
            break;

        case sf::Keyboard::Scancode::Down:
        case sf::Keyboard::Scancode::S:
            m_optionRow = (m_optionRow + 1) % RowCount;
            break;

        case sf::Keyboard::Scancode::Left:
        case sf::Keyboard::Scancode::A:
            step(-1);
            break;

        case sf::Keyboard::Scancode::Right:
        case sf::Keyboard::Scancode::D:
        case sf::Keyboard::Scancode::Enter:
        case sf::Keyboard::Scancode::Space:
            step(1);
            break;

        case sf::Keyboard::Scancode::Escape:
            openMenu();
            break;

        default:
            break;
    }
}

void Game::applyUpgrades()
{
    // Rebuilt from the tree every time, so nothing can be counted twice.
    m_tree.applyTo(m_playerStats);
    m_player.onStatsChanged();
}

void Game::handleUpgradeClick()
{
    m_respecArmed = false;

    // The hub's level strip shares the click with the tree.
    if (const std::optional<int> level = m_upgradeScreen.getHoveredLevel())
    {
        m_save.selectedLevel = std::clamp(*level, 1, m_save.unlockedLevel);
        m_upgradeScreen.setLevels(m_save.selectedLevel, m_save.unlockedLevel);
        persist();
        return;
    }

    // The tree is inert once the run is over. Buying here would both heal a dead
    // sub and write the already wiped run back to disk.
    if (m_state == State::GameOver)
        return;

    const std::optional<std::size_t> hovered = m_upgradeScreen.getHovered();
    if (!hovered)
        return;

    if (m_tree.tryUnlock(*hovered, m_salvage))
    {
        applyUpgrades();
        m_audio.play(Sfx::LevelClear, 0.5f);
        persist();
    }
}

void Game::stripHull()
{
    // Only from the hub, and never over a corpse: the game-over tree is a
    // read-only look at what might have been.
    if (m_state == State::GameOver || m_tree.getSpent() <= 0)
    {
        m_respecArmed = false;
        return;
    }

    if (!m_respecArmed)
    {
        m_respecArmed = true;
        return;
    }

    m_respecArmed = false;
    m_salvage += m_tree.stripForRefund();

    applyUpgrades();
    m_player.reset();
    m_audio.play(Sfx::PlayerHurt, 0.7f);
    persist();
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
        else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>())
        {
            if (upgradesVisible())
                m_upgradeScreen.zoomBy(wheel->delta);
        }
        else if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>())
        {
            if (m_state == State::Story)
            {
                if (mousePressed->button == sf::Mouse::Button::Left)
                    m_story.advance();
            }
            else if (m_state == State::Menu)
            {
                if (mousePressed->button == sf::Mouse::Button::Left)
                    activateMenuChoice();
            }
            else if (upgradesVisible() && mousePressed->button == sf::Mouse::Button::Left)
                handleUpgradeClick();
            else if (!upgradesVisible() && m_state == State::Fighting
                     && mousePressed->button == sf::Mouse::Button::Right)
                fireTorpedo();
        }
        else if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>())
        {
            // An interlude owns the keyboard: one key finishes the line, the next
            // moves on, and escape walks out of the whole chapter.
            if (m_state == State::Story)
            {
                if (keyPressed->scancode == sf::Keyboard::Scancode::Escape)
                    m_state = m_afterStory;
                else
                    m_story.advance();
                continue;
            }

            if (m_state == State::Options)
            {
                handleOptionsKey(keyPressed->scancode);
                continue;
            }

            // The flat screens have one way out and nothing else to press.
            if (m_state == State::Controls || m_state == State::Credits)
            {
                if (keyPressed->scancode == sf::Keyboard::Scancode::F11)
                    toggleFullscreen();
                else
                    openMenu();
                continue;
            }

            if (m_state == State::Victory)
            {
                m_creditsScroll = 0.0f;
                m_state = State::Credits;
                continue;
            }

            // The title screen owns the keyboard while it is up.
            if (m_state == State::Menu)
            {
                switch (keyPressed->scancode)
                {
                    case sf::Keyboard::Scancode::Up:
                    case sf::Keyboard::Scancode::W:     m_menu.moveSelection(-1); break;
                    case sf::Keyboard::Scancode::Down:
                    case sf::Keyboard::Scancode::S:     m_menu.moveSelection(1); break;
                    case sf::Keyboard::Scancode::Enter:
                    case sf::Keyboard::Scancode::Space: activateMenuChoice(); break;
                    case sf::Keyboard::Scancode::Escape: m_window.close(); break;
                    case sf::Keyboard::Scancode::M: m_audio.setMuted(!m_audio.isMuted()); break;
                    case sf::Keyboard::Scancode::F11: toggleFullscreen(); break;
                    default: break;
                }
                continue;
            }

            // Any other key cancels a pending strip, so the warning cannot sit
            // on the screen waiting to catch a press meant for something else.
            if (keyPressed->scancode != sf::Keyboard::Scancode::R)
                m_respecArmed = false;

            switch (keyPressed->scancode)
            {
                case sf::Keyboard::Scancode::Escape:
                case sf::Keyboard::Scancode::P:
                    // Backs out one layer at a time. A fight in progress pauses
                    // rather than being abandoned: walking out of a dive has to
                    // be a deliberate second keypress, not a mistyped escape.
                    if (m_showUpgrades)
                        m_showUpgrades = false;
                    else if (m_state == State::Fighting || m_state == State::LevelIntro)
                        m_paused = !m_paused;
                    else
                        openMenu();
                    break;

                case sf::Keyboard::Scancode::Q:
                    // Only offered by the pause overlay, and it keeps the
                    // salvage the dive has already earned.
                    if (m_paused)
                    {
                        m_paused = false;
                        m_state = State::Upgrading;
                        m_showUpgrades = false;
                        m_upgradeScreen.clearCursor();
                        m_upgradeScreen.recentre();
                        m_save.selectedLevel = m_level.getNumber();
                        persist();
                    }
                    break;

                case sf::Keyboard::Scancode::Equal:
                case sf::Keyboard::Scancode::NumpadPlus:
                    if (upgradesVisible())
                        m_upgradeScreen.zoomBy(1.0f);
                    break;

                case sf::Keyboard::Scancode::Hyphen:
                case sf::Keyboard::Scancode::NumpadMinus:
                    if (upgradesVisible())
                        m_upgradeScreen.zoomBy(-1.0f);
                    break;

                case sf::Keyboard::Scancode::R:
                    if (m_state == State::Upgrading)
                        stripHull();
                    break;

                case sf::Keyboard::Scancode::F:
                    // Kill the lamp. Nearly blind, but so is everything else.
                    if (m_state == State::Fighting && !m_showUpgrades && !m_paused)
                        toggleLamp();
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
                        diveIntoSelected();
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

void Game::fireJet(sf::Vector2f muzzle, sf::Vector2f direction, float chargeFactor)
{
    const config::ProjectileStats jet{
        m_playerStats.get(Stat::ProjectileSpeed, config::WaterJet.speed),
        config::WaterJet.radius * (chargeFactor > 1.0f ? 1.8f : 1.0f),
        config::WaterJet.scale
    };

    const int damage = std::max(1, static_cast<int>(std::lround(
                                       static_cast<float>(getJetDamage()) * chargeFactor)));

    Projectile shot(m_assets.waterJet, muzzle, direction, jet, damage,
                    m_playerStats.getInt(Stat::Pierce, 0));

    if (m_playerStats.has(Trait::Ricochet))
        shot.setBounces(m_playerStats.getInt(Stat::RicochetCount, 0));

    if (chargeFactor > 1.0f)
        shot.setScale(config::WaterJet.scale * 2.1f);

    m_playerShots.push_back(std::move(shot));

    effects::muzzleBurst(m_effects, muzzle, direction);
    m_audio.play(Sfx::JetFire, chargeFactor > 1.0f ? 1.6f : 1.0f);

    // Firing gives you away whether the lamp is lit or not. Without this,
    // running dark would be a way to shoot everything for free.
    m_noiseTimer = config::NoiseDuration;

    if (chargeFactor > 1.0f)
    {
        addShake(config::ShakeTorpedo * 0.6f);
        addHitRing(muzzle, config::JetRingRadius * 1.4f, config::CritRingColor);
    }

    ++m_tally.shotsFired;
}

void Game::handleShooting(float deltaTime)
{
    const bool held = m_window.hasFocus()
                      && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);

    // The charge coil turns the fire button into something you hold rather
    // than something you spam: let go early and it is an ordinary jet, hold it
    // and one shot carries the whole wait.
    if (m_playerStats.has(Trait::ChargedShot))
    {
        if (held)
        {
            const float before = m_charge;
            m_charge = std::min(config::ChargeTime, m_charge + deltaTime);

            // The whine starts the moment the coil catches, so holding the
            // button is audibly doing something.
            if (before <= config::ChargeMinimum && m_charge > config::ChargeMinimum)
                m_audio.play(Sfx::Charge);

            // While winding, the sub does not fire at all.
            if (m_charge > config::ChargeMinimum)
                return;
        }
        else if (m_charge > config::ChargeMinimum)
        {
            const float wound = m_charge / config::ChargeTime;
            const float factor = 1.0f + (m_playerStats.get(Stat::ChargeMultiplier, 1.0f) - 1.0f)
                                            * wound;
            m_charge = 0.0f;

            fireJet(m_player.getMuzzlePosition(), m_player.getAimDirection(), factor);
            return;
        }
        else
        {
            m_charge = 0.0f;
        }
    }

    if (!held || !m_player.tryFire())
        return;

    fireJet(m_player.getMuzzlePosition(), m_player.getAimDirection(), 1.0f);
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
        m_pendingPlayerShots.emplace_back(m_assets.waterJet, at, unit(angle), fragment, damage, 0);
    }
}

void Game::awardSalvage(int base)
{
    if (base <= 0)
        return;

    const float bonus = 1.0f + m_playerStats.get(Stat::SalvageBonus, 0.0f);
    const float factor = m_rerun ? config::RerunSalvageFactor : 1.0f;

    const int paid = std::max(1, static_cast<int>(std::lround(
                                     static_cast<float>(base) * bonus * factor
                                     * difficulty().salvageScale)));

    m_salvage += paid;
    m_tally.salvage += paid;
}

void Game::addHitRing(sf::Vector2f at, float radius, sf::Color colour)
{
    m_hitRings.push_back({ at, 0.0f, config::HitRingDuration, radius, colour });
}

void Game::updateHitRings(float deltaTime)
{
    for (HitRing& ring : m_hitRings)
        ring.age += deltaTime;

    std::erase_if(m_hitRings, [](const HitRing& ring) { return ring.age >= ring.life; });
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
        // Drone Teeth is flat damage on top of what a drone does by default.
        const int bite = config::DroneDamage
                         + m_playerStats.getInt(Stat::DroneDamage, 0);

        m_playerShots.emplace_back(m_assets.waterJet, position, facing,
                                   config::WaterJet, bite, 0);
    }
}

void Game::placeNests()
{
    m_nests.clear();

    const int wanted = m_level.getNestCount();
    if (wanted <= 0)
        return;

    for (int i = 0; i < wanted; ++i)
    {
        // Spread right round the basin and well out from the middle, so no two
        // are on screen together and none of them can be shot from the spot
        // the sub starts on.
        const float angle = 360.0f * static_cast<float>(i) / static_cast<float>(wanted)
                            + rng::range(-18.0f, 18.0f);
        const float reach = config::ArenaRadius
                            * rng::range(config::NestRingMin, config::NestRingMax);

        Nest nest{ sf::Sprite(m_assets.boss(config::NestTexturePath)),
                   WorldCentre + sf::Vector2f(reach, sf::degrees(angle)),
                   config::NestHealth, config::NestHealth,
                   rng::range(1.0f, config::NestHatchInterval), 0.0f, true };

        nest.sprite.setOrigin(sf::Vector2f(nest.sprite.getTexture().getSize()) / 2.0f);
        nest.sprite.setPosition(nest.position);
        nest.sprite.setRotation(sf::degrees(rng::range(0.0f, 360.0f)));

        m_nests.push_back(std::move(nest));
    }
}

void Game::updateNests(float deltaTime)
{
    for (Nest& nest : m_nests)
    {
        if (!nest.alive)
            continue;

        if (nest.flash > 0.0f)
            nest.flash = std::max(0.0f, nest.flash - deltaTime);

        nest.sprite.setColor(nest.flash > 0.0f ? config::EnemyHitFlashTint : sf::Color::White);

        nest.spawnTimer -= deltaTime;
        if (nest.spawnTimer > 0.0f)
            continue;

        nest.spawnTimer = config::NestHatchInterval;

        // It hatches where it stands rather than off screen, so a nest you have
        // found is a nest you are now fighting.
        if (m_enemies.size() >= config::MaxSpawnlings * 2)
            continue;

        const sf::Vector2f offset(rng::range(40.0f, 70.0f),
                                  sf::degrees(rng::range(0.0f, 360.0f)));

        m_enemies.emplace_back(EnemyType::Spawnling, m_assets.forEnemy(EnemyType::Spawnling),
                               nest.position + offset, m_level.getScaling());
        m_enemies.back().markHatched();

        effects::enemyDeath(m_effects, nest.position + offset);
        m_audio.play(Sfx::Hatch, 0.7f);
    }
}

void Game::drawNests()
{
    for (const Nest& nest : m_nests)
    {
        if (!nest.alive)
            continue;

        m_window.draw(nest.sprite);

        // A ring that tightens as it dies, so how far along a nest is can be
        // read from across the basin without a health bar over it.
        const float fraction = static_cast<float>(nest.health)
                               / static_cast<float>(std::max(1, nest.maxHealth));
        const float radius = config::NestRadius * (0.55f + 0.45f * fraction);

        sf::Color ring = config::HudSalvageColor;
        ring.a = 150;

        m_sonarRing.setRadius(radius);
        m_sonarRing.setOrigin({ radius, radius });
        m_sonarRing.setPosition(nest.position);
        m_sonarRing.setOutlineThickness(2.5f);
        m_sonarRing.setOutlineColor(ring);
        m_window.draw(m_sonarRing);
    }
}

void Game::toggleLamp()
{
    m_lampOn = !m_lampOn;

    m_arena.setLamp(m_playerStats.get(Stat::LampRadius, 0.0f),
                    m_lampOn ? 1.0f : config::LampDarkFactor);

    m_audio.play(Sfx::PartBreak, 0.35f);
}

float Game::noticeRange() const
{
    // Firing gives you away whether the lamp is on or not, which is what stops
    // running dark from being a way to shoot everything for free.
    if (m_lampOn || m_noiseTimer > 0.0f)
        return config::NoticeLit;

    return config::NoticeDark;
}

void Game::placeVents()
{
    m_vents.clear();

    const int depth = m_level.getDepth();
    if (depth < config::VentsFirstDepth)
        return;

    const int wanted = std::min(config::VentsMax,
                                (depth - config::VentsFirstDepth + 1) * config::VentsPerDepth);

    for (int i = 0; i < wanted; ++i)
    {
        // Spread round the basin rather than scattered at random, so a level
        // never opens with three of them stacked on the spot the sub starts on.
        const float angle = 360.0f * static_cast<float>(i) / static_cast<float>(wanted)
                            + rng::range(-22.0f, 22.0f);
        const float reach = rng::range(config::ArenaRadius * 0.30f, config::ArenaRadius * 0.82f);

        Vent vent;
        vent.position = WorldCentre
                        + sf::Vector2f(reach, sf::degrees(angle));
        vent.radius = rng::range(config::VentRadiusMin, config::VentRadiusMax);
        vent.interval = rng::range(config::VentIntervalMin, config::VentIntervalMax);
        // Staggered, so they do not all breathe together.
        vent.timer = rng::range(1.5f, vent.interval);
        vent.erupting = 0.0f;

        m_vents.push_back(vent);
    }
}

void Game::updateVents(float deltaTime)
{
    for (Vent& vent : m_vents)
    {
        if (vent.erupting > 0.0f)
        {
            vent.erupting -= deltaTime;

            // Damage lands on the frame it opens, not every frame it is open:
            // standing in it should cost a bite, not delete the sub.
            if (vent.erupting <= 0.0f)
                vent.timer = vent.interval;

            continue;
        }

        vent.timer -= deltaTime;
        if (vent.timer > 0.0f)
            continue;

        vent.erupting = config::VentEruptTime;
        m_audio.play(Sfx::Vent);
        addShake(config::ShakeTorpedo * 0.7f);

        effects::ventBurst(m_effects, vent.position, vent.radius);

        // The sub and the creatures both, because a hazard that only hurts the
        // player is a tax, while one that hurts both is a tool. Bosses are the
        // exception: areaDamage does not reach them, and letting the floor cook
        // a depth's creature would take the fight away from the player.
        if ((m_player.getPosition() - vent.position).length() <= vent.radius)
            hurtPlayer(config::VentDamage);

        areaDamage(vent.position, vent.radius,
                   std::max(1, static_cast<int>(std::lround(config::VentEnemyDamage))));
    }
}

void Game::drawVents()
{
    for (const Vent& vent : m_vents)
    {
        // Dormant it is a dull ring; as the pressure builds the ring brightens
        // and thickens, and the eruption is a bright disc. The warning is the
        // whole point, so it has to be readable before it is dangerous.
        const bool open = vent.erupting > 0.0f;
        const float charge = open
                                 ? 1.0f
                                 : std::clamp(1.0f - vent.timer / config::VentWarnTime, 0.0f, 1.0f);

        sf::Color glow = config::VentGlowColor;
        glow.a = static_cast<std::uint8_t>(40.0f + 170.0f * charge);

        const float radius = vent.radius * (open ? 1.0f : 0.62f + 0.38f * charge);

        m_sonarRing.setRadius(radius);
        m_sonarRing.setOrigin({ radius, radius });
        m_sonarRing.setPosition(vent.position);
        m_sonarRing.setOutlineThickness(2.5f + 5.5f * charge);
        m_sonarRing.setOutlineColor(glow);

        sf::Color fill = config::VentGlowColor;
        fill.a = static_cast<std::uint8_t>(open ? 66.0f * (vent.erupting / config::VentEruptTime)
                                                : 16.0f * charge);
        m_sonarRing.setFillColor(fill);
        m_window.draw(m_sonarRing);
        m_sonarRing.setFillColor(sf::Color::Transparent);
    }
}

void Game::updateOrbits(float deltaTime)
{
    if (!m_playerStats.has(Trait::OrbitShield))
        return;

    m_orbitAngle += config::OrbitSpeed * deltaTime;

    const int motes = std::max(1, m_playerStats.getInt(Stat::OrbitCount, 0));
    const sf::Vector2f centre = m_player.getPosition();

    // Each mote eats one orb per frame at most, which keeps a wall of stingers
    // dangerous even with a full ring up: the motes thin it, they do not delete
    // it.
    for (int i = 0; i < motes; ++i)
    {
        const sf::Angle angle = sf::degrees(m_orbitAngle
                                            + 360.0f * static_cast<float>(i)
                                                  / static_cast<float>(motes));
        const sf::Vector2f at = centre + unit(angle) * config::OrbitRadius;

        for (Projectile& orb : m_enemyShots)
        {
            if (!orb.isAlive())
                continue;

            const float reach = config::OrbitEatRadius + orb.getRadius();
            if ((orb.getPosition() - at).lengthSquared() > reach * reach)
                continue;

            orb.kill();
            effects::impact(m_effects, orb.getPosition(), -orb.getDirection());
            m_audio.play(Sfx::Impact, 0.6f);
            break;
        }
    }
}

void Game::drawOrbits()
{
    if (!m_playerStats.has(Trait::OrbitShield))
        return;

    const int motes = std::max(1, m_playerStats.getInt(Stat::OrbitCount, 0));
    const sf::Vector2f centre = m_player.getPosition();

    for (int i = 0; i < motes; ++i)
    {
        const sf::Angle angle = sf::degrees(m_orbitAngle
                                            + 360.0f * static_cast<float>(i)
                                                  / static_cast<float>(motes));
        const sf::Vector2f at = centre + unit(angle) * config::OrbitRadius;

        m_sonarMark.setPosition(at);
        m_sonarMark.setOutlineColor(config::HudPipFullEdge);
        m_window.draw(m_sonarMark);
    }
}

void Game::updateMines(float deltaTime)
{
    if (m_playerStats.has(Trait::Mines) && m_state == State::Fighting)
    {
        m_mineTimer -= deltaTime;
        if (m_mineTimer <= 0.0f)
        {
            // Deep Mines lays them faster rather than laying two at once: two in
            // the same place would only ever detonate together.
            m_mineTimer = config::MineInterval
                          / std::max(1.0f, static_cast<float>(
                                              m_playerStats.getInt(Stat::MineCount, 1)));

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

void Game::callFromTheDark(float deltaTime)
{
    // Only while there is a fight to be quiet in. On a menu it would be noise.
    if (m_state != State::Fighting || m_paused)
        return;

    m_callTimer -= deltaTime;
    if (m_callTimer > 0.0f)
        return;

    m_callTimer = rng::range(config::CreatureCallMin, config::CreatureCallMax);

    // Deeper water calls lower and louder. It is never anything you can shoot.
    const bool deep = m_level.getDepth() >= 3;
    m_audio.play(deep ? Sfx::CreatureCallB : Sfx::CreatureCallA,
                 0.7f + 0.1f * static_cast<float>(m_level.getDepth()));
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

    // Ricochet: the reef wall stops being the end of a shot and becomes part of
    // the shot. Bounced off the arena's own edge rather than the far-off kill
    // boundary, so a carom actually comes back into the fight.
    if (!m_playerStats.has(Trait::Ricochet))
        return;

    const float wall = config::ArenaRadius;

    for (Projectile& shot : m_playerShots)
    {
        if (!shot.isAlive())
            continue;

        const sf::Vector2f offset = shot.getPosition() - WorldCentre;
        if (offset.lengthSquared() < wall * wall)
            continue;

        // The wall's normal points back at the middle of the basin.
        if (shot.spendBounce(-offset.normalized()))
            addHitRing(shot.getPosition(), 22.0f, config::HudDimTextColor);
    }
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
    // Who can currently see the sub. One radius for the whole basin rather than
    // per-creature senses: the player needs a rule they can hold in their head,
    // not a simulation they have to reverse engineer.
    const float notice = noticeRange();
    const float noticeSquared = notice * notice;

    for (Enemy& enemy : m_enemies)
    {
        const bool close = (enemy.getPosition() - m_player.getPosition()).lengthSquared()
                           < noticeSquared;
        enemy.setAware(close);
    }

    const float roam = m_arena.getRadius() + config::EnemyRoamMargin;

    // Hatchlings are collected first and pushed after the loop: spawning into
    // the vector we are walking would invalidate the reference under us.
    struct Hatch { EnemyType type; sf::Vector2f at; };
    std::vector<Hatch> hatched;

    const std::size_t spawnlings = static_cast<std::size_t>(std::count_if(
        m_enemies.begin(), m_enemies.end(), [](const Enemy& e) {
            return e.isAlive() && e.getType() == EnemyType::Spawnling;
        }));

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
            m_enemyShots.emplace_back(m_assets.shotFor(enemy.getType()), muzzle, direction,
                                      config::EnemyOrb,
                                      config::EnemyOrbDamage + m_level.getScaling().bonusDamage);
            effects::hostileMuzzle(m_effects, muzzle, direction);
        }

        // A brooder holds its fire once the water is already full of its young,
        // so two of them cannot bury the arena between them.
        if (spawnlings + hatched.size() < config::MaxSpawnlings)
        {
            for (const EnemyType type : enemy.takeBrood())
                hatched.push_back({ type, enemy.getPosition() });
        }
    }

    for (const Hatch& hatch : hatched)
    {
        // Born out of the parent rather than swimming in from the dark, so the
        // brooder reads as the source and killing it visibly stops the flow.
        const sf::Vector2f offset(rng::range(26.0f, 44.0f),
                                  sf::degrees(rng::range(0.0f, 360.0f)));

        m_enemies.emplace_back(hatch.type, m_assets.forEnemy(hatch.type), hatch.at + offset,
                               m_level.getScaling());
        m_enemies.back().markHatched();
        effects::enemyDeath(m_effects, hatch.at + offset);
    }

    // One wet pop for the whole brood rather than one per hatchling, which at
    // four at a time would be a single smeared noise anyway.
    if (!hatched.empty())
        m_audio.play(Sfx::Hatch);
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
        m_enemyShots.emplace_back(m_assets.boss(m_boss->getShotTexture()), shot.position,
                                  shot.direction, config::EnemyOrb, orbDamage);
        effects::hostileMuzzle(m_effects, shot.position, shot.direction);
    }

    // A creature that calls for help. The summons arrive at the edge of the
    // basin like anything else does, and they are not part of the level's
    // roster, so they cannot be farmed by standing in a boss fight.
    for (const EnemyType type : m_boss->takeSummons())
    {
        m_enemies.emplace_back(type, m_assets.forEnemy(type), pickSpawnPosition(),
                               m_level.getScaling());
        m_enemies.back().markHatched();

        m_audio.play(Sfx::Hatch, 1.2f);
    }

    // The fight turning.
    if (m_boss->getAct() != m_bossAct)
    {
        m_bossAct = m_boss->getAct();
        onBossActChanged(m_bossAct);
    }
}

// ---------------------------------------------------------------------------
//  Collisions
// ---------------------------------------------------------------------------

void Game::killEnemy(Enemy& enemy)
{
    effects::enemyDeath(m_effects, enemy.getPosition());
    m_audio.play(Sfx::EnemyDie);

    // Some things take a swing on the way out. Killing a husk in your own face
    // is meant to hurt.
    const int burst = enemy.getDeathShots();
    for (int i = 0; i < burst; ++i)
    {
        const sf::Angle angle = sf::degrees(360.0f * static_cast<float>(i)
                                            / static_cast<float>(burst));
        const sf::Vector2f direction = unit(angle);

        m_pendingEnemyShots.emplace_back(m_assets.shotFor(enemy.getType()),
                                        enemy.getPosition() + direction * (enemy.getRadius() + 6.0f),
                                        direction, config::EnemyOrb,
                                        config::EnemyOrbDamage + m_level.getScaling().bonusDamage);
    }

    // Corpse Bloom: the body is a weapon. Deferred through areaDamage rather
    // than recursing into killEnemy, so a packed swarm cannot blow the stack.
    if (m_playerStats.has(Trait::CorpseBloom))
    {
        const int bloom = std::max(1, static_cast<int>(std::lround(
                                          static_cast<float>(enemy.getContactDamage() + 2)
                                          * config::CorpseBloomFactor * 3.0f)));

        areaDamage(enemy.getPosition(), config::CorpseBloomRadius, bloom);
        addHitRing(enemy.getPosition(), config::CorpseBloomRadius, config::HudSalvageColor);
        effects::enemyDeath(m_effects, enemy.getPosition());
    }

    // Parasite: the sub feeds on what it kills.
    if (m_playerStats.has(Trait::Lifesteal)
        && rng::range(0.0f, 1.0f) < m_playerStats.get(Stat::LifestealChance, 0.0f))
    {
        m_player.heal(1);
        addHitRing(m_player.getPosition(), 30.0f, config::HudPipFullEdge);
    }

    awardSalvage(enemy.getSalvage());
    ++m_tally.kills;

    // Only what the level script sent counts: otherwise a brooder would clear
    // its own level by hatching into the quota.
    if (enemy.isScripted())
        m_level.notifyEnemyKilled(enemy.getType());
}

void Game::hurtPlayer(int amount)
{
    // The single door every wound comes through, which is why the difficulty
    // setting is applied here and nowhere else.
    amount = std::max(1, static_cast<int>(std::lround(static_cast<float>(amount)
                                                      * difficulty().enemyDamage)));

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

    m_tally.damageTaken += amount;
    m_hurtFlash = 1.0f;
    addShake(config::ShakePlayerHurt);
    addHitStop(config::HitStopPlayerHurt);

    // Dead Man: being bitten empties the tubes in every direction at once.
    // Queued rather than pushed, because this can fire while the shot lists
    // are being walked.
    if (m_playerStats.has(Trait::Retaliate))
    {
        const int shots = static_cast<int>(config::RetaliateShots);
        const config::ProjectileStats jet{
            m_playerStats.get(Stat::ProjectileSpeed, config::WaterJet.speed),
            config::WaterJet.radius, config::WaterJet.scale
        };

        for (int i = 0; i < shots; ++i)
        {
            const sf::Vector2f direction = unit(sf::degrees(
                360.0f * static_cast<float>(i) / static_cast<float>(shots)));

            m_pendingPlayerShots.emplace_back(m_assets.waterJet,
                                              m_player.getPosition() + direction * 22.0f,
                                              direction, jet, getJetDamage(), 0);
        }

        addHitRing(m_player.getPosition(), 70.0f, config::HudPipFullEdge);
        m_audio.play(Sfx::JetFire, 1.5f);
    }
}

void Game::handleCollisions()
{
    const float weakPoint = m_playerStats.get(Stat::WeakPointDamage, 0.0f);
    const bool bleeds = m_playerStats.has(Trait::Bleed);
    const float bleedRate = m_playerStats.get(Stat::BleedDamage, 0.0f);
    const float bleedBonus = m_playerStats.get(Stat::BleedTargetDamage, 0.0f);
    const bool splits = m_playerStats.has(Trait::SplitShot);
    const bool chains = m_playerStats.has(Trait::ChainReaction);

    // Read once rather than per shot per creature: this loop is the hottest
    // thing in the frame on a busy level.
    const bool crits = m_playerStats.has(Trait::Crit);
    const float critChance = m_playerStats.get(Stat::CritChance, 0.0f);
    const float critBonus = m_playerStats.get(Stat::CritDamage, 0.0f);
    const bool chills = m_playerStats.has(Trait::Frost);
    const float chillStrength = m_playerStats.get(Stat::SlowStrength, 0.0f);
    const float blastRadius = config::TorpedoBlastRadius
                              + m_playerStats.get(Stat::TorpedoBlast, 0.0f);
    const bool executes = m_playerStats.has(Trait::Execute);
    const float executeAt = m_playerStats.get(Stat::ExecuteThreshold, 0.0f);

    // Player shots and torpedoes share every rule except what they do on impact.
    const auto resolve = [&](std::vector<Projectile>& shots, bool torpedo) {
        for (Projectile& shot : shots)
        {
            if (!shot.isAlive())
                continue;

            bool spent = false;

            // Nests take hits like anything else. They are the objective on a
            // hunt level, so the same jet that kills a creature burns one out.
            for (Nest& nest : m_nests)
            {
                if (!nest.alive)
                    continue;

                const float reach = shot.getRadius() + config::NestRadius;
                if ((nest.position - shot.getPosition()).lengthSquared() > reach * reach)
                    continue;

                nest.health -= shot.getDamage();
                nest.flash = config::EnemyHitFlashDuration;

                effects::impact(m_effects, shot.getPosition(), shot.getDirection());
                m_audio.play(Sfx::Impact);
                addHitRing(shot.getPosition(), config::JetRingRadius, config::HudSalvageColor);

                if (nest.health <= 0)
                {
                    nest.alive = false;
                    effects::playerDeath(m_effects, nest.position);
                    effects::ventBurst(m_effects, nest.position, config::NestRadius);
                    m_audio.play(Sfx::BossRoar, 0.8f);
                    addShake(config::ShakeBossPart);
                    addHitStop(config::HitStopBossPart);
                    addHitRing(nest.position, config::NestRadius * 2.6f, config::HudSalvageColor);

                    awardSalvage(config::NestSalvage);
                    m_level.notifyNestDestroyed();
                }

                if (torpedo || shot.spendPierce())
                {
                    spent = true;
                    break;
                }
            }

            if (spent || !shot.isAlive())
                continue;

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

                // A crit doubles, and Cold Eye stacks on top of that doubling.
                const bool crit = crits && rng::range(0.0f, 1.0f) < critChance;
                if (crit)
                {
                    damage = std::max(1, static_cast<int>(std::lround(
                                             static_cast<float>(damage) * (2.0f + critBonus))));
                }

                enemy.takeDamage(damage);
                if (bleeds && bleedRate > 0.0f)
                    enemy.applyBleed(bleedRate, config::BleedDuration);
                if (chills)
                    enemy.applyChill(chillStrength, config::ChillDuration);

                // Cull: anything already this far gone simply stops, which is
                // what turns a long fight against a hulk into a short one.
                if (executes && enemy.isAlive()
                    && enemy.getHealthFraction() <= executeAt)
                {
                    enemy.kill();
                    addHitRing(enemy.getPosition(), enemy.getRadius() * 2.4f,
                               config::HudBossColor);
                }

                effects::impact(m_effects, shot.getPosition(), shot.getDirection());
                m_audio.play(Sfx::Impact, crit ? 1.5f : 1.0f);
                addHitRing(shot.getPosition(),
                           crit ? config::CritRingRadius
                                : (torpedo ? blastRadius : config::JetRingRadius),
                           crit ? config::CritRingColor
                                : (torpedo ? config::HudBossColor : config::HudPipFullEdge));

                if (!torpedo)
                    ++m_tally.shotsHit;
                else
                {
                    addShake(config::ShakeTorpedo);
                    addHitStop(config::HitStopTorpedo);
                }

                if (!enemy.isAlive())
                    killEnemy(enemy);

                if (torpedo)
                {
                    // Every torpedo detonates. This is what makes a swarm of
                    // hatchlings answerable without a specific weapon build.
                    areaDamage(shot.getPosition(), blastRadius,
                               std::max(1, static_cast<int>(std::lround(
                                                static_cast<float>(shot.getDamage())
                                                * config::TorpedoSplashFactor))));

                    if (m_playerStats.has(Trait::ClusterTorpedoes))
                    {
                        for (int i = 0; i < config::TorpedoClusterShots; ++i)
                        {
                            const sf::Angle angle = sf::degrees(
                                360.0f * static_cast<float>(i) / config::TorpedoClusterShots);
                            m_pendingPlayerShots.emplace_back(m_assets.waterJet, shot.getPosition(),
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
                addShake(config::ShakeBossPart);
                addHitStop(config::HitStopBossPart);

                if (chains)
                    m_boss->splashNeighbours(hit.partIndex, config::BossChainDamage);
            }
            else
            {
                m_audio.play(Sfx::Impact);
            }

            if (!torpedo)
                ++m_tally.shotsHit;

            if (hit.killedBoss || !m_boss->isAlive())
            {
                // The end of a creature is allowed to take its time. Everything
                // the game has goes into this one: the water slows right down,
                // the screen washes out, and the body comes apart in pieces
                // rather than in one burst.
                bigMoment(hit.at, config::HudBossColor, config::ShakeDeath,
                          config::SlowMotionBossDeath);

                for (int i = 0; i < 6; ++i)
                {
                    const sf::Vector2f scatter(rng::range(30.0f, 150.0f),
                                               sf::degrees(rng::range(0.0f, 360.0f)));
                    effects::enemyDeath(m_effects, hit.at + scatter);
                    addHitRing(hit.at + scatter, 90.0f + 40.0f * static_cast<float>(i),
                               config::VentGlowColor);
                }

                m_audio.play(Sfx::BossDeath);
                m_audio.play(Sfx::BossRoar, 1.4f);

                awardSalvage(m_boss->getSalvage());

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

void Game::flushPendingShots()
{
    // Appended once a frame, well away from the loops that produced them.
    for (Projectile& shot : m_pendingPlayerShots)
        m_playerShots.push_back(std::move(shot));
    for (Projectile& shot : m_pendingEnemyShots)
        m_enemyShots.push_back(std::move(shot));

    m_pendingPlayerShots.clear();
    m_pendingEnemyShots.clear();
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

void Game::drawThreatArrows()
{
    // Screen space, so the arrows ride the edge of the frame no matter where
    // the camera is. A creature outside the frame gets a triangle on the border
    // pointing at it, sized by how close it is.
    const sf::Vector2f half{ config::WindowWidth / 2.0f, config::WindowHeight / 2.0f };

    // Whether a creature is on screen is a question about the frame; where its
    // arrow sits is a question about the frame minus the HUD, which is why the
    // two rectangles are not the same one.
    const sf::Vector2f inner{ half.x, half.y };
    const float left = config::ThreatArrowMargin;
    const float right = static_cast<float>(config::WindowWidth) - config::ThreatArrowMargin;
    const float top = config::ThreatArrowTopInset;
    const float bottom = static_cast<float>(config::WindowHeight) - config::ThreatArrowMargin;

    struct Threat { float distance; sf::Vector2f offset; bool boss; };
    std::vector<Threat> threats;

    const auto consider = [&](sf::Vector2f position, bool boss) {
        const sf::Vector2f offset = position - m_cameraCentre;

        // Already on screen, or so far out that pointing at it is just clutter.
        if (std::abs(offset.x) < inner.x && std::abs(offset.y) < inner.y)
            return;

        const float distance = offset.length();
        if (distance > config::ThreatArrowRange)
            return;

        threats.push_back({ distance, offset, boss });
    };

    for (const Enemy& enemy : m_enemies)
    {
        if (enemy.isAlive())
            consider(enemy.getPosition(), false);
    }

    if (m_boss && m_boss->isAlive())
        consider(m_boss->getPosition(), true);

    // Nearest first, then capped: ten arrows is a warning, thirty is wallpaper.
    std::sort(threats.begin(), threats.end(),
              [](const Threat& a, const Threat& b) { return a.distance < b.distance; });
    if (threats.size() > config::MaxThreatArrows)
        threats.resize(config::MaxThreatArrows);

    sf::VertexArray arrows(sf::PrimitiveType::Triangles);

    for (const Threat& threat : threats)
    {
        // Where the line out to the creature leaves the placement rectangle.
        const sf::Vector2f direction = threat.offset.normalized();
        const float limitX = direction.x > 0.0f ? right - half.x : half.x - left;
        const float limitY = direction.y > 0.0f ? bottom - half.y : half.y - top;
        const float toSide = limitX / std::max(std::abs(direction.x), 0.0001f);
        const float toTop = limitY / std::max(std::abs(direction.y), 0.0001f);
        const sf::Vector2f tip = half + direction * std::min(toSide, toTop);

        const sf::Vector2f side{ -direction.y, direction.x };
        const float size = threat.boss ? 15.0f : 10.0f;

        // Close threats are solid, distant ones barely there, so the edge of the
        // screen reads as a proximity meter rather than a list.
        const float nearness = 1.0f - threat.distance / config::ThreatArrowRange;
        sf::Color colour = threat.boss ? config::HudBossColor : config::HudPipFullEdge;
        colour.a = static_cast<std::uint8_t>(60.0f + 175.0f * nearness * nearness);

        arrows.append(sf::Vertex{ tip + direction * size * 0.9f, colour });
        arrows.append(sf::Vertex{ tip - direction * size * 0.7f + side * size * 0.7f, colour });
        arrows.append(sf::Vertex{ tip - direction * size * 0.7f - side * size * 0.7f, colour });
    }

    if (arrows.getVertexCount() > 0)
        m_window.draw(arrows);
}

void Game::drawPauseOverlay()
{
    // Its own veil rather than the death overlay: pausing is not dying, and a
    // red screen would say it was.
    m_deathOverlay.setFillColor(config::PauseVeil);
    m_window.draw(m_deathOverlay);
    m_deathOverlay.setFillColor(config::DeathOverlayColor);

    sf::VertexArray text(sf::PrimitiveType::Triangles);

    const std::string title = "HOLDING AT DEPTH";
    pixelfont::append(text, title,
                      { (config::WindowWidth - pixelfont::measure(title, 4.0f)) / 2.0f, 250.0f },
                      4.0f, config::HudTextColor);

    // One block, two columns: the keys line up under each other and the block
    // as a whole is centred, rather than every row being centred on its own.
    struct Row { const char* key; const char* what; bool warn; };
    const Row rows[] = {
        { "ESC / P", "BACK INTO IT", false },
        { "M",       "MUTE", false },
        { "F",       "LAMP ON OR OFF", false },
        { "F11",     "FULLSCREEN", false },
        { "Q",       "SURFACE, KEEPING THE SALVAGE", true },
    };

    const float scale = 1.8f;
    const float keyX = static_cast<float>(config::WindowWidth) / 2.0f - 210.0f;
    const float whatX = keyX + 130.0f;

    float y = 356.0f;
    for (const Row& row : rows)
    {
        pixelfont::append(text, row.key, { keyX, y }, scale, config::HudDimTextColor);
        pixelfont::append(text, row.what, { whatX, y }, scale,
                          row.warn ? config::HudSalvageColor : config::HudTextColor);
        y += 30.0f;
    }

    m_window.draw(text);
}

namespace
{
    // Every flat screen draws the same way: a veil, a heading, and rows of
    // label and value. Shared so they cannot drift apart from each other.
    void appendRow(sf::VertexArray& text, const std::string& left, const std::string& right,
                   float y, float scale, sf::Color leftColour, sf::Color rightColour,
                   float leftX, float rightX)
    {
        pixelfont::append(text, left, { leftX, y }, scale, leftColour);
        pixelfont::append(text, right, { rightX, y }, scale, rightColour);
    }

    void appendHeading(sf::VertexArray& text, const std::string& title, const std::string& hint)
    {
        pixelfont::append(text, title,
                          { (config::WindowWidth - pixelfont::measure(title, 3.4f)) / 2.0f, 74.0f },
                          3.4f, config::HudTextColor);
        pixelfont::append(text, hint,
                          { (config::WindowWidth - pixelfont::measure(hint, 1.5f)) / 2.0f, 660.0f },
                          1.5f, config::HudDimTextColor);
    }
}

void Game::drawControls()
{
    m_deathOverlay.setFillColor(config::PauseVeil);
    m_window.draw(m_deathOverlay);
    m_deathOverlay.setFillColor(config::DeathOverlayColor);

    sf::VertexArray text(sf::PrimitiveType::Triangles);
    appendHeading(text, "CONTROLS", "ESCAPE GOES BACK");

    struct Row { const char* key; const char* what; };
    const Row rows[] = {
        { "W A S D",      "DRIVE" },
        { "MOUSE",        "AIM. THE HULL TURNS TO FOLLOW IT" },
        { "LEFT MOUSE",   "WATER JET, HELD DOWN" },
        { "RIGHT MOUSE",  "TORPEDO, ONCE THE TUBE IS FITTED" },
        { "SHIFT",        "DASH, ONCE THE ENGINE TAKES IT" },
        { "F",            "KILL THE LAMP. RUN DARK." },
        { "TAB",          "THE UPGRADE TREE, MID DIVE" },
        { "WHEEL  + -",   "ZOOM THE TREE" },
        { "R",            "STRIP THE HULL BACK, IN THE HUB" },
        { "ESC  OR  P",   "HOLD AT DEPTH" },
        { "M",            "MUTE" },
        { "F11",          "FULLSCREEN" },
    };

    const float keyX = static_cast<float>(config::WindowWidth) / 2.0f - 300.0f;
    const float whatX = keyX + 250.0f;

    float y = 176.0f;
    for (const Row& row : rows)
    {
        appendRow(text, row.key, row.what, y, 1.8f, config::HudSalvageColor,
                  config::HudTextColor, keyX, whatX);
        y += 38.0f;
    }

    m_window.draw(text);
}

void Game::drawOptions()
{
    m_deathOverlay.setFillColor(config::PauseVeil);
    m_window.draw(m_deathOverlay);
    m_deathOverlay.setFillColor(config::DeathOverlayColor);

    sf::VertexArray text(sf::PrimitiveType::Triangles);
    appendHeading(text, "OPTIONS", "ARROWS CHANGE.  ESCAPE GOES BACK.");

    // A bar of ten blocks reads as a level at a glance without needing a
    // slider widget or a number nobody can picture.
    const auto meter = [](int tenths) {
        std::string bar;
        for (int i = 0; i < 10; ++i)
            bar += i < tenths ? '#' : '-';
        return bar;
    };

    const std::string rows[5][2] = {
        { "DIFFICULTY", difficulty().name },
        { "EFFECTS",    meter(m_save.sfxVolume) },
        { "THE DRONE",  meter(m_save.musicVolume) },
        { "MUTE",       m_save.muted ? "YES" : "NO" },
        { "FULLSCREEN", m_fullscreen ? "YES" : "NO" },
    };

    const float labelX = static_cast<float>(config::WindowWidth) / 2.0f - 260.0f;
    const float valueX = labelX + 300.0f;

    float y = 220.0f;
    for (int i = 0; i < 5; ++i)
    {
        const bool picked = i == m_optionRow;

        appendRow(text, (picked ? "> " : "  ") + rows[i][0], rows[i][1], y, 2.0f,
                  picked ? config::HudTextColor : config::HudDimTextColor,
                  picked ? config::HudSalvageColor : config::HudDimTextColor,
                  labelX, valueX);
        y += 46.0f;
    }

    // The difficulty's own sentence, under the rows, because the name alone
    // does not say what it actually does.
    const std::string note = difficulty().note;
    pixelfont::append(text, note,
                      { (config::WindowWidth - pixelfont::measure(note, 1.6f)) / 2.0f, 490.0f },
                      1.6f, config::HudDimTextColor);

    m_window.draw(text);
}

void Game::drawVictory()
{
    m_deathOverlay.setFillColor(config::PauseVeil);
    m_window.draw(m_deathOverlay);
    m_deathOverlay.setFillColor(config::DeathOverlayColor);

    sf::VertexArray text(sf::PrimitiveType::Triangles);

    const auto centred = [&](const std::string& line, float y, float scale, sf::Color colour) {
        pixelfont::append(text, line,
                          { (config::WindowWidth - pixelfont::measure(line, scale)) / 2.0f, y },
                          scale, colour);
    };

    centred("THE FLOOR OF IT", 190.0f, 4.4f, config::HudBossColor);
    centred("TWENTY LEVELS DOWN, THE WATER STOPS MOVING.", 300.0f, 1.8f, config::HudTextColor);
    centred("NOTHING COMES. NOTHING IS WAITING.", 330.0f, 1.8f, config::HudTextColor);
    centred("WHATEVER WAS HUNTING YOU IS BEHIND YOU NOW,", 372.0f, 1.8f,
            config::HudDimTextColor);
    centred("AND THE TRENCH KEEPS GOING.", 402.0f, 1.8f, config::HudDimTextColor);

    centred("THE DIVE IS OPEN PAST ITS OWN END NOW -", 470.0f, 1.6f, config::HudSalvageColor);
    centred("THE LEVELS AFTER THIS ONE ARE BUILT BY THE TRENCH ITSELF.", 496.0f, 1.6f,
            config::HudSalvageColor);

    centred("ENTER", 580.0f, 2.2f, config::HudTextColor);

    m_window.draw(text);
}

void Game::drawCredits()
{
    m_deathOverlay.setFillColor(config::PauseVeil);
    m_window.draw(m_deathOverlay);
    m_deathOverlay.setFillColor(config::DeathOverlayColor);

    sf::VertexArray text(sf::PrimitiveType::Triangles);

    // Scrolls from the bottom and stops when the last line reaches the top
    // third, so the list can be read rather than chased.
    struct Line { const char* text; float scale; bool accent; };
    const Line lines[] = {
        { "NERITH", 4.2f, true },
        { "", 1.0f, false },
        { "A 2D UNDERWATER HORROR SHOOTER", 1.7f, false },
        { "WRITTEN IN C++ AND SFML 3, WITHOUT AN ENGINE", 1.7f, false },
        { "", 1.0f, false },
        { "DESIGN AND DIRECTION", 1.5f, true },
        { "PAPO", 2.0f, false },
        { "", 1.0f, false },
        { "CODE", 1.5f, true },
        { "PAPO  WITH  CLAUDE", 2.0f, false },
        { "", 1.0f, false },
        { "ART AND SOUND", 1.5f, true },
        { "GENERATED, EVERY PIXEL AND EVERY WAVEFORM", 1.7f, false },
        { "NO ASSET WAS DOWNLOADED", 1.7f, false },
        { "", 1.0f, false },
        { "BUILT WITH", 1.5f, true },
        { "SFML 3.0.2   -   MSVC   -   WINDOWS", 1.7f, false },
        { "", 1.0f, false },
        { "", 1.0f, false },
        { "THANK YOU FOR GOING DOWN THERE", 2.0f, true },
    };

    float y = static_cast<float>(config::WindowHeight) - m_creditsScroll;

    for (const Line& line : lines)
    {
        const float height = 22.0f + line.scale * 9.0f;

        if (line.text[0] != '\0' && y > -60.0f && y < static_cast<float>(config::WindowHeight))
        {
            pixelfont::append(text, line.text,
                              { (config::WindowWidth
                                 - pixelfont::measure(line.text, line.scale)) / 2.0f, y },
                              line.scale,
                              line.accent ? config::HudSalvageColor : config::HudTextColor);
        }

        y += height;
    }

    m_window.draw(text);

    // The hint gets its own band, because the list scrolls straight through
    // where it sits.
    sf::RectangleShape band({ static_cast<float>(config::WindowWidth), 40.0f });
    band.setPosition({ 0.0f, static_cast<float>(config::WindowHeight) - 40.0f });
    band.setFillColor(sf::Color(2, 7, 12, 235));
    m_window.draw(band);

    sf::VertexArray hintText(sf::PrimitiveType::Triangles);
    const std::string hint = "ESCAPE GOES BACK";
    pixelfont::append(hintText, hint,
                      { (config::WindowWidth - pixelfont::measure(hint, 1.5f)) / 2.0f, 692.0f },
                      1.5f, config::HudDimTextColor);
    m_window.draw(hintText);
}

void Game::update(float deltaTime)
{
    // The world keeps breathing in every state. Only the fight pauses.
    m_arena.update(deltaTime);
    m_seabed.update(deltaTime);
    updateCamera(deltaTime);
    updateFeel(deltaTime);
    spawnAmbience(deltaTime);
    callFromTheDark(deltaTime);

    // A few frames of held time after something heavy lands. The camera and the
    // ambience above carry on through it, so the pause reads as impact rather
    // than as the game locking up.
    if (m_hitStop > 0.0f)
    {
        m_hitStop -= deltaTime;
        if (m_state == State::Fighting)
            return;
    }

    if (upgradesVisible())
    {
        // The tree lives in screen space, so the cursor has to be mapped through
        // the fixed view rather than the camera that follows the sub.
        m_upgradeScreen.update(deltaTime);
        m_upgradeScreen.setLevels(m_save.selectedLevel, m_save.unlockedLevel);
        m_upgradeScreen.setCursor(m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window), m_ui),
                                  m_tree);
    }

    switch (m_state)
    {
        case State::Story:
            m_story.update(deltaTime);
            if (m_story.isFinished())
                m_state = m_afterStory;
            break;

        case State::Menu:
            m_menuTime += deltaTime;
            m_menu.setCursor(m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window), m_ui));
            break;

        case State::Credits:
            // Stops rather than looping: a list that scrolls forever reads as
            // broken, and there is nothing after the last line anyway.
            m_creditsScroll = std::min(m_creditsScroll + config::CreditsScrollSpeed * deltaTime,
                                       config::CreditsScrollEnd);
            break;

        case State::Controls:
        case State::Options:
        case State::Victory:
            break;

        case State::LevelIntro:
            m_stateTimer -= deltaTime;
            if (m_stateTimer <= 0.0f)
                m_state = State::Fighting;
            break;

        case State::Fighting:
            if (!m_showUpgrades && !m_paused)
            {
                // Slow motion applies to the fight and to nothing else: the
                // camera, the ambience and the clock the player is being timed
                // on all keep real time, so a moment is a moment rather than a
                // way to buy seconds.
                deltaTime *= m_slowScale;

                m_tally.seconds += deltaTime;
                m_player.update(deltaTime, getMouseWorldPosition(), m_arena);
                handleShooting(deltaTime);
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
                updateOrbits(deltaTime);
                updateVents(deltaTime);
                updateNests(deltaTime);
                updateSonar(deltaTime);

                handleCollisions();
                flushPendingShots();
                updateHitRings(deltaTime);
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
                m_showUpgrades = false;
                m_upgradeScreen.clearCursor();
                m_upgradeScreen.recentre();

                // Clearing the last authored level is the end of the campaign,
                // and it only lands once: everything past it is the trench
                // building its own water.
                if (m_justBeatCampaign)
                {
                    m_justBeatCampaign = false;
                    m_state = State::Victory;
                }
                // Killing a depth ends a chapter, so the story lands between
                // the kill and the shopping rather than during either.
                else if (m_level.isBoss())
                    maybePlayStory(m_level.getDepth(), State::Upgrading);
                else
                    m_state = State::Upgrading;
            }
            break;

        case State::GameOver:
            // Drowning costs the attempt and nothing else, so it drops the player
            // back in the hub rather than to the start of the campaign.
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

    // Far band first: rock past the reef, sliding slower than the floor.
    m_seabed.drawFar(m_window, m_cameraCentre);
    m_seabed.draw(m_window);
    m_arena.drawLight(m_window, m_player.getPosition());

    m_ambience.draw(m_window);

    // Under everything alive: a vent is a hole in the floor, not an object.
    drawVents();
    drawNests();

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
    {
        m_player.draw(m_window);
        drawOrbits();
    }

    // Hit rings sit over the effects: they are the readable part of a hit.
    for (const HitRing& ring : m_hitRings)
    {
        const float t = ring.age / ring.life;
        const float radius = ring.radius * (0.25f + 0.75f * t);

        sf::Color edge = ring.colour;
        edge.a = static_cast<std::uint8_t>(235.0f * (1.0f - t) * (1.0f - t));

        m_sonarRing.setRadius(radius);
        m_sonarRing.setOrigin({ radius, radius });
        m_sonarRing.setPosition(ring.position);
        m_sonarRing.setOutlineThickness(2.0f + 3.0f * (1.0f - t));
        m_sonarRing.setOutlineColor(edge);
        m_window.draw(m_sonarRing);
    }

    m_effects.draw(m_window);

    // Near band last of everything in the world: silt passing between the
    // camera and the fight, which is what gives the water a thickness.
    m_seabed.drawNear(m_window, m_cameraCentre);

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

    // The moment wash. Over the gloom and under the HUD, because it is meant to
    // take the whole picture for an instant rather than tint it.
    if (m_flash > 0.0f)
    {
        sf::Color wash = m_flashColour;
        wash.a = static_cast<std::uint8_t>(190.0f * m_flash * m_flash);
        m_deathOverlay.setFillColor(wash);
        m_window.draw(m_deathOverlay);
        m_deathOverlay.setFillColor(config::DeathOverlayColor);
    }

    // A red wash over the whole frame, so a bite is felt at the edge of vision
    // rather than having to be read off the health pips.
    if (m_hurtFlash > 0.0f)
    {
        sf::Color wash = config::HurtFlashColor;
        wash.a = static_cast<std::uint8_t>(120.0f * m_hurtFlash * m_hurtFlash);
        m_deathOverlay.setFillColor(wash);
        m_window.draw(m_deathOverlay);
        m_deathOverlay.setFillColor(config::DeathOverlayColor);
    }

    if (m_state == State::Fighting && !upgradesVisible() && !m_paused)
        drawThreatArrows();

    if (m_state == State::Story)
    {
        m_story.draw(m_window);
        m_window.display();
        return;
    }

    if (m_state == State::Menu)
    {
        // The seabed keeps moving behind the title, so the game is already
        // breathing before the player has touched anything.
        m_menu.draw(m_window, m_menuTime);
        m_window.display();
        return;
    }

    if (m_state == State::Controls) { drawControls(); m_window.display(); return; }
    if (m_state == State::Options)  { drawOptions();  m_window.display(); return; }
    if (m_state == State::Victory)  { drawVictory();  m_window.display(); return; }
    if (m_state == State::Credits)  { drawCredits();  m_window.display(); return; }

    if (m_state == State::GameOver)
        m_window.draw(m_deathOverlay);

    if (upgradesVisible())
    {
        std::string prompt = m_state == State::Upgrading
                                 ? "ENTER TO DIVE INTO LEVEL "
                                       + std::to_string(m_save.selectedLevel)
                                 : "TAB TO CLOSE";

        if (m_respecArmed)
        {
            prompt = "R AGAIN TO STRIP THE HULL FOR "
                     + std::to_string(static_cast<int>(std::lround(
                           static_cast<float>(m_tree.getSpent()) * config::RespecRefund)))
                     + " SALVAGE";
        }

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
            const float alpha = bannerAlpha(m_stateTimer, config::LevelClearDuration);

            m_hud.drawBanner(m_window, "LEVEL " + std::to_string(m_level.getNumber()),
                             m_level.isBoss() ? "DEPTH CLEARED" : "CLEAR",
                             m_rerun ? "WATER ALREADY OPENED - REDUCED PAY" : "",
                             config::HudSalvageColor, alpha);

            Hud::Summary summary;
            summary.kills = m_tally.kills;
            summary.salvage = m_tally.salvage;
            summary.seconds = m_tally.seconds;
            summary.damageTaken = m_tally.damageTaken;
            summary.accuracy = m_tally.shotsFired > 0
                                   ? static_cast<float>(m_tally.shotsHit)
                                         / static_cast<float>(m_tally.shotsFired)
                                   : 0.0f;

            m_hud.drawSummary(m_window, summary, alpha);
        }
        else if (m_state == State::GameOver)
        {
            m_hud.drawBanner(m_window, "LEVEL " + std::to_string(m_level.getNumber()),
                             "THE DARK TOOK YOU",
                             "NOTHING LOST. TRY IT AGAIN.",
                             config::HudBossColor,
                             bannerAlpha(m_stateTimer, config::DeathBannerDuration));
        }
    }

    if (m_paused)
        drawPauseOverlay();

    m_window.display();
}
