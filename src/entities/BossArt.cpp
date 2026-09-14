#include "entities/BossArt.h"

#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    // The same hide the sprite was painted in, so nothing else in the basin has
    // to be retuned around it. Three tones and an ink line is the whole palette:
    // the relief pass that lights every other texture is a cheat for depth, and
    // a shape drawn in three tones with the light always coming from the same
    // corner gets the same thing honestly.
    constexpr sf::Color Hide{ 34, 58, 60 };
    constexpr sf::Color Belly{ 74, 104, 100 };
    constexpr sf::Color Ridge{ 22, 40, 42 };
    constexpr sf::Color Ink{ 8, 16, 18 };
    constexpr sf::Color Lit{ 96, 132, 126 };
    constexpr sf::Color Tooth{ 214, 224, 214 };
    constexpr sf::Color Gum{ 62, 22, 28 };
    constexpr sf::Color Iris{ 255, 176, 60 };
    constexpr sf::Color Pupil{ 16, 10, 6 };
    constexpr sf::Color Organ{ 176, 86, 104 };
    constexpr sf::Color OrganLit{ 236, 150, 160 };
    constexpr sf::Color Hurt{ 255, 220, 210 };

    constexpr float Tau = 6.283185307f;

    sf::Vector2f unit(sf::Angle angle)
    {
        return { std::cos(angle.asRadians()), std::sin(angle.asRadians()) };
    }

    sf::Color fade(sf::Color colour, float alpha)
    {
        colour.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
        return colour;
    }

    // Towards white, for the moment after something lands on a part.
    sf::Color flashed(sf::Color colour, float amount)
    {
        const auto mix = [amount](std::uint8_t from, std::uint8_t to) {
            return static_cast<std::uint8_t>(static_cast<float>(from)
                                             + (static_cast<float>(to) - static_cast<float>(from))
                                                   * std::clamp(amount, 0.0f, 1.0f));
        };

        return sf::Color(mix(colour.r, Hurt.r), mix(colour.g, Hurt.g), mix(colour.b, Hurt.b),
                         colour.a);
    }

    // A fan around a centre. Everything round in here is one of these.
    void disc(sf::VertexArray& into, sf::Vector2f centre, float radius, sf::Color colour,
              int steps = 18)
    {
        for (int i = 0; i < steps; ++i)
        {
            const sf::Angle a0 = sf::degrees(360.0f * static_cast<float>(i) / static_cast<float>(steps));
            const sf::Angle a1 = sf::degrees(360.0f * static_cast<float>(i + 1) / static_cast<float>(steps));

            into.append(sf::Vertex{ centre, colour });
            into.append(sf::Vertex{ centre + sf::Vector2f(radius, a0), colour });
            into.append(sf::Vertex{ centre + sf::Vector2f(radius, a1), colour });
        }
    }

    // A convex outline, as a fan off its own first point. Every shape in the
    // creature is convex on purpose - a fan is three lines of code and a
    // general polygon is a triangulator, and nothing here needs one.
    void shape(sf::VertexArray& into, const std::vector<sf::Vector2f>& points, sf::Color colour)
    {
        for (std::size_t i = 1; i + 1 < points.size(); ++i)
        {
            into.append(sf::Vertex{ points[0], colour });
            into.append(sf::Vertex{ points[i], colour });
            into.append(sf::Vertex{ points[i + 1], colour });
        }
    }

    void quad(sf::VertexArray& into, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c,
              sf::Vector2f d, sf::Color colour)
    {
        into.append(sf::Vertex{ a, colour });
        into.append(sf::Vertex{ b, colour });
        into.append(sf::Vertex{ c, colour });

        into.append(sf::Vertex{ a, colour });
        into.append(sf::Vertex{ c, colour });
        into.append(sf::Vertex{ d, colour });
    }

    // A tapering limb: a quad from a wide root to a narrow tip, with the tip
    // pushed sideways so it curves.
    void sweep(sf::VertexArray& into, sf::Vector2f root, sf::Vector2f tip, float rootHalf,
               float tipHalf, sf::Color colour)
    {
        const sf::Vector2f along = tip - root;
        if (along.lengthSquared() < 0.0001f)
            return;

        const sf::Vector2f across = sf::Vector2f{ -along.y, along.x }.normalized();

        quad(into, root - across * rootHalf, tip - across * tipHalf,
             tip + across * tipHalf, root + across * rootHalf, colour);
    }

    // --- the body ------------------------------------------------------
    //
    // Drawn tail first so the head sits on top of its own neck, and as a run of
    // quads between consecutive links rather than as a string of discs: discs
    // read as beads however close together they are, and the whole point of a
    // spine is that it is one animal.
    void drawSpine(sf::VertexArray& into, const bossart::Body& body, float grow, sf::Color colour,
                   sf::Vector2f offset)
    {
        const std::size_t count = body.links.size();
        if (count < 2)
            return;

        for (std::size_t i = count - 1; i > 0; --i)
        {
            const bossart::Link& back = body.links[i];
            const bossart::Link& front = body.links[i - 1];

            const sf::Vector2f acrossBack = sf::Vector2f{ -unit(back.angle).y, unit(back.angle).x };
            const sf::Vector2f acrossFront = sf::Vector2f{ -unit(front.angle).y, unit(front.angle).x };

            const float wBack = back.girth + grow;
            const float wFront = front.girth + grow;

            quad(into,
                 back.at + offset - acrossBack * wBack,
                 front.at + offset - acrossFront * wFront,
                 front.at + offset + acrossFront * wFront,
                 back.at + offset + acrossBack * wBack,
                 colour);

            // A joint cap, so a hard turn does not open a notch on the outside
            // of the bend where the two quads pull apart.
            disc(into, front.at + offset, wFront, colour, 12);
        }

        // And the tail comes to a point rather than stopping on a flat end.
        const bossart::Link& tail = body.links[count - 1];
        const sf::Vector2f back = -unit(tail.angle);
        const sf::Vector2f across{ -back.y, back.x };
        const float width = tail.girth + grow;

        shape(into, { tail.at + offset + back * (width * 2.6f),
                      tail.at + offset + across * width,
                      tail.at + offset - across * width },
              colour);
    }

    void drawFluke(sf::VertexArray& into, const bossart::Body& body, sf::Color colour,
                   sf::Vector2f offset)
    {
        if (body.links.size() < 2)
            return;

        const bossart::Link& tail = body.links.back();
        const sf::Vector2f back = -unit(tail.angle);
        const sf::Vector2f across{ -back.y, back.x };

        // The fluke lags the tail: it is being dragged through water, so it is
        // always a beat behind whatever the body just did.
        const float lag = body.beat * 0.55f;
        const sf::Vector2f root = tail.at + offset + back * (tail.girth * 2.2f);

        for (const float side : { -1.0f, 1.0f })
        {
            const sf::Vector2f tip = root + back * (tail.girth * 2.4f)
                                     + across * (side * tail.girth * 2.8f + lag * tail.girth);

            shape(into, { root,
                          root + across * (side * tail.girth * 0.5f),
                          tip,
                          tip - back * (tail.girth * 1.5f) },
                  colour);
        }
    }

    // --- the head ------------------------------------------------------
    //
    // Built around the first link and pointing along it. The jaw is two
    // mandibles hinged at the back of the skull: shut they close over the
    // teeth, open they swing apart, and the tell is what opens them - so the
    // wind-up is visible on the animal itself instead of on a ring drawn
    // underneath it.
    void drawHead(sf::VertexArray& into, const bossart::Body& body, sf::Color hide,
                  sf::Vector2f offset, bool detail)
    {
        if (body.links.empty())
            return;

        const bossart::Link& head = body.links.front();
        const sf::Vector2f nose = unit(head.angle);
        const sf::Vector2f across{ -nose.y, nose.x };
        const sf::Vector2f at = head.at + offset;

        const float length = head.girth * 4.3f;
        const float wide = head.girth * 1.28f;

        // The skull, wider at the brow than at the snout.
        shape(into, { at + nose * (length * 0.15f),
                      at + nose * (length * 0.72f) - across * (wide * 0.52f),
                      at + nose * (length * 0.30f) - across * wide,
                      at - nose * (length * 0.40f) - across * (wide * 0.86f),
                      // Tapering into the neck rather than stopping on a flat
                      // cut, so the skull is the front of the animal instead
                      // of a slab sitting on it.
                      at - nose * (length * 0.80f) - across * (wide * 0.44f),
                      at - nose * (length * 0.80f) + across * (wide * 0.44f),
                      at - nose * (length * 0.40f) + across * (wide * 0.86f),
                      at + nose * (length * 0.30f) + across * wide,
                      at + nose * (length * 0.72f) + across * (wide * 0.52f) },
              hide);

        if (!detail)
            return;

        const float swing = 26.0f * body.jaw;
        const sf::Vector2f hinge = at - nose * (length * 0.18f);

        // The mouth behind the jaws, and only while they are apart: a shut
        // mouth has to be a shut mouth and not a red line down the snout.
        if (body.jaw > 0.02f)
        {
            const sf::Vector2f open0 = unit(head.angle + sf::degrees(swing));
            const sf::Vector2f open1 = unit(head.angle - sf::degrees(swing));

            shape(into, { hinge - nose * (wide * 0.30f),
                          hinge + open0 * (length * 0.84f),
                          hinge + nose * (length * 0.78f),
                          hinge + open1 * (length * 0.84f) },
                  Gum);
        }

        for (const float side : { -1.0f, 1.0f })
        {
            const sf::Vector2f out = unit(head.angle + sf::degrees(side * swing));
            const sf::Vector2f side2{ -out.y, out.x };
            const sf::Vector2f root = hinge + across * (side * wide * 0.66f);

            // One mandible.
            shape(into, { root - side2 * (side * wide * 0.30f),
                          root + out * (length * 0.92f) - side2 * (side * wide * 0.10f),
                          root + out * (length * 0.98f) + side2 * (side * wide * 0.14f),
                          root + side2 * (side * wide * 0.34f) },
                  hide);

            // Teeth along the inner edge of it, shrinking to the tip.
            for (int i = 0; i < 5; ++i)
            {
                const float t = static_cast<float>(i) / 4.0f;
                const float size = wide * (0.15f - 0.07f * t);

                // Rooted on the inner edge of the mandible and pointing across
                // the gap at the other one, which is where a tooth goes.
                const sf::Vector2f base = root + out * (length * (0.20f + 0.66f * t))
                                          - side2 * (side * wide * 0.16f);

                shape(into, { base - out * size,
                              base - side2 * (side * size * 2.2f),
                              base + out * size },
                      Tooth);
            }
        }

        // Gills, opening as it works for air it is not going to get.
        const float flare = 1.0f + body.breath * 0.9f;
        for (int i = 0; i < 3; ++i)
        {
            const float t = static_cast<float>(i) / 2.0f;
            const sf::Vector2f on = at - nose * (length * (0.18f + 0.28f * t));

            for (const float side : { -1.0f, 1.0f })
            {
                sweep(into, on + across * (side * wide * 0.34f),
                      on + across * (side * wide * 0.86f),
                      wide * 0.13f * flare, wide * 0.05f, Ridge);
            }
        }
    }

    // --- the eyes ------------------------------------------------------
    //
    // The one thing a sprite could never do. The iris slides inside the socket
    // towards whatever the creature is looking at, which for most of the fight
    // is the sub - so it is watching you, and when it stops watching you it is
    // because it is about to be somewhere else.
    void drawEye(sf::VertexArray& into, const bossart::Limb& eye, const bossart::Body& body,
                 sf::Vector2f offset)
    {
        const sf::Vector2f at = eye.at + offset;

        // The socket is a hitbox the size of a thing you can reasonably be
        // asked to hit; the eye in it is an eye. They are not the same number.
        const float radius = eye.size * 0.44f;

        if (!eye.alive)
        {
            // Put out. A socket and nothing in it.
            disc(into, at, radius, Ink, 14);
            disc(into, at, radius * 0.62f, Ridge, 12);
            return;
        }

        disc(into, at, radius * 1.14f, Ink, 16);
        disc(into, at, radius, flashed(Iris, eye.flash), 16);

        // The pupil narrows to a slit as the thing gets angrier, which is the
        // cheapest tell in the world and reads from right across the basin.
        const float slit = 1.0f - 0.45f * body.rage;
        const sf::Vector2f look = body.look * (radius * 0.34f);

        disc(into, at + look, radius * 0.44f * slit, Pupil, 12);
        disc(into, at + look - body.look * (radius * 0.10f), radius * 0.18f,
             sf::Color(255, 255, 255, 150), 10);
    }

    // --- the fins ------------------------------------------------------
    void drawFin(sf::VertexArray& into, const bossart::Limb& fin, const bossart::Body& body,
                 sf::Color hide, sf::Vector2f offset)
    {
        if (!fin.alive)
            return;

        // Each flank beats on the opposite half of the stroke, so the animal
        // rows rather than flapping like a bird.
        const float stroke = body.beat * fin.side;
        const sf::Angle angle = fin.angle + sf::degrees(stroke * 18.0f);

        const sf::Vector2f out = unit(angle);
        const sf::Vector2f across{ -out.y, out.x };
        const sf::Vector2f root = fin.at + offset;
        const float span = fin.size * 1.9f;

        shape(into, { root - across * (fin.size * 0.5f),
                      root + out * span - across * (fin.size * 0.9f),
                      root + out * (span * 1.15f) + across * (fin.size * 0.2f),
                      root + out * (span * 0.4f) + across * (fin.size * 1.0f),
                      root + across * (fin.size * 0.6f) },
              flashed(hide, fin.flash));

        // Rays, so it is a fin and not a leaf.
        for (int i = 1; i < 4; ++i)
        {
            const float t = static_cast<float>(i) / 4.0f;
            sweep(into, root, root + out * (span * (0.4f + 0.6f * t))
                                  + across * (fin.size * (1.0f - 1.9f * t)),
                  fin.size * 0.10f, fin.size * 0.04f, Ridge);
        }
    }

    void drawPlate(sf::VertexArray& into, const bossart::Limb& plate, sf::Vector2f offset)
    {
        if (!plate.alive)
            return;

        const sf::Vector2f out = unit(plate.angle);
        const sf::Vector2f across{ -out.y, out.x };
        const sf::Vector2f at = plate.at + offset;
        const float size = plate.size;

        shape(into, { at + out * (size * 1.30f),
                      at + out * (size * 0.55f) - across * (size * 1.05f),
                      at - out * (size * 0.85f) - across * (size * 0.95f),
                      at - out * (size * 1.25f),
                      at - out * (size * 0.85f) + across * (size * 0.95f),
                      at + out * (size * 0.55f) + across * (size * 1.05f) },
              flashed(Ridge, plate.flash));

        // Three raised bands across it, so it reads as bolted on rather than
        // as a darker patch of the same animal.
        for (int i = 0; i < 3; ++i)
        {
            const float t = -0.5f + 0.5f * static_cast<float>(i);
            const sf::Vector2f on = at + out * (size * t * 1.1f);

            sweep(into, on - across * (size * 0.86f), on + across * (size * 0.86f),
                  size * 0.16f, size * 0.16f, flashed(Lit, plate.flash * 0.6f));
        }
    }

    void drawOrgan(sf::VertexArray& into, const bossart::Limb& organ, const bossart::Body& body,
                   sf::Vector2f offset)
    {
        if (!organ.alive || !organ.shown)
            return;

        const sf::Vector2f at = organ.at + offset;

        // It beats. Faster the closer the thing is to dead, which is both the
        // only soft place on the animal and a health bar you do not have to
        // read.
        const float beat = 1.0f + 0.16f * std::sin(body.breath * Tau * 2.0f)
                           + 0.10f * body.rage;

        disc(into, at, organ.size * 1.25f * beat, sf::Color(Organ.r, Organ.g, Organ.b, 90), 16);
        disc(into, at, organ.size * beat, flashed(Organ, organ.flash), 16);
        disc(into, at - body.look * (organ.size * 0.2f), organ.size * 0.52f * beat,
             flashed(OrganLit, organ.flash), 14);

        // Vessels running back into the body.
        for (int i = 0; i < 5; ++i)
        {
            const sf::Angle a = sf::degrees(360.0f * static_cast<float>(i) / 5.0f
                                            + body.breath * 40.0f);
            sweep(into, at, at + unit(a) * (organ.size * 1.5f), organ.size * 0.14f,
                  organ.size * 0.03f, sf::Color(Organ.r, Organ.g, Organ.b, 150));
        }
    }
}

