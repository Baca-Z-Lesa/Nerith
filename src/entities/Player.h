#pragma once

#include "entities/ShipVisuals.h"
#include "upgrades/Stats.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Angle.hpp>
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

    void update(float deltaTime, sf::Vector2f steering, sf::Vector2f aimTarget,
                const Arena& arena);

    // Where the sub was at the end of the previous step, and where to draw it
    // between the two. The simulation runs at a fixed rate that has nothing to
    // do with how often the picture is made, so without this the sub would
    // stutter on every frame that did not line up with a step.
    void remember() { m_prevPosition = m_position; }
    void present(float alpha);
    void draw(sf::RenderTarget& target) const;

    // Puts the sub back at full health, for a restart.
    void reset();

    // Every basin is a different shape, so where a dive starts is the map's
    // business rather than a constant.
    void setPosition(sf::Vector2f position);

    // Call after the stat block changes: a bigger tank arrives full.
    void onStatsChanged();

    // Per level bookkeeping, such as rearming the emergency vent.
    void onLevelStart();

    bool tryFire();
    bool tryFireTorpedo();

    // Parasite and the refit crew both put hull back, and neither may overfill
    // the tank.
    void heal(int amount);

    // Second wind: true while the sub is hurt badly enough to be running on
    // fumes, which is what the HUD and the trail read to show it.
    bool isDesperate() const;
    bool tryDash(sf::Vector2f steering);

    // A shove from outside: a hit, a blast, anything that should move the sub
    // without the player having asked. Replaces whatever was already pushing
    // rather than adding to it, so a rain of small hits cannot launch you.
    void applyImpulse(sf::Vector2f push, float seconds);
    bool isDashing() const { return m_dashTimer > 0.0f; }

    struct DamageResult
    {
        bool applied = false;     // health actually went down
        bool shieldBroke = false; // a charge ate it instead
        bool ventSaved = false;   // the emergency vent caught a fatal hit
        bool dodged = false;      // mid dash, and the dash is the dodge
        int  soaked = 0;          // damage the plating ate before the rest landed
    };

    DamageResult takeDamage(int amount);

    sf::Vector2f getPosition() const { return m_position; }

    // How fast the sub is actually travelling, taken from the ground it
    // covered rather than from the keys held: a dash, a current and a shove
    // all move it, and anything trying to cut it off has to know about all
    // three.
    sf::Vector2f getVelocity() const { return m_velocity; }
    sf::Vector2f getAimDirection() const { return m_aimDirection; }
    sf::Vector2f getMuzzlePosition() const;
    sf::Vector2f getSternPosition() const;

    // The four things on the sub that can be knocked out. A hull bar that
    // only counts down says nothing about what the damage did; losing the
    // sonar, the tube, the gun and finally the engine says it plainly, and
    // the sub you limp home in is not the one you dived with.
    enum class Hull
    {
        Sonar,  // no ping, and the map stops filling itself in
        Tube,   // no torpedoes
        Lamp,   // the beam drops to a stub and the halo with it
        Rudder, // it still goes where you point it, just not quickly
        Gun,    // the jet lands soft and comes slowly
        Engine, // no dash, two thirds of the speed, and it wallows
        Count
    };

    bool isBroken(Hull part) const;
    bool breakNextPart();   // true when something actually went
    void repairHull();      // between dives, everything comes back
    unsigned int getBrokenMask() const { return m_broken; }

    // What the sub is dragging, as a multiplier on its speed. One when empty.
    // Kept here rather than in Game so that every thing that reads the speed -
    // the thruster trail, the dash, the urgency bonus - sees the same number.
    void setBurden(float factor) { m_burden = factor; }
    float getBurden() const { return m_burden; }

    int  getHealth() const { return m_health; }
    int  getMaxHealth() const;
    int  getShieldCharges() const { return m_shieldCharges; }
    int  getMaxShieldCharges() const;
    float getDashCooldownFraction() const;
    bool isAlive() const { return m_health > 0; }
    bool isMoving() const { return m_moving; }

private:



    float urgency() const; // 1 normally, more when the second wind is up

    void aimAt(sf::Vector2f target);
    void updateInvulnerability(float deltaTime);
    void updateShield(float deltaTime);

    const Stats* m_stats;

    sf::Sprite   m_sprite;
    ShipVisuals  m_visuals;

    sf::Vector2f m_position;
    sf::Vector2f m_prevPosition;
    sf::Vector2f m_velocity;
    sf::Vector2f m_impulse;
    float        m_impulseTimer = 0.0f;
    sf::Vector2f m_aimDirection; // unit vector the nose points along
    sf::Vector2f m_dashDirection;

    // The hull as a thing with weight in it. None of this is the sub - the
    // nose still points exactly where the aim says and the shot still leaves
    // from there - it is only how the hull is drawn carrying itself.
    float        m_age = 0.0f;
    float        m_bank = 0.0f;    // degrees of lean, chasing the turn rate
    sf::Angle    m_lastAim{};

    float m_fireTimer;
    float m_torpedoTimer;
    float m_dashTimer;
    float m_burden = 1.0f;

    // A bitmask rather than four flags: the HUD wants it as one number and
    // nothing else ever asks about the parts individually except by name.
    unsigned int m_broken = 0;
    float m_dashCooldown;
    float m_shieldTimer;

    int   m_health;
    int   m_lastMaxHealth;
    int   m_shieldCharges;
    float m_invulnerabilityTimer;
    bool  m_moving;
    bool  m_ventReady;
};
