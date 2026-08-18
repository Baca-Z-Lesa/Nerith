#pragma once

#include <SFML/Graphics/Color.hpp>

#include <cstdint>

// How a depth looks and sounds. The descent is the whole point: the light gets
// smaller, the water loses its colour, the dark closes in tighter and the drone
// underneath it all drops in pitch. Adding a depth is a row in the table.
struct DepthTheme
{
    const char* name;

    sf::Color background;
    sf::Color sandNear;
    sf::Color sandFar;

    sf::Color light;       // the sub's own lamp
    float     lightRadius;

    sf::Color caustic;
    float     causticStrength; // multiplier on the drifting light bands

    std::uint8_t vignetteAlpha;
    float        vignetteTightness; // 1 is roomy, smaller pulls the dark inwards

    sf::Color propTint; // multiplied into every seabed prop

    float ambientPitch;  // 1 at the surface, lower and slower further down
    float ambientVolume;
};

const DepthTheme& themeFor(int depth);

// Component-wise multiply, the same rule sf::Sprite::setColor uses.
sf::Color tintBy(sf::Color base, sf::Color tint);
