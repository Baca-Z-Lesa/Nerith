#include "core/Game.h"

#include "core/Config.h"
#include "core/Mesh.h"
#include "core/Random.h"
#include "ui/PixelFont.h"
#include "world/Bestiary.h"

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Sleep.hpp>
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
    , m_diveSalvage(0)
    , m_lostSalvage(0)
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
    , m_bossTelling(false)
    , m_streak(0)
    , m_bestStreak(0)
    , m_blocks(0)
    , m_carrying(-1)
    , m_hullLost(0)
    , m_sirenTimer(0.0f)
    , m_optionRow(0)
    , m_beastRow(0)
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
    // No limiter: the pacer at the top of the loop does this, and it does it
    // where the spare time is still worth something.
    m_window.setFramerateLimit(0);
    m_window.setVerticalSyncEnabled(m_save.vsync);
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
    // Follow the sub anywhere in the basin, but never past its edges: the map
    // is far larger than the old circle, so a fixed leash on the middle would
    // leave the player swimming off the side of the screen.
    sf::Vector2f target = m_player.getPosition();

    const sf::Vector2f half{ config::WindowWidth / 2.0f, config::WindowHeight / 2.0f };
    const sf::Vector2f low = m_arena.getBoundsMin() - half * config::CameraOvershoot;
    const sf::Vector2f high = m_arena.getBoundsMax() + half * config::CameraOvershoot;

    target.x = std::clamp(target.x, low.x + half.x, std::max(low.x + half.x, high.x - half.x));
    target.y = std::clamp(target.y, low.y + half.y, std::max(low.y + half.y, high.y - half.y));

    // Exponential, written the way an exponential actually is. min(1, k * dt)
    // is only the same curve while dt is small, and it is what made the camera
    // follow at a different rate on a different machine.
    // How hard the view follows is the player's call: a loose camera reads
    // as weight to some people and as the sub not answering to others.
    const float follow = config::CameraLerpLoose
                         + (config::CameraLerpTight - config::CameraLerpLoose)
                               * static_cast<float>(m_save.cameraTight) / 10.0f;

    const float blend = 1.0f - std::exp(-follow * deltaTime);
    m_cameraCentre += (target - m_cameraCentre) * blend;
}

void Game::aimCamera(float alpha)
{
    // The view sits between the last two steps, like everything else being
    // drawn. Setting it here rather than in the step also means the cursor is
    // mapped through the view the player was actually looking at when they
    // put it there.
    sf::Vector2f centre = m_prevCameraCentre + (m_cameraCentre - m_prevCameraCentre) * alpha;

    // The jolt is applied to the view rather than to m_cameraCentre, so it
    // never drags the follow target along with it and never fights the clamp.
    if (m_shake > 0.05f)
        centre += sf::Vector2f{ rng::range(-m_shake, m_shake), rng::range(-m_shake, m_shake) };

    m_camera.setCenter(centre);
}

void Game::addShake(float strength)
{
    // Scaled by what the player asked for before anything else. Turning the
    // shake down should not also turn down the hit stop, the ring or the
    // flash - it is one thing somebody cannot take, not the whole of the
    // feedback.
    strength *= static_cast<float>(m_save.shake) / 10.0f;

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
    strength *= static_cast<float>(m_save.flashes) / 10.0f;

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

void Game::drawBossTell()
{
    const float tell = m_boss->getTell();
    if (tell <= 0.0f)
        return;

    const sf::Vector2f at = m_boss->getPosition();
    const float body = m_boss->getRadius();
    const bool lunge = m_boss->isTellLunge();
    const sf::Color base = lunge ? config::BossTellLungeColor : config::BossTellColor;

    // A ring closing on the body. It starts wide and loose and arrives tight
    // and bright exactly as the blow does, so the timing is readable without a
    // number and without having to know the fight.
    const float radius = body * (config::BossTellRing - (config::BossTellRing - 1.0f) * tell);

    sf::Color edge = base;
    edge.a = static_cast<std::uint8_t>(90.0f + 165.0f * tell);

    m_sonarRing.setRadius(radius);
    m_sonarRing.setOrigin({ radius, radius });
    m_sonarRing.setPosition(at);
    m_sonarRing.setOutlineThickness(2.0f + 5.0f * tell);
    m_sonarRing.setOutlineColor(edge);
    m_window.draw(m_sonarRing);

    if (!lunge)
        return;

    // A charge also draws the line it is about to take, tapering away from the
    // creature. Standing on that line is the mistake; stepping off it is the
    // answer, and both have to be visible before it moves.
    const sf::Vector2f aim = m_boss->getTellAim();
    const sf::Vector2f across{ -aim.y, aim.x };
    const sf::Vector2f nose = at + aim * body;
    const sf::Vector2f tip = nose + aim * (body * config::BossTellReach * tell);

    sf::Color root = base;
    root.a = static_cast<std::uint8_t>(150.0f * tell);
    sf::Color point = base;
    point.a = 0;

    sf::VertexArray spear(sf::PrimitiveType::Triangles);
    spear.append(sf::Vertex{ nose - across * (body * 0.30f), root });
    spear.append(sf::Vertex{ nose + across * (body * 0.30f), root });
    spear.append(sf::Vertex{ tip, point });
    m_window.draw(spear);
}

void Game::scheduleVolley(float delay, sf::Vector2f at, sf::Vector2f direction, float speed)
{
    // A move that takes four seconds to happen cannot be a function call. The
    // queue is what lets a boss tear the floor open a step at a time, or sweep
    // a beam across a hall, without any of it having to live in the boss.
    m_volleys.push_back({ delay, at, direction, speed });
}

void Game::updateVolleys(float deltaTime)
{
    if (m_volleys.empty())
        return;

    // Nothing left to have fired them: a queue outliving its boss would spit
    // orbs out of a dead thing's mouth once the next one arrived.
    if (!m_boss)
    {
        m_volleys.clear();
        return;
    }

    const int orbDamage = config::EnemyOrbDamage + m_level.getScaling().bonusDamage;

    for (std::size_t i = m_volleys.size(); i-- > 0;)
    {
        m_volleys[i].delay -= deltaTime;
        if (m_volleys[i].delay > 0.0f)
            continue;

        const Volley due = m_volleys[i];
        m_volleys.erase(m_volleys.begin() + static_cast<std::ptrdiff_t>(i));

        config::ProjectileStats orb = config::EnemyOrb;
        orb.speed = due.speed;

        m_pendingEnemyShots.emplace_back(m_assets.boss(m_boss->getShotTexture()), due.at,
                                         due.direction, orb, orbDamage);
        effects::hostileMuzzle(m_effects, due.at, due.direction);
    }
}

void Game::applyBossMove(const BossMove& move)
{
    const sf::Vector2f me = m_player.getPosition();

    switch (move.kind)
    {
        case BossSpecial::Closing:
        {
            // The room falls in on you from every side at once, with one way
            // out. Finding the gap is the move; there is no dodging a circle.
            const float open = rng::range(0.0f, 360.0f);

            for (int i = 0; i < config::ClosingOrbs; ++i)
            {
                const float around = 360.0f * static_cast<float>(i)
                                     / static_cast<float>(config::ClosingOrbs);

                float off = std::fmod(std::abs(around - open), 360.0f);
                off = std::min(off, 360.0f - off);

                if (off < config::ClosingGap * 0.5f)
                    continue;

                const sf::Vector2f out = unit(sf::degrees(around));

                scheduleVolley(0.0f, move.at + out * config::ClosingRadius, -out,
                               config::ClosingSpeed);
            }

            addFlash(config::HudBossColor, 0.30f);
            m_audio.play(Sfx::Charge, 1.1f);
            break;
        }

        case BossSpecial::Curtain:
        {
            // A wall the width of the hall, crossing it, with a hole somewhere
            // along it. You do not out-run a wall, you go through it.
            const sf::Vector2f along{ -move.direction.y, move.direction.x };
            const float hole = rng::range(-0.34f, 0.34f);

            for (int i = 0; i < config::CurtainOrbs; ++i)
            {
                const float t = static_cast<float>(i)
                                    / static_cast<float>(config::CurtainOrbs - 1) - 0.5f;

                if (std::abs(t - hole) * config::CurtainWidth < config::CurtainGap)
                    continue;

                scheduleVolley(0.0f, move.at + along * (t * config::CurtainWidth),
                               move.direction, config::CurtainSpeed);
            }

            m_audio.play(Sfx::BossPhase, 0.9f);
            break;
        }

        case BossSpecial::Fissure:
        {
            // The floor tearing open along a line, a step at a time, so the
            // answer is to move across it rather than away from it.
            for (int i = 1; i <= config::FissureSteps; ++i)
            {
                const sf::Vector2f at = move.at
                                        + move.direction * (config::FissureStride
                                                            * static_cast<float>(i));

                const sf::Vector2f across{ -move.direction.y, move.direction.x };

                scheduleVolley(config::FissureDelay * static_cast<float>(i), at, across,
                               config::ClosingSpeed);
                scheduleVolley(config::FissureDelay * static_cast<float>(i), at, -across,
                               config::ClosingSpeed);
            }

            addShake(config::ShakeBossPart);
            m_audio.play(Sfx::Vent, 1.2f);
            break;
        }

        case BossSpecial::Rake:
        {
            // A beam swept across the hall. Slow enough to walk out of and long
            // enough that standing anywhere is a decision.
            const float from = move.direction.angle().asDegrees() - config::RakeArc * 0.5f;
            const float turn = rng::range(0.0f, 1.0f) < 0.5f ? 1.0f : -1.0f;

            for (int i = 0; i < config::RakeSteps; ++i)
            {
                const float t = static_cast<float>(i) / static_cast<float>(config::RakeSteps - 1);
                const float angle = from + config::RakeArc * (turn > 0.0f ? t : 1.0f - t);

                scheduleVolley(config::RakeDelay * static_cast<float>(i), move.at,
                               unit(sf::degrees(angle)), config::RakeSpeed);
            }

            m_audio.play(Sfx::Charge, 0.85f);
            break;
        }

        case BossSpecial::Grab:
            // Armed rather than applied: the bite only lands if the lunge it
            // was announced with actually connects, which is checked where
            // contact is checked.
            //
            // And only against a sub that can get out of it. A hold with no
            // answer is not a move, it is a sentence, and the dash is the
            // answer - so no dash, no hold.
            if (m_playerStats.has(Trait::Dash))
                m_grabArmed = config::BossWindTime + config::BossStrikeTime;
            break;

        case BossSpecial::Sweep:
        {
            // An arc off its flank. Being behind it is safe, being anywhere in
            // front of it is not, and it throws you rather than only hurting.
            const sf::Vector2f offset = me - move.at;
            const float distance = offset.length();

            addHitRing(move.at, config::SweepRadius, config::SweepColor);
            addShake(config::ShakeBossPart);
            m_audio.play(Sfx::Vent, 0.85f);

            if (distance > config::SweepRadius || distance <= 1.0f)
                break;

            const sf::Vector2f direction = offset / distance;
            const float cosine = direction.dot(move.direction);
            if (cosine < std::cos(config::SweepArc * 3.14159265f / 180.0f))
                break;

            m_pushTo = direction * config::SweepPush;
            m_pushTime = 0.30f;
            // The sweep does its own throwing, so it must not get a second one.
            hurtPlayer(config::SweepDamage,
                       { m_boss->getPosition(), std::nullopt, true, false });
            break;
        }

        case BossSpecial::Seed:
        {
            // It plants. What it plants is not the fight, it is the clock: the
            // longer the pods are left the more of the room is theirs.
            for (int i = 0; i < config::SeedCount; ++i)
            {
                const sf::Vector2f at = m_arena.clampInside(
                    move.at + sf::Vector2f(rng::range(120.0f, config::SeedSpread),
                                           sf::degrees(rng::range(0.0f, 360.0f))),
                    config::NestRadius);

                Nest pod{ sf::Sprite(m_assets.boss(config::NestTexturePath)), at,
                          config::SeedHealth, config::SeedHealth,
                          rng::range(1.4f, 2.6f), 0.0f, true };

                pod.sprite.setOrigin(sf::Vector2f(pod.sprite.getTexture().getSize()) / 2.0f);
                pod.sprite.setPosition(at);
                pod.sprite.setScale({ 0.6f, 0.6f });
                pod.sprite.setRotation(sf::degrees(rng::range(0.0f, 360.0f)));

                m_nests.push_back(std::move(pod));
                addHitRing(at, config::NestRadius * 1.6f, config::HudSalvageColor);
            }

            m_audio.play(Sfx::Hatch, 0.7f);
            break;
        }

        case BossSpecial::Tide:
        {
            // It pushes the whole room away from itself, hostile water
            // included, and then fills the space that opens up.
            const sf::Vector2f offset = me - move.at;
            const float distance = offset.length();

            addHitRing(move.at, config::TideRadius, config::TideColor);
            addShake(config::ShakeTorpedo);
            m_audio.play(Sfx::BossPhase, 0.8f);

            if (distance > 1.0f && distance < config::TideRadius)
            {
                const float fall = 1.0f - distance / config::TideRadius;
                m_pushTo = offset / distance * config::TidePush * fall;
                m_pushTime = 0.35f;
            }

            for (Projectile& shot : m_enemyShots)
            {
                const sf::Vector2f away = shot.getPosition() - move.at;
                if (away.lengthSquared() < config::TideRadius * config::TideRadius)
                    shot.kill();
            }
            break;
        }

        case BossSpecial::Silt:
            // Blind. The lamp is the only reason the basin is navigable, and
            // for the next few seconds most of it is gone.
            m_siltTimer = config::SiltDuration;
            refreshLamp();

            addFlash(config::EruptColor, 0.5f);
            m_audio.play(Sfx::Vent, 0.7f);
            break;

        case BossSpecial::Erupt:
            // Straight up through the floor where you are standing. The floor
            // bulges first, and the bulge is where it is going to be, not where
            // you were: standing still is the mistake.
            m_eruptAt = move.at;
            m_eruptTimer = config::EruptWarning;
            m_audio.play(Sfx::Charge, 0.7f);
            break;

        case BossSpecial::Well:
            m_wellAt = move.at;
            m_wellTimer = config::WellDuration;
            addHitRing(move.at, config::WellPull * 2.0f, config::WellColor);
            m_audio.play(Sfx::BossPhase, 1.2f);
            break;

        case BossSpecial::Hurl:
        {
            // It tears a piece of the floor out and throws it. Heavy, slow
            // enough to read, and it does not care what is between the two of
            // you.
            const sf::Vector2f direction = (me - move.at).lengthSquared() > 1.0f
                                               ? (me - move.at).normalized()
                                               : move.direction;

            const config::ProjectileStats rock{ config::HurlSpeed, config::HurlRadius, 1.6f };

            m_pendingEnemyShots.emplace_back(m_assets.boss(config::SerpentSegmentPath),
                                             move.at + direction * 90.0f, direction, rock,
                                             config::HurlDamage
                                                 + m_level.getScaling().bonusDamage);

            addShake(config::ShakeBossPart);
            m_audio.play(Sfx::PartBreak, 0.8f);
            break;
        }
    }
}

void Game::updateBossMoves(float deltaTime)
{
    // Being thrown, or pulled. Both override steering entirely for the short
    // time they last, which is exactly what makes them feel like something
    // happening to you rather than a number changing.
    if (m_pushTime > 0.0f)
    {
        m_pushTime -= deltaTime;
        m_player.setPosition(m_arena.clampInside(
            m_player.getPosition() + m_pushTo * deltaTime, config::PlayerRadius));
    }

    if (m_wellTimer > 0.0f)
    {
        m_wellTimer -= deltaTime;

        const sf::Vector2f offset = m_wellAt - m_player.getPosition();
        const float distance = offset.length();

        if (distance > 40.0f)
        {
            m_player.setPosition(m_arena.clampInside(
                m_player.getPosition() + offset / distance * config::WellPull * deltaTime,
                config::PlayerRadius));
        }
    }

    if (m_grabArmed > 0.0f)
        m_grabArmed -= deltaTime;

    // In its mouth. Nothing you press moves the sub, it bites on a clock, and
    // the dash is the only way out - the same key that gets a siphon off, for
    // the same reason.
    if (m_grabTimer > 0.0f)
    {
        m_grabTimer -= deltaTime;
        m_grabBite -= deltaTime;

        if (m_boss)
        {
            m_player.setPosition(m_arena.clampInside(m_boss->getPosition(),
                                                     config::PlayerRadius));
        }

        if (m_grabBite <= 0.0f)
        {
            m_grabBite = config::GrabBiteInterval;
            // Held in its mouth: being shoved out of it would be a reward.
            hurtPlayer(config::GrabBite + m_level.getScaling().bonusDamage,
                       { m_boss->getPosition(), std::nullopt, true, false });
            addShake(config::ShakeBossPart);
        }

        if (m_grabTimer <= 0.0f)
            addFlash(config::GrabColor, 0.3f);
    }

    if (m_siltTimer > 0.0f)
    {
        m_siltTimer -= deltaTime;
        if (m_siltTimer <= 0.0f)
            refreshLamp();
    }

    if (m_eruptTimer > 0.0f)
    {
        m_eruptTimer -= deltaTime;
        if (m_eruptTimer <= 0.0f)
        {
            effects::ventBurst(m_effects, m_eruptAt, config::EruptRadius * 0.7f);
            addHitRing(m_eruptAt, config::EruptRadius, config::EruptColor);
            addShake(config::ShakeDeath * 0.7f);
            addHitStop(config::HitStopBossPart);
            m_audio.play(Sfx::BossRoar, 0.9f);

            if ((m_player.getPosition() - m_eruptAt).length() <= config::EruptRadius)
                hurtPlayer(config::EruptDamage,
                           { m_eruptAt, std::nullopt, true, true });

            areaDamage(m_eruptAt, config::EruptRadius, config::EruptDamage);
        }
    }
}

void Game::drawBossMoves()
{
    // The floor about to open. Drawn as a ring that tightens, the same
    // language every other warning in the game uses.
    if (m_eruptTimer > 0.0f)
    {
        const float progress = 1.0f - m_eruptTimer / config::EruptWarning;
        const float radius = config::EruptRadius * (1.6f - 0.6f * progress);

        sf::Color edge = config::EruptColor;
        edge.a = static_cast<std::uint8_t>(90.0f + 160.0f * progress);

        m_sonarRing.setRadius(radius);
        m_sonarRing.setOrigin({ radius, radius });
        m_sonarRing.setPosition(m_eruptAt);
        m_sonarRing.setOutlineThickness(3.0f + 4.0f * progress);
        m_sonarRing.setOutlineColor(edge);
        m_window.draw(m_sonarRing);
    }

    if (m_wellTimer > 0.0f)
    {
        const float pulse = std::fmod(m_wellTimer * 1.6f, 1.0f);
        const float radius = config::WellPull * 2.0f * pulse;

        sf::Color edge = config::WellColor;
        edge.a = static_cast<std::uint8_t>(200.0f * (1.0f - pulse));

        m_sonarRing.setRadius(radius);
        m_sonarRing.setOrigin({ radius, radius });
        m_sonarRing.setPosition(m_wellAt);
        m_sonarRing.setOutlineThickness(3.0f);
        m_sonarRing.setOutlineColor(edge);
        m_window.draw(m_sonarRing);
    }

    if (m_grabTimer > 0.0f)
    {
        const float radius = config::GrabReach * (0.7f + 0.3f * std::sin(m_stateTimer * 24.0f));

        m_sonarRing.setRadius(radius);
        m_sonarRing.setOrigin({ radius, radius });
        m_sonarRing.setPosition(m_player.getPosition());
        m_sonarRing.setOutlineThickness(4.0f);
        m_sonarRing.setOutlineColor(config::GrabColor);
        m_window.draw(m_sonarRing);
    }
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

    // The score follows the fight rather than the level. The pulse comes up
    // when something is actually hunting you and drops when it is not, so a
    // quiet stretch is audibly quiet and being found is audible before it is
    // visible; the dread is the boss and nothing else.
    {
        const bool fighting = m_state == State::Fighting && !m_paused;

        const sf::Vector2f half{ config::WindowWidth / 2.0f, config::WindowHeight / 2.0f };

        int hunting = 0;
        int unseen = 0;

        if (fighting)
        {
            for (const Enemy& enemy : m_enemies)
            {
                if (!enemy.isAlive() || !enemy.isAware())
                    continue;

                ++hunting;

                // Hunting you from somewhere you cannot look at. Either it is
                // off the frame entirely, or it is one of the things that is
                // barely there until it is on top of you.
                const sf::Vector2f offset = enemy.getPosition() - m_cameraCentre;
                const float distance = offset.length();
                const bool offScreen = std::abs(offset.x) > half.x
                                       || std::abs(offset.y) > half.y;
                const bool hidden = config::enemyStats(enemy.getType()).stealth
                                    && distance > config::StealthRevealRange;

                if (offScreen || hidden)
                    ++unseen;
            }
        }

        const float pressure = std::min(1.0f, static_cast<float>(hunting)
                                                  / config::MusicPulseFull);

        // One thing you cannot see, hunting you, is already the whole point -
        // so this fills far sooner than the pulse does, and the two are asking
        // different questions rather than swelling together on the same
        // creature.
        const float stalked = std::min(1.0f, static_cast<float>(unseen)
                                                 / config::MusicStalkFull);

        m_audio.setLayer(AudioSystem::Layer::Pulse, fighting ? pressure : 0.0f);
        m_audio.setLayer(AudioSystem::Layer::Stalk, fighting ? stalked : 0.0f);
        m_audio.setLayer(AudioSystem::Layer::Dread,
                         (fighting && m_boss && m_boss->isAlive()) ? 1.0f : 0.0f);
    }

    // Where the ear is, and how much room there is around it. Both are asked
    // once a frame rather than per sound: the listener cannot be in two places
    // and the room does not change between one hit and the next.
    m_audio.setListener(m_player.getPosition());
    m_audio.setSpace(opennessAt(m_player.getPosition()));

    m_audio.updateMusic(deltaTime);

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

    // A new basin every single time. It used to be cut from the level number
    // alone, which made a level the same place on every dive - repeatable,
    // which sounded like a virtue right up until the second run down the same
    // corridor, when the map stops being somewhere you are finding your way
    // through and becomes something you are remembering.
    //
    // The level number still goes in, so the scale and the shape a depth wants
    // are still its own; what changes is which basin of that kind you get.
    m_arena.generate(static_cast<unsigned int>(number)
                         ^ static_cast<unsigned int>(rng::range(0.0f, 1.0e9f)),
                     m_level.isBoss());
    m_seabed.reshape(m_arena);

    // The little map is cut from the basin, so it is cut when the basin is.
    buildMinimap();

    // Anything shallower than the deepest water opened so far has been beaten
    // before, and beating it again is worth a fraction of the first time.
    m_rerun = number < m_save.unlockedLevel;

    m_tally = RunTally{};
    m_paused = false;
    m_bossAct = 0;
    m_streak = 0;
    m_bestStreak = 0;
    m_blocks = 0;
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
    placeCargo();

    // And whatever came down with you. A boss hall is the one place it does
    // not follow you into - a fight that is meant to be one creature is not
    // improved by a second one arriving with its own agenda.
    if (m_save.stalkerAlive && !m_level.isBoss())
        spawnStalker();

    // Every dive starts in a whole sub. What a beating costs lasts the dive
    // and not a minute longer: the pressure is meant to be inside a level.
    m_player.repairHull();
    m_hullLost = 0;
    m_haveWound = false;

    m_volleys.clear();
    m_harpoon.reset();
    m_harpoonPull = 0.0f;
    m_modeTimer = 0.0f;
    m_fireSlot = 0;

    m_pushTime = 0.0f;
    m_wellTimer = 0.0f;
    m_eruptTimer = 0.0f;
    m_siltTimer = 0.0f;
    m_grabArmed = 0.0f;
    m_grabTimer = 0.0f;
    m_callTimer = rng::range(6.0f, config::CreatureCallMax);
    m_boss.reset();
    m_lastSpawnAngle = rng::range(0.0f, 360.0f);
    m_mineTimer = config::MineInterval;
    m_sonarTimer = 0.5f;
    m_sonarSweep = 0.0f;

    // A dive starts in the first chamber, wherever the walk put it.
    m_player.setPosition(m_arena.getStart());
    m_cameraCentre = m_arena.getStart();

    m_player.onLevelStart();

    // The refit crew works between dives, not during one.
    if (m_playerStats.has(Trait::Refit))
        m_player.heal(m_playerStats.getInt(Stat::RepairPerLevel, 0));

    // A bought lamp is worth the same at every depth, so it is applied on top
    // of the theme rather than replacing it.
    m_lampOn = true;
    m_noiseTimer = 0.0f;
    refreshLamp();

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
        // In the hall it was given, at the far side of it, so the title card
        // plays over the thing itself rather than over empty water.
        const sf::Vector2f hall = m_arena.getHallCentre();
        const sf::Vector2f away = (hall - m_arena.getStart());

        m_boss.emplace(m_assets, bossForDepth(m_level.getDepth()),
                       hall + (away.lengthSquared() > 1.0f
                                   ? away.normalized() * (m_arena.getHallRadius() * 0.55f)
                                   : sf::Vector2f{ 0.0f, 0.0f }),
                       m_level.getScaling());

        // The camera is already looking at it when the card comes up, and the
        // roar lands on the first frame rather than somewhere in the middle.
        m_cameraCentre = m_boss->getPosition();

        m_audio.play(Sfx::BossRoar);
        m_audio.play(Sfx::BossPhase, 0.7f);
        addShake(config::ShakeDeath * 0.7f);
        addFlash(config::HudBossColor, 0.6f);
    }

    m_state = State::LevelIntro;
    m_stateTimer = config::LevelIntroDuration;
    m_showUpgrades = false;
}

