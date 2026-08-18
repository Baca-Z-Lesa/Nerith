#include "entities/Enemy.h"

#include "core/Config.h"
#include "core/Random.h"

#include <SFML/System/Angle.hpp>

#include <algorithm>
#include <cmath>

Enemy::Enemy(EnemyType type, const sf::Texture& texture, sf::Vector2f position,
             const EnemyScaling& scaling)
    : m_type(type)
    , m_stats(&config::enemyStats(type))
    , m_scaling(scaling)
    , m_sprite(texture)
    , m_position(position)
    , m_aimDirection(1.0f, 0.0f)
    , m_health(std::max(1, static_cast<int>(std::lround(
          static_cast<float>(config::enemyStats(type).health) * scaling.health))))
    , m_hitFlashTimer(0.0f)
    , m_bleedTimer(0.0f)
    , m_bleedTick(0.0f)
    , m_bleedRate(0.0f)
    , m_fireTimer(rng::range(0.0f, std::max(0.0f, config::enemyStats(type).fireCooldown)))
    , m_strafeDirection(rng::range(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f)
    , m_burstTimer(rng::range(0.0f, 0.4f))
    , m_bursting(true)
    , m_spin(rng::range(0.0f, 360.0f))
    , m_inFiringRange(false)
    , m_hasEntered(false)
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

        case EnemyBehaviour::Drift:
            // Creeps forward and does not care where you are. It is a hazard
            // you have to remove, not one you can walk away from.
            m_position += direction * speed * deltaTime;
            break;
    }
}

void Enemy::update(float deltaTime, sf::Vector2f playerPosition)
{
    if (m_fireTimer > 0.0f)
        m_fireTimer -= deltaTime;

    const sf::Vector2f toPlayer = playerPosition - m_position;
    const float distance = toPlayer.length();

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

std::vector<sf::Vector2f> Enemy::takeShots()
{
    if (m_stats->fireCooldown <= 0.0f || m_fireTimer > 0.0f || !m_hasEntered)
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
    m_sprite.setColor(strength > 0.0f ? config::EnemyHitFlashTint : sf::Color::White);
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

void Enemy::draw(sf::RenderTarget& target) const
{
    target.draw(m_sprite);
}
