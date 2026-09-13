#pragma once

#include <cstddef>

// Kept in its own header so the balance tables in Config can name the types
// without Config having to pull in the whole Enemy class.
//
// There used to be seventeen of these, and half of them were the same creature
// with different numbers: three separate ways for something to appear beside
// you, two burst-chargers driving the same state machine, two things that held
// a range and fired a fan, and four that simply swam at you. A roster that
// long reads as variety in a list and as one animal in the water.
//
// Nine now, and each asks a question none of the others do. The rule for
// adding a tenth is that it must need an answer none of these need. What was
// worth keeping out of the ones that went was folded into whichever of these
// was already nearest: the Lancer took the Darter's job, the Warden the
// Hulk's, the Weaver the Spitter's, the Bloom the Husk's dying ring, and the
// Revenant both the Angler's invisibility and the Rift's fan.
enum class EnemyType
{
    Lurker,    // the yardstick: walks you down, only dangerous up close
    Lancer,    // winds up, then commits to a straight line it cannot correct
    Weaver,    // never closes; circles at range and rakes the water with fans
    Bloom,     // does not chase, denies area with rings, and empties as it dies
    Spawnling, // one hit, worth nothing - what the Brooder is made of
    Brooder,   // the tap: keeps hatching until the source itself is killed
    Warden,    // armoured in front only, so it has to be fought from behind
    Revenant,  // under the floor and untouchable, then already beside you
    Herald,    // never attacks; it lights you up and hardens everything near it

    // Not on any level's list. It follows you down from the last one.
    Stalker,   // a head towing a body that is the weapon, and it holds on
    Count
};

inline constexpr std::size_t EnemyTypeCount = static_cast<std::size_t>(EnemyType::Count);

// How a creature decides where to move. Adding a type usually means picking one
// of these rather than writing new movement code.
enum class EnemyBehaviour
{
    Chase,    // straight at the player
    Drift,    // creeps forward and spins, ignoring where the player is
    Orbit,    // circles at a fixed range rather than closing
    Lance,    // telegraphs, locks a heading, then commits to it
    Brood,    // creeps in and hatches more of something else
    Bulwark,  // walks you down behind a shield that only faces one way
    Burrow,   // under the floor, up, strike, under again
    Serpent,  // a head towing a body that is itself the weapon
};

// Difficulty multipliers stamped onto a creature the moment it spawns, so a
// late wave lurker is tougher than an early one without needing its own type.
struct EnemyScaling
{
    float health = 1.0f;
    float speed = 1.0f;
    int   bonusDamage = 0;
};
