#include "entities/Projectile.h"

#include <algorithm>

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
    , m_scale(stats.scale)
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

void Projectile::draw(sf::RenderTarget& target) const
{
    target.draw(m_sprite);
}
