#pragma once

#include "entities/EnemyType.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <algorithm>
#include <memory>
#include <vector>

namespace config { struct EnemyStats; }
class Arena;

// One hostile creature. Every kind shares this class because they differ only
// in the numbers from the config table plus which movement branch they take.
class Enemy
{
public:
    Enemy(EnemyType type, const sf::Texture& texture, sf::Vector2f position,
          const EnemyScaling& scaling);

    // The arena comes in because a creature has to be able to get across it.
    // Without a route it simply grinds into rock whenever the sub is round a
    // corner, and a level built on killing everything stops being finishable.
    void update(float deltaTime, sf::Vector2f playerPosition, sf::Vector2f playerVelocity,
                const Arena& arena);

    // Where it was at the end of the previous step, so the picture can sit
    // between two steps rather than snapping onto one of them.
    void remember();
    void present(float alpha);

    // How much light is falling on it, 0 for the pitch black and 1 for the
    // middle of the beam. The darkness is drawn over the water; without this
    // it is not drawn over anything IN the water.
    void setLit(float amount) { m_lit = amount; }
    void draw(sf::RenderTarget& target) const;

    void takeDamage(int amount);

    // --- the deep creatures ---
    // Warden: a shot arriving from inside its shield arc does nothing at all.
    // Asked by the collision code before any damage is handed over.
    bool deflects(sf::Vector2f from) const;

    // Revenant: false while it is under the floor, when it can neither be hit
    // nor hurt anything.
    bool isVulnerable() const;

    // Siphon: it stops swimming and holds on. Latched, it rides the hull, and
    // only a dash or its own death gets it off.
    bool latches() const;
    bool isLatched() const { return m_latched; }
    void latchTo(sf::Vector2f hull);
    void shakeOff(sf::Vector2f away);
    bool wantsBite(float deltaTime); // true once per bite interval while attached

    // Herald: refreshed every frame by whatever is near enough to be helped.
    // Held as a timer so a creature that swims out of the aura loses it rather
    // than keeping it for the rest of the level.
    void bolster(float seconds) { m_auraTimer = std::max(m_auraTimer, seconds); }
    bool isBolstered() const { return m_auraTimer > 0.0f; }

    // Rift: 0 when it is not about to move, rising to 1 as it goes.
    // How far through coming up a burrower is, 0 when it is not coming up.
    // The ring drawn off this is the only warning the player gets, and a
    // creature that arrives beside you with no warning is not a fight.
    float getSurfaceTell() const;

    // Coilworm: the rings it is towing, and whether any of them is on you.
    // Only the head can be hurt, so this is asked separately from the hit test.
    const std::vector<sf::Vector2f>& getBodyPoints() const { return m_body; }
    bool bodyTouches(sf::Vector2f position, float radius) const;
    void setSegmentTexture(const sf::Texture& texture);

    // Drawn past the last ring and nothing else: it is not a ring, so it is
    // not one of the things the body can catch you with.
    void setTailTexture(const sf::Texture& texture);

    // Parasite: a wound that keeps working after the shot has gone.
    void applyBleed(float damagePerSecond, float seconds);
    bool isBleeding() const { return m_bleedTimer > 0.0f; }

    // Chilled water: everything it does gets slower, and it shows.
    void applyChill(float strength, float seconds);
    bool isChilled() const { return m_chillTimer > 0.0f; }

    // How much of it is left, which is what an execute threshold is measured
    // against and what the sonar shows on a wounded creature.
    float getHealthFraction() const;
    void kill() { m_health = 0; }

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

    // Nothing sent it, so it must not be counted against what the level is
    // still owed. Unlike a hatchling it has not arrived yet - it swims in out
    // of the dark like anything else the water sent.
    void markUnscripted() { m_scripted = false; }

    // Whether it currently knows where the sub is. Unaware, it wanders and
    // holds its fire, which is what running dark buys the player.
    void setAware(bool aware) { m_aware = aware; }

