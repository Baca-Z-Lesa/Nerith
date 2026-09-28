#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>

// The water, as a post-process: what the basin looks like once there is
// water between it and the eye.
//
// Everything the lamp lights is already on the window when this runs, so the
// pass copies the window into a texture and draws it straight back through
// one fragment shader. That is four things no amount of geometry could do
// per pixel: the picture swims a little, light scatters off what hangs in the
// water, the warm end of the colour is taken out with distance from the lamp,
// and a bite pulls the channels apart. A fifth, dither, is there because a
// dark gradient in eight bits is a staircase otherwise.
//
// Copying the window rather than rendering the basin into a texture of its
// own means not one of the thirty drawing helpers in Game has to know about
// it. It runs in one place, between the light and the dark.
class WaterPass
{
public:
    // What changes from frame to frame. Positions are in pixels of whatever
    // is being drawn into, with the origin top left.
    struct Frame
    {
        sf::Vector2f lamp;         // where the sub is
        float        reach = 0.0f; // how far its lamp carries
        float        hurt = 0.0f;  // 1 at the moment of a bite, fading to 0
    };

    WaterPass();

    // False on a card without shaders, or if the shader would not compile.
    // The caller then draws the absorption the old way, as a mesh.
    bool ready() const { return m_ready; }

    // Take what is on the window now and put it back through the water.
    void apply(sf::RenderWindow& window, const Frame& frame);

    // The same, from a picture already in a texture of the target's size.
    // The game never calls this; the headless previews, which draw into a
    // texture instead of a window, do.
    void draw(sf::RenderTarget& target, const sf::Texture& picture, const Frame& frame);

private:
    sf::Shader  m_shader;
    sf::Texture m_grab;
    sf::Clock   m_clock;
    bool        m_ready = false;
};
