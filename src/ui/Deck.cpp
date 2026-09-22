#include "ui/Deck.h"

#include "core/Assets.h"
#include "core/Config.h"
#include "ui/PixelFont.h"
#include "upgrades/Stats.h"

#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/PrimitiveType.hpp>
#include <SFML/Graphics/RenderStates.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace
{
    constexpr float W = static_cast<float>(config::WindowWidth);
    constexpr float H = static_cast<float>(config::WindowHeight);

    // The plate, and the strip inside it a man can actually stand on. The gap
    // between the two is the rail.
    constexpr float PlateLeft = 152.0f;
    constexpr float PlateTop = 92.0f;
    constexpr float PlateRight = W - 152.0f;
    constexpr float PlateBottom = H - 70.0f;
    constexpr float Kerb = 30.0f;

    // The floor is drawn at half size. The tile is big so it can carry rivets
    // and rust without looking like noise; at one to one those plates are two
    // strides across, which reads as a warehouse rather than a rig.
    constexpr float PlateScale = 0.5f;

    // The hole in the middle of it. The bell hangs in this, which is why it is
    // the one part of the deck that is not deck.
    constexpr sf::Vector2f Pool{ 640.0f, 470.0f };
    constexpr float PoolRadius = 92.0f;

    // How much bigger than its sprite each thing is drawn. The man is the only
    // one that matters: at his own size he is a speck on a rig, which is true
    // and useless.
    // All one. Every one of these used to be drawn bigger than its texture -
    // the diver at 1.55, the bell at 1.72, the crane at 1.22 - so the three
    // things a player looks at most on this screen were the three blurriest
    // objects in the game. The art is drawn at the size it is shown at now.
    constexpr float DiverScale = 1.0f;
    constexpr float BellScale = 1.0f;

    // A crane is the biggest thing on a deck and it was reading as a box with
    // a stick, because most of its length is lattice and lattice is mostly
    // holes. Drawn a fifth over size it holds its corner.
    constexpr float CraneScale = 1.0f;

    constexpr sf::Color Paint{ 176, 148, 52 };
    constexpr sf::Color Hole{ 5, 10, 14 };

    // Where the water stands after a squall. A puddle is not a highlight, it
    // is a hole: darker than the plate, and then it borrows a lamp back off
    // the light pass.
    struct Puddle { sf::Vector2f at; sf::Vector2f radii; };

    constexpr std::array<Puddle, 9> Puddles{ {
        { { 256.0f, 330.0f }, { 54.0f, 33.0f } },
        { { 452.0f, 226.0f }, { 40.0f, 26.0f } },
        { { 386.0f, 566.0f }, { 66.0f, 38.0f } },
        { { 556.0f, 348.0f }, { 47.0f, 27.0f } },
        { { 712.0f, 596.0f }, { 58.0f, 34.0f } },
        { { 836.0f, 396.0f }, { 44.0f, 29.0f } },
        { { 1006.0f, 340.0f }, { 52.0f, 32.0f } },
        { { 908.0f, 596.0f }, { 62.0f, 36.0f } },
        { { 640.0f, 214.0f }, { 72.0f, 34.0f } },
    } };

    // Crates by the crane, which is where a hold that has been sold ends up.
    constexpr std::array<sf::Vector2f, 5> Crates{ {
        { 232.0f, 402.0f }, { 288.0f, 436.0f }, { 372.0f, 420.0f },
        { 1042.0f, 480.0f }, { 988.0f, 520.0f },
    } };

    sf::Color fade(sf::Color colour, float alpha)
    {
        colour.a = static_cast<std::uint8_t>(
            std::clamp(alpha, 0.0f, 1.0f) * static_cast<float>(colour.a));
        return colour;
    }

    // A sprite cannot be lit by adding a bright ellipse on top of it without
    // painting an ellipse on top of it. setColor is a multiply and costs
    // nothing, so what light is standing where a thing stands is folded into
    // its tint instead. Walking into a pool visibly brightens the man.
    sf::Color litTint(sf::Color base, float light)
    {
        // Leaned on hard on purpose. A sprite standing in a pool competes with
        // the additive light lying on the plate around it, so a timid multiply
        // leaves every object reading as a black hole in the middle of a lamp.
        const float k = std::clamp(light * 1.45f, 0.0f, 1.0f) * 0.86f;

        return { static_cast<std::uint8_t>(base.r + (255.0f - base.r) * k),
                 static_cast<std::uint8_t>(base.g + (255.0f - base.g) * k),
                 static_cast<std::uint8_t>(base.b + (255.0f - base.b) * k), 255 };
    }

    sf::Vector2f unit(float degrees)
    {
        const float r = degrees * 0.01745329f;
        return { std::cos(r), std::sin(r) };
    }

    // A repeatable wobble, so the rain falls the same way every time the deck
    // is opened and none of it needs to be stored between frames.
    float wiggle(float seed)
    {
        const float x = std::sin(seed * 12.9898f) * 43758.5453f;
        return x - std::floor(x);
    }

    float wrap(float value, float period)
    {
        return std::fmod(std::fmod(value, period) + period, period);
    }

    // How much of a lamp reaches a point t of the way to its rim. Cubed rather
    // than squared, which is the difference between a floodlight and a patch
    // of fog: with a square the half-way point is still at 56 per cent and the
    // whole pool reads as one flat wash, and with a cube it is at 42 and the
    // lamp has a middle.
    float falloff(float t)
    {
        if (t >= 1.0f)
            return 0.0f;

        const float k = 1.0f - t * t;
        return k * k * k;
    }
}

