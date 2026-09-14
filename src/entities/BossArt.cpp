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
}

namespace bossart
{
    void drawLeviathan(sf::RenderTarget& target, const Body& body, const sf::Texture& hide)
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
        for (const Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Fin)
                drawFin(skinned, limb, body, tint(Hide), kick, tile);
        }

        // Then the animal, three times: the shadow it casts on itself, the
        // hide, and the light down its back.
        drawSpine(skinned, body, 3.0f, tint(Ink), kick + down, tile);
        drawHead(skinned, body, tint(Ink), kick + down, tile, false);

        drawSpine(skinned, body, 0.0f, tint(Hide), kick, tile);

        drawSpine(skinned, body, -6.0f, tint(Belly), kick + up, tile);
        drawFluke(skinned, body, tint(Hide), kick, tile);
        drawBands(skinned, body, kick, tile, alpha);

        // The dorsal ridge, which is also where the flex shows: the plates
        // slide over one another as the spine bends.
        for (std::size_t i = 1; i + 1 < body.links.size(); ++i)
        {
            const Link& link = body.links[i];
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
                      tint(Ridge), skin);
            }
        }

        for (const Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Armour)
                drawPlate(skinned, limb, kick, tile);
        }

        drawHead(skinned, body, tint(Hide), kick, tile, true);

        // And the wind-up, on the animal rather than under it: the whole thing
        // goes pale for the half second before it commits.
        if (body.tell > 0.0f)
        {
            const float glow = body.tell * body.tell * 0.16f;

            drawSpine(skinned, body, 1.0f, fade(sf::Color(255, 140, 90), glow * alpha), kick, tile);
            drawHead(skinned, body, fade(sf::Color(255, 160, 110), glow * alpha), kick, tile, false);
        }

        // Then everything with no scale on it.
        for (const Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Organ)
                drawOrgan(plain, limb, body, kick);
        }

        drawSoft(plain, body, kick);

        for (const Limb& limb : body.limbs)
        {
            if (limb.role == PartRole::Eye)
                drawEye(plain, limb, body, kick);
        }

        sf::RenderStates states;
        states.texture = &hide;

        target.draw(skinned, states);
        target.draw(plain);
    }
}
