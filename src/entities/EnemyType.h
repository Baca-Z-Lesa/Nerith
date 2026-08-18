#pragma once

#include <cstddef>

// Kept in its own header so the balance tables in Config can name the types
// without Config having to pull in the whole Enemy class.
enum class EnemyType
{
    Lurker,  // baseline: walks you down, only dangerous up close
    Darter,  // fast and fragile, closes in bursts
    Hulk,    // slow sponge that hits hard and cannot be ignored
    Spitter, // holds its range and fires aimed orbs
    Bloom,   // barely moves, fires a full ring of orbs on a timer
    Count
};

inline constexpr std::size_t EnemyTypeCount = static_cast<std::size_t>(EnemyType::Count);

// How a creature decides where to move. Adding a type usually means picking one
// of these rather than writing new movement code.
enum class EnemyBehaviour
{
    Chase,    // straight at the player
    Dash,     // at the player, but in bursts with a rest between
    Standoff, // hold a preferred range and strafe around it
    Drift,    // creeps forward and spins, ignoring where the player is
};

// Difficulty multipliers stamped onto a creature the moment it spawns, so a
// late wave lurker is tougher than an early one without needing its own type.
struct EnemyScaling
{
    float health = 1.0f;
    float speed = 1.0f;
    int   bonusDamage = 0;
};