Deck::Deck(const Assets& assets)
    : m_stations{ {
          { { 336.0f, 238.0f }, 74.0f, "THE CRANE", "SELL THE HOLD", {}, true },
          { { 952.0f, 224.0f }, 62.0f, "THE BENCH", "OPEN THE BOARD", {}, true },
          { Pool, 0.0f, "THE BELL", "GO DOWN", {}, true },
      } }
    , m_mesh(sf::PrimitiveType::Triangles)
    , m_glow(sf::PrimitiveType::Triangles)
    , m_text(sf::PrimitiveType::Triangles)
    , m_plate(assets.byPath(config::DeckPlatePath))
    , m_rail(assets.byPath(config::DeckRailPath))
    , m_crane(assets.byPath(config::DeckCranePath))
    , m_bench(assets.byPath(config::DeckBenchPath))
    , m_bell(assets.byPath(config::DeckBellPath))
    , m_crate(assets.byPath(config::DeckCratePath))
    , m_body(assets.byPath(config::DeckDiverPath))
    , m_boot(assets.byPath(config::DeckBootPath))
    , m_subHull(assets.player)
    , m_subKit(assets)
{
    const auto centre = [](sf::Sprite& sprite) {
        const sf::Vector2f size(sprite.getTexture().getSize());
        sprite.setOrigin(size * 0.5f);
    };

    centre(m_crane);
    centre(m_bench);
    centre(m_bell);
    centre(m_crate);
    centre(m_body);
    centre(m_boot);
    centre(m_subHull);

    m_subHull.setScale({ config::PlayerScale * config::DeckSubScale,
                         config::PlayerScale * config::DeckSubScale });

    // Fitted but not charged. A live shield at this scale is a bright ring
    // most of the way across the sub, and it wins every time against the hull
    // it is supposed to be protecting - which is the wrong thing to be looking
    // at on the one screen that exists to show what you have built. Dull, it
    // still says the emitter is there.
    m_subKit.setShieldUp(false);

    m_crane.setPosition(station(Post::Crane).at);
    m_bench.setPosition(station(Post::Bench).at);
    m_bell.setPosition(Pool);

    // One sprite for the whole floor, and one for a whole run of rail.
    m_plate.setTextureRect(sf::IntRect(
        { 0, 0 }, { static_cast<int>((PlateRight - PlateLeft) / PlateScale),
                    static_cast<int>((PlateBottom - PlateTop) / PlateScale) }));
    m_plate.setScale({ PlateScale, PlateScale });
    m_plate.setPosition({ PlateLeft, PlateTop });

    m_body.setScale({ DiverScale, DiverScale });
    m_boot.setScale({ DiverScale, DiverScale });
    m_bell.setScale({ BellScale, BellScale });
    m_crane.setScale({ CraneScale, CraneScale });

    m_props.reserve(config::DeckProps.size());

    for (const config::DeckProp& prop : config::DeckProps)
    {
        sf::Sprite& sprite = m_props.emplace_back(assets.byPath(prop.path));
        centre(sprite);
        sprite.setPosition(prop.at);
        sprite.setRotation(sf::degrees(prop.turn));
    }

    enter();
}

void Deck::enter()
{
    // Out of the bell and onto the deck, facing the length of it.
    m_diver = Pool + sf::Vector2f{ 0.0f, -(PoolRadius + 54.0f) };
    m_speed = { 0.0f, 0.0f };
    m_face = sf::degrees(-90.0f);
    m_effort = 0.0f;
    m_at = Post::Count;
}

void Deck::setNote(Post which, std::string note)
{
    if (which != Post::Count)
        station(which).note = std::move(note);
}

void Deck::setOpen(Post which, bool open)
{
    if (which != Post::Count)
        station(which).open = open;
}

void Deck::setSub(const Stats& stats)
{
    m_subStats = &stats;
}

void Deck::update(float deltaTime, sf::Vector2f drive)
{
    m_time += deltaTime;

    if (drive.lengthSquared() > 1.0f)
        drive = drive.normalized();

    m_speed += drive * config::DeckAccel * deltaTime;

    // Drag rather than a hard clamp, so letting go coasts a step instead of
    // stopping dead. A man in eighty kilos of brass does not stop dead.
    m_speed -= m_speed * std::min(1.0f, config::DeckDrag * deltaTime);

    if (m_speed.length() > config::DeckWalkSpeed)
        m_speed = m_speed.normalized() * config::DeckWalkSpeed;

    m_diver += m_speed * deltaTime;
    keepOnDeck();

    const float pace = m_speed.length();
    m_effort = std::clamp(pace / config::DeckWalkSpeed, 0.0f, 1.0f);

    // A boot lands every half turn of the stride, which is the same phase the
    // sprite swings on - so the sound and the picture cannot drift apart.
    const float was = m_stride;
    m_stride += pace * deltaTime * config::DeckStride;
    m_stepped = pace > 20.0f
                && std::floor(was / 3.14159265f) != std::floor(m_stride / 3.14159265f);

    // Turn toward where he is going rather than snapping, and keep the last
    // heading when he stops: a man who halts does not face north.
    if (pace > 12.0f)
    {
        const sf::Angle want = m_speed.angle();
        const float step = config::DeckTurn * deltaTime;
        const float difference = (want - m_face).wrapSigned().asDegrees();
        m_face += sf::degrees(std::clamp(difference, -step, step));
    }

    // And what he is standing at. Nearest wins, so two stations cannot both
    // claim the same patch of deck.
    m_at = Post::Count;
    float best = config::DeckReach * config::DeckReach;

    for (std::size_t i = 0; i < m_stations.size(); ++i)
    {
        const float distance = (m_stations[i].at - m_diver).lengthSquared();
        if (m_stations[i].open && distance < best)
        {
            best = distance;
            m_at = static_cast<Post>(i);
        }
    }

    // Park the sub. It rides the swell a little, the same as the bell, and it
    // is lit and tinted like everything else on the plate - hull and hardware
    // together, so a bolted-on gun is not the one bright thing at night.
    if (m_subStats)
    {
        // Hanging, not parked: it rides the swell under it and swings a little
        // on its slings, on two clocks that do not share a period so the two
        // never quite line up.
        const float bob = std::sin(m_time * 0.9f) * config::DeckSubBob;
        const sf::Vector2f at = config::DeckSubAt + sf::Vector2f{ 0.0f, bob };
        const sf::Angle heading = sf::degrees(config::DeckSubTurn
                                              + std::sin(m_time * 0.61f) * config::DeckSubSway);
        const sf::Color tint = litTint(config::DeckNightTint,
                                       std::max(lightAt(config::DeckSubAt), 0.30f));

        m_subKit.setTint(tint);
        m_subKit.update(at, heading, *m_subStats, deltaTime);
        m_subKit.present(at, heading, config::DeckSubScale);

        m_subHull.setPosition(at);
        m_subHull.setRotation(heading);
        m_subHull.setColor(tint);
    }
}

void Deck::keepOnDeck()
{
    m_diver.x = std::clamp(m_diver.x, PlateLeft + Kerb, PlateRight - Kerb);
    m_diver.y = std::clamp(m_diver.y, PlateTop + Kerb, PlateBottom - Kerb);

    // Out of the hole, out of the shack, and out of anything bolted down.
    const auto shove = [this](sf::Vector2f from, float radius) {
        const sf::Vector2f away = m_diver - from;
        const float distance = away.length();

        if (distance >= radius || distance < 0.001f)
            return;

        m_diver = from + away / distance * radius;
        m_speed -= away / distance * std::max(0.0f, m_speed.dot(away / distance));
    };

    shove(Pool, PoolRadius + 22.0f);

    // The sub itself is out over the water now, so there is nothing to walk
    // into: only the two legs the gantry stands on where it meets the plate.
    shove({ config::DeckGantryLeft, config::DeckGantryIn }, config::DeckGantryFoot);
    shove({ config::DeckGantryRight, config::DeckGantryIn }, config::DeckGantryFoot);

    for (const Station& post : m_stations)
    {
        if (post.solid > 0.0f)
            shove(post.at, post.solid);
    }

    for (const config::DeckProp& prop : config::DeckProps)
    {
        if (prop.solid > 0.0f)
            shove(prop.at, prop.solid);
    }
}

