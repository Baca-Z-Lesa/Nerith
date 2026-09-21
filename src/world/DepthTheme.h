#pragma once

#include <SFML/Graphics/Color.hpp>

#include <array>
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

    // What is lying about at THIS depth, on top of the rock and the kelp that
    // are at every depth. Up to three, nullptr for a slot that is unused.
    //
    // Without these a depth was a palette: the same two rocks, two corals and
    // one kelp from the shallows to the cradle, so with the HUD covered up
    // there was no telling the trench from the bottom. On a game whose whole
    // subject is going further down, that is the wrong thing not to be able
    // to tell.
    std::array<const char*, 3> dressing;

    // How much of the scatter outside the arena they take. Rises with depth:
    // near the surface the floor is mostly floor, and at the bottom most of
    // what is down there is evidence.
    float dressShare;
};

// How many there are. Anything that wants to walk every depth - the texture
// preload, for one - needs to know without reading the table itself.
int depthCount();

const DepthTheme& themeFor(int depth);

// Component-wise multiply, the same rule sf::Sprite::setColor uses.
sf::Color tintBy(sf::Color base, sf::Color tint);
