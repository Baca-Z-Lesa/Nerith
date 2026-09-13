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

    // See Enemy: the picture sits between two fixed steps.
    void remember() { m_prevPosition = m_position; }
    void present(float alpha);

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

    // The one creature this shot may not hit. Fragments are born inside
    // whatever was just hit, so without this every one of them lands on that
    // same creature on the next frame and deletes it - which made splitshot
    // and the cluster torpedo into instant kills rather than into spray.
    void setIgnore(int enemyId) { m_ignore = enemyId; }

    // The last two things it landed on, so a shot with pierce left cannot
    // spend all of it on the same body. A boss is one enormous target made of
    // parts: without this a piercing shot sitting inside it landed again on
    // every step, which at a hundred and twenty steps a second is not piercing,
    // it is a blender. Two rather than one, because two parts side by side
    // would otherwise pass it back and forth.
    bool alreadyHit(int token) const { return m_recent[0] == token || m_recent[1] == token; }
    void markHit(int token) { m_recent[m_recentNext] = token; m_recentNext ^= 1; }
    int getIgnore() const { return m_ignore; }

    bool isAlive() const { return m_alive; }
    void kill() { m_alive = false; }

private:
    sf::Sprite   m_sprite;
    sf::Vector2f m_position;
    sf::Vector2f m_prevPosition;
    sf::Vector2f m_direction;
    float        m_speed;
    float        m_radius;
    int          m_damage;
    int          m_pierce;
    int          m_bounces;
    int          m_ignore;
    int          m_recent[2] = { -9999, -9998 };
    int          m_recentNext = 0;
    float        m_scale;

    // A shot that is in the water rather than drawn over it. The age is the
    // clock the throb and the roll are read off, and the phase is why a ring
    // of fourteen orbs does not beat as one object.
    float        m_age = 0.0f;
    float        m_phase;
    float        m_throb;
    float        m_throbHz;
    float        m_roll;
    float        m_rollHz;

    bool         m_alive;
};
