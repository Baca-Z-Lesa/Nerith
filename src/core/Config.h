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

    // --- Time ---
    // The fight runs on a fixed step, so what the game does never depends on
    // how fast the machine happens to be drawing it. Before this, the orbit
    // shield ate one orb per FRAME, every cooldown threw away its remainder,
    // and a creature that lost you wandered further on a slow machine than on
    // a fast one - three different ways of saying the rules changed with the
    // frame rate.
    //
    // A hundred and twenty a second rather than sixty: it halves how far a
    // shot travels between collision checks, and it keeps the quantisation of
    // a fire rate down where nobody can feel it.
    inline constexpr float FixedDeltaTime = 1.0f / 120.0f;

    // How many steps one frame may run before the rest of the backlog is
    // thrown away. Without a cap, a machine that cannot keep up spends longer
    // simulating each frame than the frame took, and falls further behind
    // every time round - the spiral that ends in a locked window.
    inline constexpr int MaxStepsPerFrame = 6;

    // How long the pacer spins rather than sleeps at the end of a frame.
    // sf::sleep rounds to whole milliseconds on Windows, so sleeping the whole
    // way leaves a millisecond of jitter in a frame that is only seven - and
    // that jitter is what reads as the picture not quite keeping still.
    inline constexpr float FrameSpinWindow = 0.0012f;

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
    inline constexpr float MapExtent = 4200.0f;       // how far a walk may wander
    inline constexpr int   RoomCount = 16;
    inline constexpr float ChamberStartRadius = 380.0f; // where a dive begins
    inline constexpr float ChamberRadiusMin = 170.0f;
    inline constexpr float ChamberRadiusMax = 290.0f;
    inline constexpr float ChamberStepMin = 900.0f;
    inline constexpr float ChamberStepMax = 1400.0f;

    // A room is not a circle. Two to four lobes of different sizes pushed off
    // a common centre and unioned, which is what stops the basin reading as a
    // diagram of overlapping discs. The offsets are kept modest on purpose:
    // push them further and consecutive rooms merge into one blob, which is
    // exactly what happened the first time the step was tuned.
    inline constexpr int   LobesMin = 2;
    inline constexpr int   LobesMax = 4;
    inline constexpr float LobeOffsetMin = 0.30f;
    inline constexpr float LobeOffsetMax = 0.62f;
    inline constexpr float LobeScaleMin = 0.52f;
    inline constexpr float LobeScaleMax = 0.88f;

    // A passage bends, and its width is not constant. Straight capsules turn
    // every corridor into a shooting gallery you can see the far end of.
    inline constexpr int   CorridorPiecesMin = 3;
    inline constexpr int   CorridorPiecesMax = 6;
    inline constexpr float CorridorBend = 250.0f;
    inline constexpr float CorridorWidthMin = 0.62f;
    inline constexpr float CorridorWidthMax = 1.45f;

    // How far apart the mouthfuls a passage is cut from sit, as a fraction of
    // its width. Close enough that consecutive ones always overlap even at
    // their narrowest, which is what keeps a corridor watertight.
    inline constexpr float BiteSpacing = 0.62f;

    // Loops are what turn a walk into a maze. Tried rather than placed: a pair
    // has to be near enough to be a shortcut and far enough that it is not a
    // second doorway into the room next door.
    // One room in five or so is much larger than the rest, so the map has a
    // rhythm rather than being a chain of identical caverns.
    inline constexpr float GreatHallChance = 0.20f;
    inline constexpr float GreatHallScale = 1.75f;

    inline constexpr int   LoopAttempts = 26;
    inline constexpr float LoopGapMin = 900.0f;
    inline constexpr float LoopGapMax = 2300.0f;
    inline constexpr float ChamberTurn = 78.0f;       // degrees the walk may swing
    inline constexpr float CorridorRadius = 105.0f;

    // How many times the generator will cut a hole to reach a room the walk
    // left stranded. More than a couple means something is wrong with the
    // walk rather than with this basin.
    inline constexpr int   CarveAttempts = 6;

    // Some passages run. A route that is easy one way and slow the other is
    // what stops a maze being symmetrical - you remember which way the water
    // was going, not just where the turnings were.
    //
    // It used to push at two thirds of the sub's own top speed, which is not a
    // current, it is a hand on your back: you were thrown down a passage you
    // meant to stop in, and swimming back up one left you crawling at a third
    // speed while something bit you. At a fifth of top speed it still tells
    // you which way the water is going and it still costs you something to go
    // the other way, but it never takes the sub off you.
    inline constexpr float CorridorCurrentChance = 0.14f;
    inline constexpr float CurrentStrength = 46.0f;
    inline constexpr float CurrentMouthTaper = 0.30f;

    // How much of it a creature feels. Well under one, because the slowest of
    // them is slower than the water: at full strength a lurker in an upstream
    // passage goes backwards, and a level that wants the water empty can
    // never be finished.
    inline constexpr float CurrentOnCreatures = 0.45f;
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
    inline constexpr float ChamberRipple = 0.30f;

    // How far a room leans off its own centre. Rooms whose widest point is in
    // the middle read as bubbles; rooms with a long side and a short one read
    // as something water cut.
    inline constexpr float ChamberLean = 0.26f;

    // The cut edge, lit. Three bands from outside in: the mass of the rock in
    // shadow, the cut itself, and then a bright line right on the waterline.
    // That last line is the whole point - it is what says "this is as far as
    // you go" without anything having to look like a fence.
    inline constexpr float WallShelf = 130.0f;    // the dark mass behind the cut
    inline constexpr sf::Color RockDeepColor{ 22, 26, 34 }; // pillar shafts only
    inline constexpr float WallRimWidth = 7.0f;

    // How far under the surface a rim point may be and still count as wall,
    // and how far past it the rock is looked for. The first is what stops a
    // buried mouthful drawing a line across open water; the second is what
    // tells a wall from a doorway.
    inline constexpr float WallEdgeDepth = 13.0f;
    inline constexpr float WallEdgeProbe = 30.0f;
    inline constexpr sf::Color WallRimLight{ 190, 216, 228, 132 };

    // And the sand right under the rock sits in its shadow, fading back to open
    // floor. Without this the floor meets the rock as a hard line and the whole
    // basin reads flat; with it, the wall has somewhere to stand.
    inline constexpr float WallShadeWidth = 78.0f;
    inline constexpr sf::Color WallShadeColor{ 2, 6, 11, 190 };

    // Boulders slumped along the foot of the wall. Purely silhouette: they sit
    // outside the water, so nothing can collide with them, and they exist to
    // stop the rim from being a curve.
    inline constexpr int   BouldersPerChamber = 15;
    inline constexpr float BoulderMinChamber = 140.0f;
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

    // A column may never be wider than this share of the room it stands in,
    // and never closer than this to its wall. Between them they are what stops
    // a small room being cut in half by its own scenery.
    inline constexpr float PillarRoomShare = 0.26f;
    inline constexpr float PillarWallGap = 90.0f;
    inline constexpr float PillarCentreGap = 70.0f;

    // Rubble sits between the ripple on the rim and a column standing in the
    // room: lumps of rock bitten out of the wall, so that a wall is never a
    // smooth curve however far it wanders. Kept clear of every doorway.
    inline constexpr float RubblePerChamber = 3.0f;
    inline constexpr float RubbleRadiusMin = 22.0f;
    inline constexpr float RubbleRadiusMax = 52.0f;

    // The gap a lump has to leave across a passage. Two hull widths and a
    // little: enough that a corridor is never a wall, tight enough that it can
    // become somewhere you have to squeeze.
    inline constexpr float RubbleClearance = 74.0f;

    // The pit's pull, in pixels per second at the middle, mostly round and a
    // little inward. Strong enough to have to be flown against, never strong
    // enough to hold anything that commits to leaving.
    inline constexpr float PitStrength = 190.0f;
    inline constexpr sf::Color PitMarkColor{ 132, 196, 210, 42 };

    // Streaks down a passage that runs. Fainter than a pit, because a pit is
    // somewhere you decide to cross and a current is somewhere you notice you
    // are already in.
    inline constexpr sf::Color FlowMarkColor{ 150, 205, 218, 34 };

    // Everything outside the basin is rock, painted as the depth's own floor
    // multiplied down. Darker than the sand by enough that the edge of the map
    // is obvious, and still the depth's colour rather than a flat grey.
    inline constexpr sf::Color RockTint{ 74, 76, 86 };
    inline constexpr float WorldExtent = 5400.0f;   // half-size the seabed covers
    // How far past the edge of the basin the camera is allowed to see. A little
    // overshoot is wanted: rock in frame is what tells the player where the
    // wall is before they hit it.
    // --- The lamp ---
    // The beam is what makes the dark cost something. Half angle wide enough
    // to fight inside and narrow enough that turning to look is a decision;
    // the halo is the spill close in, without which anything closing from
    // behind is unanswerable rather than frightening.
    //
    // The halo used to be an even disc, and an even disc is a circle: the sub
    // sat in a bright ring with a beam stuck on the front of it, and the ring
    // was the thing you looked at. It is a teardrop now - brightest where the
    // lamp is pointed, never quite nothing behind - and it falls off as a
    // square rather than a straight line, so it has no rim to notice.
    inline constexpr float LampConeHalfAngle = 42.0f;   // degrees either side
    inline constexpr float LampConeEdgeFade = 1.35f;    // higher is a softer edge
    // How far you can see around yourself as opposed to in front of you. At
    // 0.26 of a 340 pixel beam that was eighty-eight pixels - about three hull
    // widths - so anything that came at the first floor from outside the cone
    // arrived already touching the sub, which is not darkness, it is a blind
    // spot. Nearly half the beam gives about a second of warning against the
    // slowest thing in the water, which is the least a player can act on.
    inline constexpr float LampHaloFactor = 0.44f;      // of the beam reach
    inline constexpr float LampHaloBrightness = 0.46f;  // of the theme light
    inline constexpr float LampHaloBack = 0.26f;        // of that, directly astern
    inline constexpr int   LampHaloRings = 4;           // steps in the falloff

    // How much light there is where there is none. Zero, now: outside the lamp
    // the water is painted out entirely, so a creature out there is not a dim
    // shape, it is nothing at all. The sonar and the little map are what you
    // have instead, and the lamp is the only thing that turns the dark into a
    // room - which is what makes buying a bigger one worth the salvage.
    inline constexpr float LightFloor = 0.0f;

    // The shape of the dark: the exact complement of the lamp, drawn over the
    // basin after everything in it. The cone's edge is feathered over a few
    // degrees so it is a beam and not a slice of pie, the black fades in over
    // a short band rather than starting on a line, and there is always a small
    // disc of visibility around the hull - you can see your own sub.
    inline constexpr float DarkFeather = 16.0f;  // degrees the cone edge softens over
    inline constexpr float DarkFade = 84.0f;     // pixels the black takes to arrive
    inline constexpr float DarkFloor = 52.0f;    // never blind inside this radius
    inline constexpr float DarkReach = 4200.0f;  // out past any window

    // --- what the rock does to the beam ---
    // The ray starts outside the hull, never strides shorter than this however
    // close the wall says it is, and the black arrives over ShadowFade rather
    // than at a line - ninety-six wedges meeting a curved wall at ninety-six
    // slightly different distances is a saw edge otherwise.
    inline constexpr float ShadowNear = 26.0f;
    inline constexpr float ShadowStep = 9.0f;
    inline constexpr float ShadowFade = 26.0f;

    inline constexpr float CameraOvershoot = 0.55f;
    // How quickly the view catches up with the sub. At five it took about a
    // fifth of a second to close most of the gap, which does not read as a
    // camera following the sub - it reads as the sub not answering the keys.
    // Fourteen still lags enough to sell the weight and stops lying about the
    // controls.
    inline constexpr float CameraLerpLoose = 6.0f;
    inline constexpr float CameraLerpTight = 22.0f;

    // The frame has to hold the light. A finished sonar line throws the beam a
    // thousand pixels and half the window is six hundred and forty, so the
    // player spent the last third of the game lighting water that was off the
    // side of the screen. The view widens with the lamp and with nothing else:
    // at the reach a fresh sub has it does not move at all, which keeps the
    // first floor as tight as it was written to be.
    inline constexpr float CameraLampReference = 340.0f;
    inline constexpr float CameraZoomMax = 1.45f;
    inline constexpr float CameraZoomRate = 2.2f; // per second, exponential
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
    // Scaled with the basin, but not linearly: the props are sprites drawn
    // every frame, so the density drops as the map grows rather than the count
    // tracking the area.
    inline constexpr std::size_t SandGrainCount = 16000;
    inline constexpr std::size_t ScatterPropCount = 1900;
    inline constexpr std::size_t ReefPropCount = 900;
    inline constexpr float ReefOffset = 22.0f;
    inline constexpr float PropFieldRadius = 5000.0f;

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

    // Half the sub's length, which is what the drawing is written against.
    inline constexpr float PlayerHalfLength = 32.0f;
    inline constexpr float PlayerScrewRate = 0.06f; // radians per pixel travelled
    inline constexpr float PlayerSternOffset = 26.0f;
    // Four hits. It was three, which is the right number for a game about
    // learning a level and the wrong one for a game about searching a dark
    // basin with your back to things: half of what kills you now is something
    // you never saw, and a sub that dies to three of those is a sub that
    // punishes exploring. The fourth plate is the difference between a mistake
    // and a run.
    inline constexpr int   PlayerMaxHealth = 4;

    // And longer after one lands, because the thing that actually ends dives
    // is being hit twice by the same creature before the sub has moved.
    inline constexpr float PlayerInvulnerabilityDuration = 0.78f;
    inline constexpr float PlayerBlinkInterval = 0.16f;
    inline constexpr float PlayerBubbleInterval = 0.045f;

    // The sub, moving rather than being moved. A hull with weight in it leans
    // into a turn, stretches when the pumps open up, and never sits perfectly
    // still in water - and none of that is anywhere near the simulation: the
    // shot still leaves from where the aim says it does.
    inline constexpr float SubBankDegrees = 0.085f; // per degree/sec of turn
    inline constexpr float SubBankMax = 11.0f;      // and no further than this
    inline constexpr float SubBankEase = 7.0f;      // how fast the lean catches up
    inline constexpr float SubBankSquash = 0.010f;  // narrowing, per degree of lean
    inline constexpr float SubSurge = 0.055f;       // stretch along the hull at full speed
    inline constexpr float SubIdleSwell = 0.014f;   // breathing, when it is barely moving
    inline constexpr float SubIdleHz = 0.75f;
    inline constexpr float SubWake = 1.5f;          // pixels of sway across the hull
    inline constexpr float SubWakeHz = 1.15f;

    // --- Projectiles ---
    struct ProjectileStats
    {
        float speed;
        float radius;
        float scale;

        // What it does in flight besides go where it was pointed. A jet of
        // pressurised water flickers, a thrown orb of something soft rolls and
        // swells, a torpedo wallows. Zero on all of them is what a shot used
        // to be: a picture of a shot, translated.
        float throb = 0.0f;   // fraction of its own size, each way
        float throbHz = 0.0f;
        float roll = 0.0f;    // degrees of yaw wander off the heading
        float rollHz = 0.0f;
    };

    inline constexpr const char* WaterJetTexturePath = "assets/textures/water_jet.png";
    inline constexpr const char* EnemyOrbTexturePath = "assets/textures/enemy_orb.png";

    inline constexpr ProjectileStats WaterJet{ 620.0f, 8.0f, 0.75f, 0.09f, 9.0f, 2.5f, 6.0f };
    inline constexpr ProjectileStats EnemyOrb{ 245.0f, 9.0f, 0.95f, 0.09f, 3.4f, 7.0f, 1.7f };

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

        // It does not bite you and swim off, it holds on. Kept as a flag
        // rather than as a movement kind, so whatever a creature does to
        // reach you is a separate question from what it does when it gets
        // there - which is what lets the stalker both tow a body and grab.
        bool  latches;

        // How many seconds ahead of the sub it swims. Zero means it goes where
        // you are, which is where you were by the time it arrives; anything
        // above zero means it goes where you are going, and cuts you off at
        // the mouth of the passage instead of trailing you down it.
        float lead;
    };

    // Order must match EnemyType.
    inline constexpr std::array<EnemyStats, EnemyTypeCount> EnemyTable{ {
        // Lurker: the yardstick. Everything else is defined against it.
        { EnemyBehaviour::Chase, "assets/textures/enemy_lurker.png", nullptr,
          0.85f, 16.0f, 124.0f, 4, 1, 1, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f },

        // Lancer: telegraphs for most of a second, then commits to a straight
        // line it cannot correct. It also does the Darter's old job now - the
        // fast fragile thing that punishes standing still - because a
        // telegraphed charge teaches the same lesson and can be read.
        { EnemyBehaviour::Lance, "assets/textures/enemy_lancer.png", nullptr,
          0.75f, 15.0f, 495.0f, 3, 2, 1, 0.0f, 0.0f, 0, 0.0f, 0.45f, 0.80f,
          EnemyType::Lurker, 0, 0.0f, 0, false, false, 0.55f },

        // Weaver: never closes. It circles at range raking fans across the
        // water, so standing still anywhere is the mistake. It holds the
        // Spitter's old reach as well, because a creature that plants itself
        // at a range and stops moving is a turret, not a fight.
        { EnemyBehaviour::Orbit, "assets/textures/enemy_weaver.png", "assets/textures/shot_weaver.png",
          0.75f, 18.0f, 158.0f, 6, 1, 1, 245.0f, 1.25f, 3, 38.0f, 0.0f, 0.0f,
          EnemyType::Lurker, 0, 0.0f, 0, false, false, 0.40f },

        // Bloom: never chases and cannot be dodged by distance. It denies area
        // with rings of orbs, and it lets go of one last ring on the way out -
        // so killing it while standing next to it is its own mistake.
        { EnemyBehaviour::Drift, "assets/textures/enemy_bloom.png", "assets/textures/shot_bloom.png",
          0.7f, 26.0f, 38.0f, 8, 1, 1, 0.0f, 1.95f, 14, 360.0f, 0.0f, 0.0f,
          EnemyType::Lurker, 0, 0.0f, 14, false, false },

        // Spawnling: one jet kills it and it drops nothing. It exists to fill
        // the water and take up the shots you wanted to spend elsewhere.
        { EnemyBehaviour::Chase, "assets/textures/enemy_spawnling.png", nullptr,
          0.55f, 9.0f, 240.0f, 1, 1, 0, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f },

        // Brooder: the tap the spawnlings come out of. Ignore it and the arena
        // fills up; the answer is always to kill the source, not the swarm.
        { EnemyBehaviour::Brood, "assets/textures/enemy_brooder.png", nullptr,
          0.9f, 30.0f, 42.0f, 26, 1, 2, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f,
          EnemyType::Spawnling, 4, 2.5f, 0, false, false },

        // ---------------- BELOW THE CAMPAIGN ----------------
        // Everything from here has a rule rather than a bigger number, and
        // every one of those rules breaks a habit the shallow water taught.

        // Warden: the shield only faces forward. Shooting it head on does
        // nothing at all, so the answer is to get behind it - and it is slow
        // enough to get behind, and hits hard enough that failing to is fatal.
        // It is also the only thing in the water you cannot simply outshoot,
        // which is the job the Hulk used to hold.
        { EnemyBehaviour::Bulwark, "assets/textures/enemy_warden.png", nullptr,
          0.95f, 38.0f, 74.0f, 34, 3, 2, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f,
          EnemyType::Lurker, 0, 0.0f, 0, false, false, 0.90f },

        // Revenant: untouchable while it is under the floor, and it is under
        // the floor most of the time. The fight is entirely about the few
        // seconds it is up - and when it is up it is beside you, barely
        // visible, and emptying a fan into the space you were standing in.
        { EnemyBehaviour::Burrow, "assets/textures/enemy_revenant.png", "assets/textures/shot_rift.png",
          0.85f, 25.0f, 168.0f, 28, 3, 2, 300.0f, 1.60f, 7, 54.0f, 1.30f, 2.40f,
          EnemyType::Lurker, 0, 0.0f, 0, true, false },

        // Herald: it has nothing to bite with and it is the most dangerous
        // thing in the water. It hardens everything near it and it keeps you
        // lit, so running dark stops working until it is dead.
        { EnemyBehaviour::Chase, "assets/textures/enemy_herald.png", nullptr,
          0.9f, 27.0f, 58.0f, 40, 1, 3, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f },

        // ---------------- BELOW THE ABYSS ----------------

        // Moth: it has no eyes and it does not need them. In the dark it is a
        // slow drifting nothing that will not even find you if you swim into
        // it; in the beam it is the fastest thing in the trench and it does not
        // stop. Everything above it made the lamp a cost. This one makes it a
        // mistake, and the answer is the one thing twenty levels of dark water
        // taught the player never to do: turn the light off and keep going.
        { EnemyBehaviour::Moth, "assets/textures/enemy_moth.png", nullptr,
          0.8f, 20.0f, 380.0f, 10, 3, 2, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f,
          EnemyType::Lurker, 0, 0.0f, 0, false, false, 0.30f },

        // Effigy: it is only ever where you left it. It closes while the sub
        // is under way and it stops dead the moment the sub does, so the way
        // through a room full of them is to stand still - which is the one
        // thing that gets you killed by everything else in the water, and is
        // exactly why they are never sent on their own.
        { EnemyBehaviour::Creep, "assets/textures/enemy_effigy.png", nullptr,
          1.0f, 30.0f, 306.0f, 22, 3, 3, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f,
          EnemyType::Lurker, 0, 0.0f, 0, false, false },

        // Stalker: nothing puts it in the water. It is there because you left
        // it alive somewhere above, and it has been following you down ever
        // since. The head is only how it steers - the body laid along where it
        // has already been is the weapon - and if it reaches you it does not
        // bite once, it takes hold.
        { EnemyBehaviour::Serpent, "assets/textures/enemy_coilworm.png", nullptr,
          0.85f, 24.0f, 198.0f, 46, 3, 4, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0.0f,
          EnemyType::Lurker, 0, 0.0f, 0, false, true, 1.20f },
    } };

    // =====================================================================
    //  HOW A THING MOVES WHILE IT IS MOVING
    //  Everything in the water was a texture being slid across it: right
    //  place, right angle, and about as alive as a road sign. None of what
    //  follows touches a position, a hitbox or a heading - it is applied to
    //  the sprite after the step has finished with it - so none of it can
    //  change how a fight goes. It only makes the thing look like it is the
    //  one doing the swimming.
    //
    //  Keyed by behaviour rather than by creature, because how something
    //  moves is already the thing behaviour describes: chasers beat a tail,
    //  bells pulse, and a shield being walked forward plods.
    // =====================================================================
    struct Motion
    {
        float sway;    // degrees of yaw either side of where it is pointed
        float swayHz;
        float pulse;   // fraction of its own size it swells and shrinks by
        float pulseHz;
        float bob;     // pixels across its heading
        float bobHz;
    };

    // Order must match EnemyBehaviour.
    inline constexpr std::array<Motion, 10> BehaviourMotion{ {
        { 7.0f, 2.20f, 0.030f, 2.20f, 1.2f, 1.10f }, // Chase: a tail, beating
        { 2.0f, 0.85f, 0.070f, 0.80f, 2.0f, 0.45f }, // Drift: a bell, breathing
        { 5.0f, 1.60f, 0.025f, 3.20f, 1.6f, 0.80f }, // Orbit: leaning its turn
        { 3.0f, 5.40f, 0.045f, 2.60f, 0.0f, 0.00f }, // Lance: wound up, shivering
        { 1.5f, 0.60f, 0.060f, 0.55f, 0.9f, 0.35f }, // Brood: slow and swollen
        { 2.0f, 1.05f, 0.018f, 1.05f, 2.4f, 1.05f }, // Bulwark: a heavy plod
        { 6.0f, 2.00f, 0.030f, 2.00f, 1.4f, 1.00f }, // Burrow
        { 9.0f, 2.40f, 0.025f, 2.40f, 0.0f, 0.00f }, // Serpent: the whole animal
        { 14.0f, 5.20f, 0.060f, 5.20f, 3.0f, 4.20f }, // Moth: a flutter, not a swim
        { 0.8f, 0.40f, 0.045f, 0.38f, 0.0f, 0.00f },  // Creep: barely breathing
    } };

    inline constexpr const Motion& motionFor(EnemyBehaviour behaviour)
    {
        return BehaviourMotion[static_cast<std::size_t>(behaviour)];
    }

    // Everything above is scaled by this, and so is the sub and so are the
    // shots. One knob for how alive the water is, and setting it to zero puts
    // every sprite back to being slid along its line.
    inline constexpr float MotionAmount = 1.0f;

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
        CreatureVoice::Shell,   // Lancer
        CreatureVoice::Wet,     // Weaver
        CreatureVoice::Bladder, // Bloom
        CreatureVoice::Soft,    // Spawnling
        CreatureVoice::Bladder, // Brooder
        CreatureVoice::Shell,   // Warden
        CreatureVoice::Void,    // Revenant
        CreatureVoice::Bladder, // Herald
        CreatureVoice::Wet,     // Moth
        CreatureVoice::Shell,   // Effigy
        CreatureVoice::Shell,   // Stalker
    } };

    inline constexpr CreatureVoice voiceOf(EnemyType type)
    {
        return EnemyVoices[static_cast<std::size_t>(type)];
    }

    inline constexpr const EnemyStats& enemyStats(EnemyType type)
    {
        return EnemyTable[static_cast<std::size_t>(type)];
    }

    inline constexpr float EnemyApproachBoost = 2.6f;
    inline constexpr float EnemyRoamMargin = 95.0f;

    inline constexpr float BloomSpinSpeed = 42.0f;
    inline constexpr float OrbitCloseFactor = 0.6f;   // how hard an orbiter corrects its range
    inline constexpr float LanceCreepFactor = 0.14f;  // speed while it winds up
    inline constexpr float StealthRevealRange = 300.0f; // inside this a revenant is visible

    // --- What follows you down ---
    // Leave this many creatures alive in water this deep and one of them comes
    // with you. It is never rolled for: it is the price of finishing a level
    // by walking away from it rather than by emptying it.
    // --- the two below the abyss ---
    // How much lamp has to be on a moth before it wakes, and what fraction of
    // its speed it has while it is asleep. Not zero: a creature that can be
    // switched off outright is a creature a level can be won by ignoring, and
    // a clear-the-roster level with one in it would never end.
    inline constexpr float MothWakeLight = 0.34f;
    inline constexpr float MothSleepSpeed = 0.07f;

    // And how fast the sub has to be going before an effigy moves at all. Set
    // under a drifting sub's speed rather than at zero, so easing off the
    // throttle is not the same as stopping.
    inline constexpr float EffigyWakeSpeed = 96.0f;

    inline constexpr int   StalkerFromDepth = 2;
    inline constexpr int   StalkerLeftBehind = 3;

    // What each dive it survives adds. It is the same creature every time, so
    // the escalation has to be in the numbers rather than in the roster - and
    // it has to be steep enough that running from it twice is a bad idea.
    inline constexpr float StalkerHealthPerLevel = 0.30f;
    inline constexpr float StalkerSpeedPerLevel = 0.06f;
    inline constexpr int   StalkerDamagePerLevel = 1;
    inline constexpr int   StalkerMaxLevels = 6;
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

    // How the sub answers the keys. High enough that a whole hull feels
    // immediate - a fortieth of a second to most of its speed, which nobody
    // can feel - and low enough that the thing being steered reads as a
    // submarine rather than as a cursor.
    //
    // Six more numbers used to live here, for the system a hit knocked out at
    // each of six health thresholds: sonar, tube, lamp, rudder, gun, engine.
    // It is gone. Being hurt already costs a life, the streak, the cargo you
    // were carrying and the mercy window; taking the lamp and the engine away
    // on top of that punished the player hardest exactly when they were least
    // able to recover, and a spiral that starts on the first bite is not a
    // difficulty curve.
    inline constexpr float PlayerAccel = 26.0f;
    inline constexpr float PlayerDrag = 19.0f;

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


    // The ring a revenant pushes ahead of itself on the way up.
    inline constexpr sf::Color RevenantTellColor{ 192, 124, 255 };

    // Coilworm: how much body it tows, how far apart the rings sit, and how
    // wide one of them is.
    inline constexpr int   SerpentSegments = 9;
    inline constexpr float SerpentSpacing = 26.0f;
    inline constexpr float SerpentSegmentRadius = 17.0f;
    inline constexpr const char* SerpentSegmentPath = "assets/textures/enemy_coil_seg.png";

    // A body made of identical rings does not end, it stops - which is exactly
    // what the thing looked like: a string of beads somebody cut. The last ring
    // is followed by this, and this is a tail.
    inline constexpr const char* SerpentTailPath = "assets/textures/enemy_coil_tail.png";
    inline constexpr float SerpentTailScale = 1.30f;  // of a ring
    inline constexpr float SerpentTailReach = 0.85f;  // of a spacing, past the last ring
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
    // Boss parts are numbered from here downward so their tokens can never
    // collide with a creature id, which counts upward from zero.
    inline constexpr int   BossHitToken = -1000;

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
    // --- What each creature does that is not a shot ---
    // One special per creature every so often, alternating between its two, on
    // a clock that tightens with every act. These are the moves that move the
    // fight rather than fill the water: they are worth telegraphing and they
    // are worth being afraid of.
    inline constexpr float BossSpecialInterval = 7.4f;
    inline constexpr float BossSpecialFirst = 4.0f;   // the first one comes sooner

    // The moves a boss keeps back. From the second act on, and on a slow clock
    // of their own, so the second half of a fight has a rhythm rather than
    // simply more of everything.
    // The body flexing as it swims, rather than being towed round the basin in
    // one rigid piece. Rotation only, and only on the sprite: the links the
    // shots are tested against and the fins are bolted to do not move.
    inline constexpr float BossFlexDegrees = 5.0f;
    inline constexpr float BossFlexHz = 0.55f;
    inline constexpr float BossFlexLag = 0.85f; // radians of wave per link

    // --- the ring ---
    // A closed circle of body that turns and tightens rather than a chain
    // that trails. How wide it stands, how much of that it gives up by the
    // last act, how fast it turns, and how much it breathes in and out round
    // its own circumference - a perfect wheel is a machine, and this is an
    // animal.
    inline constexpr float BossRingRadius = 190.0f;
    inline constexpr float BossRingClose = 0.34f;
    inline constexpr float BossRingSpin = 0.34f;   // degrees per frame at act zero
    inline constexpr float BossRingWobble = 0.11f; // of the radius

    // --- the split ---
    // Two halves on the same slow swing, half a turn out of phase, so one is
    // never where the other is. The gap opens with every act: by the last one
    // it is not an animal, it is two things that used to be.
    inline constexpr float BossSplitGap = 244.0f;
    inline constexpr float BossSplitSwing = 0.42f; // radians a second
    inline constexpr float BossSplitArm = 104.0f;
    inline constexpr float BossSplitCurl = 1.35f;  // radians the arc bends over

    // --- the tower ---
    // A chimney that never leaves the hole it is sitting on. What it does
    // instead of chasing is grow, so the room the player has left shrinks
    // without the creature taking a step.
    inline constexpr float BossTowerBase = 132.0f;
    inline constexpr float BossTowerGrowth = 0.55f;
    inline constexpr float BossTowerTaper = 0.22f; // each course in by this much
    inline constexpr float BossTowerSpin = 0.16f;  // degrees a frame
    inline constexpr float BossTowerBoil = 0.09f;
    inline constexpr std::size_t BossTowerPerCourse = 4;

    // --- the coil ---
    // Wound on the floor at rest and thrown out straight mid strike, which is
    // what a bobbit worm actually is: a hole with a body in it.
    inline constexpr float BossCoilPitch = 18.0f;  // back along its own axis per link
    inline constexpr float BossCoilOpen = 1.15f;   // how much longer it gets striking
    inline constexpr float BossCoilWind = 1.18f;   // radians of turn per link
    inline constexpr float BossCoilWidth = 72.0f;

    // How wide the burst is when a creature crosses into its next act.
    inline constexpr float BossTurnBurst = 210.0f;

    // For the bodies whose links are laid away from the animal rather than
    // behind it, the hit radius comes off the girth the art is drawn at
    // instead of off the head. Roughly the widest the drawn shape gets.
    inline constexpr float BossLimbHitFactor = 1.9f;

    // How much brighter a mote gets for being in the beam. High enough that
    // the shaft is visible in open water and low enough that it never washes
    // out what the lamp is actually lighting.
    inline constexpr float BeamMoteLift = 0.85f;

    // --- what the water takes out of the light ---
    // Seawater eats red first, then green, and keeps blue longest, which is
    // why everything down here is blue and why a red thing far away is not a
    // dim red thing, it is a grey one. Multiplied over the frame between the
    // entities and the dark.
    //
    // Nothing inside AbsorbClear of the lamp's reach is touched, so the sub
    // keeps its own colours; full by AbsorbFull.
    inline constexpr float AbsorbRed = 0.62f;
    inline constexpr float AbsorbGreen = 0.86f;
    inline constexpr float AbsorbBlue = 1.00f;
    inline constexpr float AbsorbClear = 0.40f;  // of the lamp's reach
    inline constexpr float AbsorbFull = 1.25f;

    inline constexpr int   BossUltraFromAct = 1;
    inline constexpr float BossUltraInterval = 15.0f;

    // A ring that falls in on the sub from every side. The gap is what makes
    // it a route rather than a punishment.
    inline constexpr float ClosingRadius = 620.0f;
    inline constexpr int   ClosingOrbs = 30;
    inline constexpr float ClosingSpeed = 168.0f;
    inline constexpr float ClosingGap = 44.0f;        // degrees left open

    // A wall that crosses the hall with one hole in it.
    inline constexpr float CurtainWidth = 1500.0f;
    inline constexpr int   CurtainOrbs = 22;
    inline constexpr float CurtainSpeed = 214.0f;
    inline constexpr float CurtainGap = 150.0f;       // pixels of hole

    // The floor tearing open along a line, a step at a time.
    inline constexpr int   FissureSteps = 9;
    inline constexpr float FissureStride = 150.0f;
    inline constexpr float FissureDelay = 0.16f;

    // A beam swept across the hall. Slow enough to walk out of, long enough
    // that standing still is not an option.
    // The tail coming round, thrown from the tail rather than the head: in
    // front of the animal is the safe place, which is the opposite of every
    // other move it has.
    inline constexpr float ThreshArc = 280.0f;   // degrees the tail covers
    inline constexpr int   ThreshSteps = 22;
    inline constexpr float ThreshDelay = 0.05f;
    inline constexpr float ThreshReach = 120.0f;
    inline constexpr float ThreshSpeed = 250.0f;

    // And the roar: three walls straight down its own line, widening.
    inline constexpr int   SoundWaves = 3;
    inline constexpr int   SoundPerWave = 7;
    inline constexpr float SoundSpread = 230.0f;
    inline constexpr float SoundGap = 0.22f;
    inline constexpr float SoundSpeed = 420.0f;

    // How far down its own line the warning is painted. Roughly where the
    // first wave has got to by the time the last one leaves.
    inline constexpr float SoundReach = 520.0f;

    inline constexpr int   RakeSteps = 26;
    inline constexpr float RakeArc = 190.0f;          // degrees swept
    inline constexpr float RakeDelay = 0.07f;
    inline constexpr float RakeSpeed = 330.0f;

    // Every act the creature presses harder: this much off every gap between
    // anything it does, compounding, and again on top of its rage.
    inline constexpr float BossActPressure = 0.16f;

    inline constexpr float GrabDuration = 1.5f;   // how long it holds you
    inline constexpr float GrabBiteInterval = 0.42f;
    inline constexpr int   GrabBite = 2;
    inline constexpr float GrabReach = 132.0f;

    inline constexpr float SweepRadius = 460.0f;
    inline constexpr float SweepArc = 110.0f;     // degrees either side of its heading
    inline constexpr int   SweepDamage = 5;
    inline constexpr float SweepPush = 620.0f;

    inline constexpr int   SeedCount = 4;
    inline constexpr int   SeedHealth = 14;
    inline constexpr float SeedSpread = 330.0f;

    inline constexpr float TidePush = 940.0f;
    inline constexpr float TideRadius = 900.0f;

    inline constexpr float SiltDuration = 4.5f;
    inline constexpr float SiltLamp = 0.30f;      // what the lamp is cut to

    inline constexpr float EruptWarning = 1.05f;  // how long the floor bulges first
    inline constexpr float EruptRadius = 190.0f;
    inline constexpr int   EruptDamage = 9;

    inline constexpr float WellDuration = 2.4f;
    inline constexpr float WellPull = 340.0f;

    inline constexpr float HurlSpeed = 720.0f;
    inline constexpr int   HurlDamage = 8;
    inline constexpr float HurlRadius = 26.0f;

    inline constexpr sf::Color GrabColor{ 255, 84, 72 };
    inline constexpr sf::Color SweepColor{ 150, 214, 255 };
    inline constexpr sf::Color TideColor{ 120, 200, 232 };
    inline constexpr sf::Color WellColor{ 196, 128, 255 };
    inline constexpr sf::Color EruptColor{ 224, 186, 118 };

    inline constexpr float BossTellTime = 0.62f;
    inline constexpr float BossTellRing = 3.1f;   // ring radius, in body radii, at the start
    inline constexpr float BossTellReach = 2.4f;  // how far ahead a lunge draws its line
    inline constexpr sf::Color BossTellColor{ 255, 146, 92 };
    inline constexpr sf::Color BossTellLungeColor{ 255, 92, 88 };
    inline constexpr float BossStrikeTime = 1.15f;
    inline constexpr float BossRecoverTime = 1.3f;
    inline constexpr float BossFleeTime = 2.4f;
    inline constexpr float BossEnrageThreshold = 0.35f;

    // Degrees per second the spiral arms sweep round. Slow enough to be read,
    // fast enough that standing still is never the answer.
    inline constexpr float BossSpiralTurn = 62.0f;

    // --- The Hollow: a closed ring with one way out of it ---
    // It fires a complete circle of shots with a single gap, and the gap walks
    // round. The answer is never a dodge - it is being in the one place the
    // ring is not by the time it goes off, which is a different thing to learn
    // from every other fight in the game. Enraged, the gap reverses and
    // narrows, so everything learned about which way to run stops being true.
    inline constexpr int   BossRingSpokes = 26;
    inline constexpr float BossGapWide = 58.0f;  // degrees of opening
    inline constexpr float BossGapDrift = 44.0f; // degrees a second it walks

    // A whole ring is twenty-odd shots at once, so it fires on its own clock
    // rather than on the creature's attack cooldown - at that cooldown it put
    // sixty-six shots a second into the water, which is not a fight, it is a
    // wall. This is the interval between rings, and it is long enough to read
    // where the opening has walked to and get there.
    inline constexpr float BossRingInterval = 2.4f;

    // --- The Furnace: a gun emplacement ---
    // It does not chase. It holds its ground and turns, and the fight is about
    // the ground rather than about it: stay out of the arcs, punish between
    // them, and never stand on it - anything close when it vents is thrown off.
    inline constexpr float BossBastionHold = 0.16f;  // creep while you are near
    inline constexpr float BossBastionRange = 1.6f;  // past this it follows
    inline constexpr float BossVentInterval = 6.2f;
    inline constexpr int   BossVentShots = 18;
    inline constexpr float BossBastionSpread = 1.9f; // wider arcs than anything

    // Artillery is slow. On the creature's own cooldown these arcs came out
    // three times a second, which reads as a hose rather than as a gun being
    // laid, and a wide arc fired that often cannot be walked out of at all.
    inline constexpr float BossBastionCadence = 2.6f;

    // --- The Mother: two lobes, leading in turn ---
    // It never comes at you in a straight line: the approach is a weave, and
    // the two lobes fire together with their aims toed slightly apart, so the
    // gap between the volleys closes as it gets nearer.
    inline constexpr float BossWeaveRate = 1.45f;   // radians a second
    inline constexpr float BossWeaveWidth = 210.0f; // pixels either side
    inline constexpr float BossLobeOffset = 92.0f;  // how far apart they fire
    inline constexpr float BossLobeToe = 9.0f;      // degrees each aim is turned

    // The three acts of a fight. Crossing a threshold is one way, and each one
    // adds something rather than replacing what came before, so the last third
    // is everything at once instead of a different encounter.
    inline constexpr float BossActTwo = 0.66f;
    inline constexpr float BossActThree = 0.33f;
    inline constexpr int   BossActArmBonus = 2;

    // Act two: it pulls its own dead off the column and sends them at you.
    inline constexpr float BossSummonInterval = 5.4f;
    inline constexpr EnemyType BossSummonType = EnemyType::Bloom;

    // Act three: the basin closes. Shots come inward off the rim, so backing
    // off stops being an answer.
    // How long a bell that is not a drifter stays shut and then open. Shut is
    // multiplied by aggression, so it comes round faster as the thing is hurt;
    // open widens with the act, so the last third of the fight is the part
    // where the player finally gets to land something.
    // --- a boss that is drawn rather than stamped ---
    // How wide the body is at a link of scale one, and how the rest of the
    // animal behaves while it is being an animal. None of these are read by
    // anything that decides what happens: they are what a living thing does
    // between decisions.
    inline constexpr float BossDrawnGirth = 29.0f;
    inline constexpr float BossBreathRate = 0.34f;  // cycles a second, unhurt
    inline constexpr float BossBreathRage = 0.22f;  // added per act
    inline constexpr float BossBeatRate = 0.95f;    // fin strokes a second
    inline constexpr float BossFlinchFade = 3.4f;
    inline constexpr float BossLookLimit = 52.0f;   // degrees a head can turn
    inline constexpr float BossLookEase = 7.0f;

    // How hard it rolls its open side away from the sub, in degrees a second.
    inline constexpr float BossGuardTurn = 95.0f;

    // Nothing in the trench dies the first time it runs out of life. The bar
    // empties, the animal comes apart, and what is standing there afterwards
    // is faster, hits sooner and does not look like what you were fighting.
    //
    // The second pool is deliberately smaller than the first: this is the last
    // thirty seconds of a fight, not another fight.
    inline constexpr float BossRebornHealth = 0.52f;  // of the pool it just lost
    inline constexpr float BossRebornSpeed = 1.45f;
    inline constexpr float BossRebornPressure = 0.55f; // multiplies every gap
    inline constexpr int   BossRebornShots = 3;
    inline constexpr float BossShedTime = 1.2f;        // how long the change takes

    inline constexpr float BossBellShut = 4.6f;
    inline constexpr float BossBellOpen = 2.2f;

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
    // --- Keeping up with the sub ---
    // What the water gains as the tree fills. A level is written against a
    // level number, and a level number says nothing about how much gun turned
    // up to it: the tree is permanent, so a player who has ground the shallows
    // meets the abyss with several times the sub the curve was drawn for. This
    // is what stops the deepest water in the game being the easiest thing in
    // it - and, because the base curve came down to make room for it, what
    // stops the shelf being the hardest.
    inline constexpr float PowerScaleHealth = 1.30f;
    inline constexpr float PowerScaleSpeed = 0.22f;
    inline constexpr float PowerScaleDamage = 2.0f;

    // How much of the tree counts as a full sub. Not all of it: the last few
    // nodes are a long grind, and somebody who has bought most of one is
    // already driving the thing the curve has to answer.
    inline constexpr float PowerFullShare = 0.72f;

    // Three screws over the whole campaign, and they were all wound the wrong
    // way. The level table was authored and balanced one level at a time, and
    // then these were laid on top of every row of it: half again as many
    // creatures, arriving a quarter sooner, and - the one that actually did
    // the damage - three MORE of them alive at once than any level ever asked
    // for. Level one says five and the game was running eight.
    //
    // A player is not fighting the level table, they are fighting the level
    // table multiplied by this, so this is where "practically impossible"
    // lived.
    //
    // All three are neutral now and the curve has been written back into the
    // rows themselves, where a designer can see it. They are kept because one
    // dial over the whole campaign is genuinely useful - but the dial is at
    // one, so what the table says is what the game does.
    inline constexpr float GlobalCountScale = 1.0f;
    inline constexpr float GlobalIntervalScale = 1.0f;
    inline constexpr std::size_t GlobalMaxAliveBonus = 0;

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
        // The salvage column is the price of the setting. It used to pay the
        // same on SURVEY as on DESCENT, which made the easy setting free: the
        // same tree in the same number of dives for less work. It now trades,
        // and the three read monotonically from left to right.
        //
        // ABYSSAL's health screw also lands on boss pools, and at 1.35 it
        // turned the last fight from the two minutes it was tuned to into
        // nearly three - which is not a harder fight, it is a longer one.
        { "SURVEY", "FEWER OF THEM, AND THEY HIT SOFTER.", 0.75f, 0.60f, 0.78f, 0.85f },
        { "DESCENT", "THE DIVE AS IT WAS BUILT.", 1.0f, 1.0f, 1.0f, 1.0f },
        { "ABYSSAL", "MORE OF THEM, TOUGHER, AND THEY BITE.", 1.22f, 1.5f, 1.22f, 1.25f },
    } };

    // --- Level flow ---
    inline constexpr float LevelIntroDuration = 2.8f;
    inline constexpr float LevelClearDuration = 2.2f;
    inline constexpr float DeathBannerDuration = 2.6f;
    // --- Depth in a flat scene ---
    // Three extra bands around the floor. The numbers are how much of the
    // camera's movement each one gets: under 1 it lags behind and reads as far
    // away, over 1 it leads and reads as being between the camera and the sub.
    // Everything else here only keeps them from competing with the fight.
    //
    // The deep band is the one that says the basin is somewhere rather than
    // everywhere: enormous shapes barely moving at the limit of sight, so far
    // back that crossing the whole floor hardly shifts them. Two bands left the
    // horizon ending at the far rocks; three give it somewhere to keep going.
    inline constexpr float DeepParallax = 0.15f;
    inline constexpr float FarParallax = 0.42f;
    inline constexpr float NearParallax = 1.22f;
    inline constexpr std::size_t DeepLayerCount = 14;
    inline constexpr std::size_t FarLayerCount = 26;
    inline constexpr std::size_t NearLayerCount = 34;
    inline constexpr sf::Color DeepLayerTint{ 58, 74, 88 };
    inline constexpr std::uint8_t DeepLayerAlpha = 66;
    inline constexpr sf::Color FarLayerTint{ 92, 112, 124 };
    inline constexpr std::uint8_t FarLayerAlpha = 120;
    inline constexpr sf::Color NearLayerTint{ 150, 186, 196, 40 };

    // A contact shadow under every prop, squashed and offset away from the key
    // light. Without it nothing on the floor looks like it is on the floor.
    // How far past the edge of the view a prop is still drawn. Wide enough
    // for the tallest piece of dressing plus its shadow, so nothing ever pops
    // in at the edge of the window.
    inline constexpr float PropCullMargin = 320.0f;

    inline constexpr sf::Vector2f PropShadowOffset{ 14.0f, 10.0f };
    inline constexpr float PropShadowSquash = 0.42f;
    inline constexpr sf::Color PropShadowColor{ 0, 0, 0, 92 };

    // --- Nests, and going dark ---
    // A nest is a tap that does not move. Killing one is worth crossing the
    // basin for; leaving one is a level that never ends.
    // The things standing on the floor of a basin. Every one of these was a
    // fan of triangles until there was a picture of it; see tools/make_gear.ps1.
    inline constexpr const char* WreckTexturePath = "assets/textures/gear_wreck.png";
    inline constexpr const char* HiveTexturePath  = "assets/textures/gear_hive.png";
    inline constexpr const char* CacheTexturePath = "assets/textures/gear_cache.png";
    inline constexpr const char* ShaftTexturePath = "assets/textures/gear_shaft.png";
    inline constexpr const char* VentTexturePath  = "assets/textures/gear_vent.png";

    inline constexpr const char* NestTexturePath = "assets/textures/nest.png";
    inline constexpr const char* CargoTexturePath = "assets/textures/cargo.png";

    // The picture Windows puts on the shortcut, and the one the window carries.
    // Outside textures/ because it is not drawn in the water and is not lit by
    // the pass that lights everything in there.
    inline constexpr const char* IconPath = "assets/icon.png";

    // The surface the drawn bosses are made of. One tiling sheet, near white,
    // with the scale and the grain cut into it darker - it is multiplied by
    // whatever colour the shape wanted, so hide, plate, fin and jaw all differ
    // by tint and share a surface, the same way the sprites all share the
    // relief pass. How many world pixels one tile covers is what decides how
    // big a scale is on the animal.
    inline constexpr const char* BossHidePath = "assets/textures/boss_hide.png";
    inline constexpr float BossHideTile = 74.0f;

    // How far either side of flat a drawn surface swings as it turns toward
    // the light or away from it. Enough to give a body a top and an underside,
    // short of enough to read as two different colours of animal.
    inline constexpr float ShadeDepth = 0.34f;

    // Three more surfaces, because one sheet meant the jellyfish had fish
    // scales, the crab had fish scales and the blind moth had fish scales. A
    // surface is most of what says what an animal is made of.
    inline constexpr const char* HidePlatePath = "assets/textures/hide_plate.png";
    inline constexpr const char* HideSoftPath  = "assets/textures/hide_soft.png";
    inline constexpr const char* HideFurPath   = "assets/textures/hide_fur.png";

    // =====================================================================
    //  SALVAGE
    //  What is actually down there and what it is worth on the deck. Killing
    //  things pays, but killing things has never been why anybody goes into a
    //  trench: they go in because there is something in it, and they come out
    //  when what they are carrying is worth more than the next room is.
    //
    //  Everything here is picked up by swimming over it, carried in the hold,
    //  and lost in one piece if the sub does not come back up. Nothing is
    //  yours until it is on the deck.
    // =====================================================================
    struct SalvageDef
    {
        const char* name;
        const char* path;      // what it looks like lying on the floor
        sf::Color   colour;
        int         value;
        float       share;     // how often it is the one that turns up
        int         fromDepth; // and how deep you have to be before it does
    };

    // These were 14 to 370, which sounds like a range and was in fact the
    // whole economy: a swept floor at depth six paid a quarter of the upgrade
    // tree by itself, and two thirds of everything a player ever earned came
    // from swimming over things rather than from fighting or from opening new
    // water. What is left is the same shape at a fifth of the size.
    inline constexpr std::array<SalvageDef, 6> SalvageTable{ {
        { "SCRAP",       "assets/textures/find_scrap.png",  { 132, 142, 148 },  2, 1.00f, 1 },
        { "COPPER LINE", "assets/textures/find_copper.png", { 190, 128,  86 },  4, 0.72f, 1 },
        { "CORE ROD",    "assets/textures/find_rod.png",    { 126, 200, 184 },  7, 0.48f, 2 },
        { "BLACK GLASS", "assets/textures/find_glass.png",  { 118, 102, 168 }, 12, 0.30f, 3 },
        { "GRAVID SAC",  "assets/textures/find_sac.png",    { 216, 152, 162 }, 20, 0.18f, 4 },
        { "NERITH ORE",  "assets/textures/find_ore.png",    { 240, 178,  98 }, 33, 0.10f, 5 },
    } };

    inline constexpr std::size_t SalvageKinds = SalvageTable.size();

    // How much of it a basin holds. More the deeper you are, because the whole
    // reason to take the next lift down is that the next floor pays better.
    // Three more per floor put twenty-five pieces of salvage on the ground in
    // the last basin, which is a shopping trip rather than a dive.
    inline constexpr int   FindsBase = 7;
    inline constexpr int   FindsPerDepth = 2;
    inline constexpr float FindRadius = 17.0f;
    inline constexpr float FindPickupRange = 44.0f;
    inline constexpr float FindBob = 1.6f;

    // The art is drawn at 44 px so it stays sharp when the lamp is right on
    // top of it; on the floor a piece of salvage is about the size of a crate
    // corner, which is this.
    inline constexpr float FindScale = 0.82f;

    // =====================================================================
    //  THE SURFACE
    //
    //  A rig deck with three things on it, walked between by a man in a suit.
    //  The layout lives here rather than in the class so a station can be
    //  moved without arguing with the drawing code about it.
    // =====================================================================
    inline constexpr float DeckWalkSpeed = 215.0f;
    inline constexpr float DeckAccel = 1700.0f;
    inline constexpr float DeckDrag = 9.0f;
    inline constexpr float DeckTurn = 720.0f;   // degrees a second
    inline constexpr float DeckReach = 122.0f;  // how close counts as standing at one
    inline constexpr float DeckStride = 0.052f; // radians of step per pixel walked

    inline constexpr const char* DeckPlatePath = "assets/textures/deck_plate.png";
    inline constexpr const char* DeckRailPath  = "assets/textures/deck_rail.png";
    inline constexpr const char* DeckDiverPath = "assets/textures/deck_diver.png";
    inline constexpr const char* DeckBootPath  = "assets/textures/deck_boot.png";
    inline constexpr const char* DeckCranePath = "assets/textures/deck_crane.png";
    inline constexpr const char* DeckBenchPath = "assets/textures/deck_bench.png";
    inline constexpr const char* DeckBellPath  = "assets/textures/deck_bell.png";
    inline constexpr const char* DeckCratePath = "assets/textures/deck_crate.png";

    // What is actually lying about on the working deck of a dive support
    // vessel. None of it is a station and none of it does anything: it is
    // there because a rig that has only the three things you can use on it is
    // a menu with a floor drawn under it.
    struct DeckProp
    {
        const char*  path;
        sf::Vector2f at;
        float        turn;   // degrees, so the loose gear is not all square
        float        solid;  // radius you cannot walk through, 0 to walk over
    };

    inline constexpr std::array<DeckProp, 19> DeckProps{ {
        // The sub itself is no longer a flat prop here. It stands in its cradle
        // between the crane and the moon pool as the real machine the player
        // pilots - hull plus every piece of hardware they have bolted on - so
        // the deck shows the current state of the sub rather than a picture of
        // a generic one. Deck::drawSub owns it; see config::DeckSub* below.

        // The goods end, round the crane.
        { "assets/textures/deck_quad.png",    { 200.0f, 336.0f },   0.0f, 40.0f },
        { "assets/textures/deck_quad.png",    { 222.0f, 272.0f },   3.0f, 40.0f },
        { "assets/textures/deck_basket.png",  { 243.0f, 352.0f },  -7.0f, 36.0f },
        { "assets/textures/deck_chain.png",   { 330.0f, 352.0f },  18.0f,  0.0f },
        { "assets/textures/deck_drums.png",   { 330.0f, 474.0f }, -12.0f, 26.0f },

        // The saturation spread along the bottom. This is the half of a dive
        // support vessel that has no equivalent anywhere else in the game, and
        // it is what makes the rig read as saturation diving rather than as a
        // warehouse with a hole in it.
        { "assets/textures/deck_shack.png",   { 238.0f, 579.0f },   0.0f, 74.0f },
        { "assets/textures/deck_chamber.png", { 470.0f, 590.0f },   0.0f, 92.0f },
        { "assets/textures/deck_reel.png",    { 700.0f, 578.0f },   0.0f, 40.0f },
        { "assets/textures/deck_reel.png",    { 880.0f, 586.0f },   2.0f, 40.0f },
        { "assets/textures/deck_hatch.png",   { 1006.0f, 596.0f },  0.0f, 26.0f },
        { "assets/textures/deck_weights.png", { 620.0f, 620.0f },  -5.0f,  0.0f },

        // The work end, round the bench.
        { "assets/textures/deck_chest.png",   { 1070.0f, 336.0f },  4.0f, 24.0f },
        { "assets/textures/deck_welder.png",  { 1052.0f, 300.0f }, -6.0f, 28.0f },
        { "assets/textures/deck_hose.png",    { 1074.0f, 380.0f },  0.0f, 20.0f },

        // The safety line along the port walkway, and the bitts at the corners.
        // The two rafts that used to sit either side of the bow are gone: the
        // gantry stands there now, and a raft lashed under a launch frame is
        // not something anybody would leave there.
        { "assets/textures/deck_hose.png",    { 470.0f, 252.0f },   0.0f, 20.0f },
        { "assets/textures/deck_bitts.png",   { 470.0f, 172.0f },   0.0f,  0.0f },
        { "assets/textures/deck_bitts.png",   { 810.0f, 172.0f },   0.0f,  0.0f },
        { "assets/textures/deck_bitts.png",   { 300.0f, 634.0f },   0.0f,  0.0f },
        { "assets/textures/deck_bitts.png",   { 1076.0f, 634.0f },  0.0f,  0.0f },

        // The middle of the plate is deliberately bare. It was filled with a
        // second helping of the same crates, baskets, chain and drums that
        // dress the two ends, on the theory that a working deck is not tidy -
        // but a deck with gear on every square metre is not a working deck
        // either, it is a warehouse, and it buried the three things the player
        // is actually here to walk between. The walk is the room now.
    } };

    // --- The deck at night ---
    //
    //  The first version of the deck was lit like a car park: one flat grey
    //  plate at full brightness, which is why it read as a menu with a man on
    //  it rather than as a place. It is night now, it is raining, and the only
    //  light is five floodlights, one window and whatever the bell is doing in
    //  the hole. Everything below is either the dark, or something that adds
    //  to it.
    //
    //  The ambient is a multiply on every sprite rather than a veil over the
    //  whole screen: a multiply costs nothing, a veil would grey the lights
    //  out again, and a shader is not available to this project.
    inline constexpr sf::Color DeckNightSea{ 6, 11, 16 };
    inline constexpr sf::Color DeckNightSwell{ 20, 32, 40 };
    inline constexpr sf::Color DeckNightFoam{ 58, 78, 86 };

    inline constexpr sf::Color DeckNightTint{ 70, 78, 92 };
    inline constexpr sf::Color DeckNightDiver{ 96, 102, 112 };
    inline constexpr sf::Color DeckNightBell{ 84, 92, 104 };

    inline constexpr sf::Color DeckFlood{ 242, 226, 186 };     // halide, warmed
    inline constexpr sf::Color DeckFloodCore{ 252, 246, 228 };
    inline constexpr sf::Color DeckShack{ 238, 172, 104 };     // the one warm window
    inline constexpr sf::Color DeckMoon{ 150, 212, 220 };
    inline constexpr sf::Color DeckHelmet{ 196, 224, 216 };
    inline constexpr sf::Color DeckRain{ 150, 176, 190 };
    inline constexpr sf::Color DeckPuddle{ 10, 18, 26 };
    inline constexpr sf::Color DeckDrip{ 104, 134, 146 };
    inline constexpr sf::Color DeckVignette{ 2, 5, 9 };

    // A floodlight on a mast. The pool it throws is an ellipse whose long axis
    // points away from its own mast, which is the only thing that makes five
    // lamps read as five lamps instead of five decals: the shadows under them
    // all splay the same way the light does.
    struct DeckLight
    {
        sf::Vector2f mast;
        sf::Vector2f pool;
        sf::Vector2f radii;    // long axis first, along mast -> pool
        float        turn;     // degrees of that long axis
        float        strength; // 0..1 of DeckFlood
        float        flicker;  // 0 is a lamp nobody has to think about
        bool         failing;  // and this one drops out every few seconds
    };

    inline constexpr std::array<DeckLight, 7> DeckLights{ {
        { { 300.0f, 236.0f }, { 352.0f, 292.0f }, { 246.0f, 176.0f }, 51.2f, 0.62f, 0.04f, false },
        { { 980.0f, 236.0f }, { 930.0f, 280.0f }, { 238.0f, 172.0f }, 132.3f, 0.60f, 0.03f, false },
        { { 218.0f, 604.0f }, { 382.0f, 508.0f }, { 232.0f, 168.0f }, -30.3f, 0.56f, 0.11f, true },
        { { 1064.0f, 606.0f }, { 898.0f, 512.0f }, { 236.0f, 170.0f }, -150.5f, 0.58f, 0.04f, false },
        { { 640.0f, 108.0f }, { 640.0f, 232.0f }, { 250.0f, 186.0f }, 90.0f, 0.48f, 0.05f, false },

        // Two work lamps on the gantry legs, thrown outboard along the frame.
        // Nothing else on the rig reaches past the rail, so without these the
        // largest object on the screen sits in the dark - and a pair of lamps
        // pointed at the thing being launched is what a launch frame has.
        { { 524.0f, 168.0f }, { 566.0f, 86.0f }, { 172.0f, 128.0f }, -63.0f, 0.50f, 0.03f, false },
        { { 756.0f, 168.0f }, { 714.0f, 86.0f }, { 172.0f, 128.0f }, -117.0f, 0.50f, 0.03f, false },
    } };

    // The sub, slung off the bow on the launch gantry - out past the rail and
    // over open water, where a sub waiting to go down actually hangs.
    //
    // It used to sit in a cradle in the middle of the plate, which put the
    // largest object on the deck directly between the crane and the bell and
    // made the player walk round it every single time. Out here it is bigger,
    // it is the first thing the eye lands on, and it is standing in nobody's
    // way: there is no deck under it to walk on.
    //
    // Drawn as the real machine - hull plus every piece of fitted hardware -
    // so it always shows the current upgrade state.
    inline constexpr sf::Vector2f DeckSubAt{ 640.0f, 86.0f };
    inline constexpr float DeckSubTurn = 0.0f;   // lying along the gantry

    // Half its length on the deck.
    //
    // It used to be a 128 pixel sprite composed into its own canvas, because
    // blowing that sprite up by 1.3 put some source pixels on one screen pixel
    // and some on two and turned every edge into a staircase. It is drawn out
    // of shapes now, so hero size is simply a bigger number, and the canvas is
    // gone along with the sprite that needed it.
    inline constexpr float DeckSubLength = 90.0f;
    inline constexpr float DeckSubBob = 3.0f;    // it rides the swell under it
    inline constexpr float DeckSubSway = 1.7f;   // degrees, hanging on its slings

    // The gantry it hangs from, seen from directly above: two rails running
    // out over the water, a cross beam at each end, bracing between them, and
    // the slings dropping to the hull. Legs are what you can walk into; the
    // rest is overhead and drawn above the sub because that is where it is.
    inline constexpr float DeckGantryLeft = 524.0f;
    inline constexpr float DeckGantryRight = 756.0f;
    inline constexpr float DeckGantryOut = 18.0f;   // outboard end, over the sea
    inline constexpr float DeckGantryIn = 196.0f;   // and where it meets the plate
    inline constexpr float DeckGantryBeam = 30.0f;  // the beam the slings hang off
    inline constexpr float DeckGantryRail = 6.0f;   // how thick the steel is
    inline constexpr float DeckGantryFoot = 26.0f;  // walk round the legs

    // Where the bracing lives. Only the inboard run, between the sub and the
    // plate: a lattice drawn across the middle of the frame goes straight over
    // the hull, and the whole point of moving the sub out here was that it
    // stops being the thing everything else is drawn on top of.
    inline constexpr float DeckGantryBraceFrom = 144.0f;

    // One knob over the whole rig. The per-lamp strengths above are the shape
    // of the lighting - which lamp is the strong one, which one is failing -
    // and this is how much of it there is. Five overlapping pools at their
    // paper strength add up to a deck that is uniformly white, which is the
    // same mistake as no lighting at all wearing a different hat.
    inline constexpr float DeckLightGain = 0.86f;
    inline constexpr float DeckSheen = 0.020f;   // the broad wet glimmer
    inline constexpr float DeckCore = 0.52f;     // the hot middle of a pool

    inline constexpr int   DeckRainCount = 260;
    inline constexpr float DeckRainFall = 1180.0f;   // pixels a second
    inline constexpr float DeckRainAngle = 76.0f;    // down and to starboard

    // The cutscene plates. Painted once and then moved, panned and lit by the
    // reel, because a rig is not a thing anybody draws out of triangles.
    inline constexpr const char* ReelRigPath     = "assets/textures/reel_rig.png";
    inline constexpr const char* ReelBellPath    = "assets/textures/reel_bell.png";
    inline constexpr const char* ReelWallPath    = "assets/textures/reel_wall.png";
    inline constexpr const char* ReelStationPath = "assets/textures/reel_station.png";
    inline constexpr const char* ReelBeastPath   = "assets/textures/reel_beast.png";
    inline constexpr const char* ReelDiverPath   = "assets/textures/reel_diver.png";
    inline constexpr const char* ReelCratePath   = "assets/textures/reel_crate.png";
    inline constexpr const char* ReelLedgePath   = "assets/textures/reel_ledge.png";
    inline constexpr const char* ReelWreckPath   = "assets/textures/reel_wreck.png";
    inline constexpr const char* ReelHatchPath   = "assets/textures/reel_hatch.png";

    // Every plate is now painted bigger than the size its shots were framed
    // against, because the station is stretched to nearly three times its own
    // width in the shot that goes close on its hatches and a stretched texture
    // is a blurred texture however well it was drawn. Rather than re-derive
    // thirteen camera moves, each shot still asks for the scale it always did
    // and the reel divides by what the plate grew by.
    inline constexpr float ReelFitRig = 460.0f / 640.0f;
    inline constexpr float ReelFitBell = 180.0f / 240.0f;
    inline constexpr float ReelFitWall = 300.0f / 420.0f;
    inline constexpr float ReelFitStation = 560.0f / 1000.0f;
    inline constexpr float ReelFitBeast = 760.0f / 1120.0f;
    inline constexpr float ReelFitDiver = 150.0f / 210.0f;
    inline constexpr float ReelFitCrate = 46.0f / 96.0f;

    // Going down is free.
    //
    // It was not: the bell, the gas and the men on the winch charged by the
    // fathom, and the price climbed with every floor. On paper that turns a
    // dive into a bet. In the hand it did the opposite - a bad dive left you
    // unable to afford the next one, so the game answered a run of bad luck by
    // taking the game away, and the deeper you got the worse the spiral. The
    // hold is already the thing you can lose. One tax on failure is enough.

    // =====================================================================
    //  WHAT ELSE IS DOWN THERE
    //
    //  The level's objective is the job. These are not the job. They are
    //  three kinds of thing lying about in the water that are worth money or
    //  worth knowing, and every one of them is in a room the objective would
    //  never have sent anybody into.
    //
    //  That is the point of them. A basin with one task in it is a corridor;
    //  a basin with a task and four reasons to go the other way is a place
    //  somebody has to make decisions in.
    // =====================================================================
    enum class OfferKind
    {
        Wreck, // a hull nobody ever got open. Cut it, and be heard doing it.
        Cache, // a rig off a diver who did not come back, and their last log.
        Hive,  // an egg mass that is awake and will stay awake until it is not.
        Count,
    };

    struct OfferDef
    {
        const char* name;
        const char* what;    // the one line under it that says what it wants
        sf::Color   colour;
        int         health;  // zero means you take it by swimming into it
        int         pieces;  // how much salvage it is worth
        int         floor;   // and the worst kind it is allowed to pay in
        float       radius;
    };

    inline constexpr std::array<OfferDef, 3> OfferTable{ {
        { "A SEALED HULL", "CUT IT OPEN",      { 198, 172,  96 }, 260, 5, 1, 44.0f },
        { "A DIVER",       "TAKE WHAT IS LEFT",{ 140, 188, 210 },   0, 2, 0, 26.0f },
        { "AN OPEN BROOD", "BURN IT OUT",      { 214, 128, 148 }, 340, 6, 2, 40.0f },
    } };

    inline constexpr int   OffersBase = 2;
    inline constexpr int   OffersPerDepth = 1;
    inline constexpr int   OffersMost = 5;
    inline constexpr float OfferFromMouth = 620.0f;
    inline constexpr float OfferApart = 460.0f;
    inline constexpr float OfferNoticeRange = 260.0f;  // when its name appears
    inline constexpr float HiveHatch = 3.4f;           // seconds between spawns
    inline constexpr int   WreckCallers = 5;           // what cutting one brings

    // What a dead diver had time to write down. One per rig, and the only
    // place in the game anybody says anything about what happened here - a
    // player reads these because they walked across a basin to get them,
    // which is the only way a wall of lore ever gets read.
    inline constexpr std::array<const char*, 14> DiverLogs{ {
        "HIS LOG: \"STATION SAYS HOLD AT NINE HUNDRED. STATION IS NOT AT NINE HUNDRED.\"",
        "HIS LOG: \"THE SECOND SUB WENT PAST US GOING UP. NOBODY WAS DRIVING IT.\"",
        "HER LOG: \"IT COPIES THE LAMP. TURN THE LAMP OFF AND IT WAITS.\"",
        "HER LOG: \"WE ARE NOT THE FIRST. THERE IS RIGGING DOWN HERE OLDER THAN THE COMPANY.\"",
        "HIS LOG: \"THE ORE IS WARM. NOTHING AT THIS DEPTH IS WARM.\"",
        "HIS LOG: \"IT DOES NOT HUNT. IT COLLECTS.\"",
        "HER LOG: \"COUNTED ELEVEN ON THE SONAR. COUNTED ONE WHEN THE LAMP CAME ON.\"",
        "HER LOG: \"THE STATION IS INTACT. EVERY HATCH IS OPEN FROM THE INSIDE.\"",
        "HIS LOG: \"DAY SIX. THE WATER IS GETTING SHALLOWER AND WE ARE GOING DOWN.\"",
        "HIS LOG: \"IT ANSWERED THE SONAR. USE THE SONAR AND IT ANSWERS.\"",
        "HER LOG: \"WHATEVER IS AT THE BOTTOM IS NOT THE THING THAT IS KILLING US.\"",
        "HER LOG: \"IF YOU ARE READING THIS ON THE WAY DOWN, GO BACK UP.\"",
        "HIS LOG: \"THEY SENT A FOURTH. THEY ALWAYS SEND A FOURTH.\"",
        "HIS LOG: \"IT IS NOT DEEP. IT IS OPEN AT THE BOTTOM.\"",
    } };

    // How long the dive report stays up on the deck, and how much of that is
    // spent fading out. It clears on any key as well, so it is a thing shown
    // rather than a thing sat through.
    // How long the objective bar has to sit completely still before the game
    // points at the job, how long the arrow takes to arrive, how solid it ever
    // gets and how far out in front of the sub it sits. Late enough that a
    // player working through a room is never nagged; the basin is the size of
    // a town and being lost in it is not a skill.
    inline constexpr float LostAfter = 26.0f;
    inline constexpr float LostFade = 1.4f;
    inline constexpr float LostAlpha = 0.5f;
    inline constexpr float LostReach = 118.0f;

    inline constexpr float ReportHold = 7.0f;
    inline constexpr float ReportFade = 1.2f;

    inline constexpr float NoticeDuration = 5.2f;

    // --- The way out, and the way further in ---
    //
    //  The bell you were lowered in stays in the mouth of the basin and will
    //  take you up whenever you have had enough - that is the whole shape of a
    //  dive now: nobody makes you finish a level, they make you decide whether
    //  what is in the hold is worth the next room.
    //
    //  The shaft down is somewhere else in the water and stays shut until the
    //  level's work is done. Riding it keeps the hold, keeps the damage, and
    //  costs nothing but the fact that you are now one floor further from air.
    inline constexpr float LiftRadius = 62.0f;
    inline constexpr float LiftHold = 1.1f;       // seconds of sitting in it
    inline constexpr float LiftForget = 2.6f;     // how fast leaving undoes that
    inline constexpr float ShaftFromMouth = 1050.0f;
    inline constexpr float LiftBeacon = 26.0f;    // the dot that says where it is

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
    // =====================================================================
    //  MARKS
    //  One thing standing in the water, used by two of the goals. A beacon is
    //  lit by holding station beside it; a winch is a thing creatures go for
    //  when the sub is not nearer. They are one struct because they are one
    //  shape - a position, a radius, a number filling up - and two structs
    //  would be the same drawing code twice.
    // =====================================================================
    inline constexpr float MarkRadius = 40.0f;

    // How near counts as standing beside it, and how long standing there
    // takes. Six seconds is long enough that it cannot be done on the way
    // past and short enough that it is not a chore.
    inline constexpr float MarkHold = 6.0f;
    inline constexpr float MarkReach = 96.0f;
    inline constexpr sf::Color MarkColdColor{ 96, 116, 128 };
    inline constexpr sf::Color MarkLitColor{ 128, 232, 204 };

    // The winch. A creature within this much of it, and further from the sub
    // than it is, goes for the winch instead - and every bite knocks this many
    // seconds off the lift.
    inline constexpr float WardPull = 620.0f;
    inline constexpr float WardBiteRange = 62.0f;
    inline constexpr float WardBiteInterval = 0.9f;
    inline constexpr float WardSetback = 3.5f;
    inline constexpr sf::Color WardColor{ 226, 182, 108 };

    // How far in front of itself a fleeing creature is told to go. Far enough
    // that the route it asks for is a room away rather than a step away, which
    // is the difference between running and backing off.
    inline constexpr float QuarryFlee = 900.0f;

    // And how much more of it there is to get through than an ordinary one of
    // its kind. It has to survive being seen.
    inline constexpr float QuarryHealth = 4.0f;

    // How near the mouth counts as out, on an escape.
    inline constexpr float MouthReach = 120.0f;

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

    // How hard a creature that has lost the sub still leans towards it. Not a
    // beeline - running dark has to keep buying something - but enough that
    // nothing is ever left on the far side of the basin forever.
    inline constexpr float WanderLean = 0.85f;

    // How often anything crossing the basin asks the map which way to go.
    // Routing is cheap; routing fifteen times a frame is not.
    inline constexpr float NavInterval = 0.35f;

    // The navigation grid under the room graph. Seventy pixels is fine enough
    // to find a way past a pillar and coarse enough that a whole basin is a
    // few thousand cells rather than a few hundred thousand.
    inline constexpr float NavCell = 70.0f;
    inline constexpr float NavClearance = 26.0f;

    // The shape index that the distance field reads instead of walking every
    // chamber in the basin. The cell is a little larger than the passages, so
    // a query lands in a bucket holding a handful of shapes rather than one
    // holding half a room.
    //
    // The margins are the interesting part: a shape is filed under every cell
    // it could reach plus this much further, which is what lets the bucket be
    // trusted rather than merely believed. A chamber left out of a bucket is
    // more than ShapeMarginRock away from it, so it can only be the nearest
    // rock if the answer is further out than that - and a query that far into
    // rock asks every shape instead. Water needs the wider margin because a
    // point in the middle of a great hall is a long way from anything and a
    // column on the far side still shortens its distance; deeper than that,
    // the columns are simply all read, since there are only ever a couple of
    // hundred and almost none survive the reach test.
    inline constexpr float ShapeCell = 200.0f;
    inline constexpr float ShapeMarginRock = 170.0f;
    inline constexpr float ShapeMarginWater = 420.0f;

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
    inline constexpr int   FirstClearBase = 8;
    inline constexpr int   FirstClearPerLevel = 2;
    inline constexpr float RerunSalvageFactor = 0.30f;
    inline constexpr float RespecRefund = 0.70f;

    // --- Particles ---
    inline constexpr const char* ParticleTexturePath = "assets/textures/particle_dot.png";
    inline constexpr std::size_t MaxParticles = 4000;
    inline constexpr float AmbientSnowInterval = 0.03f;
    inline constexpr float AmbientBubbleInterval = 0.5f;

    // --- Audio ---
    inline constexpr const char* AmbientTrackPath = "assets/audio/ambient_abyss.wav";
    // Forty rather than twenty four, because every sound in an open room now
    // asks for one or two more voices a moment later for its echo, and the
    // pool has no priorities - running out means whatever was speaking gets
    // cut off mid-word.
    inline constexpr std::size_t AudioVoiceCount = 40;

    // --- What the water does to a sound ---
    // How far a sound can be before it starts getting quieter. Half the window
    // height, so anything on screen is at full strength and distance only
    // starts to matter out in the dark.
    inline constexpr float AudioNearDistance = 360.0f;
    inline constexpr float AudioRolloff = 1.15f;

    // Rock in the way. The volume drop is the small half of it; what actually
    // says "that is behind something" is the top coming off, and the corner
    // frequency is swept between these in log space.
    inline constexpr float AudioMuffledVolume = 0.42f;
    inline constexpr float AudioOpenCutoff = 18000.0f;
    inline constexpr float AudioMuffledCutoff = 520.0f;

    // How much rock has to be between you and a sound before it is treated as
    // fully behind something. Three rays, so a doorway gives a partial answer
    // rather than a yes or a no.
    inline constexpr float AudioOcclusionSpread = 62.0f;

    // The room coming back at you. A passage returns nothing, a cavern one
    // repeat, a great hall two - the second quieter, later and darker than the
    // first, which is the whole of what makes a space sound large.
    inline constexpr float EchoNearDelay = 0.115f;
    inline constexpr float EchoFarDelay = 0.235f;
    inline constexpr float EchoNearVolume = 0.30f;
    inline constexpr float EchoFarVolume = 0.16f;
    inline constexpr float EchoNearCutoff = 2600.0f;
    inline constexpr float EchoFarCutoff = 1250.0f;

    // Below this a room is a passage and gives nothing back; above it the
    // second repeat comes in. Measured against the room radii the generator
    // actually produces.
    inline constexpr float EchoRoomFloor = 150.0f;
    inline constexpr float EchoHallRadius = 520.0f;
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
    inline constexpr float VolumeDeflect = 58.0f;
    inline constexpr float VolumeSiren = 44.0f;
    inline constexpr float VolumeHullGroan = 46.0f;

    // Music. Three loops over the same root, faded independently.
    inline constexpr const char* MusicBedPath = "assets/audio/music_bed.wav";
    inline constexpr const char* MusicPulsePath = "assets/audio/music_pulse.wav";
    inline constexpr const char* MusicDreadPath = "assets/audio/music_dread.wav";
    inline constexpr const char* MusicStalkPath = "assets/audio/music_stalk.wav";
    inline constexpr float VolumeMusic = 30.0f;

    // The surface, and what the rework added under it.
    inline constexpr float VolumeDeckStep = 13.0f;
    inline constexpr float VolumeDeckRig = 26.0f;
    inline constexpr float VolumeFind = 34.0f;
    inline constexpr float VolumeCut = 30.0f;

    // The four objective voices. Louder than salvage and quieter than a boss:
    // these are the things the level is actually about.
    inline constexpr float VolumeObjective = 52.0f;
    inline constexpr float VolumeLift = 62.0f;
    inline constexpr float VolumeLiftHold = 26.0f;
    inline constexpr float VolumeReel = 56.0f;

    inline constexpr const char* DeckTrackPath = "assets/audio/ambient_deck.wav";
    inline constexpr const char* MusicReelPath = "assets/audio/music_reel.wav";
    inline constexpr const char* MusicSurfacePath = "assets/audio/music_surface.wav";

    // How much of the depth's pitch drop the score takes. The drone takes all
    // of it; the music takes a share, so the deeper water is heavier and
    // slower without the whole score sliding audibly out of tune.
    inline constexpr float MusicDepthPitch = 0.45f;

    // How long the two ambiences take to trade places. Long enough that
    // surfacing is a transition rather than a cut, short enough that the deck
    // is not still carrying the abyss while you are standing in the rain.
    inline constexpr float AmbienceFade = 1.6f;
    inline constexpr float MusicFadeRate = 0.9f; // per second, so a swell takes about one

    // How many creatures actively hunting you counts as full pressure. Low on
    // purpose: three things closing in is already a fight.
    inline constexpr float MusicPulseFull = 4.0f;

    // One creature hunting you from somewhere you cannot look is already the
    // whole of what this layer is for, so two fills it.
    inline constexpr float MusicStalkFull = 2.0f;

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

    inline constexpr ProjectileStats Torpedo{ 300.0f, 13.0f, 0.9f, 0.05f, 2.2f, 3.5f, 1.3f };
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

    // And the shortest it may ever get. The engine line takes ninety percent
    // off between three nodes, which on a two second cooldown is a fifth of a
    // second - and with GHOST fitted that is a sub that is untouchable half
    // the time it is moving.
    inline constexpr float DashMinCooldown = 0.75f;
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
    // What a hit does to where the sub is, as opposed to what it does to the
    // bar. Peak well under the boss sweep, so a boss move stays the thing
    // that throws you; time well under the invulnerability window, so a shove
    // can never push you into a second hit you had no answer to.
    inline constexpr float KnockbackPeak = 430.0f;
    inline constexpr float KnockbackTime = 0.22f;
    inline constexpr float KnockbackDecay = 7.0f;

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
    inline constexpr float StreakSalvageStep = 0.04f;
    inline constexpr int   StreakSalvageCap = 12;  // +48% and no further

    // How little of a hit plating may ever leave. Six plates used to reduce
    // every bite in the game to one point; a third of the hit always lands.
    inline constexpr float ArmourFloor = 0.34f;

    inline constexpr float SlowMotionScale = 0.34f;
    inline constexpr float SlowMotionBossAct = 1.1f;
    inline constexpr float SlowMotionBossDeath = 2.2f;
    inline constexpr float FlashFade = 2.8f;
    inline constexpr float BigMomentRing = 420.0f;
    inline constexpr float SirenInterval = 1.6f;

    // The hull working against the pressure, on a loop through a whole dive. It
    // is not damage - the sub is fine - it is the deep leaning on it, so it
    // comes closer together and drops in pitch the further down you are, and
    // the frame draws in a hair on each groan. The pull is tiny on purpose: a
    // felt thing, not an effect.
    inline constexpr float GroanIntervalShallow = 15.0f; // seconds between, up top
    inline constexpr float GroanIntervalDeep = 5.5f;     // and at the bottom
    inline constexpr float GroanPitchShallow = 1.0f;     // as recorded
    inline constexpr float GroanPitchDeep = 0.6f;        // lower, heavier
    inline constexpr float GroanPinch = 0.028f;          // fraction the view draws in
    inline constexpr float GroanPinchTime = 0.9f;        // how long the draw-in lasts

    // The viewport glass cracks as the sub takes hits, and the pane fills over
    // a dive. Capped so a long fight does not white the screen out.
    inline constexpr int GlassCrackMax = 14;

    // How long SCUTTLE has to be held. Erasing a run is the only thing in the
    // game that cannot be taken back, and it sits one row under SURFACE.
    inline constexpr float ScuttleHold = 1.4f;

    // Something enormous, a long way out, that has nothing to do with you. It
    // is never drawn and it never arrives: it is heard, and a good enough sonar
    // gets a return off it. What is down there is the thing waiting at the
    // bottom of the campaign, moving about its own business while you work.
    //
    // Placed past the lamp's own reach so there is never a contact sitting in
    // water the player can plainly see is empty.
    inline constexpr int   GiantFromDepth = 3;
    inline constexpr float GiantIntervalMin = 55.0f;
    inline constexpr float GiantIntervalMax = 130.0f;
    inline constexpr float GiantHold = 7.0f;     // how long the contact stays live
    inline constexpr float GiantNear = 1.3f;     // multiples of the lamp's reach
    inline constexpr float GiantFar = 2.0f;
    inline constexpr float GiantDrift = 55.0f;   // pixels a second, crossing
    inline constexpr float GiantPitch = 0.45f;   // far below anything in the basin
    inline constexpr float GiantVolume = 1.2f;
    inline constexpr float GiantMuffle = 0.55f;  // a lot of water in the way
    inline constexpr float GiantMark = 170.0f;   // how big it reads back
    inline constexpr sf::Color GiantPingColor{ 176, 146, 232 };

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
    // How far around the sub the map fills itself in. Roughly what the lamp
    // and the hull can account for: the map is a record of where you have
    // been rather than a survey handed to you at the mouth.
    inline constexpr float MapRevealRadius = 420.0f;

    // Where the card naming what killed you sits, under the death banner.
    inline constexpr float DeathCardTop = 430.0f;

    // How far the brightness slider can go either way, as an alpha over the
    // whole frame. Lifting washes the blacks out faster than dropping crushes
    // them, so the two are not the same number.
    inline constexpr float BrightnessLift = 46.0f;
    inline constexpr float BrightnessDrop = 96.0f;

    // The column down the left that says what is in the water with you. Under
    // the hull pips and the objective bar, which is the one part of the frame
    // nothing else was using.
    // Where a boss move is about to land. Additive on the floor, filling up as
    // the clock runs out, so how full it is says how long is left.
    // A lamp the sub leaves behind. It is the one light in the game that is not
    // attached to you: you put it somewhere and it is still there an hour
    // later, which is what turns a basin from a corridor you are lost in into
    // somewhere you have been. It reveals what walks into it - the dark is
    // painted over everything, so whatever is inside a beacon is drawn again
    // afterwards - and it never runs out, because a lamp you have to ration is
    // a lamp nobody puts down.
    inline constexpr float BeaconReach = 210.0f;
    inline constexpr float BeaconCore = 26.0f;
    inline constexpr sf::Color BeaconColor{ 236, 196, 120 };
    inline constexpr int   BeaconBase = 2;      // with the trait and nothing else
    inline constexpr float BeaconDropGap = 0.35f; // so one press is one lamp

    inline constexpr sf::Color DangerColor{ 168, 52, 44 };
    inline constexpr float SoundLead = 0.55f; // seconds of floor warning

    inline constexpr float RosterTop = 168.0f;
    inline constexpr float RosterStep = 38.0f;
    inline constexpr float RosterSize = 13.0f;

    inline constexpr float MinimapWidth = 214.0f;
    inline constexpr float MinimapHeight = 142.0f;
    inline constexpr float MinimapMargin = 16.0f;
    inline constexpr sf::Color MinimapBackdrop{ 3, 9, 14, 205 };
    inline constexpr sf::Color MinimapWater{ 38, 72, 86, 210 };

    // --- What is out there ---
    // The frame darkens on the side something is closing from, and a call
    // comes at a rate that tightens as it gets nearer. Not a list of markers:
    // the point is that you know it is coming, not what or exactly where.
    inline constexpr float ThreatCueRange = 1500.0f;
    inline constexpr float ThreatCueFloor = 0.06f;   // below this, nothing at all
    inline constexpr float ThreatCueFade = 1.6f;     // how fast the wash swells
    inline constexpr float ThreatCueSlow = 2.60f;    // seconds between calls, far
    inline constexpr float ThreatCueFast = 0.55f;    // and near
    inline constexpr float ThreatCueVolume = 0.55f;
    inline constexpr float ThreatVignetteDepth = 190.0f;
    inline constexpr sf::Color ThreatCueColor{ 116, 26, 34, 150 };

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
