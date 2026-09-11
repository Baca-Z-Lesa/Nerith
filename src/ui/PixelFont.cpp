#include "ui/PixelFont.h"

#include <array>
#include <cstdint>

namespace
{
    // Each glyph is seven rows of five bits, bit 4 being the leftmost column.
    using Glyph = std::array<std::uint8_t, 7>;

    // Order: space, 0-9, A-Z, then the handful of symbols the UI needs.
    constexpr std::array<Glyph, 47> Glyphs{ {
        { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000 }, // space

        { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 }, // 0
        { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 }, // 1
        { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111 }, // 2
        { 0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110 }, // 3
        { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 }, // 4
        { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 }, // 5
        { 0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 }, // 6
        { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 }, // 7
        { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 }, // 8
        { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100 }, // 9

        { 0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 }, // A
        { 0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110 }, // B
        { 0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110 }, // C
        { 0b11110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b11110 }, // D
        { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111 }, // E
        { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000 }, // F
        { 0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111 }, // G
        { 0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 }, // H
        { 0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b11111 }, // I
        { 0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100 }, // J
        { 0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001 }, // K
        { 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111 }, // L
        { 0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001 }, // M
        { 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001, 0b10001 }, // N
        { 0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 }, // O
        { 0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000 }, // P
        { 0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101 }, // Q
        { 0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001 }, // R
        { 0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110 }, // S
        { 0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100 }, // T
        { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 }, // U
        { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100 }, // V
        { 0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b11011, 0b10001 }, // W
        { 0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001 }, // X
        { 0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100 }, // Y
        { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111 }, // Z

        { 0b00000, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0b00000 }, // +
        { 0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000 }, // -
        { 0b11001, 0b11010, 0b00010, 0b00100, 0b01000, 0b01011, 0b10011 }, // %
        { 0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000 }, // /
        { 0b00000, 0b00100, 0b00100, 0b00000, 0b00100, 0b00100, 0b00000 }, // :
        { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100 }, // .
        { 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111 }, // #, a full block
        { 0b01000, 0b00100, 0b00010, 0b00001, 0b00010, 0b00100, 0b01000 }, // >
        { 0b00010, 0b00100, 0b01000, 0b10000, 0b01000, 0b00100, 0b00010 }, // <
        { 0b01110, 0b01110, 0b01110, 0b01110, 0b01110, 0b01110, 0b01110 }, // = a half block
    } };

    std::size_t glyphIndex(char character)
    {
        if (character >= 'a' && character <= 'z')
            character = static_cast<char>(character - 'a' + 'A');

        if (character >= '0' && character <= '9')
            return 1 + static_cast<std::size_t>(character - '0');
        if (character >= 'A' && character <= 'Z')
            return 11 + static_cast<std::size_t>(character - 'A');

        switch (character)
        {
            case '+': return 37;
            case '-': return 38;
            case '%': return 39;
            case '/': return 40;
            case ':': return 41;
            case '.': return 42;
            case '#': return 43;
            case '>': return 44;
            case '<': return 45;
            case '=': return 46;
            default:  return 0; // anything unknown renders as a blank
        }
    }

    void appendQuad(sf::VertexArray& vertices, sf::Vector2f topLeft, float size, sf::Color color)
    {
        const sf::Vector2f a{ topLeft.x, topLeft.y };
        const sf::Vector2f b{ topLeft.x + size, topLeft.y };
        const sf::Vector2f c{ topLeft.x + size, topLeft.y + size };
        const sf::Vector2f d{ topLeft.x, topLeft.y + size };

        vertices.append(sf::Vertex{ a, color });
        vertices.append(sf::Vertex{ b, color });
        vertices.append(sf::Vertex{ c, color });
        vertices.append(sf::Vertex{ a, color });
        vertices.append(sf::Vertex{ c, color });
        vertices.append(sf::Vertex{ d, color });
    }
}

float pixelfont::measure(std::string_view text, float scale)
{
    if (text.empty())
        return 0.0f;

    const float advance = (GlyphWidth + Tracking) * scale;
    return static_cast<float>(text.size()) * advance - Tracking * scale;
}

void pixelfont::append(sf::VertexArray& vertices, std::string_view text,
                       sf::Vector2f position, float scale, sf::Color color)
{
    float penX = position.x;

    for (const char character : text)
    {
        const Glyph& glyph = Glyphs[glyphIndex(character)];

        for (std::size_t row = 0; row < glyph.size(); ++row)
        {
            for (std::size_t column = 0; column < 5; ++column)
            {
                // Bit 4 is the leftmost column, so shift down from the left.
                if ((glyph[row] & (1u << (4 - column))) == 0)
                    continue;

                appendQuad(vertices,
                           { penX + static_cast<float>(column) * scale,
                             position.y + static_cast<float>(row) * scale },
                           scale, color);
            }
        }

        penX += (GlyphWidth + Tracking) * scale;
    }
}
