#include "entities/Enemy.h"

#include "core/Config.h"
#include "core/Random.h"
#include "world/Arena.h"

#include <SFML/System/Angle.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    // Never reused, so an id cannot be mistaken for a creature that has since
    // died and been replaced in the same slot.
    int nextEnemyId = 1;
}

Enemy::Enemy(EnemyType type, const sf::Texture& texture, sf::Vector2f position,
             const EnemyScaling& scaling)
    : m_id(nextEnemyId++)
    , m_type(type)
    , m_stats(&config::enemyStats(type))
    , m_scaling(scaling)
    , m_sprite(texture)
    , m_position(position)
    , m_aimDirection(1.0f, 0.0f)
    , m_phase(rng::range(0.0f, 6.283185307f))
    , m_health(std::max(1, static_cast<int>(std::lround(
          static_cast<float>(config::enemyStats(type).health) * scaling.health))))
    , m_maxHealth(m_health)
    , m_hitFlashTimer(0.0f)
    , m_chillTimer(0.0f)
    , m_chillStrength(0.0f)
    , m_bleedTimer(0.0f)
    , m_bleedTick(0.0f)
    , m_bleedRate(0.0f)
    , m_fireTimer(rng::range(0.0f, std::max(0.0f, config::enemyStats(type).fireCooldown)))
    , m_strafeDirection(rng::range(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f)
    , m_burstTimer(rng::range(0.0f, 0.4f))
    , m_bursting(true)
    , m_spin(rng::range(0.0f, 360.0f))
    , m_lanceHeading(1.0f, 0.0f)
    , m_broodTimer(rng::range(1.0f, std::max(1.2f, config::enemyStats(type).broodInterval)))
    , m_playerDistance(0.0f)
    , m_inFiringRange(false)
    , m_hasEntered(false)
    , m_aware(true)
    , m_wanderAngle(rng::range(0.0f, 360.0f))
    , m_scripted(true)
{
    m_sprite.setOrigin(sf::Vector2f(texture.getSize()) / 2.0f);
    m_sprite.setScale({ m_stats->scale, m_stats->scale });
    m_sprite.setPosition(m_position);
    m_bodyScale = m_stats->scale;
}

float Enemy::getRadius() const
{
    return m_stats->radius;
}

int Enemy::getContactDamage() const
{
    return m_stats->contactDamage + m_scaling.bonusDamage;
}

int Enemy::getSalvage() const
{
    return m_stats->salvage;
}

void Enemy::move(float deltaTime, sf::Vector2f toPlayer, float distance)
{
    const sf::Vector2f direction = toPlayer / distance;
    const float speed = m_stats->speed * m_scaling.speed;

    // Still out in the dark: everything charges straight in, and faster than it
    // normally moves. Otherwise crossing the gloom would eat the wave timer,
    // and a spitter would stop to snipe from off screen.
    if (!m_hasEntered)
    {
        m_position += direction * speed * config::EnemyApproachBoost * deltaTime;
        return;
    }

    switch (m_stats->behaviour)
    {
        case EnemyBehaviour::Chase:
            m_position += direction * speed * deltaTime;
            break;


        case EnemyBehaviour::Orbit:
        {
            // Never closes. It rides a ring around you and corrects its radius
            // gently, so the only way to be safe is to keep moving yourself.
            const sf::Vector2f tangent(-direction.y, direction.x);
            const float gap = distance - m_stats->preferredDistance;

            m_position += tangent * m_strafeDirection * speed * deltaTime;
            m_position += direction * std::clamp(gap / 70.0f, -1.0f, 1.0f) * speed
                          * config::OrbitCloseFactor * deltaTime;
            break;
        }

        case EnemyBehaviour::Lance:
        {
            // Winds up in place, locks a heading, then commits to it. Once it
            // is moving it cannot correct, which is the whole counterplay.
            m_burstTimer -= deltaTime;
            if (m_burstTimer <= 0.0f)
            {
                m_bursting = !m_bursting;
                m_burstTimer = m_bursting ? m_stats->burstOn : m_stats->burstOff;

                if (m_bursting)
                    m_lanceHeading = direction;
            }

            if (m_bursting)
                m_position += m_lanceHeading * speed * deltaTime;
            else
                m_position += direction * speed * config::LanceCreepFactor * deltaTime;
            break;
        }

        case EnemyBehaviour::Bulwark:
        {
            // Straight at you, but it can only turn so fast, and the shield
            // goes where it is pointing. Getting behind it is not a trick, it
            // is the intended answer, and the turn rate is what makes it one.
            const float turn = config::SerpentTurn * 0.42f * deltaTime;
            const sf::Angle wanted = direction.angle();
            const sf::Angle have = m_heading.angle();
            const float error = sf::degrees(std::fmod(
                (wanted - have).asDegrees() + 540.0f, 360.0f) - 180.0f).asDegrees();

            m_heading = m_heading.rotatedBy(
                sf::degrees(std::clamp(error, -turn, turn)));
            m_position += m_heading * speed * deltaTime;
            break;
        }

        case EnemyBehaviour::Burrow:
            // Under the floor it tracks you at full speed, because nothing can
            // stop it down there. Up, it is committed to where it came up.
            m_position += direction * speed * (m_up ? 0.55f : 1.0f) * deltaTime;
            break;



        case EnemyBehaviour::Serpent:
        {
            // A long turning circle, so the body it is towing sweeps the water
            // behind it. A worm that could turn on the spot would have no body
            // worth avoiding.
            const float turn = config::SerpentTurn * deltaTime;
            const sf::Angle wanted = direction.angle();
            const sf::Angle have = m_heading.angle();
            const float error = sf::degrees(std::fmod(
                (wanted - have).asDegrees() + 540.0f, 360.0f) - 180.0f).asDegrees();

            m_heading = m_heading.rotatedBy(
                sf::degrees(std::clamp(error, -turn, turn)));
            m_position += m_heading * speed * deltaTime;
            break;
        }

        case EnemyBehaviour::Brood:
        case EnemyBehaviour::Drift:
            // Creeps forward and does not care where you are. It is a hazard
            // you have to remove, not one you can walk away from.
            m_position += direction * speed * deltaTime;
            break;
    }
}

void Enemy::update(float deltaTime, sf::Vector2f playerPosition, sf::Vector2f playerVelocity,
                   const Arena& arena)
{
    // Its own clock, which is what everything it does with its body is read
    // off. Cold water slows the beat along with everything else, so a chilled
    // creature does not swim at half speed while its tail keeps full time.
    m_age += deltaTime * (1.0f - m_chillStrength);

    // Where to swim, as opposed to where the sub is. Straight at it when the
    // water is open, and along the room graph when it is not.
    //
    // Some of them do not swim at the sub at all. They swim at where the sub
    // is going to be, which is the difference between a thing that trails you
    // down a passage and a thing that is waiting at the far end of it. How far
    // ahead is per creature, and it is capped by how long it would take to
    // arrive - aiming a second ahead of something you cannot reach for five is
    // not cutting anyone off, it is just swimming at the wrong place.
    m_navTimer -= deltaTime;
    if (m_navTimer <= 0.0f)
    {
        // Tighter jitter for the ones that lead, because a stale intercept
        // point is worse than a stale chase: it aims at a corner the sub
        // turned away from half a second ago.
        const bool cuts = m_stats->lead > 0.0f;
        m_navTimer = config::NavInterval
                     * (cuts ? rng::range(0.4f, 0.7f) : rng::range(0.75f, 1.25f));

        sf::Vector2f wanted = playerPosition;

        if (cuts)
        {
            const float speed = std::max(1.0f, m_stats->speed * m_scaling.speed);
            const float reach = std::min(m_stats->lead,
                                         (playerPosition - m_position).length() / speed);
            const sf::Vector2f ahead = playerPosition + playerVelocity * reach;

            // Only if it is somewhere it could actually be. An intercept point
            // inside rock sends it grinding at a wall, which is the exact
            // failure the routing was built to end.
            if (arena.isOpen(ahead, getRadius()))
                wanted = ahead;
        }

        m_navTarget = arena.waypointToward(m_position, wanted, getRadius());
    }

    if (m_chillTimer > 0.0f)
    {
        m_chillTimer -= deltaTime;
        if (m_chillTimer <= 0.0f)
        {
            m_chillStrength = 0.0f;
            m_sprite.setColor(sf::Color::White);
        }
    }

    // Chilled water slows everything the creature does, not only how fast it
    // swims: its guns wind down with it.
    const float chill = 1.0f - m_chillStrength;
    deltaTime *= chill;

    if (m_fireTimer > 0.0f)
        m_fireTimer -= deltaTime;

    const sf::Vector2f toPlayer = playerPosition - m_position;
    const float distance = toPlayer.length();

    // It has lost the sub. Anything still swimming in from the dark keeps
    // coming, because it was already committed; anything inside the basin
    // wanders on a slowly turning heading and holds its fire. This is the whole
    // of what running dark buys the player.
    if (!m_aware && m_hasEntered)
    {
        m_wanderAngle += rng::range(-config::WanderTurnRate, config::WanderTurnRate) * deltaTime;

        const sf::Vector2f drift{ std::cos(m_wanderAngle * 3.14159265f / 180.0f),
                                  std::sin(m_wanderAngle * 3.14159265f / 180.0f) };

        // It has lost the sub, not forgotten it. Mostly aimless, but leaning
        // along the route towards it, because it is hunting by smell now
        // rather than by sight - and because a creature that truly wanders can
        // be left on the far side of a basin this size for the rest of a
        // level, which turns "kill everything" into "go and find everything".
        const sf::Vector2f route = m_navTarget - m_position;
        const sf::Vector2f lean = route.lengthSquared() > 1.0f
                                      ? route.normalized()
                                      : sf::Vector2f{ 0.0f, 0.0f };

        sf::Vector2f heading = drift + lean * config::WanderLean;
        heading = heading.lengthSquared() > 0.0f ? heading.normalized() : drift;

        m_position += heading * m_stats->speed * m_scaling.speed
                      * config::WanderSpeedFactor * deltaTime;

        m_aimDirection = heading;
        m_facing = heading.angle();
        m_sprite.setPosition(m_position);
        m_playerDistance = distance;
        m_inFiringRange = false;

        if (m_hitFlashTimer > 0.0f)
            m_hitFlashTimer = std::max(m_hitFlashTimer - deltaTime, 0.0f);

        applyHitFlash();
        return;
    }

    // The ones that burrow, blink or hold a shield run their own clock before
    // anything else: what they do this frame depends on where they are in it.
    switch (m_stats->behaviour)
    {
        case EnemyBehaviour::Burrow:
        {
            m_burstTimer -= deltaTime;
            if (m_burstTimer <= 0.0f)
            {
                m_up = !m_up;
                m_burstTimer = m_up ? m_stats->burstOn : m_stats->burstOff;

                // Coming up, it comes up beside you rather than under you.
                // Under you is not a fight, it is a mugging.
                if (m_up && distance > 1.0f)
                {
                    const sf::Angle where = sf::degrees(rng::range(0.0f, 360.0f));
                    m_position = playerPosition
                                 + sf::Vector2f(config::RevenantSurfaceDistance, where);
                    m_warped = true;
                }
            }

            const float target = m_up ? 1.0f : 0.0f;
            m_surfaced += std::clamp(target - m_surfaced,
                                     -config::RevenantFade * deltaTime,
                                     config::RevenantFade * deltaTime);
            break;
        }


        default:
            break;
    }

    if (m_auraTimer > 0.0f)
        m_auraTimer -= deltaTime;

    // Attached, it has nowhere to swim to. The hull carries it, and Game puts
    // it where the hull is.
    if (m_latched)
    {
        m_facing = m_aimDirection.angle();
        m_sprite.setPosition(m_position);
        m_playerDistance = distance;

        if (m_hitFlashTimer > 0.0f)
            m_hitFlashTimer = std::max(m_hitFlashTimer - deltaTime, 0.0f);

        applyHitFlash();
        return;
    }

    if (distance > 0.0f)
    {
        m_aimDirection = toPlayer / distance;

        // It aims at the sub and it swims at the route. When the two are the
        // same thing - which is most of the time - nothing changes; when they
        // are not, it goes round the rock instead of into it.
        const sf::Vector2f route = m_navTarget - m_position;
        const float travel = route.length();

        if (travel > 1.0f)
            move(deltaTime, route, travel);
        else
            move(deltaTime, toPlayer, distance);

        // A shield is only a shield if it is pointing somewhere, so these two
        // face where they are actually swimming rather than at the player.
        if (m_stats->behaviour == EnemyBehaviour::Bulwark
            || m_stats->behaviour == EnemyBehaviour::Serpent)
        {
            m_aimDirection = m_heading;
        }

        // Ring firers have no facing worth showing, so they just rotate.
        if (m_stats->behaviour == EnemyBehaviour::Drift)
        {
            m_spin += config::BloomSpinSpeed * deltaTime;
            m_facing = sf::degrees(m_spin);
        }
        else
        {
            m_facing = m_aimDirection.angle();
        }

        m_inFiringRange = m_hasEntered
                          && distance < m_stats->preferredDistance * 1.8f;
    }

    m_sprite.setPosition(m_position);
    m_playerDistance = distance;

    if (m_broodTimer > 0.0f)
        m_broodTimer -= deltaTime;

    if (m_hitFlashTimer > 0.0f)
        m_hitFlashTimer = std::max(m_hitFlashTimer - deltaTime, 0.0f);

    // Parasite keeps working between shots. Ticking rather than draining every
    // frame means a bleeding creature visibly flinches on a rhythm.
    if (m_bleedTimer > 0.0f)
    {
        m_bleedTimer -= deltaTime;
        m_bleedTick -= deltaTime;

        if (m_bleedTick <= 0.0f)
        {
            m_bleedTick = config::BleedTickInterval;
            m_health -= std::max(1, static_cast<int>(std::lround(
                                        m_bleedRate * config::BleedTickInterval)));
            m_hitFlashTimer = config::EnemyHitFlashDuration * 0.5f;
        }
    }

    updateBody(deltaTime);
    applyHitFlash();
}

std::vector<EnemyType> Enemy::takeBrood()
{
    if (m_stats->broodCount <= 0 || !m_hasEntered || m_broodTimer > 0.0f)
        return {};

    m_broodTimer = m_stats->broodInterval;
    return std::vector<EnemyType>(static_cast<std::size_t>(m_stats->broodCount),
                                  m_stats->broodType);
}

int Enemy::getDeathShots() const
{
    return m_stats->deathShots;
}

std::vector<sf::Vector2f> Enemy::takeShots()
{
    // A creature that has lost the sub does not shoot into the dark on the off
    // chance, which is what makes going quiet worth doing.
    if (m_stats->fireCooldown <= 0.0f || m_fireTimer > 0.0f || !m_hasEntered || !m_aware)
        return {};

    const bool ring = m_stats->shotSpread >= 360.0f;

    // Aimed weapons need the player in range; a ring does not care.
    if (!ring && !m_inFiringRange)
        return {};

    m_fireTimer = m_stats->fireCooldown;

    const int count = std::max(1, m_stats->shotCount);
    std::vector<sf::Vector2f> shots;
    shots.reserve(static_cast<std::size_t>(count));

    if (count == 1)
    {
        shots.push_back(m_aimDirection);
        return shots;
    }

    // A full ring wraps, so the last shot must not land on top of the first.
    const float step = m_stats->shotSpread / static_cast<float>(ring ? count : count - 1);
    const float start = ring ? 0.0f : -m_stats->shotSpread / 2.0f;
    const sf::Angle base = m_aimDirection.angle();

    for (int i = 0; i < count; ++i)
    {
        const sf::Angle angle = base + sf::degrees(start + step * static_cast<float>(i));
        shots.emplace_back(std::cos(angle.asRadians()), std::sin(angle.asRadians()));
    }

    return shots;
}

// ---------------------------------------------------------------------------
//  Below the campaign
//  Six rules rather than six sets of bigger numbers. Each one is answered by
//  doing something other than pointing at the creature and holding the button.
// ---------------------------------------------------------------------------

bool Enemy::deflects(sf::Vector2f from) const
{
    if (m_stats->behaviour != EnemyBehaviour::Bulwark || !isAlive())
        return false;

    const sf::Vector2f offset = from - m_position;
    if (offset.lengthSquared() <= 0.0001f)
        return false;

    // The shield is where it is looking. Anything arriving from inside that
    // wedge is turned away; anything from behind it lands as normal, which is
    // the whole of the fight.
    const float cosine = offset.normalized().dot(m_aimDirection);
    return cosine >= std::cos(config::BulwarkArc * 3.14159265f / 180.0f);
}

bool Enemy::isVulnerable() const
{
    // Half surfaced is still surfaced: the window has to be generous enough to
    // be worth reacting to rather than a frame-perfect punish.
    return m_stats->behaviour != EnemyBehaviour::Burrow || m_surfaced > 0.35f;
}

void Enemy::latchTo(sf::Vector2f hull)
{
    // Called both to take hold and every frame afterwards: attached, it rides
    // the hull rather than swimming, so its position is simply the sub's.
    m_position = hull;

    if (m_latched)
        return;

    m_latched = true;
    m_biteTimer = config::LeechInterval;
}

void Enemy::shakeOff(sf::Vector2f away)
{
    if (!m_latched)
        return;

    m_latched = false;
    m_position += away * config::LeechThrow;

    // It will not come straight back: the throw also puts it on its back foot
    // by resetting the burst it closes with.
    m_burstTimer = 0.6f;
    m_bursting = false;
}

bool Enemy::wantsBite(float deltaTime)
{
    if (!m_latched || !isAlive())
        return false;

    m_biteTimer -= deltaTime;
    if (m_biteTimer > 0.0f)
        return false;

    m_biteTimer = config::LeechInterval;
    return true;
}

bool Enemy::latches() const
{
    return m_stats->latches;
}

float Enemy::getSurfaceTell() const
{
    if (m_stats->behaviour != EnemyBehaviour::Burrow || !m_up)
        return 0.0f;

    return std::clamp(m_surfaced, 0.0f, 1.0f) < 0.999f ? m_surfaced : 0.0f;
}

void Enemy::setSegmentTexture(const sf::Texture& texture)
{
    m_segment = std::make_unique<sf::Sprite>(texture);
    m_segment->setOrigin(sf::Vector2f(texture.getSize()) / 2.0f);

    const float scale = config::SerpentSegmentRadius * 2.0f
                        / std::max(1.0f, static_cast<float>(texture.getSize().x));
    m_segment->setScale({ scale, scale });
}

bool Enemy::bodyTouches(sf::Vector2f position, float radius) const
{
    const float reach = config::SerpentSegmentRadius + radius;

    for (const sf::Vector2f& point : m_body)
    {
        if ((position - point).lengthSquared() <= reach * reach)
            return true;
    }

    return false;
}

void Enemy::updateBody(float deltaTime)
{
    (void)deltaTime;

    if (m_stats->behaviour != EnemyBehaviour::Serpent)
        return;

    // A dense record of where the head has actually been, then the rings
    // picked off it at fixed spacing. Springs were tried first and the body
    // cut every corner the head swam round, which made the thing look like a
    // chain of balls rather than an animal.
    if (m_history.empty() || (m_history.front() - m_position).lengthSquared() > 16.0f)
        m_history.insert(m_history.begin(), m_position);

    const std::size_t needed = static_cast<std::size_t>(
        config::SerpentSegments * config::SerpentSpacing / 4.0f + 40.0f);

    if (m_history.size() > needed)
        m_history.resize(needed);

    m_body.clear();

    float wanted = config::SerpentSpacing;
    float walked = 0.0f;

    for (std::size_t i = 1; i < m_history.size() && m_body.size() < static_cast<std::size_t>(config::SerpentSegments); ++i)
    {
        const sf::Vector2f step = m_history[i] - m_history[i - 1];
        const float length = step.length();
        if (length <= 0.0001f)
            continue;

        while (walked + length >= wanted
               && m_body.size() < static_cast<std::size_t>(config::SerpentSegments))
        {
            const float t = (wanted - walked) / length;
            m_body.push_back(m_history[i - 1] + step * t);
            wanted += config::SerpentSpacing;
        }

        walked += length;
    }

    // Still growing: anything that has not been laid down yet stacks up behind
    // the head rather than not existing, so a freshly spawned worm is a worm.
    while (m_body.size() < static_cast<std::size_t>(config::SerpentSegments))
    {
        const sf::Vector2f last = m_body.empty() ? m_position : m_body.back();
        m_body.push_back(last - m_heading * config::SerpentSpacing);
    }
}

void Enemy::confine(const Arena& arena, float deltaTime)
{
    // Dragged by whatever the water is doing here, but not as hard as the sub
    // is. They live down here and it does not: a creature that could be pinned
    // at the wrong end of a flowing passage would simply never arrive, and a
    // level that asks you to clear the water would stop being finishable.
    if (m_hasEntered)
        m_position += arena.currentAt(m_position) * config::CurrentOnCreatures * deltaTime;

    // The basin is cut out of rock now, so "inside" is a question about the
    // map. A creature still swimming in is left alone; once it is in the water
    // it stays in the water, which is what stops anything gliding through a
    // wall to reach the sub.
    if (arena.isOpen(m_position, getRadius()))
    {
        m_hasEntered = true;
        m_sprite.setPosition(m_position);
        return;
    }

    if (!m_hasEntered)
        return; // still crossing the rock from wherever it was spawned

    m_position = arena.clampInside(m_position, getRadius());
    m_sprite.setPosition(m_position);
}

void Enemy::remember()
{
    m_prevPosition = m_position;
    m_warped = false;
}

void Enemy::present(float alpha)
{
    // A creature that surfaced or blinked did not swim there, so it is drawn
    // where it is and nowhere in between - interpolating that would send it
    // sailing through the rock it came out of.
    sf::Vector2f at = m_warped
                          ? m_position
                          : m_prevPosition + (m_position - m_prevPosition) * alpha;

    // And then the part that makes it an animal rather than a decal. Every
    // number here comes out of the behaviour table, every one of them is read
    // only while the picture is being made, and not one of them is visible to
    // anything that decides what happens: the position it was hit at, the
    // radius it is hit on and the way it is pointed are all exactly where the
    // step left them.
    const config::Motion& motion = config::motionFor(m_stats->behaviour);
    const float strength = config::MotionAmount;

    if (strength > 0.0f)
    {
        constexpr float Tau = 6.283185307f;

        const float beat = m_phase + m_age * Tau;

        // The tail. Yawing off the heading rather than turning the creature,
        // so a shield still faces where it is walking.
        const float sway = motion.sway * strength * std::sin(beat * motion.swayHz);

        // The body. Swelling across itself and pinching along, which is what a
        // thing that swims by squeezing water actually does.
        const float swell = motion.pulse * strength * std::sin(beat * motion.pulseHz);

        // And the whole animal riding a little across its own line, so two of
        // them side by side are not one shape drawn twice.
        at += sf::Vector2f{ -std::sin(m_facing.asRadians()), std::cos(m_facing.asRadians()) }
              * (motion.bob * strength * std::sin(beat * motion.bobHz));

        m_sprite.setRotation(m_facing + sf::degrees(sway));
        m_sprite.setScale({ m_bodyScale * (1.0f - swell * 0.6f),
                            m_bodyScale * (1.0f + swell) });
    }
    else
    {
        m_sprite.setRotation(m_facing);
        m_sprite.setScale({ m_bodyScale, m_bodyScale });
    }

    m_sprite.setPosition(at);
}

sf::Vector2f Enemy::getMuzzlePosition() const
{
    return m_position + m_aimDirection * (m_stats->radius + 8.0f);
}

void Enemy::applyHitFlash()
{
    // A tint alone barely shows on an already dark sprite, so a hit also punches
    // the scale for a moment. That reads instantly at any size.
    const float strength = m_hitFlashTimer / config::EnemyHitFlashDuration;

    // Handed to the picture rather than to the sprite, because the picture has
    // a beat of its own to fold in and the two have to agree on one number.
    m_bodyScale = m_stats->scale * (1.0f + config::EnemyHitFlashPunch * strength);

    sf::Color tint = strength > 0.0f ? config::EnemyHitFlashTint
                     : m_chillTimer > 0.0f ? config::ChillTint
                                           : sf::Color::White;

    // A stealth creature is only a shape in the dark until it is inside the
    // lamp. Being hit gives it away, which is the one tell the player gets.
    if (m_stats->stealth && strength <= 0.0f && m_playerDistance > config::StealthRevealRange)
        tint.a = config::StealthAlpha;

    // A burrower dissolves into the floor rather than blinking out of it, so
    // that coming up reads as coming up and the window is visibly opening.
    if (m_stats->behaviour == EnemyBehaviour::Burrow)
    {
        tint.a = static_cast<std::uint8_t>(
            static_cast<float>(tint.a) * (0.12f + 0.88f * m_surfaced));
    }

    // And then however much light is actually falling on it. The darkness was
    // painted over the water but not over the things in it, so a creature
    // three rooms away in the pitch black was as bright and as legible as one
    // in the beam - which is most of the reason running dark never felt like
    // anything. Multiplied into the tint rather than replacing it, so a hit
    // flash still reads on something barely lit.
    const float lit = std::clamp(m_lit, 0.0f, 1.0f);

    tint.r = static_cast<std::uint8_t>(static_cast<float>(tint.r) * lit);
    tint.g = static_cast<std::uint8_t>(static_cast<float>(tint.g) * lit);
    tint.b = static_cast<std::uint8_t>(static_cast<float>(tint.b) * lit);

    m_sprite.setColor(tint);
}

void Enemy::applyBleed(float damagePerSecond, float seconds)
{
    // Refreshed rather than stacked: reapplying keeps the wound open at the
    // strongest rate seen so far instead of multiplying into nonsense.
    m_bleedRate = std::max(m_bleedRate, damagePerSecond);
    m_bleedTimer = std::max(m_bleedTimer, seconds);
}

void Enemy::takeDamage(int amount)
{
    // A herald does not heal what it is standing over, it makes it harder to
    // hurt in the first place. Rounded up, so nothing ever becomes immune to a
    // small weapon.
    if (m_auraTimer > 0.0f)
        amount = std::max(1, static_cast<int>(std::lround(
                                 static_cast<float>(amount) * (1.0f - config::HeraldArmour))));

    m_health -= amount;
    m_hitFlashTimer = config::EnemyHitFlashDuration;
}

void Enemy::applyChill(float strength, float seconds)
{
    // Never all the way to a stop: a frozen creature you can walk away from is
    // not a fight, and it would make the chill line strictly better than every
    // other way of dealing with something.
    m_chillStrength = std::clamp(std::max(m_chillStrength, strength), 0.0f,
                                 config::MaxChillStrength);
    m_chillTimer = std::max(m_chillTimer, seconds);

    // It has to be visible, or a player cannot tell a chilled creature from one
    // that simply has not noticed them yet.
    m_sprite.setColor(config::ChillTint);
}

float Enemy::getHealthFraction() const
{
    if (m_maxHealth <= 0)
        return 0.0f;

    return std::clamp(static_cast<float>(m_health) / static_cast<float>(m_maxHealth),
                      0.0f, 1.0f);
}

void Enemy::draw(sf::RenderTarget& target) const
{
    // The body goes down before the head, tail first, so the rings overlap the
    // way a body actually does and the head sits on top of its own neck.
    if (m_segment)
    {
        const sf::Color tint = m_sprite.getColor();

        for (std::size_t i = m_body.size(); i-- > 0;)
        {
            sf::Sprite ring = *m_segment;
            ring.setPosition(m_body[i]);

            // Tapering to the tail, so it ends rather than stopping.
            const float taper = 1.0f - 0.45f * static_cast<float>(i)
                                           / static_cast<float>(config::SerpentSegments);
            ring.setScale(m_segment->getScale() * taper);
            ring.setColor(tint);
            target.draw(ring);
        }
    }

    target.draw(m_sprite);
}