void Game::followedDown()
{
    // Whatever is still in the water when the level ends does not simply stop
    // existing because the objective was met. One of them follows.
    const bool stalkerHere = m_save.stalkerAlive;
    bool stalkerSurvived = false;
    int left = 0;

    for (const Enemy& enemy : m_enemies)
    {
        if (!enemy.isAlive())
            continue;

        ++left;
        if (enemy.getType() == EnemyType::Stalker)
            stalkerSurvived = true;
    }

    if (stalkerHere)
    {
        // It was down here with you. Either you killed it - in which case it
        // is over, and the next dive is yours again - or it watched you leave.
        m_save.stalkerAlive = stalkerSurvived;
        m_save.stalkerLevels = stalkerSurvived ? m_save.stalkerLevels + 1 : 0;
        return;
    }

    // Nothing follows you out of the shallows, and nothing follows you out of
    // water you emptied. It is specifically the price of walking away.
    if (m_level.getDepth() >= config::StalkerFromDepth && left >= config::StalkerLeftBehind)
    {
        m_save.stalkerAlive = true;
        m_save.stalkerLevels = 0;
    }
}

void Game::onLevelComplete()
{
    followedDown();

    for (const Enemy& enemy : m_enemies)
        effects::enemyDeath(m_effects, enemy.getPosition());

    m_enemies.clear();
    m_enemyShots.clear();

    // The bonus is for opening water, so a repeat pays nothing for the clear
    // itself and only a fraction for what was in it.
    if (!m_rerun)
    {
        m_diveSalvage += config::FirstClearBase
                         + config::FirstClearPerLevel * m_level.getNumber();
    }

    bankSalvage();

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

    // The tree is never taken away and never will be: an upgrade you bought is
    // yours. What is taken away is the hold - everything this dive earned and
    // did not surface with. That is what makes the last two minutes of a level
    // a decision rather than a formality.
    // Whatever it was, the player has certainly met it now. Dying to
    // something is the most memorable way there is of being introduced, so it
    // goes in the logbook the same as a kill would.
    if (m_haveWound && m_lastWound.creature)
    {
        const std::size_t index = static_cast<std::size_t>(*m_lastWound.creature);

        if (m_save.killed.size() <= index)
            m_save.killed.resize(EnemyTypeCount, 0);

        m_save.killed[index] = std::max(1, m_save.killed[index]);
    }

    m_lostSalvage = m_diveSalvage;
    m_diveSalvage = 0;

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

        case MainMenu::Choice::Bestiary:
            m_beastRow = 0;
            m_state = State::Bestiary;
            break;

        case MainMenu::Choice::Options:
            m_optionRow = 0;
            m_state = State::Options;

            // Onto the first thing that can actually be changed: row zero is a
            // heading, and a list that opens with nothing selected reads as
            // a list that cannot be used.
            m_optionRow = firstOptionRow();
            m_optionScroll = 0;
            m_rebinding = -1;
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

    // And how much sub is being driven, which the water is about to be told
    // to keep up with.
    const int total = std::max(1, m_tree.getTotalCost());
    const float share = static_cast<float>(m_tree.getSpent()) / static_cast<float>(total);
    m_level.setPower(std::clamp(share / config::PowerFullShare, 0.0f, 1.0f));

    // Pacing. Vsync and the cap are two answers to the same question and the
    // window will happily do both, which is how a machine ends up waiting
    // twice for one frame - so it is one or the other.
    m_window.setVerticalSyncEnabled(m_save.vsync);
    m_frameBudget = (m_save.vsync || m_save.frameCap <= 0)
                        ? 0.0f
                        : 1.0f / static_cast<float>(m_save.frameCap);
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

void Game::stepSelectedLevel(int delta)
{
    // The dive strip is the other half of the hub, and reaching for the mouse
    // to change one number is the sort of thing that wears a hub out.
    if (m_state != State::Upgrading)
        return;

    const int wanted = std::clamp(m_save.selectedLevel + delta, 1, m_save.unlockedLevel);
    if (wanted == m_save.selectedLevel)
        return;

    m_save.selectedLevel = wanted;
    m_upgradeScreen.setLevels(m_save.selectedLevel, m_save.unlockedLevel);
    m_audio.play(Sfx::Impact, 0.25f);
    persist();
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
                queue(Command::Torpedo);
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

            // The logbook is the one flat screen with something to move
            // through, so it takes the arrows before the blanket exit below.
            if (m_state == State::Bestiary)
            {
                switch (keyPressed->scancode)
                {
                    case sf::Keyboard::Scancode::Up:
                    case sf::Keyboard::Scancode::W:
                        m_beastRow = (m_beastRow + static_cast<int>(EnemyTypeCount) - 1)
                                     % static_cast<int>(EnemyTypeCount);
                        break;
                    case sf::Keyboard::Scancode::Down:
                    case sf::Keyboard::Scancode::S:
                        m_beastRow = (m_beastRow + 1) % static_cast<int>(EnemyTypeCount);
                        break;
                    case sf::Keyboard::Scancode::F11:
                        toggleFullscreen();
                        break;
                    default:
                        openMenu();
                        break;
                }
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

            // The bound actions before the fixed ones, so that rebinding a key
            // reaches everything the player can actually press during a dive.
            // Escape always backs out whatever pause happens to be bound to:
            // a player who has moved pause onto some other key has not asked
            // to be trapped in a menu.
            const sf::Keyboard::Scancode pressed = keyPressed->scancode;

            if (pressed == keyFor(Action::Pause) || pressed == sf::Keyboard::Scancode::Escape
                || pressed == sf::Keyboard::Scancode::P)
            {
                // Backs out one layer at a time. A fight in progress pauses
                // rather than being abandoned: walking out of a dive has to
                // be a deliberate second keypress, not a mistyped escape.
                if (m_showUpgrades)
                    m_showUpgrades = false;
                else if (m_state == State::Fighting || m_state == State::LevelIntro)
                    m_paused = !m_paused;
                else
                    openMenu();

                continue;
            }

            if (pressed == keyFor(Action::Surface))
            {
                // Surfacing on purpose. It is the only way to keep what a dive
                // has earned without finishing it, which is what turns a bad
                // run into a decision instead of a write-off.
                if (m_paused)
                {
                    bankSalvage();
                    m_paused = false;
                    m_state = State::Upgrading;
                    m_showUpgrades = false;
                    m_upgradeScreen.clearCursor();
                    m_upgradeScreen.recentre();
                    m_save.selectedLevel = m_level.getNumber();
                    persist();
                }

                continue;
            }

            if (pressed == keyFor(Action::Lamp))
            {
                // Kill the lamp. Nearly blind, but so is everything else.
                if (m_state == State::Fighting && !m_showUpgrades && !m_paused)
                    queue(Command::Lamp);

                continue;
            }

            if (pressed == keyFor(Action::FireMode))
            {
                // Round the fitted guns. Nothing happens when only the jet is
                // fitted, which is most of the campaign.
                if (m_state == State::Fighting && !m_showUpgrades && !m_paused)
                    queue(Command::FireMode);

                continue;
            }

            if (pressed == keyFor(Action::Dash) || pressed == sf::Keyboard::Scancode::RShift)
            {
                if (m_state == State::Fighting && !m_showUpgrades)
                    queue(Command::Dash);

                continue;
            }

            if (pressed == keyFor(Action::Upgrades))
            {
                // Between levels the tree is the screen, so there is nothing to
                // toggle; after a death there is nothing left to spend.
                if (m_state != State::Upgrading && m_state != State::GameOver)
                {
                    m_showUpgrades = !m_showUpgrades;
                    m_upgradeScreen.clearCursor();
                    m_upgradeScreen.recentre();
                }

                continue;
            }

            switch (keyPressed->scancode)
            {
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

                case sf::Keyboard::Scancode::Left:
                    if (upgradesVisible())
                        m_upgradeScreen.moveFocus({ -1.0f, 0.0f }, m_tree);
                    break;

                case sf::Keyboard::Scancode::Right:
                    if (upgradesVisible())
                        m_upgradeScreen.moveFocus({ 1.0f, 0.0f }, m_tree);
                    break;

                case sf::Keyboard::Scancode::Up:
                    if (upgradesVisible())
                        m_upgradeScreen.moveFocus({ 0.0f, -1.0f }, m_tree);
                    break;

                case sf::Keyboard::Scancode::Down:
                    if (upgradesVisible())
                        m_upgradeScreen.moveFocus({ 0.0f, 1.0f }, m_tree);
                    break;

                case sf::Keyboard::Scancode::Comma:
                    stepSelectedLevel(-1);
                    break;

                case sf::Keyboard::Scancode::Period:
                    stepSelectedLevel(1);
                    break;

                case sf::Keyboard::Scancode::Space:
                    // Space buys whatever the arrow keys have walked to, so the
                    // whole tree can be worked without ever finding the mouse.
                    if (upgradesVisible() && m_upgradeScreen.getHovered())
                    {
                        handleUpgradeClick();
                        break;
                    }
                    [[fallthrough]];

                case sf::Keyboard::Scancode::Enter:
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

    // A cracked gun still fires. It just does not hurt anything much.
    if (m_player.isBroken(Player::Hull::Gun))
    {
        damage = std::max(1, static_cast<int>(std::lround(
                                 static_cast<float>(damage) * config::BrokenGunDamage)));
    }

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

    if (!held)
        return;

    // Each mode keeps its own rhythm. The jet goes through the player's fire
    // timer because everything in the tree tunes it; the other two have their
    // own clocks, because a harpoon on a jet's cadence would be absurd.
    switch (modeAt(m_fireSlot))
    {
        case FireMode::Harpoon:
            if (m_modeTimer <= 0.0f)
            {
                m_modeTimer = config::HarpoonCooldown;
                fireHarpoon();
            }
            return;

        case FireMode::Pulse:
            if (m_modeTimer <= 0.0f)
            {
                m_modeTimer = config::PulseCooldown;
                firePulse();
            }
            return;

        default:
            break;
    }

    if (!m_player.tryFire())
        return;

    fireJet(m_player.getMuzzlePosition(), m_player.getAimDirection(), 1.0f);
}

// ---------------------------------------------------------------------------
//  Fire modes
//  The jet, and two guns that are not the jet with different numbers. Cycled
//  in the water rather than chosen in the hub, because which one is right
//  changes between one room and the next.
// ---------------------------------------------------------------------------

int Game::modeCount() const
{
    int count = 1; // the jet is always fitted
    if (m_playerStats.has(Trait::Harpoon))
        ++count;
    if (m_playerStats.has(Trait::Pulse))
        ++count;
    return count;
}

Game::FireMode Game::modeAt(int slot) const
{
    // Walked rather than indexed, so the slots stay in a fixed order however
    // many of them are actually fitted.
    int at = 0;
    if (at++ == slot)
        return FireMode::Jet;
    if (m_playerStats.has(Trait::Harpoon) && at++ == slot)
        return FireMode::Harpoon;
    if (m_playerStats.has(Trait::Pulse) && at++ == slot)
        return FireMode::Pulse;

    return FireMode::Jet;
}

void Game::cycleFireMode()
{
    const int count = modeCount();
    if (count <= 1)
        return;

    m_fireSlot = (m_fireSlot + 1) % count;
    m_audio.play(Sfx::Impact, 1.6f);
}

const char* Game::modeName(FireMode mode)
{
    switch (mode)
    {
        case FireMode::Harpoon: return "HARPOON";
        case FireMode::Pulse:   return "PULSE";
        default:                return "JET";
    }
}

void Game::fireHarpoon()
{
    // One line at a time. A second head in the water while the first is still
    // dragging you would be two anchors pulling opposite ways.
    if (m_harpoon || m_harpoonPull > 0.0f)
        return;

    const sf::Vector2f muzzle = m_player.getMuzzlePosition();
    const sf::Vector2f direction = m_player.getAimDirection();

    const config::ProjectileStats head{ config::HarpoonSpeed, 7.0f, config::WaterJet.scale * 1.4f };

    int damage = config::HarpoonDamage
                 + std::max(0, m_playerStats.getInt(Stat::ProjectileDamage,
                                                    config::WaterJetDamage) / 2);

    if (m_player.isBroken(Player::Hull::Gun))
    {
        damage = std::max(1, static_cast<int>(std::lround(
                                 static_cast<float>(damage) * config::BrokenGunDamage)));
    }

    m_harpoon.emplace(m_assets.torpedo, muzzle, direction, head, damage, 0);
    m_harpoonFrom = muzzle;

    effects::muzzleBurst(m_effects, muzzle, direction);
    m_audio.play(Sfx::JetFire, 0.7f);
    m_noiseTimer = config::NoiseDuration;
    ++m_tally.shotsFired;
}

void Game::firePulse()
{
    const sf::Vector2f at = m_player.getPosition();

    int damage = config::PulseDamage
                 + std::max(0, m_playerStats.getInt(Stat::ProjectileDamage,
                                                    config::WaterJetDamage) / 3);

    if (m_player.isBroken(Player::Hull::Gun))
    {
        damage = std::max(1, static_cast<int>(std::lround(
                                 static_cast<float>(damage) * config::BrokenGunDamage)));
    }

    areaDamage(at, config::PulseRadius, damage);

    // The cold is the point. A pulse that only hurt would be a worse torpedo;
    // one that leaves everything it touched slow is how you get out of a room
    // that has closed around you.
    for (Enemy& enemy : m_enemies)
    {
        if (!enemy.isAlive() || !enemy.isVulnerable())
            continue;

        if ((enemy.getPosition() - at).length() <= config::PulseRadius + enemy.getRadius())
            enemy.applyChill(config::PulseChill, config::PulseChillTime);
    }

    // Hostile water goes with it, which is the other half of why it is the
    // answer to being surrounded.
    for (Projectile& shot : m_enemyShots)
    {
        if (shot.isAlive() && (shot.getPosition() - at).length() <= config::PulseRadius)
            shot.kill();
    }

    effects::ventBurst(m_effects, at, config::PulseRadius * 0.6f);
    addHitRing(at, config::PulseRadius, config::PulseColor);
    addShake(config::ShakeTorpedo * 0.7f);
    m_audio.play(Sfx::Vent, 1.25f);

    m_noiseTimer = config::NoiseDuration;
    ++m_tally.shotsFired;
}

void Game::updateHarpoon(float deltaTime)
{
    // Being dragged. Steering is ignored for the third of a second it lasts,
    // which is exactly what makes it worth firing across a room.
    if (m_harpoonPull > 0.0f)
    {
        m_harpoonPull -= deltaTime;

        const sf::Vector2f offset = m_harpoonTo - m_player.getPosition();
        const float distance = offset.length();

        if (distance <= 26.0f)
        {
            m_harpoonPull = 0.0f;
        }
        else
        {
            const sf::Vector2f step = offset / distance
                                      * std::min(distance, config::HarpoonPullSpeed * deltaTime);

            m_player.setPosition(m_arena.clampInside(m_player.getPosition() + step,
                                                     config::PlayerRadius));
            emitThrusterTrail(deltaTime);
        }
    }

    if (!m_harpoon)
        return;

    m_harpoon->update(deltaTime);

    const sf::Vector2f at = m_harpoon->getPosition();
    const bool tooFar = (at - m_harpoonFrom).length() > config::HarpoonReach;

    // Rock stops it and it still pulls: the wall is a handhold, which is the
    // whole reason the line is a movement tool and not only a weapon.
    if (!m_harpoon->isAlive() || tooFar || !m_arena.isOpen(at, 0.0f))
    {
        if (!tooFar && m_harpoon->isAlive())
        {
            m_harpoonTo = m_arena.clampInside(at, config::PlayerRadius + 4.0f);
            m_harpoonPull = config::HarpoonPullTime;
            m_audio.play(Sfx::Impact, 1.3f);
            addHitRing(at, 40.0f, config::HarpoonLineColor);
        }

        m_harpoon.reset();
        return;
    }

    for (Enemy& enemy : m_enemies)
    {
        if (!enemy.isAlive() || !enemy.isVulnerable())
            continue;

        const float reach = m_harpoon->getRadius() + enemy.getRadius();
        if ((enemy.getPosition() - at).lengthSquared() > reach * reach)
            continue;

        if (enemy.deflects(at))
        {
            effects::impact(m_effects, at, -m_harpoon->getDirection());
            m_audio.play(Sfx::Impact, 0.6f);
            m_harpoon.reset();
            return;
        }

        enemy.takeDamage(m_harpoon->getDamage());
        effects::impact(m_effects, at, m_harpoon->getDirection());
        m_audio.playAt(hitVoice(enemy.getType()), enemy.getPosition(), 1.2f,
                       occlusionAt(enemy.getPosition()));
        addHitRing(at, config::JetRingRadius, config::HarpoonLineColor);
        ++m_tally.shotsHit;

        if (!enemy.isAlive())
            killEnemy(enemy);

        // It bites into whatever it hit and hauls you to it, alive or not.
        m_harpoonTo = enemy.getPosition();
        m_harpoonPull = config::HarpoonPullTime;

        m_harpoon.reset();
        return;
    }
}

void Game::drawHarpoon()
{
    if (!m_harpoon && m_harpoonPull <= 0.0f)
        return;

    // The line, drawn as a thin quad from the muzzle to wherever the head is,
    // because a harpoon without a visible line is a slow bullet.
    const sf::Vector2f from = m_player.getMuzzlePosition();
    const sf::Vector2f to = m_harpoon ? m_harpoon->getPosition() : m_harpoonTo;

    const sf::Vector2f span = to - from;
    if (span.lengthSquared() <= 1.0f)
        return;

    const sf::Vector2f across = sf::Vector2f{ -span.y, span.x }.normalized() * 1.6f;

    sf::VertexArray line(sf::PrimitiveType::Triangles);
    line.append(sf::Vertex{ from - across, config::HarpoonLineColor });
    line.append(sf::Vertex{ from + across, config::HarpoonLineColor });
    line.append(sf::Vertex{ to + across, config::HarpoonLineColor });
    line.append(sf::Vertex{ from - across, config::HarpoonLineColor });
    line.append(sf::Vertex{ to + across, config::HarpoonLineColor });
    line.append(sf::Vertex{ to - across, config::HarpoonLineColor });
    m_window.draw(line);

    if (m_harpoon)
        m_harpoon->draw(m_window);
}

void Game::fireTorpedo()
{
    // A holed tube is a holed tube. Nothing to fire it with, and nothing on
    // the screen pretending otherwise.
    if (m_player.isBroken(Player::Hull::Tube))
        return;

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
    if (!m_player.tryDash(m_input.move))
        return;

    effects::thrusterBubbles(m_effects, m_player.getPosition(), -m_player.getAimDirection());
    m_audio.play(Sfx::Impact, 0.6f);

    // And the one thing that gets you out of a mouth. Same key, same reason.
    if (m_grabTimer > 0.0f)
    {
        m_grabTimer = 0.0f;
        addFlash(config::BlockDodgeColor, 0.5f);
        addHitRing(m_player.getPosition(), config::GrabReach, config::BlockDodgeColor);
        m_audio.play(Sfx::PartBreak, 1.3f);
    }

    // The one thing that gets a siphon off. It is deliberately the dash rather
    // than a button of its own: you have to spend the escape you were saving.
    for (Enemy& enemy : m_enemies)
    {
        if (!enemy.isAlive() || !enemy.isLatched())
            continue;

        const sf::Vector2f away = -m_player.getAimDirection();
        enemy.shakeOff(away);
        effects::impact(m_effects, m_player.getPosition(), away);
        addHitRing(m_player.getPosition(), config::PlayerRadius * 3.0f,
                   config::BlockDodgeColor);
    }

    // Pressure Burst turns the dash itself into a weapon.
    if (m_playerStats.has(Trait::PressureBurst))
    {
        areaDamage(m_player.getPosition(), config::PressureBurstRadius, config::PressureBurstDamage);
        effects::impact(m_effects, m_player.getPosition(), m_player.getAimDirection());
    }
}

void Game::spawnSplit(sf::Vector2f at, sf::Vector2f incoming, int fromEnemyId)
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

        // The fragments are standing inside the creature they came out of.
        // Without this every one of them lands on it again on the next frame,
        // which turned splitshot into an execute rather than into spray.
        m_pendingPlayerShots.back().setIgnore(fromEnemyId);
    }
}

Sfx Game::deathVoice(EnemyType type)
{
    switch (config::voiceOf(type))
    {
        case config::CreatureVoice::Shell:   return Sfx::DieShell;
        case config::CreatureVoice::Bladder: return Sfx::DieBladder;
        case config::CreatureVoice::Wet:     return Sfx::DieWet;
        case config::CreatureVoice::Void:    return Sfx::DieVoid;
        default:                             return Sfx::DieSoft;
    }
}

Sfx Game::hitVoice(EnemyType type)
{
    // Three, not five: a hit is over in a tenth of a second, and the ear only
    // has time to ask whether it landed on meat, on shell, or on something
    // that should not be down here.
    switch (config::voiceOf(type))
    {
        case config::CreatureVoice::Shell: return Sfx::HitShell;
        case config::CreatureVoice::Void:  return Sfx::HitVoid;
        default:                           return Sfx::HitSoft;
    }
}

void Game::bankSalvage()
{
    // Surfacing is what makes it yours. Called when a level closes and when
    // the player walks out of one on purpose, and never when they drown.
    if (m_diveSalvage <= 0)
        return;

    m_salvage += m_diveSalvage;
    m_diveSalvage = 0;
}

void Game::awardSalvage(int base)
{
    if (base <= 0)
        return;

    const float bonus = 1.0f + m_playerStats.get(Stat::SalvageBonus, 0.0f);
    const float factor = m_rerun ? config::RerunSalvageFactor : 1.0f;

    // Momentum pays. A clean run is worth going for rather than merely being
    // worth hearing, and the first thing that lands on you takes the whole
    // multiplier back rather than shaving it.
    const float run = 1.0f + config::StreakSalvageStep
                                 * static_cast<float>(std::min(m_streak,
                                                               config::StreakSalvageCap));

    const int paid = std::max(1, static_cast<int>(std::lround(
                                     static_cast<float>(base) * bonus * factor * run
                                     * difficulty().salvageScale)));

    // Into the hold, not into the bank. Nothing earned on a dive is yours
    // until you surface with it, which is the whole of what dying costs now.
    m_diveSalvage += paid;
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

    // Ordered by how far each chamber is from where the dive starts, so the
    // nests go in the far rooms rather than the first one. A hunt should begin
    // by leaving, not by turning round.
    std::vector<sf::Vector2f> spots;
    for (const Arena::Chamber& chamber : m_arena.getChambers())
        spots.push_back(chamber.centre);

    const sf::Vector2f start = m_arena.getStart();
    std::sort(spots.begin(), spots.end(), [start](sf::Vector2f a, sf::Vector2f b) {
        return (a - start).lengthSquared() > (b - start).lengthSquared();
    });

    for (int i = 0; i < wanted; ++i)
    {
        // One per chamber, furthest first, wrapping if the map is small. Each
        // one sits out against that chamber's wall where it is cemented to
        // rock rather than floating in the middle of the water.
        const sf::Vector2f room = spots.empty()
                                      ? WorldCentre
                                      : spots[static_cast<std::size_t>(i) % spots.size()];

        const sf::Vector2f at = m_arena.clampInside(
            room + sf::Vector2f(rng::range(40.0f, 150.0f),
                                sf::degrees(rng::range(0.0f, 360.0f))),
            config::NestRadius);

        Nest nest{ sf::Sprite(m_assets.boss(config::NestTexturePath)),
                   at,
                   config::NestHealth, config::NestHealth,
                   rng::range(1.0f, config::NestHatchInterval), 0.0f, true };

        nest.sprite.setOrigin(sf::Vector2f(nest.sprite.getTexture().getSize()) / 2.0f);
        nest.sprite.setPosition(nest.position);
        nest.sprite.setRotation(sf::degrees(rng::range(0.0f, 360.0f)));

        m_nests.push_back(std::move(nest));
    }
}

void Game::placeCargo()
{
    m_cargo.clear();
    m_carrying = -1;

    const int wanted = m_level.getCargoCount();
    if (wanted <= 0)
        return;

    // Furthest rooms first, same as the nests: a haul that starts next to the
    // mouth is a walk, and the whole point of the level is the distance.
    std::vector<sf::Vector2f> spots;
    for (const Arena::Chamber& chamber : m_arena.getChambers())
        spots.push_back(chamber.centre);

    const sf::Vector2f start = m_arena.getStart();
    std::sort(spots.begin(), spots.end(), [start](sf::Vector2f a, sf::Vector2f b) {
        return (a - start).lengthSquared() > (b - start).lengthSquared();
    });

    for (int i = 0; i < wanted; ++i)
    {
        const sf::Vector2f room = spots.empty()
                                      ? WorldCentre
                                      : spots[static_cast<std::size_t>(i) % spots.size()];

        const sf::Vector2f at = m_arena.clampInside(
            room + sf::Vector2f(rng::range(0.0f, config::CargoDropInset),
                                sf::degrees(rng::range(0.0f, 360.0f))),
            config::CargoRadius);

        Cargo pod{ sf::Sprite(m_assets.boss(config::CargoTexturePath)), at, false, false,
                   rng::range(0.0f, 6.28f) };

        pod.sprite.setOrigin(sf::Vector2f(pod.sprite.getTexture().getSize()) / 2.0f);
        pod.sprite.setScale({ config::CargoScale, config::CargoScale });
        pod.sprite.setPosition(pod.position);

        m_cargo.push_back(std::move(pod));
    }
}

void Game::dropCargo()
{
    // Let go where you are, not where you were going. A pod dropped halfway
    // home is the whole cost of being hit on a haul, and it has to be visibly
    // that: it stays exactly where it fell.
    if (m_carrying < 0)
        return;

    Cargo& pod = m_cargo[static_cast<std::size_t>(m_carrying)];
    pod.carried = false;
    pod.position = m_arena.clampInside(m_player.getPosition(), config::CargoRadius);
    pod.sprite.setPosition(pod.position);

    m_carrying = -1;
    m_player.setBurden(1.0f);

    m_audio.play(Sfx::PartBreak, 0.7f);
    addHitRing(pod.position, config::CargoRadius * 2.2f, config::CargoMarkColor);
}

void Game::updateCargo(float deltaTime)
{
    if (m_cargo.empty())
        return;

    const sf::Vector2f me = m_player.getPosition();
    const sf::Vector2f home = m_arena.getStart();

    for (std::size_t i = 0; i < m_cargo.size(); ++i)
    {
        Cargo& pod = m_cargo[i];
        if (pod.delivered)
            continue;

        pod.bob += deltaTime * 2.2f;

        if (pod.carried)
        {
            // Towed behind the stern rather than carried inside the hull, so
            // the thing you are risking everything for is on the screen the
            // whole way back.
            const sf::Vector2f behind = m_player.getSternPosition();
            pod.position += (behind - pod.position)
                            * std::min(1.0f, config::CargoBob * deltaTime * 4.0f);
            pod.position = m_arena.clampInside(pod.position, config::CargoRadius);
            pod.sprite.setPosition(pod.position);
            pod.sprite.setRotation(sf::degrees(std::sin(pod.bob) * 9.0f));

            if ((pod.position - home).length() <= config::CargoDropRange)
            {
                pod.delivered = true;
                pod.carried = false;
                m_carrying = -1;
                m_player.setBurden(1.0f);

                m_level.notifyCargoDelivered();
                awardSalvage(static_cast<int>(config::CargoSalvage));

                bigMoment(home, config::CargoHomeColor, config::ShakeTorpedo * 0.7f, 0.0f);
                m_audio.play(Sfx::LevelClear, 0.8f);
            }

            continue;
        }

        pod.sprite.setPosition(pod.position);
        pod.sprite.setRotation(sf::degrees(std::sin(pod.bob * 0.4f) * 3.0f));

        // Picked up by swimming into it, and only one at a time: a sub towing
        // four drums is not a decision, it is an errand.
        if (m_carrying < 0 && m_player.isAlive()
            && (pod.position - me).length() <= config::CargoPickupRange)
        {
            pod.carried = true;
            m_carrying = static_cast<int>(i);
            m_player.setBurden(config::CargoBurden);

            m_audio.play(Sfx::Hatch, 0.8f);
            addHitRing(pod.position, config::CargoRadius * 2.6f, config::CargoHomeColor);
        }
    }
}

void Game::drawCargo()
{
    if (m_cargo.empty())
        return;

    // The mouth, marked, and only while there is something to bring back to it.
    // A ring painted on the floor for the whole level would stop meaning
    // anything by the second minute.
    if (m_carrying >= 0)
    {
        const sf::Vector2f home = m_arena.getStart();
        const float pulse = 0.86f + 0.14f * std::sin(m_stateTimer * 3.4f);
        const float radius = config::CargoDropRange * pulse;

        sf::Color edge = config::CargoHomeColor;
        edge.a = 130;

        m_sonarRing.setRadius(radius);
        m_sonarRing.setOrigin({ radius, radius });
        m_sonarRing.setPosition(home);
        m_sonarRing.setOutlineThickness(3.0f);
        m_sonarRing.setOutlineColor(edge);
        m_window.draw(m_sonarRing);
    }

    for (const Cargo& pod : m_cargo)
    {
        if (pod.delivered)
            continue;

        m_window.draw(pod.sprite);
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
        m_audio.playAt(Sfx::Hatch, nest.position, 0.7f, occlusionAt(nest.position));
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

float Game::occlusionAt(sf::Vector2f where) const
{
    const sf::Vector2f ear = m_player.getPosition();
    const sf::Vector2f span = where - ear;

    if (span.lengthSquared() < 1.0f)
        return 0.0f;

    // Three rays, spread across the line rather than along it. One ray is a
    // yes or a no, and a creature dying in a doorway would flip between the
    // two as it drifted; three give a fraction, so a mouth is half muffled
    // and a wall is all of it.
    const sf::Vector2f across = sf::Vector2f{ -span.y, span.x }.normalized()
                               * config::AudioOcclusionSpread;

    int blocked = 0;
    for (int i = -1; i <= 1; ++i)
    {
        const sf::Vector2f offset = across * static_cast<float>(i);
        if (!m_arena.clearLine(ear + offset, where + offset, 0.0f))
            ++blocked;
    }

    return static_cast<float>(blocked) / 3.0f;
}

float Game::opennessAt(sf::Vector2f where) const
{
    const int room = m_arena.roomAt(where);
    if (room < 0)
        return 0.0f;

    const float radius = m_arena.getRooms()[static_cast<std::size_t>(room)].radius;

    return std::clamp((radius - config::EchoRoomFloor)
                          / (config::EchoHallRadius - config::EchoRoomFloor),
                      0.0f, 1.0f);
}

void Game::refreshLamp()
{
    // Everything that dims the lamp, in one place. It used to be worked out
    // at each of the four call sites, which is how a broken lamp could have
    // been fixed in three of them and still be bright in the fourth.
    float factor = m_lampOn ? 1.0f : config::LampDarkFactor;

    if (m_siltTimer > 0.0f)
        factor *= config::SiltLamp;

    if (m_player.isBroken(Player::Hull::Lamp))
        factor *= config::BrokenLampFactor;

    m_arena.setLamp(m_playerStats.get(Stat::LampRadius, 0.0f), factor);
}

void Game::toggleLamp()
{
    m_lampOn = !m_lampOn;

    refreshLamp();


    m_audio.play(Sfx::PartBreak, 0.35f);
}

float Game::noticeRange() const
{
    // Firing gives you away whether the lamp is on or not, which is what stops
    // running dark from being a way to shoot everything for free.
    // A pod under tow is a beacon strapped to the stern. Running dark with one
    // is still quieter than running lit, but it is nothing like running dark
    // empty-handed - which is the trade the level is about.
    const float towing = m_carrying >= 0 ? config::CargoNoticeBonus : 1.0f;

    // And a herald standing over you is a lamp you do not own. Running dark
    // simply stops working until it is dead, which is the whole reason it is
    // in the water.
    const float heraldSquared = config::HeraldRadius * config::HeraldRadius;
    for (const Enemy& enemy : m_enemies)
    {
        if (enemy.isAlive() && enemy.getType() == EnemyType::Herald
            && (enemy.getPosition() - m_player.getPosition()).lengthSquared() <= heraldSquared)
        {
            return config::NoticeLit * towing;
        }
    }

    if (m_lampOn || m_noiseTimer > 0.0f)
        return config::NoticeLit * towing;

    return config::NoticeDark * towing;
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
        // Scattered through the water wherever it happens to be. A vent in a
        // corridor is worth far more than one in a hall, because there is
        // nowhere to stand while it blows.
        Vent vent;
        vent.radius = rng::range(config::VentRadiusMin, config::VentRadiusMax);
        vent.position = m_arena.clampInside(m_arena.randomPointInside(), vent.radius * 0.4f);
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
        m_audio.playAt(Sfx::Vent, vent.position, 1.0f, occlusionAt(vent.position));
        addShake(config::ShakeTorpedo * 0.7f);

        effects::ventBurst(m_effects, vent.position, vent.radius);

        // The sub and the creatures both, because a hazard that only hurts the
        // player is a tax, while one that hurts both is a tool. Bosses are the
        // exception: areaDamage does not reach them, and letting the floor cook
        // a depth's creature would take the fight away from the player.
        if ((m_player.getPosition() - vent.position).length() <= vent.radius)
            hurtPlayer(config::VentDamage, { vent.position, std::nullopt, false, true });

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
    // Blind first. Losing the sonar costs you the least in a straight fight
    // and the most in a basin you have to cross, which is why it is the one
    // that goes first.
    if (m_player.isBroken(Player::Hull::Sonar))
        return;

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

        // A ping is also a survey. It is the only thing that puts water on
        // the map that the sub has not personally swum through, which is
        // most of what the sonar is worth once the basin is a maze.
        revealMap(m_player.getPosition(),
                  m_playerStats.get(Stat::SonarRange, config::SonarBaseRange));
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

    // Off screen is not enough any more: off screen can be inside a wall. The
    // candidate is pulled into the nearest water, and creatures swim in from
    // there the way they always did.
    return m_arena.clampInside(best, 24.0f);
}

void Game::spawnEnemy(EnemyType type)
{
    m_enemies.emplace_back(type, m_assets.forEnemy(type), pickSpawnPosition(),
                           m_level.getScaling());

    // The worm tows a body, and the body is a different sprite. Handed over
    // here rather than looked up inside Enemy, so the creature never has to
    // know where the assets live.
    if (config::enemyStats(type).behaviour == EnemyBehaviour::Serpent)
        m_enemies.back().setSegmentTexture(m_assets.boss(config::SerpentSegmentPath));
}

void Game::spawnStalker()
{
    // Not on the level's list, so it does not count against what the level is
    // still owed and it does not take up a slot in the roster. It is simply
    // already here, and it has been since you left it.
    const int dives = std::min(m_save.stalkerLevels, config::StalkerMaxLevels);

    EnemyScaling scaling = m_level.getScaling();
    scaling.health *= 1.0f + config::StalkerHealthPerLevel * static_cast<float>(dives);
    scaling.speed *= 1.0f + config::StalkerSpeedPerLevel * static_cast<float>(dives);
    scaling.bonusDamage += config::StalkerDamagePerLevel * dives;

    m_enemies.emplace_back(EnemyType::Stalker, m_assets.forEnemy(EnemyType::Stalker),
                           pickSpawnPosition(), scaling);
    m_enemies.back().setSegmentTexture(m_assets.boss(config::SerpentSegmentPath));
    m_enemies.back().markUnscripted();
}

void Game::updateProjectiles(float deltaTime)
{
    const bool bounces = m_playerStats.has(Trait::Ricochet);

    // Shots stop at rock now. That is the whole difference between a boundary
    // and terrain: a corner is cover, a corridor is a shooting gallery, and
    // firing across a hall no longer reaches whatever is behind the wall.
    for (Projectile& shot : m_playerShots)
    {
        shot.update(deltaTime);

        if (!shot.isAlive() || m_arena.isOpen(shot.getPosition(), 0.0f))
            continue;

        const sf::Vector2f normal = m_arena.wallNormal(shot.getPosition(), 0.0f);

        if (bounces && normal.lengthSquared() > 0.0f && shot.spendBounce(normal))
        {
            addHitRing(shot.getPosition(), 22.0f, config::HudDimTextColor);
            continue;
        }

        effects::impact(m_effects, shot.getPosition(), shot.getDirection());
        shot.kill();
    }

    for (Projectile& shot : m_enemyShots)
    {
        shot.update(deltaTime);

        // Hostile shots stop at rock too, which is what makes a corner worth
        // standing behind rather than only worth looking at.
        if (shot.isAlive() && !m_arena.isOpen(shot.getPosition(), 0.0f))
        {
            effects::impact(m_effects, shot.getPosition(), shot.getDirection());
            shot.kill();
        }
    }
}

void Game::updateTorpedoes(float deltaTime)
{
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

        // A torpedo that hits rock detonates there rather than vanishing. It
        // is the one thing the player has that clears a corridor.
        if (torpedo.isAlive() && !m_arena.isOpen(torpedo.getPosition(), 0.0f))
        {
            const float radius = config::TorpedoBlastRadius
                                 + m_playerStats.get(Stat::TorpedoBlast, 0.0f);

            areaDamage(torpedo.getPosition(), radius,
                       std::max(1, static_cast<int>(std::lround(
                                        static_cast<float>(torpedo.getDamage())
                                        * config::TorpedoSplashFactor))));

            addHitRing(torpedo.getPosition(), radius, config::HudBossColor);
            effects::ventBurst(m_effects, torpedo.getPosition(), radius * 0.5f);
            m_audio.play(Sfx::Impact, 1.3f);
            addShake(config::ShakeTorpedo * 0.7f);
            torpedo.kill();
        }
    }
}

void Game::updateEnemies(float deltaTime)
{
    // Who can currently see the sub. One radius for the whole basin rather than
    // per-creature senses: the player needs a rule they can hold in their head,
    // not a simulation they have to reverse engineer.
    // The flow field is swept from the sub, so everything that has lost its
    // line can share one sweep instead of each doing its own arithmetic.
    m_navTimer -= deltaTime;
    if (m_navTimer <= 0.0f)
    {
        m_navTimer = config::NavInterval;
        m_arena.updateFlow(m_player.getPosition());
    }

    const float notice = noticeRange();
    const float noticeSquared = notice * notice;

    for (Enemy& enemy : m_enemies)
    {
        const bool close = (enemy.getPosition() - m_player.getPosition()).lengthSquared()
                           < noticeSquared;
        enemy.setAware(close);
    }

    // A herald is worth killing first, and this is why: everything alive
    // inside its reach gets harder to kill and quicker to arrive. Done in its
    // own pass because a creature has to be bolstered before it moves, not
    // after it has already been walked over.
    const float heraldSquared = config::HeraldRadius * config::HeraldRadius;

    for (const Enemy& herald : m_enemies)
    {
        if (!herald.isAlive() || herald.getType() != EnemyType::Herald)
            continue;

        for (Enemy& other : m_enemies)
        {
            if (!other.isAlive() || other.getId() == herald.getId())
                continue;

            if ((other.getPosition() - herald.getPosition()).lengthSquared() <= heraldSquared)
                other.bolster(0.4f);
        }
    }

    // Anything hanging off the hull rides it, and takes a mouthful on its own
    // clock. A dash is what gets it off, which is handled where the dash is.
    for (Enemy& enemy : m_enemies)
    {
        if (!enemy.isAlive() || !enemy.isLatched())
            continue;

        enemy.latchTo(m_player.getPosition());

        if (enemy.wantsBite(deltaTime))
        {
            // It is attached. Shoving the sub would only take it along.
            hurtPlayer(config::LeechDrain + m_level.getScaling().bonusDamage,
                       { enemy.getPosition(), enemy.getType(), false, false });
            addHitRing(m_player.getPosition(), config::PlayerRadius * 1.8f,
                       config::HudBossColor);
        }
    }


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

        enemy.update(deltaTime, m_player.getPosition(), m_player.getVelocity(), m_arena);
        enemy.confine(m_arena, deltaTime);

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

    const bool wasTelling = m_bossTelling;

    m_boss->update(deltaTime, m_player.getPosition());

    // The announcement gets a sound of its own. A tell nobody hears is a tell
    // that only works when the player happens to be looking at the creature,
    // and at this size they are usually looking at the water around it.
    m_bossTelling = m_boss->isTelling();
    if (m_bossTelling && !wasTelling)
    {
        m_audio.play(Sfx::Charge, m_boss->isTellLunge() ? 1.25f : 0.95f);
        addShake(config::ShakeTorpedo * 0.35f);
    }

    // A boss is held in the hall rather than in the whole basin: it is far too
    // big to be allowed down a corridor, and the hall is cut for it.
    m_boss->confine(m_arena.getHallCentre(),
                    std::max(240.0f, m_arena.getHallRadius() - m_boss->getRadius() * 0.5f));
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
    for (const BossMove& move : m_boss->takeMoves())
        applyBossMove(move);

    updateBossMoves(deltaTime);

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

    // The logbook. Counted here rather than in the tally, because it outlives
    // the run: it is a record of what has been met, not of what this dive did.
    {
        const auto index = static_cast<std::size_t>(enemy.getType());
        if (m_save.killed.size() <= index)
            m_save.killed.resize(EnemyTypeCount, 0);

        ++m_save.killed[index];
    }

    // Momentum. Every kill without being touched pitches the death note up a
    // step, so a clean run through a room audibly climbs - and the moment
    // something lands on you it drops back to the floor. Nothing about it is a
    // score; it is the sound of doing well.
    ++m_streak;
    m_bestStreak = std::max(m_bestStreak, m_streak);

    const float pitch = std::min(config::StreakPitchMax,
                                 1.0f + config::StreakPitchStep
                                            * static_cast<float>(m_streak));
    // Out in the water, so it comes from where the thing was and through
    // whatever was between the two of you.
    m_audio.playAt(deathVoice(enemy.getType()), enemy.getPosition(), pitch,
                   occlusionAt(enemy.getPosition()));

    if (m_streak == config::StreakNotable || m_streak == config::StreakLoud)
        addFlash(config::HudPipFullEdge, 0.30f);

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

void Game::hurtPlayer(int amount, const Wound& from)
{
    // Remembered before anything can absorb it, so the death screen names what
    // actually finished the dive rather than whatever happened to be nearest.
    m_lastWound = from;
    m_haveWound = true;

    if (from.shove)
    {
        const sf::Vector2f away = m_player.getPosition() - from.at;

        if (away.lengthSquared() > 0.0001f)
        {
            // Scaled by how hard it hit, so a spawnling nudges and a warden
            // throws. Capped at both ends: a shove nobody can feel is not
            // worth the code, and one that crosses the room is a boss move.
            const float weight = std::clamp(static_cast<float>(amount) * 0.45f, 0.55f, 1.8f);
            m_player.applyImpulse(away.normalized() * config::KnockbackPeak * weight,
                                  config::KnockbackTime);
        }
    }

    hurtPlayer(amount);
}

void Game::hurtPlayer(int amount)
{
    // The single door every wound comes through, which is why the difficulty
    // setting is applied here and nowhere else.
    amount = std::max(1, static_cast<int>(std::lround(static_cast<float>(amount)
                                                      * difficulty().enemyDamage)));

    const Player::DamageResult result = m_player.takeDamage(amount);

    // A hit that does not land is the best thing that happens in a fight, and
    // until now it happened silently. Each kind of save gets its own colour,
    // its own ring and a few frames of held time, so the player knows which of
    // the things they bought just paid for itself.
    if (result.shieldBroke)
    {
        m_audio.play(Sfx::PartBreak, 1.2f);
        effects::impact(m_effects, m_player.getPosition(), { 1.0f, 0.0f });

        addHitRing(m_player.getPosition(), config::BlockRingRadius, config::BlockShieldColor);
        addFlash(config::BlockShieldColor, 0.34f);
        addHitStop(config::HitStopBlock);
        addShake(config::ShakeBlock);
        ++m_blocks;

        if (m_playerStats.has(Trait::ReactiveArmour))
            areaDamage(m_player.getPosition(), config::ReactiveBurstRadius, config::ReactiveBurstDamage);
        return;
    }

    if (result.dodged)
    {
        // Threading a dash through something is the most skilful thing in the
        // game. It gets a moment of its own.
        addHitRing(m_player.getPosition(), config::BlockRingRadius * 0.8f, config::BlockDodgeColor);
        addSlowMotion(config::SlowMotionDodge, 0.5f);
        m_audio.play(Sfx::Impact, 0.5f);
        ++m_blocks;
        return;
    }

    if (!result.applied)
        return;

    if (result.soaked > 0)
    {
        // Plating did not stop the hit, but it took a bite out of it. A
        // smaller, duller ring: a save, but not a clean one.
        addHitRing(m_player.getPosition(), config::BlockRingRadius * 0.6f, config::BlockArmourColor);
        m_audio.play(Sfx::PartBreak, 0.55f);
    }

    if (result.ventSaved)
    {
        // One hit a level cannot finish you, and being told so afterwards is
        // far too late.
        bigMoment(m_player.getPosition(), config::BlockVentColor,
                  config::ShakeDeath, config::SlowMotionBossAct * 0.7f);
        m_audio.play(Sfx::Vent, 1.3f);
    }

    // Momentum breaks on a hit that actually lands. That is the whole point of
    // it: it is a record of not having been touched.
    m_streak = 0;

    // And so does the sub. Crossing each threshold knocks out the next system
    // for the rest of the dive, so the hull bar stops being a number counting
    // down and starts being a list of things that no longer work.
    {
        const float left = m_player.getMaxHealth() > 0
                               ? static_cast<float>(m_player.getHealth())
                                     / static_cast<float>(m_player.getMaxHealth())
                               : 0.0f;

        int wanted = 0;
        for (const float threshold : config::HullBreakAt)
            wanted += left <= threshold ? 1 : 0;

        while (m_hullLost < wanted && m_player.breakNextPart())
        {
            ++m_hullLost;
            m_audio.play(Sfx::PartBreak, 1.1f);
            addFlash(config::HullBrokenColor, 0.45f);
            addShake(config::ShakeBossPart);
            addHitStop(config::HitStopBossPart);
            addHitRing(m_player.getPosition(), config::PlayerRadius * 3.4f,
                       config::HullBrokenColor);

            // Losing the lamp has to be visible the instant it goes, not at
            // the next time something else happens to touch the light.
            refreshLamp();
        }
    }

    // And so does your grip. Being bitten halfway home is what a haul costs,
    // and it has to cost the distance rather than a number.
    dropCargo();

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
                if (!enemy.isAlive() || shot.getIgnore() == enemy.getId())
                    continue;

                // Under the floor, nothing reaches it, and the shot carries on
                // rather than being eaten - a buried revenant is not cover for
                // whatever is standing behind it.
                if (!enemy.isVulnerable())
                    continue;

                const float reach = shot.getRadius() + enemy.getRadius();
                if ((enemy.getPosition() - shot.getPosition()).lengthSquared() > reach * reach)
                    continue;

                // Already landed on this one. A piercing shot travels slower
                // than a big creature is wide, so without this it hits the same
                // body on step after step and spends the pierce the player paid
                // for on a single target.
                if (shot.alreadyHit(enemy.getId()))
                    continue;

                shot.markHit(enemy.getId());

                // A shield that only faces one way. The shot dies on it and
                // says so: a hit that silently does nothing reads as a bug
                // rather than as a rule.
                if (enemy.deflects(shot.getPosition()))
                {
                    effects::impact(m_effects, shot.getPosition(), -shot.getDirection());
                    m_audio.play(Sfx::Impact, 0.6f);
                    addHitRing(shot.getPosition(), config::JetRingRadius * 0.8f,
                               config::BlockArmourColor);
                    spent = true;
                    break;
                }

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
                m_audio.playAt(hitVoice(enemy.getType()), enemy.getPosition(),
                               crit ? 1.5f : 1.0f, occlusionAt(enemy.getPosition()));
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

                            // Same reason as the splitshot fragments: these are
                            // born inside whatever the torpedo just hit, and
                            // the blast has already accounted for that creature.
                            m_pendingPlayerShots.back().setIgnore(enemy.getId());
                        }
                    }
                    shot.kill();
                    spent = true;
                    break;
                }

                if (splits)
                    spawnSplit(shot.getPosition(), shot.getDirection(), enemy.getId());

                if (shot.spendPierce())
                {
                    spent = true;
                    break;
                }
            }

            if (spent || !shot.isAlive() || !m_boss)
                continue;

            // Which part it would land on, asked before any damage is done, so
            // a shot that is still inside the part it already hit simply
            // passes through it rather than hitting it again this step.
            const int part = m_boss->partAt(shot.getPosition(), shot.getRadius());

            if (part >= 0 && shot.alreadyHit(config::BossHitToken - part))
                continue;

            const Boss::Hit hit = m_boss->tryHit(shot.getPosition(), shot.getRadius(),
                                                 shot.getDamage(), weakPoint);
            if (!hit.hit)
                continue;

            shot.markHit(config::BossHitToken - static_cast<int>(hit.partIndex));

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
        hurtPlayer(shot.getDamage(), { shot.getPosition(), std::nullopt, false, true });
    }

    // Contact damage. The invulnerability window is what stops a pile-up from
    // draining the whole bar in a single second.
    for (Enemy& enemy : m_enemies)
    {
        if (!enemy.isAlive() || !enemy.isVulnerable())
            continue;

        const float reach = enemy.getRadius() + config::PlayerRadius;
        const bool touching =
            (enemy.getPosition() - m_player.getPosition()).lengthSquared() <= reach * reach;

        // The worm is mostly body, and the body is the weapon. Only the head
        // can be shot, but every ring of it can hit you.
        if (!touching && enemy.bodyTouches(m_player.getPosition(), config::PlayerRadius))
        {
            hurtPlayer(enemy.getContactDamage(),
                       { enemy.getPosition(), enemy.getType(), false, true });
            continue;
        }

        if (!touching)
            continue;

        // A siphon does not bite you once, it takes hold. Latching is not a
        // hit, so it does not break the streak by arriving - it breaks it on
        // every mouthful after that, which is worse.
        if (enemy.latches() && !enemy.isLatched())
        {
            enemy.latchTo(m_player.getPosition());
            m_audio.play(Sfx::Hatch, 1.4f);
            addHitRing(m_player.getPosition(), config::PlayerRadius * 2.6f,
                       config::HudBossColor);
            continue;
        }

        if (enemy.isLatched())
            continue; // it is feeding, and feeding is handled on its own clock

        hurtPlayer(enemy.getContactDamage(),
                   { enemy.getPosition(), enemy.getType(), false, true });
    }

    if (m_boss && m_boss->isVulnerable()
        && m_boss->touches(m_player.getPosition(), config::PlayerRadius))
    {
        // A lunge that was announced as a grab and actually connects takes
        // hold instead of biting once. Everything else is an ordinary maul.
        if (m_grabArmed > 0.0f && m_grabTimer <= 0.0f)
        {
            m_grabArmed = 0.0f;
            m_grabTimer = config::GrabDuration;
            m_grabBite = config::GrabBiteInterval * 0.5f;

            bigMoment(m_player.getPosition(), config::GrabColor,
                      config::ShakeDeath, config::SlowMotionBossAct);
            m_audio.play(Sfx::BossRoar, 1.2f);
        }
        else if (m_grabTimer <= 0.0f)
        {
            hurtPlayer(m_boss->getContactDamage(),
                       { m_boss->getPosition(), std::nullopt, true, true });
        }
    }
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

