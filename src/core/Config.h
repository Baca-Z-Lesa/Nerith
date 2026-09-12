#pragma once

#include "entities/EnemyType.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

// Central place for tunable values. Everything that decides how the game feels
// lives here or in the level table, so balancing never means hunting through
// the code.
namespace config
{
    // --- Window ---
    inline constexpr unsigned int WindowWidth = 1280;
    inline constexpr unsigned int WindowHeight = 720;
    inline constexpr const char* WindowTitle = "Nerith";
    inline constexpr unsigned int FramerateLimit = 144;

    // Upper bound for a single frame's delta time. Without it, a stalled frame
    // (window dragged, breakpoint hit) would teleport everything on resume.
    inline constexpr float MaxDeltaTime = 0.05f;

    inline constexpr sf::Color BackgroundColor{ 2, 5, 9 };

    // --- Arena ---
    // The basin is far wider than the window, so the camera follows the sub and
    // a boss no longer fills the whole playable space.
    inline constexpr float ArenaRadius = 520.0f;

    // =====================================================================
    //  THE MAP
    //  The basin is cut, not drawn: a walk of chambers joined by corridors,
    //  generated from the level number so every level is its own place and
    //  the same place twice. The rock between them is solid, which is what
    //  makes the map terrain rather than a boundary.
    // =====================================================================
    // The step has to be comfortably more than two radii or consecutive
    // chambers merge into one blob and the corridors vanish - which is exactly
    // what happened the first time these were tuned.
    inline constexpr float MapExtent = 3000.0f;       // how far a walk may wander
    inline constexpr int   ChamberCount = 9;
    inline constexpr float ChamberStartRadius = 380.0f; // where a dive begins
    inline constexpr float ChamberRadiusMin = 190.0f;
    inline constexpr float ChamberRadiusMax = 330.0f;
    inline constexpr float ChamberStepMin = 780.0f;
    inline constexpr float ChamberStepMax = 1150.0f;
    inline constexpr float ChamberTurn = 78.0f;       // degrees the walk may swing
    inline constexpr float CorridorRadius = 105.0f;
    inline constexpr float BranchChance = 0.45f;

    // A boss gets one room it can turn in, off to one side of where you start.
    inline constexpr float HallRadius = 700.0f;
    inline constexpr float HallDistance = 900.0f;

    // The cut edge of the rock, drawn under the floor and showing only where no
    // floor covers it.
    inline constexpr float WallLip = 24.0f;
    inline constexpr sf::Color WallLipColor{ 62, 72, 86 };

    // --- Making the edge read as a cave rather than as a wall ---
    // Rock is not turned on a lathe. Each chamber's rim is its radius pushed in
    // and out by two slow waves round it, which is enough to stop a basin
    // looking like a diagram of overlapping circles. The collision field and
    // the drawing both go through the same function, so this changes the shape
    // of the map and not merely the picture of it.
    inline constexpr float ChamberRipple = 0.11f;

    // The cut edge, lit. Three bands from outside in: the mass of the rock in
    // shadow, the cut itself, and then a bright line right on the waterline.
    // That last line is the whole point - it is what says "this is as far as
    // you go" without anything having to look like a fence.
    inline constexpr float WallShelf = 130.0f;    // the dark mass behind the cut
    inline constexpr sf::Color RockDeepColor{ 22, 26, 34 }; // pillar shafts only
    inline constexpr float WallRimWidth = 7.0f;
    inline constexpr sf::Color WallRimLight{ 190, 216, 228, 190 };

    // And the sand right under the rock sits in its shadow, fading back to open
    // floor. Without this the floor meets the rock as a hard line and the whole
    // basin reads flat; with it, the wall has somewhere to stand.
    inline constexpr float WallShadeWidth = 78.0f;
    inline constexpr sf::Color WallShadeColor{ 2, 6, 11, 190 };

    // Boulders slumped along the foot of the wall. Purely silhouette: they sit
    // outside the water, so nothing can collide with them, and they exist to
    // stop the rim from being a curve.
    inline constexpr int   BouldersPerChamber = 15;
    inline constexpr float BoulderRadiusMin = 22.0f;
    inline constexpr float BoulderRadiusMax = 62.0f;

    // --- What a chamber can be ---
    // A basin of nine identical discs is a shape, not a place. Every chamber
    // past the first rolls for one of these, and each one changes how the room
    // is fought rather than only how it looks.
    inline constexpr float ThroatChance = 0.22f;  // a squeeze, entered down a narrow neck
    inline constexpr float PillarChance = 0.26f;  // rock columns to break line of fire
    inline constexpr float PitChance = 0.18f;     // water that will not let you stand still
    inline constexpr float ThroatCorridorScale = 0.42f;

    inline constexpr int   PillarCountMin = 3;
    inline constexpr int   PillarCountMax = 6;
    inline constexpr float PillarRadiusMin = 44.0f;
    inline constexpr float PillarRadiusMax = 76.0f;

    // The pit's pull, in pixels per second at the middle, mostly round and a
    // little inward. Strong enough to have to be flown against, never strong
    // enough to hold anything that commits to leaving.
    inline constexpr float PitStrength = 190.0f;
    inline constexpr sf::Color PitMarkColor{ 132, 196, 210, 42 };

    // Everything outside the basin is rock, painted as the depth's own floor
    // multiplied down. Darker than the sand by enough that the edge of the map
    // is obvious, and still the depth's colour rather than a flat grey.
    inline constexpr sf::Color RockTint{ 74, 76, 86 };
    inline constexpr float WorldExtent = 3800.0f;   // half-size the seabed covers
    // How far past the edge of the basin the camera is allowed to see. A little
    // overshoot is wanted: rock in frame is what tells the player where the
    // wall is before they hit it.
    inline constexpr float CameraOvershoot = 0.55f;
    inline constexpr float CameraLerp = 5.0f;       // how quickly it catches up
    inline constexpr float ArenaLightRadius = 340.0f;
    inline constexpr sf::Color ArenaLightColor{ 32, 78, 90 };
    inline constexpr std::size_t ArenaCausticBands = 7;
    inline constexpr float ArenaCausticAngle = 24.0f;

