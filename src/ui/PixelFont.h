#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <string_view>

// A 5x7 bitmap font baked into the binary. The project ships no font file and
// this keeps it that way: nothing to install, nothing to fail to load. Text is
// appended as quads so a whole screen of it costs one draw call.
namespace pixelfont
{
    inline constexpr float GlyphWidth = 5.0f;
    inline constexpr float GlyphHeight = 7.0f;
    inline constexpr float Tracking = 1.0f; // blank columns between glyphs

    // Width in pixels the text will occupy at the given scale.
    float measure(std::string_view text, float scale);

    // vertices must have been created with sf::PrimitiveType::Triangles.
    void append(sf::VertexArray& vertices, std::string_view text,
                sf::Vector2f position, float scale, sf::Color color);
}