bool Game::takeCommand(Command command)
{
    const unsigned int bit = static_cast<unsigned int>(command);
    const bool held = (m_commands & bit) != 0u;
    m_commands &= ~bit;
    return held;
}

void Game::sampleInput()
{
    // Read once, as late as possible, and let every step of this frame use the
    // same answer. Reading it per step would mean a fast machine polling the
    // keyboard more often than a slow one, which is the frame rate leaking
    // back into the rules by another door.
    m_input.focused = m_window.hasFocus();

    if (!m_input.focused)
    {
        // An unfocused window used to keep the sub swimming, because the
        // movement keys were read straight from the operating system and never
        // asked whether anyone was looking.
        m_input.move = { 0.0f, 0.0f };
        m_input.firing = false;
        m_commands = 0;
        return;
    }

    // Through the bindings rather than straight at a scancode. The scancode
    // already handles layout - W is wherever W physically sits on a QWERTZ
    // board - but it cannot handle preference, and "I want to swim with the
    // arrows" is not an unreasonable thing to want.
    sf::Vector2f move{ 0.0f, 0.0f };

    if (isHeld(Action::Up))
        move.y -= 1.0f;
    if (isHeld(Action::Down))
        move.y += 1.0f;
    if (isHeld(Action::Left))
        move.x -= 1.0f;
    if (isHeld(Action::Right))
        move.x += 1.0f;

    // Normalise so diagonals are not ~41% faster than straight lines.
    m_input.move = move.lengthSquared() > 0.0f ? move.normalized() : move;

    // Through the camera as it was last drawn, which is the one the player was
    // actually looking at when they put the cursor there.
    m_input.aimWorld = m_window.mapPixelToCoords(sf::Mouse::getPosition(m_window), m_camera);
    m_input.firing = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
}