namespace bossart
{
    void drawLeviathan(sf::RenderTarget& target, const Body& body)
    {
        if (body.links.empty() || body.alpha <= 0.01f)
            return;

        sf::VertexArray mesh(sf::PrimitiveType::Triangles);

        // The light in this game always comes from up and left, because that is
        // where the relief pass puts it on every other texture in the basin.
        const sf::Vector2f up{ -3.0f, -4.0f };
        const sf::Vector2f down{ 3.0f, 4.0f };

        const float alpha = std::clamp(body.alpha, 0.0f, 1.0f);
        const auto tint = [alpha](sf::Color colour) { return fade(colour, alpha); };

        // A creature that has just been hit rocks back along its own length,
        // which is the difference between a shot landing and a shot being
        // subtracted from a number.
        const sf::Vector2f kick = body.links.empty()
                                      ? sf::Vector2f{ 0.0f, 0.0f }
                                      : -unit(body.links.front().angle) * (body.flinch * 7.0f);

        // Fins first: they hang off the flanks and the body sits over them.
        for (const Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Fin)
                drawFin(mesh, limb, body, tint(Hide), kick);
        }

        // Then the animal, three times: the shadow it casts on itself, the
        // hide, and the light down its back.
        drawSpine(mesh, body, 3.0f, tint(Ink), kick + down);
        drawHead(mesh, body, tint(Ink), kick + down, false);

