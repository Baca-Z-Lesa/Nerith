#include "ui/UpgradeIcons.h"

#include <cmath>

namespace
{
    // Marks are drawn in a unit square running -1..1 and scaled on the way out,
    // so every icon is authored at the same size no matter where it is used.
    constexpr float Pi = 3.14159265f;

    struct Pen
    {
        sf::VertexArray* out;
        sf::Vector2f     centre;
        float            scale;     // unit to pixels
        float            thickness; // in pixels
        sf::Color        colour;

        sf::Vector2f at(float x, float y) const
        {
            return { centre.x + x * scale, centre.y + y * scale };
        }

        void tri(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c) const
        {
            out->append(sf::Vertex{ a, colour });
            out->append(sf::Vertex{ b, colour });
            out->append(sf::Vertex{ c, colour });
        }

        void quad(sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Vector2f d) const
        {
            tri(a, b, c);
            tri(a, c, d);
        }

        // A stroke from one unit point to another, as a thickened quad.
        void line(float x0, float y0, float x1, float y1, float weight = 1.0f) const
        {
            const sf::Vector2f from = at(x0, y0);
            const sf::Vector2f to = at(x1, y1);
            const sf::Vector2f delta = to - from;

            if (delta.lengthSquared() <= 0.0f)
                return;

            const sf::Vector2f dir = delta.normalized();
            const sf::Vector2f n{ -dir.y, dir.x };
            const sf::Vector2f off = n * (thickness * weight * 0.5f);

            quad(from - off, from + off, to + off, to - off);
        }

        void disc(float x, float y, float radius, int points = 14) const
        {
            const sf::Vector2f middle = at(x, y);
            const float r = radius * scale;

            for (int i = 0; i < points; ++i)
            {
                const float a0 = 2.0f * Pi * static_cast<float>(i) / static_cast<float>(points);
                const float a1 = 2.0f * Pi * static_cast<float>(i + 1) / static_cast<float>(points);

                tri(middle,
                    middle + sf::Vector2f{ std::cos(a0) * r, std::sin(a0) * r },
                    middle + sf::Vector2f{ std::cos(a1) * r, std::sin(a1) * r });
            }
        }

        // An arc of stroke, from `from` to `to` degrees.
        void arc(float x, float y, float radius, float from, float to, float weight = 1.0f) const
        {
            const int steps = 12;
            for (int i = 0; i < steps; ++i)
            {
                const float t0 = from + (to - from) * static_cast<float>(i) / steps;
                const float t1 = from + (to - from) * static_cast<float>(i + 1) / steps;
                const float r0 = t0 * Pi / 180.0f;
                const float r1 = t1 * Pi / 180.0f;

                line(x + std::cos(r0) * radius, y + std::sin(r0) * radius,
                     x + std::cos(r1) * radius, y + std::sin(r1) * radius, weight);
            }
        }

        void ring(float x, float y, float radius, float weight = 1.0f) const
        {
            arc(x, y, radius, 0.0f, 360.0f, weight);
        }

        // An arrowhead pointing along (dx, dy).
        void head(float x, float y, float dx, float dy, float size) const
        {
            const sf::Vector2f dir = sf::Vector2f{ dx, dy }.normalized();
            const sf::Vector2f side{ -dir.y, dir.x };
            const sf::Vector2f tip = at(x, y);

            tri(tip,
                tip - (dir * size - side * size * 0.62f) * scale,
                tip - (dir * size + side * size * 0.62f) * scale);
        }

        // The recurring silhouette: a little hull, nose to the right.
        void hull(float x, float y, float length, float height) const
        {
            const sf::Vector2f a = at(x - length, y - height);
            const sf::Vector2f b = at(x + length * 0.55f, y - height);
            const sf::Vector2f c = at(x + length, y);
            const sf::Vector2f d = at(x + length * 0.55f, y + height);
            const sf::Vector2f e = at(x - length, y + height);

            tri(a, b, c);
            tri(a, c, d);
            tri(a, d, e);
        }

        // A dart of water: the projectile shape, pointing right.
        void dart(float x, float y, float size) const
        {
            tri(at(x + size, y),
                at(x - size * 0.8f, y - size * 0.52f),
                at(x - size * 0.8f, y + size * 0.52f));
        }