float Game::simTimeScale() const
{
    if (m_state != State::Fighting)
        return 1.0f;

    // A few frames of held time after something heavy lands. The camera and
    // the ambience carry on through it, so the pause reads as impact rather
    // than as the game locking up.
    if (m_hitStop > 0.0f)
        return 0.0f;

    return m_slowScale;
}

void Game::pace()
{
    if (m_frameBudget <= 0.0f)
    {
        m_pacer.restart();
        return;
    }

    const sf::Time budget = sf::seconds(m_frameBudget);
    const sf::Time spin = sf::seconds(config::FrameSpinWindow);
    const sf::Time left = budget - m_pacer.getElapsedTime();

    // Hand the bulk of the wait back to the machine, then hold the last
    // fraction here. SFML's own limiter sleeps the whole way and does it after
    // the swap, which both leaves a millisecond of jitter and spends the spare
    // time at the far end of the frame from where it is useful.
    if (left > spin)
        sf::sleep(left - spin);

    while (m_pacer.getElapsedTime() < budget)
    {
    }

    m_pacer.restart();
}

void Game::run()
{
    m_clock.restart();
    m_pacer.restart();

    while (m_window.isOpen())
    {
        // Wait first and read second. Whatever is left of the frame's budget is
        // spent BEFORE the keyboard is looked at, so what the player is holding
        // is as fresh as the machine can make it. Spending it after the swap
        // instead - which is what a framerate limiter does - means every press
        // sits in a queue for most of a frame before anything sees it.
        pace();

        const float frame = std::min(m_clock.restart().asSeconds(), config::MaxDeltaTime);

        processEvents();
        sampleInput();

        m_lastFrame = frame;
        m_accumulator += frame * simTimeScale();

        int steps = 0;
        while (m_accumulator >= config::FixedDeltaTime && steps < config::MaxStepsPerFrame)
        {
            rememberPositions();
            step(config::FixedDeltaTime);
            m_accumulator -= config::FixedDeltaTime;
            ++steps;
        }

        // Too far behind to catch up: drop the backlog rather than spend the
        // next frame simulating the last one.
        if (steps >= config::MaxStepsPerFrame)
            m_accumulator = 0.0f;

        presentUpdate(frame);
        render(m_accumulator / config::FixedDeltaTime);
    }
}