        drawSpine(mesh, body, 0.0f, tint(Hide), kick);

        drawSpine(mesh, body, -6.0f, tint(Belly), kick + up);
        drawFluke(mesh, body, tint(Hide), kick);

        // The dorsal ridge, which is also where the flex shows: the plates
        // slide over one another as the spine bends.
        for (std::size_t i = 1; i + 1 < body.links.size(); ++i)
        {
            const Link& link = body.links[i];
            const sf::Vector2f out = unit(link.angle);
            const sf::Vector2f across{ -out.y, out.x };
            const sf::Vector2f at = link.at + kick;

            shape(mesh, { at - out * (link.girth * 0.40f) - across * (link.girth * 0.40f),
                          at + out * (link.girth * 0.10f) - across * (link.girth * 0.78f),
                          at + out * (link.girth * 0.40f) - across * (link.girth * 0.40f) },
                  tint(Ridge));
            shape(mesh, { at - out * (link.girth * 0.40f) + across * (link.girth * 0.40f),
                          at + out * (link.girth * 0.10f) + across * (link.girth * 0.78f),
                          at + out * (link.girth * 0.40f) + across * (link.girth * 0.40f) },
                  tint(Ridge));
        }

        for (const Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Organ)
                drawOrgan(mesh, limb, body, kick);
        }

        for (const Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Armour)
                drawPlate(mesh, limb, kick);
        }

        drawHead(mesh, body, tint(Hide), kick, true);

        for (const Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Eye)
                drawEye(mesh, limb, body, kick);
        }

        // And the wind-up, on the animal rather than under it: the whole thing
        // goes pale for the half second before it commits.
        if (body.tell > 0.0f)
        {
            const float glow = body.tell * body.tell * 0.16f;

            drawSpine(mesh, body, 1.0f,
                      fade(sf::Color(255, 140, 90), glow * alpha), kick);
            drawHead(mesh, body, fade(sf::Color(255, 160, 110), glow * alpha), kick, false);
        }

        target.draw(mesh);
    }
}
