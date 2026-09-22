#include "entities/BossArt.h"

#include "core/Config.h"

#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    // The same hide the sprite was painted in, so nothing else in the basin has
    // to be retuned around it. Three tones and an ink line is the whole palette;
    // the surface on top of it comes from the sheet, which is what every other
    // creature gets out of the relief pass.
    constexpr sf::Color Hide{ 34, 58, 60 };
    constexpr sf::Color Belly{ 74, 104, 100 };
    constexpr sf::Color Band{ 96, 124, 116 };
    constexpr sf::Color Ridge{ 22, 40, 42 };
    constexpr sf::Color Ink{ 8, 16, 18 };
    constexpr sf::Color Lit{ 96, 132, 126 };
    constexpr sf::Color Tooth{ 214, 224, 214 };
    constexpr sf::Color Gum{ 62, 22, 28 };
    constexpr sf::Color Throat{ 34, 12, 16 };
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

    sf::Vector2f across(sf::Vector2f of) { return { -of.y, of.x }; }

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

    // ------------------------------------------------------------------
    //  Surface
    //
    //  Where a point on the animal sits on the sheet. Measured in the body's
    //  own frame rather than in the world, so the scales belong to the
    //  creature: they run down its length, they turn when it turns, and they
    //  do not swim across it while it moves.
    // ------------------------------------------------------------------
    struct Skin
    {
        sf::Vector2f origin;
        sf::Vector2f along{ 1.0f, 0.0f };
        float        shift = 0.0f;  // how far down the body this piece starts
        float        tile = 128.0f; // texture pixels across one tile
    };

    sf::Vector2f uvOf(const Skin& skin, sf::Vector2f at)
    {
        const sf::Vector2f d = at - skin.origin;
        const float per = skin.tile / config::BossHideTile;

        return { (d.dot(skin.along) + skin.shift) * per, d.dot(across(skin.along)) * per };
    }

    void vert(sf::VertexArray& into, sf::Vector2f at, sf::Color colour, const Skin& skin)
    {
        into.append(sf::Vertex{ at, colour, uvOf(skin, at) });
    }

    // A convex outline, as a fan off its own first point. Every shape in the
    // creature is convex on purpose - a fan is three lines of code and a
    // general polygon is a triangulator, and nothing here needs one.
    void shape(sf::VertexArray& into, const std::vector<sf::Vector2f>& points, sf::Color colour,
               const Skin& skin)
    {
        for (std::size_t i = 1; i + 1 < points.size(); ++i)
        {
            vert(into, points[0], colour, skin);
            vert(into, points[i], colour, skin);
            vert(into, points[i + 1], colour, skin);
        }
    }

    void quad(sf::VertexArray& into, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c,
              sf::Vector2f d, sf::Color colour, const Skin& skin)
    {
        shape(into, { a, b, c, d }, colour, skin);
    }

    void disc(sf::VertexArray& into, sf::Vector2f centre, float radius, sf::Color colour,
              const Skin& skin, int steps = 18)
    {
        for (int i = 0; i < steps; ++i)
        {
            const sf::Angle a0 = sf::degrees(360.0f * static_cast<float>(i) / static_cast<float>(steps));
            const sf::Angle a1 = sf::degrees(360.0f * static_cast<float>(i + 1) / static_cast<float>(steps));

            vert(into, centre, colour, skin);
            vert(into, centre + sf::Vector2f(radius, a0), colour, skin);
            vert(into, centre + sf::Vector2f(radius, a1), colour, skin);
        }
    }

    // A tapering limb: a quad from a wide root to a narrow tip.
    void sweep(sf::VertexArray& into, sf::Vector2f root, sf::Vector2f tip, float rootHalf,
               float tipHalf, sf::Color colour, const Skin& skin)
    {
        const sf::Vector2f along = tip - root;
        if (along.lengthSquared() < 0.0001f)
            return;

        const sf::Vector2f side = across(along).normalized();

        quad(into, root - side * rootHalf, tip - side * tipHalf,
             tip + side * tipHalf, root + side * rootHalf, colour, skin);
    }

    // What the second phase looks like. Every creature keeps its own shape and
    // its own anatomy - it is the same animal - but the colour of it goes from
    // whatever it was to whatever is underneath, and on all six of them what is
    // underneath is raw.
    //
    // One function rather than six, because the player has to be able to read
    // "that is the thing on its second wind" at a glance whichever one it is.
    constexpr sf::Color Raw{ 196, 62, 58 };
    constexpr sf::Color RawLit{ 255, 156, 120 };

    sf::Color blend(sf::Color from, sf::Color to, float t);

    sf::Color shed(sf::Color base, float amount, float towards = 0.72f)
    {
        return blend(base, sf::Color(Raw.r, Raw.g, Raw.b, base.a),
                     std::clamp(amount, 0.0f, 1.0f) * towards);
    }

    // Two colours down a length, alpha included, because most of what the
    // creatures below are made of fades out towards its own tip.
    sf::Color blend(sf::Color from, sf::Color to, float t)
    {
        const auto mix = [t](std::uint8_t a, std::uint8_t b) {
            return static_cast<std::uint8_t>(static_cast<float>(a)
                                             + (static_cast<float>(b) - static_cast<float>(a))
                                                   * std::clamp(t, 0.0f, 1.0f));
        };

        return sf::Color(mix(from.r, to.r), mix(from.g, to.g), mix(from.b, to.b),
                         mix(from.a, to.a));
    }

    // A limb that curves. Walks out from a root, turning by `curl` degrees each
    // step, narrowing from one half-width to the other and fading from one
    // colour to the other.
    //
    // It is the workhorse of everything that is not a fish: an oral arm, a
    // chaeta, a rib, a crinoid arm, a tube worm's plume and an octopus arm are
    // all this function with different numbers in it.
    void ribbon(sf::VertexArray& into, sf::Vector2f root, sf::Angle dir, float length,
                float halfRoot, float halfTip, float curl, int steps, sf::Color from,
                sf::Color to, const Skin& skin)
    {
        if (steps < 1 || length <= 0.0f)
            return;

        const float stride = length / static_cast<float>(steps);

        sf::Vector2f at = root;
        sf::Angle angle = dir;

        for (int i = 0; i < steps; ++i)
        {
            const float t0 = static_cast<float>(i) / static_cast<float>(steps);
            const float t1 = static_cast<float>(i + 1) / static_cast<float>(steps);

            const sf::Vector2f side0 = across(unit(angle));
            const sf::Vector2f next = at + unit(angle) * stride;

            angle += sf::degrees(curl);
            const sf::Vector2f side1 = across(unit(angle));

            const float w0 = halfRoot + (halfTip - halfRoot) * t0;
            const float w1 = halfRoot + (halfTip - halfRoot) * t1;

            const sf::Color c0 = blend(from, to, t0);
            const sf::Color c1 = blend(from, to, t1);

            vert(into, at - side0 * w0, c0, skin);
            vert(into, next - side1 * w1, c1, skin);
            vert(into, next + side1 * w1, c1, skin);

            vert(into, at - side0 * w0, c0, skin);
            vert(into, next + side1 * w1, c1, skin);
            vert(into, at + side0 * w0, c0, skin);

            at = next;
        }
    }


    // ------------------------------------------------------------------
    //  The body
    //
    //  Drawn tail first so the head sits on top of its own neck, and as a run
    //  of quads between consecutive links rather than as a string of discs:
    //  discs read as beads however close together they are, and the whole
    //  point of a spine is that it is one animal.
    // ------------------------------------------------------------------
    void drawSpine(sf::VertexArray& into, const bossart::Body& body, float grow, sf::Color colour,
                   sf::Vector2f offset, float tile)
    {
        const std::size_t count = body.links.size();
        if (count < 2)
            return;

        // How far down the animal each link is, so the sheet runs along it
        // rather than being stamped onto each piece from the same corner.
        float walked = 0.0f;
        std::vector<float> reach(count, 0.0f);
        for (std::size_t i = 1; i < count; ++i)
        {
            walked += (body.links[i - 1].at - body.links[i].at).length();
            reach[i] = walked;
        }

        for (std::size_t i = count - 1; i > 0; --i)
        {
            const bossart::Link& back = body.links[i];
            const bossart::Link& front = body.links[i - 1];

            const Skin skin{ front.at + offset, unit(front.angle), reach[i - 1], tile };

            const sf::Vector2f sideBack = across(unit(back.angle));
            const sf::Vector2f sideFront = across(unit(front.angle));

            const float wBack = back.girth + grow;
            const float wFront = front.girth + grow;

            quad(into,
                 back.at + offset - sideBack * wBack,
                 front.at + offset - sideFront * wFront,
                 front.at + offset + sideFront * wFront,
                 back.at + offset + sideBack * wBack,
                 colour, skin);

            // A joint cap, so a hard turn does not open a notch on the outside
            // of the bend where the two quads pull apart.
            disc(into, front.at + offset, wFront, colour, skin, 12);
        }

        // And the tail comes to a point rather than stopping on a flat end.
        const bossart::Link& tail = body.links[count - 1];
        const sf::Vector2f back = -unit(tail.angle);
        const sf::Vector2f side = across(back);
        const float width = tail.girth + grow;
        const Skin skin{ tail.at + offset, unit(tail.angle), reach[count - 1], tile };

        shape(into, { tail.at + offset + back * (width * 2.6f),
                      tail.at + offset + side * width,
                      tail.at + offset - side * width },
              colour, skin);
    }

    // Bands across the body. A long animal drawn as one smooth tube has no
    // length to read: the bands are what make it obvious the thing is eight
    // segments and not a painted sausage, and they slide over one another as
    // the spine bends.
    void drawBands(sf::VertexArray& into, const bossart::Body& body, sf::Vector2f offset,
                   float tile, float alpha)
    {
        for (std::size_t i = 1; i + 1 < body.links.size(); ++i)
        {
            const bossart::Link& link = body.links[i];
            const sf::Vector2f out = unit(link.angle);
            const sf::Vector2f side = across(out);
            const sf::Vector2f at = link.at + offset;
            const Skin skin{ at, out, 0.0f, tile };

            const float squeeze = 0.72f + 0.10f * std::sin(body.breath * Tau
                                                           + static_cast<float>(i));

            sweep(into, at - side * (link.girth * squeeze), at + side * (link.girth * squeeze),
                  link.girth * 0.20f, link.girth * 0.20f, fade(Band, alpha * 0.34f), skin);
        }
    }

    void drawFluke(sf::VertexArray& into, const bossart::Body& body, sf::Color colour,
                   sf::Vector2f offset, float tile)
    {
        if (body.links.size() < 2)
            return;

        const bossart::Link& tail = body.links.back();
        const sf::Vector2f back = -unit(tail.angle);
        const sf::Vector2f side = across(back);

        // The fluke lags the tail: it is being dragged through water, so it is
        // always a beat behind whatever the body just did.
        const float lag = body.beat * 0.55f;
        const sf::Vector2f root = tail.at + offset + back * (tail.girth * 2.2f);
        const Skin skin{ root, unit(tail.angle), 0.0f, tile };

        for (const float flank : { -1.0f, 1.0f })
        {
            const sf::Vector2f tip = root + back * (tail.girth * 2.4f)
                                     + side * (flank * tail.girth * 2.8f + lag * tail.girth);

            shape(into, { root,
                          root + side * (flank * tail.girth * 0.5f),
                          tip,
                          tip - back * (tail.girth * 1.5f) },
                  colour, skin);
        }
    }

    // ------------------------------------------------------------------
    //  The head
    //
    //  Built around the first link and pointing along it. The jaw is two
    //  mandibles hinged at the back of the skull: shut they close over the
    //  teeth, open they swing apart, and the tell is what opens them - so the
    //  wind-up is visible on the animal itself instead of on a ring drawn
    //  underneath it.
    // ------------------------------------------------------------------
    void drawHead(sf::VertexArray& into, const bossart::Body& body, sf::Color hide,
                  sf::Vector2f offset, float tile, bool detail)
    {
        if (body.links.empty())
            return;

        const bossart::Link& head = body.links.front();
        const sf::Vector2f nose = unit(head.angle);
        const sf::Vector2f side = across(nose);
        const sf::Vector2f at = head.at + offset;
        const Skin skin{ at, nose, 0.0f, tile };

        const float length = head.girth * 4.3f;
        const float wide = head.girth * 1.28f;

        // The skull, wider at the brow than at the snout, tapering into the
        // neck rather than stopping on a flat cut.
        shape(into, { at + nose * (length * 0.15f),
                      at + nose * (length * 0.72f) - side * (wide * 0.52f),
                      at + nose * (length * 0.30f) - side * wide,
                      at - nose * (length * 0.40f) - side * (wide * 0.86f),
                      at - nose * (length * 0.80f) - side * (wide * 0.44f),
                      at - nose * (length * 0.80f) + side * (wide * 0.44f),
                      at - nose * (length * 0.40f) + side * (wide * 0.86f),
                      at + nose * (length * 0.30f) + side * wide,
                      at + nose * (length * 0.72f) + side * (wide * 0.52f) },
              hide, skin);

        if (!detail)
            return;

        const float swing = 26.0f * body.jaw;
        const sf::Vector2f hinge = at - nose * (length * 0.18f);

        for (const float flank : { -1.0f, 1.0f })
        {
            const sf::Vector2f out = unit(head.angle + sf::degrees(flank * swing));
            const sf::Vector2f rim = across(out);
            const sf::Vector2f root = hinge + side * (flank * wide * 0.66f);

            // One mandible.
            shape(into, { root - rim * (flank * wide * 0.30f),
                          root + out * (length * 0.92f) - rim * (flank * wide * 0.10f),
                          root + out * (length * 0.98f) + rim * (flank * wide * 0.14f),
                          root + rim * (flank * wide * 0.34f) },
                  hide, skin);

            // The muscle that shuts it, bunched at the hinge and swelling as
            // the jaw comes open. It is the one part of a bite that is doing
            // any work before the bite happens.
            disc(into, root - out * (wide * 0.12f), wide * (0.32f + 0.16f * body.jaw),
                 Ridge, skin, 12);
        }

        // Brow ridges over the sockets, and folds where the skull meets the
        // neck. Both exist to break up the one big flat panel the head would
        // otherwise be from directly above.
        for (const float flank : { -1.0f, 1.0f })
        {
            shape(into, { at - nose * (length * 0.44f) + side * (flank * wide * 0.30f),
                          at - nose * (length * 0.10f) + side * (flank * wide * 0.42f),
                          at - nose * (length * 0.06f) + side * (flank * wide * 0.86f),
                          at - nose * (length * 0.46f) + side * (flank * wide * 0.80f) },
                  Ridge, skin);
        }

        for (int i = 0; i < 3; ++i)
        {
            const float t = static_cast<float>(i) / 2.0f;
            const sf::Vector2f on = at - nose * (length * (0.52f + 0.14f * t));
            const float flare = 1.0f + body.breath * 0.7f;

            sweep(into, on - side * (wide * 0.52f), on + side * (wide * 0.52f),
                  wide * 0.08f * flare, wide * 0.08f * flare, Ridge, skin);
        }
    }

    // Everything with no scale on it: the inside of the mouth, the teeth, the
    // eyes and the organ. Drawn flat in a second pass with no sheet, because a
    // tooth with hide grain on it is a tooth made of skin.
    void drawSoft(sf::VertexArray& into, const bossart::Body& body, sf::Vector2f offset)
    {
        if (body.links.empty())
            return;

        const Skin none{};
        const bossart::Link& head = body.links.front();
        const sf::Vector2f nose = unit(head.angle);
        const sf::Vector2f side = across(nose);
        const sf::Vector2f at = head.at + offset;

        const float length = head.girth * 4.3f;
        const float wide = head.girth * 1.28f;
        const float swing = 26.0f * body.jaw;
        const sf::Vector2f hinge = at - nose * (length * 0.18f);

        // The mouth, and only while the jaws are apart: a shut mouth has to be
        // a shut mouth and not a red line down the snout.
        if (body.jaw > 0.02f)
        {
            const sf::Vector2f open0 = unit(head.angle + sf::degrees(swing));
            const sf::Vector2f open1 = unit(head.angle - sf::degrees(swing));

            shape(into, { hinge - nose * (wide * 0.30f),
                          hinge + open0 * (length * 0.84f),
                          hinge + nose * (length * 0.78f),
                          hinge + open1 * (length * 0.84f) },
                  Gum, none);

            // And the throat behind it, which is the part that means there is
            // an inside to this thing.
            disc(into, hinge + nose * (wide * 0.10f), wide * (0.26f + 0.24f * body.jaw),
                 Throat, none, 14);
        }

        for (const float flank : { -1.0f, 1.0f })
        {
            const sf::Vector2f out = unit(head.angle + sf::degrees(flank * swing));
            const sf::Vector2f rim = across(out);
            const sf::Vector2f root = hinge + side * (flank * wide * 0.66f);

            for (int i = 0; i < 6; ++i)
            {
                const float t = static_cast<float>(i) / 5.0f;
                const float size = wide * (0.16f - 0.08f * t);

                // Rooted on the inner edge of the mandible and pointing across
                // the gap at the other one, which is where a tooth goes.
                const sf::Vector2f base = root + out * (length * (0.18f + 0.70f * t))
                                          - rim * (flank * wide * 0.16f);

                shape(into, { base - out * size,
                              base - rim * (flank * size * 2.2f),
                              base + out * size },
                      Tooth, none);
            }
        }
    }

    void drawEye(sf::VertexArray& into, const bossart::Limb& eye, const bossart::Body& body,
                 sf::Vector2f offset)
    {
        const Skin none{};
        const sf::Vector2f at = eye.at + offset;

        // The socket is a hitbox the size of a thing you can reasonably be
        // asked to hit; the eye in it is an eye. They are not the same number.
        const float radius = eye.size * 0.44f;

        if (!eye.alive)
        {
            // Put out. A socket and nothing in it.
            disc(into, at, radius, Ink, none, 14);
            disc(into, at, radius * 0.62f, Ridge, none, 12);
            return;
        }

        disc(into, at, radius * 1.16f, Ink, none, 16);
        disc(into, at, radius, flashed(Iris, eye.flash), none, 16);

        // The pupil narrows to a slit as the thing gets angrier, which is the
        // cheapest tell in the world and reads from across the basin.
        const float slit = 1.0f - 0.45f * body.rage;
        const sf::Vector2f look = body.look * (radius * 0.34f);

        disc(into, at + look, radius * 0.44f * slit, Pupil, none, 12);
        disc(into, at + look - body.look * (radius * 0.10f), radius * 0.18f,
             sf::Color(255, 255, 255, 150), none, 10);

        // A lid, mostly down, and up on the wind-up. It is the last thing a
        // lot of players are going to see.
        const float lid = (1.0f - body.tell) * 0.32f;
        if (lid > 0.01f)
        {
            const sf::Vector2f up = across(body.look);

            quad(into, at - body.look * radius - up * radius,
                 at + body.look * radius - up * radius,
                 at + body.look * radius - up * (radius * (1.0f - 2.0f * lid)),
                 at - body.look * radius - up * (radius * (1.0f - 2.0f * lid)),
                 Ridge, none);
        }
    }

    void drawFin(sf::VertexArray& into, const bossart::Limb& fin, const bossart::Body& body,
                 sf::Color hide, sf::Vector2f offset, float tile)
    {
        if (!fin.alive)
            return;

        // Each flank beats on the opposite half of the stroke, so the animal
        // rows rather than flapping like a bird.
        const float stroke = body.beat * fin.side;
        const sf::Angle angle = fin.angle + sf::degrees(stroke * 18.0f);

        const sf::Vector2f out = unit(angle);
        const sf::Vector2f side = across(out);
        const sf::Vector2f root = fin.at + offset;
        const float span = fin.size * 1.9f;
        const Skin skin{ root, out, 0.0f, tile };

        shape(into, { root - side * (fin.size * 0.5f),
                      root + out * span - side * (fin.size * 0.9f),
                      root + out * (span * 1.15f) + side * (fin.size * 0.2f),
                      root + out * (span * 0.4f) + side * (fin.size * 1.0f),
                      root + side * (fin.size * 0.6f) },
              flashed(hide, fin.flash), skin);

        // Rays, so it is a fin and not a leaf.
        for (int i = 1; i < 4; ++i)
        {
            const float t = static_cast<float>(i) / 4.0f;
            sweep(into, root, root + out * (span * (0.4f + 0.6f * t))
                                  + side * (fin.size * (1.0f - 1.9f * t)),
                  fin.size * 0.10f, fin.size * 0.04f, Ridge, skin);
        }
    }

    void drawPlate(sf::VertexArray& into, const bossart::Limb& plate, sf::Vector2f offset,
                   float tile)
    {
        if (!plate.alive)
            return;

        const sf::Vector2f out = unit(plate.angle);
        const sf::Vector2f side = across(out);
        const sf::Vector2f at = plate.at + offset;
        const float size = plate.size;
        const Skin skin{ at, out, 0.0f, tile };

        shape(into, { at + out * (size * 1.30f),
                      at + out * (size * 0.55f) - side * (size * 1.05f),
                      at - out * (size * 0.85f) - side * (size * 0.95f),
                      at - out * (size * 1.25f),
                      at - out * (size * 0.85f) + side * (size * 0.95f),
                      at + out * (size * 0.55f) + side * (size * 1.05f) },
              flashed(Ridge, plate.flash), skin);

        // Three raised bands across it, so it reads as bolted on rather than
        // as a darker patch of the same animal.
        for (int i = 0; i < 3; ++i)
        {
            const float t = -0.5f + 0.5f * static_cast<float>(i);
            const sf::Vector2f on = at + out * (size * t * 1.1f);

            sweep(into, on - side * (size * 0.86f), on + side * (size * 0.86f),
                  size * 0.16f, size * 0.16f, flashed(Lit, plate.flash * 0.6f), skin);
        }
    }

    void drawOrgan(sf::VertexArray& into, const bossart::Limb& organ, const bossart::Body& body,
                   sf::Vector2f offset)
    {
        if (!organ.alive || !organ.shown)
            return;

        const Skin none{};
        const sf::Vector2f at = organ.at + offset;

        // It beats. Faster the closer the thing is to dead, which is both the
        // only soft place on the animal and a health bar you do not have to
        // read.
        const float beat = 1.0f + 0.16f * std::sin(body.breath * Tau * 2.0f)
                           + 0.10f * body.rage;

        disc(into, at, organ.size * 1.25f * beat, sf::Color(Organ.r, Organ.g, Organ.b, 90), none, 16);
        disc(into, at, organ.size * beat, flashed(Organ, organ.flash), none, 16);
        disc(into, at - body.look * (organ.size * 0.2f), organ.size * 0.52f * beat,
             flashed(OrganLit, organ.flash), none, 14);

        // Vessels running back into the body.
        for (int i = 0; i < 5; ++i)
        {
            const sf::Angle a = sf::degrees(360.0f * static_cast<float>(i) / 5.0f
                                            + body.breath * 40.0f);
            sweep(into, at, at + unit(a) * (organ.size * 1.5f), organ.size * 0.14f,
                  organ.size * 0.03f, sf::Color(Organ.r, Organ.g, Organ.b, 150), none);
        }
    }

    // The animal from the first pass: a long-jawed marine reptile, which is
    // what a pliosaur was and what this thing is pretending to be.
    void drawLeviathan(sf::RenderTarget& target, const bossart::Body& body,
                       const sf::Texture& hide)
    {
        if (body.links.empty() || body.alpha <= 0.01f)
            return;

        const float tile = static_cast<float>(hide.getSize().x);

        sf::VertexArray skinned(sf::PrimitiveType::Triangles);
        sf::VertexArray plain(sf::PrimitiveType::Triangles);

        // The light in this game always comes from up and left, because that is
        // where the relief pass puts it on every other texture in the basin.
        const sf::Vector2f up{ -3.0f, -4.0f };
        const sf::Vector2f down{ 3.0f, 4.0f };

        const float alpha = std::clamp(body.alpha, 0.0f, 1.0f);
        const auto tint = [alpha](sf::Color colour) { return fade(colour, alpha); };

        // A creature that has just been hit rocks back along its own length,
        // which is the difference between a shot landing and a shot being
        // subtracted from a number.
        const sf::Vector2f kick = -unit(body.links.front().angle) * (body.flinch * 7.0f);

        // Fins first: they hang off the flanks and the body sits over them.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Fin)
                drawFin(skinned, limb, body, tint(shed(Hide, body.shed)), kick, tile);
        }

        // Then the animal, three times: the shadow it casts on itself, the
        // hide, and the light down its back.
        drawSpine(skinned, body, 3.0f, tint(Ink), kick + down, tile);
        drawHead(skinned, body, tint(Ink), kick + down, tile, false);

        drawSpine(skinned, body, 0.0f, tint(shed(Hide, body.shed)), kick, tile);

        drawSpine(skinned, body, -6.0f, tint(shed(Belly, body.shed, 0.5f)), kick + up, tile);
        drawFluke(skinned, body, tint(shed(Hide, body.shed)), kick, tile);
        drawBands(skinned, body, kick, tile, alpha);

        // The dorsal ridge, which is also where the flex shows: the plates
        // slide over one another as the spine bends.
        for (std::size_t i = 1; i + 1 < body.links.size(); ++i)
        {
            const bossart::Link& link = body.links[i];
            const sf::Vector2f out = unit(link.angle);
            const sf::Vector2f side = across(out);
            const sf::Vector2f at = link.at + kick;
            const Skin skin{ at, out, 0.0f, tile };

            for (const float flank : { -1.0f, 1.0f })
            {
                shape(skinned,
                      { at - out * (link.girth * 0.40f) + side * (flank * link.girth * 0.40f),
                        at + out * (link.girth * 0.10f) + side * (flank * link.girth * 0.78f),
                        at + out * (link.girth * 0.40f) + side * (flank * link.girth * 0.40f) },
                      tint(shed(Ridge, body.shed, 0.55f)), skin);
            }
        }

        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Armour)
                drawPlate(skinned, limb, kick, tile);
        }

        drawHead(skinned, body, tint(shed(Hide, body.shed)), kick, tile, true);

        // And the wind-up, on the animal rather than under it: the whole thing
        // goes pale for the half second before it commits.
        if (body.tell > 0.0f)
        {
            const float glow = body.tell * body.tell * 0.16f;

            drawSpine(skinned, body, 1.0f, fade(sf::Color(255, 140, 90), glow * alpha), kick, tile);
            drawHead(skinned, body, fade(sf::Color(255, 160, 110), glow * alpha), kick, tile, false);
        }

        // Then everything with no scale on it.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Organ)
                drawOrgan(plain, limb, body, kick);
        }

        drawSoft(plain, body, kick);

        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Eye)
                drawEye(plain, limb, body, kick);
        }

        sf::RenderStates states;
        states.texture = &hide;

        target.draw(skinned, states);
        target.draw(plain);
    }

    // ==================================================================
    //  THE BLOOM SOVEREIGN  -  a lion's mane
    //
    //  The one creature in the trench you can see through. A scyphozoan is a
    //  bell of jelly with the plumbing printed on it: eight radial canals off
    //  a central stomach, a scalloped margin with a sense organ in every
    //  notch, a curtain of fine stinging threads hanging off that margin, and
    //  four frilled oral arms underneath doing the actual eating.
    //
    //  It is drawn at about two thirds alpha, because that is the whole
    //  character of the animal: the water and whatever is behind it show
    //  through, and the gonads under the bell are visible from outside.
    // ==================================================================
    namespace medusa
    {
        constexpr sf::Color Jelly{ 118, 158, 186, 132 };
        constexpr sf::Color Rim{ 168, 206, 224, 168 };
        constexpr sf::Color Canal{ 196, 148, 172, 190 };
        constexpr sf::Color Arm{ 176, 132, 158, 150 };
        constexpr sf::Color Thread{ 150, 176, 198, 96 };
        constexpr sf::Color Gonad{ 226, 142, 158, 210 };
        constexpr sf::Color Stomach{ 122, 66, 90, 190 };

        // The margin, as a run of lobes rather than a circle. A bell rim with
        // no notches in it is a plate; the notches are where the eyes are.
        void bell(sf::VertexArray& into, const bossart::Body& body, sf::Vector2f at,
                  float radius, const Skin& skin, float alpha, float torn = 0.0f)
        {
            // Second wind: the jelly clouds over and the plumbing goes dark.
            const sf::Color jelly = shed(Jelly, torn, 0.55f);
            const sf::Color rim = shed(Rim, torn, 0.55f);
            const sf::Color canal = shed(Canal, torn);

            constexpr int Lobes = 16;

            for (int i = 0; i < Lobes; ++i)
            {
                const float t0 = static_cast<float>(i) / Lobes;
                const float t1 = static_cast<float>(i + 1) / Lobes;

                const sf::Angle a0 = sf::degrees(360.0f * t0);
                const sf::Angle a1 = sf::degrees(360.0f * t1);
                const sf::Angle mid = sf::degrees(360.0f * (t0 + t1) * 0.5f);

                // Each lobe bulges out between two notches, and the whole rim
                // breathes with the clench.
                const float notch = radius * (0.90f - 0.05f * body.pulse);
                const float belly = radius * (1.02f + 0.06f * std::sin(body.breath * Tau
                                                                      + static_cast<float>(i)));

                shape(into, { at,
                              at + sf::Vector2f(notch, a0),
                              at + sf::Vector2f(belly, mid),
                              at + sf::Vector2f(notch, a1) },
                      fade(jelly, alpha), skin);

                // The thickened rim itself, and the sense organ in the notch.
                sweep(into, at + sf::Vector2f(notch, a0), at + sf::Vector2f(belly, mid),
                      radius * 0.05f, radius * 0.05f, fade(rim, alpha), skin);
                sweep(into, at + sf::Vector2f(belly, mid), at + sf::Vector2f(notch, a1),
                      radius * 0.05f, radius * 0.05f, fade(rim, alpha), skin);

                disc(into, at + sf::Vector2f(notch, a0), radius * 0.035f,
                     fade(canal, alpha), skin, 7);
            }
        }

        // Eight canals off the stomach, which is the plumbing of the animal and
        // the only structure a jelly has to show.
        void canals(sf::VertexArray& into, const bossart::Body& body, sf::Vector2f at,
                    float radius, const Skin& skin, float alpha)
        {
            for (int i = 0; i < 8; ++i)
            {
                const sf::Angle a = sf::degrees(45.0f * static_cast<float>(i)
                                                + std::sin(body.breath * Tau) * 3.0f);

                sweep(into, at + unit(a) * (radius * 0.16f), at + unit(a) * (radius * 0.94f),
                      radius * 0.055f, radius * 0.018f, fade(Canal, alpha), skin);
            }
        }

        // The curtain. A lion's mane trails threads many times its own width,
        // and they are the reason nobody swims past one.
        void threads(sf::VertexArray& into, const bossart::Body& body, sf::Vector2f at,
                     float radius, const Skin& skin, float alpha)
        {
            constexpr int Count = 44;

            for (int i = 0; i < Count; ++i)
            {
                const float t = static_cast<float>(i) / Count;
                const sf::Angle a = sf::degrees(360.0f * t);

                // Every thread drifts on its own beat, so the curtain moves
                // like water rather than like a skirt.
                const float drift = std::sin(body.breath * Tau * 0.7f
                                             + static_cast<float>(i) * 1.7f);
                const float reach = radius * (1.3f + 1.3f * t) * (0.9f + 0.2f * drift);

                // Fine, many, and curling off on their own. A lion's mane
                // trails a curtain, not a set of spokes.
                const sf::Color thread = shed(Thread, body.shed);

                ribbon(into, at + unit(a) * (radius * 0.92f), a, reach * (1.0f + body.shed * 0.4f),
                       radius * 0.016f, 0.0f, drift * 9.0f, 7,
                       fade(thread, alpha * 0.8f), fade(thread, 0.0f), skin);
            }
        }
    }

    void drawMedusaArm(sf::VertexArray& into, const bossart::Limb& limb,
                       const bossart::Body& body, const Skin& skin, float alpha)
    {
        if (!limb.alive)
            return;

        // An oral arm is a frill, not a rope: a ribbon with a ruffled edge that
        // gathers and lets go as the bell pumps.
        const float wave = std::sin(body.breath * Tau + limb.angle.asRadians());
        const float reach = limb.size * (4.0f + 1.4f * body.pulse);

        ribbon(into, limb.at, limb.angle, reach, limb.size * 0.9f, limb.size * 0.24f,
               wave * 5.0f, 7, fade(flashed(medusa::Arm, limb.flash), alpha),
               fade(medusa::Arm, alpha * 0.35f), skin);

        // The ruffle, as a row of lobes down one side and then the other.
        for (int i = 1; i < 6; ++i)
        {
            const float t = static_cast<float>(i) / 6.0f;
            const sf::Angle a = limb.angle + sf::degrees(wave * 5.0f * static_cast<float>(i));
            const sf::Vector2f on = limb.at + unit(a) * (reach * t);
            const sf::Vector2f side = across(unit(a));
            const float flank = (i % 2) == 0 ? 1.0f : -1.0f;

            disc(into, on + side * (flank * limb.size * 0.5f * (1.0f - t)),
                 limb.size * 0.36f * (1.0f - t * 0.7f),
                 fade(flashed(medusa::Arm, limb.flash), alpha * 0.7f), skin, 9);
        }
    }

    void drawMedusa(sf::RenderTarget& target, const bossart::Body& body, const sf::Texture& hide)
    {
        const float tile = static_cast<float>(hide.getSize().x);
        const float alpha = std::clamp(body.alpha, 0.0f, 1.0f);

        sf::VertexArray skinned(sf::PrimitiveType::Triangles);
        sf::VertexArray plain(sf::PrimitiveType::Triangles);

        const bossart::Link& head = body.links.front();
        const sf::Vector2f at = head.at;
        const Skin skin{ at, unit(head.angle), 0.0f, tile };

        // Clenching pulls the bell in and pushes it taller, which is how a
        // jelly actually swims: it is squeezing water out from under itself.
        const float radius = head.girth * (3.1f - 0.30f * body.pulse);

        medusa::threads(plain, body, at, radius, skin, alpha);

        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Tentacle)
                drawMedusaArm(plain, limb, body, skin, alpha);
        }

        // Into the plain pass on purpose. The sheet is a scale sheet, and a
        // jelly has no scales: it is one sheet of mesogloea with the plumbing
        // showing through it, and putting reptile grain on that would be the
        // only wrong thing on the animal.
        medusa::bell(plain, body, at, radius, Skin{}, alpha, body.shed);

        // The stomach and the gonads, seen through the jelly rather than
        // sitting on top of it - which is the whole reason this one is drawn
        // see-through in the first place.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role != PartRole::Organ || !limb.alive)
                continue;

            const float beat = 1.0f + 0.14f * std::sin(body.breath * Tau * 2.0f);

            disc(plain, at, limb.size * 1.3f * beat, fade(medusa::Stomach, alpha * 0.8f),
                 Skin{}, 16);

            for (int i = 0; i < 4; ++i)
            {
                const sf::Angle a = sf::degrees(90.0f * static_cast<float>(i) + 45.0f);
                const sf::Vector2f on = at + unit(a) * (limb.size * 0.9f);

                // Horseshoes, the way a scyphozoan's gonads actually sit.
                for (int k = 0; k < 5; ++k)
                {
                    const sf::Angle b = a + sf::degrees(-40.0f + 20.0f * static_cast<float>(k));
                    disc(plain, on + unit(b) * (limb.size * 0.44f), limb.size * 0.17f * beat,
                         fade(flashed(medusa::Gonad, limb.flash),
                              alpha * (limb.shown ? 0.95f : 0.45f)),
                         Skin{}, 9);
                }
            }
        }

        medusa::canals(plain, body, at, radius, skin, alpha);

        if (body.tell > 0.0f)
        {
            medusa::bell(plain, body, at, radius * 1.02f,
                         Skin{}, body.tell * body.tell * 0.22f * alpha);
        }

        sf::RenderStates states;
        states.texture = &hide;

        target.draw(skinned, states);
        target.draw(plain);
    }

    // ==================================================================
    //  THE TRENCH WORM  -  a bobbit worm
    //
    //  A eunicid polychaete, which is a real animal that really does live in a
    //  burrow with only its head out and really does cut things in half. The
    //  body is a run of chitin rings; every ring carries a pair of parapodia
    //  with a bundle of bristles in each; the head is a prostomium with five
    //  antennae on it and a set of scissor jaws that open sideways.
    //
    //  The chitin is iridescent, which on a flat palette means the tint slides
    //  down the body rather than staying one colour.
    // ==================================================================
    namespace worm
    {
        constexpr sf::Color Chitin{ 96, 78, 54 };
        constexpr sf::Color Sheen{ 138, 128, 78 };
        constexpr sf::Color Under{ 54, 42, 30 };
        constexpr sf::Color Bristle{ 196, 178, 130 };
        constexpr sf::Color Jaw{ 36, 26, 20 };
        constexpr sf::Color Gill{ 186, 74, 78 };
        constexpr sf::Color GillLit{ 232, 128, 116 };

        // Iridescence, as a slide from one tone to the other down the length.
        sf::Color along(float t, float alpha, float split = 0.0f)
        {
            const auto mix = [t](std::uint8_t a, std::uint8_t b) {
                return static_cast<std::uint8_t>(static_cast<float>(a)
                                                 + (static_cast<float>(b) - static_cast<float>(a))
                                                       * std::clamp(t, 0.0f, 1.0f));
            };

            // Second wind: the chitin splits and what is under it is not
            // chitin. The sheen goes first, because that was only ever a
            // surface.
            return fade(shed(sf::Color(mix(Sheen.r, Chitin.r), mix(Sheen.g, Chitin.g),
                                       mix(Sheen.b, Chitin.b)), split), alpha);
        }
    }

    void drawWorm(sf::RenderTarget& target, const bossart::Body& body, const sf::Texture& hide)
    {
        const float tile = static_cast<float>(hide.getSize().x);
        const float alpha = std::clamp(body.alpha, 0.0f, 1.0f);
        const std::size_t count = body.links.size();

        sf::VertexArray skinned(sf::PrimitiveType::Triangles);
        sf::VertexArray plain(sf::PrimitiveType::Triangles);

        const sf::Vector2f kick = -unit(body.links.front().angle) * (body.flinch * 6.0f);

        // Parapodia first: they stick out sideways off every ring and the ring
        // has to sit over its own roots.
        for (std::size_t i = 1; i < count; ++i)
        {
            const bossart::Link& link = body.links[i];
            const sf::Vector2f out = unit(link.angle);
            const sf::Vector2f side = across(out);
            const sf::Vector2f at = link.at + kick;
            const Skin skin{ at, out, 0.0f, tile };
            const float t = static_cast<float>(i) / static_cast<float>(count);

            // They row. Each ring is a beat behind the one in front, which is
            // exactly what a wave of parapodia looks like going down a worm.
            const float row = std::sin(body.beat * Tau - static_cast<float>(i) * 0.8f);

            for (const float flank : { -1.0f, 1.0f })
            {
                const sf::Vector2f root = at + side * (flank * link.girth * 0.75f);
                const sf::Angle lean = link.angle + sf::degrees(flank * (74.0f + row * 16.0f));

                // The lobe.
                ribbon(skinned, root, lean, link.girth * 1.25f, link.girth * 0.44f,
                       link.girth * 0.20f, 0.0f, 2, worm::along(t, alpha, body.shed),
                       fade(worm::Under, alpha), skin);

                // And the bundle of chaetae out of the end of it, which is the
                // part that actually holds a burrow wall.
                for (int k = -1; k <= 1; ++k)
                {
                    const sf::Angle spread = lean + sf::degrees(static_cast<float>(k) * 15.0f);

                    sweep(plain, root + unit(lean) * (link.girth * 1.1f),
                          root + unit(spread) * (link.girth * 2.0f),
                          link.girth * 0.07f, link.girth * 0.02f,
                          fade(worm::Bristle, alpha * 0.75f), Skin{});
                }
            }
        }

        // Then the rings, tail first so each overlaps the one behind it the way
        // a worm's segments actually do.
        for (std::size_t i = count; i-- > 1;)
        {
            const bossart::Link& link = body.links[i];
            const bossart::Link& front = body.links[i - 1];
            const sf::Vector2f out = unit(link.angle);
            const sf::Vector2f side = across(out);
            const sf::Vector2f sideFront = across(unit(front.angle));
            const sf::Vector2f at = link.at + kick;
            const Skin skin{ at, out, 0.0f, tile };
            const float t = static_cast<float>(i) / static_cast<float>(count);

            quad(skinned, at - side * link.girth,
                 front.at + kick - sideFront * front.girth,
                 front.at + kick + sideFront * front.girth,
                 at + side * link.girth, worm::along(t, alpha, body.shed), skin);

            disc(skinned, at, link.girth, worm::along(t, alpha, body.shed), skin, 12);

            // The annulus: the raised band at the front of every segment, which
            // is what makes a worm read as a worm and not as a tube.
            sweep(skinned, at - side * (link.girth * 0.92f), at + side * (link.girth * 0.92f),
                  link.girth * 0.16f, link.girth * 0.16f, fade(worm::Under, alpha * 0.8f), skin);
        }

        // The head: a prostomium, five antennae, and the jaws.
        {
            const bossart::Link& head = body.links.front();
            const sf::Vector2f nose = unit(head.angle);
            const sf::Vector2f side = across(nose);
            const sf::Vector2f at = head.at + kick;
            const Skin skin{ at, nose, 0.0f, tile };
            const float wide = head.girth;

            shape(skinned, { at + nose * (wide * 1.5f),
                             at + nose * (wide * 0.5f) - side * (wide * 0.95f),
                             at - nose * (wide * 1.1f) - side * (wide * 1.05f),
                             at - nose * (wide * 1.1f) + side * (wide * 1.05f),
                             at + nose * (wide * 0.5f) + side * (wide * 0.95f) },
                  worm::along(0.0f, alpha, body.shed), skin);

            // Five antennae, which is how many a eunicid has, waving.
            for (int i = -2; i <= 2; ++i)
            {
                const float t = static_cast<float>(i);
                const sf::Angle a = head.angle + sf::degrees(t * 26.0f
                                                             + std::sin(body.breath * Tau + t) * 9.0f);

                ribbon(plain, at + nose * (wide * 1.2f), a, wide * (2.3f - std::abs(t) * 0.35f),
                       wide * 0.13f, wide * 0.02f, std::sin(body.breath * Tau + t) * 5.0f, 5,
                       worm::along(0.1f, alpha, body.shed), fade(worm::Bristle, alpha * 0.5f), Skin{});
            }

            // The jaws. Five hooked pieces that swing out sideways, which is
            // the one thing everybody has seen a bobbit worm do.
            const float swing = 34.0f * body.jaw;

            for (const float flank : { -1.0f, 1.0f })
            {
                for (int k = 0; k < 2; ++k)
                {
                    const float lean = flank * (swing + static_cast<float>(k) * 16.0f);
                    const sf::Angle a = head.angle + sf::degrees(lean);
                    const sf::Vector2f root = at + nose * (wide * 0.7f)
                                              + side * (flank * wide * 0.4f);

                    ribbon(plain, root, a, wide * (1.9f - 0.3f * static_cast<float>(k)),
                           wide * 0.22f, wide * 0.02f, -flank * 13.0f, 4,
                           fade(worm::Jaw, alpha), fade(worm::Jaw, alpha), Skin{});
                }
            }
        }

        // The gills. On a worm these are the soft tufts along the back, and on
        // this one they are the only thing worth shooting.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role != PartRole::Organ || !limb.alive || !limb.shown)
                continue;

            const sf::Vector2f at = limb.at + kick;
            const float beat = 1.0f + 0.18f * std::sin(body.breath * Tau * 2.2f);

            for (int i = 0; i < 9; ++i)
            {
                const sf::Angle a = sf::degrees(40.0f * static_cast<float>(i)
                                                + std::sin(body.breath * Tau) * 12.0f);

                ribbon(plain, at, a, limb.size * 1.5f * beat, limb.size * 0.20f, 0.0f,
                       9.0f, 4, fade(flashed(worm::GillLit, limb.flash), alpha),
                       fade(worm::Gill, alpha * 0.4f), Skin{});
            }

            disc(plain, at, limb.size * 0.45f, fade(flashed(worm::Gill, limb.flash), alpha),
                 Skin{}, 12);
        }

        sf::RenderStates states;
        states.texture = &hide;

        target.draw(skinned, states);
        target.draw(plain);
    }

    // ==================================================================
    //  THE HOLLOW  -  a whale fall
    //
    //  Not an animal any more. A whale fall is a skeleton on the floor with a
    //  community living in it: bare vertebrae, a rib cage, a cranium picked
    //  clean, and something pale and soft that has moved into the space where
    //  the organs used to be. The crown is a crinoid - a stalked feather star,
    //  which is a real animal that really does sit on bone and really does
    //  hold its arms up into the current.
    //
    //  Everything here is drawn with a gap down the middle of it, because that
    //  is what a skeleton is: a thing you can see through.
    // ==================================================================
    namespace hollow
    {
        constexpr sf::Color Bone{ 172, 178, 160 };
        constexpr sf::Color BoneLo{ 104, 112, 104 };
        constexpr sf::Color BoneHi{ 214, 218, 200 };
        constexpr sf::Color Marrow{ 62, 66, 60 };
        constexpr sf::Color Soft{ 206, 190, 200 };
        constexpr sf::Color SoftLit{ 236, 224, 230 };
        constexpr sf::Color Lamp{ 196, 244, 226 };
        constexpr sf::Color LampLo{ 66, 156, 140 };

        // One vertebra: a centrum with a neural spine up the back and a pair of
        // transverse processes out of the sides. Three shapes, and it reads as
        // bone from right across the basin.
        void vertebra(sf::VertexArray& into, sf::Vector2f at, sf::Angle angle, float size,
                      const Skin& skin, float alpha, float flash)
        {
            const sf::Vector2f out = unit(angle);
            const sf::Vector2f side = across(out);

            for (const float flank : { -1.0f, 1.0f })
            {
                sweep(into, at, at + side * (flank * size * 1.5f) + out * (size * 0.2f),
                      size * 0.30f, size * 0.14f, fade(flashed(BoneLo, flash), alpha), skin);
            }

            shape(into, { at + out * (size * 0.9f),
                          at + side * (size * 0.62f),
                          at - out * (size * 0.9f),
                          at - side * (size * 0.62f) },
                  fade(flashed(Bone, flash), alpha), skin);

            disc(into, at, size * 0.26f, fade(Marrow, alpha), skin, 10);
        }
    }

    void drawHollow(sf::RenderTarget& target, const bossart::Body& body, const sf::Texture& hide)
    {
        const float tile = static_cast<float>(hide.getSize().x);
        const float alpha = std::clamp(body.alpha, 0.0f, 1.0f);
        const std::size_t count = body.links.size();

        sf::VertexArray skinned(sf::PrimitiveType::Triangles);
        sf::VertexArray plain(sf::PrimitiveType::Triangles);

        const sf::Vector2f kick = -unit(body.links.front().angle) * (body.flinch * 6.0f);

        // The column, tail first. There is no flesh between the vertebrae, so
        // the cord is drawn as a thin dark line and the bone sits on it.
        for (std::size_t i = count; i-- > 1;)
        {
            const bossart::Link& link = body.links[i];
            const bossart::Link& front = body.links[i - 1];

            sweep(skinned, link.at + kick, front.at + kick, link.girth * 0.22f,
                  front.girth * 0.22f, fade(hollow::Marrow, alpha),
                  Skin{ link.at + kick, unit(link.angle), 0.0f, tile });
        }

        for (std::size_t i = count; i-- > 1;)
        {
            const bossart::Link& link = body.links[i];
            const Skin skin{ link.at + kick, unit(link.angle), 0.0f, tile };

            hollow::vertebra(skinned, link.at + kick, link.angle, link.girth, skin, alpha, 0.0f);
        }

        // The rib cage, on whichever links carry armour. Ribs curve forward and
        // meet nothing at the front, because the sternum went a long time ago.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role != PartRole::Armour || !limb.alive)
                continue;

            const sf::Vector2f at = limb.at + kick;
            const sf::Vector2f out = unit(limb.angle);
            const sf::Vector2f side = across(out);
            const Skin skin{ at, out, 0.0f, tile };

            for (int i = -2; i <= 2; ++i)
            {
                const float t = static_cast<float>(i) / 2.0f;
                const sf::Vector2f root = at + out * (t * limb.size * 0.9f);

                for (const float flank : { -1.0f, 1.0f })
                {
                    ribbon(skinned, root, limb.angle + sf::degrees(flank * 96.0f),
                           limb.size * (1.5f - std::abs(t) * 0.4f),
                           limb.size * 0.16f, limb.size * 0.07f, -flank * 15.0f, 5,
                           fade(flashed(shed(hollow::Bone, body.shed, 0.45f), limb.flash), alpha),
                           fade(flashed(hollow::BoneLo, limb.flash), alpha), skin);
                }
            }
        }

        // Whatever moved in. A whale fall grows a mat of pale soft things over
        // the bone, and this one has grown something with a pulse.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role != PartRole::Organ || !limb.alive || !limb.shown)
                continue;

            const sf::Vector2f at = limb.at + kick;
            const float beat = 1.0f + 0.14f * std::sin(body.breath * Tau * 2.0f)
                               + 0.08f * body.rage;

            disc(plain, at, limb.size * 1.2f * beat,
                 fade(flashed(hollow::Soft, limb.flash), alpha * 0.55f), Skin{}, 16);
            disc(plain, at, limb.size * 0.78f * beat,
                 fade(flashed(hollow::SoftLit, limb.flash), alpha), Skin{}, 16);

            // Lobes, so it is a colony rather than a ball.
            for (int i = 0; i < 6; ++i)
            {
                const sf::Angle a = sf::degrees(60.0f * static_cast<float>(i)
                                                + body.breath * 30.0f);
                disc(plain, at + unit(a) * (limb.size * 0.62f), limb.size * 0.34f * beat,
                     fade(flashed(hollow::Soft, limb.flash), alpha * 0.8f), Skin{}, 10);
            }
        }

        // The cranium, and the crown of feather-star arms growing out of it.
        {
            const bossart::Link& head = body.links.front();
            const sf::Vector2f nose = unit(head.angle);
            const sf::Vector2f side = across(nose);
            const sf::Vector2f at = head.at + kick;
            const Skin skin{ at, nose, 0.0f, tile };
            const float wide = head.girth;

            // The skull: a long rostrum and a braincase, with the sockets cut
            // right through it.
            shape(skinned, { at + nose * (wide * 2.1f),
                             at + nose * (wide * 0.6f) - side * (wide * 0.52f),
                             at - nose * (wide * 0.9f) - side * (wide * 1.05f),
                             at - nose * (wide * 1.4f),
                             at - nose * (wide * 0.9f) + side * (wide * 1.05f),
                             at + nose * (wide * 0.6f) + side * (wide * 0.52f) },
                  fade(shed(hollow::Bone, body.shed, 0.45f), alpha), skin);

            for (const float flank : { -1.0f, 1.0f })
            {
                disc(plain, at - nose * (wide * 0.45f) + side * (flank * wide * 0.56f),
                     wide * 0.34f, fade(hollow::Marrow, alpha), Skin{}, 12);
            }

            // The jaw, hinged the way a whale's is: one long piece each side
            // that swings out rather than down.
            const float swing = 20.0f * body.jaw;

            for (const float flank : { -1.0f, 1.0f })
            {
                ribbon(skinned, at - nose * (wide * 0.2f) + side * (flank * wide * 0.5f),
                       head.angle + sf::degrees(flank * swing), wide * 2.4f,
                       wide * 0.24f, wide * 0.10f, -flank * 6.0f, 4,
                       fade(hollow::BoneHi, alpha), fade(shed(hollow::Bone, body.shed, 0.45f), alpha), skin);
            }
        }

        // The lamps. On a crinoid the arms are feeding arms held into the
        // current; on this one they hold the lights it aims with.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role != PartRole::Eye)
                continue;

            const sf::Vector2f at = limb.at + kick;
            const sf::Vector2f root = body.links.front().at + kick;
            const sf::Angle out = (at - root).lengthSquared() > 1.0f
                                      ? (at - root).angle()
                                      : limb.angle;

            // The arm, with pinnules down both sides of it the way a feather
            // star's arms actually branch.
            ribbon(skinned, root, out, (at - root).length() * 1.15f,
                   limb.size * 0.34f, limb.size * 0.16f,
                   std::sin(body.breath * Tau + limb.angle.asRadians()) * 4.0f, 5,
                   fade(hollow::Bone, alpha), fade(hollow::BoneLo, alpha),
                   Skin{ root, unit(body.links.front().angle), 0.0f, tile });

            for (int i = 1; i < 5; ++i)
            {
                const float t = static_cast<float>(i) / 5.0f;
                const sf::Vector2f on = root + unit(out) * ((at - root).length() * t);

                for (const float flank : { -1.0f, 1.0f })
                {
                    sweep(plain, on, on + unit(out + sf::degrees(flank * 62.0f)) * (limb.size * 0.7f),
                          limb.size * 0.09f, limb.size * 0.02f, fade(hollow::BoneLo, alpha), Skin{});
                }
            }

            if (!limb.alive)
            {
                disc(plain, at, limb.size * 0.5f, fade(hollow::Marrow, alpha), Skin{}, 10);
                continue;
            }

            // The light itself: a bloom, a core, and a pinpoint, which is how
            // every other lit thing in this game is drawn.
            const float flare = 0.8f + 0.2f * std::sin(body.breath * Tau * 3.0f)
                                + 0.4f * body.tell;

            // Second wind: whatever is burning in them stops being green.
            const sf::Color glow = blend(hollow::Lamp, sf::Color(255, 226, 196), body.shed);
            const sf::Color halo = blend(hollow::LampLo, sf::Color(220, 110, 70), body.shed);

            disc(plain, at, limb.size * (1.15f + 0.5f * body.shed) * flare,
                 fade(halo, alpha * 0.26f), Skin{}, 14);
            disc(plain, at, limb.size * 0.50f * flare,
                 fade(flashed(glow, limb.flash), alpha), Skin{}, 12);
            disc(plain, at, limb.size * 0.20f, fade(sf::Color::White, alpha * 0.9f), Skin{}, 8);
        }

        sf::RenderStates states;
        states.texture = &hide;

        target.draw(skinned, states);
        target.draw(plain);
    }

    // ==================================================================
    //  THE FURNACE  -  a vent chimney
    //
    //  A black smoker with a colony on it. The chimney is mineral, built out
    //  of what the water drops as it cools, so it is layered crust with the
    //  heat showing through the cracks. Growing off it are Riftia: white
    //  chitin tubes with a red branchial plume out of the top, which retracts
    //  into the tube the instant anything comes near.
    //
    //  Nothing on it is an animal shape. That is the point of it.
    // ==================================================================
    namespace furnace
    {
        constexpr sf::Color Crust{ 74 , 60, 54 };
        constexpr sf::Color CrustLo{ 40, 30, 28 };
        constexpr sf::Color CrustHi{ 116, 96, 84 };
        constexpr sf::Color Ember{ 236, 128, 54 };
        constexpr sf::Color EmberHi{ 255, 214, 138 };
        constexpr sf::Color Tube{ 214, 208, 194 };
        constexpr sf::Color TubeLo{ 140, 134, 122 };
        constexpr sf::Color Plume{ 198, 62, 66 };
        constexpr sf::Color PlumeHi{ 244, 116, 104 };
    }

    void drawFurnace(sf::RenderTarget& target, const bossart::Body& body, const sf::Texture& hide)
    {
        const float tile = static_cast<float>(hide.getSize().x);
        const float alpha = std::clamp(body.alpha, 0.0f, 1.0f);

        sf::VertexArray skinned(sf::PrimitiveType::Triangles);
        sf::VertexArray plain(sf::PrimitiveType::Triangles);

        const bossart::Link& head = body.links.front();
        const sf::Vector2f at = head.at;
        const Skin skin{ at, unit(head.angle), 0.0f, tile };
        const float radius = head.girth * 2.0f;

        // A field of chimneys, not one dome. Every link is its own stack, so
        // the body reads as something the floor grew over centuries rather
        // than as a head with limbs on it - which is what it was, and what the
        // creature two depths above it also was.
        //
        // Drawn furthest out first, so the near stacks overlap the far ones
        // and the pile has a front and a back.
        for (std::size_t link = body.links.size(); link-- > 0;)
        {
            const bossart::Link& course = body.links[link];
            const sf::Vector2f base = course.at;
            const float span = course.girth * 2.0f;

            // The mound: layered crust, each layer a ragged ring, because a
            // chimney is built a millimetre at a time out of what the water
            // drops.
            for (int layer = 0; layer < 4; ++layer)
            {
                const float t = static_cast<float>(layer) / 4.0f;
                const float size = span * (1.0f - t * 0.62f);

                std::vector<sf::Vector2f> rim;
                for (int i = 0; i < 15; ++i)
                {
                    const sf::Angle a = sf::degrees(360.0f * static_cast<float>(i) / 15.0f);

                    // Seeded off the link as well as the layer, so no two
                    // stacks in the field are the same lump of rock.
                    const float wobble =
                        0.88f + 0.16f * std::sin(a.asRadians() * 3.0f
                                                 + static_cast<float>(layer) * 2.1f
                                                 + static_cast<float>(link) * 1.7f);

                    rim.push_back(base + unit(a) * (size * wobble));
                }

                // Second wind: the crust does not come off, it lights up. What
                // is under a chimney is the thing the chimney was built by.
                const sf::Color stone = layer == 0   ? furnace::CrustLo
                                        : layer == 3 ? furnace::CrustHi
                                                     : furnace::Crust;

                shape(skinned, rim, fade(shed(stone, body.shed, 0.85f), alpha), skin);
            }

            // The outlying stacks get a mouth of their own, banked rather than
            // burning: the hub is the one that is actually open.
            if (link > 0)
            {
                const float heat = 0.18f + 0.14f * std::sin(body.breath * Tau
                                                            + static_cast<float>(link));

                disc(plain, base, span * 0.26f, fade(furnace::Ember, alpha * heat), Skin{}, 10);
            }
        }

        // Cracks, with the heat under them. They breathe with the thing.
        for (int i = 0; i < 8; ++i)
        {
            const sf::Angle a = sf::degrees(45.0f * static_cast<float>(i) + 12.0f);
            const float glow = 0.6f + 0.4f * std::sin(body.breath * Tau
                                                      + static_cast<float>(i));

            sweep(plain, at + unit(a) * (radius * 0.22f),
                  at + unit(a + sf::degrees(14.0f)) * (radius * 0.94f),
                  radius * 0.05f, radius * 0.012f,
                  fade(furnace::Ember, alpha * 0.55f * glow), Skin{});
        }

        // The throat at the top, which is the hole the whole thing is built
        // around and the only part of it that is soft.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role != PartRole::Organ || !limb.alive || !limb.shown)
                continue;

            const float shimmer = 1.0f + 0.16f * std::sin(body.breath * Tau * 3.0f);

            disc(plain, limb.at, limb.size * 1.5f * shimmer,
                 fade(furnace::Ember, alpha * 0.30f), Skin{}, 14);
            disc(plain, limb.at, limb.size * 0.86f * shimmer,
                 fade(flashed(furnace::Ember, limb.flash), alpha), Skin{}, 14);
            disc(plain, limb.at, limb.size * 0.40f * shimmer,
                 fade(flashed(furnace::EmberHi, limb.flash), alpha), Skin{}, 12);

            // The water going up out of it, which is the only thing on this
            // creature that moves on its own.
            for (int i = 0; i < 5; ++i)
            {
                const float t = std::fmod(body.breath + static_cast<float>(i) * 0.2f, 1.0f);
                const sf::Angle a = sf::degrees(72.0f * static_cast<float>(i));

                disc(plain, limb.at + unit(a) * (limb.size * (0.6f + 1.6f * t)),
                     limb.size * 0.22f * (1.0f - t),
                     fade(furnace::EmberHi, alpha * 0.4f * (1.0f - t)), Skin{}, 8);
            }
        }

        // And the colony. Every flue is a tube worm: a hard white tube with a
        // red plume out of the end, and the plume pulls in when the thing is
        // winding up to do something.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (!limb.alive)
                continue;
            if (limb.role != PartRole::Tentacle)
                continue;

            const sf::Vector2f out = unit(limb.angle);
            const sf::Vector2f root = limb.at - out * (limb.size * 0.6f);
            const Skin tubeSkin{ root, out, 0.0f, tile };

            // The tube, leaning a little and ridged the way they really are.
            const float lean = std::sin(body.breath * Tau + limb.angle.asRadians()) * 5.0f;

            ribbon(skinned, root, limb.angle + sf::degrees(lean), limb.size * 3.4f,
                   limb.size * 0.62f, limb.size * 0.40f, lean * 0.6f, 5,
                   fade(flashed(shed(furnace::Tube, body.shed), limb.flash), alpha),
                   fade(flashed(furnace::TubeLo, limb.flash), alpha), tubeSkin);

            // The plume: out when it is calm, pulled into the tube when it is
            // about to commit to something.
            const float outAmount = 1.0f - body.tell * 0.85f;
            const sf::Vector2f mouth = root + unit(limb.angle + sf::degrees(lean))
                                              * (limb.size * 3.2f);

            for (int i = -2; i <= 2; ++i)
            {
                const float spread = static_cast<float>(i) * 17.0f;
                const sf::Angle a = limb.angle + sf::degrees(lean + spread);

                ribbon(plain, mouth, a, limb.size * 1.9f * outAmount,
                       limb.size * 0.26f, limb.size * 0.06f, spread * 0.18f, 4,
                       fade(flashed(furnace::PlumeHi, limb.flash), alpha),
                       fade(furnace::Plume, alpha * 0.5f), Skin{});
            }
        }

        sf::RenderStates states;
        states.texture = &hide;

        target.draw(skinned, states);
        target.draw(plain);
    }

    // ==================================================================
    //  THE MOTHER  -  a brooding octopus
    //
    //  A giant Pacific octopus lays once, guards the eggs, and does not eat
    //  again. By the end she is translucent and falling apart and she will
    //  still come at anything that gets near the clutch, which is the entire
    //  fight this creature is.
    //
    //  Mantle, eyes with a horizontal slit, four arms with two rows of suckers
    //  down each, and the eggs themselves in strings under her.
    // ==================================================================
    namespace mother
    {
        constexpr sf::Color Skin_{ 168, 142, 138 };
        constexpr sf::Color SkinLo{ 104, 84, 86 };
        constexpr sf::Color SkinHi{ 214, 190, 182 };
        constexpr sf::Color Spot{ 128, 78, 88 };
        constexpr sf::Color Sucker{ 226, 206, 196 };
        constexpr sf::Color Egg{ 232, 224, 204 };
        constexpr sf::Color EggLit{ 255, 248, 230 };
        constexpr sf::Color Pupil_{ 14, 12, 10 };
        constexpr sf::Color Eye_{ 224, 208, 158 };
    }

    void drawMother(sf::RenderTarget& target, const bossart::Body& body, const sf::Texture& hide)
    {
        const float tile = static_cast<float>(hide.getSize().x);
        const float alpha = std::clamp(body.alpha, 0.0f, 1.0f);

        sf::VertexArray skinned(sf::PrimitiveType::Triangles);
        sf::VertexArray plain(sf::PrimitiveType::Triangles);

        const bossart::Link& head = body.links.front();
        const sf::Vector2f nose = unit(head.angle);
        const sf::Vector2f side = across(nose);
        const sf::Vector2f kick = -nose * (body.flinch * 7.0f);
        const sf::Vector2f at = head.at + kick;
        const Skin skin{ at, nose, 0.0f, tile };

        // A mantle is a bag, and it jets by squeezing that bag, so the clench
        // narrows it across and stretches it along.
        const float len = head.girth * (2.1f + 0.26f * body.pulse);
        const float wide = head.girth * (1.55f - 0.20f * body.pulse);

        // The arms go down first: they come out from under the mantle.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role != PartRole::Tentacle || !limb.alive)
                continue;

            const float curl = std::sin(body.beat * Tau + limb.angle.asRadians()) * 9.0f
                               + body.tell * 14.0f;
            // An arm comes out from under the mantle, not off the end of it.
            // The hitbox stays where the fight put it; the drawing starts back
            // under the body so the animal is one piece.
            const float reach = limb.size * 6.2f;
            const sf::Vector2f root = at + (limb.at - head.at) * 0.25f;
            const Skin armSkin{ root, unit(limb.angle), 0.0f, tile };

            ribbon(skinned, root, limb.angle, reach, limb.size * 1.05f, limb.size * 0.14f,
                   curl, 8, fade(flashed(shed(mother::Skin_, body.shed), limb.flash), alpha),
                   fade(flashed(shed(mother::SkinLo, body.shed), limb.flash), alpha), armSkin);

            // Two rows of suckers down the underside, shrinking to the tip.
            for (int i = 1; i < 9; ++i)
            {
                const float t = static_cast<float>(i) / 9.0f;
                const sf::Angle a = limb.angle + sf::degrees(curl * static_cast<float>(i) * 0.5f);
                const sf::Vector2f on = root + unit(a) * (reach * t);
                const sf::Vector2f rim = across(unit(a));
                const float size = limb.size * 0.26f * (1.0f - t * 0.7f);

                for (const float flank : { -1.0f, 1.0f })
                {
                    disc(plain, on + rim * (flank * limb.size * 0.42f * (1.0f - t * 0.6f)),
                         size, fade(mother::Sucker, alpha * 0.85f), Skin{}, 8);
                }
            }
        }

        // The clutch. Strings of eggs hung under her, which is the thing she is
        // standing over and the reason she will not leave.
        for (const bossart::Limb& limb : body.limbs)
        {
            if (limb.role != PartRole::Organ || !limb.alive || !limb.shown)
                continue;

            const sf::Vector2f on = limb.at + kick;

            for (int strand = 0; strand < 3; ++strand)
            {
                const sf::Angle a = sf::degrees(120.0f * static_cast<float>(strand)
                                                + std::sin(body.breath * Tau) * 8.0f);

                for (int i = 0; i < 5; ++i)
                {
                    const float t = static_cast<float>(i) / 5.0f;
                    const sf::Vector2f egg = on + unit(a) * (limb.size * (0.3f + 1.1f * t));
                    const float size = limb.size * 0.30f * (1.0f - t * 0.35f);

                    disc(plain, egg, size, fade(flashed(mother::Egg, limb.flash), alpha),
                         Skin{}, 9);
                    disc(plain, egg - unit(a) * (size * 0.25f), size * 0.45f,
                         fade(mother::EggLit, alpha * 0.8f), Skin{}, 7);
                }
            }
        }

        // The two halves, and the tissue that has not let go yet.
        //
        // This creature comes apart: BossBody::Split lays its links in two
        // arcs on one slow swing, half a turn out of phase, so one lobe is
        // never where the other is. Drawn before the mantle so the mantle -
        // which is the core, and the thing worth shooting - sits over the
        // strand between them.
        if (body.links.size() > 2)
        {
            const std::size_t rest = body.links.size() - 1;
            const std::size_t half = (rest + 1) / 2;

            sf::Vector2f lobe[2]{ {}, {} };
            float counted[2]{ 0.0f, 0.0f };

            for (std::size_t i = 1; i < body.links.size(); ++i)
            {
                const std::size_t which = (i - 1) >= half ? 1u : 0u;
                lobe[which] += body.links[i].at;
                counted[which] += 1.0f;
            }

            for (int which = 0; which < 2; ++which)
            {
                if (counted[which] > 0.0f)
                    lobe[which] /= counted[which];
                else
                    lobe[which] = at;
            }

            // The strand: thick at each lobe, pinched in the middle, and
            // thinner the further apart they have got. By the last act it is
            // barely there, which is the second phase without a colour change.
            const sf::Vector2f span = lobe[1] - lobe[0];
            const float apart = span.length();

            if (apart > 1.0f)
            {
                const sf::Vector2f along = span / apart;
                const sf::Vector2f edge = across(along);
                const float pinch = head.girth * 0.55f
                                    / (1.0f + apart / std::max(1.0f, head.girth * 4.0f));

                for (int i = 0; i < 8; ++i)
                {
                    const float t0 = static_cast<float>(i) / 8.0f;
                    const float t1 = static_cast<float>(i + 1) / 8.0f;

                    const auto waist = [&](float t) {
                        // Fat at both ends, narrow in the middle, and sagging
                        // across itself so it reads as tissue under tension
                        // rather than as a drawn line.
                        const float bulge = 0.35f + 0.65f * std::abs(t - 0.5f) * 2.0f;
                        return pinch * bulge;
                    };

                    const sf::Vector2f a0 = lobe[0] + along * (apart * t0)
                                            + edge * (std::sin(t0 * 3.14159265f) * pinch * 0.4f);
                    const sf::Vector2f a1 = lobe[0] + along * (apart * t1)
                                            + edge * (std::sin(t1 * 3.14159265f) * pinch * 0.4f);

                    quad(skinned, a0 + edge * waist(t0), a0 - edge * waist(t0),
                         a1 - edge * waist(t1), a1 + edge * waist(t1),
                         fade(shed(mother::SkinLo, body.shed), alpha * 0.9f), skin);
                }
            }

            // And a lobe of mantle over each end of it, so the halves are
            // bodies rather than the ends of a rope.
            for (int which = 0; which < 2; ++which)
            {
                const sf::Vector2f face = apart > 1.0f
                                              ? (which == 0 ? -span / apart : span / apart)
                                              : nose;
                const sf::Vector2f rim = across(face);
                const float reach = head.girth * 1.15f;
                const float across_ = head.girth * 0.92f;

                shape(skinned, { lobe[which] + face * (reach * 0.70f),
                                 lobe[which] + face * (reach * 0.10f) - rim * across_,
                                 lobe[which] - face * (reach * 0.72f) - rim * (across_ * 0.55f),
                                 lobe[which] - face * (reach * 0.72f) + rim * (across_ * 0.55f),
                                 lobe[which] + face * (reach * 0.10f) + rim * across_ },
                      fade(shed(mother::Skin_, body.shed), alpha), skin);

                disc(plain, lobe[which] + face * (reach * 0.26f), head.girth * 0.22f,
                     fade(mother::Spot, alpha * (0.4f + 0.5f * body.tell)), Skin{}, 9);
            }
        }

        // The mantle.
        shape(skinned, { at + nose * (len * 0.55f),
                         at + nose * (len * 0.10f) - side * (wide * 0.92f),
                         at - nose * (len * 0.55f) - side * (wide * 0.78f),
                         at - nose * (len * 1.00f) - side * (wide * 0.26f),
                         at - nose * (len * 1.00f) + side * (wide * 0.26f),
                         at - nose * (len * 0.55f) + side * (wide * 0.78f),
                         at + nose * (len * 0.10f) + side * (wide * 0.92f) },
              fade(shed(mother::Skin_, body.shed), alpha), skin);

        // Papillae: the little raised bumps an octopus puts up to match rock.
        // They come and go, because that is exactly what they do.
        for (int i = 0; i < 11; ++i)
        {
            const float t = static_cast<float>(i) / 11.0f;
            const sf::Angle a = sf::degrees(360.0f * t * 3.0f);
            const float raise = 0.5f + 0.5f * std::sin(body.breath * Tau * 0.6f
                                                       + static_cast<float>(i) * 1.3f);

            disc(skinned, at + nose * (std::cos(a.asRadians()) * len * 0.42f)
                             + side * (std::sin(a.asRadians()) * wide * 0.52f),
                 head.girth * 0.14f * (0.5f + raise),
                 fade(mother::SkinHi, alpha * 0.55f), skin, 8);
        }

        // Chromatophores. They flush dark when she is about to move, which is
        // the one tell an octopus actually gives.
        {
            const float flush = 0.25f + 0.65f * body.tell + 0.2f * body.rage;

            for (int i = 0; i < 14; ++i)
            {
                const float t = static_cast<float>(i) / 14.0f;
                const sf::Angle a = sf::degrees(360.0f * t * 5.0f + 40.0f);

                disc(plain, at + nose * (std::sin(a.asRadians()) * len * 0.48f)
                               + side * (std::cos(a.asRadians()) * wide * 0.58f),
                     head.girth * 0.16f * flush,
                     fade(mother::Spot, alpha * flush), Skin{}, 7);
            }
        }

        // The eyes, on top of the head where an octopus keeps them, with the
        // horizontal slit that stays level however the animal is turned. It is
        // the single most recognisable thing about the animal and it is also
        // how you can tell she is watching you.
        for (const float flank : { -1.0f, 1.0f })
        {
            const sf::Vector2f eye = at + nose * (len * 0.14f) + side * (flank * wide * 0.66f);
            const float size = head.girth * 0.34f;

            disc(plain, eye, size * 1.12f, fade(mother::SkinLo, alpha), Skin{}, 12);
            disc(plain, eye, size, fade(mother::Eye_, alpha), Skin{}, 12);

            // The slit lies along the look direction, and it is a bar rather
            // than a dot, which is what makes it read as an octopus and not as
            // a fish.
            const sf::Vector2f look = body.look;

            sweep(plain, eye - look * (size * 0.74f), eye + look * (size * 0.74f),
                  size * 0.26f, size * 0.26f, fade(mother::Pupil_, alpha), Skin{});
            disc(plain, eye + look * (size * 0.3f) - across(look) * (size * 0.3f),
                 size * 0.16f, fade(sf::Color(255, 255, 255, 140), alpha), Skin{}, 7);
        }

        // The siphon, under the mantle edge, pointing wherever she is about to
        // go. It is how she steers and it is the only warning of a charge.
        {
            const sf::Vector2f mouth = at - nose * (len * 0.30f);

            ribbon(skinned, mouth, (-body.look).angle(), head.girth * (0.9f + 0.5f * body.pulse),
                   head.girth * 0.36f, head.girth * 0.22f, 0.0f, 3,
                   fade(mother::SkinLo, alpha), fade(shed(mother::Skin_, body.shed), alpha), skin);
        }

        sf::RenderStates states;
        states.texture = &hide;

        target.draw(skinned, states);
        target.draw(plain);
    }
}