    // Widened when the map became terrain: a wall the player can never see is
    // not cover, it is a surprise. The dark still closes in, it just closes in
    // far enough out that the shape of the basin is readable first.
    inline constexpr float GloomInnerX = 540.0f;
    inline constexpr float GloomInnerY = 310.0f;
    inline constexpr float GloomOuterX = 900.0f;
    inline constexpr float GloomOuterY = 520.0f;
    inline constexpr std::uint8_t GloomMaxAlpha = 242;

    // --- Seabed ---
    inline constexpr sf::Color SandNearColor{ 15, 30, 38 };
    inline constexpr sf::Color SandFarColor{ 9, 19, 26 };
    // Scaled to the basin: the map is several times the old circle, so the same
    // counts would leave it bare.
    inline constexpr std::size_t SandGrainCount = 9000;
    inline constexpr std::size_t ScatterPropCount = 1500;
    inline constexpr std::size_t ReefPropCount = 520;
    inline constexpr float ReefOffset = 22.0f;
    inline constexpr float PropFieldRadius = 3600.0f;

    inline constexpr std::size_t MaxCritters = 4;
    inline constexpr float CritterSpawnIntervalMin = 3.5f;
    inline constexpr float CritterSpawnIntervalMax = 9.0f;

    // --- Player ---
    inline constexpr const char* PlayerTexturePath = "assets/textures/player_sub.png";
    inline constexpr float PlayerScale = 0.5f;
    inline constexpr float PlayerRadius = 20.0f;
    inline constexpr float PlayerSpeed = 228.0f;
    inline constexpr float PlayerFireCooldown = 0.21f;
    inline constexpr float PlayerMuzzleOffset = 30.0f;
    inline constexpr float PlayerSternOffset = 26.0f;
    // Three hits. The campaign is meant to be beaten by learning a level, not
    // by having enough hull to walk through it.
    inline constexpr int   PlayerMaxHealth = 3;
    inline constexpr float PlayerInvulnerabilityDuration = 0.62f;
    inline constexpr float PlayerBlinkInterval = 0.16f;
    inline constexpr float PlayerBubbleInterval = 0.045f;

    // --- Projectiles ---
    struct ProjectileStats
    {
        float speed;
        float radius;
        float scale;
    };

    inline constexpr const char* WaterJetTexturePath = "assets/textures/water_jet.png";
    inline constexpr const char* EnemyOrbTexturePath = "assets/textures/enemy_orb.png";

    inline constexpr ProjectileStats WaterJet{ 620.0f, 8.0f, 0.75f };
    inline constexpr ProjectileStats EnemyOrb{ 245.0f, 9.0f, 0.95f };

    inline constexpr int WaterJetDamage = 1;
    inline constexpr int EnemyOrbDamage = 1;

    // =====================================================================
    //  ENEMY TABLE
    //  One row per creature. This is the whole design of every enemy: change
    //  a number here and that is the balance change, no code to touch.
    // =====================================================================
    struct EnemyStats
    {
        EnemyBehaviour behaviour;
        const char* texturePath;

        // What it throws. Every kind that fires has its own, so the water tells
        // you what is shooting before you have found the thing that fired it.
        // Creatures that never fire leave it null and fall back to the orb.
        const char* shotPath;

        float scale;
        float radius;
        float speed;
        int   health;
        int   contactDamage;
        int   salvage;
        float preferredDistance; // Standoff only
        float fireCooldown;      // 0 means it has no ranged attack
        int   shotCount;
        float shotSpread;        // degrees; 360 is a full ring
        float burstOn;           // Dash and Lance: seconds committed
        float burstOff;          // Dash and Lance: seconds recovering
        EnemyType broodType;     // Brood only: what it hatches
        int   broodCount;        // 0 means it hatches nothing
        float broodInterval;
        int   deathShots;        // ring of orbs fired as it dies
        bool  stealth;           // barely visible outside the lamp
    };

