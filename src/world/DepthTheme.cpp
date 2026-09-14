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
          1.00f, 34.0f },

        { "THE SHELF",
          { 2, 4, 8 }, { 12, 25, 33 }, { 7, 15, 22 },
          { 26, 66, 84 }, 312.0f,
          { 60, 150, 172 }, 0.82f,
          246, 0.90f,
          { 224, 230, 240 },
          0.94f, 36.0f },

        { "THE TRENCH",
          { 3, 3, 8 }, { 14, 16, 32 }, { 8, 9, 19 },
          { 32, 46, 92 }, 284.0f,
          { 72, 92, 186 }, 0.62f,
          249, 0.79f,
          { 202, 198, 228 },
          0.88f, 38.0f },

        { "THE ABYSS",
          { 3, 2, 4 }, { 17, 12, 17 }, { 9, 7, 10 },
          { 48, 30, 56 }, 256.0f,
          { 116, 62, 126 }, 0.46f,
          251, 0.69f,
          { 190, 168, 182 },
          0.82f, 40.0f },

        // The floor is open here and what comes out of it is the only light
        // the depth has of its own.
        { "THE VENTS",
          { 2, 1, 2 }, { 15, 8, 8 }, { 8, 4, 4 },
          { 60, 26, 26 }, 228.0f,
          { 146, 52, 42 }, 0.32f,
          253, 0.59f,
          { 186, 150, 146 },
          0.74f, 42.0f },

        // And the bottom of it. Nothing here is any colour; the only thing the
        // lamp finds is bone, and the dark is tight enough to touch.
        { "THE CRADLE",
          { 1, 2, 2 }, { 11, 14, 12 }, { 5, 7, 6 },
          { 44, 62, 52 }, 206.0f,
          { 94, 132, 108 }, 0.22f,
          254, 0.50f,
          { 168, 178, 168 },
          0.66f, 44.0f },
    };

    constexpr std::size_t ThemeCount = sizeof(Themes) / sizeof(Themes[0]);
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
