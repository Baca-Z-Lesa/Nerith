#pragma once

#include "entities/EnemyType.h"

#include <array>
#include <cstddef>

// How a level ends.
enum class ObjectiveKind
{
    // Two thirds of the campaign used to be ClearRoster or KillCount, which is
    // the same level twice and neither of them is what anybody is down here
    // for. They are gone. What is left asks for the things the game is
    // actually made of: salvage, the dark, sealed hulls, and the men who came
    // down before you.
    LiftQuota,   // fill the hold off the floor
    RunDark,     // work with the lamp off
    OpenWrecks,  // cut the sealed hulls open
    RecoverRig,  // find what is left of the last diver, and their recorder
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
inline constexpr std::array<LevelDef, 30> AuthoredLevels{ {
    // ---------------- DEPTH 1: THE SHALLOWS ----------------
    // Still the teaching level, but no longer a formality: the sub is fragile
    // now and even lurkers will drown you if you stand still.
    { "FIRST CONTACT", "THE FLOOR IS LITTERED. FILL THE HOLD.",
      ObjectiveKind::LiftQuota, 5, 2.20f, 3, { 0.90f, 0.95f, 0 },
      { { { EnemyType::Lurker, 10 } } }, false },

    { "RUNNING DARK", "KILL THE LAMP. THEY CANNOT FIND WHAT IS NOT LIT.",
      ObjectiveKind::RunDark, 40, 1.80f, 4, { 0.88f, 0.96f, 0 },
      { { { EnemyType::Lurker, 6 }, { EnemyType::Lancer, 4 },
          { EnemyType::Lurker, 5 }, { EnemyType::Lancer, 3 } } }, false },

    // The first hunt. Nothing here is solved by holding the middle: the
    // nests are cemented out at the reef, one is never in sight from
    // another, and they hatch until they are dead.
    { "THE NURSERY", "THREE NESTS OUT ON THE REEF. BURN THEM OUT.",
      ObjectiveKind::DestroyNests, 3, 2.00f, 4, { 0.92f, 0.98f, 0 },
      { { { EnemyType::Lurker, 5 }, { EnemyType::Lancer, 3 } } }, false },

    { "SEALED", "TWO HULLS NOBODY EVER GOT OPEN. CUT THEM.",
      ObjectiveKind::OpenWrecks, 2, 1.50f, 5, { 0.96f, 1.00f, 0 },
      { { { EnemyType::Lancer, 5 }, { EnemyType::Lurker, 8 },
          { EnemyType::Brooder, 2 }, { EnemyType::Lancer, 4 } } }, false },

    { "THE LEVIATHAN", "IT HAS BEEN CIRCLING SINCE YOU ARRIVED.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 1.00f, 1.00f, 0 },
      { {} }, true },

    // ---------------- DEPTH 2: THE SHELF ----------------
    // The first haul. Two pods, a short basin, and the thing it teaches is
    // that you cannot fight your way through a room with your hands full.
    { "DEAD DROP", "TWO PODS OUT THERE. SOMETHING ARMOURED IS IN THE WAY.",
      ObjectiveKind::HaulCargo, 2, 1.40f, 5, { 1.00f, 1.02f, 0 },
      { { { EnemyType::Lurker, 6 }, { EnemyType::Warden, 2 },
          { EnemyType::Lancer, 4 }, { EnemyType::Warden, 1 } } }, false },

    { "THE REEF", "WHATEVER SANK HERE, IT SANK LOADED.",
      ObjectiveKind::LiftQuota, 7, 1.30f, 6, { 1.02f, 1.03f, 0 },
      { { { EnemyType::Warden, 2 }, { EnemyType::Weaver, 5 },
          { EnemyType::Lancer, 5 }, { EnemyType::Weaver, 3 } } }, false },

    { "THE THIRD SUB", "SOMEBODY CAME DOWN BEFORE YOU. FIND HIM.",
      ObjectiveKind::RecoverRig, 0, 1.25f, 6, { 1.05f, 1.04f, 0 },
      { { { EnemyType::Lancer, 5 }, { EnemyType::Lurker, 7 },
          { EnemyType::Lancer, 5 }, { EnemyType::Weaver, 3 } } }, false },

    { "HOLD", "SURVIVE. THEY DO NOT STOP COMING.",
      ObjectiveKind::SurviveTime, 65, 1.10f, 7, { 1.08f, 1.06f, 0 },
      { { { EnemyType::Lancer, 3 }, { EnemyType::Lurker, 6 },
          { EnemyType::Warden, 1 }, { EnemyType::Brooder, 1 },
          { EnemyType::Weaver, 3 } } }, false },

    { "THE BLOOM SOVEREIGN", "IT DOES NOT HUNT. IT SIMPLY FILLS THE WATER.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 1.45f, 1.10f, 0 },
      { {} }, true },

    // ---------------- DEPTH 3: THE TRENCH ----------------
    { "LIGHTS OUT", "THE LAMP IS WHAT THEY FOLLOW. SO DO NOT USE IT.",
      ObjectiveKind::RunDark, 55, 1.05f, 7, { 1.30f, 1.12f, 0 },
      { { { EnemyType::Weaver, 5 }, { EnemyType::Lancer, 5 },
          { EnemyType::Weaver, 4 }, { EnemyType::Lurker, 6 } } }, false },

    { "THE SPILL", "IT CAME OUT OF SOMETHING AND IT IS STILL WORTH MONEY.",
      ObjectiveKind::LiftQuota, 9, 1.00f, 7, { 1.35f, 1.14f, 0 },
      { { { EnemyType::Bloom, 4 }, { EnemyType::Warden, 2 },
          { EnemyType::Bloom, 3 }, { EnemyType::Weaver, 3 } } }, false },

    { "GARDEN", "FIVE OF THEM. THE FLOOR IS THEIRS NOW.",
      ObjectiveKind::DestroyNests, 5, 1.10f, 8, { 1.40f, 1.15f, 1 },
      { { { EnemyType::Bloom, 4 }, { EnemyType::Weaver, 4 },
          { EnemyType::Bloom, 3 } } }, false },

    // The same idea at depth, with four of them and nothing that stops coming.
    { "SALVAGE RUN", "FOUR PODS. EVERYTHING DOWN HERE KNOWS YOU ARE LOADED.",
      ObjectiveKind::HaulCargo, 4, 0.95f, 8, { 1.45f, 1.18f, 1 },
      { { { EnemyType::Lancer, 4 }, { EnemyType::Bloom, 3 },
          { EnemyType::Lancer, 4 }, { EnemyType::Weaver, 3 },
          { EnemyType::Lurker, 5 } } }, false },

    { "THE TRENCH WORM", "THE FLOOR MOVES. THAT IS NOT SILT.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 1.90f, 1.15f, 1 },
      { {} }, true },

    // ---------------- DEPTH 4: THE ABYSS ----------------
    { "THE CONVOY", "THREE OF THEM WENT DOWN TOGETHER.",
      ObjectiveKind::OpenWrecks, 3, 0.90f, 9, { 1.55f, 1.20f, 1 },
      { { { EnemyType::Revenant, 3 }, { EnemyType::Lurker, 7 },
          { EnemyType::Warden, 3 }, { EnemyType::Lancer, 5 } } }, false },

    { "PRESSURE", "THE FLOOR IS NOT THE FLOOR. HOLD OUT ANYWAY.",
      ObjectiveKind::SurviveTime, 80, 0.80f, 9, { 1.60f, 1.22f, 1 },
      { { { EnemyType::Revenant, 4 }, { EnemyType::Weaver, 3 },
          { EnemyType::Lurker, 6 }, { EnemyType::Warden, 2 },
          { EnemyType::Brooder, 1 } } }, false },

    { "THE SHOAL", "SOMETHING IS HOLDING A LIGHT UP FOR THEM.",
      ObjectiveKind::DestroyNests, 7, 0.85f, 10, { 1.65f, 1.24f, 1 },
      { { { EnemyType::Revenant, 2 }, { EnemyType::Lancer, 6 },
          { EnemyType::Herald, 3 }, { EnemyType::Lurker, 7 } } }, false },

    { "THE SECOND SUB", "HE GOT FURTHER THAN THE THIRD ONE DID.",
      ObjectiveKind::RecoverRig, 0, 0.75f, 10, { 1.75f, 1.26f, 1 },
      { { { EnemyType::Revenant, 3 }, { EnemyType::Weaver, 4 },
          { EnemyType::Warden, 3 }, { EnemyType::Herald, 2 },
          { EnemyType::Lurker, 8 }, { EnemyType::Brooder, 2 } } }, false },

    // Not the floor of the trench after all. Everything the campaign has
    // taught, asked for at once, by the thing that was down here before any
    // of it - and behind it the water keeps going.
    { "THE HOLLOW", "IT IS A RING OF BONE. NOTHING LIVES IN THE MIDDLE OF IT.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 3.40f, 1.30f, 2 },
      { {} }, true },

    // ---------------- DEPTH 5: THE VENTS ----------------
    // One idea: the lamp is now a liability. Everything above taught that the
    // dark costs you and the light is worth what it costs. The moth is the
    // first thing down here that is drawn to the beam, and the whole depth is
    // built on taking the lamp away from a player who has learned to need it.
    { "THE SEAM", "THE ORE IS WARM. NOTHING AT THIS DEPTH IS WARM.",
      ObjectiveKind::LiftQuota, 11, 0.85f, 11, { 1.85f, 1.26f, 1 },
      { { { EnemyType::Moth, 3 }, { EnemyType::Lurker, 6 },
          { EnemyType::Moth, 4 }, { EnemyType::Lancer, 4 } } }, false },

    // Drill it. A basin full of them and nothing else worth the beam.
    { "BLIND", "DOWN HERE THE LAMP IS AN ANNOUNCEMENT.",
      ObjectiveKind::RunDark, 70, 0.75f, 11, { 1.95f, 1.28f, 2 },
      { { { EnemyType::Moth, 7 }, { EnemyType::Lurker, 7 },
          { EnemyType::Moth, 6 }, { EnemyType::Weaver, 3 } } }, false },

    // And then take the choice away. A herald keeps you lit whether you like
    // it or not, so the moths are awake until the herald is dead - which puts
    // the one thing that never attacks at the top of the target list.
    { "THE YARD", "SOMEBODY WAS STACKING THEM.",
      ObjectiveKind::OpenWrecks, 3, 0.70f, 12, { 2.05f, 1.30f, 2 },
      { { { EnemyType::Herald, 2 }, { EnemyType::Moth, 6 },
          { EnemyType::Warden, 2 }, { EnemyType::Herald, 2 },
          { EnemyType::Moth, 5 } } }, false },

    // Hauling in the dark, with the pod itself giving you away.
    { "THE LAST HAUL", "FOUR PODS. NO LIGHT. GOOD LUCK.",
      ObjectiveKind::HaulCargo, 4, 0.70f, 12, { 2.15f, 1.32f, 2 },
      { { { EnemyType::Moth, 5 }, { EnemyType::Revenant, 3 },
          { EnemyType::Lurker, 7 }, { EnemyType::Moth, 5 },
          { EnemyType::Weaver, 3 } } }, false },

    { "THE FURNACE", "IT IS SITTING ON THE HOLE IN THE FLOOR.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 4.00f, 1.28f, 2 },
      { {} }, true },

    // ---------------- DEPTH 6: THE CRADLE ----------------
    // The last idea, and the opposite of every other one: stop moving. An
    // effigy only closes while the sub is under way, so a room of them is
    // crossed in bursts - and everything else in the water is still built to
    // punish exactly that.
    { "THE VEIN", "IT RUNS ALL THE WAY DOWN.",
      ObjectiveKind::LiftQuota, 13, 0.65f, 13, { 2.30f, 1.34f, 2 },
      { { { EnemyType::Effigy, 4 }, { EnemyType::Lurker, 6 },
          { EnemyType::Effigy, 4 }, { EnemyType::Lancer, 4 } } }, false },

    // The drill and the contradiction in one level: the effigies want you
    // still and the weavers kill you for it.
    { "HOLD STILL", "ONE OF THEM WANTS YOU TO STOP. THE OTHERS DO NOT.",
      ObjectiveKind::SurviveTime, 90, 0.60f, 14, { 2.45f, 1.36f, 2 },
      { { { EnemyType::Effigy, 5 }, { EnemyType::Weaver, 5 },
          { EnemyType::Effigy, 4 }, { EnemyType::Lancer, 5 },
          { EnemyType::Bloom, 3 } } }, false },

    // Both of the deep answers at once: lamp off and feet still.
    { "THE FIRST SUB", "ELEVEN DAYS. HE IS STILL DOWN HERE.",
      ObjectiveKind::RecoverRig, 0, 0.58f, 15, { 2.60f, 1.38f, 3 },
      { { { EnemyType::Effigy, 5 }, { EnemyType::Moth, 6 },
          { EnemyType::Effigy, 5 }, { EnemyType::Herald, 2 },
          { EnemyType::Warden, 3 } } }, false },

    { "NOTHING TO SEE", "THE LAMP DOES NOT HELP YOU HERE.",
      ObjectiveKind::RunDark, 85, 0.55f, 16, { 2.75f, 1.40f, 3 },
      { { { EnemyType::Effigy, 5 }, { EnemyType::Moth, 6 },
          { EnemyType::Revenant, 4 }, { EnemyType::Warden, 4 },
          { EnemyType::Herald, 3 }, { EnemyType::Weaver, 5 } } }, false },

    // The bottom. It is where all of it came from, and it is the first thing
    // shaped like that which has ever come at you.
    { "THE MOTHER", "IT IS WHERE THEY ALL CAME FROM.",
      ObjectiveKind::DefeatBoss, 0, 0.0f, 1, { 5.00f, 1.36f, 3 },
      { {} }, true },
} };

// Past the authored campaign the curve keeps going, still with a boss on every
// fifth level, so it never simply stops.
LevelDef levelFor(int number);
