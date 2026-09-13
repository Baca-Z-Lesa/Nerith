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
    DestroyNests, // find every nest in the basin and burn it out
    HaulCargo,    // drag salvage pods back to the mouth while it hunts you
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
    int objectiveAmount;    // kills, seconds for SurviveTime, pods for HaulCargo
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

// How much of the campaign is hand authored. Past it the trench builds its own
// levels, which is also where the story runs out, so this is what "finished"
// means.
int authoredLevelCount();

// =====================================================================
//  THE CAMPAIGN
//  Hand authored, because the point is a learning curve rather than a
//  random wave. Every depth introduces one thing, drills it, combines it,
//  then tests all of it against a creature. Adding levels means adding
//  rows; nothing else changes.
// =====================================================================
inline constexpr std::array<LevelDef, 20> AuthoredLevels{ {
    // ---------------- DEPTH 1: THE SHALLOWS ----------------
    // Still the teaching level, but no longer a formality: the sub is fragile
    // now and even lurkers will drown you if you stand still.
    { "FIRST CONTACT", "TEN OF THEM. NOTHING ELSE.",
      ObjectiveKind::ClearRoster, 0, 1.30f, 5, { 0.90f, 0.95f, 0 },
      { { { EnemyType::Lurker, 10 } } }, false },

    { "THEY COME FASTER", "SOMETHING QUICK MOVES IN THE DARK.",
      ObjectiveKind::ClearRoster, 0, 1.05f, 6, { 0.95f, 1.00f, 0 },
      { { { EnemyType::Lurker, 6 }, { EnemyType::Lancer, 4 },
          { EnemyType::Lurker, 5 }, { EnemyType::Lancer, 3 } } }, false },

    // The first hunt. Nothing here is solved by holding the middle: the
    // nests are cemented out at the reef, one is never in sight from
    // another, and they hatch until they are dead.
    { "THE NURSERY", "THREE NESTS OUT ON THE REEF. BURN THEM OUT.",
      ObjectiveKind::DestroyNests, 3, 1.40f, 7, { 1.00f, 1.00f, 0 },
      { { { EnemyType::Lurker, 5 }, { EnemyType::Lancer, 3 } } }, false },

    { "THE PACK", "ALL OF IT AT ONCE.",
      ObjectiveKind::ClearRoster, 0, 0.90f, 9, { 1.05f, 1.05f, 0 },
      { { { EnemyType::Lancer, 5 }, { EnemyType::Lurker, 8 },
          { EnemyType::Brooder, 2 }, { EnemyType::Lancer, 4 } } }, false },

    { "THE LEVIATHAN", "IT HAS BEEN CIRCLING SINCE YOU ARRIVED.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 1.00f, 1.00f, 0 },
      { {} }, true },

    // ---------------- DEPTH 2: THE SHELF ----------------
    // The first haul. Two pods, a short basin, and the thing it teaches is
    // that you cannot fight your way through a room with your hands full.
    { "DEAD DROP", "TWO PODS OUT THERE. SOMETHING ARMOURED IS IN THE WAY.",
      ObjectiveKind::HaulCargo, 2, 1.05f, 9, { 1.10f, 1.05f, 0 },
      { { { EnemyType::Lurker, 6 }, { EnemyType::Warden, 2 },
          { EnemyType::Lancer, 4 }, { EnemyType::Warden, 1 } } }, false },

    { "CROSSFIRE", "THE HEAVY ONES HERD YOU INTO THE GUNS.",
      ObjectiveKind::ClearRoster, 0, 0.95f, 9, { 1.12f, 1.06f, 0 },
      { { { EnemyType::Warden, 2 }, { EnemyType::Weaver, 5 },
          { EnemyType::Lancer, 5 }, { EnemyType::Weaver, 3 } } }, false },

    { "THE CHARGE", "THEY AIM BEFORE THEY MOVE. WATCH FOR IT.",
      ObjectiveKind::ClearRoster, 0, 0.90f, 10, { 1.15f, 1.08f, 0 },
      { { { EnemyType::Lancer, 5 }, { EnemyType::Lurker, 7 },
          { EnemyType::Lancer, 5 }, { EnemyType::Weaver, 3 } } }, false },

    { "HOLD", "SURVIVE. THEY DO NOT STOP COMING.",
      ObjectiveKind::SurviveTime, 65, 0.80f, 11, { 1.20f, 1.10f, 0 },
      { { { EnemyType::Lancer, 3 }, { EnemyType::Lurker, 6 },
          { EnemyType::Warden, 1 }, { EnemyType::Brooder, 1 },
          { EnemyType::Weaver, 3 } } }, false },

    { "THE BLOOM SOVEREIGN", "IT DOES NOT HUNT. IT SIMPLY FILLS THE WATER.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 1.45f, 1.10f, 0 },
      { {} }, true },

    // ---------------- DEPTH 3: THE TRENCH ----------------
    { "THE TRENCH", "THEY CIRCLE. THEY DO NOT CLOSE.",
      ObjectiveKind::ClearRoster, 0, 0.85f, 10, { 1.30f, 1.12f, 0 },
      { { { EnemyType::Weaver, 5 }, { EnemyType::Lancer, 5 },
          { EnemyType::Weaver, 4 }, { EnemyType::Lurker, 6 } } }, false },

    { "DEAD WEIGHT", "DO NOT BE STANDING NEXT TO THEM WHEN THEY GO.",
      ObjectiveKind::ClearRoster, 0, 0.85f, 10, { 1.35f, 1.14f, 0 },
      { { { EnemyType::Bloom, 4 }, { EnemyType::Warden, 2 },
          { EnemyType::Bloom, 3 }, { EnemyType::Weaver, 3 } } }, false },

    { "GARDEN", "FIVE OF THEM. THE FLOOR IS THEIRS NOW.",
      ObjectiveKind::DestroyNests, 5, 1.05f, 10, { 1.40f, 1.15f, 1 },
      { { { EnemyType::Bloom, 4 }, { EnemyType::Weaver, 4 },
          { EnemyType::Bloom, 3 } } }, false },

    // The same idea at depth, with four of them and nothing that stops coming.
    { "SALVAGE RUN", "FOUR PODS. EVERYTHING DOWN HERE KNOWS YOU ARE LOADED.",
      ObjectiveKind::HaulCargo, 4, 0.75f, 12, { 1.45f, 1.18f, 1 },
      { { { EnemyType::Lancer, 4 }, { EnemyType::Bloom, 3 },
          { EnemyType::Lancer, 4 }, { EnemyType::Weaver, 3 },
          { EnemyType::Lurker, 5 } } }, false },

    { "THE TRENCH WORM", "THE FLOOR MOVES. THAT IS NOT SILT.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 1.90f, 1.15f, 1 },
      { {} }, true },

    // ---------------- DEPTH 4: THE ABYSS ----------------
    { "THE ABYSS", "SHOOTING THE BIG ONE IN THE FACE WILL NOT WORK.",
      ObjectiveKind::ClearRoster, 0, 0.90f, 11, { 1.55f, 1.20f, 1 },
      { { { EnemyType::Revenant, 3 }, { EnemyType::Lurker, 7 },
          { EnemyType::Warden, 3 }, { EnemyType::Lancer, 5 } } }, false },

    { "PRESSURE", "THE FLOOR IS NOT THE FLOOR. HOLD OUT ANYWAY.",
      ObjectiveKind::SurviveTime, 80, 0.70f, 12, { 1.60f, 1.22f, 1 },
      { { { EnemyType::Revenant, 4 }, { EnemyType::Weaver, 3 },
          { EnemyType::Lurker, 6 }, { EnemyType::Warden, 2 },
          { EnemyType::Brooder, 1 } } }, false },

    { "THE SHOAL", "SOMETHING IS HOLDING A LIGHT UP FOR THEM.",
      ObjectiveKind::DestroyNests, 7, 0.85f, 12, { 1.65f, 1.24f, 1 },
      { { { EnemyType::Revenant, 2 }, { EnemyType::Lancer, 6 },
          { EnemyType::Herald, 3 }, { EnemyType::Lurker, 7 } } }, false },

    { "EVERYTHING", "ALL OF IT. AT ONCE. IN THE DARK.",
      ObjectiveKind::KillCount, 55, 0.65f, 14, { 1.75f, 1.26f, 1 },
      { { { EnemyType::Revenant, 3 }, { EnemyType::Weaver, 4 },
          { EnemyType::Warden, 3 }, { EnemyType::Herald, 2 },
          { EnemyType::Lurker, 8 }, { EnemyType::Brooder, 2 } } }, false },

    // The floor of the trench. Everything the campaign has taught, asked for
    // at once, by the thing that was down here before any of it.
    { "THE HOLLOW", "IT IS A RING OF BONE. NOTHING LIVES IN THE MIDDLE OF IT.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 3.40f, 1.30f, 2 },
      { {} }, true },
} };

// Past the authored campaign the curve keeps going, still with a boss on every
// fifth level, so it never simply stops.
LevelDef levelFor(int number);
