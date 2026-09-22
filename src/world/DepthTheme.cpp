#include "world/DepthTheme.h"

#include <algorithm>
#include <cstddef>

namespace
{
    // Six steps down. Teal water gives way to cold blue, then indigo, then a
    // bruised violet, then the colour of dried blood where the floor is open
    // and burning, and at the bottom no colour at all. The lamp shrinks the
    // whole way and the vignette tightens around it.
    const DepthTheme Themes[] = {
        { "THE SHALLOWS",
          { 2, 5, 9 }, { 15, 30, 38 }, { 9, 19, 26 },
          { 32, 78, 90 }, 340.0f,
          { 80, 175, 185 }, 1.00f,
          242, 1.00f,
          { 255, 255, 255 },
          1.00f, 34.0f,
          // Nothing but weed and rock. The shallows are the one floor where
          // what is down there is only what grew there.
          { { nullptr, nullptr, nullptr } }, 0.00f,
          // Plankton, drifting down through water that still has light in it.
          0.55f,
          { .spread = 3.15f, .speedMin = 6.0f, .speedMax = 16.0f,
            .lifeMin = 3.0f, .lifeMax = 5.5f, .sizeMin = 1.5f, .sizeMax = 3.0f,
            .endSizeScale = 0.6f, .drag = 0.8f, .buoyancy = -4.0f,
            .color = { 120, 190, 170 }, .count = 5 } },

        { "THE SHELF",
          { 2, 4, 8 }, { 12, 25, 33 }, { 7, 15, 22 },
          { 26, 66, 84 }, 312.0f,
          { 60, 150, 172 }, 0.82f,
          246, 0.90f,
          { 224, 230, 240 },
          0.94f, 36.0f,
          // The first floor anybody worked. Pipe, split crates, cut cable.
          { { "assets/textures/deep_pipes.png", nullptr, nullptr } }, 0.22f,
          // Gas still leaking out of somebody else's pipework, in strings.
          1.6f,
          { .direction = { 0.0f, -1.0f }, .spread = 0.35f,
            .speedMin = 20.0f, .speedMax = 46.0f,
            .lifeMin = 1.8f, .lifeMax = 3.2f, .sizeMin = 1.5f, .sizeMax = 3.5f,
            .endSizeScale = 1.2f, .drag = 1.2f, .buoyancy = 30.0f,
            .color = { 170, 210, 225 }, .count = 7 } },

        { "THE TRENCH",
          { 3, 3, 8 }, { 14, 16, 32 }, { 8, 9, 19 },
          { 32, 46, 92 }, 284.0f,
          { 72, 92, 186 }, 0.62f,
          249, 0.79f,
          { 202, 198, 228 },
          0.88f, 38.0f,
          // Still their gear, and the first thing that fell and stayed.
          { { "assets/textures/deep_pipes.png", "assets/textures/deep_bones.png",
              nullptr } }, 0.28f,
          // Marine snow, falling harder than it does up top: everything that
          // died in the water above this one is on its way past.
          0.50f,
          { .spread = 3.15f, .speedMin = 8.0f, .speedMax = 20.0f,
            .lifeMin = 3.0f, .lifeMax = 6.0f, .sizeMin = 1.5f, .sizeMax = 3.5f,
            .endSizeScale = 0.7f, .drag = 1.0f, .buoyancy = -10.0f,
            .color = { 150, 156, 190 }, .count = 6 } },

        { "THE ABYSS",
          { 3, 2, 4 }, { 17, 12, 17 }, { 9, 7, 10 },
          { 48, 30, 56 }, 256.0f,
          { 116, 62, 126 }, 0.46f,
          251, 0.69f,
          { 190, 168, 182 },
          0.82f, 40.0f,
          // And the first course of block nobody laid.
          { { "assets/textures/deep_bones.png", "assets/textures/deep_wall.png",
              nullptr } }, 0.32f,
          // Something out there lights itself, briefly, for reasons of its own.
          // Few, bright and gone - the only light down here nobody switched on.
          0.85f,
          { .spread = 3.15f, .speedMin = 4.0f, .speedMax = 14.0f,
            .lifeMin = 1.6f, .lifeMax = 3.0f, .sizeMin = 3.0f, .sizeMax = 6.0f,
            .endSizeScale = 0.35f, .drag = 1.5f, .buoyancy = 2.0f,
            .color = { 176, 112, 238 }, .count = 5 } },

        // The floor is open here and what comes out of it is the only light
        // the depth has of its own.
        { "THE VENTS",
          { 2, 1, 2 }, { 15, 8, 8 }, { 8, 4, 4 },
          { 60, 26, 26 }, 228.0f,
          { 146, 52, 42 }, 0.32f,
          253, 0.59f,
          { 186, 150, 146 },
          0.74f, 42.0f,
          // The floor is open here, so this is the one depth with anything
          // living on it in numbers.
          { { "assets/textures/deep_worms.png", "assets/textures/deep_bones.png",
              "assets/textures/deep_wall.png" } }, 0.38f,
          // Soot off a black smoker, going up hard and spreading as it climbs.
          0.70f,
          { .direction = { 0.0f, -1.0f }, .spread = 0.50f,
            .speedMin = 30.0f, .speedMax = 70.0f,
            .lifeMin = 2.2f, .lifeMax = 4.0f, .sizeMin = 3.0f, .sizeMax = 7.0f,
            .endSizeScale = 1.8f, .drag = 1.6f, .buoyancy = 42.0f,
            .color = { 150, 60, 44 }, .count = 9 } },

        // And the bottom of it. Nothing here is any colour; the only thing the
        // lamp finds is bone, and the dark is tight enough to touch.
        { "THE CRADLE",
          { 1, 2, 2 }, { 11, 14, 12 }, { 5, 7, 6 },
          { 44, 62, 52 }, 206.0f,
          { 94, 132, 108 }, 0.22f,
          254, 0.50f,
          { 168, 178, 168 },
          0.66f, 44.0f,
          // Mostly architecture by now, and the bones of whatever used it.
          { { "assets/textures/deep_wall.png", "assets/textures/deep_wall.png",
              "assets/textures/deep_bones.png" } }, 0.45f,
          // Almost nothing, very slowly. The bottom is the one depth whose
          // water is nearly still, and the emptiness is the phenomenon.
          2.2f,
          { .spread = 3.15f, .speedMin = 3.0f, .speedMax = 9.0f,
            .lifeMin = 5.0f, .lifeMax = 8.0f, .sizeMin = 2.0f, .sizeMax = 5.0f,
            .endSizeScale = 0.5f, .drag = 0.6f, .buoyancy = -3.0f,
            .color = { 110, 130, 116 }, .count = 2 } },
    };

    constexpr std::size_t ThemeCount = sizeof(Themes) / sizeof(Themes[0]);
}

int depthCount()
{
    return static_cast<int>(ThemeCount);
}

const DepthTheme& themeFor(int depth)
{
    // Everything past the authored depths stays in the dark rather than
    // wrapping back around to daylight.
    const std::size_t index = static_cast<std::size_t>(std::clamp(depth, 1, static_cast<int>(ThemeCount))) - 1;
    return Themes[index];
}

sf::Color tintBy(sf::Color base, sf::Color tint)
{
    const auto mix = [](std::uint8_t a, std::uint8_t b) {
        return static_cast<std::uint8_t>(static_cast<int>(a) * static_cast<int>(b) / 255);
    };

    return sf::Color(mix(base.r, tint.r), mix(base.g, tint.g), mix(base.b, tint.b), base.a);
}