// ---------------------------------------------------------------------------
//  Light
// ---------------------------------------------------------------------------

float Deck::lampNow(std::size_t index) const
{
    const config::DeckLight& light = config::DeckLights[index];
    const float seed = static_cast<float>(index);

    float strength = light.strength
                     * (1.0f + light.flicker * 0.5f
                                   * (std::sin(m_time * 7.3f + seed * 2.1f)
                                      + std::sin(m_time * 11.7f + seed * 4.7f)));

    // The bad one. Every five and a bit seconds it goes, for about a fifth of
    // a second, and then comes back - which is the difference between a rig
    // that is lit and a rig that somebody stopped maintaining.
    if (light.failing && std::fmod(m_time * 0.19f + 0.33f, 1.0f) < 0.035f)
        strength *= 0.28f;

    return std::max(0.0f, strength) * config::DeckLightGain;
}

float Deck::lightAt(sf::Vector2f where) const
{
    float sum = 0.0f;

    for (std::size_t i = 0; i < config::DeckLights.size(); ++i)
    {
        const config::DeckLight& light = config::DeckLights[i];
        const sf::Vector2f away = (where - light.pool).rotatedBy(sf::degrees(-light.turn));
        const float u = away.x / light.radii.x;
        const float v = away.y / light.radii.y;
        const float t = std::sqrt(u * u + v * v);

        sum += lampNow(i) * falloff(t);
    }

    // The hole. It is the only cold light up here, and it is brighter when the
    // bell will take you than when it will not.
    const float moon = station(Post::Bell).open
                           ? 0.32f + 0.09f * std::sin(m_time * 2.1f)
                           : 0.08f;
    sum += moon * falloff((where - Pool).length() / 150.0f);

    // And the shack window, which throws a slot rather than a circle.
    if (where.x > 292.0f && where.x < 524.0f && where.y > 520.0f && where.y < 640.0f)
        sum += 0.18f * (1.0f - (where.x - 292.0f) / 232.0f);

    return std::clamp(sum, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
//  The vocabulary
// ---------------------------------------------------------------------------

void Deck::disc(sf::Vector2f at, float radius, sf::Color colour, int steps)
{
    oval(at, { radius, radius }, colour, steps);
}

void Deck::oval(sf::Vector2f at, sf::Vector2f radii, sf::Color colour, int steps)
{
    for (int i = 0; i < steps; ++i)
    {
        const float a0 = 360.0f * static_cast<float>(i) / static_cast<float>(steps);
        const float a1 = 360.0f * static_cast<float>(i + 1) / static_cast<float>(steps);

        const sf::Vector2f d0 = unit(a0);
        const sf::Vector2f d1 = unit(a1);

        m_mesh.append(sf::Vertex{ at, colour });
        m_mesh.append(sf::Vertex{ at + sf::Vector2f{ d0.x * radii.x, d0.y * radii.y }, colour });
        m_mesh.append(sf::Vertex{ at + sf::Vector2f{ d1.x * radii.x, d1.y * radii.y }, colour });
    }
}

void Deck::ovalTurned(sf::Vector2f at, sf::Vector2f radii, float turn, sf::Color colour,
                      int steps)
{
    const sf::Vector2f face = unit(turn);
    const sf::Vector2f side{ -face.y, face.x };

    for (int i = 0; i < steps; ++i)
    {
        const float a0 = 360.0f * static_cast<float>(i) / static_cast<float>(steps);
        const float a1 = 360.0f * static_cast<float>(i + 1) / static_cast<float>(steps);

        const sf::Vector2f d0 = unit(a0);
        const sf::Vector2f d1 = unit(a1);

        m_mesh.append(sf::Vertex{ at, colour });
        m_mesh.append(sf::Vertex{ at + face * (radii.x * d0.x) + side * (radii.y * d0.y),
                                  colour });
        m_mesh.append(sf::Vertex{ at + face * (radii.x * d1.x) + side * (radii.y * d1.y),
                                  colour });
    }
}

void Deck::ring(sf::VertexArray& into, sf::Vector2f at, float inner, float outer,
                sf::Color colour, int steps)
{
    for (int i = 0; i < steps; ++i)
    {
        const float a0 = 360.0f * static_cast<float>(i) / static_cast<float>(steps);
        const float a1 = 360.0f * static_cast<float>(i + 1) / static_cast<float>(steps);

        const sf::Vector2f d0 = unit(a0);
        const sf::Vector2f d1 = unit(a1);

        into.append(sf::Vertex{ at + d0 * inner, colour });
        into.append(sf::Vertex{ at + d0 * outer, colour });
        into.append(sf::Vertex{ at + d1 * outer, colour });

        into.append(sf::Vertex{ at + d0 * inner, colour });
        into.append(sf::Vertex{ at + d1 * outer, colour });
        into.append(sf::Vertex{ at + d1 * inner, colour });
    }
}

void Deck::quad(sf::Vector2f at, sf::Vector2f size, sf::Color colour)
{
    const sf::Vector2f a = at;
    const sf::Vector2f b = at + sf::Vector2f{ size.x, 0.0f };
    const sf::Vector2f c = at + size;
    const sf::Vector2f d = at + sf::Vector2f{ 0.0f, size.y };

    m_mesh.append(sf::Vertex{ a, colour });
    m_mesh.append(sf::Vertex{ b, colour });
    m_mesh.append(sf::Vertex{ c, colour });

    m_mesh.append(sf::Vertex{ a, colour });
    m_mesh.append(sf::Vertex{ c, colour });
    m_mesh.append(sf::Vertex{ d, colour });
}

void Deck::pool(sf::VertexArray& into, sf::Vector2f at, sf::Vector2f radii, float turn,
                sf::Color colour, float strength, int bands, int steps)
{
    if (strength <= 0.0f)
        return;

    const sf::Vector2f face = unit(turn);
    const sf::Vector2f side{ -face.y, face.x };

    const auto point = [&](float t, float degrees) {
        const sf::Vector2f d = unit(degrees);
        return at + face * (radii.x * t * d.x) + side * (radii.y * t * d.y);
    };

    for (int band = 0; band < bands; ++band)
    {
        const float t0 = static_cast<float>(band) / static_cast<float>(bands);
        const float t1 = static_cast<float>(band + 1) / static_cast<float>(bands);

        const sf::Color c0 = fade(colour, strength * falloff(t0));
        const sf::Color c1 = fade(colour, strength * falloff(t1));

        for (int i = 0; i < steps; ++i)
        {
            const float a0 = 360.0f * static_cast<float>(i) / static_cast<float>(steps);
            const float a1 = 360.0f * static_cast<float>(i + 1) / static_cast<float>(steps);

            into.append(sf::Vertex{ point(t0, a0), c0 });
            into.append(sf::Vertex{ point(t1, a0), c1 });
            into.append(sf::Vertex{ point(t1, a1), c1 });

            into.append(sf::Vertex{ point(t0, a0), c0 });
            into.append(sf::Vertex{ point(t1, a1), c1 });
            into.append(sf::Vertex{ point(t0, a1), c0 });
        }
    }
}

void Deck::smear(sf::VertexArray& into, sf::Vector2f from, sf::Vector2f to, float wide0,
                 float wide1, sf::Color a, sf::Color b)
{
    const sf::Vector2f along = to - from;
    if (along.lengthSquared() < 0.001f)
        return;

    const sf::Vector2f side = sf::Vector2f{ -along.y, along.x }.normalized();

    into.append(sf::Vertex{ from - side * wide0, a });
    into.append(sf::Vertex{ to - side * wide1, b });
    into.append(sf::Vertex{ to + side * wide1, b });

    into.append(sf::Vertex{ from - side * wide0, a });
    into.append(sf::Vertex{ to + side * wide1, b });
    into.append(sf::Vertex{ from + side * wide0, a });
}

void Deck::shadowFor(sf::Vector2f at, float across, float contact)
{
    // Contact first. It has no direction and no offset, and it is the single
    // thing that stops a crate floating above the plate.
    oval(at + sf::Vector2f{ 0.0f, 3.0f }, { across * 0.30f, across * 0.26f },
         sf::Color(0, 0, 0, static_cast<std::uint8_t>(contact)), 18);

    // Then one cast per lamp, strongest two only. Every lamp throws a shadow,
    // but five of them under one crate is a smudge rather than a rig.
    std::size_t best = 0;
    std::size_t second = 0;
    float bestWeight = -1.0f;
    float secondWeight = -1.0f;

    for (std::size_t i = 0; i < config::DeckLights.size(); ++i)
    {
        const config::DeckLight& light = config::DeckLights[i];
        const sf::Vector2f away = (at - light.pool).rotatedBy(sf::degrees(-light.turn));
        const float u = away.x / light.radii.x;
        const float v = away.y / light.radii.y;
        const float t = std::sqrt(u * u + v * v);
        const float weight = lampNow(i) * falloff(t);

        if (weight > bestWeight)
        {
            secondWeight = bestWeight;
            second = best;
            bestWeight = weight;
            best = i;
        }
        else if (weight > secondWeight)
        {
            secondWeight = weight;
            second = i;
        }
    }

    const auto cast = [&](std::size_t index, float scale) {
        if (scale <= 0.0f)
            return;

        const config::DeckLight& light = config::DeckLights[index];
        const sf::Vector2f away = at - light.mast;

        if (away.lengthSquared() < 1.0f)
            return;

        const sf::Vector2f along = away.normalized();
        const float reach = std::clamp(away.length() / light.radii.x, 0.25f, 1.35f);
        const float alpha = (96.0f - 34.0f * reach) * scale;

        ovalTurned(at + along * (6.0f + 22.0f * reach),
                   { across * 0.44f * (1.0f + 0.55f * reach), across * 0.40f },
                   along.angle().asDegrees(),
                   sf::Color(0, 0, 0, static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 255.0f))),
                   20);
    };

    cast(best, bestWeight > 0.0f ? 1.0f : 0.0f);
    cast(second, secondWeight > 0.0f ? 0.5f : 0.0f);
}

