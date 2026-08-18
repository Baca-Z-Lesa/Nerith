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
            BossBody::Serpent, BossMind::Prowler,
            "assets/textures/lev_head.png", "assets/textures/lev_body.png",
            1.0f, 0.92f, 0.88f, 4, 46.0f, 46.0f,
            2, 40,
            78.0f, 240.0f,
            2.2f, 3, 26.0f,
            6,
            { {
                { PartRole::Eye,    "assets/textures/lev_eye.png",   0,  22.0f, -26.0f, 15.0f, 0.55f,   0.0f, 18 },
                { PartRole::Eye,    "assets/textures/lev_eye.png",   0,  22.0f,  26.0f, 15.0f, 0.55f,   0.0f, 18 },
                { PartRole::Fin,    "assets/textures/lev_fin.png",   1,  -4.0f, -34.0f, 22.0f, 0.8f,  -58.0f, 26 },
                { PartRole::Fin,    "assets/textures/lev_fin.png",   1,  -4.0f,  34.0f, 22.0f, 0.8f,   58.0f, 26 },
                { PartRole::Armour, "assets/textures/lev_plate.png", 2,   0.0f,   0.0f, 34.0f, 0.95f,   0.0f, 46 },
                { PartRole::Organ,  "assets/textures/lev_organ.png", 2,   0.0f,   0.0f, 24.0f, 0.9f,    0.0f, 90 },
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
            BossBody::Bell, BossMind::Drifter,
            "assets/textures/med_bell.png", "assets/textures/med_bell.png",
            1.0f, 1.0f, 1.0f, 0, 0.0f, 84.0f,
            2, 46,
            34.0f, 0.0f,
            3.0f, 9, 360.0f,
            7,
            { {
                { PartRole::Tentacle, "assets/textures/med_tentacle.png", 0,  62.0f,   0.0f, 20.0f, 0.85f,   0.0f, 26 },
                { PartRole::Tentacle, "assets/textures/med_tentacle.png", 0,  31.0f,  54.0f, 20.0f, 0.85f,  60.0f, 26 },
                { PartRole::Tentacle, "assets/textures/med_tentacle.png", 0, -31.0f,  54.0f, 20.0f, 0.85f, 120.0f, 26 },
                { PartRole::Tentacle, "assets/textures/med_tentacle.png", 0, -62.0f,   0.0f, 20.0f, 0.85f, 180.0f, 26 },
                { PartRole::Tentacle, "assets/textures/med_tentacle.png", 0, -31.0f, -54.0f, 20.0f, 0.85f, 240.0f, 26 },
                { PartRole::Tentacle, "assets/textures/med_tentacle.png", 0,  31.0f, -54.0f, 20.0f, 0.85f, 300.0f, 26 },
                { PartRole::Organ,    "assets/textures/med_nucleus.png",  0,   0.0f,   0.0f, 28.0f, 0.9f,    0.0f, 120 },
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
            BossBody::Serpent, BossMind::Burrower,
            "assets/textures/worm_head.png", "assets/textures/worm_seg.png",
            0.95f, 0.9f, 0.94f, 6, 44.0f, 44.0f,
            2, 52,
            132.0f, 0.0f,
            1.5f, 5, 54.0f,
            5,
            { {
                { PartRole::Fin,   "assets/textures/lev_fin.png",    1,  0.0f, -32.0f, 20.0f, 0.7f, -62.0f, 30 },
                { PartRole::Fin,   "assets/textures/lev_fin.png",    1,  0.0f,  32.0f, 20.0f, 0.7f,  62.0f, 30 },
                { PartRole::Organ, "assets/textures/worm_vent.png",  2,  0.0f,   0.0f, 24.0f, 0.9f,   0.0f, 46 },
                { PartRole::Organ, "assets/textures/worm_vent.png",  4,  0.0f,   0.0f, 24.0f, 0.9f,   0.0f, 46 },
                { PartRole::Organ, "assets/textures/worm_vent.png",  6,  0.0f,   0.0f, 24.0f, 0.9f,   0.0f, 46 },
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
