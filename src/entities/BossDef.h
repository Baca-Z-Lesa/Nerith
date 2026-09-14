#pragma once

#include <array>
#include <cstddef>

enum class BossId
{
    Leviathan, // the great predator: stalks, telegraphs, charges
    Medusa,    // the bloom sovereign: drifts and pulses rings of stingers
    Worm,      // the trench worm: dives, vanishes, erupts somewhere else
    Hollow,    // the crown of bone: holds its distance and winds a spiral
    Count
};

// How the body is built and drawn.
enum class BossBody
{
    Serpent, // a head towing a chain of segments that follow it
    Bell,    // a single pulsing dome with tentacles hanging off it
};

// How it thinks. One of these plus the numbers below is a whole fight.
enum class BossMind
{
    Prowler,  // circles at range, winds up, then charges the player down
    Drifter,  // creeps in and attacks on a slow rhythm
    Burrower, // surfaces, sweeps, dives, comes back somewhere else
    Spiral,   // keeps its distance and winds a turning spiral out of itself
};

// What happens when a creature is hurt badly enough to change its mind. Every
// boss has a second half to its fight, and this is the whole of it: the same
// behaviour wound tighter, with more coming out of it. Written as data so a new
// creature gets a second phase by filling in four numbers.
struct BossRage
{
    float cooldownScale = 0.65f; // attacks this much sooner
    float speedScale = 1.25f;
    int   extraShots = 2;
    float spreadScale = 1.2f;
};

// What a body part is for, and therefore what breaking it costs the creature.
enum class PartRole
{
    Eye,      // its aim gets worse
    Fin,      // it gets slower and clumsier
    Tentacle, // one fewer thing firing at you
    Armour,   // shields the organ on its own segment
    Organ,    // the actual life of the animal
};

struct BossPartDef
{
    PartRole role;
    const char* texture;
    int   segment;   // which body segment it rides on; 0 is the head
    float along;     // offset along that segment's forward axis
    float lateral;   // offset across it, sign picks the side
    float radius;    // hitbox
    float scale;
    float baseAngle; // extra rotation in degrees, for fins and tentacles
    int   health;
};

inline constexpr std::size_t MaxBossParts = 10;

struct BossDef
{
    const char* name;
    const char* brief;

    BossBody body;
    BossMind mind;

    const char* headTexture;
    const char* shotTexture; // what this animal throws
    const char* segmentTexture;

    // What the last link is drawn as. A chain of identical links does not end,
    // it stops, which made every one of these look like half an animal. Null
    // for a body that is not a chain.
    const char* tailTexture;
    float headScale;
    float segmentScale;
    float segmentTaper;   // each link this much smaller than the one ahead
    int   segmentCount;   // trailing links, not counting the head
    float segmentSpacing;
    float headRadius;

    int   contactDamage;
    int   salvage;
    float speed;
    float preferredDistance;

    float attackCooldown;
    int   attackShots;
    float attackSpread;   // degrees the volley covers; 360 is a ring

    BossRage rage;

    std::size_t partCount;
    std::array<BossPartDef, MaxBossParts> parts;
};

const BossDef& bossDefFor(BossId id);

// Which creature guards the end of a depth. Past the authored list they come
// back around, scaled by the level table.
BossId bossForDepth(int depth);