// ---------------------------------------------------------------------------
//  Drawing
// ---------------------------------------------------------------------------

void Deck::drawSea()
{
    quad({ 0.0f, 0.0f }, { W, H }, config::DeckNightSea);

    // Swell. Long, slow, and going one way, which is all it takes for the deck
    // to read as floating rather than as a room with a black border.
    for (int i = 0; i < 30; ++i)
    {
        const float y = static_cast<float>(i) * 26.0f - 20.0f;
        const float phase = m_time * 0.55f + static_cast<float>(i) * 0.7f;
        const float lift = std::sin(phase) * 7.0f;
        const float strength = 0.30f + 0.26f * std::sin(phase * 0.5f + 1.3f);

        quad({ -40.0f, y + lift }, { W + 80.0f, 7.0f }, fade(config::DeckNightSwell, strength));
        quad({ -40.0f, y + lift + 9.0f }, { W + 80.0f, 2.0f },
             fade(config::DeckNightFoam, strength * 0.22f));
    }

    // Where the sea breaks against the rig. Only a thin light edge, but
    // without it the plate looks laid on top of the water rather than in it.
    const float surge = 3.0f + 2.0f * std::sin(m_time * 1.7f);

    quad({ PlateLeft - surge - 5.0f, PlateTop - surge - 5.0f },
         { PlateRight - PlateLeft + (surge + 5.0f) * 2.0f, surge + 5.0f },
         fade(config::DeckNightFoam, 0.5f));
    quad({ PlateLeft - surge - 5.0f, PlateBottom },
         { PlateRight - PlateLeft + (surge + 5.0f) * 2.0f, surge + 5.0f },
         fade(config::DeckNightFoam, 0.5f));
    quad({ PlateLeft - surge - 5.0f, PlateTop }, { surge + 5.0f, PlateBottom - PlateTop },
         fade(config::DeckNightFoam, 0.4f));
    quad({ PlateRight, PlateTop }, { surge + 5.0f, PlateBottom - PlateTop },
         fade(config::DeckNightFoam, 0.4f));
}

void Deck::drawPlate(sf::RenderTarget& target)
{
    target.draw(m_mesh);
    m_mesh.clear();

    // The floor takes the flat night tint and no light at all. It is one
    // sprite covering the whole deck, so any light folded into its colour
    // lifts every corner of it equally - which is a uniformly grey floor, the
    // exact thing the night was supposed to get rid of. All of its brightness
    // comes from the additive pass, where it belongs.
    m_plate.setColor(config::DeckNightTint);
    target.draw(m_plate);

    // A painted lane from the bell to each station, because the first thing a
    // player does up here is ask where they are supposed to go. Dimmer than it
    // was: paint does not glow, and at night it is only where a lamp finds it.
    for (const Station& post : m_stations)
    {
        if (post.solid <= 0.0f)
            continue;

        const sf::Vector2f along = (post.at - Pool).normalized();
        const sf::Vector2f side{ -along.y, along.x };
        const float start = PoolRadius + 26.0f;
        const float end = (post.at - Pool).length() - post.solid - 8.0f;

        for (float travel = start; travel < end; travel += 46.0f)
        {
            const sf::Vector2f at = Pool + along * travel;
            const float length = std::min(26.0f, end - travel);
            const sf::Color ink = fade(Paint, 0.10f + 0.22f * lightAt(at));

            m_mesh.append(sf::Vertex{ at - side * 5.0f, ink });
            m_mesh.append(sf::Vertex{ at + along * length - side * 5.0f, ink });
            m_mesh.append(sf::Vertex{ at + along * length + side * 5.0f, ink });

            m_mesh.append(sf::Vertex{ at - side * 5.0f, ink });
            m_mesh.append(sf::Vertex{ at + along * length + side * 5.0f, ink });
            m_mesh.append(sf::Vertex{ at + side * 5.0f, ink });
        }
    }

    // The hole, and the sea moving about in the bottom of it.
    disc(Pool, PoolRadius + 14.0f, sf::Color(26, 30, 34), 34);
    disc(Pool, PoolRadius, Hole, 34);
    ring(m_mesh, Pool, PoolRadius * 0.62f, PoolRadius * 0.70f,
         fade(config::DeckNightSwell, 0.5f + 0.3f * std::sin(m_time * 1.4f)), 30);
    ring(m_mesh, Pool, PoolRadius * 0.86f, PoolRadius * 0.92f,
         fade(config::DeckNightSwell, 0.4f + 0.3f * std::sin(m_time * 1.4f + 2.0f)), 30);
}

