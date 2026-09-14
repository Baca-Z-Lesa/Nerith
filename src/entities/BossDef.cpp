#include "entities/BossDef.h"

#include <algorithm>
#include <cmath>

namespace
{

    const BossDef Definitions[static_cast<std::size_t>(BossId::Count)] = {
        // =================================================================
        //  THE LEVIATHAN
        //  A long predator that stalks you, sizes you up, then commits to a
        //  charge. Blind it and the charges start missing; clip its fins and
        //  it can no longer turn with you; crack the dorsal plate and the
        //  heart underneath is finally reachable.
        // =================================================================
        {
            "THE LEVIATHAN", "IT HAS BEEN CIRCLING SINCE YOU ARRIVED.",
            BossBody::Serpent, BossMind::Prowler, BossSkin::Leviathan,
            // Not a texture in it. Every part below is a hitbox and a role;
            // what the player sees is drawn from the pose in BossArt, which is
            // why this one can look at you.
            nullptr, "assets/textures/shot_leviathan.png",
            nullptr, nullptr,
            1.0f, 0.92f, 0.90f, 6, 56.0f, 46.0f,
            2, 40,
            78.0f, 240.0f,
            2.2f, 3, 26.0f,
            { 0.55f, 1.35f, 3, 1.4f },
            6,
            { {
                { PartRole::Eye,    nullptr, 0, -30.0f, -22.0f, 19.0f, 0.62f,   0.0f, 18 },
                { PartRole::Eye,    nullptr, 0, -30.0f,  22.0f, 19.0f, 0.62f,   0.0f, 18 },
                { PartRole::Fin,    nullptr, 2,   2.0f, -22.0f, 24.0f, 0.8f,  -82.0f, 26 },
                { PartRole::Fin,    nullptr, 2,   2.0f,  22.0f, 24.0f, 0.8f,   82.0f, 26 },
                { PartRole::Armour, nullptr, 1,   0.0f,   0.0f, 36.0f, 1.0f ,   0.0f, 46 },
                { PartRole::Organ,  nullptr, 1,   0.0f,   0.0f, 26.0f, 1.05f,   0.0f, 90 },
            } }
        },

        // =================================================================
        //  THE BLOOM SOVEREIGN
        //  Never chases. It breathes: the bell clenches, the nucleus shows
        //  for a moment, then it throws a wall of stingers outward. Every
        //  tentacle you cut is one less arm feeding that wall.
        // =================================================================
        {
            "THE BLOOM SOVEREIGN", "IT DOES NOT HUNT. IT SIMPLY FILLS THE WATER.",
            BossBody::Bell, BossMind::Drifter, BossSkin::Medusa,
            // Drawn, not stamped: a translucent bell with the plumbing showing
            // through it. Every part below is a hitbox and a role.
            nullptr, "assets/textures/shot_sovereign.png",
            nullptr, nullptr,
            1.0f, 1.0f, 1.0f, 0, 0.0f, 84.0f,
            2, 46,
            34.0f, 0.0f,
            3.0f, 9, 360.0f,
            { 0.60f, 1.15f, 5, 1.0f },
            7,
            { {
                { PartRole::Tentacle, nullptr,                              0,  62.0f,   0.0f, 26.0f, 0.85f,   0.0f, 26 },
                { PartRole::Tentacle, nullptr,                              0,  31.0f,  54.0f, 26.0f, 0.85f,  60.0f, 26 },
                { PartRole::Tentacle, nullptr,                              0, -31.0f,  54.0f, 26.0f, 0.85f, 120.0f, 26 },
                { PartRole::Tentacle, nullptr,                              0, -62.0f,   0.0f, 26.0f, 0.85f, 180.0f, 26 },
                { PartRole::Tentacle, nullptr,                              0, -31.0f, -54.0f, 26.0f, 0.85f, 240.0f, 26 },
                { PartRole::Tentacle, nullptr,                              0,  31.0f, -54.0f, 26.0f, 0.85f, 300.0f, 26 },
                { PartRole::Organ,    nullptr,                            0,   0.0f,   0.0f, 32.0f, 0.95f,    0.0f, 120 },
            } }
        },

        // =================================================================
        //  THE TRENCH WORM
        //  You cannot touch it while it is under the silt. It surfaces,
        //  sweeps through, and the heat vents along its back are the only
        //  soft thing on it. Three of them. Burn them all out.
        // =================================================================
        {
            "THE TRENCH WORM", "THE FLOOR MOVES. THAT IS NOT SILT.",
            BossBody::Serpent, BossMind::Burrower, BossSkin::Worm,
            nullptr, "assets/textures/shot_worm.png",
            nullptr, nullptr,
            0.95f, 0.9f, 0.94f, 6, 44.0f, 44.0f,
            2, 52,
            132.0f, 0.0f,
            1.5f, 5, 54.0f,
            { 0.58f, 1.30f, 4, 1.5f },
            5,
            { {
                { PartRole::Fin,   nullptr,                          1,  0.0f, -32.0f, 20.0f, 0.7f, -62.0f, 30 },
                { PartRole::Fin,   nullptr,                          1,  0.0f,  32.0f, 20.0f, 0.7f,  62.0f, 30 },
                { PartRole::Organ, nullptr,                          2,  0.0f,   0.0f, 34.0f, 1.15f,   0.0f, 46 },
                { PartRole::Organ, nullptr,                          4,  0.0f,   0.0f, 34.0f, 1.15f,   0.0f, 46 },
                // Not on the last link: that one is the tail now, and a heat
                // vent sitting on a point is not anywhere on an animal.
                { PartRole::Organ, nullptr,                          5,  0.0f,   0.0f, 34.0f, 1.15f,   0.0f, 46 },
            } }
        },

        // =================================================================
        //  THE HOLLOW
        //  A crown of bone with nothing inside it. It never closes: it holds
        //  its distance and winds a turning spiral out of itself, which is
        //  survivable only by reading which way the arms are sweeping. The
        //  four lamps on the crown are what it aims with, and every one that
        //  goes out costs it an arm of the spiral. Behind the ribs there is
        //  something pale that is the only living part of it left.
        // =================================================================
        {
            "THE HOLLOW", "IT IS A RING OF BONE. NOTHING LIVES IN THE MIDDLE OF IT.",
            BossBody::Serpent, BossMind::Spiral, BossSkin::Hollow,
            nullptr, "assets/textures/shot_hollow.png",
            nullptr, nullptr,
            1.0f, 0.82f, 0.90f, 5, 66.0f, 58.0f,
            3, 96,
            96.0f, 330.0f,
            0.30f, 4, 90.0f,
            { 0.70f, 1.20f, 2, 1.0f },
            10,
            { {
                // Four lamps on the crown, each on the end of an arm it holds up
                // into the current the way a feather star does. Out on the ends
                // rather than tucked against the skull: an arm you cannot see is
                // not a thing the player can decide to shoot off.
                { PartRole::Eye,    nullptr,                            0,  62.0f, -66.0f, 20.0f, 0.70f,   0.0f, 30 },
                { PartRole::Eye,    nullptr,                            0,  62.0f,  66.0f, 20.0f, 0.70f,   0.0f, 30 },
                { PartRole::Eye,    nullptr,                            0, -34.0f, -80.0f, 20.0f, 0.70f,   0.0f, 30 },
                { PartRole::Eye,    nullptr,                            0, -34.0f,  80.0f, 20.0f, 0.70f,   0.0f, 30 },
                // Three joints down the column. Breaking them is what finally
                // slows the thing down, and they are furthest from the crown,
                // so going for them means turning your back on the spiral.
                { PartRole::Fin,    nullptr,                         2,   0.0f,   0.0f, 26.0f, 0.72f,   0.0f, 40 },
                { PartRole::Fin,    nullptr,                         3,   0.0f,   0.0f, 26.0f, 0.72f,   0.0f, 40 },
                { PartRole::Fin,    nullptr,                         4,   0.0f,   0.0f, 26.0f, 0.72f,   0.0f, 40 },
                // Two shells over the mass, one behind the other, so the last
                // act cannot be skipped by a lucky torpedo early on.
                { PartRole::Armour, nullptr,                            1,   0.0f,   0.0f, 40.0f, 1.0f,    0.0f, 70 },
                { PartRole::Armour, nullptr,                            1,  14.0f,   0.0f, 34.0f, 0.82f,   0.0f, 70 },
                { PartRole::Organ,  nullptr,                            1,   0.0f,   0.0f, 30.0f, 1.0f,    0.0f, 180 },
            } }
        },

        // =================================================================
        //  THE FURNACE
        //  It does not hunt and it does not leave. It has settled on an open
        //  vent and it is turning the whole room around itself, and the only
        //  soft things on it are the three throats the heat comes out of.
        //  Every flue you shear off is one less arm of the spiral, which is
        //  the only way the room ever gets crossable.
        // =================================================================
        {
            "THE FURNACE", "IT IS SITTING ON THE HOLE IN THE FLOOR.",
            BossBody::Bell, BossMind::Spiral, BossSkin::Furnace,
            nullptr, "assets/textures/shot_furnace.png",
            nullptr, nullptr,
            1.0f, 1.0f, 1.0f, 0, 0.0f, 92.0f,
            3, 58,
            30.0f, 300.0f,
            0.34f, 5, 90.0f,
            { 0.62f, 1.10f, 3, 1.0f },
            8,
            { {
                { PartRole::Tentacle, nullptr,                          0,  74.0f,   0.0f, 24.0f, 0.85f,   0.0f, 34 },
                { PartRole::Tentacle, nullptr,                          0,  23.0f,  70.0f, 24.0f, 0.85f,  72.0f, 34 },
                { PartRole::Tentacle, nullptr,                          0, -60.0f,  44.0f, 24.0f, 0.85f, 144.0f, 34 },
                { PartRole::Tentacle, nullptr,                          0, -60.0f, -44.0f, 24.0f, 0.85f, 216.0f, 34 },
                { PartRole::Tentacle, nullptr,                          0,  23.0f, -70.0f, 24.0f, 0.85f, 288.0f, 34 },
                // Three throats, spread so no single pass over the top can
                // reach two of them.
                { PartRole::Organ,    nullptr,                          0,  38.0f,   0.0f, 26.0f, 1.0f,    0.0f, 96 },
                { PartRole::Organ,    nullptr,                          0, -19.0f,  33.0f, 26.0f, 1.0f,    0.0f, 96 },
                { PartRole::Organ,    nullptr,                          0, -19.0f, -33.0f, 26.0f, 1.0f,    0.0f, 96 },
            } }
        },

        // =================================================================
        //  THE MOTHER
        //  A bell that charges. Everything shaped like this until now has
        //  drifted and pulsed, so the first time it puts its head down and
        //  comes at you across the room it reads as the wrong animal doing
        //  it - which is the point. It also refills the water while it does,
        //  so a fight spent dodging is a fight spent losing ground.
        // =================================================================
        {
            "THE MOTHER", "IT IS WHERE THEY ALL CAME FROM.",
            BossBody::Bell, BossMind::Prowler, BossSkin::Mother,
            nullptr, "assets/textures/shot_mother.png",
            nullptr, nullptr,
            1.0f, 1.0f, 1.0f, 0, 0.0f, 96.0f,
            3, 72,
            104.0f, 330.0f,
            2.0f, 7, 200.0f,
            { 0.55f, 1.30f, 4, 1.2f },
            7,
            { {
                { PartRole::Tentacle, nullptr,                          0,  46.0f,  62.0f, 26.0f, 0.9f,   52.0f, 42 },
                { PartRole::Tentacle, nullptr,                          0, -46.0f,  62.0f, 26.0f, 0.9f,  128.0f, 42 },
                { PartRole::Tentacle, nullptr,                          0, -46.0f, -62.0f, 26.0f, 0.9f,  232.0f, 42 },
                { PartRole::Tentacle, nullptr,                          0,  46.0f, -62.0f, 26.0f, 0.9f,  308.0f, 42 },
                // Three sacs, and every one of them is still hatching.
                { PartRole::Organ,    nullptr,                          0,  44.0f,   0.0f, 28.0f, 1.0f,    0.0f, 120 },
                { PartRole::Organ,    nullptr,                          0, -22.0f,  38.0f, 28.0f, 1.0f,    0.0f, 120 },
                { PartRole::Organ,    nullptr,                          0, -22.0f, -38.0f, 28.0f, 1.0f,    0.0f, 120 },
            } }
        },
    };
}

const BossDef& bossDefFor(BossId id)
{
    const auto index = static_cast<std::size_t>(id);
    return Definitions[std::min(index, static_cast<std::size_t>(BossId::Count) - 1)];
}

BossId bossForDepth(int depth)
{
    const int count = static_cast<int>(BossId::Count);
    const int index = (std::max(1, depth) - 1) % count;
    return static_cast<BossId>(index);
}