void Game::updateThreatCue(float deltaTime)
{
    // The arrows this replaced were a crutch fighting the darkness: the whole
    // game is about not being able to see, and then a row of triangles told
    // you exactly where everything was. What survives is the part that was
    // actually doing work - knowing something is close and roughly which way -
    // without it being a readout.
    const sf::Vector2f half{ config::WindowWidth / 2.0f, config::WindowHeight / 2.0f };

    float nearest = config::ThreatCueRange;
    sf::Vector2f bearing{ 0.0f, 0.0f };
    bool boss = false;

    const auto consider = [&](sf::Vector2f position, bool isBoss) {
        const sf::Vector2f offset = position - m_cameraCentre;

        // Only what cannot be seen. Something on the frame is already telling
        // the player where it is by being there.
        if (std::abs(offset.x) < half.x && std::abs(offset.y) < half.y)
            return;

        const float distance = offset.length();
        if (distance > config::ThreatCueRange)
            return;

        // Weighted by how close, so a wall of distant things does not point
        // the same way as one that is nearly on you.
        const float weight = 1.0f - distance / config::ThreatCueRange;
        bearing += offset / std::max(1.0f, distance) * weight * weight;

        if (distance < nearest)
        {
            nearest = distance;
            boss = isBoss;
        }
    };

    if (m_state == State::Fighting && !m_paused && !m_showUpgrades)
    {
        for (const Enemy& enemy : m_enemies)
        {
            if (enemy.isAlive())
                consider(enemy.getPosition(), false);
        }

        if (m_boss && m_boss->isAlive())
            consider(m_boss->getPosition(), true);
    }

    const float wanted = nearest >= config::ThreatCueRange
                             ? 0.0f
                             : 1.0f - nearest / config::ThreatCueRange;

    // Eased rather than switched. Pressure that appears on the frame a
    // creature crosses the line reads as a bug; pressure that swells reads as
    // the water getting closer.
    const float step = config::ThreatCueFade * deltaTime;
    m_threatPressure += std::clamp(wanted - m_threatPressure, -step, step);

    if (bearing.lengthSquared() > 0.0001f)
        m_threatBearing = bearing.normalized();

    m_threatBoss = boss;

    // And a tick, getting closer together as it does. Nothing says direction -
    // the edge of the frame does that - this only says how long you have.
    if (m_threatPressure <= config::ThreatCueFloor)
    {
        m_threatCueTimer = 0.0f;
        return;
    }

    m_threatCueTimer -= deltaTime;
    if (m_threatCueTimer > 0.0f)
        return;

    m_threatCueTimer = config::ThreatCueSlow
                       + (config::ThreatCueFast - config::ThreatCueSlow) * m_threatPressure;

    m_audio.play(boss ? Sfx::CreatureCallA : Sfx::CreatureCallB,
                 config::ThreatCueVolume * m_threatPressure);
}