void Deck::drawWet()
{
    // A puddle is a hole, not a highlight: darker than the plate, and then it
    // borrows a lamp back in the light pass.
    for (const Puddle& puddle : Puddles)
    {
        oval(puddle.at, puddle.radii, fade(config::DeckPuddle, 0.78f), 20);
        oval(puddle.at, puddle.radii * 1.14f, fade(config::DeckPuddle, 0.30f), 20);
    }

    // The one lit window on the whole rig, painted over the shack sprite where
    // its east wall is. Everything else up here is halide white; this is the
    // only thing anybody is warm behind.
    quad({ 288.0f, 552.0f }, { 5.0f, 48.0f }, fade(config::DeckShack, 0.75f));

    // Drips off the rails. The outboard ones, hanging off the edge into the
    // sea, are the ones that sell it.
    int drip = 0;

    const auto hang = [&](float x, float edge, float reach) {
        const float phase = wrap(m_time * 1.7f + wiggle(static_cast<float>(drip) * 3.1f), 1.0f);
        const float y = edge + phase * reach;

        quad({ x - 0.8f, y }, { 1.6f, 9.0f + 5.0f * phase },
             fade(config::DeckDrip, 0.17f * (1.0f - phase)));
        ++drip;
    };

    for (float x = PlateLeft + 24.0f; x < PlateRight; x += 46.0f)
    {
        hang(x, PlateTop - 6.0f, -18.0f);
        hang(x + 22.0f, PlateBottom, 18.0f);
    }

    for (float y = PlateTop + 40.0f; y < PlateBottom; y += 52.0f)
    {
        hang(PlateLeft + 10.0f, y, 14.0f);
        hang(PlateRight - 10.0f, y + 26.0f, 14.0f);
    }
}

void Deck::drawLightPass()
{
    // Everything on the floor. The order inside the pass does not matter -
    // additive is commutative - but the pass as a whole has to land before the
    // shadows, because a shadow is an absence of light and an absence painted
    // under the light would simply be erased by it.
    for (std::size_t i = 0; i < config::DeckLights.size(); ++i)
    {
        const config::DeckLight& light = config::DeckLights[i];
        const float now = lampNow(i);

        pool(m_glow, light.pool, light.radii * 2.05f, light.turn, config::DeckFlood,
             now * config::DeckSheen);
        pool(m_glow, light.pool, light.radii, light.turn, config::DeckFlood, now);
        pool(m_glow, light.pool, light.radii * 0.36f, light.turn, config::DeckFloodCore,
             now * config::DeckCore);

        // The streak wet steel always shows pointing straight at the lamp.
        smear(m_glow, light.mast,
              light.mast + unit(light.turn) * ((light.pool - light.mast).length() * 1.62f),
              7.0f, 11.0f, fade(config::DeckFlood, 0.09f * now),
              fade(config::DeckFlood, 0.0f));
    }

    // Spill over the side, so the floods obviously do not stop at the rail.
    // These sit on the moving swell and get its animation for nothing.
    pool(m_glow, { 200.0f, 70.0f }, { 150.0f, 54.0f }, 0.0f, config::DeckFlood, 0.045f);
    pool(m_glow, { 1082.0f, 68.0f }, { 146.0f, 52.0f }, 0.0f, config::DeckFlood, 0.042f);
    pool(m_glow, { 198.0f, 676.0f }, { 150.0f, 52.0f }, 0.0f, config::DeckFlood, 0.038f);
    pool(m_glow, { 1086.0f, 678.0f }, { 146.0f, 52.0f }, 0.0f, config::DeckFlood, 0.040f);

    // What each puddle is giving back. Offset toward the mast that is lighting
    // it, squashed to half height, and breathing on two sines that do not
    // share a period - which is the whole difference between water and a
    // sticker of water.
    for (std::size_t i = 0; i < Puddles.size(); ++i)
    {
        const Puddle& puddle = Puddles[i];
        const float here = lightAt(puddle.at);

        if (here < 0.03f)
            continue;

        std::size_t best = 0;
        float bestWeight = -1.0f;

        for (std::size_t l = 0; l < config::DeckLights.size(); ++l)
        {
            const config::DeckLight& light = config::DeckLights[l];
            const float away = (puddle.at - light.pool).length();
            const float weight = lampNow(l) / std::max(40.0f, away);

            if (weight > bestWeight)
            {
                bestWeight = weight;
                best = l;
            }
        }

        const sf::Vector2f toward = (config::DeckLights[best].mast - puddle.at).normalized();
        const float wobble = static_cast<float>(i);

        pool(m_glow, puddle.at + toward * (puddle.radii.y * 0.42f),
             { puddle.radii.x * 0.82f * (1.0f + 0.07f * std::sin(m_time * 0.80f + wobble * 1.3f)),
               puddle.radii.y * 0.50f * (1.0f - 0.05f * std::sin(m_time * 1.10f + wobble * 0.7f)) },
             0.0f, config::DeckFlood, here * 0.22f, 4, 20);
    }

    // The shack window throws a slot, because a window is a slot.
    const sf::Color warm = fade(config::DeckShack, 0.18f);
    const sf::Color gone = fade(config::DeckShack, 0.0f);

    m_glow.append(sf::Vertex{ { 292.0f, 552.0f }, warm });
    m_glow.append(sf::Vertex{ { 524.0f, 496.0f }, gone });
    m_glow.append(sf::Vertex{ { 524.0f, 656.0f }, gone });
    m_glow.append(sf::Vertex{ { 292.0f, 552.0f }, warm });
    m_glow.append(sf::Vertex{ { 524.0f, 656.0f }, gone });
    m_glow.append(sf::Vertex{ { 292.0f, 600.0f }, warm });

    pool(m_glow, { 292.0f, 576.0f }, { 34.0f, 30.0f }, 0.0f, config::DeckShack, 0.30f);

    const float moon = station(Post::Bell).open ? 0.32f + 0.09f * std::sin(m_time * 2.1f)
                                                : 0.08f;

    pool(m_glow, Pool, { 150.0f, 150.0f }, 0.0f, config::DeckMoon, moon);
    pool(m_glow, Pool, { 48.0f, 48.0f }, 0.0f, config::DeckMoon, moon * 0.9f);

    pool(m_glow, m_diver, { 54.0f, 54.0f }, 0.0f, config::DeckHelmet, 0.14f);
}

