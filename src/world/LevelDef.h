#pragma once

#include "entities/EnemyType.h"

#include <array>
#include <cstddef>

// How a level ends.
enum class ObjectiveKind
{
    ClearRoster, // kill everything the level sends, then it is over
    KillCount,   // hit a quota while the level keeps sending more
    SurviveTime, // last out a clock
    DefeatBoss,  // one encounter, nothing else spawns
};

// One block of the spawn script. Groups are released in order, so the designer
// controls the shape of a level, not a random weight roll.
struct SpawnGroup
{
    EnemyType type = EnemyType::Lurker;
    int count = 0; // zero terminates the script
};

inline constexpr std::size_t MaxSpawnGroups = 6;

struct LevelDef
{
    const char* title;
    const char* brief; // one line on the intro banner
    ObjectiveKind objective;
    int objectiveAmount;    // kills, or seconds for SurviveTime; ignored otherwise
    float spawnInterval;    // seconds between releases
    std::size_t maxAlive;
    EnemyScaling scaling;   // also scales boss health on boss levels
    std::array<SpawnGroup, MaxSpawnGroups> script;
    bool isBoss;
};

// Every fifth level is a boss and closes a depth.
inline constexpr int LevelsPerDepth = 5;

inline constexpr int depthOf(int level) { return (level - 1) / LevelsPerDepth + 1; }
inline constexpr int levelWithinDepth(int level) { return (level - 1) % LevelsPerDepth + 1; }
inline constexpr bool isBossLevel(int level) { return levelWithinDepth(level) == LevelsPerDepth; }

const char* depthName(int depth);

