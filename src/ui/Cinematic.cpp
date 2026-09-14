#include "ui/Cinematic.h"

#include "core/Assets.h"
#include "core/Config.h"
#include "ui/PixelFont.h"

#include <SFML/Graphics/PrimitiveType.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace
{
    constexpr float W = static_cast<float>(config::WindowWidth);
    constexpr float H = static_cast<float>(config::WindowHeight);

    // How tall the black bars are. Wide letterbox on purpose: it is the one
    // signal every player already knows for "this is not a thing you are
    // playing right now".
    constexpr float Letterbox = 92.0f;
    constexpr float Top = Letterbox;
    constexpr float Bottom = H - Letterbox;
    constexpr float MidY = H * 0.5f;

    constexpr sf::Color Deep{ 4, 9, 13 };
    constexpr sf::Color Water{ 12, 30, 40 };
    constexpr sf::Color Night{ 20, 26, 36 };
    constexpr sf::Color Sky{ 118, 150, 160 };
    constexpr sf::Color Steel{ 96, 108, 116 };
    constexpr sf::Color Lamp{ 128, 214, 224 };
    constexpr sf::Color Ember{ 236, 150, 96 };

    sf::Color fade(sf::Color colour, float alpha)
    {
        colour.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
        return colour;
    }

    sf::Color dim(sf::Color colour, float amount)
    {
        amount = std::clamp(amount, 0.0f, 1.0f);
        return { static_cast<std::uint8_t>(static_cast<float>(colour.r) * amount),
                 static_cast<std::uint8_t>(static_cast<float>(colour.g) * amount),
                 static_cast<std::uint8_t>(static_cast<float>(colour.b) * amount), colour.a };
    }

    sf::Vector2f unit(float degrees)
    {
        const float r = degrees * 0.01745329f;
        return { std::cos(r), std::sin(r) };
    }

    // Ease in and out, so nothing in a shot starts or stops abruptly. Cheap,
    // and the difference between a camera move and a slide transition.
    float ease(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    // A repeatable wobble, so the same seed gives the same rock every time the
    // reel is watched.
    float wiggle(float seed)
    {
        const float x = std::sin(seed * 12.9898f) * 43758.5453f;
        return x - std::floor(x);
    }
}

Cinematic::Cinematic(const Assets& assets)
    : m_mesh(sf::PrimitiveType::Triangles)
    , m_text(sf::PrimitiveType::Triangles)
    , m_rig(assets.byPath(config::ReelRigPath))
    , m_bell(assets.byPath(config::ReelBellPath))
    , m_wall(assets.byPath(config::ReelWallPath))
    , m_station(assets.byPath(config::ReelStationPath))
    , m_beast(assets.byPath(config::ReelBeastPath))
    , m_diver(assets.byPath(config::ReelDiverPath))
    , m_crate(assets.byPath(config::DeckCratePath))
    , m_crane(assets.byPath(config::DeckCranePath))
{
    m_fill.setFillColor(sf::Color::Black);
}

void Cinematic::begin(Reel reel, const std::string& caption)
{
    m_shots.clear();
    m_caption = caption;

    if (reel == Reel::Down)
    {
        // Two seconds of rock going past. Anything longer than that between
        // one floor and the next is a loading screen with ambitions.
        m_shots = {
            { Scene::Fall, 2.3f, nullptr },
        };
    }
    else if (reel == Reel::Up)
    {
        m_shots = {
            { Scene::Climb, 2.3f, nullptr },
        };
    }
    else if (reel == Reel::Descent)
    {
        m_shots = {
            { Scene::Rig,     4.6f, "NERITH STATION STOPPED ANSWERING ELEVEN DAYS AGO." },
            { Scene::Lower,   4.4f, "THE COMPANY PUT A SUB IN THE WATER. IT FOUND NOTHING WRONG." },
            { Scene::Fall,    4.2f, "THEN A SECOND. THEN A THIRD." },
            { Scene::Pass,    4.6f, "ALL THREE STOPPED TRANSMITTING AT THE SAME DEPTH." },
            { Scene::Station, 4.6f, "THE STATION IS STILL DOWN THERE, AND IT IS NOT DAMAGED." },
            { Scene::Hatches, 4.6f, "EVERY HATCH ON IT IS OPEN FROM THE INSIDE." },
            { Scene::Trench,  4.8f, "AND THE TRENCH UNDERNEATH IT IS NOT A TRENCH." },
            { Scene::Mark,    4.2f, nullptr },
        };
    }
    else
    {
        m_shots = {
            { Scene::Hatch, 4.4f, "THE FLOOR OF THE TRENCH CAME OPEN." },
            { Scene::Climb, 4.4f, "EVERYTHING THAT LIVED IN IT WENT UP PAST YOU." },
            { Scene::Dawn,  4.4f, "YOU SURFACED AT FIRST LIGHT, WHICH NOBODY ELSE HAS DONE." },
            { Scene::Count, 4.6f, "THEY COUNTED WHAT YOU BROUGHT UP AND THEY PAID FOR IT." },
            { Scene::Wake,  5.0f, "NOT FOR LONG." },
            { Scene::Mark,  4.4f, nullptr },
        };
    }

    m_shot = 0;
    m_time = 0.0f;
    m_total = 0.0f;
    m_finished = m_shots.empty();
}

void Cinematic::update(float deltaTime)
{
    if (m_finished)
        return;

    m_time += deltaTime;
    m_total += deltaTime;

    while (!m_finished && m_time >= current().seconds)
    {
        m_time -= current().seconds;

        if (++m_shot >= m_shots.size())
            m_finished = true;
    }
}

void Cinematic::skip()
{
    // The whole thing, not the current shot. Somebody pressing a key during a
    // cutscene wants to be playing, not one line further into it.
    m_finished = true;
}

const Cinematic::Shot& Cinematic::current() const
{
    return m_shots[std::min(m_shot, m_shots.size() - 1)];
}

float Cinematic::shotTime() const
{
    return std::clamp(m_time / std::max(0.01f, current().seconds), 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
//  The vocabulary
// ---------------------------------------------------------------------------

void Cinematic::disc(sf::Vector2f at, float radius, sf::Color colour, int steps)
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

void Cinematic::glow(sf::Vector2f at, float radius, sf::Color colour, float strength, int steps)
{
    const sf::Color core = fade(colour, strength);
    const sf::Color rim = fade(colour, 0.0f);

    for (int i = 0; i < steps; ++i)
    {
        const float a0 = 360.0f * static_cast<float>(i) / static_cast<float>(steps);
        const float a1 = 360.0f * static_cast<float>(i + 1) / static_cast<float>(steps);

        m_mesh.append(sf::Vertex{ at, core });
        m_mesh.append(sf::Vertex{ at + unit(a0) * radius, rim });
        m_mesh.append(sf::Vertex{ at + unit(a1) * radius, rim });
    }
}

void Cinematic::bar(sf::Vector2f from, sf::Vector2f to, float width, sf::Color a, sf::Color b)
{
    const sf::Vector2f along = to - from;
    if (along.lengthSquared() < 0.0001f)
        return;

    const sf::Vector2f side = sf::Vector2f{ -along.y, along.x }.normalized() * width;

    m_mesh.append(sf::Vertex{ from - side, a });
    m_mesh.append(sf::Vertex{ to - side, b });
    m_mesh.append(sf::Vertex{ to + side, b });

    m_mesh.append(sf::Vertex{ from - side, a });
    m_mesh.append(sf::Vertex{ to + side, b });
    m_mesh.append(sf::Vertex{ from + side, a });
}

void Cinematic::wedge(sf::Vector2f from, sf::Vector2f along, float length, float spread,
                      sf::Color core, sf::Color edge)
{
    const sf::Vector2f side{ -along.y, along.x };

    m_mesh.append(sf::Vertex{ from, core });
    m_mesh.append(sf::Vertex{ from + along * length - side * spread, edge });
    m_mesh.append(sf::Vertex{ from + along * length + side * spread, edge });
}

void Cinematic::hull(sf::Vector2f at, float size, float lean, sf::Color colour)
{
    // The sub, as a silhouette. It is never more than a shape in these: a
    // cutscene that stops to show off the model is a cutscene about the model.
    const sf::Vector2f nose = unit(lean);
    const sf::Vector2f side{ -nose.y, nose.x };

    m_mesh.append(sf::Vertex{ at + nose * (size * 1.5f), colour });
    m_mesh.append(sf::Vertex{ at - nose * (size * 0.9f) - side * (size * 0.62f), colour });
    m_mesh.append(sf::Vertex{ at - nose * (size * 0.9f) + side * (size * 0.62f), colour });

    m_mesh.append(sf::Vertex{ at - nose * (size * 0.9f) - side * (size * 0.62f), colour });
    m_mesh.append(sf::Vertex{ at - nose * (size * 1.7f) - side * (size * 0.22f), colour });
    m_mesh.append(sf::Vertex{ at - nose * (size * 1.7f) + side * (size * 0.22f), colour });

    m_mesh.append(sf::Vertex{ at - nose * (size * 0.9f) - side * (size * 0.62f), colour });
    m_mesh.append(sf::Vertex{ at - nose * (size * 1.7f) + side * (size * 0.22f), colour });
    m_mesh.append(sf::Vertex{ at - nose * (size * 0.9f) + side * (size * 0.62f), colour });
}

void Cinematic::wash(sf::Color top, sf::Color bottom)
{
    m_mesh.append(sf::Vertex{ { 0.0f, 0.0f }, top });
    m_mesh.append(sf::Vertex{ { W, 0.0f }, top });
    m_mesh.append(sf::Vertex{ { W, H }, bottom });

    m_mesh.append(sf::Vertex{ { 0.0f, 0.0f }, top });
    m_mesh.append(sf::Vertex{ { W, H }, bottom });
    m_mesh.append(sf::Vertex{ { 0.0f, H }, bottom });
}

void Cinematic::motes(float phase, sf::Vector2f drift, float length, sf::Color colour, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const float seed = static_cast<float>(i) * 1.7f;
        const float speed = 0.6f + wiggle(seed) * 0.8f;

        float x = wiggle(seed + 0.3f) * (W + 400.0f) + drift.x * phase * speed;
        float y = wiggle(seed + 0.7f) * (H + 400.0f) + drift.y * phase * speed;

        x = std::fmod(std::fmod(x, W + 400.0f) + W + 400.0f, W + 400.0f) - 200.0f;
        y = std::fmod(std::fmod(y, H + 400.0f) + H + 400.0f, H + 400.0f) - 200.0f;

        const sf::Vector2f at{ x, y };
        const sf::Vector2f along = drift.lengthSquared() > 0.001f
                                       ? drift.normalized() * length
                                       : sf::Vector2f{ 0.0f, length };

        bar(at, at + along, 1.2f, colour, fade(colour, 0.0f));
    }
}

void Cinematic::flush(sf::RenderTarget& target)
{
    if (m_mesh.getVertexCount() == 0)
        return;

    target.draw(m_mesh);
    m_mesh.clear();
}

void Cinematic::plate(sf::RenderTarget& target, sf::Sprite& sprite, sf::Vector2f at, float scale,
                      sf::Color tint, float turn)
{
    // Whatever has been put in the mesh belongs behind the plate, and a sprite
    // cannot be batched into it, so the mesh goes out first.
    flush(target);

    const sf::Vector2f size(sprite.getTexture().getSize());

    sprite.setOrigin(size * 0.5f);
    sprite.setPosition(at);
    sprite.setScale({ scale, scale });
    sprite.setRotation(sf::degrees(turn));
    sprite.setColor(tint);
    target.draw(sprite);
}

void Cinematic::rock(sf::RenderTarget& target, float offset, float scale, sf::Color tint)
{
    flush(target);

    const float band = Bottom - Top;

    m_wall.setOrigin({ 0.0f, 0.0f });
    m_wall.setTextureRect(sf::IntRect({ 0, static_cast<int>(offset) },
                                      { static_cast<int>(W / scale) + 2,
                                        static_cast<int>(band / scale) + 2 }));
    m_wall.setScale({ scale, scale });
    m_wall.setPosition({ 0.0f, Top });
    m_wall.setRotation(sf::degrees(0.0f));
    m_wall.setColor(tint);
    target.draw(m_wall);
}

// ---------------------------------------------------------------------------
//  The shots
// ---------------------------------------------------------------------------

void Cinematic::drawRig(sf::RenderTarget& target, float t)
{
    // Night, rain, and a rig with its lights on. The camera pushes in slowly
    // and the bell is already over the side: whatever was decided here was
    // decided before the player arrived.
    wash(Night, sf::Color(10, 14, 20));

    const float sea = MidY + 96.0f;

    m_mesh.append(sf::Vertex{ { 0.0f, sea }, sf::Color(16, 26, 34) });
    m_mesh.append(sf::Vertex{ { W, sea }, sf::Color(16, 26, 34) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(5, 9, 13) });
    m_mesh.append(sf::Vertex{ { 0.0f, sea }, sf::Color(16, 26, 34) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(5, 9, 13) });
    m_mesh.append(sf::Vertex{ { 0.0f, H }, sf::Color(5, 9, 13) });

    for (int i = 0; i < 6; ++i)
    {
        const float y = sea + 14.0f + static_cast<float>(i) * 26.0f;
        const float slide = std::sin(m_total * 0.7f + static_cast<float>(i)) * 20.0f;

        bar({ -40.0f + slide, y }, { W + 40.0f + slide, y + 3.0f }, 2.0f,
            fade(Sky, 0.10f), fade(Sky, 0.04f));
    }

    const float push = 1.12f + 0.16f * ease(t);
    const float pan = -30.0f * ease(t);

    plate(target, m_rig, { W * 0.52f + pan, sea - 84.0f * push }, push,
          sf::Color(148, 158, 166));

    // The bell, already over the side and swinging.
    const float swing = std::sin(m_total * 0.9f) * 12.0f;

    plate(target, m_bell, { W * 0.52f + pan + 300.0f * push + swing, sea - 6.0f },
          0.34f * push, sf::Color(130, 142, 150), swing * 0.2f);

    // The one warm thing in the shot.
    glow({ W * 0.52f + pan + 12.0f * push, sea - 176.0f * push }, 54.0f, Ember, 0.45f);

    motes(m_total, { -90.0f, 780.0f }, 26.0f, fade(Sky, 0.30f), 110);
    flush(target);
}

void Cinematic::drawLower(sf::RenderTarget& target, float t)
{
    // Under, looking up. The whole shot is the daylight getting further away
    // and the bell getting lower in the frame.
    wash(sf::Color(26, 54, 66), Deep);

    const float lid = Top + 40.0f - 70.0f * ease(t);
    const float bright = 1.0f - 0.72f * ease(t);

    for (int i = 0; i < 26; ++i)
    {
        const float x = static_cast<float>(i) * (W / 25.0f);
        const float lift = std::sin(m_total * 1.4f + static_cast<float>(i) * 0.6f) * 7.0f;

        bar({ x, lid + lift }, { x + W / 25.0f, lid + lift }, 5.0f,
            fade(Sky, 0.55f * bright), fade(Sky, 0.55f * bright));
    }

    for (int i = 0; i < 7; ++i)
    {
        const float x = 120.0f + static_cast<float>(i) * 170.0f;

        wedge({ x, lid }, { 0.12f, 1.0f }, 520.0f, 60.0f,
              fade(Sky, 0.14f * bright), fade(Sky, 0.0f));
    }

    const float drop = Top + 120.0f + 300.0f * ease(t);

    bar({ W * 0.5f, lid }, { W * 0.5f, drop }, 2.0f, fade(Steel, 0.5f), fade(Steel, 0.8f));

    plate(target, m_bell, { W * 0.5f, drop }, 0.72f, sf::Color(150, 164, 172),
          std::sin(m_total * 0.8f) * 3.0f);

    glow({ W * 0.5f, drop + 46.0f }, 64.0f, Lamp, 0.30f);
    motes(m_total, { 20.0f, -260.0f }, 14.0f, fade(Sky, 0.22f), 60);
    flush(target);
}

void Cinematic::drawFall(sf::RenderTarget& target, float)
{
    // Rock going past, faster than is comfortable. Two layers at different
    // speeds, because one layer scrolling is a texture and two is a fall.
    wash(sf::Color(14, 20, 24), Deep);
    flush(target);

    rock(target, -m_total * 620.0f, 1.9f, sf::Color(76, 86, 92));
    rock(target, -m_total * 1150.0f + 220.0f, 3.1f, sf::Color(26, 32, 36, 190));

    // Black down the middle, so the plate reads as two walls with a gap
    // between them rather than as wallpaper.
    const auto gloom = [this](float from, float to) {
        m_mesh.append(sf::Vertex{ { from, Top }, fade(sf::Color::Black, 0.0f) });
        m_mesh.append(sf::Vertex{ { to, Top }, fade(sf::Color::Black, 0.84f) });
        m_mesh.append(sf::Vertex{ { to, Bottom }, fade(sf::Color::Black, 0.84f) });
        m_mesh.append(sf::Vertex{ { from, Top }, fade(sf::Color::Black, 0.0f) });
        m_mesh.append(sf::Vertex{ { to, Bottom }, fade(sf::Color::Black, 0.84f) });
        m_mesh.append(sf::Vertex{ { from, Bottom }, fade(sf::Color::Black, 0.0f) });
    };

    gloom(W * 0.20f, W * 0.5f);
    gloom(W * 0.80f, W * 0.5f);

    const float sway = std::sin(m_total * 1.6f) * 16.0f;

    wedge({ W * 0.5f + sway, MidY - 20.0f }, { 0.0f, 1.0f }, 340.0f, 130.0f,
          fade(Lamp, 0.16f), fade(Lamp, 0.0f));
    flush(target);

    plate(target, m_bell, { W * 0.5f + sway, MidY - 30.0f }, 0.46f, sf::Color(140, 152, 160),
          sway * 0.3f);

    motes(m_total, { 0.0f, -1500.0f }, 90.0f, fade(Sky, 0.22f), 70);
    flush(target);
}

void Cinematic::drawPass(sf::RenderTarget& target, float t)
{
    // Something the length of the screen crosses the beam. It is never lit and
    // never explained; the shot is an eye going past, and then the lamp not
    // being there any more.
    wash(sf::Color(8, 14, 18), sf::Color(2, 4, 6));

    const float dying = std::clamp((t - 0.74f) / 0.14f, 0.0f, 1.0f);
    const float beam = 1.0f - dying;

    const sf::Vector2f from{ 200.0f, MidY + 10.0f };

    // The animal first and very nearly black. It is never lit: a thing you can
    // see all of is a thing with a size, and the whole point of this shot is
    // that nobody on board finds out what the size was.
    const float cross = 1500.0f - 2000.0f * ease(t);
    const float rise = std::sin(m_total * 0.7f) * 18.0f;

    flush(target);
    plate(target, m_beast, { cross, MidY + 40.0f + rise }, 1.42f,
          dim(sf::Color(96, 88, 116), 0.14f + 0.16f * beam), -4.0f);

    // Then the beam, over it, because the water between a lamp and a thing is
    // in front of the thing.
    wedge(from, { 1.0f, 0.02f }, 1250.0f * beam, 300.0f * beam,
          fade(Lamp, 0.16f * beam), fade(Lamp, 0.0f));
    wedge(from, { 1.0f, 0.02f }, 900.0f * beam, 150.0f * beam,
          fade(Lamp, 0.10f * beam), fade(Lamp, 0.0f));

    // Its eye, which is the only part of it that is lit at all.
    const sf::Vector2f head{ cross + 380.0f, MidY + 40.0f + rise - 26.0f };

    if (beam > 0.05f)
    {
        glow(head, 70.0f, Ember, 0.30f * beam);
        disc(head, 10.0f, fade(sf::Color(255, 226, 170), 0.9f * beam));
    }

    flush(target);

    plate(target, m_bell, from + sf::Vector2f{ -30.0f, 0.0f }, 0.40f,
          dim(sf::Color(140, 152, 160), 0.35f + 0.65f * beam));

    glow(from, 40.0f, Lamp, 0.6f * beam);
    flush(target);

    if (dying > 0.0f)
    {
        m_fill.setSize({ W, H });
        m_fill.setPosition({ 0.0f, 0.0f });
        m_fill.setFillColor(fade(sf::Color::Black, dying * 0.9f));
        target.draw(m_fill);
    }
}

void Cinematic::drawStation(sf::RenderTarget& target, float t)
{
    // Nerith itself, on the floor, whole. One window is lit and none of the
    // others are, and nothing anywhere in the frame is moving.
    wash(sf::Color(10, 18, 22), sf::Color(3, 6, 8));

    const float floor = Bottom - 40.0f;

    m_mesh.append(sf::Vertex{ { 0.0f, floor }, sf::Color(22, 28, 30) });
    m_mesh.append(sf::Vertex{ { W, floor }, sf::Color(22, 28, 30) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(8, 11, 12) });
    m_mesh.append(sf::Vertex{ { 0.0f, floor }, sf::Color(22, 28, 30) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(8, 11, 12) });
    m_mesh.append(sf::Vertex{ { 0.0f, H }, sf::Color(8, 11, 12) });

    wedge({ -60.0f, MidY - 130.0f }, { 1.0f, 0.22f }, 1100.0f, 260.0f,
          fade(Lamp, 0.10f), fade(Lamp, 0.0f));

    const float pan = 60.0f - 120.0f * ease(t);

    plate(target, m_station, { W * 0.52f + pan, floor - 108.0f }, 1.02f,
          sf::Color(120, 132, 140));

    // The lit window, given the halo the plate cannot paint on itself.
    glow({ W * 0.52f + pan + 30.0f, floor - 152.0f }, 88.0f, Ember, 0.30f);

    motes(m_total, { -40.0f, 60.0f }, 10.0f, fade(Sky, 0.14f), 70);
    flush(target);
}

void Cinematic::drawHatches(sf::RenderTarget& target, float t)
{
    // The same structure, close enough to see that the doors are open, and one
    // of the men who opened them still out in the water.
    wash(sf::Color(8, 15, 19), sf::Color(2, 5, 7));

    const float creep = 40.0f * ease(t);

    plate(target, m_station, { W * 0.28f - creep, MidY + 150.0f }, 2.9f,
          sf::Color(74, 84, 90));

    wedge({ W + 40.0f, Top + 40.0f }, { -1.0f, 0.25f }, 1000.0f, 250.0f,
          fade(Lamp, 0.09f), fade(Lamp, 0.0f));
    flush(target);

    // A man, drifting, with nothing holding him.
    const float turn = std::sin(m_total * 0.35f) * 22.0f - 12.0f;
    const float lift = std::sin(m_total * 0.5f) * 14.0f;

    plate(target, m_diver, { W * 0.74f, MidY - 110.0f + lift }, 0.92f,
          sf::Color(96, 104, 110), turn);

    glow({ W * 0.74f + 12.0f, MidY - 166.0f + lift }, 26.0f, Lamp, 0.5f);

    motes(m_total, { -30.0f, 50.0f }, 9.0f, fade(Sky, 0.12f), 60);
    flush(target);
}

void Cinematic::drawTrench(sf::RenderTarget& target, float t)
{
    // The floor under the station, coming apart. What is in the gap is never
    // shown: two lights come on in it, a long way apart, and that is all.
    wash(sf::Color(7, 12, 16), sf::Color(0, 0, 0));

    const float floor = MidY + 60.0f;
    const float gap = 40.0f + 420.0f * ease(t);

    const auto slab = [&](float from, float to) {
        m_mesh.append(sf::Vertex{ { from, floor }, sf::Color(24, 30, 34) });
        m_mesh.append(sf::Vertex{ { to, floor - 18.0f }, sf::Color(24, 30, 34) });
        m_mesh.append(sf::Vertex{ { to, H }, sf::Color(6, 9, 11) });
        m_mesh.append(sf::Vertex{ { from, floor }, sf::Color(24, 30, 34) });
        m_mesh.append(sf::Vertex{ { to, H }, sf::Color(6, 9, 11) });
        m_mesh.append(sf::Vertex{ { from, H }, sf::Color(6, 9, 11) });
    };

    slab(-20.0f, W * 0.5f - gap * 0.5f);
    slab(W * 0.5f + gap * 0.5f, W + 20.0f);

    // Two eyes, a long way apart, that were not on a moment ago.
    const float wake = std::clamp((t - 0.55f) / 0.4f, 0.0f, 1.0f);

    if (wake > 0.0f)
    {
        for (const float side : { -1.0f, 1.0f })
        {
            const sf::Vector2f at{ W * 0.5f + side * gap * 0.26f, floor + 120.0f };

            glow(at, 86.0f, Ember, 0.24f * wake);
            disc(at, 11.0f, fade(sf::Color(255, 216, 158), 0.75f * wake));
        }
    }

    wedge({ W * 0.5f, MidY - 150.0f }, { 0.0f, 1.0f }, 300.0f, 120.0f,
          fade(Lamp, 0.13f), fade(Lamp, 0.0f));
    flush(target);

    plate(target, m_bell, { W * 0.5f, MidY - 170.0f }, 0.40f, sf::Color(130, 142, 150));

    motes(m_total, { 0.0f, 120.0f }, 10.0f, fade(Sky, 0.12f), 50);
    flush(target);
}

void Cinematic::drawMark(sf::RenderTarget& target, float)
{
    wash(Deep, sf::Color(0, 0, 0));
    glow({ W * 0.5f, MidY }, 420.0f, Water, 0.55f);
    flush(target);
}

void Cinematic::drawHatch(sf::RenderTarget& target, float t)
{
    // The bottom of it opening, from much closer than anybody would choose.
    wash(sf::Color(6, 10, 14), sf::Color(0, 0, 0));

    const float floor = MidY - 30.0f;
    const float gap = 100.0f + 540.0f * ease(t);

    const auto slab = [&](float from, float to) {
        m_mesh.append(sf::Vertex{ { from, floor }, sf::Color(30, 36, 40) });
        m_mesh.append(sf::Vertex{ { to, floor }, sf::Color(30, 36, 40) });
        m_mesh.append(sf::Vertex{ { to, Top }, sf::Color(10, 14, 16) });
        m_mesh.append(sf::Vertex{ { from, floor }, sf::Color(30, 36, 40) });
        m_mesh.append(sf::Vertex{ { to, Top }, sf::Color(10, 14, 16) });
        m_mesh.append(sf::Vertex{ { from, Top }, sf::Color(10, 14, 16) });
    };

    slab(-20.0f, W * 0.5f - gap * 0.5f);
    slab(W * 0.5f + gap * 0.5f, W + 20.0f);

    // Light out of the gap, which is the wrong way round, and is the point.
    // Wide and faint: narrow shafts out of a hole read as teeth.
    for (int i = 0; i < 5; ++i)
    {
        const float x = W * 0.5f + (static_cast<float>(i) - 2.0f) * gap * 0.22f;

        wedge({ x, floor }, { 0.0f, 1.0f }, 560.0f, 150.0f,
              fade(Ember, 0.10f * ease(t)), fade(Ember, 0.0f));
    }

    glow({ W * 0.5f, floor + 30.0f }, gap * 0.9f, Ember, 0.22f * ease(t));

    motes(m_total, { 0.0f, -420.0f }, 30.0f, fade(Ember, 0.24f), 80);
    flush(target);
}

void Cinematic::drawClimb(sf::RenderTarget& target, float)
{
    // Rising. The rock goes the other way and so does everything else in the
    // water, which is the half of it that is not good news.
    wash(sf::Color(12, 22, 28), Deep);
    flush(target);

    rock(target, m_total * 540.0f, 2.1f, sf::Color(50, 58, 64));
    rock(target, m_total * 980.0f - 140.0f, 3.4f, sf::Color(22, 28, 32, 190));

    const auto gloom = [this](float from, float to) {
        m_mesh.append(sf::Vertex{ { from, Top }, fade(sf::Color::Black, 0.0f) });
        m_mesh.append(sf::Vertex{ { to, Top }, fade(sf::Color::Black, 0.8f) });
        m_mesh.append(sf::Vertex{ { to, Bottom }, fade(sf::Color::Black, 0.8f) });
        m_mesh.append(sf::Vertex{ { from, Top }, fade(sf::Color::Black, 0.0f) });
        m_mesh.append(sf::Vertex{ { to, Bottom }, fade(sf::Color::Black, 0.8f) });
        m_mesh.append(sf::Vertex{ { from, Bottom }, fade(sf::Color::Black, 0.0f) });
    };

    gloom(W * 0.24f, W * 0.5f);
    gloom(W * 0.76f, W * 0.5f);

    // Things going up past you, none of which are yours.
    for (int i = 0; i < 12; ++i)
    {
        const float seed = static_cast<float>(i) * 3.3f;
        const float x = 120.0f + wiggle(seed) * (W - 240.0f);
        const float speed = 180.0f + wiggle(seed + 0.5f) * 460.0f;
        const float y = H + 120.0f
                        - std::fmod(m_total * speed + wiggle(seed + 0.9f) * 900.0f, H + 300.0f);
        const float size = 8.0f + wiggle(seed + 0.2f) * 26.0f;

        disc({ x, y }, size, fade(sf::Color(20, 18, 26), 0.85f));
        disc({ x + size * 0.4f, y - size * 0.3f }, size * 0.22f, fade(Ember, 0.35f));
    }

    const float sway = std::sin(m_total * 1.3f) * 14.0f;

    wedge({ W * 0.5f + sway, MidY + 10.0f }, { 0.0f, -1.0f }, 300.0f, 110.0f,
          fade(Lamp, 0.14f), fade(Lamp, 0.0f));
    flush(target);

    plate(target, m_bell, { W * 0.5f + sway, MidY + 20.0f }, 0.46f, sf::Color(140, 152, 160),
          sway * 0.3f);

    motes(m_total, { 0.0f, 1200.0f }, 70.0f, fade(Sky, 0.18f), 60);
    flush(target);
}

void Cinematic::drawDawn(sf::RenderTarget& target, float t)
{
    // The surface from underneath, with the rig standing in it. The first warm
    // light in the whole game, and it arrives as a ceiling.
    wash(sf::Color(52, 92, 104), sf::Color(6, 14, 20));

    const float lid = Top + 90.0f - 50.0f * ease(t);

    for (int i = 0; i < 30; ++i)
    {
        const float x = static_cast<float>(i) * (W / 29.0f);
        const float lift = std::sin(m_total * 1.2f + static_cast<float>(i) * 0.5f) * 9.0f;

        bar({ x, lid + lift }, { x + W / 29.0f, lid + lift }, 6.0f,
            fade(sf::Color(226, 206, 168), 0.7f), fade(sf::Color(226, 206, 168), 0.7f));
    }

    for (int i = 0; i < 9; ++i)
    {
        const float x = 60.0f + static_cast<float>(i) * 150.0f;

        wedge({ x, lid }, { -0.08f, 1.0f }, 620.0f, 70.0f,
              fade(sf::Color(236, 214, 170), 0.16f), fade(Ember, 0.0f));
    }

    // The rig's legs, from below.
    for (const float x : { W * 0.30f, W * 0.42f, W * 0.62f, W * 0.74f })
        bar({ x, lid }, { x + 10.0f, lid + 260.0f }, 9.0f, fade(Deep, 0.8f), fade(Deep, 0.25f));

    const float rise = Bottom - 120.0f - 240.0f * ease(t);

    plate(target, m_bell, { W * 0.5f, rise }, 0.68f, sf::Color(168, 180, 186));

    motes(m_total, { 10.0f, -320.0f }, 18.0f, fade(sf::Color(226, 214, 180), 0.30f), 80);
    flush(target);
}

void Cinematic::drawCount(sf::RenderTarget& target, float t)
{
    // The deck, in the rain, with the hold on it. This is the only shot in the
    // game where anything goes right, and it lasts four seconds.
    wash(sf::Color(56, 62, 74), sf::Color(24, 30, 38));

    const float deck = MidY + 110.0f;

    m_mesh.append(sf::Vertex{ { 0.0f, deck }, sf::Color(74, 80, 84) });
    m_mesh.append(sf::Vertex{ { W, deck }, sf::Color(74, 80, 84) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(42, 48, 52) });
    m_mesh.append(sf::Vertex{ { 0.0f, deck }, sf::Color(74, 80, 84) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(42, 48, 52) });
    m_mesh.append(sf::Vertex{ { 0.0f, H }, sf::Color(42, 48, 52) });

    // Dawn behind it.
    glow({ W * 0.78f, deck - 30.0f }, 300.0f, sf::Color(236, 178, 118), 0.30f);

    const float pan = -40.0f * ease(t);

    // A derrick, drawn rather than plated: the deck crane is a top down sprite
    // and this shot is in profile, so the sprite would be a crane lying on its
    // side with its jib pointing at the camera.
    const sf::Color frame(96, 104, 110);
    const float mast = W * 0.16f + pan;

    bar({ mast, deck }, { mast + 26.0f, deck - 250.0f }, 7.0f, frame, frame);
    bar({ mast + 96.0f, deck }, { mast + 40.0f, deck - 250.0f }, 7.0f, frame, frame);

    for (int i = 1; i < 5; ++i)
    {
        const float up = deck - static_cast<float>(i) * 50.0f;
        bar({ mast + static_cast<float>(i) * 5.0f, up },
            { mast + 96.0f - static_cast<float>(i) * 11.0f, up }, 4.0f, frame, frame);
    }

    bar({ mast + 33.0f, deck - 250.0f }, { mast + 250.0f, deck - 216.0f }, 6.0f, frame, frame);
    bar({ mast + 244.0f, deck - 218.0f }, { mast + 244.0f, deck - 96.0f }, 2.0f,
        fade(frame, 0.7f), fade(frame, 0.7f));
    disc({ mast + 244.0f, deck - 88.0f }, 11.0f, frame);

    // The hold, stacked and counted one crate at a time.
    const sf::Vector2f stack[] = {
        { W * 0.40f, deck - 36.0f }, { W * 0.52f, deck - 30.0f }, { W * 0.46f, deck - 104.0f },
        { W * 0.64f, deck - 42.0f }, { W * 0.75f, deck - 34.0f },
    };

    for (std::size_t i = 0; i < std::size(stack); ++i)
    {
        const float show = std::clamp((t - static_cast<float>(i) * 0.12f) * 4.0f, 0.0f, 1.0f);
        if (show <= 0.0f)
            continue;

        plate(target, m_crate, stack[i] + sf::Vector2f{ pan, 0.0f }, 1.55f,
              dim(sf::Color(160, 166, 170), show));
    }

    // And a man standing next to it with a board, which is the whole of the
    // reward anybody in this line of work gets.
    plate(target, m_diver, { W * 0.88f + pan, deck - 92.0f }, 0.95f, sf::Color(142, 148, 136),
          std::sin(m_total * 0.6f) * 3.0f);

    motes(m_total, { -70.0f, 700.0f }, 24.0f, fade(Sky, 0.24f), 90);
    flush(target);
}

void Cinematic::drawWake(sf::RenderTarget& target, float t)
{
    // Black, and then not black. The rig is a shape a long way up and the
    // thing under it is most of the frame.
    wash(sf::Color(10, 16, 22), sf::Color(0, 0, 0));

    const float lid = Top + 50.0f;

    for (int i = 0; i < 20; ++i)
    {
        const float x = static_cast<float>(i) * (W / 19.0f);
        const float lift = std::sin(m_total * 0.9f + static_cast<float>(i) * 0.7f) * 5.0f;

        bar({ x, lid + lift }, { x + W / 19.0f, lid + lift }, 3.0f,
            fade(sf::Color(180, 176, 156), 0.30f), fade(sf::Color(180, 176, 156), 0.30f));
    }

    plate(target, m_rig, { W * 0.5f, lid - 24.0f }, 0.40f, sf::Color(52, 60, 68));

    // It comes up out of the bottom of the frame, and stops. Lit just enough
    // to be a shape: a thing you cannot see at all is not frightening, it is
    // a black screen.
    const float lift = 360.0f - 300.0f * ease(std::min(1.0f, t * 1.4f));
    const sf::Vector2f at{ W * 0.5f, Bottom + lift };

    glow(at, 620.0f, sf::Color(40, 54, 76), 0.55f);
    flush(target);

    plate(target, m_beast, at, 1.55f, sf::Color(52, 48, 66), 3.0f);

    // One eye, opening late.
    const float open = std::clamp((t - 0.55f) / 0.35f, 0.0f, 1.0f);

    if (open > 0.0f)
    {
        const sf::Vector2f eye{ W * 0.5f + 400.0f, Bottom + lift - 76.0f };

        glow(eye, 150.0f, Ember, 0.26f * open);
        disc(eye, 26.0f * open, fade(sf::Color(255, 222, 168), 0.9f * open));
        disc(eye, 9.0f * open, fade(sf::Color::Black, 0.9f * open));
    }

    flush(target);
}

void Cinematic::drawScene(sf::RenderTarget& target)
{
    const float t = shotTime();

    switch (current().scene)
    {
        case Scene::Rig:     drawRig(target, t);     break;
        case Scene::Lower:   drawLower(target, t);   break;
        case Scene::Fall:    drawFall(target, t);    break;
        case Scene::Pass:    drawPass(target, t);    break;
        case Scene::Station: drawStation(target, t); break;
        case Scene::Hatches: drawHatches(target, t); break;
        case Scene::Trench:  drawTrench(target, t);  break;
        case Scene::Mark:    drawMark(target, t);    break;
        case Scene::Hatch:   drawHatch(target, t);   break;
        case Scene::Climb:   drawClimb(target, t);   break;
        case Scene::Dawn:    drawDawn(target, t);    break;
        case Scene::Count:   drawCount(target, t);   break;
        case Scene::Wake:    drawWake(target, t);    break;
    }

    flush(target);
}

void Cinematic::draw(sf::RenderTarget& target)
{
    if (m_finished)
        return;

    m_mesh.clear();
    m_text.clear();

    m_fill.setSize({ W, H });
    m_fill.setPosition({ 0.0f, 0.0f });
    m_fill.setFillColor(Deep);
    target.draw(m_fill);

    drawScene(target);

    // A cut between shots is a hard black frame rather than a dissolve: this
    // is a thing that was filmed, not a slideshow with transitions on it.
    const float t = shotTime();
    const float cut = std::max(std::clamp(1.0f - t * 14.0f, 0.0f, 1.0f),
                               std::clamp((t - 0.94f) * 16.0f, 0.0f, 1.0f));

    if (cut > 0.0f)
    {
        m_fill.setSize({ W, H });
        m_fill.setPosition({ 0.0f, 0.0f });
        m_fill.setFillColor(fade(sf::Color::Black, cut));
        target.draw(m_fill);
    }

    // Letterbox.
    m_fill.setFillColor(sf::Color::Black);
    m_fill.setSize({ W, Letterbox });
    m_fill.setPosition({ 0.0f, 0.0f });
    target.draw(m_fill);
    m_fill.setPosition({ 0.0f, H - Letterbox });
    target.draw(m_fill);

    // The line, in the lower bar, fading at both ends of its shot. It shrinks
    // rather than running off the edge, because a caption is written to say
    // what it needs to say and not to fit a measurement.
    if (current().line != nullptr || !m_caption.empty())
    {
        const float say = std::min(std::clamp(t * 5.0f, 0.0f, 1.0f),
                                   std::clamp((1.0f - t) * 5.0f, 0.0f, 1.0f));

        const std::string line = current().line != nullptr ? current().line : m_caption;
        const float scale = pixelfont::measure(line, 1.7f) > W - 90.0f ? 1.35f : 1.7f;
        const float width = pixelfont::measure(line, scale);

        pixelfont::append(m_text, line, { (W - width) * 0.5f, H - Letterbox + 34.0f }, scale,
                          fade(config::HudTextColor, say));
    }

    if (current().scene == Scene::Mark)
    {
        // The title card carries the name, and the line the whole reel has
        // been working toward.
        const float say = std::clamp(t * 2.2f, 0.0f, 1.0f)
                          * std::clamp((1.0f - t) * 3.0f, 0.0f, 1.0f);

        const std::string mark = "NERITH";
        const float width = pixelfont::measure(mark, 7.0f);

        pixelfont::append(m_text, mark, { (W - width) * 0.5f, H * 0.36f }, 7.0f,
                          fade(config::HudBossColor, say));

        // Eight shots is the descent; anything shorter is the way back up.
        const std::string under = m_shots.size() > 6 ? "YOU ARE THE FOURTH"
                                                     : "NOBODY ELSE CAME BACK UP";
        const float underWidth = pixelfont::measure(under, 2.0f);

        pixelfont::append(m_text, under, { (W - underWidth) * 0.5f, H * 0.36f + 78.0f }, 2.0f,
                          fade(config::HudTextColor, say * 0.85f));
    }

    // And the way out, always, from the first frame.
    const std::string out = "ANY KEY TO SKIP";
    pixelfont::append(m_text, out,
                      { W - pixelfont::measure(out, 1.2f) - config::HudMargin,
                        H - Letterbox + 66.0f },
                      1.2f, fade(config::HudDimTextColor, 0.7f));

    target.draw(m_text);
}
