#include "ui/Deck.h"

#include "core/Assets.h"
#include "core/Config.h"
#include "ui/PixelFont.h"

#include <SFML/Graphics/PrimitiveType.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

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
    constexpr float DiverScale = 1.55f;
    constexpr float BellScale = 1.72f;

    constexpr sf::Color Sea{ 17, 28, 35 };
    constexpr sf::Color Swell{ 44, 66, 76 };
    constexpr sf::Color Foam{ 128, 152, 158 };
    constexpr sf::Color Shadow{ 0, 0, 0, 74 };
    constexpr sf::Color Hole{ 11, 24, 30 };
    constexpr sf::Color Paint{ 176, 148, 52 };
    constexpr sf::Color Lit{ 150, 212, 220 };

    sf::Color fade(sf::Color colour, float alpha)
    {
        colour.a = static_cast<std::uint8_t>(
            std::clamp(alpha, 0.0f, 1.0f) * static_cast<float>(colour.a));
        return colour;
    }

    sf::Vector2f unit(float degrees)
    {
        const float r = degrees * 0.01745329f;
        return { std::cos(r), std::sin(r) };
    }
}

Deck::Deck(const Assets& assets)
    : m_stations{ {
          { { 336.0f, 238.0f }, 74.0f, "THE CRANE", "SELL THE HOLD", {}, true },
          { { 952.0f, 224.0f }, 62.0f, "THE BENCH", "OPEN THE BOARD", {}, true },
          { Pool, 0.0f, "THE BELL", "GO DOWN", {}, true },
      } }
    , m_mesh(sf::PrimitiveType::Triangles)
    , m_text(sf::PrimitiveType::Triangles)
    , m_plate(assets.byPath(config::DeckPlatePath))
    , m_rail(assets.byPath(config::DeckRailPath))
    , m_crane(assets.byPath(config::DeckCranePath))
    , m_bench(assets.byPath(config::DeckBenchPath))
    , m_bell(assets.byPath(config::DeckBellPath))
    , m_crate(assets.byPath(config::DeckCratePath))
    , m_body(assets.byPath(config::DeckDiverPath))
    , m_boot(assets.byPath(config::DeckBootPath))
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

    m_crane.setPosition(station(Post::Crane).at);
    m_bench.setPosition(station(Post::Bench).at);
    m_bell.setPosition(Pool);

    // The jib hangs out over the port side rather than across the deck, which
    // is the only reason the crane is drawn facing the other way.
    m_crane.setRotation(sf::degrees(180.0f));

    // One sprite for the whole floor, and one for a whole run of rail.
    m_plate.setTextureRect(sf::IntRect(
        { 0, 0 }, { static_cast<int>((PlateRight - PlateLeft) / PlateScale),
                    static_cast<int>((PlateBottom - PlateTop) / PlateScale) }));
    m_plate.setScale({ PlateScale, PlateScale });
    m_plate.setPosition({ PlateLeft, PlateTop });

    m_body.setScale({ DiverScale, DiverScale });
    m_boot.setScale({ DiverScale, DiverScale });
    m_bell.setScale({ BellScale, BellScale });

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
    m_stride += pace * deltaTime * config::DeckStride;

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
}

void Deck::keepOnDeck()
{
    m_diver.x = std::clamp(m_diver.x, PlateLeft + Kerb, PlateRight - Kerb);
    m_diver.y = std::clamp(m_diver.y, PlateTop + Kerb, PlateBottom - Kerb);

    // Out of the hole, and out of anything bolted to the deck.
    const auto shove = [this](sf::Vector2f from, float radius) {
        const sf::Vector2f away = m_diver - from;
        const float distance = away.length();

        if (distance >= radius || distance < 0.001f)
            return;

        m_diver = from + away / distance * radius;
        m_speed -= away / distance * std::max(0.0f, m_speed.dot(away / distance));
    };

    shove(Pool, PoolRadius + 22.0f);

    for (const Station& post : m_stations)
    {
        if (post.solid > 0.0f)
            shove(post.at, post.solid);
    }
}

// ---------------------------------------------------------------------------
//  The vocabulary
// ---------------------------------------------------------------------------