void Game::drawThreatVignette()
{
    if (m_threatPressure <= config::ThreatCueFloor)
        return;

    // A wash pulled in from the edge of the frame, heaviest on the side the
    // thing is on. Drawn in screen space, so it is the window closing in
    // rather than anything in the water.
    const sf::Vector2f half{ config::WindowWidth / 2.0f, config::WindowHeight / 2.0f };
    const float depth = config::ThreatVignetteDepth;

    const sf::Color base = m_threatBoss ? config::HudBossColor : config::ThreatCueColor;

    sf::VertexArray wash(sf::PrimitiveType::Triangles);

    // Four bands, each faded by how much of the bearing points its way, so the
    // frame darkens unevenly and the heavy side is the answer.
    const sf::Vector2f faces[4] = { { -1.0f, 0.0f }, { 1.0f, 0.0f },
                                    { 0.0f, -1.0f }, { 0.0f, 1.0f } };

    for (const sf::Vector2f& face : faces)
    {
        const float facing = std::max(0.0f, m_threatBearing.dot(face));
        const float weight = (0.22f + 0.78f * facing) * m_threatPressure;

        sf::Color edge = base;
        edge.a = static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(base.a) * weight, 0.0f, 255.0f));

        const sf::Color clear(base.r, base.g, base.b, 0);

        // The band runs the length of its own side of the frame and reaches
        // inward by depth.
        const sf::Vector2f centre = half + sf::Vector2f{ face.x * half.x, face.y * half.y };
        const sf::Vector2f along{ -face.y, face.x };

        const sf::Vector2f reach = along * (face.x != 0.0f ? half.y : half.x);
        const sf::Vector2f inward = -face * depth;

        const sf::Vector2f a = centre - reach;
        const sf::Vector2f b = centre + reach;
        const sf::Vector2f c = b + inward;
        const sf::Vector2f d = a + inward;

        wash.append(sf::Vertex{ a, edge });
        wash.append(sf::Vertex{ b, edge });
        wash.append(sf::Vertex{ c, clear });
        wash.append(sf::Vertex{ a, edge });
        wash.append(sf::Vertex{ c, clear });
        wash.append(sf::Vertex{ d, clear });
    }

    m_window.draw(wash);
}

Game::MapFrame Game::mapFrameFor(sf::Vector2f low, sf::Vector2f high)
{
    MapFrame frame;

    const sf::Vector2f span = high - low;
    if (span.x <= 1.0f || span.y <= 1.0f)
        return frame;

    frame.box = { config::MinimapWidth, config::MinimapHeight };
    frame.scale = std::min(frame.box.x / span.x, frame.box.y / span.y);
    frame.corner = { static_cast<float>(config::WindowWidth) - frame.box.x
                         - config::MinimapMargin,
                     static_cast<float>(config::WindowHeight) - frame.box.y
                         - config::MinimapMargin };
    frame.origin = frame.corner + (frame.box - span * frame.scale) / 2.0f;
    frame.low = low;
    frame.usable = true;

    return frame;
}

void Game::buildMinimap()
{
    m_mapWater.clear();
    m_mapSeen.clear();
    m_mapVertex.clear();
    m_mapOnCard = false;
    m_mapFrame = mapFrameFor(m_arena.getBoundsMin(), m_arena.getBoundsMax());

    if (!m_mapFrame.usable)
        return;

    // Out of the same cells the routing runs on, rather than out of a second
    // idea of where the water is. One quad each, which is what lets the map be
    // uncovered a piece at a time: a room is one enormous lobe and could only
    // ever be revealed whole, and a separate masking mesh would be a second
    // geometry that has to be kept exactly on top of the first one forever.
    const int cells = m_arena.getNavWidth() * m_arena.getNavHeight();
    if (cells <= 0)
        return;

    m_mapSeen.assign(static_cast<std::size_t>(cells), 0u);
    m_mapVertex.assign(static_cast<std::size_t>(cells), NoCell);

    // Half a pixel over, so neighbouring cells do not leave seams between them
    // once they are both uncovered.
    const float half = config::NavCell * 0.5f * m_mapFrame.scale + 0.5f;
    const sf::Color hidden(config::MinimapWater.r, config::MinimapWater.g,
                           config::MinimapWater.b, 0);

    for (int i = 0; i < cells; ++i)
    {
        if (!m_arena.isNavOpen(i))
            continue;

        const sf::Vector2f at = m_mapFrame.place(m_arena.navCellCentre(i));

        m_mapVertex[static_cast<std::size_t>(i)] =
            static_cast<std::uint32_t>(m_mapWater.getVertexCount());

        const sf::Vector2f a{ at.x - half, at.y - half };
        const sf::Vector2f b{ at.x + half, at.y - half };
        const sf::Vector2f c{ at.x + half, at.y + half };
        const sf::Vector2f d{ at.x - half, at.y + half };

        m_mapWater.append(sf::Vertex{ a, hidden });
        m_mapWater.append(sf::Vertex{ b, hidden });
        m_mapWater.append(sf::Vertex{ c, hidden });
        m_mapWater.append(sf::Vertex{ a, hidden });
        m_mapWater.append(sf::Vertex{ c, hidden });
        m_mapWater.append(sf::Vertex{ d, hidden });
    }

    // Dynamic, because from here on it is uploaded six vertices at a time as
    // the basin is uncovered rather than once and never again.
    m_mapWaterCard.setUsage(sf::VertexBuffer::Usage::Dynamic);
    m_mapOnCard = mesh::parkOnCard(m_mapWater, m_mapWaterCard);

    // The mouth is where you started, so it is not a discovery.
    revealMap(m_arena.getStart(), config::MapRevealRadius);
}

void Game::revealMap(sf::Vector2f centre, float radius)
{
    if (m_mapSeen.empty() || radius <= 0.0f)
        return;

    const int width = m_arena.getNavWidth();
    const int height = m_arena.getNavHeight();

    const int here = m_arena.navCellAt(centre);
    if (here < 0)
        return;

    const int reach = static_cast<int>(radius / config::NavCell) + 1;
    const int cx = here % width;
    const int cy = here / width;

    for (int y = std::max(0, cy - reach); y <= std::min(height - 1, cy + reach); ++y)
    {
        for (int x = std::max(0, cx - reach); x <= std::min(width - 1, cx + reach); ++x)
        {
            const int index = y * width + x;
            if (m_mapSeen[static_cast<std::size_t>(index)] != 0u)
                continue;

            const std::uint32_t first = m_mapVertex[static_cast<std::size_t>(index)];
            if (first == NoCell)
                continue;

            if ((m_arena.navCellCentre(index) - centre).length() > radius)
                continue;

            m_mapSeen[static_cast<std::size_t>(index)] = 1u;

            for (std::uint32_t v = 0; v < 6u; ++v)
                m_mapWater[first + v].color = config::MinimapWater;

            // Six vertices to the card rather than the whole basin. The map is
            // uncovered continuously while the sub moves, and re-uploading a
            // few thousand cells for every one of them would cost more than
            // drawing the thing did.
            if (m_mapOnCard)
                (void)m_mapWaterCard.update(&m_mapWater[first], 6u, first);
        }
    }
}

bool Game::mapSeen(sf::Vector2f at) const
{
    const int cell = m_arena.navCellAt(at);

    return cell >= 0 && cell < static_cast<int>(m_mapSeen.size())
           && m_mapSeen[static_cast<std::size_t>(cell)] != 0u;
}

void Game::drawMinimap()
{
    if (!m_mapFrame.usable)
        return;

    const MapFrame& frame = m_mapFrame;
    const auto toMap = [&frame](sf::Vector2f world) { return frame.place(world); };

    sf::RectangleShape backdrop(frame.box);
    backdrop.setPosition(frame.corner);
    backdrop.setFillColor(config::MinimapBackdrop);
    backdrop.setOutlineThickness(1.5f);
    backdrop.setOutlineColor(config::NodeLockedEdge);
    m_window.draw(backdrop);

    if (m_mapOnCard)
        m_window.draw(m_mapWaterCard);
    else
        m_window.draw(m_mapWater);

    // Only on water that has been seen. A marker floating in the dark would
    // give away the shape of the basin a stroke at a time, which is the whole
    // of what the fog was for.
    const auto pip = [&](sf::Vector2f world, float radius, sf::Color colour) {
        if (!mapSeen(world))
            return;

        m_sonarMark.setRadius(radius);
        m_sonarMark.setOrigin({ radius, radius });
        m_sonarMark.setPosition(toMap(world));
        m_sonarMark.setOutlineColor(colour);
        m_sonarMark.setOutlineThickness(2.0f);
        m_window.draw(m_sonarMark);
    };

    // What is worth knowing from across the map, and nothing else. Creatures
    // are deliberately absent: the map says where you have been, not what is
    // in the room with you.
    for (const Vent& vent : m_vents)
        pip(vent.position, 2.5f, config::VentGlowColor);

    for (const Nest& nest : m_nests)
    {
        if (nest.alive)
            pip(nest.position, 4.0f, config::HudSalvageColor);
    }

    // A haul is navigation, so the map has to carry it: where the pods still
    // are, and where home is. Without this the level is a search rather than a
    // decision about which way to run.
    if (!m_cargo.empty())
    {
        pip(m_arena.getStart(), 5.0f, config::CargoHomeColor);

        for (const Cargo& pod : m_cargo)
        {
            if (!pod.delivered && !pod.carried)
                pip(pod.position, 4.0f, config::CargoMarkColor);
        }
    }

    if (m_boss && m_boss->isAlive())
        pip(m_boss->getPosition(), 5.0f, config::HudBossColor);

    // The sub itself always, whether the cell under it has registered yet or
    // not: a map that cannot say where you are standing is not a map.
    m_sonarMark.setRadius(3.5f);
    m_sonarMark.setOrigin({ 3.5f, 3.5f });
    m_sonarMark.setPosition(toMap(m_player.getPosition()));
    m_sonarMark.setOutlineColor(sf::Color::White);
    m_sonarMark.setOutlineThickness(2.0f);
    m_window.draw(m_sonarMark);
}

void Game::drawKillerCard()
{
    // A death that only says you died teaches nothing. This says what did it,
    // what it is, and what it does about you - which is the same two lines the
    // logbook would have shown if the player had ever had a quiet moment to go
    // and look them up. Dying is the moment they most want to know.
    if (!m_haveWound)
        return;

    const float alpha = bannerAlpha(m_stateTimer, config::DeathBannerDuration);
    if (alpha <= 0.01f)
        return;

    const auto fade = [alpha](sf::Color colour) {
        colour.a = static_cast<std::uint8_t>(static_cast<float>(colour.a) * alpha);
        return colour;
    };

    std::string name;
    std::string what;
    std::string how;

    if (m_lastWound.creature)
    {
        const bestiary::Entry& entry = bestiary::entryFor(*m_lastWound.creature);
        name = entry.name;
        what = entry.what;
        how = entry.how;
    }
    else if (m_lastWound.boss && m_boss)
    {
        name = m_boss->getName();
        what = m_boss->getBrief();
        how = "IT WAS NEVER GOING TO BE A FAIR FIGHT.";
    }
    else
    {
        // A vent, the pressure, something that is not an animal at all.
        name = "THE TRENCH";
        what = "NOTHING KILLED YOU. THE WATER DID.";
        how = "IT IS NOT HUNTING. IT IS SIMPLY DEEPER THAN YOU.";
    }

    const float centre = static_cast<float>(config::WindowWidth) / 2.0f;
    float y = config::DeathCardTop;

    sf::VertexArray text(sf::PrimitiveType::Triangles);

    const auto centred = [&](const std::string& line, float scale, sf::Color colour) {
        pixelfont::append(text, line,
                          { centre - pixelfont::measure(line, scale) / 2.0f, y },
                          scale, fade(colour));
        y += 13.0f * scale;
    };

    centred("KILLED BY", 1.4f, config::HudDimTextColor);
    y += 6.0f;
    centred(name, 2.6f, config::HudBossColor);
    y += 10.0f;
    centred(what, 1.5f, config::HudTextColor);
    y += 4.0f;
    centred(how, 1.5f, config::HudDimTextColor);

    m_window.draw(text);
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
        { "Q",       "SURFACE WITH WHAT IS IN THE HOLD", true },
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

    // What surfacing is currently worth, so the choice on this screen is a
    // number rather than a shrug.
    if (m_diveSalvage > 0)
    {
        const std::string hold = std::to_string(m_diveSalvage) + " IN THE HOLD";
        pixelfont::append(text, hold,
                          { (config::WindowWidth - pixelfont::measure(hold, 2.0f)) / 2.0f,
                            y + 26.0f },
                          2.0f, config::HudBossColor);
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
        { "E",            "CYCLE THE FITTED GUNS" },
        { "ARROWS",       "WALK THE ROOTS, IN THE TREE" },
        { "SPACE",        "INSTALL WHAT THE ROOTS LANDED ON" },
        { ",  .",         "PICK THE DIVE, IN THE HUB" },
        { "WHEEL  + -",   "ZOOM THE TREE" },
        { "R",            "STRIP THE HULL BACK, IN THE HUB" },
        { "ESC  OR  P",   "HOLD AT DEPTH" },
        { "M",            "MUTE" },
        { "F11",          "FULLSCREEN" },
    };

    const float keyX = static_cast<float>(config::WindowWidth) / 2.0f - 300.0f;
    const float whatX = keyX + 250.0f;

    // Fifteen rows now, so the step is tightened to keep the last one on
    // screen rather than letting it slide off the bottom.
    float y = 158.0f;
    for (const Row& row : rows)
    {
        appendRow(text, row.key, row.what, y, 1.8f, config::HudSalvageColor,
                  config::HudTextColor, keyX, whatX);
        y += 35.0f;
    }

    m_window.draw(text);
}