        void shield(float x, float y, float size) const
        {
            const sf::Vector2f top = at(x, y - size);
            const sf::Vector2f left = at(x - size * 0.78f, y - size * 0.45f);
            const sf::Vector2f right = at(x + size * 0.78f, y - size * 0.45f);
            const sf::Vector2f tip = at(x, y + size);

            tri(left, right, top);
            tri(left, right, tip);
        }

        void drop(float x, float y, float size) const
        {
            tri(at(x, y - size), at(x - size * 0.62f, y + size * 0.4f),
                at(x + size * 0.62f, y + size * 0.4f));
            disc(x, y + size * 0.34f, size * 0.5f, 10);
        }
    };

    void draw(const Pen& p, Icon icon)
    {
        switch (icon)
        {
            case Icon::None:
                break;

            // ------------------------------------------------------- weapons
            case Icon::Jet:
                p.dart(0.1f, 0.0f, 0.62f);
                p.line(-0.92f, -0.34f, -0.42f, -0.20f, 0.7f);
                p.line(-0.92f, 0.34f, -0.42f, 0.20f, 0.7f);
                break;

            case Icon::Muzzle:
                p.quad(p.at(-0.9f, -0.24f), p.at(0.1f, -0.24f), p.at(0.1f, 0.24f),
                       p.at(-0.9f, 0.24f));
                p.line(0.24f, 0.0f, 0.88f, 0.0f);
                p.line(0.32f, -0.5f, 0.66f, -0.78f, 0.8f);
                p.line(0.32f, 0.5f, 0.66f, 0.78f, 0.8f);
                break;

            case Icon::RapidFire:
                p.dart(0.2f, -0.62f, 0.42f);
                p.dart(-0.1f, 0.0f, 0.42f);
                p.dart(0.2f, 0.62f, 0.42f);
                break;

            case Icon::Pierce:
                p.ring(-0.34f, 0.0f, 0.46f, 0.7f);
                p.ring(0.44f, 0.0f, 0.46f, 0.7f);
                p.line(-0.95f, 0.0f, 0.95f, 0.0f);
                p.head(0.95f, 0.0f, 1.0f, 0.0f, 0.34f);
                break;

            case Icon::Harpoon:
                // A barbed head on a line: the barbs are the point of it, so
                // they are drawn big enough to survive the icon being small.
                p.line(-0.95f, 0.0f, 0.55f, 0.0f, 0.7f);
                p.head(0.95f, 0.0f, 1.0f, 0.0f, 0.36f);
                p.line(0.42f, 0.0f, 0.06f, -0.44f, 0.7f);
                p.line(0.42f, 0.0f, 0.06f, 0.44f, 0.7f);
                break;

            case Icon::Pulse:
                // The hull, and the water leaving it in every direction.
                p.disc(0.0f, 0.0f, 0.22f, 12);
                p.arc(0.0f, 0.0f, 0.56f, 0.0f, 360.0f);
                p.arc(0.0f, 0.0f, 0.92f, 0.0f, 360.0f);
                p.line(0.56f, 0.0f, 0.92f, 0.0f, 0.6f);
                p.line(-0.56f, 0.0f, -0.92f, 0.0f, 0.6f);
                break;

            case Icon::Split:
                p.line(-0.95f, 0.0f, -0.1f, 0.0f);
                p.line(-0.1f, 0.0f, 0.9f, -0.62f, 0.8f);
                p.line(-0.1f, 0.0f, 0.95f, 0.0f, 0.8f);
                p.line(-0.1f, 0.0f, 0.9f, 0.62f, 0.8f);
                break;

            case Icon::Cannon:
                p.quad(p.at(-0.9f, -0.4f), p.at(0.2f, -0.4f), p.at(0.2f, 0.4f), p.at(-0.9f, 0.4f));
                p.quad(p.at(0.2f, -0.22f), p.at(0.95f, -0.22f), p.at(0.95f, 0.22f),
                       p.at(0.2f, 0.22f));
                break;

            case Icon::TwinCannon:
                p.quad(p.at(-0.9f, -0.7f), p.at(0.9f, -0.7f), p.at(0.9f, -0.24f),
                       p.at(-0.9f, -0.24f));
                p.quad(p.at(-0.9f, 0.24f), p.at(0.9f, 0.24f), p.at(0.9f, 0.7f),
                       p.at(-0.9f, 0.7f));
                break;

            case Icon::Crosshair:
                p.ring(0.0f, 0.0f, 0.66f, 0.8f);
                p.line(-0.95f, 0.0f, -0.42f, 0.0f, 0.8f);
                p.line(0.42f, 0.0f, 0.95f, 0.0f, 0.8f);
                p.line(0.0f, -0.95f, 0.0f, -0.42f, 0.8f);
                p.line(0.0f, 0.42f, 0.0f, 0.95f, 0.8f);
                break;

            case Icon::Crit:
                p.ring(0.0f, 0.0f, 0.62f, 0.7f);
                p.disc(0.0f, 0.0f, 0.16f, 10);
                p.line(0.44f, -0.44f, 0.95f, -0.95f);
                p.line(0.62f, -0.95f, 0.95f, -0.62f, 0.7f);
                break;

            case Icon::Charge:
                p.arc(0.0f, 0.0f, 0.34f, 200.0f, 520.0f, 0.9f);
                p.arc(0.0f, 0.0f, 0.66f, 220.0f, 500.0f, 0.9f);
                p.disc(0.0f, 0.0f, 0.14f, 10);
                p.head(0.62f, -0.34f, 0.6f, -0.8f, 0.34f);
                break;

            // ----------------------------------------------------- torpedoes
            case Icon::Torpedo:
                p.hull(0.0f, 0.0f, 0.82f, 0.32f);
                p.line(-0.86f, -0.62f, -0.5f, -0.3f, 0.7f);
                p.line(-0.86f, 0.62f, -0.5f, 0.3f, 0.7f);
                break;

            case Icon::Warhead:
                p.hull(-0.2f, 0.0f, 0.62f, 0.3f);
                p.line(0.44f, 0.0f, 0.95f, -0.5f, 0.8f);
                p.line(0.44f, 0.0f, 0.95f, 0.0f, 0.8f);
                p.line(0.44f, 0.0f, 0.95f, 0.5f, 0.8f);
                break;

            case Icon::Reload:
                p.arc(0.0f, 0.0f, 0.66f, 40.0f, 330.0f);
                p.head(0.5f, -0.43f, 0.75f, 0.66f, 0.4f);
                break;

            case Icon::Seeker:
                p.hull(-0.3f, 0.2f, 0.5f, 0.24f);
                p.arc(0.2f, -0.2f, 0.7f, 200.0f, 340.0f, 0.7f);
                p.disc(0.62f, -0.56f, 0.16f, 10);
                break;

            case Icon::Cluster:
                p.disc(0.0f, 0.0f, 0.24f, 12);
                for (int i = 0; i < 6; ++i)
                {
                    const float a = 2.0f * Pi * static_cast<float>(i) / 6.0f;
                    p.disc(std::cos(a) * 0.7f, std::sin(a) * 0.7f, 0.15f, 8);
                }
                break;

            case Icon::Blast:
                p.disc(0.0f, 0.0f, 0.2f, 12);
                p.ring(0.0f, 0.0f, 0.52f, 0.8f);
                p.ring(0.0f, 0.0f, 0.9f, 0.6f);
                break;

            // --------------------------------------------------------- hull
            case Icon::Hull:
                p.arc(0.0f, 0.2f, 0.86f, 180.0f, 360.0f);
                p.line(-0.86f, 0.2f, 0.86f, 0.2f, 0.8f);
                p.line(-0.3f, -0.6f, -0.3f, 0.2f, 0.6f);
                p.line(0.3f, -0.6f, 0.3f, 0.2f, 0.6f);
                break;

            case Icon::Plate:
                p.quad(p.at(-0.82f, -0.7f), p.at(0.82f, -0.7f), p.at(0.82f, 0.7f),
                       p.at(-0.82f, 0.7f));
                p.disc(-0.5f, -0.4f, 0.12f, 8);
                p.disc(0.5f, -0.4f, 0.12f, 8);
                p.disc(-0.5f, 0.4f, 0.12f, 8);
                p.disc(0.5f, 0.4f, 0.12f, 8);
                break;

            case Icon::Heart:
                for (int i = 0; i < 2; ++i)
                {
                    const float x = -0.4f + 0.8f * static_cast<float>(i);
                    const int points = 6;
                    const float r = 0.44f;
                    for (int k = 0; k < points; ++k)
                    {
                        const float a0 = 2.0f * Pi * static_cast<float>(k) / points;
                        const float a1 = 2.0f * Pi * static_cast<float>(k + 1) / points;
                        p.tri(p.at(x, 0.0f),
                              p.at(x + std::cos(a0) * r, std::sin(a0) * r),
                              p.at(x + std::cos(a1) * r, std::sin(a1) * r));
                    }
                }
                break;

            case Icon::Mercy:
                p.shield(0.0f, 0.0f, 0.8f);
                p.line(0.0f, 0.0f, 0.0f, -0.4f, 0.6f);
                p.line(0.0f, 0.0f, 0.36f, 0.1f, 0.6f);
                break;

            case Icon::Vent:
                p.line(-0.82f, 0.7f, 0.82f, 0.7f);
                for (int i = 0; i < 3; ++i)
                {
                    const float x = -0.52f + 0.52f * static_cast<float>(i);
                    p.line(x, 0.6f, x, -0.3f, 0.7f);
                    p.head(x, -0.55f, 0.0f, -1.0f, 0.32f);
                }
                break;

            case Icon::Repair:
                p.line(-0.7f, 0.7f, 0.42f, -0.42f);
                p.ring(0.6f, -0.6f, 0.32f, 0.9f);
                p.line(-0.82f, 0.46f, -0.46f, 0.82f, 0.8f);
                break;

            // ------------------------------------------------------- engine
            case Icon::Propeller:
                p.disc(0.0f, 0.0f, 0.2f, 10);
                for (int i = 0; i < 3; ++i)
                {
                    const float a = 2.0f * Pi * static_cast<float>(i) / 3.0f;
                    p.tri(p.at(0.0f, 0.0f),
                          p.at(std::cos(a) * 0.95f, std::sin(a) * 0.95f),
                          p.at(std::cos(a + 0.8f) * 0.72f, std::sin(a + 0.8f) * 0.72f));
                }
                break;

            case Icon::Dash:
                for (int i = 0; i < 3; ++i)
                {
                    const float x = -0.7f + 0.62f * static_cast<float>(i);
                    p.line(x, -0.6f, x + 0.44f, 0.0f, 0.9f);
                    p.line(x + 0.44f, 0.0f, x, 0.6f, 0.9f);
                }
                break;

            case Icon::Shock:
                p.disc(0.0f, 0.0f, 0.24f, 12);
                for (int i = 0; i < 8; ++i)
                {
                    const float a = 2.0f * Pi * static_cast<float>(i) / 8.0f;
                    p.line(std::cos(a) * 0.42f, std::sin(a) * 0.42f,
                           std::cos(a) * 0.95f, std::sin(a) * 0.95f, 0.7f);
                }
                break;

            case Icon::Ghost:
                for (int i = 0; i < 5; ++i)
                {
                    const float a0 = 2.0f * Pi * static_cast<float>(i) / 5.0f + 0.2f;
                    const float a1 = a0 + 0.72f;
                    p.line(std::cos(a0) * 0.8f, std::sin(a0) * 0.8f,
                           std::cos(a1) * 0.8f, std::sin(a1) * 0.8f, 0.8f);
                }
                break;

            // -------------------------------------------------------- sonar
            case Icon::Sonar:
                p.disc(-0.7f, 0.6f, 0.18f, 10);
                p.arc(-0.7f, 0.6f, 0.55f, -90.0f, 0.0f, 0.9f);
                p.arc(-0.7f, 0.6f, 0.95f, -90.0f, 0.0f, 0.8f);
                p.arc(-0.7f, 0.6f, 1.35f, -90.0f, 0.0f, 0.7f);
                break;

            case Icon::Map:
                p.quad(p.at(-0.86f, -0.7f), p.at(0.86f, -0.7f), p.at(0.86f, 0.7f),
                       p.at(-0.86f, 0.7f));
                p.line(-0.86f, 0.0f, 0.86f, 0.0f, 0.45f);
                p.line(0.0f, -0.7f, 0.0f, 0.7f, 0.45f);
                p.disc(0.42f, -0.34f, 0.2f, 10);
                break;

            case Icon::Eye:
                p.arc(0.0f, 0.0f, 0.88f, 200.0f, 340.0f, 0.9f);
                p.arc(0.0f, 0.0f, 0.88f, 20.0f, 160.0f, 0.9f);
                p.disc(0.0f, 0.0f, 0.3f, 12);
                break;

            case Icon::Mark:
                p.disc(0.0f, 0.0f, 0.24f, 12);
                p.line(-0.9f, -0.9f, -0.3f, -0.9f, 0.8f);
                p.line(-0.9f, -0.9f, -0.9f, -0.3f, 0.8f);
                p.line(0.9f, 0.9f, 0.3f, 0.9f, 0.8f);
                p.line(0.9f, 0.9f, 0.9f, 0.3f, 0.8f);
                break;

            // ------------------------------------------------------ defense
            case Icon::Shield:
                p.shield(0.0f, 0.0f, 0.88f);
                break;

            case Icon::ShieldTwo:
                p.shield(-0.32f, 0.0f, 0.62f);
                p.shield(0.4f, 0.0f, 0.62f);
                break;

            case Icon::Reactive:
                p.shield(0.0f, 0.1f, 0.7f);
                p.line(-0.9f, -0.6f, -0.5f, -0.3f, 0.7f);
                p.line(0.9f, -0.6f, 0.5f, -0.3f, 0.7f);
                p.line(0.0f, -0.95f, 0.0f, -0.7f, 0.7f);
                break;

            case Icon::Orbit:
                p.hull(0.0f, 0.0f, 0.4f, 0.2f);
                p.ring(0.0f, 0.0f, 0.82f, 0.5f);
                p.disc(0.82f, 0.0f, 0.16f, 8);
                p.disc(-0.41f, -0.71f, 0.16f, 8);
                p.disc(-0.41f, 0.71f, 0.16f, 8);
                break;

            // ----------------------------------------------------- mutation
            case Icon::Bleed:
                p.drop(-0.5f, -0.3f, 0.34f);
                p.drop(0.1f, 0.2f, 0.34f);
                p.drop(0.66f, -0.45f, 0.3f);
                break;

            case Icon::Fang:
                p.tri(p.at(-0.62f, -0.8f), p.at(-0.2f, -0.8f), p.at(-0.4f, 0.86f));
                p.tri(p.at(0.2f, -0.8f), p.at(0.62f, -0.8f), p.at(0.4f, 0.86f));
                p.line(-0.86f, -0.82f, 0.86f, -0.82f, 0.8f);
                break;

            case Icon::Spore:
                p.disc(0.0f, 0.0f, 0.34f, 12);
                p.ring(0.0f, 0.0f, 0.72f, 0.5f);
                for (int i = 0; i < 6; ++i)
                {
                    const float a = 2.0f * Pi * static_cast<float>(i) / 6.0f;
                    p.disc(std::cos(a) * 0.72f, std::sin(a) * 0.72f, 0.1f, 8);
                }
                break;

            case Icon::Drone:
                p.hull(0.0f, 0.0f, 0.44f, 0.26f);
                p.ring(0.0f, 0.0f, 0.86f, 0.5f);
                break;

            case Icon::Hive:
                p.hull(-0.42f, -0.4f, 0.32f, 0.18f);
                p.hull(0.48f, -0.3f, 0.32f, 0.18f);
                p.hull(0.0f, 0.52f, 0.32f, 0.18f);
                p.ring(0.0f, 0.0f, 0.9f, 0.4f);
                break;

            case Icon::Predator:
                p.arc(0.0f, 0.0f, 0.9f, 205.0f, 335.0f, 0.9f);
                p.arc(0.0f, 0.0f, 0.9f, 25.0f, 155.0f, 0.9f);
                p.quad(p.at(-0.12f, -0.45f), p.at(0.12f, -0.45f), p.at(0.12f, 0.45f),
                       p.at(-0.12f, 0.45f));
                break;

            case Icon::Abyssal:
                p.disc(0.0f, -0.1f, 0.42f, 12);
                for (int i = 0; i < 7; ++i)
                {
                    const float a = Pi * (0.15f + 0.7f * static_cast<float>(i) / 6.0f);
                    p.line(std::cos(a) * 0.3f, -0.1f + std::sin(a) * 0.3f,
                           std::cos(a) * 0.95f, -0.1f + std::sin(a) * 1.05f, 0.6f);
                }
                break;

            case Icon::Corpse:
                p.ring(0.0f, 0.0f, 0.4f, 0.7f);
                for (int i = 0; i < 5; ++i)
                {
                    const float a = 2.0f * Pi * static_cast<float>(i) / 5.0f + 0.4f;
                    p.tri(p.at(std::cos(a) * 0.5f, std::sin(a) * 0.5f),
                          p.at(std::cos(a + 0.3f) * 1.0f, std::sin(a + 0.3f) * 1.0f),
                          p.at(std::cos(a - 0.3f) * 1.0f, std::sin(a - 0.3f) * 1.0f));
                }
                break;

            // ------------------------------------------------------ salvage
            case Icon::Salvage:
                p.ring(-0.38f, 0.0f, 0.46f, 1.0f);
                p.disc(0.5f, 0.0f, 0.3f, 6);
                p.line(-0.38f, 0.0f, 0.5f, 0.0f, 0.6f);
                break;

            case Icon::Scrap:
                p.tri(p.at(-0.9f, -0.6f), p.at(0.3f, -0.85f), p.at(-0.2f, 0.3f));
                p.tri(p.at(0.1f, 0.9f), p.at(0.9f, 0.2f), p.at(0.2f, -0.2f));
                break;

            case Icon::Crate:
                p.quad(p.at(-0.8f, -0.6f), p.at(0.8f, -0.6f), p.at(0.8f, 0.72f),
                       p.at(-0.8f, 0.72f));
                p.line(-0.8f, -0.15f, 0.8f, -0.15f, 0.6f);
                p.line(0.0f, -0.6f, 0.0f, 0.72f, 0.6f);
                break;

            // --------------------------------------------------------- lamp
            case Icon::Lamp:
                p.disc(-0.6f, 0.0f, 0.28f, 12);
                p.tri(p.at(-0.36f, -0.2f), p.at(0.9f, -0.72f), p.at(0.9f, 0.72f));
                break;

            case Icon::Beacon:
                p.disc(0.0f, 0.0f, 0.34f, 12);
                for (int i = 0; i < 8; ++i)
                {
                    const float a = 2.0f * Pi * static_cast<float>(i) / 8.0f;
                    p.line(std::cos(a) * 0.5f, std::sin(a) * 0.5f,
                           std::cos(a) * 0.98f, std::sin(a) * 0.98f, 0.6f);
                }
                break;

            case Icon::Blackout:
                p.disc(-0.4f, 0.0f, 0.26f, 12);
                p.tri(p.at(-0.2f, -0.18f), p.at(0.8f, -0.6f), p.at(0.8f, 0.6f));
                p.line(-0.95f, 0.85f, 0.95f, -0.85f, 1.1f);
                break;

            // --------------------------------------------------------- root
            case Icon::Sub:
                p.hull(-0.05f, 0.0f, 0.8f, 0.4f);
                p.disc(0.2f, 0.0f, 0.2f, 10);
                p.line(-0.85f, -0.75f, -0.5f, -0.4f, 0.8f);
                p.line(-0.85f, 0.75f, -0.5f, 0.4f, 0.8f);
                break;

            case Icon::Count:
                break;
        }
    }
}

void icons::append(sf::VertexArray& vertices, Icon icon, sf::Vector2f centre, float size,
                   sf::Color colour)
{
    if (icon == Icon::None || icon == Icon::Count || size <= 0.0f)
        return;

    Pen pen;
    pen.out = &vertices;
    pen.centre = centre;
    pen.scale = size * 0.5f;

    // Thick enough to survive being zoomed out, thin enough not to fill in.
    pen.thickness = std::max(1.4f, size * 0.12f);
    pen.colour = colour;

    draw(pen, icon);
}
