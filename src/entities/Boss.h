#pragma once

#include "entities/BossDef.h"
#include "entities/EnemyType.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <string>
#include <vector>

struct Assets;

struct BossShot
{
    sf::Vector2f position;
    sf::Vector2f direction;
};

// One animal. Its body is a head towing segments, or a bell trailing tentacles;
// its parts are eyes, fins, tentacles, plating and organs that all sit on that
// body. Breaking a part does not just remove a health bar, it takes something
// away from the creature: its aim, its speed, one of its arms, or the shell
// over the thing keeping it alive.
class Boss
{
public:
    Boss(const Assets& assets, BossId id, sf::Vector2f position, const EnemyScaling& scaling);

    void update(float deltaTime, sf::Vector2f playerPosition);
    void draw(sf::RenderTarget& target) const;

    struct Hit
    {
        bool hit = false;
        bool blocked = false;       // landed on hide or plating and did nothing
        bool destroyedPart = false;
        bool killedBoss = false;
        bool wasOrgan = false;
        std::size_t partIndex = 0;
        sf::Vector2f at{};
    };

    // weakPointBonus is added as a multiplier when the shot lands on an organ.
    Hit tryHit(sf::Vector2f position, float radius, int damage, float weakPointBonus);

    // Chain Reaction: hurt whatever is bolted next to a part that just died.
    void splashNeighbours(std::size_t partIndex, int damage);

    std::vector<BossShot> takeShots();
    void confine(sf::Vector2f center, float maxDistance);

    // True when any part of the body is close enough to maul the player.
    bool touches(sf::Vector2f position, float radius) const;

    sf::Vector2f getPosition() const;
    float getRadius() const;
    int getContactDamage() const { return m_contactDamage; }
    int getSalvage() const;
    bool isAlive() const;
    bool isVulnerable() const; // false while it is under the silt
    float getCoreFraction() const;

    const char* getName() const;
    const char* getBrief() const;
    std::string getStateLabel() const;

    std::size_t getPartCount() const { return m_parts.size(); }
    bool isPartAlive(std::size_t index) const;
    float getPartFraction(std::size_t index) const;
    PartRole getPartRole(std::size_t index) const;
    sf::Vector2f getPartPosition(std::size_t index) const;

private:
    // What the creature is doing right now. Not every mind uses every one.
    enum class Phase
    {
        Stalk,     // sizing you up
        Wind,      // committed, about to move
        Strike,    // the charge or the burst
        Recover,   // spent, slow, open
        Flee,      // hurt badly enough to back off
        Submerged, // under the silt, untouchable
        Rising,    // coming back up
    };

    struct Segment
    {
        sf::Vector2f position;
        sf::Angle    angle;
        float        scale;
        sf::Sprite   sprite;
    };

    struct Part
    {
        BossPartDef  def;
        int          health;
        int          maxHealth;
        float        flash;
        bool         alive;
        sf::Vector2f anchor; // where the sprite sits; tentacles hang from their root
        sf::Vector2f world;
        sf::Angle    angle;
        sf::Sprite   sprite;
    };

    // Everything the surviving parts add up to. Recomputed whenever one dies,
    // so the fight changes shape rather than just losing numbers.
    struct Vitals
    {
        float tracking = 1.0f; // 1 perfect aim, 0.25 nearly blind
        float mobility = 1.0f;
        int   emitters = 0;    // living tentacles
        int   organs = 0;
    };

    void refreshVitals();
    void steer(float deltaTime, sf::Vector2f target, float turnScale);
    void advance(float deltaTime, float speed);
    void followChain();
    void placeParts(float deltaTime);
    bool organExposed(const Part& part) const;
    bool bodyBlocks(sf::Vector2f position, float radius) const;
    std::size_t nearestExposedOrgan(sf::Vector2f position) const;
    float aimError() const;

    void thinkProwler(float deltaTime, sf::Vector2f playerPosition);
    void thinkDrifter(float deltaTime, sf::Vector2f playerPosition);
    void thinkBurrower(float deltaTime, sf::Vector2f playerPosition);

    const BossDef* m_def;
    BossId         m_id;

    std::vector<Segment> m_segments;
    std::vector<Part>    m_parts;
    Vitals               m_vitals;
    std::vector<BossShot> m_pending; // filled by the think code, drained by takeShots

    sf::Vector2f m_heading;
    sf::Vector2f m_playerPosition;
    sf::Vector2f m_strikeTarget;

    Phase m_phase;
    float m_phaseTimer;
    float m_attackTimer;
    float m_time;
    float m_bellPulse;    // Bell body: 0 relaxed, 1 clenched
    float m_visibility;   // 0 fully buried, 1 fully surfaced
    float m_strafe;       // +1 or -1
    bool  m_rhythmOpen;   // Bell body: the nucleus is showing
    bool  m_enraged;
    bool  m_hasEntered;
    bool  m_dead;

    int   m_contactDamage;
    int   m_salvage;
    float m_speed;
    int   m_startingOrganHealth;
};