// ---------------------------------------------------------------------------
//  Settings
//
//  One table, walked by both the drawing and the keys. It used to be a five
//  entry string array next to a five case switch with the count written out
//  as a constant in a third place, which is three things to keep in step every
//  time a setting is added - and settings are exactly the sort of thing that
//  gets added one at a time forever.
// ---------------------------------------------------------------------------

namespace
{
    enum class Opt
    {
        Header,
        Difficulty,
        Sfx,
        Music,
        Muted,
        Fullscreen,
        VSync,
        FrameCap,
        Brightness,
        Shake,
        Flashes,
        Camera,
        Fps,
        Bind,       // one key, named by the row's action
        Defaults,
    };

    struct OptionRow
    {
        const char* label;
        Opt         kind;
        Action      action = Action::Count; // Bind rows only
    };

    // Sections rather than pages: a list that scrolls is one idea, and a list
    // with tabs is two. The rows the player is looking for are the ones near
    // the top of their own heading.
    const OptionRow OptionRows[] = {
        { "GAMEPLAY",     Opt::Header },
        { "DIFFICULTY",   Opt::Difficulty },
        { "CAMERA",       Opt::Camera },

        { "SOUND",        Opt::Header },
        { "EFFECTS",      Opt::Sfx },
        { "THE DRONE",    Opt::Music },
        { "MUTE",         Opt::Muted },

        { "PICTURE",      Opt::Header },
        { "FULLSCREEN",   Opt::Fullscreen },
        { "VSYNC",        Opt::VSync },
        { "FRAME CAP",    Opt::FrameCap },
        { "SHOW FPS",     Opt::Fps },
        { "BRIGHTNESS",   Opt::Brightness },

        { "COMFORT",      Opt::Header },
        { "SCREEN SHAKE", Opt::Shake },
        { "FLASHES",      Opt::Flashes },

        { "CONTROLS",     Opt::Header },
        { "SWIM UP",      Opt::Bind, Action::Up },
        { "SWIM DOWN",    Opt::Bind, Action::Down },
        { "SWIM LEFT",    Opt::Bind, Action::Left },
        { "SWIM RIGHT",   Opt::Bind, Action::Right },
        { "DASH",         Opt::Bind, Action::Dash },
        { "LAMP",         Opt::Bind, Action::Lamp },
        { "CYCLE GUN",    Opt::Bind, Action::FireMode },
        { "SURFACE",      Opt::Bind, Action::Surface },
        { "PAUSE",        Opt::Bind, Action::Pause },
        { "UPGRADES",     Opt::Bind, Action::Upgrades },

        { "",             Opt::Header },
        { "BACK TO DEFAULTS", Opt::Defaults },
    };

    constexpr int OptionRowCount = static_cast<int>(std::size(OptionRows));

    // How many rows fit between the heading and the hint at the bottom.
    constexpr int OptionWindow = 11;

    // Frame caps worth offering. Zero is uncapped, which is genuinely what
    // somebody with a 240 Hz panel and a fast machine wants.
    constexpr int FrameCaps[] = { 30, 60, 75, 90, 120, 144, 165, 240, 0 };
}

int Game::firstOptionRow() const
{
    for (int i = 0; i < OptionRowCount; ++i)
    {
        if (OptionRows[static_cast<std::size_t>(i)].kind != Opt::Header)
            return i;
    }

    return 0;
}

std::string Game::optionValue(int row) const
{
    const OptionRow& option = OptionRows[static_cast<std::size_t>(row)];

    // A bar of ten blocks reads as a level at a glance without needing a
    // slider widget or a number nobody can picture.
    const auto meter = [](int tenths) {
        std::string bar;
        for (int i = 0; i < 10; ++i)
            bar += i < tenths ? '#' : '-';
        return bar;
    };

    switch (option.kind)
    {
        case Opt::Difficulty: return difficulty().name;
        case Opt::Sfx:        return meter(m_save.sfxVolume);
        case Opt::Music:      return meter(m_save.musicVolume);
        case Opt::Muted:      return m_save.muted ? "YES" : "NO";
        case Opt::Fullscreen: return m_fullscreen ? "YES" : "NO";
        case Opt::VSync:      return m_save.vsync ? "ON" : "OFF";
        case Opt::FrameCap:   return m_save.frameCap == 0
                                         ? std::string("UNCAPPED")
                                         : std::to_string(m_save.frameCap);
        case Opt::Brightness: return meter(m_save.brightness);
        case Opt::Shake:      return meter(m_save.shake);
        case Opt::Flashes:    return meter(m_save.flashes);
        case Opt::Camera:     return meter(m_save.cameraTight);
        case Opt::Fps:        return m_save.showFps ? "ON" : "OFF";
        case Opt::Defaults:   return "ENTER";

        case Opt::Bind:
        {
            if (m_rebinding == row)
                return "PRESS A KEY";

            const sf::Keyboard::Scancode key = keyFor(option.action);
            const char* name = keyName(key);

            return name != nullptr ? std::string(name)
                                   : std::to_string(static_cast<int>(key));
        }

        case Opt::Header: break;
    }

    return {};
}

void Game::stepOption(int row, int delta)
{
    const OptionRow& option = OptionRows[static_cast<std::size_t>(row)];

    const auto bump = [delta](int value) { return std::clamp(value + delta, 0, 10); };

    switch (option.kind)
    {
        case Opt::Difficulty:
            m_save.difficulty = std::clamp(m_save.difficulty + delta, 0,
                                           static_cast<int>(config::Difficulties.size()) - 1);
            break;

        case Opt::Sfx:        m_save.sfxVolume = bump(m_save.sfxVolume); break;
        case Opt::Music:      m_save.musicVolume = bump(m_save.musicVolume); break;
        case Opt::Brightness: m_save.brightness = bump(m_save.brightness); break;
        case Opt::Shake:      m_save.shake = bump(m_save.shake); break;
        case Opt::Flashes:    m_save.flashes = bump(m_save.flashes); break;
        case Opt::Camera:     m_save.cameraTight = bump(m_save.cameraTight); break;

        case Opt::Muted: m_save.muted = delta > 0; break;
        case Opt::VSync: m_save.vsync = delta > 0; break;
        case Opt::Fps:   m_save.showFps = delta > 0; break;

        case Opt::Fullscreen:
            if ((delta > 0) != m_fullscreen)
            {
                toggleFullscreen();
                m_save.fullscreen = m_fullscreen;
            }
            break;

        case Opt::FrameCap:
        {
            constexpr int count = static_cast<int>(std::size(FrameCaps));

            int index = 0;
            for (int i = 0; i < count; ++i)
            {
                if (FrameCaps[i] == m_save.frameCap)
                    index = i;
            }

            m_save.frameCap = FrameCaps[std::clamp(index + delta, 0, count - 1)];
            break;
        }

        case Opt::Bind:
            // Left and right do nothing to a binding. Enter arms it, because a
            // key is not on a scale and nudging one would be nonsense.
            break;

        case Opt::Defaults:
        case Opt::Header:
            break;
    }

    applySettings();
    m_save.save();
}

void Game::pickOption(int row)
{
    const OptionRow& option = OptionRows[static_cast<std::size_t>(row)];

    if (option.kind == Opt::Bind)
    {
        m_rebinding = row;
        m_audio.play(Sfx::Charge, 0.4f);
        return;
    }

    if (option.kind == Opt::Defaults)
    {
        // Settings only. Progress, the tree and the logbook are not settings
        // and are not what anybody means by this button.
        const SaveGame fresh;

        m_save.difficulty = fresh.difficulty;
        m_save.sfxVolume = fresh.sfxVolume;
        m_save.musicVolume = fresh.musicVolume;
        m_save.muted = fresh.muted;
        m_save.vsync = fresh.vsync;
        m_save.frameCap = fresh.frameCap;
        m_save.brightness = fresh.brightness;
        m_save.shake = fresh.shake;
        m_save.flashes = fresh.flashes;
        m_save.cameraTight = fresh.cameraTight;
        m_save.keys.clear();

        if (m_fullscreen != fresh.fullscreen)
        {
            toggleFullscreen();
            m_save.fullscreen = m_fullscreen;
        }

        applySettings();
        m_save.save();
        m_audio.play(Sfx::PartBreak, 0.6f);
        return;
    }

    // Everything else: enter nudges it forward, so the list can be driven
    // without ever reaching for the arrow keys.
    stepOption(row, 1);
}

sf::Keyboard::Scancode Game::keyFor(Action action) const
{
    const std::size_t index = static_cast<std::size_t>(action);

    if (index < m_save.keys.size())
        return static_cast<sf::Keyboard::Scancode>(m_save.keys[index]);

    return DefaultKeys[index];
}

bool Game::isHeld(Action action) const
{
    return m_input.focused && sf::Keyboard::isKeyPressed(keyFor(action));
}

void Game::bindKey(int row, sf::Keyboard::Scancode key)
{
    const OptionRow& option = OptionRows[static_cast<std::size_t>(row)];
    if (option.kind != Opt::Bind)
        return;

    if (m_save.keys.size() < ActionCount)
    {
        // First rebind: start from whatever the defaults were, so the other
        // ten actions do not silently become unbound.
        m_save.keys.resize(ActionCount);
        for (std::size_t i = 0; i < ActionCount; ++i)
            m_save.keys[i] = static_cast<int>(DefaultKeys[i]);
    }

    // A key can only do one thing. Whatever had it loses it - unless losing it
    // would leave the sub unable to swim, in which case the two swap.
    const std::size_t wanted = static_cast<std::size_t>(option.action);

    for (std::size_t i = 0; i < ActionCount; ++i)
    {
        if (i == wanted || m_save.keys[i] != static_cast<int>(key))
            continue;

        m_save.keys[i] = isEssential(static_cast<Action>(i))
                             ? m_save.keys[wanted]
                             : static_cast<int>(sf::Keyboard::Scancode::Unknown);
    }

    m_save.keys[wanted] = static_cast<int>(key);

    m_rebinding = -1;
    m_save.save();
    m_audio.play(Sfx::Impact, 0.4f);
}

void Game::drawOptions()
{
    m_deathOverlay.setFillColor(config::PauseVeil);
    m_window.draw(m_deathOverlay);
    m_deathOverlay.setFillColor(config::DeathOverlayColor);

    sf::VertexArray text(sf::PrimitiveType::Triangles);
    appendHeading(text, "OPTIONS",
                  m_rebinding >= 0 ? "PRESS A KEY.  ESCAPE CANCELS."
                                   : "ARROWS CHANGE.  ENTER PICKS.  ESCAPE GOES BACK.");

    const float labelX = static_cast<float>(config::WindowWidth) / 2.0f - 260.0f;
    const float valueX = labelX + 320.0f;

    float y = 200.0f;

    for (int i = m_optionScroll; i < OptionRowCount && i < m_optionScroll + OptionWindow; ++i)
    {
        const OptionRow& option = OptionRows[static_cast<std::size_t>(i)];

        if (option.kind == Opt::Header)
        {
            if (option.label[0] != '\0')
            {
                pixelfont::append(text, option.label, { labelX - 18.0f, y + 6.0f }, 1.5f,
                                  config::HudSalvageColor);
            }

            y += 34.0f;
            continue;
        }

        const bool picked = i == m_optionRow;

        appendRow(text, (picked ? "> " : "  ") + std::string(option.label), optionValue(i),
                  y, 2.0f,
                  picked ? config::HudTextColor : config::HudDimTextColor,
                  picked ? config::HudSalvageColor : config::HudDimTextColor,
                  labelX, valueX);

        y += 34.0f;
    }

    // A hint that there is more, because a list that scrolls silently reads as
    // a list that ends.
    if (m_optionScroll + OptionWindow < OptionRowCount)
    {
        pixelfont::append(text, "...", { labelX, y + 4.0f }, 1.6f, config::HudDimTextColor);
    }

    m_window.draw(text);
}

