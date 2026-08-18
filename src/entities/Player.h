#pragma once

#include "entities/ShipVisuals.h"
#include "upgrades/Stats.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Vector2.hpp>

class Arena;
struct Assets;

// The submarine the user controls: WASD moves it, the mouse aims it. Every
// number it uses is read through the stat block and every extra system it has
// is switched on by a trait, so an upgrade changes how it plays without this
// class ever learning that upgrades exist.
class Player
{
public:
    Player(const Assets& assets, const Stats& stats);

    void update(float deltaTime, sf::Vector2f aimTarget, const Arena& arena);
    void draw(sf::RenderTarget& target) const;

    // Puts the sub back at the centre at full health, for a restart.
    void reset();

    // Call after the stat block changes: a bigger tank arrives full.
    void onStatsChanged();

    // Per level bookkeeping, such as rearming the emergency vent.
    void onLevelStart();

    bool tryFire();
    bool tryFireTorpedo();
    bool tryDash();
    bool isDashing() const { return m_dashTimer > 0.0f; }

    struct DamageResult
    {
        bool applied = false;     // health actually went down
        bool shieldBroke = false; // a charge ate it instead
        bool ventSaved = false;   // the emergency vent caught a fatal hit
    };

    DamageResult takeDamage(int amount);

    sf::Vector2f getPosition() const { return m_position; }
    sf::Vector2f getAimDirection() const { return m_aimDirection; }
    sf::Vector2f getMuzzlePosition() const;
    sf::Vector2f getSternPosition() const;

    int  getHealth() const { return m_health; }
    int  getMaxHealth() const;
    int  getShieldCharges() const { return m_shieldCharges; }
    int  getMaxShieldCharges() const;
    float getDashCooldownFraction() const;
    bool isAlive() const { return m_health > 0; }
    bool isMoving() const { return m_moving; }

private:
    // Reads WASD and returns a direction of length 0 or 1.
    static sf::Vector2f readMovementInput();

    void aimAt(sf::Vector2f target);
    void updateInvulnerability(float deltaTime);
    void updateShield(float deltaTime);

    const Stats* m_stats;

    sf::Sprite   m_sprite;
    ShipVisuals  m_visuals;

    sf::Vector2f m_position;
    sf::Vector2f m_aimDirection; // unit vector the nose points along
    sf::Vector2f m_dashDirection;

    float m_fireTimer;
    float m_torpedoTimer;
    float m_dashTimer;
    float m_dashCooldown;
    float m_shieldTimer;

    int   m_health;
    int   m_lastMaxHealth;
    int   m_shieldCharges;
    float m_invulnerabilityTimer;
    bool  m_moving;
    bool  m_ventReady;
};