    // The one creature a hunt is about. It changes nothing in here: the caller
    // hands a fleeing creature a destination behind itself instead of the sub,
    // and everything else - the routing, the wandering, the chill - carries on
    // exactly as it does for the rest of the water.
    void setQuarry(bool quarry) { m_quarry = quarry; }

    // A hunt's quarry has to last long enough to be cornered rather than shot
    // the first time it crosses the beam, so the level hands it more to lose.
    void toughen(float factor) { m_health = std::max(1, static_cast<int>(m_health * factor)); }
    bool isQuarry() const { return m_quarry; }
    bool isAware() const { return m_aware; }
    bool isScripted() const { return m_scripted; }

    // Keeps a creature that has already swum in inside the water. Anything
    // still crossing the rock to get in is left alone.
    void confine(const Arena& arena, float deltaTime);

    // Carried by the basin's current like everything else loose in the water.
    // Applied to the position, not to whatever it thinks it is doing, so a
    // creature still swims exactly where it means to and simply arrives a
    // little downstream of it.
    void drift(sf::Vector2f by, float deltaTime) { m_position += by * deltaTime; }

    // A number that is only ever this creature's, for as long as it exists.
    // Splitshot and cluster fragments are born inside whatever was just hit,
    // so a shot needs a way to say "anything but that one" - and a pointer or
    // an index into the enemy vector would not survive the vector growing.
    int getId() const { return m_id; }

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
    void updateBody(float deltaTime);

    int                       m_id;
    EnemyType                 m_type;
    const config::EnemyStats* m_stats;
    EnemyScaling              m_scaling;

    sf::Sprite   m_sprite;
    sf::Vector2f m_position;
    sf::Vector2f m_prevPosition;

    // Set when something moved it somewhere it could not have swum to - a
    // surfacing, a level start. Interpolating across one of those would draw
    // the creature flying through rock to get there.
    bool         m_warped = false;
    float        m_lit = 1.0f;
    sf::Vector2f m_aimDirection;

    // Where it is pointed, kept here rather than pushed straight at the sprite,
    // because the picture adds a beat to it and adding that to the sprite's own
    // rotation would wind the creature up like a spring.
    sf::Angle    m_facing{};
    float        m_bodyScale = 1.0f; // what the hit flash left, before the beat
    float        m_age = 0.0f;
    float        m_phase;            // so a school does not swim in lockstep
    int          m_health;
    int          m_maxHealth;
    float        m_hitFlashTimer;   // seconds of impact feedback left
    float        m_chillTimer;
    float        m_chillStrength;   // 0..0.8 taken off everything it does
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
    float        m_playerSpeed = 0.0f; // Creep only: it moves when the sub does
    bool         m_inFiringRange;
    bool         m_hasEntered;       // has it crossed into the lit basin yet
    bool         m_aware;            // does it currently know where the sub is
    bool         m_quarry = false;   // the one creature a hunt is about
    float        m_wanderAngle;      // the heading it drifts along when it does not
    bool         m_scripted;        // false once it was hatched rather than sent

    // --- the deep creatures ---
    bool         m_latched = false;   // Leech
    float        m_biteTimer = 0.0f;
    float        m_surfaced = 0.0f;   // Burrow: 0 buried, 1 fully up
    bool         m_up = false;
    float        m_auraTimer = 0.0f;  // Beacon, on whatever it is helping


    sf::Vector2f m_heading{ 1.0f, 0.0f }; // Serpent: turned rather than snapped

    // Where it is actually swimming, which is the sub when it can see it and
    // the next room along the route when it cannot. Recomputed on a timer
    // rather than every frame: routing is cheap, but not fifteen times a frame
    // cheap.
    sf::Vector2f m_navTarget{};
    float        m_navTimer = 0.0f;

    // Serpent: where the head has been, densely, and the ring positions picked
    // off it. Kept as history rather than as springs because a chain of
    // springs stretches when the head turns hard and the body then cuts a
    // corner it never swam round.
    std::vector<sf::Vector2f> m_history;
    std::vector<sf::Vector2f> m_body;
    std::unique_ptr<sf::Sprite> m_segment;
    std::unique_ptr<sf::Sprite> m_tail;
};