// ----------------------------------------------------------------------
//  The small ones
//
//  Same vocabulary, a twentieth of the size. What changes at twenty pixels
//  is that detail stops paying: what reads is the outline, where the widest
//  point is, and whether the thing has a front. So each of these is a handful
//  of shapes chosen to give it an unmistakable silhouette and honest anatomy -
//  a head end, a tail end, fins where fins take hold, gills behind the head -
//  and almost nothing else.
// ----------------------------------------------------------------------
namespace
{
    // The creature's own frame. Everything below is written as "so far forward,
    // so far out to the side", which is the only way anatomy stays anatomy when
    // the animal turns round.
    struct Frame
    {
        sf::Vector2f at;
        sf::Vector2f fwd{ 1.0f, 0.0f };
        sf::Vector2f side{ 0.0f, 1.0f };
        float        along = 1.0f;
        float        across = 1.0f;

        sf::Vector2f p(float x, float y) const
        {
            return at + fwd * (x * along) + side * (y * across);
        }
    };

    sf::Color wash(sf::Color base, sf::Color tint)
    {
        const auto mix = [](std::uint8_t a, std::uint8_t b) {
            return static_cast<std::uint8_t>(static_cast<int>(a) * static_cast<int>(b) / 255);
        };

        return sf::Color(mix(base.r, tint.r), mix(base.g, tint.g), mix(base.b, tint.b),
                         mix(base.a, tint.a));
    }