// =====================================================================
//  THE CAMPAIGN
//  Hand authored, because the point is a learning curve rather than a
//  random wave. Every depth introduces one thing, drills it, combines it,
//  then tests all of it against a creature. Adding levels means adding
//  rows; nothing else changes.
// =====================================================================
inline constexpr std::array<LevelDef, 20> AuthoredLevels{ {
    // ---------------- DEPTH 1: THE SHALLOWS ----------------
    // Deliberately weaker than baseline. This level exists to teach WASD and
    // the mouse, and nothing else.
    { "FIRST CONTACT", "SIX OF THEM. NOTHING ELSE.",
      ObjectiveKind::ClearRoster, 0, 1.70f, 3, { 0.80f, 0.85f, 0 },
      { { { EnemyType::Lurker, 6 } } }, false },

    { "THEY COME FASTER", "SOMETHING QUICK MOVES IN THE DARK.",
      ObjectiveKind::ClearRoster, 0, 1.40f, 4, { 0.90f, 0.90f, 0 },
      { { { EnemyType::Lurker, 4 }, { EnemyType::Darter, 3 }, { EnemyType::Lurker, 4 } } }, false },

    { "OPEN WATER", "THEY HAVE LEARNED TO SHOOT BACK.",
      ObjectiveKind::ClearRoster, 0, 1.25f, 5, { 1.00f, 1.00f, 0 },
      { { { EnemyType::Lurker, 4 }, { EnemyType::Spitter, 2 },
          { EnemyType::Darter, 3 }, { EnemyType::Spitter, 2 } } }, false },

    { "THE PACK", "ALL OF IT AT ONCE.",
      ObjectiveKind::ClearRoster, 0, 1.05f, 6, { 1.05f, 1.02f, 0 },
      { { { EnemyType::Darter, 4 }, { EnemyType::Lurker, 5 },
          { EnemyType::Spitter, 3 }, { EnemyType::Darter, 4 } } }, false },

    { "THE LEVIATHAN", "IT HAS BEEN CIRCLING SINCE YOU ARRIVED.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 1.00f, 1.00f, 0 },
      { {} }, true },

    // ---------------- DEPTH 2: THE SHELF ----------------
    { "THE SHELF", "SOMETHING SLOW AND ARMOURED DOWN HERE.",
      ObjectiveKind::ClearRoster, 0, 1.20f, 6, { 1.00f, 1.00f, 0 },
      { { { EnemyType::Lurker, 4 }, { EnemyType::Hulk, 2 }, { EnemyType::Lurker, 4 } } }, false },

    { "CROSSFIRE", "THE HEAVY ONES HERD YOU INTO THE GUNS.",
      ObjectiveKind::ClearRoster, 0, 1.10f, 7, { 1.05f, 1.03f, 0 },
      { { { EnemyType::Hulk, 2 }, { EnemyType::Spitter, 4 }, { EnemyType::Darter, 4 } } }, false },

    { "BLOOM", "IT DOES NOT CHASE. IT FILLS THE WATER.",
      ObjectiveKind::ClearRoster, 0, 1.20f, 6, { 1.05f, 1.03f, 0 },
      { { { EnemyType::Bloom, 2 }, { EnemyType::Lurker, 5 }, { EnemyType::Bloom, 2 } } }, false },

    { "HOLD", "SURVIVE. THEY DO NOT STOP COMING.",
      ObjectiveKind::SurviveTime, 50, 1.00f, 8, { 1.10f, 1.05f, 0 },
      { { { EnemyType::Lurker, 3 }, { EnemyType::Darter, 3 },
          { EnemyType::Spitter, 2 }, { EnemyType::Hulk, 1 } } }, false },

    { "THE BLOOM SOVEREIGN", "IT DOES NOT HUNT. IT SIMPLY FILLS THE WATER.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 1.35f, 1.05f, 0 },
      { {} }, true },

    // ---------------- DEPTH 3: THE TRENCH ----------------
    { "THE TRENCH", "DEEPER. EVERYTHING BITES HARDER.",
      ObjectiveKind::KillCount, 24, 0.95f, 9, { 1.15f, 1.08f, 0 },
      { { { EnemyType::Darter, 4 }, { EnemyType::Spitter, 3 },
          { EnemyType::Lurker, 4 }, { EnemyType::Bloom, 1 } } }, false },

    { "GRINDER", "TWO WALLS OF ARMOUR AND NOWHERE TO STAND.",
      ObjectiveKind::ClearRoster, 0, 0.95f, 9, { 1.20f, 1.08f, 0 },
      { { { EnemyType::Hulk, 4 }, { EnemyType::Spitter, 4 }, { EnemyType::Darter, 6 } } }, false },

    { "GARDEN", "THE BLOOMS HAVE TAKEN THE FLOOR.",
      ObjectiveKind::ClearRoster, 0, 1.00f, 9, { 1.20f, 1.10f, 0 },
      { { { EnemyType::Bloom, 4 }, { EnemyType::Darter, 6 }, { EnemyType::Lurker, 6 } } }, false },

    { "NO QUARTER", "EVERYTHING THE TRENCH HAS LEFT.",
      ObjectiveKind::SurviveTime, 65, 0.85f, 10, { 1.25f, 1.12f, 1 },
      { { { EnemyType::Darter, 4 }, { EnemyType::Hulk, 2 },
          { EnemyType::Spitter, 3 }, { EnemyType::Bloom, 2 } } }, false },

    { "THE TRENCH WORM", "THE FLOOR MOVES. THAT IS NOT SILT.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 1.70f, 1.10f, 1 },
      { {} }, true },

    // ---------------- DEPTH 4: THE ABYSS ----------------
    { "THE ABYSS", "THE LIGHT STOPS HERE.",
      ObjectiveKind::KillCount, 30, 0.90f, 10, { 1.30f, 1.14f, 1 },
      { { { EnemyType::Lurker, 5 }, { EnemyType::Hulk, 2 },
          { EnemyType::Darter, 5 }, { EnemyType::Spitter, 3 } } }, false },

    { "PRESSURE", "HOLD. NOTHING IS COMING TO HELP.",
      ObjectiveKind::SurviveTime, 70, 0.80f, 11, { 1.35f, 1.16f, 1 },
      { { { EnemyType::Darter, 5 }, { EnemyType::Spitter, 4 },
          { EnemyType::Bloom, 2 }, { EnemyType::Hulk, 2 } } }, false },

    { "THE SHOAL", "THEY MOVE LIKE ONE ANIMAL.",
      ObjectiveKind::ClearRoster, 0, 0.72f, 12, { 1.30f, 1.20f, 1 },
      { { { EnemyType::Darter, 12 }, { EnemyType::Bloom, 3 }, { EnemyType::Darter, 10 } } }, false },

    { "EVERYTHING", "ALL OF IT. AT ONCE. IN THE DARK.",
      ObjectiveKind::ClearRoster, 0, 0.80f, 12, { 1.40f, 1.20f, 1 },
      { { { EnemyType::Hulk, 4 }, { EnemyType::Spitter, 5 }, { EnemyType::Bloom, 3 },
          { EnemyType::Darter, 8 }, { EnemyType::Lurker, 6 } } }, false },

    { "THE LEVIATHAN, GROWN", "IT REMEMBERS WHAT YOU DID TO IT.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 2.30f, 1.22f, 1 },
      { {} }, true },
} };

// Past the authored campaign the curve keeps going, still with a boss on every
// fifth level, so it never simply stops.
LevelDef levelFor(int number);