void Deck::drawShadows()
{
    for (const sf::Vector2f& at : Crates)
        shadowFor(at, 42.0f, 118.0f);

    // Everything bolted down or dropped on the deck. Walk-over gear - chain,
    // ballast slabs, bitts - gets a contact shadow and no cast: a thing you
    // step over is a thing lying flat on the plate, and a cast shadow would
    // stand it up.
    for (std::size_t i = 0; i < config::DeckProps.size(); ++i)
    {
        const config::DeckProp& prop = config::DeckProps[i];
        const sf::FloatRect bounds = m_props[i].getGlobalBounds();
        const float across = std::min(bounds.size.x, bounds.size.y);

        if (prop.solid > 0.0f)
            shadowFor(prop.at, across, 112.0f);
        else
            oval(prop.at + sf::Vector2f{ 0.0f, 3.0f }, { across * 0.40f, across * 0.30f },
                 sf::Color(0, 0, 0, 90), 16);
    }

    shadowFor(station(Post::Crane).at, 112.0f, 110.0f);
    shadowFor(station(Post::Bench).at, 84.0f, 110.0f);

    // Nothing for the sub: it is over open water and casts onto the sea, which
    // drawSub handles itself. The gantry feet are on the plate though.
    if (m_subStats)
    {
        for (const float x : { config::DeckGantryLeft, config::DeckGantryRight })
            shadowFor({ x, config::DeckGantryIn }, 34.0f, 116.0f);
    }

    shadowFor(m_diver, 44.0f, 130.0f);
}

void Deck::drawStations(sf::RenderTarget& target)
{
    target.draw(m_mesh);
    m_mesh.clear();

    for (std::size_t i = 0; i < m_props.size(); ++i)
    {
        m_props[i].setColor(litTint(config::DeckNightTint,
                                    lightAt(config::DeckProps[i].at)));
        target.draw(m_props[i]);
    }

    for (const sf::Vector2f& at : Crates)
    {
        m_crate.setPosition(at);
        m_crate.setColor(litTint(config::DeckNightTint, lightAt(at)));
        target.draw(m_crate);
    }

    m_crane.setColor(litTint(config::DeckNightTint, lightAt(station(Post::Crane).at)));
    m_bench.setColor(litTint(config::DeckNightTint, lightAt(station(Post::Bench).at)));
    target.draw(m_crane);
    target.draw(m_bench);

    // The bell rides the swell a little, hanging in its hole.
    m_bell.setPosition(Pool + sf::Vector2f{ 0.0f, std::sin(m_time * 0.9f) * 3.0f });
    m_bell.setColor(litTint(config::DeckNightBell, lightAt(Pool)));
    target.draw(m_bell);
}

void Deck::drawSub(sf::RenderTarget& target)
{
    if (!m_subStats)
        return;

    const sf::Vector2f at = config::DeckSubAt;
    const float light = std::max(lightAt(at), 0.30f);

    // Kept dark on purpose. This is the frame the thing hangs in, not the thing
    // - lit up like the plate it becomes a white cage with a sub somewhere
    // behind it, which is what the first pass of this was.
    const sf::Color steel = litTint(sf::Color(18, 24, 31), light * 0.55f);
    const sf::Color edge = litTint(sf::Color(54, 66, 78), light * 0.8f);
    const sf::Color cable = litTint(sf::Color(64, 76, 88), light);

    const float L = config::DeckGantryLeft;
    const float R = config::DeckGantryRight;
    const float out = config::DeckGantryOut;
    const float in = config::DeckGantryIn;
    const float rail = config::DeckGantryRail;

    // --- what the sub is hanging over --------------------------------------
    //
    // There is no deck under it, so there is no contact shadow either: what is
    // down there is the sea. A soft dark patch on the swell instead, which is
    // the one thing that says the drop below it is real.
    oval(at + sf::Vector2f{ 0.0f, 18.0f }, { 108.0f, 46.0f }, sf::Color(0, 0, 0, 108), 24);
    oval(at + sf::Vector2f{ 0.0f, 26.0f }, { 144.0f, 60.0f }, sf::Color(0, 0, 0, 54), 24);

    // --- the frame it hangs in ---------------------------------------------
    //
    // Seen from straight down, a launch gantry is two rails running out over
    // the water with a beam at each end. It is drawn UNDER the sub: the steel
    // is really above, but a lattice painted over the hull turns the hero
    // object into something behind a cage, and the whole reason for moving the
    // sub out here was to stop it being the thing everything is drawn on.
    const auto strut = [this](sf::Vector2f a, sf::Vector2f b, float wide, sf::Color colour) {
        smear(m_mesh, a, b, wide, wide, colour, colour);
    };

    // Bracing only on the inboard run, which is the part of the frame the sub
    // is not hanging in.
    for (int i = 0; i < 2; ++i)
    {
        const float y0 = config::DeckGantryBraceFrom
                         + (in - config::DeckGantryBraceFrom) * (static_cast<float>(i) / 2.0f);
        const float y1 = config::DeckGantryBraceFrom
                         + (in - config::DeckGantryBraceFrom) * (static_cast<float>(i + 1) / 2.0f);

        strut({ L, y0 }, { R, y1 }, rail * 0.34f, steel);
        strut({ R, y0 }, { L, y1 }, rail * 0.34f, steel);
    }

    // The two rails, out over the water and back onto the plate, each with a
    // lit inboard edge so they read as box section rather than as tape.
    for (const float x : { L, R })
    {
        strut({ x, out }, { x, in }, rail, steel);
        strut({ x + (x < 640.0f ? rail * 0.42f : -rail * 0.42f), out },
              { x + (x < 640.0f ? rail * 0.42f : -rail * 0.42f), in }, rail * 0.20f, edge);
    }

    // A beam at each end, and the one in between that the slings hang off.
    strut({ L - rail, out + 5.0f }, { R + rail, out + 5.0f }, rail * 0.85f, steel);
    strut({ L - rail, config::DeckGantryBeam }, { R + rail, config::DeckGantryBeam },
          rail * 1.05f, steel);
    strut({ L - rail, in - 10.0f }, { R + rail, in - 10.0f }, rail * 0.85f, steel);

    // The winch head at the inboard end, and a bolt at each foot.
    disc({ (L + R) * 0.5f, in - 10.0f }, 12.0f, steel, 20);
    disc({ (L + R) * 0.5f, in - 10.0f }, 6.0f, edge, 16);

    for (const float x : { L, R })
        disc({ x, in }, 8.0f, steel, 14);

    target.draw(m_mesh);
    m_mesh.clear();

    // --- the machine -------------------------------------------------------
    //
    // Composed exactly as it is in the water: slung hardware underneath, the
    // hull, then plating and guns over the top.
    m_subKit.drawUnder(target);
    target.draw(m_subHull);
    m_subKit.drawOver(target);

    // --- and the two slings holding it -------------------------------------
    //
    // The only part of the frame that does go over the hull, because a cable
    // landing on it is what says the thing is hanging rather than resting.
    for (const float dx : { -54.0f, 54.0f })
    {
        const sf::Vector2f hook{ at.x + dx, at.y - 6.0f };
        strut({ at.x + dx, config::DeckGantryBeam }, hook, rail * 0.26f, cable);
        disc(hook, 4.5f, cable, 12);
        disc(hook, 2.0f, steel, 10);
    }

    target.draw(m_mesh);
    m_mesh.clear();
}