    // Order must match EnemyType.
    inline constexpr std::array<EnemyStats, EnemyTypeCount> EnemyTable{ {
        // Lurker: the yardstick. Everything else is defined against it.
        { EnemyBehaviour::Chase, "assets/textures/enemy_lurker.png", nullptr,
          0.85f, 16.0f, 124.0f, 4, 1, 1, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f },

        // Darter: dies to a single jet, but closes faster than you can back
        // away. Punishes standing still, harmless if you keep track of it.
        { EnemyBehaviour::Dash, "assets/textures/enemy_darter.png", nullptr,
          0.8f, 12.0f, 405.0f, 1, 1, 1, 0.0f, 0.0f, 0, 0.0f, 0.55f, 0.55f },

        // Hulk: cannot be outfought quickly, but can be walked away from all
        // day. It is there to take up space and soak your damage.
        { EnemyBehaviour::Chase, "assets/textures/enemy_hulk.png", nullptr,
          0.75f, 34.0f, 58.0f, 20, 2, 3, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f },

        // Spitter: safe to ignore up close, lethal if left alone at range.
        { EnemyBehaviour::Standoff, "assets/textures/enemy_spitter.png", "assets/textures/shot_spitter.png",
          0.8f, 20.0f, 88.0f, 6, 1, 2, 225.0f, 1.10f, 2, 16.0f, 0.0f, 0.0f },

        // Bloom: never chases and cannot be dodged by distance. It denies area
        // with rings of orbs, so it has to be killed rather than avoided.
        { EnemyBehaviour::Drift, "assets/textures/enemy_bloom.png", "assets/textures/shot_bloom.png",
          0.7f, 26.0f, 38.0f, 8, 1, 3, 0.0f, 1.95f, 14, 360.0f, 0.0f, 0.0f },

        // Spawnling: one jet kills it and it drops nothing. It exists to fill
        // the water and take up the shots you wanted to spend elsewhere.
        { EnemyBehaviour::Chase, "assets/textures/enemy_spawnling.png", nullptr,
          0.55f, 9.0f, 240.0f, 1, 1, 0, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f },

        // Brooder: the tap the spawnlings come out of. Ignore it and the arena
        // fills up; the answer is always to kill the source, not the swarm.
        { EnemyBehaviour::Brood, "assets/textures/enemy_brooder.png", nullptr,
          0.9f, 30.0f, 42.0f, 26, 1, 5, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f,
          EnemyType::Spawnling, 4, 2.5f, 0, false },

        // Lancer: telegraphs for most of a second, then commits to a straight
        // line it cannot correct. Readable, and lethal if you read it late.
        { EnemyBehaviour::Lance, "assets/textures/enemy_lancer.png", nullptr,
          0.75f, 15.0f, 495.0f, 4, 2, 2, 0.0f, 0.0f, 0, 0.0f, 0.45f, 0.80f },

        // Weaver: never closes. It circles at range raking fans across the
        // water, so standing still anywhere is the mistake.
        { EnemyBehaviour::Orbit, "assets/textures/enemy_weaver.png", "assets/textures/shot_weaver.png",
          0.75f, 18.0f, 158.0f, 7, 1, 3, 265.0f, 1.35f, 4, 40.0f, 0.0f, 0.0f },

        // Husk: slow, soft, and it fires a full ring the moment it dies. The
        // punishment is for killing it while you are standing next to it.
        { EnemyBehaviour::Chase, "assets/textures/enemy_husk.png", "assets/textures/shot_husk.png",
          0.8f, 22.0f, 70.0f, 7, 1, 2, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f,
          EnemyType::Lurker, 0, 0.0f, 16, false },

        // Angler: invisible until it is inside your lamp. Fast and it hits
        // hard, so the sonar upgrades stop being a luxury.
        { EnemyBehaviour::Chase, "assets/textures/enemy_angler.png", nullptr,
          0.8f, 17.0f, 205.0f, 6, 2, 4, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f,
          EnemyType::Lurker, 0, 0.0f, 0, true },

        // ---------------- BELOW THE CAMPAIGN ----------------
        // Everything from here has a rule rather than a bigger number, and
        // every one of those rules breaks a habit the shallow water taught.

        // Warden: the shield only faces forward. Shooting it head on does
        // nothing at all, so the answer is to get behind it - and it is slow
        // enough to get behind, and hits hard enough that failing to is fatal.
        { EnemyBehaviour::Bulwark, "assets/textures/enemy_warden.png", nullptr,
          0.95f, 38.0f, 74.0f, 44, 4, 9, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f },

        // Siphon: it does not want to kill you, it wants to hold on. Once it
        // is attached it drains, and it keeps breaking the streak, so it has
        // to come off before anything else gets dealt with.
        { EnemyBehaviour::Leech, "assets/textures/enemy_siphon.png", "assets/textures/shot_siphon.png",
          0.7f, 14.0f, 340.0f, 11, 2, 6, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f },

        // Revenant: untouchable while it is under the floor, and it is under
        // the floor most of the time. The fight is entirely about the few
        // seconds it is up.
        { EnemyBehaviour::Burrow, "assets/textures/enemy_revenant.png", nullptr,
          0.85f, 25.0f, 168.0f, 30, 4, 10, 0.0f, 0.0f, 0, 0.0f, 1.30f, 2.40f },

        // Herald: it has nothing to bite with and it is the most dangerous
        // thing in the water. It hardens everything near it and it keeps you
        // lit, so running dark stops working until it is dead.
        { EnemyBehaviour::Beacon, "assets/textures/enemy_herald.png", nullptr,
          0.9f, 27.0f, 58.0f, 40, 1, 13, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f },

        // Rift: distance is not a defence against it. It steps through the
        // water to a range it likes and empties a fan into you on arrival.
        { EnemyBehaviour::Blink, "assets/textures/enemy_rift.png", "assets/textures/shot_rift.png",
          0.8f, 21.0f, 46.0f, 24, 3, 11, 300.0f, 1.60f, 7, 54.0f, 0.0f, 0.0f },

        // Coilworm: the head is only how it steers. The body laid along where
        // it has already been is the weapon, and only the head can be hurt.
        { EnemyBehaviour::Serpent, "assets/textures/enemy_coilworm.png", nullptr,
          0.8f, 23.0f, 196.0f, 38, 3, 14, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f },
    } };

    // What a creature is made of, for the ear rather than the eye. Kept as its
    // own table rather than a field on EnemyStats because it is answering a
    // different question from every other number in there, and because the
    // same five voices cover seventeen animals.
    enum class CreatureVoice
    {
        Soft,    // meat, and enough air in it to fall a long way
        Shell,   // plate and chitin: it does not slide, it stops
        Bladder, // gas going out of something that was mostly gas
        Wet,     // a chittering squeal that argues on the way down
        Void,    // whatever the rift is, it does not end the way flesh does
    };

    // Order must match EnemyType.
    inline constexpr std::array<CreatureVoice, EnemyTypeCount> EnemyVoices{ {
        CreatureVoice::Soft,    // Lurker
        CreatureVoice::Wet,     // Darter
        CreatureVoice::Shell,   // Hulk
        CreatureVoice::Wet,     // Spitter
        CreatureVoice::Bladder, // Bloom
        CreatureVoice::Soft,    // Spawnling
        CreatureVoice::Bladder, // Brooder
        CreatureVoice::Shell,   // Lancer
        CreatureVoice::Wet,     // Weaver
        CreatureVoice::Bladder, // Husk
        CreatureVoice::Wet,     // Angler
        CreatureVoice::Shell,   // Warden
        CreatureVoice::Soft,    // Siphon
        CreatureVoice::Soft,    // Revenant
        CreatureVoice::Bladder, // Herald
        CreatureVoice::Void,    // Rift
        CreatureVoice::Shell,   // Coilworm
    } };

    inline constexpr CreatureVoice voiceOf(EnemyType type)
    {
        return EnemyVoices[static_cast<std::size_t>(type)];
    }

    inline constexpr const EnemyStats& enemyStats(EnemyType type)
    {
        return EnemyTable[static_cast<std::size_t>(type)];
    }