void Deck::disc(sf::Vector2f at, float radius, sf::Color colour, int steps)
{
    for (int i = 0; i < steps; ++i)
    {
        const float a0 = 360.0f * static_cast<float>(i) / static_cast<float>(steps);
        const float a1 = 360.0f * static_cast<float>(i + 1) / static_cast<float>(steps);

        m_mesh.append(sf::Vertex{ at, colour });
        m_mesh.append(sf::Vertex{ at + unit(a0) * radius, colour });
        m_mesh.append(sf::Vertex{ at + unit(a1) * radius, colour });
    }
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

void Deck::ring(sf::Vector2f at, float inner, float outer, sf::Color colour, int steps)
{
    for (int i = 0; i < steps; ++i)
    {
        const float a0 = 360.0f * static_cast<float>(i) / static_cast<float>(steps);
        const float a1 = 360.0f * static_cast<float>(i + 1) / static_cast<float>(steps);

        const sf::Vector2f d0 = unit(a0);
        const sf::Vector2f d1 = unit(a1);

        m_mesh.append(sf::Vertex{ at + d0 * inner, colour });
        m_mesh.append(sf::Vertex{ at + d0 * outer, colour });
        m_mesh.append(sf::Vertex{ at + d1 * outer, colour });

        m_mesh.append(sf::Vertex{ at + d0 * inner, colour });
        m_mesh.append(sf::Vertex{ at + d1 * outer, colour });
        m_mesh.append(sf::Vertex{ at + d1 * inner, colour });
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

// ---------------------------------------------------------------------------
//  Drawing
// ---------------------------------------------------------------------------

void Deck::drawSea()
{
    quad({ 0.0f, 0.0f }, { W, H }, Sea);

    // Swell. Long, slow, and going one way, which is all it takes for the deck
    // to read as floating rather than as a room with a grey border.
    for (int i = 0; i < 30; ++i)
    {
        const float y = static_cast<float>(i) * 26.0f - 20.0f;
        const float phase = m_time * 0.55f + static_cast<float>(i) * 0.7f;
        const float lift = std::sin(phase) * 7.0f;
        const float strength = 0.30f + 0.26f * std::sin(phase * 0.5f + 1.3f);

        quad({ -40.0f, y + lift }, { W + 80.0f, 7.0f }, fade(Swell, strength));
        quad({ -40.0f, y + lift + 9.0f }, { W + 80.0f, 2.0f },
             fade(Foam, strength * 0.22f));
    }

    // Where the sea breaks against the rig. It is only a thin light edge, but
    // without it the plate looks laid on top of the water rather than in it.
    const float surge = 3.0f + 2.0f * std::sin(m_time * 1.7f);

    quad({ PlateLeft - surge - 5.0f, PlateTop - surge - 5.0f },
         { PlateRight - PlateLeft + (surge + 5.0f) * 2.0f, surge + 5.0f }, fade(Foam, 0.5f));
    quad({ PlateLeft - surge - 5.0f, PlateBottom },
         { PlateRight - PlateLeft + (surge + 5.0f) * 2.0f, surge + 5.0f }, fade(Foam, 0.5f));
    quad({ PlateLeft - surge - 5.0f, PlateTop },
         { surge + 5.0f, PlateBottom - PlateTop }, fade(Foam, 0.4f));
    quad({ PlateRight, PlateTop }, { surge + 5.0f, PlateBottom - PlateTop }, fade(Foam, 0.4f));
}

void Deck::drawPlate(sf::RenderTarget& target)
{
    target.draw(m_mesh);
    m_mesh.clear();

    target.draw(m_plate);

    // A painted lane from the bell to each station, because the first thing a
    // player does up here is ask where they are supposed to go.
    for (const Station& post : m_stations)
    {
        if (post.solid <= 0.0f)
            continue;

        const sf::Vector2f from = Pool;
        const sf::Vector2f along = (post.at - from).normalized();
        const sf::Vector2f side{ -along.y, along.x };
        const float start = PoolRadius + 26.0f;
        const float end = (post.at - from).length() - post.solid - 8.0f;

        for (float travel = start; travel < end; travel += 46.0f)
        {
            const sf::Vector2f at = from + along * travel;
            const float length = std::min(26.0f, end - travel);

            m_mesh.append(sf::Vertex{ at - side * 5.0f, fade(Paint, 0.22f) });
            m_mesh.append(sf::Vertex{ at + along * length - side * 5.0f, fade(Paint, 0.22f) });
            m_mesh.append(sf::Vertex{ at + along * length + side * 5.0f, fade(Paint, 0.22f) });

            m_mesh.append(sf::Vertex{ at - side * 5.0f, fade(Paint, 0.22f) });
            m_mesh.append(sf::Vertex{ at + along * length + side * 5.0f, fade(Paint, 0.22f) });
            m_mesh.append(sf::Vertex{ at + side * 5.0f, fade(Paint, 0.22f) });
        }
    }

    // The hole, and the sea moving about in the bottom of it.
    disc(Pool, PoolRadius + 14.0f, sf::Color(52, 58, 62), 34);
    disc(Pool, PoolRadius, Hole, 34);
    ring(Pool, PoolRadius * 0.62f, PoolRadius * 0.70f,
         fade(Swell, 0.5f + 0.3f * std::sin(m_time * 1.4f)), 30);
    ring(Pool, PoolRadius * 0.86f, PoolRadius * 0.92f,
         fade(Swell, 0.4f + 0.3f * std::sin(m_time * 1.4f + 2.0f)), 30);

    target.draw(m_mesh);
    m_mesh.clear();

    // Rails, all the way round the edge of the plate.
    const auto run = [this, &target](sf::Vector2f from, float length, float degrees) {
        m_rail.setOrigin({ 0.0f, 0.0f });
        m_rail.setTextureRect(sf::IntRect({ 0, 0 }, { static_cast<int>(length), 20 }));
        m_rail.setPosition(from);
        m_rail.setRotation(sf::degrees(degrees));
        target.draw(m_rail);
    };

    const float across = PlateRight - PlateLeft;
    const float down = PlateBottom - PlateTop;

    run({ PlateLeft, PlateTop }, across, 0.0f);
    run({ PlateRight, PlateBottom }, across, 180.0f);
    run({ PlateLeft, PlateBottom }, down, -90.0f);
    run({ PlateRight, PlateTop }, down, 90.0f);
}

void Deck::drawStations(sf::RenderTarget& target)
{
    // Crates by the crane, which is where a hold that has been sold ends up.
    const sf::Vector2f stack[] = {
        { 232.0f, 402.0f }, { 288.0f, 436.0f }, { 372.0f, 420.0f },
        { 1042.0f, 480.0f }, { 988.0f, 520.0f },
    };

    // A shadow is the shape of the thing above it, not a circle under it. The
    // difference is the whole reason the crane looked like it was standing in
    // a hole rather than on a deck.
    const auto cast = [this](const sf::Sprite& sprite) {
        const sf::FloatRect bounds = sprite.getGlobalBounds();
        const float across = std::min(bounds.size.x, bounds.size.y);

        // Off the short side of the sprite rather than its whole box: half of
        // the crane is a jib hanging over the sea, and a shadow the length of
        // that is a shadow of something that is not standing there.
        oval(bounds.position + bounds.size * 0.5f + sf::Vector2f{ 7.0f, 9.0f },
             { across * 0.44f, across * 0.38f }, Shadow, 20);
    };

    for (const sf::Vector2f& at : stack)
    {
        m_crate.setPosition(at);
        cast(m_crate);
        target.draw(m_mesh);
        m_mesh.clear();

        target.draw(m_crate);
    }

    cast(m_crane);
    cast(m_bench);

    target.draw(m_mesh);
    m_mesh.clear();

    target.draw(m_crane);
    target.draw(m_bench);

    // The bell rides the swell a little, hanging in its hole.
    m_bell.setPosition(Pool + sf::Vector2f{ 0.0f, std::sin(m_time * 0.9f) * 3.0f });
    target.draw(m_bell);

    // And it is lit from inside when it will take you, dark when it will not.
    const Station& bell = station(Post::Bell);
    const float glow = bell.open ? 0.34f + 0.16f * std::sin(m_time * 2.1f) : 0.08f;

    disc(Pool, 34.0f, fade(bell.open ? Lit : sf::Color(90, 96, 100), glow), 22);
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

    oval(m_diver + sf::Vector2f{ 6.0f, 8.0f }, { 24.0f, 21.0f }, Shadow, 18);
    target.draw(m_mesh);
    m_mesh.clear();

    for (const float foot : { -1.0f, 1.0f })
    {
        m_boot.setPosition(m_diver + side * (foot * 8.0f) + along * (swing * foot));
        m_boot.setRotation(m_face);
        target.draw(m_boot);
    }

    m_body.setPosition(m_diver - along * (roll * 0.4f));
    m_body.setRotation(m_face + sf::degrees(roll));
    target.draw(m_body);
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
        pixelfont::append(m_text, post.name, { at.x - nameWidth * 0.5f, at.y }, 1.5f,
                          fade(ink, shine));

        if (!post.note.empty())
        {
            const float noteWidth = pixelfont::measure(post.note, 1.1f);
            pixelfont::append(m_text, post.note,
                              { at.x - noteWidth * 0.5f, at.y + 16.0f }, 1.1f,
                              fade(config::HudTextColor, shine * 0.8f));
        }

        // The prompt only ever appears on the one he is standing at, and it
        // sits under his feet rather than up with the sign, so the eye does
        // not have to leave him to read it.
        if (here && post.open)
        {
            const std::string say = std::string("[E]  ") + post.verb;
            const float width = pixelfont::measure(say, 1.3f);
            const float pulse = 0.72f + 0.28f * std::sin(m_time * 4.4f);

            quad({ m_diver.x - width * 0.5f - 10.0f, m_diver.y + 30.0f },
                 { width + 20.0f, 22.0f }, sf::Color(0, 0, 0, 150));

            pixelfont::append(m_text, say,
                              { m_diver.x - width * 0.5f, m_diver.y + 36.0f }, 1.3f,
                              fade(config::HudBossColor, pulse));
        }
    }
}

void Deck::draw(sf::RenderTarget& target)
{
    m_mesh.clear();
    m_text.clear();

    drawSea();
    drawPlate(target);
    drawStations(target);
    drawDiver(target);
    drawSigns();

    target.draw(m_mesh);
    target.draw(m_text);
}
