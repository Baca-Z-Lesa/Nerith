#include "ui/Cinematic.h"

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

    // How tall the black bars are. Wide letterbox on purpose: it is the one
    // signal every player already knows for "this is not a thing you are
    // playing right now".
    constexpr float Letterbox = 92.0f;

    constexpr sf::Color Deep{ 4, 9, 13 };
    constexpr sf::Color Water{ 12, 30, 40 };
    constexpr sf::Color Sky{ 118, 150, 160 };
    constexpr sf::Color Rock{ 26, 34, 38 };
    constexpr sf::Color RockLit{ 52, 64, 66 };
    constexpr sf::Color Steel{ 96, 108, 116 };
    constexpr sf::Color Lamp{ 128, 214, 224 };
    constexpr sf::Color Ember{ 236, 150, 96 };
    constexpr sf::Color Egg{ 226, 214, 178 };

    sf::Color fade(sf::Color colour, float alpha)
    {
        colour.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
        return colour;
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
        return std::sin(seed * 12.9898f) * 43758.5453f - std::floor(std::sin(seed * 12.9898f)
                                                                    * 43758.5453f);
    }

}

Cinematic::Cinematic()
    : m_mesh(sf::PrimitiveType::Triangles)
    , m_text(sf::PrimitiveType::Triangles)
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
            { Scene::Winch,   5.0f, "NERITH STATION STOPPED ANSWERING ELEVEN DAYS AGO." },
            { Scene::Fall,    4.6f, "YOU ARE THE FOURTH SUB THEY HAVE SENT DOWN." },
            { Scene::Pass,    4.4f, "THE OTHER THREE REPORTED NOTHING WRONG." },
            { Scene::Station, 4.6f, "AND THEN THEY STOPPED REPORTING." },
            { Scene::Mark,    4.0f, nullptr },
        };
    }
    else
    {
        m_shots = {
            { Scene::Hatch, 4.6f, "THE FLOOR OF THE TRENCH CAME OPEN." },
            { Scene::Climb, 4.8f, "EVERYTHING IN IT WENT UP PAST YOU." },
            { Scene::Dawn,  4.6f, "YOU SURFACED AT FIRST LIGHT. ALONE." },
            { Scene::Wake,  5.2f, "NOT FOR LONG." },
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

// ---------------------------------------------------------------------------
//  The shots
// ---------------------------------------------------------------------------

void Cinematic::drawWinch(float t)
{
    // A hole of daylight closing up above, a cable, and the sub going away
    // from it. The whole shot is the light getting smaller.
    const float sink = ease(t);
    const float sky = 200.0f * (1.0f - sink * 0.92f);

    disc({ W * 0.5f, -40.0f }, sky * 2.6f, fade(Sky, 0.22f * (1.0f - sink * 0.7f)), 26);
    disc({ W * 0.5f, -40.0f }, sky, fade(Sky, 0.5f * (1.0f - sink * 0.6f)), 22);

    const float y = H * (0.18f + 0.5f * sink);

    bar({ W * 0.5f, -40.0f }, { W * 0.5f, y }, 2.0f,
        fade(Steel, 0.5f), fade(Steel, 0.75f));

    hull({ W * 0.5f, y }, 26.0f, 90.0f, fade(sf::Color(20, 26, 30), 1.0f));
    disc({ W * 0.5f, y + 16.0f }, 5.0f, fade(Lamp, 0.9f), 10);

    // Bubbles going the other way, which is the only thing in the frame that
    // is not sinking.
    for (int i = 0; i < 22; ++i)
    {
        const float seed = static_cast<float>(i) * 3.1f;
        const float up = std::fmod(m_total * 0.35f + wiggle(seed), 1.0f);

        disc({ W * (0.32f + 0.36f * wiggle(seed + 1.0f)), H * (1.1f - up * 1.2f) },
             1.0f + 2.0f * wiggle(seed + 2.0f), fade(Lamp, 0.16f), 6);
    }
}

void Cinematic::drawFall(float t)
{
    // Rock going past. The walls are drawn from a scroll offset rather than
    // moved, so the shot can be any length and never repeat visibly.
    const float scroll = m_total * 420.0f + ease(t) * 600.0f;

    for (int side = 0; side < 2; ++side)
    {
        const float x = side == 0 ? 0.0f : W;
        const float sign = side == 0 ? 1.0f : -1.0f;

        for (int i = -1; i < 14; ++i)
        {
            const float seed = static_cast<float>(i + side * 40);
            const float y = std::fmod(static_cast<float>(i) * 120.0f + scroll, H + 260.0f) - 130.0f;
            const float reach = 120.0f + 150.0f * wiggle(seed);

            m_mesh.append(sf::Vertex{ { x, y - 90.0f }, fade(Rock, 1.0f) });
            m_mesh.append(sf::Vertex{ { x + sign * reach, y }, fade(RockLit, 1.0f) });
            m_mesh.append(sf::Vertex{ { x, y + 90.0f }, fade(Rock, 1.0f) });
        }
    }

    // And the sub in the middle of it with the lamp on, because this is the
    // only shot where the player is told what the lamp is for.
    const sf::Vector2f at{ W * 0.5f, H * 0.46f + std::sin(m_total * 2.0f) * 8.0f };

    wedge(at, { 0.0f, 1.0f }, 420.0f, 190.0f, fade(Lamp, 0.16f), fade(Lamp, 0.0f));
    hull(at, 30.0f, 90.0f, fade(sf::Color(22, 28, 32), 1.0f));
}

void Cinematic::drawPass(float t)
{
    // Something crosses the beam. It is never shown - only the beam going out
    // in a band and coming back - because whatever the player imagines there
    // is worse than anything that can be drawn.
    const sf::Vector2f at{ W * 0.5f, H * 0.62f };

    wedge(at, { 0.0f, -1.0f }, 520.0f, 230.0f, fade(Lamp, 0.20f), fade(Lamp, 0.0f));
    hull(at, 30.0f, -90.0f, fade(sf::Color(22, 28, 32), 1.0f));

    const float sweep = ease(t) * (W + 900.0f) - 450.0f;
    const float thick = 150.0f + 90.0f * std::sin(t * 3.14159265f);

    for (int i = 0; i < 7; ++i)
    {
        const float lag = static_cast<float>(i) * 46.0f;
        const float x = sweep - lag;
        const float y = H * (0.16f + 0.02f * static_cast<float>(i));

        m_mesh.append(sf::Vertex{ { x - thick, y - 60.0f }, fade(Deep, 0.0f) });
        m_mesh.append(sf::Vertex{ { x, y - 90.0f }, fade(Deep, 1.0f) });
        m_mesh.append(sf::Vertex{ { x, H * 0.9f }, fade(Deep, 1.0f) });

        m_mesh.append(sf::Vertex{ { x - thick, y - 60.0f }, fade(Deep, 0.0f) });
        m_mesh.append(sf::Vertex{ { x, H * 0.9f }, fade(Deep, 1.0f) });
        m_mesh.append(sf::Vertex{ { x - thick, H * 0.9f }, fade(Deep, 0.0f) });
    }

    // One eye, open for about a second in the middle of it, and never again.
    const float look = std::clamp(1.0f - std::abs(t - 0.52f) * 9.0f, 0.0f, 1.0f);
    if (look > 0.0f)
    {
        const sf::Vector2f eye{ sweep - 150.0f, H * 0.30f };

        disc(eye, 26.0f * look, fade(Ember, 0.7f * look), 16);
        disc(eye, 11.0f * look, fade(sf::Color(12, 8, 6), look), 12);
    }
}

void Cinematic::drawStation(float t)
{
    // A structure with nothing running in it. Boxes and legs: the moment it is
    // detailed it stops being a wreck and starts being a set.
    const float reveal = ease(std::clamp(t * 1.6f, 0.0f, 1.0f));
    const float base = H * 0.72f;

    for (int i = 0; i < 5; ++i)
    {
        const float seed = static_cast<float>(i) * 7.0f;
        const float x = W * (0.18f + 0.16f * static_cast<float>(i));
        const float w = 42.0f + 40.0f * wiggle(seed);
        const float h = 60.0f + 120.0f * wiggle(seed + 1.0f);

        m_mesh.append(sf::Vertex{ { x - w, base }, fade(Steel, 0.30f * reveal) });
        m_mesh.append(sf::Vertex{ { x - w, base - h }, fade(Steel, 0.30f * reveal) });
        m_mesh.append(sf::Vertex{ { x + w, base - h }, fade(Steel, 0.30f * reveal) });

        m_mesh.append(sf::Vertex{ { x - w, base }, fade(Steel, 0.30f * reveal) });
        m_mesh.append(sf::Vertex{ { x + w, base - h }, fade(Steel, 0.30f * reveal) });
        m_mesh.append(sf::Vertex{ { x + w, base }, fade(Steel, 0.30f * reveal) });

        bar({ x - w * 0.6f, base }, { x - w * 0.9f, base + 70.0f }, 4.0f,
            fade(Steel, 0.26f * reveal), fade(Steel, 0.0f));
        bar({ x + w * 0.6f, base }, { x + w * 0.9f, base + 70.0f }, 4.0f,
            fade(Steel, 0.26f * reveal), fade(Steel, 0.0f));
    }

    // The one light still on, and it is going out while you watch.
    const float alive = std::clamp(1.0f - t * 1.7f, 0.0f, 1.0f);
    const float flicker = alive * (0.55f + 0.45f * std::sin(m_total * 23.0f));

    disc({ W * 0.5f, base - 96.0f }, 54.0f, fade(Ember, 0.16f * flicker), 18);
    disc({ W * 0.5f, base - 96.0f }, 7.0f, fade(Ember, flicker), 10);

    hull({ W * 0.78f, base - 150.0f + std::sin(m_total * 1.4f) * 6.0f }, 20.0f, 180.0f,
         fade(sf::Color(24, 30, 34), reveal));
}

void Cinematic::drawMark(float t)
{
    // The title, out of the dark. Letters do not type: the word is already
    // there and the dark is what moves.
    const float open = ease(std::clamp(t * 1.5f, 0.0f, 1.0f));

    disc({ W * 0.5f, H * 0.5f }, 90.0f + 620.0f * open,
         fade(Water, 0.5f * open * (1.0f - std::clamp((t - 0.75f) * 4.0f, 0.0f, 1.0f))), 30);

    for (int i = 0; i < 40; ++i)
    {
        const float seed = static_cast<float>(i) * 2.7f;
        const float up = std::fmod(m_total * 0.22f + wiggle(seed), 1.0f);

        disc({ W * wiggle(seed + 4.0f), H * (1.1f - up * 1.2f) }, 1.0f + 2.0f * wiggle(seed + 5.0f),
             fade(Lamp, 0.14f * open), 6);
    }
}

void Cinematic::drawHatch(float t)
{
    // The floor opens. A dark seam that widens, and light coming up out of it
    // that is not the sort of light anybody wants.
    const float open = ease(t);
    const float base = H * 0.80f;

    m_mesh.append(sf::Vertex{ { 0.0f, base }, fade(Rock, 1.0f) });
    m_mesh.append(sf::Vertex{ { 0.0f, H }, fade(Rock, 1.0f) });
    m_mesh.append(sf::Vertex{ { W, H }, fade(Rock, 1.0f) });

    m_mesh.append(sf::Vertex{ { 0.0f, base }, fade(Rock, 1.0f) });
    m_mesh.append(sf::Vertex{ { W, H }, fade(Rock, 1.0f) });
    m_mesh.append(sf::Vertex{ { W, base + 40.0f }, fade(Rock, 1.0f) });

    const float gap = 12.0f + 300.0f * open;

    m_mesh.append(sf::Vertex{ { W * 0.5f - gap, H }, fade(Deep, 1.0f) });
    m_mesh.append(sf::Vertex{ { W * 0.5f, base - 40.0f * open }, fade(Deep, 1.0f) });
    m_mesh.append(sf::Vertex{ { W * 0.5f + gap, H }, fade(Deep, 1.0f) });

    // What is coming up out of it.
    for (int i = 0; i < 90; ++i)
    {
        const float seed = static_cast<float>(i) * 1.7f;
        const float up = std::fmod(m_total * 0.30f + wiggle(seed), 1.0f);
        const float spread = (wiggle(seed + 3.0f) - 0.5f) * gap * 1.8f;

        disc({ W * 0.5f + spread * (0.4f + up), base - (base - 40.0f) * up * up },
             1.5f + 2.5f * wiggle(seed + 6.0f), fade(Egg, 0.55f * open * (1.0f - up * 0.6f)), 7);
    }

    disc({ W * 0.5f, H }, gap * 1.4f, fade(Egg, 0.10f * open), 24);
}

void Cinematic::drawClimb(float t)
{
    // Rising, with everything that came out of the hole going past faster than
    // the sub is. It is not an escape: it is being overtaken.
    const float rise = m_total * 300.0f + ease(t) * 700.0f;

    for (int i = 0; i < 120; ++i)
    {
        const float seed = static_cast<float>(i) * 2.3f;
        const float y = std::fmod(static_cast<float>(i) * 51.0f + rise, H + 200.0f) - 100.0f;
        const float x = W * wiggle(seed);
        const float size = 2.0f + 5.0f * wiggle(seed + 1.0f);

        disc({ x, H - y }, size, fade(Egg, 0.42f), 7);
        bar({ x, H - y }, { x, H - y + size * 5.0f }, size * 0.4f,
            fade(Egg, 0.18f), fade(Egg, 0.0f));
    }

    const sf::Vector2f at{ W * 0.5f, H * 0.58f - std::sin(m_total * 1.6f) * 10.0f };

    wedge(at, { 0.0f, -1.0f }, 300.0f, 120.0f, fade(Lamp, 0.13f), fade(Lamp, 0.0f));
    hull(at, 28.0f, -90.0f, fade(sf::Color(24, 30, 34), 1.0f));
}

void Cinematic::drawDawn(float t)
{
    // The surface, from underneath, and then through it. The first warm colour
    // in the entire game.
    const float up = ease(t);
    const float line = H * (1.15f - 0.85f * up);

    m_mesh.append(sf::Vertex{ { 0.0f, line }, fade(Sky, 0.55f) });
    m_mesh.append(sf::Vertex{ { 0.0f, 0.0f }, fade(Ember, 0.55f) });
    m_mesh.append(sf::Vertex{ { W, 0.0f }, fade(Ember, 0.55f) });

    m_mesh.append(sf::Vertex{ { 0.0f, line }, fade(Sky, 0.55f) });
    m_mesh.append(sf::Vertex{ { W, 0.0f }, fade(Ember, 0.55f) });
    m_mesh.append(sf::Vertex{ { W, line }, fade(Sky, 0.55f) });

    // The waterline itself, wobbling.
    for (int i = 0; i < 40; ++i)
    {
        const float x0 = W * static_cast<float>(i) / 40.0f;
        const float x1 = W * static_cast<float>(i + 1) / 40.0f;
        const float w0 = std::sin(m_total * 1.4f + static_cast<float>(i) * 0.6f) * 7.0f;
        const float w1 = std::sin(m_total * 1.4f + static_cast<float>(i + 1) * 0.6f) * 7.0f;

        bar({ x0, line + w0 }, { x1, line + w1 }, 3.0f, fade(Sky, 0.8f), fade(Sky, 0.8f));
    }

    hull({ W * 0.5f, line + 60.0f - 40.0f * up }, 30.0f, -90.0f,
         fade(sf::Color(26, 32, 36), 1.0f));
}

void Cinematic::drawWake(float t)
{
    // Dawn, held - and then the water behind the sub starts to light up, one
    // point at a time, all the way to the horizon. Nobody says anything about
    // it. That is the ending.
    const float line = H * 0.34f;

    m_mesh.append(sf::Vertex{ { 0.0f, line }, fade(Sky, 0.6f) });
    m_mesh.append(sf::Vertex{ { 0.0f, 0.0f }, fade(Ember, 0.62f) });
    m_mesh.append(sf::Vertex{ { W, 0.0f }, fade(Ember, 0.62f) });

    m_mesh.append(sf::Vertex{ { 0.0f, line }, fade(Sky, 0.6f) });
    m_mesh.append(sf::Vertex{ { W, 0.0f }, fade(Ember, 0.62f) });
    m_mesh.append(sf::Vertex{ { W, line }, fade(Sky, 0.6f) });

    m_mesh.append(sf::Vertex{ { 0.0f, line }, fade(Water, 0.9f) });
    m_mesh.append(sf::Vertex{ { W, line }, fade(Water, 0.9f) });
    m_mesh.append(sf::Vertex{ { W, H }, fade(Deep, 1.0f) });

    m_mesh.append(sf::Vertex{ { 0.0f, line }, fade(Water, 0.9f) });
    m_mesh.append(sf::Vertex{ { W, H }, fade(Deep, 1.0f) });
    m_mesh.append(sf::Vertex{ { 0.0f, H }, fade(Deep, 1.0f) });

    hull({ W * 0.34f, line + 22.0f }, 26.0f, 0.0f, fade(sf::Color(26, 32, 36), 1.0f));

    // They arrive one at a time, and they keep arriving after the line of text
    // has gone.
    const int lit = static_cast<int>(std::clamp(t * 1.5f, 0.0f, 1.0f) * 120.0f);

    for (int i = 0; i < lit; ++i)
    {
        const float seed = static_cast<float>(i) * 3.7f;
        const float x = W * wiggle(seed);
        const float y = line + 10.0f + (H - line) * wiggle(seed + 1.0f) * wiggle(seed + 1.0f);
        const float pulse = 0.6f + 0.4f * std::sin(m_total * 3.0f + seed);

        disc({ x, y }, 2.0f + 2.5f * wiggle(seed + 2.0f), fade(Egg, 0.75f * pulse), 7);
        disc({ x, y }, 9.0f, fade(Egg, 0.10f * pulse), 10);
    }
}

// ---------------------------------------------------------------------------

void Cinematic::drawScene(sf::RenderTarget& target)
{
    const float t = shotTime();

    switch (current().scene)
    {
        case Scene::Winch:   drawWinch(t);   break;
        case Scene::Fall:    drawFall(t);    break;
        case Scene::Pass:    drawPass(t);    break;
        case Scene::Station: drawStation(t); break;
        case Scene::Mark:    drawMark(t);    break;
        case Scene::Hatch:   drawHatch(t);   break;
        case Scene::Climb:   drawClimb(t);   break;
        case Scene::Dawn:    drawDawn(t);    break;
        case Scene::Wake:    drawWake(t);    break;
    }

    target.draw(m_mesh);
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

    // The line, in the lower bar, fading at both ends of its shot.
    if (current().line != nullptr || !m_caption.empty())
    {
        const float say = std::min(std::clamp(t * 5.0f, 0.0f, 1.0f),
                                   std::clamp((1.0f - t) * 5.0f, 0.0f, 1.0f));

        const std::string line = current().line != nullptr ? current().line : m_caption;
        const float width = pixelfont::measure(line, 1.7f);

        pixelfont::append(m_text, line, { (W - width) * 0.5f, H - Letterbox + 32.0f }, 1.7f,
                          fade(config::HudTextColor, say));
    }
    else
    {
        // The title card carries the name instead.
        const float say = std::clamp(t * 2.2f, 0.0f, 1.0f)
                          * std::clamp((1.0f - t) * 3.0f, 0.0f, 1.0f);

        const std::string mark = "NERITH";
        const float width = pixelfont::measure(mark, 7.0f);

        pixelfont::append(m_text, mark, { (W - width) * 0.5f, H * 0.42f }, 7.0f,
                          fade(config::HudBossColor, say));
    }

    // And the way out, always, from the first frame.
    const std::string out = "ANY KEY TO SKIP";
    pixelfont::append(m_text, out,
                      { W - pixelfont::measure(out, 1.2f) - config::HudMargin,
                        H - Letterbox + 66.0f },
                      1.2f, fade(config::HudDimTextColor, 0.7f));

    target.draw(m_text);
}