    inline constexpr float EnemyDashRestFactor = 0.12f;
    inline constexpr float EnemyApproachBoost = 2.6f;
    inline constexpr float EnemyRoamMargin = 95.0f;
    inline constexpr float SpitterStrafeFactor = 0.6f;
    inline constexpr float SpitterRangeBand = 24.0f;
    inline constexpr float BloomSpinSpeed = 42.0f;
    inline constexpr float OrbitCloseFactor = 0.6f;   // how hard an orbiter corrects its range
    inline constexpr float LanceCreepFactor = 0.14f;  // speed while it winds up
    inline constexpr float StealthRevealRange = 300.0f; // inside this the angler is visible
    inline constexpr std::uint8_t StealthAlpha = 26;    // how faint it is outside that
    // --- What the deep creatures do that the shallow ones do not ---
    // Warden: a shot inside this many degrees either side of where it faces is
    // turned away entirely. Wide enough that backing off does not work,
    // narrow enough that going round it is a real answer.
    // --- Fire modes ---
    // Two guns that are not the jet with different numbers. The harpoon is
    // mobility as much as a weapon: it lands, and then it pulls you to it. The
    // pulse has no projectile at all, which makes it the answer to being
    // surrounded rather than to anything at range.
    inline constexpr float HarpoonSpeed = 980.0f;
    inline constexpr int   HarpoonDamage = 9;
    inline constexpr float HarpoonCooldown = 0.95f;
    inline constexpr float HarpoonReach = 720.0f;  // how far the line runs out
    inline constexpr float HarpoonPullSpeed = 1350.0f;
    inline constexpr float HarpoonPullTime = 0.34f;
    inline constexpr sf::Color HarpoonLineColor{ 196, 208, 214 };

    inline constexpr float PulseRadius = 210.0f;
    inline constexpr int   PulseDamage = 7;
    inline constexpr float PulseCooldown = 1.25f;
    inline constexpr float PulseChill = 0.55f;
    inline constexpr float PulseChillTime = 2.2f;
    inline constexpr sf::Color PulseColor{ 150, 226, 255 };

    inline constexpr float BulwarkArc = 78.0f;

    // --- What a beating does to the sub ---
    // A hull bar that only counts down says nothing about what the damage did.
    // Crossing each of these fractions knocks out the next system, least
    // crippling first, and none of it comes back until the dive is over.
    inline constexpr std::array<float, 4> HullBreakAt{ { 0.72f, 0.52f, 0.32f, 0.16f } };
    inline constexpr float BrokenEngineSpeed = 0.66f;
    inline constexpr float BrokenGunDamage = 0.62f;
    inline constexpr float BrokenGunRate = 1.45f;   // multiplies the gap between shots
    inline constexpr sf::Color HullBrokenColor{ 226, 96, 78 };

    // Siphon: how near it has to be to take hold, what it takes per bite, and
    // how hard a dash throws it off.
    inline constexpr float LeechRange = 34.0f;
    inline constexpr float LeechInterval = 0.9f;
    inline constexpr int   LeechDrain = 1;
    inline constexpr float LeechThrow = 190.0f;

    // Revenant: burstOn is how long it stays up, burstOff how long it is under.
    // Buried it cannot be hit or touched; it comes up this far from the sub.
    inline constexpr float RevenantSurfaceDistance = 150.0f;
    inline constexpr float RevenantFade = 3.0f;

    // Herald: everything alive inside this takes less and moves more, and the
    // sub inside it is lit whether its own lamp is on or not.
    inline constexpr float HeraldRadius = 620.0f;
    inline constexpr float HeraldSpeedBonus = 0.45f;
    inline constexpr float HeraldArmour = 0.40f;
    inline constexpr sf::Color HeraldAuraColor{ 120, 208, 240, 30 };

    // Rift: it holds still, announces, and then it is somewhere else.
    inline constexpr float BlinkInterval = 2.20f;
    inline constexpr float BlinkTell = 0.55f;
    inline constexpr float BlinkSpread = 240.0f;
    inline constexpr sf::Color BlinkColor{ 192, 124, 255 };

    // Coilworm: how much body it tows, how far apart the rings sit, and how
    // wide one of them is.
    inline constexpr int   SerpentSegments = 9;
    inline constexpr float SerpentSpacing = 26.0f;
    inline constexpr float SerpentSegmentRadius = 17.0f;
    inline constexpr const char* SerpentSegmentPath = "assets/textures/enemy_coil_seg.png";
    inline constexpr float SerpentTurn = 150.0f;   // degrees a second the head may swing

    inline constexpr float EnemyHitFlashDuration = 0.12f;
    inline constexpr float EnemyHitFlashPunch = 0.20f;
    inline constexpr sf::Color EnemyHitFlashTint{ 255, 170, 150 };

    // =====================================================================
    //  BOSSES
    //  Per creature numbers live in the boss table (entities/BossDef.cpp).
    //  These are the shared rules every one of them plays by.
    // =====================================================================
    inline constexpr float BossTurnRate = 150.0f;   // degrees per second at full mobility
    inline constexpr float BossFlashDuration = 0.14f;
    inline constexpr float BossAimErrorMax = 34.0f; // degrees of wander once fully blinded
    inline constexpr float BossChargeSpeed = 3.4f;  // speed multiplier during a lunge
    inline constexpr float BossChainRadius = 78.0f; // Chain Reaction splash
    inline constexpr int   BossChainDamage = 8;
    inline constexpr float BossTentacleHitOffset = 62.0f;
    // A shot that misses every part still has to be inside this much of the body
    // to count as turned away, so parts win far more contests than the hide.
    inline constexpr float BossBlockRadiusFactor = 0.72f;
    // Once nothing is shielding an organ, hits anywhere on the animal chip it
    // for this fraction. Before that they simply spark off.
    inline constexpr float BossBodyHitFactor = 0.35f;

    // Once it has swum in, a boss is held this close to the middle so it can
    // never orbit a wall-hugging player straight off the bottom of the view.
    inline constexpr float BossRoamRadius = 430.0f;

    inline constexpr float BossStalkTime = 3.4f;
    inline constexpr float BossWindTime = 0.85f;

    // How long before a committed attack the creature announces it, and what
    // the announcement looks like. A ring closes on the body over this long
    // and a spear grows along the line it is about to take. Blocking used to
    // be a coin toss because nothing said a blow was coming; this is what
    // turns it into something you can read and answer.
    inline constexpr float BossTellTime = 0.62f;
    inline constexpr float BossTellRing = 3.1f;   // ring radius, in body radii, at the start
    inline constexpr float BossTellReach = 2.4f;  // how far ahead a lunge draws its line
    inline constexpr sf::Color BossTellColor{ 255, 146, 92 };
    inline constexpr sf::Color BossTellLungeColor{ 255, 92, 88 };
    inline constexpr float BossStrikeTime = 1.15f;
    inline constexpr float BossRecoverTime = 1.3f;
    inline constexpr float BossFleeTime = 2.4f;
    inline constexpr float BossEnrageThreshold = 0.35f;

