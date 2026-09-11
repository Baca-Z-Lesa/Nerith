#include "entities/Enemy.h"

#include "core/Config.h"
#include "core/Random.h"

#include <SFML/System/Angle.hpp>

#include <algorithm>
#include <cmath>

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

        case EnemyBehaviour::Dash:
        {
            // Sprint, coast, sprint. The rests are the window you get to shoot
            // it, which is the whole point of a one hit creature this fast.
            m_burstTimer -= deltaTime;
            if (m_burstTimer <= 0.0f)
            {
                m_bursting = !m_bursting;
                m_burstTimer = m_bursting ? m_stats->burstOn : m_stats->burstOff;
            }

            const float factor = m_bursting ? 1.0f : config::EnemyDashRestFactor;
            m_position += direction * speed * factor * deltaTime;
            break;
        }

        case EnemyBehaviour::Standoff:
        {
            // Hold the range. The deadzone stops it twitching once it settles.
            const float gap = distance - m_stats->preferredDistance;
            float approach = 0.0f;
            if (gap > config::SpitterRangeBand)
                approach = 1.0f;
            else if (gap < -config::SpitterRangeBand)
                approach = -1.0f;

            const sf::Vector2f strafe(-direction.y, direction.x);
            m_position += direction * approach * speed * deltaTime;
            m_position += strafe * m_strafeDirection * speed
                          * config::SpitterStrafeFactor * deltaTime;
            break;
        }

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

        case EnemyBehaviour::Brood:
        case EnemyBehaviour::Drift:
            // Creeps forward and does not care where you are. It is a hazard
            // you have to remove, not one you can walk away from.
            m_position += direction * speed * deltaTime;
            break;
    }
}

void Enemy::update(float deltaTime, sf::Vector2f playerPosition)
{
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

        const sf::Vector2f heading{ std::cos(m_wanderAngle * 3.14159265f / 180.0f),
                                    std::sin(m_wanderAngle * 3.14159265f / 180.0f) };

        m_position += heading * m_stats->speed * m_scaling.speed
                      * config::WanderSpeedFactor * deltaTime;

        m_aimDirection = heading;
        m_sprite.setRotation(heading.angle());
        m_sprite.setPosition(m_position);
        m_playerDistance = distance;
        m_inFiringRange = false;

        if (m_hitFlashTimer > 0.0f)
            m_hitFlashTimer = std::max(m_hitFlashTimer - deltaTime, 0.0f);

        applyHitFlash();
        return;
    }

    if (distance > 0.0f)
    {
        m_aimDirection = toPlayer / distance;
        move(deltaTime, toPlayer, distance);

        // Ring firers have no facing worth showing, so they just rotate.
        if (m_stats->behaviour == EnemyBehaviour::Drift)
        {
            m_spin += config::BloomSpinSpeed * deltaTime;
            m_sprite.setRotation(sf::degrees(m_spin));
        }
        else
        {
            m_sprite.setRotation(m_aimDirection.angle());
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

void Enemy::confine(sf::Vector2f center, float maxDistance)
{
    const sf::Vector2f delta = m_position - center;

    if (delta.lengthSquared() <= maxDistance * maxDistance)
    {
        m_hasEntered = true; // it is inside now, so from here on it stays inside
        return;
    }

    if (!m_hasEntered)
        return; // still swimming in from off screen

    m_position = center + delta.normalized() * maxDistance;
    m_sprite.setPosition(m_position);
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
    const float scale = m_stats->scale * (1.0f + config::EnemyHitFlashPunch * strength);

    m_sprite.setScale({ scale, scale });

    sf::Color tint = strength > 0.0f ? config::EnemyHitFlashTint
                     : m_chillTimer > 0.0f ? config::ChillTint
                                           : sf::Color::White;

    // A stealth creature is only a shape in the dark until it is inside the
    // lamp. Being hit gives it away, which is the one tell the player gets.
    if (m_stats->stealth && strength <= 0.0f && m_playerDistance > config::StealthRevealRange)
        tint.a = config::StealthAlpha;

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
    target.draw(m_sprite);
}
