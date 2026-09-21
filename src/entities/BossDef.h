#pragma once

#include <array>
#include <cstddef>

enum class BossId
{
    Leviathan, // the great predator: stalks, telegraphs, charges
    Medusa,    // the bloom sovereign: drifts and pulses rings of stingers
    Worm,      // the trench worm: dives, vanishes, erupts somewhere else
    Hollow,    // the crown of bone: holds its distance and winds a spiral

    // Below the abyss. Both are bells, and a bell has never done either of
    // these things: one plants itself on an open vent and winds the water
    // around it, the other charges.
    Furnace,   // sits on the hole in the floor and turns the room into one
    Mother,    // a bell that runs you down, and refills the water while it does
    Count
};

// How the body is built and drawn.
enum class BossBody
{
    Serpent, // a head towing a chain of segments that follow it
    Bell,    // a single pulsing dome with tentacles hanging off it

    // Two shapes and six creatures is why four of them read as the same
    // animal in different colours: a core with limbs on it, again. What
    // decides whether two bosses look alike is not the skin - the skins are
    // a reptile, a jellyfish, a bobbit worm, a whale fall, a vent and an
    // octopus - it is where the body PUTS itself, and there were only ever
    // two answers to that.
    //
    // A skin draws whatever the links do, so a new plan is a new layout and
    // not a new drawing. These are the layouts.
    Ring,    // a closed circle that turns and tightens; it does not travel
    Split,   // two halves that come apart and hunt on their own clocks
    Tower,   // courses stacked on a hub that never moves, and grows instead
    Coil,    // wound on the floor at rest, and the longest thing here mid strike
};

// What actually gets put on the screen. A sprite is the right answer for
// something twenty pixels across that there are fifteen of; it is the wrong
// answer for two hundred pixels of animal the player looks at for a minute and
// a half, because a picture held at the right angle cannot breathe, cannot
// open its jaws before it bites, and cannot look at you.
//
// Anything with its own skin is drawn by BossArt out of shapes, and its
// textures do not exist.
// Every drawn creature is built from something that is actually down there.
// The trench is not a bestiary of invented shapes: it is a pliosaur, a lion's
// mane, a bobbit worm, a whale fall, a vent colony and a brooding octopus, and
// each of them is drawn the way that animal is actually put together.
enum class BossSkin
{
    Sprites,   // the textures named in the definition
    Leviathan, // a long-jawed marine reptile: spine, hinged jaw, fins, eyes
    Medusa,    // a lion's mane: translucent bell, radial canals, oral arms
    Worm,      // a bobbit worm: chitin rings, parapodia, chaetae, scissor jaws
    Hollow,    // a whale fall: cranium, vertebrae, ribs, and a crinoid crown
    Furnace,   // a vent chimney: mineral crust, tube worms, shimmering water
    Mother,    // a brooding octopus: mantle, chromatophores, arms, eggs
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
    BossSkin skin;

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

    // What it is worth, and how many things are feeding its volleys.
    //
    // The parts below used to be things you broke off one at a time. Shooting
    // the eyes off an animal and watching it aim worse is a good fight to have
    // once, and it was being had six times - so they are anatomy now. An
    // octopus without arms is not an octopus; the arms are simply part of one
    // creature with one pool of life, and what makes these six different from
    // each other is what they do rather than which bits fall off.
    int coreHealth;
    int emitters;

    // How many times it gets back up. Every one of them has a second wind; the
    // thing at the bottom of the trench has two, and nobody is expecting the
    // second one.
    int lives;

    std::size_t partCount;
    std::array<BossPartDef, MaxBossParts> parts;
};

const BossDef& bossDefFor(BossId id);

// Which creature guards the end of a depth. Past the authored list they come
// back around, scaled by the level table.
BossId bossForDepth(int depth);