    // Degrees per second the Hollow's spiral arms sweep round. Slow enough to
    // be read, fast enough that standing still is never the answer.
    inline constexpr float BossSpiralTurn = 62.0f;

    // The three acts of a fight. Crossing a threshold is one way, and each one
    // adds something rather than replacing what came before, so the last third
    // is everything at once instead of a different encounter.
    inline constexpr float BossActTwo = 0.66f;
    inline constexpr float BossActThree = 0.33f;
    inline constexpr int   BossActArmBonus = 2;

    // Act two: it pulls its own dead off the column and sends them at you.
    inline constexpr float BossSummonInterval = 5.4f;
    inline constexpr EnemyType BossSummonType = EnemyType::Husk;

    // Act three: the basin closes. Shots come inward off the rim, so backing
    // off stops being an answer.
    inline constexpr float BossClosingInterval = 3.1f;
    inline constexpr float BossClosingRadius = 760.0f;
    inline constexpr int   BossClosingArms = 22;

    inline constexpr float BossSurfaceTime = 4.2f;
    inline constexpr float BossSubmergeTime = 1.7f;
    inline constexpr float BossRiseTime = 0.6f;
    inline constexpr float BossSweepRate = 46.0f;
    inline constexpr float BossEmergeDistance = 200.0f;
    inline constexpr sf::Color BossSealedTint{ 168, 176, 200 };

    // =====================================================================
    //  SPAWNING
    //  Creatures always arrive from beyond the visible frame and swim in.
    // =====================================================================
    inline constexpr float SpawnMarginX = 80.0f;
    inline constexpr float SpawnMarginY = 80.0f;
    inline constexpr float MinSpawnDistanceFromPlayer = 380.0f;
    inline constexpr float MinSpawnAngleSeparation = 45.0f;
    inline constexpr int   SpawnPlacementAttempts = 14;

    // =====================================================================
    //  GLOBAL DIFFICULTY SCREWS
    //  Turning these is how the whole campaign gets harder without editing
    //  twenty rows of the level table by hand.
    // =====================================================================
    inline constexpr float GlobalCountScale = 1.45f;    // more of everything
    inline constexpr float GlobalIntervalScale = 0.74f; // arriving sooner
    inline constexpr std::size_t GlobalMaxAliveBonus = 3;

    // Three settings of the same screws. HARD is the campaign as designed and
    // as balanced; the other two sit either side of it so somebody meeting the
    // game for the first time, or somebody who has finished it, both have
    // somewhere to stand. Nothing else in the game reads the difficulty.
    struct DifficultyDef
    {
        const char* name;
        const char* note;
        float enemyHealth;   // multiplies whatever the level table asked for
        float enemyDamage;   // multiplies every wound that reaches the sub
        float countScale;    // multiplies GlobalCountScale
        float salvageScale;
    };

    inline constexpr std::array<DifficultyDef, 3> Difficulties{ {
        { "SURVEY", "FEWER OF THEM, AND THEY HIT SOFTER.", 0.75f, 0.60f, 0.78f, 1.0f },
        { "DESCENT", "THE DIVE AS IT WAS BUILT.", 1.0f, 1.0f, 1.0f, 1.0f },
        { "ABYSSAL", "MORE OF THEM, TOUGHER, AND THEY BITE.", 1.35f, 1.5f, 1.22f, 1.25f },
    } };

    // --- Level flow ---
    inline constexpr float LevelIntroDuration = 2.8f;
    inline constexpr float LevelClearDuration = 2.2f;
    inline constexpr float DeathBannerDuration = 2.6f;
    // --- Depth in a flat scene ---
    // Two extra bands either side of the floor. The numbers are how much of the
    // camera's movement each one gets: under 1 it lags behind and reads as far
    // away, over 1 it leads and reads as being between the camera and the sub.
    // Everything else here only keeps them from competing with the fight.
    inline constexpr float FarParallax = 0.42f;
    inline constexpr float NearParallax = 1.22f;
    inline constexpr std::size_t FarLayerCount = 26;
    inline constexpr std::size_t NearLayerCount = 34;
    inline constexpr sf::Color FarLayerTint{ 92, 112, 124 };
    inline constexpr std::uint8_t FarLayerAlpha = 120;
    inline constexpr sf::Color NearLayerTint{ 150, 186, 196, 40 };

    // A contact shadow under every prop, squashed and offset away from the key
    // light. Without it nothing on the floor looks like it is on the floor.
    inline constexpr sf::Vector2f PropShadowOffset{ 14.0f, 10.0f };
    inline constexpr float PropShadowSquash = 0.42f;
    inline constexpr sf::Color PropShadowColor{ 0, 0, 0, 92 };

    // --- Nests, and going dark ---
    // A nest is a tap that does not move. Killing one is worth crossing the
    // basin for; leaving one is a level that never ends.
    inline constexpr const char* NestTexturePath = "assets/textures/nest.png";
    inline constexpr const char* CargoTexturePath = "assets/textures/cargo.png";

    // --- Hauling ---
    // A pod is dropped at the far end of the basin and has to be dragged back
    // to the mouth. Carrying one costs speed and lights you up, and a landed
    // hit makes you let go of it - which is what makes the run back the level
    // rather than the walk out.
    inline constexpr float CargoRadius = 30.0f;
    inline constexpr float CargoScale = 1.0f;
    inline constexpr float CargoDropInset = 120.0f;  // how far off the wall a pod lies
    inline constexpr float CargoBurden = 0.62f;      // what the sub moves at, carrying
    inline constexpr float CargoNoticeBonus = 1.55f; // how much further it can be heard
    inline constexpr float CargoSalvage = 24;        // paid per pod delivered
    inline constexpr float CargoBob = 2.4f;          // how it rides behind the sub
    inline constexpr float CargoPickupRange = 46.0f;
    inline constexpr float CargoDropRange = 130.0f;  // how near the mouth counts as home
    inline constexpr sf::Color CargoMarkColor{ 255, 196, 96 };
    inline constexpr sf::Color CargoHomeColor{ 132, 240, 190 };
    inline constexpr int   NestHealth = 34;
    inline constexpr float NestRadius = 44.0f;
    inline constexpr float NestHatchInterval = 4.6f;
    inline constexpr int   NestSalvage = 14;
    // A nest reads back from much further than a creature does: it is the size
    // of a room and it is bolted to rock.
    inline constexpr float SonarNestReach = 2.4f;