void Deck::drawDiver(sf::RenderTarget& target)
{
    // A step is a boot swinging forward and the body rocking a little onto it.
    const sf::Vector2f along = unit(m_face.asDegrees());
    const sf::Vector2f side{ -along.y, along.x };
    const float swing = std::sin(m_stride) * 9.0f * m_effort;
    const float roll = std::cos(m_stride) * 2.5f * m_effort;

    // Never fully dark: a man standing in the one unlit strip has to be a
    // shape rather than a rumour, or the player loses him and blames the game.
    const sf::Color tint = litTint(config::DeckNightDiver,
                                   std::max(lightAt(m_diver), 0.18f));

    for (const float foot : { -1.0f, 1.0f })
    {
        m_boot.setPosition(m_diver + side * (foot * 8.0f) + along * (swing * foot));
        m_boot.setRotation(m_face);
        m_boot.setColor(tint);
        target.draw(m_boot);
    }

    m_body.setPosition(m_diver - along * (roll * 0.4f));
    m_body.setRotation(m_face + sf::degrees(roll));
    m_body.setColor(tint);
    target.draw(m_body);
}

void Deck::drawWeather()
{
    // The cone of each flood, which is the light you can see because the air
    // between the lamp and the deck is full of falling water.
    for (std::size_t i = 0; i < config::DeckLights.size(); ++i)
    {
        const config::DeckLight& light = config::DeckLights[i];
        const float now = lampNow(i);

        smear(m_glow, light.mast,
              light.pool + unit(light.turn) * (light.radii.x * 0.65f), 10.0f,
              light.radii.y * 0.72f, fade(config::DeckFlood, 0.085f * now),
              fade(config::DeckFlood, 0.0f));
    }

    // The helmet lamp: the sub's cone, shrunk, so the two read as the same
    // piece of kit. In the air rather than on the floor, so it goes over the
    // crates instead of under them.
    const sf::Vector2f face = unit(m_face.asDegrees());
    const sf::Vector2f side{ -face.y, face.x };

    constexpr std::array<std::pair<float, float>, 5> band{ {
        { 0.0f, 0.34f }, { 44.0f, 1.00f }, { 78.0f, 0.62f }, { 114.0f, 0.30f },
        { 150.0f, 0.00f },
    } };

    for (std::size_t b = 0; b + 1 < band.size(); ++b)
    {
        for (int step = 0; step < 10; ++step)
        {
            const float u0 = -1.0f + 2.0f * static_cast<float>(step) / 10.0f;
            const float u1 = -1.0f + 2.0f * static_cast<float>(step + 1) / 10.0f;

            const auto edge = [](float u) {
                return std::pow(std::cos(std::abs(u) * 1.5707963f), 1.35f);
            };

            const auto at = [&](std::size_t which, float u) {
                const float spread = std::tan(26.0f * 0.01745329f) * band[which].first;
                return m_diver + face * band[which].first + side * (spread * u);
            };

            const sf::Color c0 = fade(config::DeckHelmet, band[b].second * edge(u0) * 0.30f);
            const sf::Color c1 = fade(config::DeckHelmet, band[b].second * edge(u1) * 0.30f);
            const sf::Color d0 = fade(config::DeckHelmet, band[b + 1].second * edge(u0) * 0.30f);
            const sf::Color d1 = fade(config::DeckHelmet, band[b + 1].second * edge(u1) * 0.30f);

            m_glow.append(sf::Vertex{ at(b, u0), c0 });
            m_glow.append(sf::Vertex{ at(b + 1, u0), d0 });
            m_glow.append(sf::Vertex{ at(b + 1, u1), d1 });

            m_glow.append(sf::Vertex{ at(b, u0), c0 });
            m_glow.append(sf::Vertex{ at(b + 1, u1), d1 });
            m_glow.append(sf::Vertex{ at(b, u1), c1 });
        }
    }

    // And the rain. Two different wrap periods for x and y is the one thing
    // that stops a field of streaks resolving into a repeating pattern, and it
    // is the difference between rain and static.
    const sf::Vector2f fall = unit(config::DeckRainAngle + 3.5f * std::sin(m_time * 0.37f));

    for (int i = 0; i < config::DeckRainCount; ++i)
    {
        const float seed = static_cast<float>(i) * 1.7f;
        const float speed = config::DeckRainFall * (0.78f + 0.44f * wiggle(seed + 0.11f));

        sf::Vector2f head{ wiggle(seed + 0.31f) * (W + 240.0f) + fall.x * m_time * speed,
                           wiggle(seed + 0.79f) * (H + 340.0f) + fall.y * m_time * speed };

        head.x = wrap(head.x + 120.0f, W + 240.0f) - 120.0f;
        head.y = wrap(head.y + 160.0f, H + 340.0f) - 160.0f;

        const float here = lightAt(head);
        const float length = 24.0f + 14.0f * wiggle(seed + 0.53f);

        // Rain in a beam is the colour of the beam, which is why a floodlit
        // deck at night reads as warm streaks over a cold one.
        sf::Color lit{
            static_cast<std::uint8_t>(config::DeckRain.r
                                      + (config::DeckFlood.r - config::DeckRain.r) * here),
            static_cast<std::uint8_t>(config::DeckRain.g
                                      + (config::DeckFlood.g - config::DeckRain.g) * here),
            static_cast<std::uint8_t>(config::DeckRain.b
                                      + (config::DeckFlood.b - config::DeckRain.b) * here),
            static_cast<std::uint8_t>(38.0f + 128.0f * here)
        };

        sf::Color tail = lit;
        tail.a = 0;

        smear(m_glow, head, head - fall * length, 0.6f, 0.6f, lit, tail);

        // A splash is only ever seen where a lamp is: you never watch rain hit
        // a dark deck, only a lit one.
        if (i % 3 != 0 || here < 0.05f)
            continue;

        if (head.x < PlateLeft + 6.0f || head.x > PlateRight - 6.0f
            || head.y < PlateTop + 6.0f || head.y > PlateBottom - 6.0f)
        {
            continue;
        }

        const float phase = wrap(m_time * 2.6f + wiggle(seed + 0.91f), 1.0f);
        const float radius = 2.0f + 8.0f * phase;

        ring(m_glow, head, radius, radius + 1.6f,
             fade(config::DeckRain, (1.0f - phase) * (0.07f + 0.30f * here)), 8);
    }
}

