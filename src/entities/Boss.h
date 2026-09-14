#pragma once

#include "core/Config.h"
#include "entities/BossDef.h"
#include "entities/EnemyType.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

struct Assets;

struct BossShot
{
    sf::Vector2f position;
    sf::Vector2f direction;
};

// The one thing each creature does that is not a shot. Every one of these
// changes where the fight happens rather than only how much of it is in the
// air, and none of them can be answered by strafing a little harder.
enum class BossSpecial
{
    Grab,    // it has you in its mouth. Dash, or be shaken apart.
    Sweep,   // an arc of water that throws you off it
    Seed,    // it plants, and what it plants hatches
    Tide,    // it pushes the whole room away from itself
    Silt,    // it blinds you and goes back under
    Erupt,   // it comes up through the floor exactly where you are standing
    Well,    // it pulls, and the pulling does not stop
    Hurl,    // it tears something out of the floor and throws it

    // The second half of the fight. Every one of these takes several seconds
    // to play out rather than landing in a frame, so the answer is a route
    // through it rather than a dodge - which is the difference between a boss
    // that hits harder and a boss that is a harder fight.
    Closing, // a ring of water falls in on you from every side at once
    Curtain, // a wall crosses the hall with one gap in it
    Fissure, // the floor tears open in a line, and the line keeps coming
    Rake,    // a beam sweeps the hall, and standing still is standing on it

    // What a prowler learns as the fight goes on. It starts the fight with two
    // moves; by the last act it has four, so the second half is new things to
    // read rather than the same two arriving sooner.
    Thresh,  // the whole tail comes round, and it sweeps from behind forward
    Sound,   // it stops, opens wide, and the water in front of it leaves
};

// What a special wants doing, drained by the caller the same way the shots
// are. The boss never touches the world itself: it asks.
struct BossMove
{
    BossSpecial kind;
    sf::Vector2f at;
    sf::Vector2f direction;
    float value = 0.0f;
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

    // Which part a shot would land on, without landing it. Asked so a shot
    // that is still inside the part it has already hit can be let through
    // rather than hitting it again.
    int partAt(sf::Vector2f position, float radius) const;

    // Chain Reaction: hurt whatever is bolted next to a part that just died.
    void splashNeighbours(std::size_t partIndex, int damage);

    std::vector<BossShot> takeShots();

    // Everything the creature wants doing to the world this frame that is not
    // a shot. Drained, like the shots and the summons.
    std::vector<BossMove> takeMoves();

    // How hard it is currently pressing: one at full health, rising with each
    // act and again when it is enraged. Multiplies every gap between attacks.
    float aggression() const;

    // Creatures the boss wants brought in, drained the same way its shots are.
    // A creature that calls for help is a different fight from one that does
    // not, and it costs the caller nothing to ignore.
    std::vector<EnemyType> takeSummons();

    // Which act of the fight it is in: 0 while it is whole, then one per
    // threshold it has crossed. The caller reads a change to know it should
    // stop the world for a moment; the boss itself uses it to decide what it
    // is willing to do.
    int getAct() const { return m_act; }

    // Whether it has already run out of life once. Nothing down there dies the
    // first time: the bar empties, the animal comes apart, and what stands up
    // afterwards is faster and does not look the same.
    bool isReborn() const { return m_reborn; }

    // Whether it is still made of pieces. Only one of them is.
    bool hasParts() const { return m_def->piecemeal && !m_reborn && !m_parts.empty(); }

    // The moment before a committed attack, as 0 at the announcement rising to
    // 1 as the blow lands. Anything that can really hurt you says so first, so
    // that blocking one is a read rather than a guess.
    float getTell() const;
    bool  isTelling() const { return m_tellTimer > 0.0f; }
    sf::Vector2f getTellAim() const { return m_tellAim; }
    bool  isTellLunge() const { return m_tellLunge; } // a charge, rather than a volley
    void confine(sf::Vector2f center, float maxDistance);

    // A bell whose mind does not open it opens itself, because a sealed bell
    // is a creature nothing can reach the inside of.
    void updateBellRhythm(float deltaTime);

    // Its life runs out. Once, that is a second phase; twice, that is the end
    // of it. Everything that decides which is in here.
    void fall(Hit& result);
    void beginSecondPhase();

    // How much faster it is once it has already been killed once.
    float rebornSpeed() const { return m_reborn ? config::BossRebornSpeed : 1.0f; }

