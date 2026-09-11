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
    inline constexpr float WorldExtent = 1500.0f;   // half-size the seabed covers
    inline constexpr float CameraClamp = 300.0f;    // how far the camera may leave the middle
    inline constexpr float CameraLerp = 5.0f;       // how quickly it catches up
    inline constexpr float ArenaLightRadius = 340.0f;
    inline constexpr sf::Color ArenaLightColor{ 32, 78, 90 };
    inline constexpr std::size_t ArenaCausticBands = 7;
    inline constexpr float ArenaCausticAngle = 24.0f;

    inline constexpr float GloomInnerX = 470.0f;
    inline constexpr float GloomInnerY = 264.0f;
    inline constexpr float GloomOuterX = 700.0f;
    inline constexpr float GloomOuterY = 394.0f;
    inline constexpr std::uint8_t GloomMaxAlpha = 242;

    // --- Seabed ---
    inline constexpr sf::Color SandNearColor{ 15, 30, 38 };
    inline constexpr sf::Color SandFarColor{ 9, 19, 26 };
    inline constexpr std::size_t SandGrainCount = 2600;
    inline constexpr std::size_t ScatterPropCount = 520;
    inline constexpr std::size_t ReefPropCount = 180;
    inline constexpr float ReefOffset = 22.0f;
    inline constexpr float PropFieldRadius = 1400.0f;

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
    } };

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
    inline constexpr float BossStrikeTime = 1.15f;
    inline constexpr float BossRecoverTime = 1.3f;
    inline constexpr float BossFleeTime = 2.4f;
    inline constexpr float BossEnrageThreshold = 0.35f;

    // Degrees per second the Hollow's spiral arms sweep round. Slow enough to
    // be read, fast enough that standing still is never the answer.
    inline constexpr float BossSpiralTurn = 62.0f;

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
    inline constexpr int   NestHealth = 34;
    inline constexpr float NestRadius = 44.0f;
    inline constexpr float NestHatchInterval = 4.6f;
    inline constexpr int   NestSalvage = 14;
    inline constexpr float NestRingMin = 0.52f; // as a fraction of the arena
    inline constexpr float NestRingMax = 0.88f;

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
    inline constexpr float HurtFlashFade = 2.6f;
    inline constexpr sf::Color HurtFlashColor{ 168, 26, 34 };
    inline constexpr sf::Color PauseVeil{ 2, 7, 12, 208 };

    inline constexpr float CreditsScrollSpeed = 52.0f;
    inline constexpr float CreditsScrollEnd = 740.0f;

    // Threat arrows: how far outside the frame still counts as a threat worth
    // pointing at, and how many can be on screen before it becomes noise. The
    // top inset is deeper because the HUD's own row lives up there.
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