    // The lamp is what lets something find you. Turning it off is the whole
    // second way to play a level: you are nearly blind, but so is everything
    // hunting you - right up until you fire, which gives you away for a while.
    inline constexpr float LampDarkFactor = 0.26f;
    inline constexpr float NoticeLit = 900.0f;
    inline constexpr float NoticeDark = 300.0f;
    inline constexpr float NoiseDuration = 2.4f;
    inline constexpr float DarkSalvageBonus = 0.35f; // for doing it the hard way

    // How a creature that has lost you behaves: slower than it hunts, on a
    // heading that turns on its own, and not firing at all.
    inline constexpr float WanderSpeedFactor = 0.45f;
    inline constexpr float WanderTurnRate = 55.0f; // degrees per second, either way

    // --- Chilled water ---
    // Never all the way to a stop: a creature you can simply walk away from is
    // not a fight, so the chill line has a ceiling rather than stacking to one.
    inline constexpr float MaxChillStrength = 0.62f;
    inline constexpr float ChillDuration = 2.2f;
    inline constexpr sf::Color ChillTint{ 150, 205, 255 };

    // --- Crits, charges and the rest of the new hardware ---
    inline constexpr float CritRingRadius = 46.0f;
    inline constexpr sf::Color CritRingColor{ 255, 226, 128 };
    inline constexpr float ChargeTime = 0.85f;      // seconds to wind a jet fully
    inline constexpr float ChargeMinimum = 0.35f;   // below this it is just a jet
    inline constexpr float RetaliateShots = 10.0f;
    inline constexpr float CorpseBloomRadius = 96.0f;
    inline constexpr float CorpseBloomFactor = 0.55f; // of the creature's own max health
    inline constexpr float OrbitRadius = 62.0f;
    inline constexpr float OrbitSpeed = 150.0f;       // degrees per second
    inline constexpr float OrbitEatRadius = 16.0f;
    inline constexpr float SecondWindThreshold = 0.25f;
    inline constexpr float SecondWindFireRate = 0.55f; // multiplier on the cooldown
    inline constexpr float SecondWindSpeed = 1.35f;

    // --- Hydrothermal vents ---
    // Cracks in the floor that breathe. They only appear from the second depth
    // down, and there are more of them the deeper it gets, so the arena itself
    // becomes an opponent at exactly the point the creatures stop being enough
    // on their own. Everything they hurt, they hurt equally.
    inline constexpr int   VentsFirstDepth = 2;
    inline constexpr int   VentsPerDepth = 2;
    inline constexpr int   VentsMax = 7;
    inline constexpr float VentRadiusMin = 74.0f;
    inline constexpr float VentRadiusMax = 116.0f;
    inline constexpr float VentIntervalMin = 5.5f;
    inline constexpr float VentIntervalMax = 9.0f;
    inline constexpr float VentEruptTime = 1.15f;
    inline constexpr float VentWarnTime = 1.30f; // glow ramps over this much of the wait
    inline constexpr int   VentDamage = 2;
    inline constexpr float VentEnemyDamage = 6.0f; // it cooks creatures harder than it cooks steel
    inline constexpr sf::Color VentGlowColor{ 236, 132, 62 };

    // --- Salvage economy ---
    // Opening up water nobody has cleared is what pays for the tree. Going back
    // over old ground pays a fraction of the kills and no bonus at all, so the
    // way to afford anything is downward rather than round in circles. Without
    // this a player sits in the shallows with more salvage than the tree has
    // things to sell.
    inline constexpr int   FirstClearBase = 12;
    inline constexpr int   FirstClearPerLevel = 4;
    inline constexpr float RerunSalvageFactor = 0.30f;
    inline constexpr float RespecRefund = 0.70f;

    // --- Particles ---
    inline constexpr const char* ParticleTexturePath = "assets/textures/particle_dot.png";
    inline constexpr std::size_t MaxParticles = 4000;
    inline constexpr float AmbientSnowInterval = 0.03f;
    inline constexpr float AmbientBubbleInterval = 0.5f;

    // --- Audio ---
    inline constexpr const char* AmbientTrackPath = "assets/audio/ambient_abyss.wav";
    inline constexpr std::size_t AudioVoiceCount = 24;
    inline constexpr float VolumeAmbient = 34.0f;
    inline constexpr float VolumeJet = 13.0f;   // fires several times a second, so it stays quiet
    inline constexpr float VolumeImpact = 22.0f;
    inline constexpr float VolumeEnemyDie = 40.0f;
    inline constexpr float VolumePlayerHurt = 70.0f;
    inline constexpr float VolumePartBreak = 75.0f;
    inline constexpr float VolumeLevelClear = 60.0f;
    inline constexpr float VolumeBossRoar = 85.0f;
    inline constexpr float VolumeCreatureCall = 52.0f;
    inline constexpr float VolumeHatch = 46.0f;
    inline constexpr float VolumeVent = 58.0f;
    inline constexpr float VolumeBossPhase = 96.0f;
    inline constexpr float VolumeBossDeath = 100.0f;
    inline constexpr float VolumeCharge = 34.0f;
    inline constexpr float VolumeSiren = 44.0f;

    // Music. Three loops over the same root, faded independently.
    inline constexpr const char* MusicBedPath = "assets/audio/music_bed.wav";
    inline constexpr const char* MusicPulsePath = "assets/audio/music_pulse.wav";
    inline constexpr const char* MusicDreadPath = "assets/audio/music_dread.wav";
    inline constexpr float VolumeMusic = 30.0f;
    inline constexpr float MusicFadeRate = 0.9f; // per second, so a swell takes about one

    // How many creatures actively hunting you counts as full pressure. Low on
    // purpose: three things closing in is already a fight.
    inline constexpr float MusicPulseFull = 4.0f;

    // How often something out in the dark calls. Long gaps on purpose: a noise
    // you hear every few seconds stops being frightening.
    inline constexpr float CreatureCallMin = 14.0f;
    inline constexpr float CreatureCallMax = 32.0f;

