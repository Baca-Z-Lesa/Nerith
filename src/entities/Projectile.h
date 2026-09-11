#pragma once

#include "core/Config.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

// A single shot: a jet, a fragment, a torpedo or an enemy orb. It carries its
// own damage and how many bodies it can pass through, so the same class covers
// everything that flies without the owner having to remember what it fired.
class Projectile
{
public:
    // direction must be a unit vector.
    Projectile(const sf::Texture& texture, sf::Vector2f position, sf::Vector2f direction,
               const config::ProjectileStats& stats, int damage, int pierce = 0);

    void update(float deltaTime);

    // Homing torpedoes: bend the heading towards a point, capped per second.
    void steerTowards(sf::Vector2f target, float turnRateDegrees, float deltaTime);

    void draw(sf::RenderTarget& target) const;

    sf::Vector2f getPosition() const { return m_position; }
    sf::Vector2f getDirection() const { return m_direction; }
    float getRadius() const { return m_radius; }
    int getDamage() const { return m_damage; }

    // Spends one body's worth of penetration; true once the shot is used up.
    bool spendPierce();

    // Ricochet: a shot that still has a bounce left turns rather than dying on
    // the basin wall. `normal` points back into the water.
    void setBounces(int bounces) { m_bounces = bounces; }
    bool spendBounce(sf::Vector2f normal);

    // A charged jet is bigger and brighter, so the player can see what the
    // wait bought them.
    void setScale(float scale);

    bool isAlive() const { return m_alive; }
    void kill() { m_alive = false; }

private:
    sf::Sprite   m_sprite;
    sf::Vector2f m_position;
    sf::Vector2f m_direction;
    float        m_speed;
    float        m_radius;
    int          m_damage;
    int          m_pierce;
    int          m_bounces;
    float        m_scale;
    bool         m_alive;
};