    // Everything a drawn body does that the fight does not care about: the
    // jaw, the breath, the beat of the fins, where it is looking, and the rock
    // back after something lands on it.
    void updateLife(float deltaTime, sf::Vector2f player);

    // Which flank its exposed organ is on, so it can turn that side away from
    // the sub. Zero once nothing is exposed, which is the moment it stops
    // giving ground and comes at you instead.
    float guardBias(sf::Vector2f player) const;

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
    const char* getShotTexture() const;
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

    // The sprite is optional because a drawn boss has no textures at all, and
    // sf::Sprite cannot be made without one. Everything else about a link -
    // where it is, which way it points, how big it is - is what the fight uses
    // and is there either way.
    struct Segment
    {
        sf::Vector2f position;
        sf::Angle    angle;
        float        scale;
        std::optional<sf::Sprite> sprite;
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
        std::optional<sf::Sprite> sprite;
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

    void beginTell(float seconds, sf::Vector2f aim, bool lunge);
    void considerSpecial(float deltaTime, sf::Vector2f player);

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
    void thinkSpiral(float deltaTime, sf::Vector2f playerPosition);

    // How many lamps are still lit, which is how many arms the spiral has.
    int  lampsAlive() const;
    void checkRage();

    // Moves the fight on a step when the creature crosses a threshold. Returns
    // true on the frame it happens, which is the caller's cue to make an event
    // of it.
    void updateActs();

    // The last act of the last creature: the water itself closes in. Shots come
    // inward off the rim of the basin rather than out of the boss, so there is
    // no safe distance left to stand at.
    void closeTheBasin(sf::Vector2f centre);

    const BossDef* m_def;
    BossId         m_id;

    std::vector<Segment> m_segments;
    std::vector<Part>    m_parts;
    Vitals               m_vitals;
    std::vector<BossShot>  m_pending; // filled by the think code, drained by takeShots
    std::vector<BossMove>  m_moves;
    float                  m_specialTimer;
    int                    m_specialTurn = 0; // alternates between its two moves

    // How long until the move it has been saving is allowed out again. Its
    // own clock rather than a share of the ordinary one, so the second half
    // of a fight has a rhythm of its own.
    float                  m_ultraTimer = 0.0f;
    std::vector<EnemyType> m_summons;

    sf::Vector2f m_heading;
    sf::Vector2f m_playerPosition;
    sf::Vector2f m_strikeTarget;
    sf::Vector2f m_confineCentre;

    // Counted down by update, so every mind announces its attacks the same way
    // and the drawing has one thing to ask rather than four.
    float        m_tellTimer = 0.0f;
    float        m_tellLength = 1.0f;
    sf::Vector2f m_tellAim{ 1.0f, 0.0f };
    bool         m_tellLunge = false;

    Phase m_phase;
    float m_phaseTimer;
    float m_attackTimer;
    float m_time;
    float m_bellPulse;    // Bell body: 0 relaxed, 1 clenched
    float m_visibility;   // 0 fully buried, 1 fully surfaced
    float m_strafe;       // +1 or -1
    float m_spiralAngle;  // Spiral mind: where the arms currently point
    int   m_act;          // 0 whole, 1 hurt, 2 cornered
    float m_summonTimer;
    float m_closeTimer;   // the last act's inward volley
    bool  m_rhythmOpen;   // Bell body: the nucleus is showing
    float m_rhythmTimer = 0.0f; // a bell that is not a drifter runs its own

    // --- what a drawn body is doing, as opposed to what it is deciding ---
    float m_jaw = 0.0f;     // 0 shut, 1 wide
    float m_breath = 0.0f;  // a clock of its own, quicker as it tires
    float m_beat = 0.0f;    // where the fins are in their stroke
    float m_flinch = 0.0f;  // it has just been hurt
    sf::Vector2f m_look{ 1.0f, 0.0f };

    // Not owned: Assets outlives every creature in the water.
    const sf::Texture* m_hide = nullptr;

    // The pool everything that is not the leviathan lives off, and the one the
    // leviathan itself comes back with once its organs are gone.
    int   m_core = 0;
    int   m_coreMax = 0;
    bool  m_reborn = false;
    float m_shed = 0.0f;  // 0..1, how far through coming apart it is
    bool  m_enraged;
    bool  m_hasEntered;
    bool  m_dead;

    int   m_contactDamage;
    int   m_salvage;
    float m_speed;
    int   m_startingOrganHealth;
};
