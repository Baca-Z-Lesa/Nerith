#pragma once

#include "entities/EnemyType.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>

#include <vector>

namespace config { struct EnemyStats; }

// One hostile creature. Every kind shares this class because they differ only
// in the numbers from the config table plus which movement branch they take.
class Enemy
{
public:
    Enemy(EnemyType type, const sf::Texture& texture, sf::Vector2f position,
          const EnemyScaling& scaling);

    void update(float deltaTime, sf::Vector2f playerPosition);
    void draw(sf::RenderTarget& target) const;

    void takeDamage(int amount);

    // Parasite: a wound that keeps working after the shot has gone.
    void applyBleed(float damagePerSecond, float seconds);
    bool isBleeding() const { return m_bleedTimer > 0.0f; }

    // Directions this creature wants to fire along right now, empty when it is
    // not firing. One call covers aimed shots, shotguns and full rings alike.
    std::vector<sf::Vector2f> takeShots();

    // Minions this creature wants hatched right now, empty otherwise.
    std::vector<EnemyType> takeBrood();

    int getDeathShots() const;

    // Hatchlings are born inside the arena, so they skip the swim in.
    // Hatchlings are born inside the arena, so they skip the swim in. They also
    // do not count toward a level's roster: only what the script sent does.
    void markHatched() { m_hasEntered = true; m_scripted = false; }
    bool isScripted() const { return m_scripted; }

    // Stops a creature that has already swum in from wandering back off screen
    // and sniping out of the dark.
    void confine(sf::Vector2f center, float maxDistance);

    EnemyType getType() const { return m_type; }
    sf::Vector2f getPosition() const { return m_position; }
    sf::Vector2f getMuzzlePosition() const;
    float getRadius() const;
    int getContactDamage() const;
    int getSalvage() const;
    bool isAlive() const { return m_health > 0; }

private:
    void move(float deltaTime, sf::Vector2f toPlayer, float distance);
    void applyHitFlash();

    EnemyType                 m_type;
    const config::EnemyStats* m_stats;
    EnemyScaling              m_scaling;

    sf::Sprite   m_sprite;
    sf::Vector2f m_position;
    sf::Vector2f m_aimDirection;
    int          m_health;
    float        m_hitFlashTimer;   // seconds of impact feedback left
    float        m_bleedTimer;
    float        m_bleedTick;
    float        m_bleedRate;
    float        m_fireTimer;
    float        m_strafeDirection; // +1 or -1, so a group does not orbit in lockstep
    float        m_burstTimer;      // Dash only
    bool         m_bursting;
    float        m_spin;            // Drift only
    sf::Vector2f m_lanceHeading;    // Lance only: locked when the charge starts
    float        m_broodTimer;
    float        m_playerDistance;  // kept for the stealth fade
    bool         m_inFiringRange;
    bool         m_hasEntered;      // has it crossed into the lit basin yet
    bool         m_scripted;        // false once it was hatched rather than sent
};
