#include "entities/Projectile.h"

#include "core/Random.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float Tau = 6.283185307f;
}

Projectile::Projectile(const sf::Texture& texture, sf::Vector2f position, sf::Vector2f direction,
                       const config::ProjectileStats& stats, int damage, int pierce)
    : m_sprite(texture)
    , m_position(position)
    , m_direction(direction)
    , m_speed(stats.speed)
    , m_radius(stats.radius)
    , m_damage(std::max(1, damage))
    , m_pierce(std::max(0, pierce))
    , m_bounces(0)
    , m_ignore(0)
    , m_scale(stats.scale)
    , m_phase(rng::range(0.0f, Tau))
    , m_throb(stats.throb)
    , m_throbHz(stats.throbHz)
    , m_roll(stats.roll)
    , m_rollHz(stats.rollHz)
    , m_alive(true)
{
    m_sprite.setOrigin(sf::Vector2f(texture.getSize()) / 2.0f);
    m_sprite.setScale({ stats.scale, stats.scale });
    // Every shot texture is drawn pointing right, so the sprite angle is just
    // the travel direction.
    m_sprite.setRotation(direction.angle());
    m_sprite.setPosition(m_position);
}

void Projectile::update(float deltaTime)
{
    m_age += deltaTime;
    m_position += m_direction * m_speed * deltaTime;
    m_sprite.setPosition(m_position);
}

void Projectile::steerTowards(sf::Vector2f target, float turnRateDegrees, float deltaTime)
{
    const sf::Vector2f delta = target - m_position;
    if (delta.lengthSquared() <= 0.0001f)
        return;

    const float error = (delta.angle() - m_direction.angle()).wrapSigned().asDegrees();
    const float allowed = turnRateDegrees * deltaTime;

    m_direction = m_direction.rotatedBy(sf::degrees(std::clamp(error, -allowed, allowed)));
    m_sprite.setRotation(m_direction.angle());
}

bool Projectile::spendPierce()
{
    if (m_pierce <= 0)
    {
        m_alive = false;
        return true;
    }

    --m_pierce;
    return false;
}

bool Projectile::spendBounce(sf::Vector2f normal)
{
    if (m_bounces <= 0 || normal.lengthSquared() <= 0.0f)
        return false;

    --m_bounces;

    // Mirror the heading about the wall, then step off it so the shot cannot
    // be caught by the same boundary test on the very next frame.
    const sf::Vector2f n = normal.normalized();
    m_direction = m_direction - n * (2.0f * m_direction.dot(n));
    m_position += n * (m_radius + 2.0f);

    m_sprite.setRotation(m_direction.angle());
    m_sprite.setPosition(m_position);
    return true;
}

void Projectile::setScale(float scale)
{
    m_scale = scale;
    m_sprite.setScale({ scale, scale });
}

void Projectile::present(float alpha)
{
    m_sprite.setPosition(m_prevPosition + (m_position - m_prevPosition) * alpha);

    // What follows is the whole difference between a shot and a picture of a
    // shot being slid along a line. Nothing that can hurt anybody reads any of
    // it: the radius the collision code asks for has not moved a pixel.
    if (m_throb > 0.0f)
    {
        const float beat = std::sin(m_phase + m_age * m_throbHz * Tau);

        // Swelling across the shot and pinching along it, so a jet reads as a
        // column of water under pressure rather than as a balloon.
        m_sprite.setScale({ m_scale * (1.0f - m_throb * 0.45f * beat),
                            m_scale * (1.0f + m_throb * beat) });
    }

    if (m_roll > 0.0f)
    {
        const float wander = std::sin(m_phase * 1.7f + m_age * m_rollHz * Tau);
        m_sprite.setRotation(m_direction.angle() + sf::degrees(m_roll * wander));
    }
}

void Projectile::draw(sf::RenderTarget& target) const
{
    target.draw(m_sprite);
}