void Deck::drawVignette()
{
    // The frame goes away at the corners. Ring quads out to well past the
    // screen, because a vignette that stops inside the window has a visible
    // edge and a visible edge is worse than no vignette.
    constexpr int Steps = 36;
    constexpr sf::Vector2f Middle{ 640.0f, 360.0f };

    const auto edge = [](float t, float degrees) {
        const sf::Vector2f d = unit(degrees);
        return Middle + sf::Vector2f{ d.x * (470.0f + 370.0f * t), d.y * (300.0f + 245.0f * t) };
    };

    for (int i = 0; i < Steps; ++i)
    {
        const float a0 = 360.0f * static_cast<float>(i) / Steps;
        const float a1 = 360.0f * static_cast<float>(i + 1) / Steps;

        const sf::Color inner = fade(config::DeckVignette, 0.0f);
        const sf::Color outer = fade(config::DeckVignette, 0.70f);

        m_mesh.append(sf::Vertex{ edge(0.0f, a0), inner });
        m_mesh.append(sf::Vertex{ edge(1.0f, a0), outer });
        m_mesh.append(sf::Vertex{ edge(1.0f, a1), outer });

        m_mesh.append(sf::Vertex{ edge(0.0f, a0), inner });
        m_mesh.append(sf::Vertex{ edge(1.0f, a1), outer });
        m_mesh.append(sf::Vertex{ edge(0.0f, a1), inner });

        const sf::Vector2f d0 = unit(a0);
        const sf::Vector2f d1 = unit(a1);
        const sf::Vector2f far0 = Middle + sf::Vector2f{ d0.x * 2688.0f, d0.y * 1744.0f };
        const sf::Vector2f far1 = Middle + sf::Vector2f{ d1.x * 2688.0f, d1.y * 1744.0f };

        m_mesh.append(sf::Vertex{ edge(1.0f, a0), outer });
        m_mesh.append(sf::Vertex{ far0, outer });
        m_mesh.append(sf::Vertex{ far1, outer });

        m_mesh.append(sf::Vertex{ edge(1.0f, a0), outer });
        m_mesh.append(sf::Vertex{ far1, outer });
        m_mesh.append(sf::Vertex{ edge(1.0f, a1), outer });
    }
}

void Deck::drawSigns()
{
    for (std::size_t i = 0; i < m_stations.size(); ++i)
    {
        const Station& post = m_stations[i];
        const bool here = m_at == static_cast<Post>(i);

        // Over the thing it names, except the bell, whose sign goes under it:
        // above the hole is where a man arriving on deck is standing.
        const sf::Vector2f at =
            post.solid > 0.0f ? post.at - sf::Vector2f{ 0.0f, post.solid + 34.0f }
                              : post.at + sf::Vector2f{ 0.0f, PoolRadius + 42.0f };

        const float shine = post.open ? (here ? 1.0f : 0.72f) : 0.34f;
        const sf::Color ink = here ? config::HudBossColor : config::HudTextColor;

        const float nameWidth = pixelfont::measure(post.name, 1.5f);
        const float noteWidth = post.note.empty()
                                    ? 0.0f
                                    : pixelfont::measure(post.note, 1.1f);
        const float widest = std::max(nameWidth, noteWidth);

        // On its own strip of dark. The deck is now a floodlit rig with gear
        // all over it, and a line of dim text laid straight on top of a gas
        // quad cannot be read at all.
        quad({ at.x - widest * 0.5f - 9.0f, at.y - 5.0f },
             { widest + 18.0f, post.note.empty() ? 20.0f : 36.0f },
             sf::Color(0, 0, 0, 140));

        pixelfont::append(m_text, post.name, { at.x - nameWidth * 0.5f, at.y }, 1.5f,
                          fade(ink, shine));

        if (!post.note.empty())
        {
            pixelfont::append(m_text, post.note, { at.x - noteWidth * 0.5f, at.y + 16.0f },
                              1.1f, fade(config::HudTextColor, shine * 0.8f));
        }

        // The prompt only ever appears on the one he is standing at, and it
        // sits under his feet rather than up with the sign, so the eye does
        // not have to leave him to read it.
        if (here && post.open)
        {
            const std::string say = std::string("[E]  ") + post.verb;
            const float width = pixelfont::measure(say, 1.3f);
            const float pulse = 0.72f + 0.28f * std::sin(m_time * 4.4f);

            quad({ m_diver.x - width * 0.5f - 10.0f, m_diver.y + 30.0f }, { width + 20.0f, 22.0f },
                 sf::Color(0, 0, 0, 170));

            pixelfont::append(m_text, say, { m_diver.x - width * 0.5f, m_diver.y + 36.0f }, 1.3f,
                              fade(config::HudBossColor, pulse));
        }
    }
}

void Deck::draw(sf::RenderTarget& target)
{
    m_mesh.clear();
    m_glow.clear();
    m_text.clear();

    sf::RenderStates add;
    add.blendMode = sf::BlendAdd; // light adds to a wet deck, it does not paint over it

    drawSea();
    drawPlate(target);
    drawWet();

    target.draw(m_mesh);
    m_mesh.clear();

    // The rails, each run lit by whatever is standing over its middle.
    const auto run = [this, &target](sf::Vector2f from, float length, float degrees,
                                     sf::Vector2f middle) {
        m_rail.setOrigin({ 0.0f, 0.0f });
        m_rail.setTextureRect(sf::IntRect({ 0, 0 }, { static_cast<int>(length), 20 }));
        m_rail.setPosition(from);
        m_rail.setRotation(sf::degrees(degrees));
        m_rail.setColor(litTint(config::DeckNightTint, lightAt(middle)));
        target.draw(m_rail);
    };

    const float across = PlateRight - PlateLeft;
    const float down = PlateBottom - PlateTop;

    run({ PlateLeft, PlateTop }, across, 0.0f, { 640.0f, PlateTop });
    run({ PlateRight, PlateBottom }, across, 180.0f, { 640.0f, PlateBottom });
    run({ PlateLeft, PlateBottom }, down, -90.0f, { PlateLeft, 370.0f });
    run({ PlateRight, PlateTop }, down, 90.0f, { PlateRight, 370.0f });

    drawLightPass();
    target.draw(m_glow, add);
    m_glow.clear();

    drawShadows();
    drawStations(target);
    drawSub(target);
    drawDiver(target);

    drawWeather();
    target.draw(m_glow, add);
    m_glow.clear();

    drawVignette();
    target.draw(m_mesh);
    m_mesh.clear();

    // The signs are deliberately unlit. A prompt you cannot read because you
    // are standing in the dark is a bug, not atmosphere.
    drawSigns();
    target.draw(m_mesh);
    m_mesh.clear();
    target.draw(m_text);
}