    // =====================================================================
    //  PLAYER SYSTEMS
    //  The tree grants a trait; these numbers say how that trait behaves.
    // =====================================================================
    inline constexpr const char* TorpedoTexturePath = "assets/textures/torpedo.png";
    inline constexpr const char* MineTexturePath = "assets/textures/mine.png";

    inline constexpr ProjectileStats Torpedo{ 300.0f, 13.0f, 0.9f };
    inline constexpr int   TorpedoBaseDamage = 4;
    inline constexpr float TorpedoCooldown = 1.9f;
    inline constexpr float TorpedoTurnRate = 190.0f; // degrees per second while homing
    inline constexpr float TorpedoSeekRange = 340.0f;
    inline constexpr int   TorpedoClusterShots = 6;

    inline constexpr float SplitFragmentSpeed = 340.0f;
    inline constexpr float SplitFragmentScale = 0.45f;
    inline constexpr float SplitSpread = 120.0f;

    inline constexpr float BleedDuration = 3.0f;
    inline constexpr float BleedTickInterval = 0.5f;

    inline constexpr float DashSpeed = 900.0f;
    inline constexpr float DashDuration = 0.18f;
    inline constexpr float DashBaseCooldown = 2.2f;
    inline constexpr float PressureBurstRadius = 118.0f;
    inline constexpr int   PressureBurstDamage = 3;

    inline constexpr float ShieldBaseRecharge = 7.0f;
    inline constexpr float ShieldRadius = 33.0f;
    inline constexpr float ReactiveBurstRadius = 130.0f;
    inline constexpr int   ReactiveBurstDamage = 4;
    inline constexpr sf::Color ShieldColor{ 122, 214, 205, 120 };

    inline constexpr float SonarBaseRange = 320.0f;
    inline constexpr float SonarInterval = 2.4f;
    inline constexpr float SonarSweepTime = 0.9f;
    inline constexpr sf::Color SonarPingColor{ 120, 220, 210 };

    inline constexpr float DroneOrbitRadius = 62.0f;
    inline constexpr float DroneOrbitSpeed = 96.0f; // degrees per second
    inline constexpr float DroneFireCooldown = 0.85f;
    inline constexpr float DroneRange = 300.0f;
    inline constexpr int   DroneDamage = 1;

    inline constexpr float MineInterval = 2.6f;
    inline constexpr float MineLifetime = 14.0f;
    inline constexpr float MineTriggerRadius = 34.0f;
    // Every torpedo detonates, not just the cluster upgrade. Without this the
    // only answer to a brooder swarm was a weapon branch you might not own.
    inline constexpr float TorpedoBlastRadius = 118.0f;
    inline constexpr float TorpedoSplashFactor = 0.6f; // of the direct hit

    // How long a hit ring lives and how far it opens.
    // --- Feel ---
    // Shake is in pixels of camera jitter and dies off exponentially; hit stop
    // is measured in seconds of frozen simulation and has to stay short enough
    // that it reads as weight rather than as a dropped frame.
    inline constexpr float ShakeDecay = 9.0f;
    inline constexpr float ShakeMax = 22.0f;
    inline constexpr float ShakeJet = 0.0f;  // a jet is too small to move the camera
    inline constexpr float ShakeTorpedo = 7.0f;
    inline constexpr float ShakePlayerHurt = 13.0f;
    inline constexpr float ShakeBossPart = 10.0f;
    inline constexpr float ShakeDeath = 26.0f;
    inline constexpr float HitStopTorpedo = 0.045f;
    inline constexpr float HitStopPlayerHurt = 0.075f;
    inline constexpr float HitStopBossPart = 0.08f;
    // What a moment is made of. Slow motion is the strongest tool in here and
    // the easiest to overuse, so it is reserved for a fight turning and for the
    // blow that ends one.
    // A hit that does not land. Each save has its own colour so the player can
    // tell at a glance which of the things they bought just paid for itself.
    inline constexpr float BlockRingRadius = 74.0f;
    inline constexpr float HitStopBlock = 0.06f;
    inline constexpr float ShakeBlock = 8.0f;
    inline constexpr float SlowMotionDodge = 0.35f;
    inline constexpr sf::Color BlockShieldColor{ 120, 226, 255 };
    inline constexpr sf::Color BlockArmourColor{ 186, 196, 210 };
    inline constexpr sf::Color BlockDodgeColor{ 168, 255, 214 };
    inline constexpr sf::Color BlockVentColor{ 255, 196, 110 };

    // Momentum: kills without being touched. It is a record of playing well,
    // and the game gets visibly louder the longer it runs.
    inline constexpr int   StreakNotable = 5;   // where it starts being shown
    inline constexpr int   StreakLoud = 12;     // where the screen starts to pulse
    inline constexpr float StreakPitchStep = 0.045f;
    inline constexpr float StreakPitchMax = 1.7f;

    // What a streak is actually worth. Every kill without being touched pays
    // a little more for the next one, and the first hit that lands takes all
    // of it back. That is the whole point: the counter was a number nobody had
    // a reason to care about until losing it cost something.
    inline constexpr float StreakSalvageStep = 0.05f;
    inline constexpr int   StreakSalvageCap = 24;  // +120% and no further

    inline constexpr float SlowMotionScale = 0.34f;
    inline constexpr float SlowMotionBossAct = 1.1f;
    inline constexpr float SlowMotionBossDeath = 2.2f;
    inline constexpr float FlashFade = 2.8f;
    inline constexpr float BigMomentRing = 420.0f;
    inline constexpr float SirenInterval = 1.6f;

    inline constexpr float HurtFlashFade = 2.6f;
    inline constexpr sf::Color HurtFlashColor{ 168, 26, 34 };
    inline constexpr sf::Color PauseVeil{ 2, 7, 12, 208 };

    inline constexpr float CreditsScrollSpeed = 52.0f;
    inline constexpr float CreditsScrollEnd = 740.0f;

