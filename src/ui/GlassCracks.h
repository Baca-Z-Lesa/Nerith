#pragma once

#include "core/Random.h"

#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <cmath>
#include <cstdint>

// The viewport glass. Every bite that lands puts another star in it, and the
// cracks stay for the rest of the dive - a running tally of what the water has
// done to the one thing between you and it, written where you cannot help but
// read it. Cleared when a fresh sub goes down.
//
// Header-only and self-contained: it owns one mesh in screen space, appended to
// on each hit and never rebuilt, so an accumulating pane costs nothing per
// frame beyond drawing it. Nothing here reads the game; the caller says where a
// hit landed and when the glass is new.
class GlassCracks
{
public:
    void clear()
    {
        m_lines.clear();
        m_count = 0;
    }

    int count() const { return m_count; }

    // A new star. Glass does two things when something hits it: it splits
    // outward in near-straight radials, and it shears across those radials in
    // rings. Only the radials and it reads as a bare tree; both together and it
    // reads as a pane. Later hits are drawn over earlier ones, so a battered
    // viewport builds up in layers rather than one busy scribble.
    void add(sf::Vector2f at)
    {
        ++m_count;

        const int arms = 5 + static_cast<int>(rng::range(0.0f, 2.99f)); // 5..7
        const float base = rng::range(0.0f, 6.2831853f);
        const float span = 6.2831853f / static_cast<float>(arms);

        sf::Vector2f point[MaxArms][Rings + 1];

        for (int arm = 0; arm < arms; ++arm)
        {
            // Near-radial: the wander is small and the run is long, which is
            // what keeps a fracture from curling up like a root.
            float angle = base + static_cast<float>(arm) * span + rng::range(-0.13f, 0.13f);
            float radius = 0.0f;
            float wide = rng::range(1.7f, 2.3f);

            point[arm][0] = at;

            for (int ring = 1; ring <= Rings; ++ring)
            {
                radius += rng::range(11.0f, 17.0f) * static_cast<float>(ring);
                angle += rng::range(-0.12f, 0.12f);

                point[arm][ring] =
                    at + sf::Vector2f{ std::cos(angle), std::sin(angle) } * radius;

                stroke(point[arm][ring - 1], point[arm][ring], wide);
                wide = std::max(0.5f, wide - 0.45f);
            }

            // One radial in a few runs past the last ring and dies out there,
            // so the star does not end on a perfect circle.
            if (rng::range(0.0f, 1.0f) < 0.45f)
            {
                stroke(point[arm][Rings],
                       point[arm][Rings]
                           + sf::Vector2f{ std::cos(angle), std::sin(angle) }
                                 * rng::range(18.0f, 40.0f),
                       0.6f);
            }
        }

        // And the shears across them. Skipped here and there, because a pane
        // that cracks all the way round on every ring is a spider web.
        for (int ring = 1; ring < Rings; ++ring)
        {
            for (int arm = 0; arm < arms; ++arm)
            {
                if (rng::range(0.0f, 1.0f) < 0.32f)
                    continue;

                stroke(point[arm][ring], point[(arm + 1) % arms][ring],
                       ring == 1 ? 1.1f : 0.8f);
            }
        }

        // The pock itself: a tight bright knot where the thing actually struck.
        for (int tick = 0; tick < 7; ++tick)
        {
            const float a = rng::range(0.0f, 6.2831853f);
            stroke(at, at + sf::Vector2f{ std::cos(a), std::sin(a) } * rng::range(3.0f, 7.0f),
                   2.2f);
        }
    }

    void draw(sf::RenderTarget& target) const
    {
        if (m_lines.getVertexCount() != 0)
            target.draw(m_lines);
    }

private:
    // One crack segment: a dark spread to lift it off the water, and a thin
    // cold highlight down the middle where the lamp catches the fracture.
    void stroke(sf::Vector2f a, sf::Vector2f b, float wide)
    {
        quad(a, b, wide, sf::Color(4, 8, 12, 150));
        quad(a, b, std::max(0.5f, wide * 0.5f), sf::Color(150, 186, 205, 140));
    }

    void quad(sf::Vector2f a, sf::Vector2f b, float wide, sf::Color colour)
    {
        sf::Vector2f along = b - a;
        const float length = std::sqrt(along.x * along.x + along.y * along.y);
        if (length < 0.001f)
            return;

        along /= length;
        const sf::Vector2f side{ -along.y * wide, along.x * wide };

        m_lines.append(sf::Vertex{ a - side, colour });
        m_lines.append(sf::Vertex{ b - side, colour });
        m_lines.append(sf::Vertex{ b + side, colour });

        m_lines.append(sf::Vertex{ a - side, colour });
        m_lines.append(sf::Vertex{ b + side, colour });
        m_lines.append(sf::Vertex{ a + side, colour });
    }

    static constexpr int MaxArms = 8;
    static constexpr int Rings = 3;

    sf::VertexArray m_lines{ sf::PrimitiveType::Triangles };
    int             m_count = 0;
};