    // A body sampled along its own length rather than listed corner by corner.
    // Hand-written outlines came out as six-sided facets - a creature made of
    // flats reads as a gem, not an animal - and every one of these is convex,
    // so the fan in shape() still holds.
    //
    // fullAt is where along the length it is widest: forward of a half for
    // anything that swims nose first, which is almost everything.
    void spindle(sf::VertexArray& into, const Frame& F, float nose, float tail, float wide,
                 float fullAt, sf::Color colour, const Skin& skin, int steps = 13)
    {
        std::vector<sf::Vector2f> pts;
        pts.reserve(static_cast<std::size_t>(steps) * 2 + 2);

        const auto halfAt = [&](float t) {
            const float u = (t < fullAt) ? t / std::max(0.01f, fullAt)
                                         : (1.0f - t) / std::max(0.01f, 1.0f - fullAt);
            return wide * std::sqrt(std::clamp(u, 0.0f, 1.0f));
        };

        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            pts.push_back(F.p(nose + (tail - nose) * t, -halfAt(t)));
        }

        for (int i = steps; i >= 0; --i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            pts.push_back(F.p(nose + (tail - nose) * t, halfAt(t)));
        }

        shape(into, pts, colour, skin);
    }

    // The same idea for anything built round rather than along: a smooth
    // ellipse in the creature's own frame.
    void round(sf::VertexArray& into, const Frame& F, float cx, float ax, float by,
               sf::Color colour, const Skin& skin, int steps = 20)
    {
        std::vector<sf::Vector2f> pts;
        pts.reserve(static_cast<std::size_t>(steps));

        for (int i = 0; i < steps; ++i)
        {
            const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(steps);
            pts.push_back(F.p(cx + ax * std::cos(a), by * std::sin(a)));
        }

        shape(into, pts, colour, skin);
    }

    // An eye that is a socket with something in it rather than a dot, because a
    // dot on a blob is exactly what all of these used to be.
    void look(sf::VertexArray& into, sf::Vector2f at, float radius, sf::Color iris,
              const Skin& skin)
    {
        disc(into, at, radius, sf::Color(8, 10, 14, iris.a), skin, 10);
        disc(into, at, radius * 0.55f, iris, skin, 8);
    }

    // Gill slits: three short strokes behind the head, angled back. The single
    // cheapest mark that says "this end is the front".
    void gills(sf::VertexArray& into, const Frame& F, float x, float y, float length,
               sf::Color colour, const Skin& skin)
    {
        for (int i = 0; i < 3; ++i)
        {
            const float step = x - static_cast<float>(i) * length * 0.55f;

            for (const float s : { -1.0f, 1.0f })
            {
                sweep(into, F.p(step, s * y), F.p(step - length * 0.6f, s * (y * 0.72f)),
                      length * 0.22f, length * 0.16f, colour, skin);
            }
        }
    }

    // A paired fin, one each side, beating on the creature's own clock.
    void pairedFin(sf::VertexArray& into, const Frame& F, float x, float y, float reach,
                   float sweepBack, float beat, sf::Color root, sf::Color tip, const Skin& skin)
    {
        for (const float s : { -1.0f, 1.0f })
        {
            const float lift = 1.0f + 0.30f * beat * s;

            ribbon(into, F.p(x, s * y),
                   (F.fwd * -sweepBack + F.side * (s * reach)).angle(),
                   reach * lift, reach * 0.34f, reach * 0.10f, s * 7.0f, 3, root, tip, skin);
        }
    }

    // The tail. Two lobes off a wrist, which is what makes it a tail rather
    // than a spike.
    void fluke(sf::VertexArray& into, const Frame& F, float x, float span, float reach,
               float beat, sf::Color root, sf::Color tip, const Skin& skin)
    {
        const sf::Vector2f wrist = F.p(x, beat * span * 0.35f);

        for (const float s : { -1.0f, 1.0f })
        {
            sweep(into, wrist, wrist + (F.fwd * -reach + F.side * (s * span)),
                  span * 0.34f, span * 0.14f, blend(root, tip, 0.5f), skin);
        }

        sweep(into, F.p(x + reach * 0.5f, 0.0f), wrist, span * 0.42f, span * 0.24f, root, skin);
    }

    // ---- 1. Lurker ---------------------------------------------------
    // The yardstick, so it is the plainest honest fish in the game: a deep
    // body at its widest just behind the head, a real jaw, gills, pectorals
    // and a forked tail. Everything else is a departure from this.
    void drawLurker(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                    const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;
        const float gape = pose.jaw * R * 0.34f;

        fluke(into, F, -R * 0.72f, R * 0.46f, R * 0.52f, pose.beat, fin, fade(fin, 0.4f), skin);
        pairedFin(into, F, R * 0.10f, R * 0.44f, R * 0.52f, 0.5f, pose.beat, fin,
                  fade(fin, 0.35f), skin);

        // The body. Widest a third back, tapering to a wrist, which is the
        // profile of most things that swim.
        spindle(into, F, R * 1.05f, -R * 0.88f, R * 0.62f, 0.34f, hide, skin);

        // A pale underside. Every animal in open water has one.
        shape(into,
              { F.p(R * 0.92f, 0.0f), F.p(R * 0.30f, R * 0.30f), F.p(-R * 0.50f, R * 0.22f),
                F.p(-R * 0.70f, 0.0f) },
              belly, skin);

        // The lower jaw drops away from the head rather than the whole face
        // opening, so a shut mouth is still a mouth.
        shape(into,
              { F.p(R * 1.05f, 0.0f), F.p(R * 0.50f, R * 0.34f + gape),
                F.p(R * 0.86f, R * 0.10f + gape) },
              belly, skin);

        gills(into, F, R * 0.44f, R * 0.46f, R * 0.26f, fade(sf::Color(10, 14, 18), 200), skin);
        look(into, F.p(R * 0.66f, -R * 0.24f), R * 0.15f, sf::Color(226, 206, 140), skin);
    }

    // ---- 2. Lancer ---------------------------------------------------
    // Built for one straight line and nothing else: a long rigid body, a
    // rostrum out front, fins swept hard back like a thing that cannot turn.
    void drawLancer(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                    const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;

        fluke(into, F, -R * 0.90f, R * 0.40f, R * 0.46f, pose.beat, fin, fade(fin, 0.35f), skin);

        // Swept back hard. On this one the fins say "committed" before it moves.
        pairedFin(into, F, -R * 0.10f, R * 0.30f, R * 0.46f, 1.5f, pose.beat * 0.4f, fin,
                  fade(fin, 0.3f), skin);

        spindle(into, F, R * 0.92f, -R * 1.00f, R * 0.34f, 0.30f, hide, skin);

        // The rostrum, and a dorsal ridge down the spine.
        sweep(into, F.p(R * 0.92f, 0.0f), F.p(R * 1.85f, 0.0f), R * 0.16f, R * 0.03f, hide, skin);
        sweep(into, F.p(R * 0.40f, 0.0f), F.p(-R * 0.45f, 0.0f), R * 0.10f, R * 0.05f,
              fade(fin, 170), skin);

        shape(into,
              { F.p(R * 0.80f, 0.0f), F.p(R * 0.20f, R * 0.20f), F.p(-R * 0.55f, R * 0.12f) },
              belly, skin);

        look(into, F.p(R * 0.62f, -R * 0.16f), R * 0.13f, sf::Color(238, 160, 96), skin);
    }

    // ---- 3. Weaver ---------------------------------------------------
    // It never closes and it never stops turning, so it is all wing: a broad
    // flat disc that undulates at the edges, with a whip behind it.
    void drawWeaver(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                    const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;
        const float flap = pose.beat * R * 0.22f;

        // The whip, trailing and stiff at the root.
        ribbon(into, F.p(-R * 0.70f, 0.0f), (-F.fwd).angle(), R * 1.5f, R * 0.12f, R * 0.02f,
               pose.beat * 5.0f, 4, fin, fade(fin, 0.0f), skin);

        // The wings: one smooth disc, wider across than it is long, with the
        // trailing edge lifting and falling. Listed corner by corner this came
        // out as a facet - a ray is all curve, and there is nothing else to it.
        {
            std::vector<sf::Vector2f> wing;
            wing.reserve(24);

            for (int i = 0; i < 24; ++i)
            {
                const float a = 6.2831853f * static_cast<float>(i) / 24.0f;
                const float c = std::cos(a);
                const float s = std::sin(a);

                // Blunt in front, drawn out behind, and the tips ride the beat.
                const float x = R * (c > 0.0f ? 0.80f * c : 0.66f * c);
                const float y = R * 1.18f * s;

                wing.push_back(F.p(x, y + flap * s * s * ((s > 0.0f) ? -1.0f : 1.0f)));
            }

            shape(into, wing, hide, skin);
        }

        // The pale disc of the underside, seen through it when it banks.
        round(into, F, -R * 0.05f, R * 0.44f, R * 0.52f, belly, skin, 16);

        for (const float s : { -1.0f, 1.0f })
            look(into, F.p(R * 0.48f, s * R * 0.20f), R * 0.12f, sf::Color(196, 214, 226), skin);
    }

    // ---- 4. Bloom ----------------------------------------------------
    // The one radial animal in the water: a bell that pumps, a fringe of short
    // tentacles all the way round it, and four long oral arms under it. It has
    // no front, which is exactly why it does not chase.
    void drawBloom(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                   const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;
        const float pump = 1.0f + pose.beat * 0.16f;

        // The long arms first, so the bell sits over their roots.
        for (int i = 0; i < 4; ++i)
        {
            const sf::Angle a = sf::degrees(45.0f + 90.0f * static_cast<float>(i));
            ribbon(into, F.at + sf::Vector2f(R * 0.30f, a), a, R * 1.25f, R * 0.13f, R * 0.02f,
                   6.0f * pose.beat, 4, fin, fade(fin, 0.0f), skin);
        }

        // The fringe: many short tentacles round the rim. A bell without one
        // is a dome, and a dome is not an animal.
        for (int i = 0; i < 16; ++i)
        {
            const sf::Angle a = sf::degrees(360.0f * static_cast<float>(i) / 16.0f);
            const float len = R * (0.34f + 0.16f * std::sin(static_cast<float>(i) * 2.1f));

            ribbon(into, F.at + sf::Vector2f(R * 0.80f * pump, a), a, len, R * 0.05f, R * 0.01f,
                   0.0f, 2, belly, fade(belly, 0.0f), skin);
        }

        disc(into, F.at, R * 0.82f * pump, hide, skin, 22);
        disc(into, F.at, R * 0.52f * pump, belly, skin, 18);

        // The gut, which is the only part of a jelly that is not water.
        disc(into, F.at, R * 0.22f * pump, wash(fin, sf::Color(255, 255, 255, 220)), skin, 12);
    }

    // ---- 5. Spawnling ------------------------------------------------
    // A larva: nearly all head, a yolk sac it has not finished, and a tail it
    // has not learned to use. It is the smallest thing down there and it has
    // to read at nine pixels, so it is three shapes.
    void drawSpawnling(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                       const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;

        ribbon(into, F.p(-R * 0.30f, 0.0f), (-F.fwd).angle(), R * 1.30f, R * 0.22f, R * 0.02f,
               pose.beat * 16.0f, 3, fin, fade(fin, 0.0f), skin);

        disc(into, F.p(R * 0.18f, 0.0f), R * 0.70f, hide, skin, 12);
        disc(into, F.p(-R * 0.02f, R * 0.22f), R * 0.34f, belly, skin, 10); // the yolk
        look(into, F.p(R * 0.48f, -R * 0.18f), R * 0.24f, sf::Color(242, 236, 214), skin);
    }

    // ---- 6. Brooder --------------------------------------------------
    // A body that is mostly brood pouch. The eggs are visible through it and
    // they are the thing you are meant to want to shoot, which is the whole
    // argument for killing the source rather than the swarm.
    void drawBrooder(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                     const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;
        const float swell = 1.0f + pose.beat * 0.07f;

        // Stubby limbs underneath, holding it off the floor.
        for (int i = 0; i < 6; ++i)
        {
            const float s = (i % 2 == 0) ? -1.0f : 1.0f;
            const float x = R * (0.42f - 0.34f * static_cast<float>(i / 2));

            ribbon(into, F.p(x, s * R * 0.52f),
                   (F.side * s - F.fwd * 0.25f).angle(), R * 0.46f, R * 0.11f, R * 0.04f,
                   -s * 18.0f, 3, fin, fade(fin, 0.5f), skin);
        }

        // The sac, deepest at the back where the brood is carried.
        shape(into,
              { F.p(R * 0.86f, 0.0f), F.p(R * 0.50f, -R * 0.44f), F.p(-R * 0.26f, -R * 0.78f),
                F.p(-R * 0.90f, -R * 0.40f), F.p(-R * 0.96f, 0.0f),
                F.p(-R * 0.90f, R * 0.40f), F.p(-R * 0.26f, R * 0.78f),
                F.p(R * 0.50f, R * 0.44f) },
              hide, skin);

        // What is inside it.
        for (int i = 0; i < 5; ++i)
        {
            const float x = -R * (0.14f + 0.16f * static_cast<float>(i % 3));
            const float y = R * (0.30f - 0.30f * static_cast<float>(i % 2)) * ((i < 3) ? -1.0f : 1.0f);

            disc(into, F.p(x, y), R * 0.15f * swell, belly, skin, 9);
        }

        gills(into, F, R * 0.50f, R * 0.40f, R * 0.20f, fade(sf::Color(12, 10, 8), 190), skin);
        look(into, F.p(R * 0.70f, -R * 0.18f), R * 0.12f, sf::Color(226, 172, 96), skin);
    }

    // ---- 7. Warden ---------------------------------------------------
    // Armour that only faces one way: a carapace over the front half with a
    // hard rim, a soft segmented abdomen behind it, and legs that only show
    // from the side it cannot defend.
    void drawWarden(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                    const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;

        // A tail spine, straight back. Short: at the length it first had, it
        // read as a lance on the wrong end of the animal.
        sweep(into, F.p(-R * 0.76f, 0.0f), F.p(-R * 1.22f, 0.0f), R * 0.09f, R * 0.02f, fin, skin);

        // Legs, and only behind the shield - that is the whole creature: the
        // side it cannot cover is the side that has anything on it. Angled
        // back, because forward-raked they read as tusks on the wrong end.
        for (int i = 0; i < 4; ++i)
        {
            const float s = (i % 2 == 0) ? -1.0f : 1.0f;
            const float x = -R * (0.34f + 0.30f * static_cast<float>(i / 2));

            ribbon(into, F.p(x, s * R * 0.34f), (F.side * s - F.fwd * 0.9f).angle(), R * 0.40f,
                   R * 0.07f, R * 0.03f, -s * 14.0f, 2, fin, fade(fin, 0.4f), skin);
        }

        // The soft half.
        spindle(into, F, R * 0.16f, -R * 0.86f, R * 0.52f, 0.18f, belly, skin, 9);

        // Segment joints, on the abdomen itself and no wider than it is - laid
        // across the whole creature they came out as two bars over the shield.
        for (int i = 0; i < 2; ++i)
        {
            const float x = -R * (0.42f + 0.20f * static_cast<float>(i));
            const float w = R * (0.40f - 0.10f * static_cast<float>(i));

            sweep(into, F.p(x, -w), F.p(x, w), R * 0.025f, R * 0.025f,
                  fade(sf::Color(10, 12, 14), 150), skin);
        }

        // And the shield: a broad curved carapace, blunt across the front,
        // because that is the face it wants you to meet. Curved rather than
        // cut from corners - the first version was a hexagon with spikes.
        {
            std::vector<sf::Vector2f> plate;
            plate.reserve(20);

            for (int i = 0; i < 20; ++i)
            {
                const float a = 6.2831853f * static_cast<float>(i) / 20.0f;
                const float c = std::cos(a);

                // Full and round forward, cut off square at the back where the
                // abdomen comes out from under it.
                plate.push_back(F.p(R * (0.36f + 0.66f * c), R * 0.88f * std::sin(a)
                                                                 * (c < -0.4f ? 0.74f : 1.0f)));
            }

            shape(into, plate, hide, skin);
        }

        // A raised rim round the front of it, which is what makes plate read as
        // plate. Followed round the curve rather than laid across it as two
        // bars, which is what made it look like a lance before.
        for (int i = -5; i < 5; ++i)
        {
            const float a0 = 6.2831853f * (static_cast<float>(i) / 20.0f);
            const float a1 = 6.2831853f * (static_cast<float>(i + 1) / 20.0f);

            sweep(into, F.p(R * (0.36f + 0.66f * std::cos(a0)), R * 0.88f * std::sin(a0)),
                  F.p(R * (0.36f + 0.66f * std::cos(a1)), R * 0.88f * std::sin(a1)),
                  R * 0.06f, R * 0.06f, fade(fin, 200), skin);
        }

        for (const float s : { -1.0f, 1.0f })
            look(into, F.p(R * 0.72f, s * R * 0.26f), R * 0.11f, sf::Color(212, 120, 74), skin);
    }

    // ---- 8. Revenant -------------------------------------------------
    // Built to be under the silt: flattened to nothing, both eyes moved onto
    // the top of the head, and a mouth that opens upward at whatever swims
    // over it.
    void drawRevenant(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                      const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;
        const float gape = pose.jaw * R * 0.40f;

        // A fin that runs the whole way round, the way a flatfish has.
        for (const float s : { -1.0f, 1.0f })
        {
            ribbon(into, F.p(R * 0.60f, s * R * 0.42f),
                   (F.side * s - F.fwd * 0.2f).angle(), R * 0.30f, R * 0.30f, R * 0.06f,
                   -s * 26.0f, 5, fade(fin, 170), fade(fin, 60), skin);
        }

        fluke(into, F, -R * 0.84f, R * 0.34f, R * 0.40f, pose.beat, fin, fade(fin, 0.3f), skin);

        spindle(into, F, R * 0.94f, -R * 0.90f, R * 0.68f, 0.42f, hide, skin, 13);

        // The mouth, across the front and opening wide.
        shape(into,
              { F.p(R * 0.94f, -R * 0.20f), F.p(R * 0.94f, R * 0.20f),
                F.p(R * 0.52f + gape, R * 0.34f), F.p(R * 0.52f + gape, -R * 0.34f) },
              sf::Color(14, 8, 10, hide.a), skin);

        // Both eyes on top and close together, which is the mark of something
        // that lies buried and looks up.
        for (const float s : { -1.0f, 1.0f })
            look(into, F.p(R * 0.20f, s * R * 0.20f), R * 0.15f, sf::Color(236, 226, 176), skin);
    }

    // ---- 9. Herald ---------------------------------------------------
    // Not one animal but a chain of them: a float at the front and a string of
    // repeated units behind it, every one lit. It never attacks because it does
    // not have anything to attack with.
    void drawHerald(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                    const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;

        // The trailing filaments.
        for (int i = 0; i < 3; ++i)
        {
            const float s = static_cast<float>(i - 1);
            ribbon(into, F.p(-R * 1.10f, s * R * 0.16f), (-F.fwd).angle(), R * 1.4f, R * 0.05f,
                   R * 0.01f, pose.beat * 8.0f * s, 4, fin, fade(fin, 0.0f), skin);
        }

        // The colony: six bodies down one stem, each smaller than the last.
        for (int i = 0; i < 6; ++i)
        {
            const float t = static_cast<float>(i) / 5.0f;
            const float x = R * (0.52f - 1.55f * t);
            const float r = R * (0.40f - 0.20f * t);
            const float wag = std::sin(pose.beat * 3.14159f + static_cast<float>(i)) * R * 0.10f;

            disc(into, F.p(x, wag), r, blend(hide, belly, t), skin, 10);
            disc(into, F.p(x, wag), r * 0.42f, fade(belly, 210), skin, 8);
        }

        // The float, which is the only part with any structure to it.
        disc(into, F.p(R * 0.86f, 0.0f), R * 0.52f, hide, skin, 14);
        disc(into, F.p(R * 0.92f, 0.0f), R * 0.26f, fade(belly, 230), skin, 10);
    }

    // ---- 10. Moth ----------------------------------------------------
    // Blind, so it has no eyes at all - it has antennae instead, and they are
    // the biggest thing on it. Broad soft wings, no hard edge anywhere.
    void drawMoth(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                  const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;
        const float beatUp = 1.0f + pose.beat * 0.45f;

        // Wings: two each side, the rear pair smaller, both beating. Sampled as
        // curves off a hinge - as four-cornered slabs they read as a windmill,
        // and the one thing a wing has that a slab has not is a round leading
        // edge running back to a point.
        const auto wing = [&](float hingeX, float hingeY, float spanIn, float spanOut,
                              float lead, float trail, float s, sf::Color colour) {
            std::vector<sf::Vector2f> pts;
            pts.reserve(11);

            for (int i = 0; i <= 10; ++i)
            {
                const float t = static_cast<float>(i) / 10.0f;
                const float a = t * 3.14159265f;

                // Out along the span, and forward-then-back across it.
                const float out = spanIn + (spanOut - spanIn) * std::sin(a * 0.5f);
                const float fore = lead * std::sin(a) - trail * t;

                pts.push_back(F.p(hingeX + fore, s * out * beatUp));
            }

            pts.push_back(F.p(hingeX - trail * 0.2f, s * hingeY));
            shape(into, pts, colour, skin);
        };

        for (const float s : { -1.0f, 1.0f })
        {
            wing(R * 0.24f, R * 0.20f, R * 0.24f, R * 1.22f, R * 0.46f, R * 0.70f, s,
                 fade(fin, 190));
            wing(-R * 0.44f, R * 0.20f, R * 0.20f, R * 0.82f, R * 0.22f, R * 0.44f, s,
                 fade(fin, 150));
        }

        // The body, furred and blunt.
        spindle(into, F, R * 0.78f, -R * 0.92f, R * 0.28f, 0.34f, hide, skin, 9);

        // And what it has instead of eyes. Feathered, and it leads with them.
        for (const float s : { -1.0f, 1.0f })
        {
            const sf::Angle dir = (F.fwd + F.side * (s * 0.78f)).angle();
            ribbon(into, F.p(R * 0.70f, s * R * 0.12f), dir, R * 0.95f, R * 0.09f, R * 0.02f,
                   s * 9.0f, 4, belly, fade(belly, 0.0f), skin);

            for (int i = 1; i < 4; ++i)
            {
                const float t = static_cast<float>(i) / 4.0f;
                const sf::Vector2f root = F.p(R * (0.70f + 0.62f * t), s * R * (0.12f + 0.52f * t));
                sweep(into, root, root + (F.fwd * 0.2f + F.side * s) * (R * 0.22f), R * 0.03f,
                      R * 0.01f, fade(belly, 150), skin);
            }
        }
    }

    // ---- 11. Effigy --------------------------------------------------
    // Something that survives by not moving. Arms drawn in tight against a
    // mantle, everything tucked, a posture with no motion in it at all - and
    // that stillness is the tell, because everything else down there is busy.
    void drawEffigy(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                    const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;

        // Eight arms, held folded rather than streaming.
        for (int i = 0; i < 8; ++i)
        {
            const float t = static_cast<float>(i) / 8.0f;
            const sf::Angle a = sf::degrees(-62.0f + 124.0f * t);
            const float curl = (i % 2 == 0) ? 22.0f : -18.0f;

            ribbon(into, F.p(R * 0.50f, 0.0f), (F.fwd.rotatedBy(a)).angle(), R * 0.92f,
                   R * 0.12f, R * 0.03f, curl, 4, hide, blend(hide, fin, 0.7f), skin);
        }

        // The mantle: a plain sac, broadest at the shoulders and drawn to a
        // point at the back.
        spindle(into, F, R * 0.66f, -R * 1.16f, R * 0.60f, 0.22f, hide, skin, 11);

        // Mottling, which is the one thing a creature that hides has to have.
        for (int i = 0; i < 5; ++i)
        {
            const float x = R * (0.34f - 0.30f * static_cast<float>(i));
            const float y = R * ((i % 2 == 0) ? 0.24f : -0.26f);
            disc(into, F.p(x, y), R * 0.17f, fade(belly, 120), skin, 8);
        }

        for (const float s : { -1.0f, 1.0f })
            look(into, F.p(R * 0.46f, s * R * 0.34f), R * 0.14f, sf::Color(196, 196, 170), skin);
    }

    // ---- 12. Stalker -------------------------------------------------
    // The head is small and the body is the weapon, so the body is what is
    // drawn: a ribbon that runs the whole length with a fin down all of it.
    // Its spine comes from the fight, because the fight is what makes it move.
    void drawStalker(sf::VertexArray& into, const Frame& F, const creatureart::Pose& pose,
                     const Skin& skin, sf::Color hide, sf::Color belly, sf::Color fin)
    {
        const float R = pose.size;
        const float gape = pose.jaw * R * 0.30f;

        // The head only. The rest of it is drawn by Enemy off its own spine,
        // because the spine is where the animal actually is.
        shape(into,
              { F.p(R * 1.10f, 0.0f), F.p(R * 0.56f, -R * 0.40f), F.p(-R * 0.34f, -R * 0.46f),
                F.p(-R * 0.62f, 0.0f), F.p(-R * 0.34f, R * 0.46f), F.p(R * 0.56f, R * 0.40f) },
              hide, skin);

        // A jaw that hinges well back, because it holds on.
        shape(into,
              { F.p(R * 1.10f, 0.0f), F.p(-R * 0.20f, R * 0.44f + gape),
                F.p(R * 0.54f, R * 0.18f + gape) },
              belly, skin);

        sweep(into, F.p(R * 0.30f, 0.0f), F.p(-R * 0.60f, 0.0f), R * 0.16f, R * 0.08f,
              fade(fin, 190), skin);

        look(into, F.p(R * 0.64f, -R * 0.20f), R * 0.14f, sf::Color(246, 96, 84), skin);
    }

    // Three colours per creature: the hide, whatever is pale underneath, and
    // whatever the soft parts are. Everything is multiplied by the tint the
    // fight hands over, so a hit flash and a chill still work without any of
    // the shapes above knowing they exist.
    struct Palette { sf::Color hide, belly, fin; };

    constexpr std::array<Palette, EnemyTypeCount> Coats{ {
        { { 74, 92, 104 },   { 168, 182, 188 }, { 96, 122, 136 } },  // Lurker
        { { 186, 178, 150 }, { 236, 232, 214 }, { 150, 138, 112 } }, // Lancer
        { { 92, 74, 104 },   { 176, 160, 186 }, { 128, 104, 148 } }, // Weaver
        { { 158, 70, 122 },  { 232, 168, 206 }, { 196, 104, 164 } }, // Bloom
        { { 178, 186, 170 }, { 232, 236, 214 }, { 140, 150, 140 } }, // Spawnling
        { { 138, 112, 58 },  { 216, 190, 108 }, { 106, 86, 46 } },   // Brooder
        { { 62, 70, 78 },    { 148, 138, 120 }, { 168, 176, 182 } }, // Warden
        { { 96, 96, 86 },    { 156, 156, 142 }, { 120, 120, 108 } }, // Revenant
        { { 64, 128, 138 },  { 168, 244, 238 }, { 96, 188, 196 } },  // Herald
        { { 148, 140, 128 }, { 214, 206, 186 }, { 178, 170, 156 } }, // Moth
        { { 104, 104, 96 },  { 62, 60, 54 },    { 142, 142, 128 } }, // Effigy
        { { 58, 62, 74 },    { 154, 96, 92 },   { 96, 104, 122 } },  // Stalker
    } };
}