void Game::handleOptionsKey(sf::Keyboard::Scancode key)
{
    // Armed and waiting for a key: everything except escape is the answer.
    if (m_rebinding >= 0)
    {
        if (key == sf::Keyboard::Scancode::Escape)
            m_rebinding = -1;
        else
            bindKey(m_rebinding, key);

        return;
    }

    const auto move = [&](int delta) {
        // Headings are labels, not choices, so the cursor steps over them.
        for (int i = 0; i < OptionRowCount; ++i)
        {
            m_optionRow = (m_optionRow + delta + OptionRowCount) % OptionRowCount;

            if (OptionRows[static_cast<std::size_t>(m_optionRow)].kind != Opt::Header)
                break;
        }

        // Keep the cursor inside the window, and keep its heading above it
        // wherever there is room.
        m_optionScroll = std::clamp(m_optionScroll, m_optionRow - OptionWindow + 2,
                                    std::max(0, m_optionRow - 1));
        m_optionScroll = std::clamp(m_optionScroll, 0,
                                    std::max(0, OptionRowCount - OptionWindow));
    };

    switch (key)
    {
        case sf::Keyboard::Scancode::Up:
        case sf::Keyboard::Scancode::W:
            move(-1);
            break;

        case sf::Keyboard::Scancode::Down:
        case sf::Keyboard::Scancode::S:
            move(1);
            break;

        case sf::Keyboard::Scancode::Left:
        case sf::Keyboard::Scancode::A:
            stepOption(m_optionRow, -1);
            break;

        case sf::Keyboard::Scancode::Right:
        case sf::Keyboard::Scancode::D:
            stepOption(m_optionRow, 1);
            break;

        case sf::Keyboard::Scancode::Enter:
        case sf::Keyboard::Scancode::Space:
            pickOption(m_optionRow);
            break;

        case sf::Keyboard::Scancode::Escape:
            openMenu();
            break;

        default:
            break;
    }
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

void Game::drawBestiary()
{
    m_deathOverlay.setFillColor(config::PauseVeil);
    m_window.draw(m_deathOverlay);
    m_deathOverlay.setFillColor(config::DeathOverlayColor);

    sf::VertexArray text(sf::PrimitiveType::Triangles);
    appendHeading(text, "LOGBOOK", "ARROWS PICK.  ESCAPE GOES BACK.");

    const auto killsOf = [this](std::size_t index) {
        return index < m_save.killed.size() ? m_save.killed[index] : 0;
    };

    int met = 0;
    for (std::size_t i = 0; i < EnemyTypeCount; ++i)
        met += killsOf(i) > 0 ? 1 : 0;

    const std::string tally = "MET " + std::to_string(met) + "/"
                              + std::to_string(static_cast<int>(EnemyTypeCount));
    pixelfont::append(text, tally, { config::HudMargin, 116.0f }, 1.6f, config::HudDimTextColor);

    // The list down the left. Anything never killed is a blank line rather
    // than a name: the book is a record of what has actually been met, and
    // reading it ahead of time would give the whole trench away.
    const float listX = config::HudMargin;
    float y = 150.0f;

    for (std::size_t i = 0; i < EnemyTypeCount; ++i)
    {
        const bool known = killsOf(i) > 0;
        const bool picked = static_cast<int>(i) == m_beastRow;

        const std::string label = known ? bestiary::Entries[i].name : "- - - - -";

        sf::Color colour = config::HudDimTextColor;
        if (picked)
            colour = known ? sf::Color::White : config::NodeLockedEdge;
        else if (known)
            colour = config::HudTextColor;

        pixelfont::append(text, picked ? ("> " + label) : ("  " + label),
                          { listX, y }, 1.7f, colour);

        if (known)
        {
            const std::string count = std::to_string(killsOf(i));
            pixelfont::append(text, count,
                              { 330.0f - pixelfont::measure(count, 1.4f), y + 2.0f },
                              1.4f, config::HudSalvageColor);
        }

        y += 30.0f;
    }

    m_window.draw(text);

    // The panel on the right: the animal itself, as big as it will go, and
    // what the book has to say about it.
    const auto row = static_cast<std::size_t>(std::clamp(m_beastRow, 0,
                                                         static_cast<int>(EnemyTypeCount) - 1));
    const bool known = killsOf(row) > 0;

    sf::RectangleShape panel;
    panel.setSize({ 820.0f, 480.0f });
    panel.setPosition({ 400.0f, 140.0f });
    panel.setFillColor(sf::Color(4, 12, 18, 210));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(known ? config::NodeReadyEdge : config::NodeLockedEdge);
    m_window.draw(panel);

    sf::VertexArray panelText(sf::PrimitiveType::Triangles);

    if (!known)
    {
        const std::string unknown = "NOT YET MET";
        pixelfont::append(panelText, unknown,
                          { 400.0f + (820.0f - pixelfont::measure(unknown, 2.4f)) / 2.0f, 350.0f },
                          2.4f, config::NodeLockedEdge);
        m_window.draw(panelText);
        return;
    }

    const bestiary::Entry& entry = bestiary::Entries[row];
    const auto type = static_cast<EnemyType>(row);

    // Scaled to fill the plate rather than drawn at the size it fights at: the
    // whole point of the book is that it is the one place these hold still.
    sf::Sprite portrait(m_assets.forEnemy(type));
    const sf::Vector2f size(portrait.getTexture().getSize());
    portrait.setOrigin(size / 2.0f);

    // The portrait sits over the text rather than beside it: the longest line
    // in the book is fifty-eight characters, and beside a portrait there is
    // nowhere near enough room for that without shrinking it to nothing.
    const float fit = std::min(340.0f / std::max(1.0f, size.x), 190.0f / std::max(1.0f, size.y));
    portrait.setScale({ fit, fit });
    portrait.setPosition({ 810.0f, 255.0f });
    m_window.draw(portrait);

    const float textX = 440.0f;

    pixelfont::append(panelText, entry.name, { textX, 370.0f }, 3.0f, config::HudTextColor);
    pixelfont::append(panelText, entry.what, { textX, 424.0f }, 1.5f, config::HudDimTextColor);
    pixelfont::append(panelText, entry.how, { textX, 450.0f }, 1.5f, config::HudSalvageColor);

    const config::EnemyStats& stats = config::enemyStats(type);

    struct Line { const char* label; std::string value; };
    const Line lines[] = {
        { "KILLED", std::to_string(killsOf(row)) },
        { "HULL",   std::to_string(stats.health) },
        { "BITE",   std::to_string(stats.contactDamage) },
        { "SALVAGE", std::to_string(stats.salvage) },
    };

    // Two columns, because four stacked rows under two lines of prose would
    // run the panel out of floor.
    float statX = textX;
    float statY = 500.0f;

    for (const Line& line : lines)
    {
        pixelfont::append(panelText, line.label, { statX, statY }, 1.5f, config::HudDimTextColor);
        pixelfont::append(panelText, line.value, { statX + 130.0f, statY }, 1.5f,
                          config::HudTextColor);

        statY += 30.0f;
        if (statY > 540.0f)
        {
            statY = 500.0f;
            statX += 330.0f;
        }
    }

    m_window.draw(panelText);
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

void Game::presentUpdate(float deltaTime)
{
    // Real time, always, for everything that is only looked at. None of this is
    // allowed to decide anything: if it did, a machine drawing twice as often
    // would be playing a different game.
    m_presentTime += deltaTime;

    m_arena.update(deltaTime);
    m_seabed.update(deltaTime);
    updateFeel(deltaTime);
    updateThreatCue(deltaTime);
    spawnAmbience(deltaTime);
    callFromTheDark(deltaTime);

    // The hit is held in real time too - a tenth of a second is a tenth of a
    // second - and what it holds is the accumulator, not this.
    if (m_hitStop > 0.0f)
        m_hitStop = std::max(0.0f, m_hitStop - deltaTime);

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

        default:
            break;
    }

    m_ambience.update(deltaTime);
    m_effects.update(deltaTime);
}


void Game::step(float deltaTime)
{
    // One fixed slice of the fight. Nothing in here may ask how long a frame
    // took, and nothing outside it may change what the game does.
    updateCamera(deltaTime);

    switch (m_state)
    {
        case State::Story:
        case State::Menu:
        case State::Credits:
        case State::Controls:
        case State::Options:
        case State::Victory:
        case State::Upgrading:
            break;

        case State::LevelIntro:
            m_stateTimer -= deltaTime;
            if (m_stateTimer <= 0.0f)
                m_state = State::Fighting;
            break;

        case State::Fighting:
            if (!m_showUpgrades && !m_paused)
            {
                // Slow motion is applied to what goes into the accumulator, not
                // to the step: a shorter step would change the rate everything
                // is integrated at, and with it every number in the game.
                m_tally.seconds += deltaTime;

                // Pressed once, so it is spent once - on the first step of the
                // frame it arrived in, whichever step that turns out to be.
                if (takeCommand(Command::Lamp))
                    toggleLamp();
                if (takeCommand(Command::FireMode))
                    cycleFireMode();
                if (takeCommand(Command::Torpedo))
                    fireTorpedo();
                if (takeCommand(Command::Dash))
                    performDash();

                // Held in a mouth, nothing the player presses moves the sub.
                // updateBossMoves is what carries it, and the dash is the only
                // way out.
                if (m_grabTimer <= 0.0f)
                {
                    m_player.update(deltaTime, m_input.move, m_input.aimWorld, m_arena);
                }
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
                updateCargo(deltaTime);

                if (m_modeTimer > 0.0f)
                    m_modeTimer -= deltaTime;

                updateVolleys(deltaTime);
                updateHarpoon(deltaTime);
                updateSonar(deltaTime);

                // The map fills in behind the sub as it goes. A broken hull
                // does not stop you seeing where you are - it stops the
                // ping, which is handled inside updateSonar.
                revealMap(m_player.getPosition(), config::MapRevealRadius);

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
    }
}

void Game::rememberPositions()
{
    // Taken before every step, so that after it there is a pair to draw
    // between. Only things that move fast enough to stutter are in here;
    // mines, vents and nests sit still once they are placed.
    m_prevCameraCentre = m_cameraCentre;
    m_player.remember();

    for (Enemy& enemy : m_enemies)
        enemy.remember();

    for (Projectile& shot : m_playerShots)
        shot.remember();

    for (Projectile& shot : m_enemyShots)
        shot.remember();

    for (Projectile& torpedo : m_torpedoes)
        torpedo.remember();
}

float Game::lightAt(sf::Vector2f where) const
{
    // The same shape the lamp is actually drawn as, asked about a point: a
    // narrow beam along the aim and a short pool close in. It is worked out
    // here rather than read back off the light mesh because the mesh is on the
    // graphics card and this is a question the simulation wants answered.
    const sf::Vector2f offset = where - m_player.getPosition();
    const float distance = offset.length();
    const float reach = m_arena.getLampReach();

    if (reach <= 1.0f)
        return config::LightFloor;

    const float halo = reach * config::LampHaloFactor;
    float amount = config::LightFloor;

    // Which way the point is off the nose, which both halves want.
    const float facing = distance > 1.0f
                             ? offset.dot(m_player.getAimDirection()) / distance
                             : 1.0f;

    // The spill, which is what lets you see something that has got behind you.
    // Weighted the same way the mesh is - a teardrop, not a disc - so a thing
    // astern is a shape in the dark rather than as lit as one in the beam.
    if (distance < halo)
    {
        const float side = config::LampHaloBack
                           + (1.0f - config::LampHaloBack) * (0.5f + 0.5f * facing);
        const float fall = 1.0f - distance / halo;

        amount = std::max(amount, config::LampHaloBrightness * side * fall * fall);
    }

    if (distance < reach && distance > 1.0f)
    {
        const float edge = std::cos(sf::degrees(config::LampConeHalfAngle).asRadians());

        if (facing > edge)
        {
            // Across the beam and along it, the same two falloffs the mesh has.
            const float across = (facing - edge) / std::max(0.01f, 1.0f - edge);
            amount = std::max(amount, std::sqrt(across) * (1.0f - distance / reach));
        }
    }

    return std::clamp(amount, config::LightFloor, 1.0f);
}

void Game::presentEntities(float alpha)
{
    m_player.present(alpha);

    for (Enemy& enemy : m_enemies)
    {
        enemy.present(alpha);
        enemy.setLit(lightAt(enemy.getPosition()));
    }

    for (Projectile& shot : m_playerShots)
        shot.present(alpha);

    for (Projectile& shot : m_enemyShots)
        shot.present(alpha);

    for (Projectile& torpedo : m_torpedoes)
        torpedo.present(alpha);
}

void Game::render(float alpha)
{
    presentEntities(alpha);
    aimCamera(alpha);

    m_window.clear(themeFor(m_level.getDepth()).background);

    // Everything that lives in the basin is drawn through the camera.
    m_window.setView(m_camera);

    // The rock first, then the far boulders standing on it, then the basin cut
    // out of both. Painting the rock after the far layer would bury it, which
    // is exactly what used to happen to the basin itself.
    m_seabed.drawRock(m_window);
    m_seabed.drawFar(m_window, m_cameraCentre);
    m_arena.drawGround(m_window);
    m_seabed.draw(m_window);
    m_arena.drawLight(m_window, m_player.getPosition(), m_player.getAimDirection().angle());

    m_ambience.draw(m_window);

    // Under everything alive: a vent is a hole in the floor, not an object.
    drawVents();
    drawNests();
    drawCargo();

    for (const Mine& mine : m_mines)
        m_window.draw(mine.sprite);

    if (m_boss)
    {
        m_boss->draw(m_window);
        drawBossTell();
        drawBossMoves();
    }

    // What the deep creatures are about to do, over the water rather than on
    // them: a herald's reach, and the moment before a rift is somewhere else.
    for (const Enemy& enemy : m_enemies)
    {
        if (!enemy.isAlive())
            continue;

        if (enemy.getType() == EnemyType::Herald)
        {
            const float pulse = 0.94f + 0.06f * std::sin(m_stateTimer * 2.2f);
            const float radius = config::HeraldRadius * pulse;

            m_sonarRing.setRadius(radius);
            m_sonarRing.setOrigin({ radius, radius });
            m_sonarRing.setPosition(enemy.getPosition());
            m_sonarRing.setOutlineThickness(2.5f);
            m_sonarRing.setOutlineColor(config::HeraldAuraColor);
            m_sonarRing.setFillColor(sf::Color(config::HeraldAuraColor.r,
                                               config::HeraldAuraColor.g,
                                               config::HeraldAuraColor.b, 12));
            m_window.draw(m_sonarRing);
            m_sonarRing.setFillColor(sf::Color::Transparent);
        }

        const float tell = enemy.getSurfaceTell();
        if (tell > 0.0f)
        {
            const float radius = enemy.getRadius() * (3.2f - 2.2f * tell);

            sf::Color edge = config::RevenantTellColor;
            edge.a = static_cast<std::uint8_t>(80.0f + 160.0f * tell);

            m_sonarRing.setRadius(radius);
            m_sonarRing.setOrigin({ radius, radius });
            m_sonarRing.setPosition(enemy.getPosition());
            m_sonarRing.setOutlineThickness(2.0f + 3.0f * tell);
            m_sonarRing.setOutlineColor(edge);
            m_window.draw(m_sonarRing);
        }
    }

    for (const Enemy& enemy : m_enemies)
        enemy.draw(m_window);

    for (const Projectile& shot : m_enemyShots)
        shot.draw(m_window);

    for (const Projectile& shot : m_playerShots)
        shot.draw(m_window);

    for (const Projectile& torpedo : m_torpedoes)
        torpedo.draw(m_window);

    drawHarpoon();

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

        // A nest reads back far louder than a creature does: it is cemented to
        // rock and it is the size of a room. The ping is what makes a sonar
        // array worth buying on a hunt.
        sf::Color found = config::HudSalvageColor;
        found.a = fade;

        for (const Nest& nest : m_nests)
        {
            if (!nest.alive
                || (nest.position - m_player.getPosition()).length() > range * config::SonarNestReach)
                continue;

            const float mark = config::NestRadius * 1.5f;
            m_sonarRing.setRadius(mark);
            m_sonarRing.setOrigin({ mark, mark });
            m_sonarRing.setPosition(nest.position);
            m_sonarRing.setOutlineColor(found);
            m_window.draw(m_sonarRing);
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
    {
        drawThreatVignette();
        drawMinimap();
    }

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
    if (m_state == State::Bestiary) { drawBestiary(); m_window.display(); return; }
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

        // Only shown once it is worth showing. A counter that reads 1 the
        // whole time is noise; one that appears when a run gets going is a
        // thing the player starts protecting.
        info.streak = m_streak >= config::StreakNotable ? m_streak : 0;
        info.streakLoud = m_streak >= config::StreakLoud;
        info.held = m_diveSalvage;
        info.broken = m_player.getBrokenMask();
        info.mode = modeCount() > 1 ? modeName(modeAt(m_fireSlot)) : nullptr;
        info.streakBonus = static_cast<int>(std::lround(
            config::StreakSalvageStep * 100.0f
            * static_cast<float>(std::min(m_streak, config::StreakSalvageCap))));

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
            summary.blocks = m_blocks;
        summary.bestStreak = m_bestStreak;
        summary.accuracy = m_tally.shotsFired > 0
                                   ? static_cast<float>(m_tally.shotsHit)
                                         / static_cast<float>(m_tally.shotsFired)
                                   : 0.0f;

            m_hud.drawSummary(m_window, summary, alpha);
        }
        else if (m_state == State::GameOver)
        {
            // The tree survives, so the line has to say what did not: the
            // hold. A death that says NOTHING LOST when it has just cost two
            // hundred salvage is a lie the player will catch immediately.
            const std::string cost =
                m_lostSalvage > 0
                    ? std::to_string(m_lostSalvage) + " SALVAGE WENT DOWN WITH YOU"
                    : std::string("THE HOLD WAS EMPTY. TRY IT AGAIN.");

            m_hud.drawBanner(m_window, "LEVEL " + std::to_string(m_level.getNumber()),
                             "THE DARK TOOK YOU",
                             cost,
                             config::HudBossColor,
                             bannerAlpha(m_stateTimer, config::DeathBannerDuration));

            drawKillerCard();
        }
    }

    if (m_paused)
        drawPauseOverlay();

    drawFrameRate();
    drawBrightness();
    m_window.display();
}

void Game::drawFrameRate()
{
    // Averaged over half a second rather than taken from one frame: an
    // instantaneous figure jitters by twenty either way and tells nobody
    // anything. Off by default, because a number in the corner of a horror
    // game is a number in the corner of a horror game.
    if (!m_save.showFps)
        return;

    ++m_fpsFrames;
    m_fpsWindow += m_lastFrame;

    if (m_fpsWindow >= 0.5f)
    {
        m_fpsShown = static_cast<float>(m_fpsFrames) / m_fpsWindow;
        m_fpsFrames = 0;
        m_fpsWindow = 0.0f;
    }

    sf::VertexArray text(sf::PrimitiveType::Triangles);
    const std::string line = std::to_string(static_cast<int>(std::lround(m_fpsShown))) + " FPS";

    m_window.setView(m_ui);
    pixelfont::append(text, line,
                      { static_cast<float>(config::WindowWidth)
                            - pixelfont::measure(line, 1.4f) - config::HudMargin,
                        static_cast<float>(config::WindowHeight) - 22.0f },
                      1.4f, config::HudDimTextColor);

    m_window.draw(text);
}

void Game::drawBrightness()
{
    // The game is very dark on purpose, and on somebody else's monitor that
    // can be the difference between atmospheric and unplayable. A wash over
    // the finished frame rather than a change to the art: lifting it lifts
    // everything together, so nothing that was meant to be readable against
    // something else stops being.
    if (m_save.brightness == 5)
        return;

    const float amount = (static_cast<float>(m_save.brightness) - 5.0f) / 5.0f;

    m_deathOverlay.setFillColor(
        amount > 0.0f
            ? sf::Color(255, 255, 255,
                        static_cast<std::uint8_t>(amount * config::BrightnessLift))
            : sf::Color(0, 0, 0,
                        static_cast<std::uint8_t>(-amount * config::BrightnessDrop)));

    m_window.setView(m_ui);
    m_window.draw(m_deathOverlay);
    m_deathOverlay.setFillColor(config::DeathOverlayColor);
}