    // Threat arrows: how far outside the frame still counts as a threat worth
    // pointing at, and how many can be on screen before it becomes noise. The
    // top inset is deeper because the HUD's own row lives up there.
    // The basin, small, in the corner. It shows where to go - nests, vents, the
    // boss, you - and deliberately not what is coming, which is what the threat
    // arrows are for.
    inline constexpr float MinimapWidth = 214.0f;
    inline constexpr float MinimapHeight = 142.0f;
    inline constexpr float MinimapMargin = 16.0f;
    inline constexpr sf::Color MinimapBackdrop{ 3, 9, 14, 205 };
    inline constexpr sf::Color MinimapWater{ 38, 72, 86, 210 };

    inline constexpr float ThreatArrowMargin = 34.0f;
    inline constexpr float ThreatArrowTopInset = 104.0f;
    inline constexpr float ThreatArrowRange = 1500.0f;
    inline constexpr std::size_t MaxThreatArrows = 10;

    inline constexpr float HitRingDuration = 0.26f;
    inline constexpr float JetRingRadius = 34.0f;

    // Brooders stop hatching once this many of their young are already out, so
    // two of them cannot bury the arena on their own.
    inline constexpr std::size_t MaxSpawnlings = 14;

    inline constexpr float MineBlastRadius = 96.0f;
    inline constexpr int   MineBaseDamage = 6;

    inline constexpr float PredatorThreshold = 0.5f; // fraction of max health
    inline constexpr float PredatorBonus = 0.6f;

    // Where each piece of hardware rides on the hull, in sub-local pixels
    // before the player's own scale is applied. x is forward, y is starboard.
    inline constexpr float AttachmentScale = 0.42f;
    inline constexpr sf::Vector2f MountCannon{ 12.0f, -16.0f };
    inline constexpr sf::Vector2f MountTwin{ 2.0f, 0.0f };
    inline constexpr sf::Vector2f MountTorpedo{ -2.0f, 15.0f };
    inline constexpr sf::Vector2f MountSonar{ 34.0f, 0.0f };
    inline constexpr sf::Vector2f MountArmour{ -4.0f, 0.0f };
    inline constexpr sf::Vector2f MountEngine{ -28.0f, 0.0f };

    // --- HUD ---
    inline constexpr float HudMargin = 30.0f;
    inline constexpr float HudPipRadius = 11.0f;
    inline constexpr float HudPipSpacing = 31.0f;
    inline constexpr sf::Color HudPipFullColor{ 108, 206, 198 };
    inline constexpr sf::Color HudPipFullEdge{ 190, 246, 240 };
    inline constexpr sf::Color HudPipEmptyColor{ 14, 26, 34 };
    inline constexpr sf::Color HudPipEmptyEdge{ 42, 62, 72 };

    inline constexpr float HudTextScale = 2.0f;
    inline constexpr sf::Color HudTextColor{ 130, 206, 200 };
    inline constexpr sf::Color HudDimTextColor{ 68, 104, 112 };
    inline constexpr sf::Color HudSalvageColor{ 226, 196, 116 };
    inline constexpr sf::Color HudBossColor{ 240, 150, 80 };

    inline constexpr float HudBarWidth = 300.0f;
    inline constexpr float HudBarHeight = 6.0f;
    inline constexpr sf::Color HudBarBackColor{ 14, 26, 34 };
    inline constexpr sf::Color HudBarFillColor{ 82, 168, 164 };

    inline constexpr float BossBarWidth = 460.0f;
    inline constexpr float BossBarHeight = 12.0f;
    inline constexpr float BossBarTop = 78.0f;

    // --- Banners ---
    inline constexpr sf::Color BannerVeil{ 2, 8, 14, 170 };
    inline constexpr float BannerTitleScale = 5.0f;
    inline constexpr float BannerLabelScale = 2.2f;
    inline constexpr float BannerBriefScale = 2.0f;

    // --- Upgrade screen ---
    // The tree is drawn around the middle of the screen and panned from there.
    inline constexpr sf::Vector2f TreeOrigin{ 640.0f, 372.0f };
    inline constexpr float TreePanSpeed = 620.0f;

    // How fast the view slides to whatever the arrow keys just picked. Fast
    // enough not to feel like waiting, slow enough to show which way you went.
    inline constexpr float TreeFollowRate = 7.5f;
    inline constexpr float TreePanLimit = 1150.0f;
    // How fast a node's card unrolls, and how fast it rolls back up. Shutting
    // is faster on purpose: a cursor crossing the tree must not leave a trail
    // of half open cards behind it.
    inline constexpr float CardOpenRate = 5.2f;
    inline constexpr float CardShutRate = 9.0f;

    inline constexpr float TreeZoomStep = 1.12f;
    inline constexpr float TreeZoomMin = 0.45f;
    inline constexpr float TreeZoomMax = 1.60f;

    // The hub's level picker.
    inline constexpr float LevelStripY = 556.0f;
    inline constexpr float LevelChipWidth = 38.0f;
    inline constexpr float LevelChipHeight = 30.0f;
    inline constexpr float LevelChipGap = 6.0f;
    inline constexpr int   LevelStripMax = 24;
    inline constexpr float NodeWidth = 132.0f;
    inline constexpr float NodeHeight = 40.0f;
    inline constexpr float NodeRadius = 27.0f;
    // Wide enough for a one line description and tall enough for the mark, the
    // name, that line and the status without any of them touching.
    inline constexpr float NodeCardWidth = 392.0f;
    inline constexpr float NodeCardHeight = 104.0f;

    inline constexpr sf::Color ScreenBackdrop{ 2, 8, 14, 238 };
    inline constexpr sf::Color NodeUnlockedFill{ 20, 62, 60 };
    inline constexpr sf::Color NodeUnlockedEdge{ 130, 226, 216 };
    inline constexpr sf::Color NodeReadyFill{ 14, 34, 42 };
    inline constexpr sf::Color NodeReadyEdge{ 104, 198, 190 };
    inline constexpr sf::Color NodePricyEdge{ 168, 132, 70 };
    inline constexpr sf::Color NodeLockedFill{ 8, 14, 20 };
    inline constexpr sf::Color NodeLockedEdge{ 40, 56, 64 };
    inline constexpr sf::Color LinkOpenColor{ 90, 172, 166 };
    inline constexpr sf::Color LinkShutColor{ 34, 50, 58 };

    // --- Game over ---
    inline constexpr sf::Color DeathOverlayColor{ 60, 8, 14, 150 };
}