namespace creatureart
{
    sf::Color coatOf(EnemyType type)
    {
        const std::size_t i = static_cast<std::size_t>(type);
        return i < Coats.size() ? Coats[i].hide : sf::Color(150, 160, 170);
    }

    void draw(sf::RenderTarget& target, EnemyType type, const Pose& pose, const sf::Texture& hide)
    {
        if (pose.size <= 0.5f || pose.tint.a == 0)
            return;

        const float tile = static_cast<float>(hide.getSize().x);

        sf::VertexArray skinned(sf::PrimitiveType::Triangles);

        Frame F;
        F.fwd = unit(pose.angle);
        F.side = across(F.fwd);
        F.along = pose.along;
        F.across = pose.across;

        // A hit rocks it back along its own length, the same as it does on the
        // big ones. It is the difference between a shot landing and a shot
        // being subtracted from a number.
        F.at = pose.at - F.fwd * (pose.flinch * pose.size * 0.34f);

        Skin skin;
        skin.origin = F.at;
        skin.along = F.fwd;
        skin.tile = tile;

        const Palette& coat = Coats[static_cast<std::size_t>(type)];

        const sf::Color hideC = wash(coat.hide, pose.tint);
        const sf::Color bellyC = wash(coat.belly, pose.tint);
        const sf::Color finC = wash(coat.fin, pose.tint);

        // The ones that tow a body rather than having one. It is drawn first
        // and from the tail forward, so each section overlaps the one behind
        // it and the animal is a body instead of a row of beads.
        if (pose.spine != nullptr && pose.spine->size() >= 2)
        {
            const std::vector<sf::Vector2f>& bone = *pose.spine;
            const float steps = static_cast<float>(bone.size() - 1);

            for (std::size_t i = bone.size() - 1; i-- > 0;)
            {
                const float t0 = static_cast<float>(i) / steps;
                const float t1 = static_cast<float>(i + 1) / steps;

                // Thickest a little behind the head and tapering to nothing,
                // which is the profile of everything that swims by wave.
                const auto girth = [&](float t) {
                    return pose.size * (0.92f - 0.80f * t * t) * (0.55f + 0.45f * (1.0f - t));
                };

                const sf::Vector2f a = bone[i];
                const sf::Vector2f b = bone[i + 1];
                const sf::Vector2f run = b - a;

                if (run.lengthSquared() < 0.0001f)
                    continue;

                const sf::Vector2f n = across(run.normalized());
                const float g0 = girth(t0);
                const float g1 = girth(t1);

                Skin along = skin;
                along.along = run.normalized();
                along.origin = a;

                quad(skinned, a - n * g0, b - n * g1, b + n * g1, a + n * g0,
                     blend(hideC, fade(hideC, 0.85f), t0), along);

                // The fin that runs the whole length, which is the thing that
                // makes a ribbon read as an animal and not as a rope.
                quad(skinned, a - n * (g0 * 1.55f), b - n * (g1 * 1.55f), b - n * g1, a - n * g0,
                     blend(fade(finC, 150), fade(finC, 0), t0), along);
                quad(skinned, a + n * g0, b + n * g1, b + n * (g1 * 1.55f), a + n * (g0 * 1.55f),
                     blend(fade(finC, 150), fade(finC, 0), t0), along);
            }
        }

        switch (type)
        {
            case EnemyType::Lurker:    drawLurker(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Lancer:    drawLancer(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Weaver:    drawWeaver(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Bloom:     drawBloom(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Spawnling: drawSpawnling(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Brooder:   drawBrooder(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Warden:    drawWarden(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Revenant:  drawRevenant(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Herald:    drawHerald(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Moth:      drawMoth(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Effigy:    drawEffigy(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Stalker:   drawStalker(skinned, F, pose, skin, hideC, bellyC, finC); break;
            case EnemyType::Count:     break;
        }

        if (skinned.getVertexCount() == 0)
            return;

        sf::RenderStates states;
        states.texture = &hide;
        target.draw(skinned, states);
    }
}

namespace bossart
{
    void draw(sf::RenderTarget& target, BossSkin skin, const Body& body, const sf::Texture& hide)
    {
        if (body.links.empty() || body.alpha <= 0.01f)
            return;

        switch (skin)
        {
            case BossSkin::Leviathan: drawLeviathan(target, body, hide); break;
            case BossSkin::Medusa:    drawMedusa(target, body, hide);    break;
            case BossSkin::Worm:      drawWorm(target, body, hide);      break;
            case BossSkin::Hollow:    drawHollow(target, body, hide);    break;
            case BossSkin::Furnace:   drawFurnace(target, body, hide);   break;
            case BossSkin::Mother:    drawMother(target, body, hide);    break;
            case BossSkin::Sprites:   break; // not ours to draw
        }
    }
}
