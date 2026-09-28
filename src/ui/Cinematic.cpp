#include "ui/Cinematic.h"

#include "core/Assets.h"
#include "core/Config.h"
#include "ui/PixelFont.h"

#include <SFML/Graphics/PrimitiveType.hpp>

#include <algorithm>
#include <array>
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
    constexpr float Top = Letterbox;
    constexpr float Bottom = H - Letterbox;
    constexpr float MidY = H * 0.5f;

    // What each depth band does with the camera. The foreground moves more
    // than twice as much as the middle distance, and is out of focus too.
    constexpr float BackPan = 0.25f;
    constexpr float ForePan = 2.20f;

    // The ledge plate is 640 by 240, so at the scales a foreground wants it is
    // bigger than the screen. Left to itself it eats the bottom half of every
    // shot and the picture becomes a black mound with a caption under it - it
    // did, on the first pass. A foreground is a band, not a wall: no more than
    // this much of one is ever allowed to be above the bottom of the frame.
    constexpr float ForeBand = 62.0f;
    constexpr float LedgeTall = 240.0f;

    // Where to put a ledge so exactly ForeBand of it shows.
    float ledgeAt(float scale, float extra = 0.0f)
    {
        return Bottom + LedgeTall * scale * 0.5f - ForeBand + extra;
    }

    constexpr sf::Color Deep{ 4, 9, 13 };
    constexpr sf::Color Water{ 12, 30, 40 };
    constexpr sf::Color Night{ 20, 26, 36 };
    constexpr sf::Color Sky{ 118, 150, 160 };
    constexpr sf::Color Steel{ 96, 108, 116 };
    constexpr sf::Color Lamp{ 128, 214, 224 };
    constexpr sf::Color Ember{ 236, 150, 96 };
    constexpr sf::Color Shade{ 6, 9, 11 };

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

    // 0 before from, 1 after to, eased between. Every beat inside a shot is
    // one of these, which is what stops a seven second shot being a still.
    float beat(float t, float from, float to)
    {
        return ease(std::clamp((t - from) / std::max(0.001f, to - from), 0.0f, 1.0f));
    }

    // A repeatable wobble, so the same seed gives the same rock every time the
    // reel is watched.
    float wiggle(float seed)
    {
        const float x = std::sin(seed * 12.9898f) * 43758.5453f;
        return x - std::floor(x);
    }

    // The foreground's focus. A plate drawn through this samples itself on
    // two rings round every pixel, so an edge becomes a falloff. Averaged by
    // alpha, because the see-through part of a plate has no colour worth
    // having and would otherwise leave a dark rim round everything.
    //
    // Past the plate's border counts as empty rather than as more of the edge.
    // Left to clamp, the texture would repeat its last row outward and the
    // plate's own straight sides - which do cross the frame, in the shots
    // where rock scrolls past - would stay razor sharp beside a soft skyline.
    constexpr const char* Soften = R"(
uniform sampler2D plate;
uniform vec2 reach;   // the blur radius, in texture coordinates

vec4 tap(vec2 p)
{
    if (p.x < 0.0 || p.y < 0.0 || p.x > 1.0 || p.y > 1.0)
        return vec4(0.0);
    vec4 s = texture2D(plate, p);
    return vec4(s.rgb * s.a, s.a);
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;

    vec4 sum = tap(uv) * 2.0;

    for (int i = 0; i < 12; ++i)
    {
        float a = float(i) * 0.5236;
        float ring = mod(float(i), 2.0) < 0.5 ? 0.5 : 1.0;
        sum += tap(uv + vec2(cos(a), sin(a)) * ring * reach);
    }

    sum /= 14.0;
    vec3 rgb = sum.a > 0.0 ? sum.rgb / sum.a : vec3(0.0);
    gl_FragColor = vec4(rgb, sum.a) * gl_Color;
}
)";
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
    , m_crate(assets.byPath(config::ReelCratePath))
    , m_ledge(assets.byPath(config::ReelLedgePath))
    , m_wreck(assets.byPath(config::ReelWreckPath))
    , m_hatch(assets.byPath(config::ReelHatchPath))
{
    m_fill.setFillColor(sf::Color::Black);

    if (sf::Shader::isAvailable()
        && m_soften.loadFromMemory(Soften, sf::Shader::Type::Fragment))
    {
        const sf::Vector2f size(m_ledge.getTexture().getSize());
        m_soften.setUniform("plate", sf::Shader::CurrentTexture);
        m_soften.setUniform("reach", sf::Glsl::Vec2(config::ForeBlur / size.x,
                                                    config::ForeBlur / size.y));
        m_softens = true;
    }
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
        // Six shots where there were eight. Every line leans on one of the
        // things the whole story actually turns on - eleven days of silence,
        // three boats at one depth, doors that open outward, a trench that is
        // not a trench, and the fact that somebody is still paying for this.
        m_shots = {
            { Scene::Rig,      7.0f, "ELEVEN DAYS OF SILENCE, AND THEY ARE STILL PAYING." },
            { Scene::Shutter,  6.0f, "THREE CREWS WENT DOWN THIS CABLE. NONE CAME UP IT." },
            { Scene::Sounding, 7.0f, "ALL THREE STOPPED HERE. NOT NEAR HERE. HERE." },
            { Scene::Nerith,   7.0f, "EVERY DOOR IS OPEN, AND EVERY DOOR OPENS OUTWARD." },
            { Scene::Trench,   6.5f, "IT IS NOT A TRENCH. IT IS THE SPACE AROUND SOMETHING." },
            { Scene::Mark,     5.5f, nullptr },
        };
    }
    else
    {
        m_shots = {
            { Scene::Ascent,   7.0f, "THE FLOOR CAME OPEN AND EVERYTHING IN IT WENT UP." },
            { Scene::Daylight, 6.5f, "NOBODY HAS SURFACED FROM NERITH BEFORE." },
            { Scene::Count,    7.0f, "THEY WEIGHED IT, PAID IT, AND ASKED NO QUESTIONS." },
            { Scene::Wake,     7.0f, "IT DID NOT FOLLOW YOU. IT CAME UP ON ITS OWN." },
            { Scene::Mark,     6.0f, nullptr },
        };
    }

    m_shot = 0;
    m_time = 0.0f;
    m_total = 0.0f;
    m_scroll = 0.0f;
    m_finished = m_shots.empty();
    m_sting = m_finished ? Sting::Count : stingFor(current().scene);
}

Cinematic::Sting Cinematic::stingFor(Scene scene)
{
    switch (scene)
    {
        case Scene::Rig:
        case Scene::Count:    return Sting::Horn;
        case Scene::Fall:
        case Scene::Shutter:  return Sting::Fall;
        case Scene::Climb:
        case Scene::Ascent:
        case Scene::Trench:   return Sting::Climb;
        case Scene::Daylight: return Sting::Dawn;
        case Scene::Mark:
        case Scene::Sounding:
        case Scene::Nerith:
        case Scene::Wake:     return Sting::Mark;
        default:              return Sting::Count;
    }
}

Cinematic::Sting Cinematic::takeSting()
{
    const Sting due = m_sting;
    m_sting = Sting::Count;
    return due;
}

void Cinematic::update(float deltaTime)
{
    if (m_finished)
        return;

    m_time += deltaTime;
    m_total += deltaTime;

    // The scroll is integrated rather than read off the clock, because the
    // shot that matters stops the fall dead half way through itself and then
    // never starts it again. A scroll cut from the clock cannot stop.
    const float t = shotTime();
    float pace = 0.0f;

    switch (current().scene)
    {
        case Scene::Fall:     pace = 1.0f; break;
        case Scene::Climb:    pace = -1.0f; break;
        case Scene::Sounding: pace = 1.0f - beat(t, 0.42f, 0.66f); break;
        case Scene::Ascent:   pace = -beat(t, 0.22f, 0.38f); break;
        default:              pace = 0.0f; break;
    }

    m_scroll += deltaTime * pace;

    while (!m_finished && m_time >= current().seconds)
    {
        m_time -= current().seconds;

        if (++m_shot >= m_shots.size())
            m_finished = true;
        else
            m_sting = stingFor(current().scene);
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
    // A boat, as a silhouette. It is never more than a shape in these: a
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

float Cinematic::fitOf(const sf::Sprite& sprite) const
{
    if (&sprite == &m_rig)     return config::ReelFitRig;
    if (&sprite == &m_bell)    return config::ReelFitBell;
    if (&sprite == &m_station) return config::ReelFitStation;
    if (&sprite == &m_beast)   return config::ReelFitBeast;
    if (&sprite == &m_diver)   return config::ReelFitDiver;
    if (&sprite == &m_crate)   return config::ReelFitCrate;

    return 1.0f;
}

void Cinematic::fore(sf::RenderTarget& target, sf::Vector2f at, float scale, sf::Color tint,
                     float turn)
{
    plate(target, m_ledge, at, scale, tint, turn, m_softens ? &m_soften : nullptr);
}

void Cinematic::plate(sf::RenderTarget& target, sf::Sprite& sprite, sf::Vector2f at, float scale,
                      sf::Color tint, float turn, const sf::Shader* shader)
{
    scale *= fitOf(sprite);

    // Whatever has been put in the mesh belongs behind the plate, and a sprite
    // cannot be batched into it, so the mesh goes out first.
    flush(target);

    const sf::Vector2f size(sprite.getTexture().getSize());

    sprite.setOrigin(size * 0.5f);
    sprite.setPosition(at);
    sprite.setScale({ scale, scale });
    sprite.setRotation(sf::degrees(turn));
    sprite.setColor(tint);
    target.draw(sprite, shader);
}

sf::Vector2f Cinematic::onPlate(const sf::Sprite& sprite, sf::Vector2f at, float scale,
                                float turn, sf::Vector2f uv) const
{
    const sf::Vector2f size(sprite.getTexture().getSize());
    const float k = scale * fitOf(sprite);
    const sf::Vector2f local{ (uv.x - 0.5f) * size.x * k, (uv.y - 0.5f) * size.y * k };

    return at + local.rotatedBy(sf::degrees(turn));
}

void Cinematic::rock(sf::RenderTarget& target, float offset, float scale, sf::Color tint)
{
    flush(target);

    // The wall goes the other way from every other plate: its scale divides
    // the rectangle it takes off the texture, so a bigger texture needs a
    // bigger number to show the same apparent rock.
    scale /= config::ReelFitWall;

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
//  The way down
// ---------------------------------------------------------------------------

void Cinematic::drawRig(sf::RenderTarget& target, float t)
{
    // Night, rain, and a rig with its lights on. Everything in this shot was
    // decided before the player arrived: the bell is already over the side and
    // somebody is already signing for it.
    const float push = 1.06f + 0.24f * ease(t);
    const float pan = -54.0f * ease(t);
    const float sea = MidY + 96.0f;

    wash(Night, sf::Color(10, 14, 20));

    // BACK. Cloud, a boat a long way out, and the swell.
    for (int i = 0; i < 3; ++i)
    {
        const float y = Top + 40.0f + static_cast<float>(i) * 26.0f;
        bar({ -60.0f + pan * BackPan, y }, { W + 60.0f + pan * BackPan, y - 4.0f }, 14.0f,
            fade(sf::Color(34, 40, 52), 0.50f), fade(sf::Color(34, 40, 52), 0.0f));
    }

    m_mesh.append(sf::Vertex{ { 0.0f, sea }, sf::Color(16, 26, 34) });
    m_mesh.append(sf::Vertex{ { W, sea }, sf::Color(16, 26, 34) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(5, 9, 13) });
    m_mesh.append(sf::Vertex{ { 0.0f, sea }, sf::Color(16, 26, 34) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(5, 9, 13) });
    m_mesh.append(sf::Vertex{ { 0.0f, H }, sf::Color(5, 9, 13) });

    hull({ W * 0.10f + pan * BackPan, sea - 12.0f }, 26.0f, 8.0f, dim(Steel, 0.30f));

    for (int i = 0; i < 8; ++i)
    {
        const float y = sea + 10.0f + static_cast<float>(i) * 22.0f;
        const float slide = std::sin(m_total * 0.7f + static_cast<float>(i)) * 20.0f;

        bar({ -40.0f + slide, y }, { W + 40.0f + slide, y + 3.0f }, 2.0f,
            fade(Sky, 0.12f), fade(Sky, 0.03f));
    }

    // MID. The rig, its windows, and the bell hanging on the crane's hook.
    const sf::Vector2f rigAt{ W * 0.52f + pan, sea - 84.0f * push };

    plate(target, m_rig, rigAt, push, sf::Color(148, 158, 166));

    glow(onPlate(m_rig, rigAt, push, 0.0f, { 0.525f, 0.275f }), 48.0f, Ember, 0.50f);
    glow(onPlate(m_rig, rigAt, push, 0.0f, { 0.748f, 0.380f }), 30.0f, Ember, 0.40f);

    for (int i = 0; i < 5; ++i)
    {
        const sf::Vector2f uv{ 0.151f + 0.042f * static_cast<float>(i), 0.418f };
        glow(onPlate(m_rig, rigAt, push, 0.0f, uv), 16.0f, sf::Color(230, 190, 130), 0.35f);
    }

    // The swing grows across the shot: a crane that has just picked the bell
    // up and a crane about to put it in the water do not move the same.
    const float swing = std::sin(m_total * 0.9f) * 12.0f * (0.5f + 0.9f * t);
    const sf::Vector2f hook = onPlate(m_rig, rigAt, push, 0.0f, { 0.975f, 0.545f });
    const sf::Vector2f bellAt{ hook.x + swing, hook.y + 46.0f * push };

    bar(hook, bellAt, 2.0f, fade(Steel, 0.80f), fade(Steel, 0.80f));
    plate(target, m_bell, bellAt, 0.30f * push, sf::Color(130, 142, 150), swing * 0.2f);

    // FORE. Two shoulders of rock, which up here is the rig's own steelwork
    // between the camera and the light.
    fore(target, { W * 0.10f + pan * ForePan, ledgeAt(1.6f) }, 1.6f, Shade);
    fore(target, { W * 0.94f + pan * ForePan, ledgeAt(1.9f, 14.0f) }, 1.9f, Shade, 8.0f);

    // Rain in two passes, and the near one is faster. One pass of rain is a
    // texture; two at different speeds is weather.
    motes(m_total, { -110.0f, 900.0f }, 30.0f, fade(Sky, 0.34f),
          90 + static_cast<int>(50.0f * t));
    motes(m_total * 1.4f, { -150.0f, 1200.0f }, 46.0f, fade(Sky, 0.20f), 60);

    flush(target);
}

void Cinematic::drawShutter(sf::RenderTarget& target, float t)
{
    // Under, looking up. The whole shot is the daylight getting further away,
    // and it is the only shot in either reel with a locked lens: the bell has
    // to be the thing that moves.
    const float bright = 1.0f - 0.82f * ease(t);
    const float lid = Top + 46.0f - 96.0f * ease(t);
    const float drift = 130.0f * ease(t);

    wash(sf::Color(26, 54, 66), Deep);

    // BACK. The surface, the rig's legs hanging off it, and the daylight
    // coming through - put out one wedge at a time from the outside in.
    for (int i = 0; i < 26; ++i)
    {
        const float x = static_cast<float>(i) * (W / 25.0f);
        const float lift = std::sin(m_total * 1.4f + static_cast<float>(i) * 0.6f) * 7.0f;

        bar({ x, lid + lift }, { x + W / 25.0f, lid + lift }, 5.0f,
            fade(Sky, 0.55f * bright), fade(Sky, 0.55f * bright));
    }

    for (const float x : { W * 0.30f, W * 0.42f, W * 0.62f, W * 0.74f })
        bar({ x, lid }, { x + 8.0f, lid + 200.0f }, 7.0f,
            fade(sf::Color(12, 20, 26), 0.55f), fade(sf::Color(12, 20, 26), 0.10f));

    for (int i = 0; i < 7; ++i)
    {
        if (t > 0.55f + 0.05f * std::abs(static_cast<float>(i) - 3.0f))
            continue;

        const float x = 120.0f + static_cast<float>(i) * 170.0f;
        wedge({ x, lid }, { 0.12f, 1.0f }, 520.0f, 60.0f, fade(Sky, 0.14f * bright),
              fade(Sky, 0.0f));
    }

    plate(target, m_ledge, { W * 0.08f, ledgeAt(1.8f, 34.0f) + drift * BackPan }, 1.8f,
          sf::Color(10, 15, 18));
    plate(target, m_ledge, { W * 0.94f, ledgeAt(2.1f, 48.0f) + drift * BackPan }, 2.1f,
          sf::Color(10, 15, 18), -6.0f);

    // MID. The bell going down its own cable, with the umbilical beside it.
    const float drop = Top + 110.0f + 330.0f * ease(t);
    const sf::Vector2f bellAt{ W * 0.5f, drop };

    bar({ W * 0.5f, lid }, bellAt, 2.5f, fade(Steel, 0.45f), fade(Steel, 0.85f));

    for (int i = 0; i < 4; ++i)
    {
        const float a = static_cast<float>(i) / 4.0f;
        const float b = static_cast<float>(i + 1) / 4.0f;

        bar({ W * 0.5f + 26.0f + 60.0f * a, lid + (drop - lid) * a },
            { W * 0.5f + 26.0f + 60.0f * b, lid + (drop - lid) * b }, 3.0f,
            fade(sf::Color(40, 38, 42), 0.80f), fade(sf::Color(40, 38, 42), 0.80f));
    }

    plate(target, m_bell, bellAt, 0.72f, sf::Color(150, 164, 172),
          std::sin(m_total * 0.8f) * 3.0f);

    for (const float u : { 0.26f, 0.50f, 0.74f })
        glow(onPlate(m_bell, bellAt, 0.72f, 0.0f, { u, 0.44f }), 22.0f, Lamp, 0.45f);

    const sf::Vector2f front = onPlate(m_bell, bellAt, 0.72f, 0.0f, { 0.50f, 0.70f });
    glow(front, 54.0f, Lamp, 0.34f);
    wedge(front, { 0.0f, 1.0f }, 260.0f, 90.0f, fade(Lamp, 0.12f), fade(Lamp, 0.0f));

    // FORE. One shoulder rising past the lens as the bell drops.
    fore(target, { W * 0.26f, ledgeAt(1.7f) - 46.0f * ease(t) }, 1.7f, Shade);

    motes(m_total, { 20.0f, -260.0f }, 14.0f, fade(Sky, 0.22f), 60);
    flush(target);
}

void Cinematic::drawSounding(sf::RenderTarget& target, float t)
{
    // The longest shot in the game, and the only one with three phases. It
    // falls, then it stops falling because of what is coming up past it, then
    // the lamp turns to look at what is crossing in front of it and goes out.
    const float stopped = beat(t, 0.30f, 0.62f);
    const float turned = beat(t, 0.66f, 0.80f);
    const float dying = beat(t, 0.90f, 0.98f);
    const float beam = 1.0f - dying;

    // Lifted off Deep: at four nine thirteen the whole shot was a black
    // rectangle with a caption on it, and the three boats the shot exists for
    // could not be made out at all.
    wash(sf::Color(30, 44, 52), sf::Color(9, 17, 23));
    flush(target);

    // BACK. Two rock layers off the integrated scroll, so they can be stopped.
    // rock() multiplies the sheet, so a tint that reads as grey on paper comes
    // out at about a third of itself on screen. These are the numbers the
    // shot actually needs, not the ones it looks like it needs.
    rock(target, -m_scroll * 620.0f, 1.9f, sf::Color(154, 168, 176));
    rock(target, -m_scroll * 1150.0f + 220.0f, 3.1f, sf::Color(58, 70, 78, 190));

    const auto gloom = [this](float from, float to) {
        m_mesh.append(sf::Vertex{ { from, Top }, fade(sf::Color::Black, 0.0f) });
        m_mesh.append(sf::Vertex{ { to, Top }, fade(sf::Color::Black, 0.50f) });
        m_mesh.append(sf::Vertex{ { to, Bottom }, fade(sf::Color::Black, 0.50f) });
        m_mesh.append(sf::Vertex{ { from, Top }, fade(sf::Color::Black, 0.0f) });
        m_mesh.append(sf::Vertex{ { to, Bottom }, fade(sf::Color::Black, 0.50f) });
        m_mesh.append(sf::Vertex{ { from, Bottom }, fade(sf::Color::Black, 0.0f) });
    };

    // from is the clear end, to is the dark end: the dark ends are the two
    // sides of the frame. Written the other way round - which is how it was -
    // the shot is a black column with the boats hidden behind it.
    gloom(W * 0.34f, 0.0f);
    gloom(W * 0.66f, W);

    // MID. Three boats come up out of the bottom of the frame together. Their
    // beacons are still lit and their cables are still attached and still
    // going up, which is the entire argument the caption is making.
    if (stopped > 0.0f)
    {
        // They come up and then they stay up: the first pass ran them clean
        // off the top of the frame inside a second and a half, so the shot
        // spent most of its length being an empty wash. They end just under
        // the letterbox and hang there for the rest of it.
        const float rise = H + 150.0f - (H + 92.0f - Top) * stopped;
        constexpr std::array<float, 3> where{ { 0.22f, 0.55f, 0.84f } };
        constexpr std::array<float, 3> lean{ { 12.0f, -7.0f, 24.0f } };

        for (std::size_t i = 0; i < where.size(); ++i)
        {
            const sf::Vector2f at{ W * where[i], rise + static_cast<float>(i) * 34.0f };

            bar({ at.x, at.y - 60.0f }, { at.x - 14.0f, Top - 60.0f }, 1.5f,
                fade(Steel, 0.55f), fade(Steel, 0.0f));

            plate(target, m_wreck, at, 0.90f, dim(sf::Color(118, 128, 136), 0.78f), lean[i]);
            glow(onPlate(m_wreck, at, 0.90f, lean[i], { 0.49f, 0.16f }), 30.0f, Ember, 0.55f);
        }
    }

    // The bell stops swinging when the rock stops moving.
    const float sway = std::sin(m_total * 1.6f) * 16.0f * (1.0f - stopped);
    const sf::Vector2f bellAt{ W * 0.5f + sway, MidY - 30.0f };

    // The lamp swings from pointing down to pointing along, which is a man
    // deciding to look at something instead of at where he is going.
    const sf::Vector2f aim{ turned, 1.0f - turned * 0.98f };

    wedge(bellAt + sf::Vector2f{ 0.0f, 10.0f }, aim,
          (340.0f + 910.0f * turned) * beam, (130.0f + 170.0f * turned) * beam,
          fade(Lamp, 0.16f * beam), fade(Lamp, 0.0f));
    flush(target);

    if (turned > 0.0f)
    {
        const float cross = 1900.0f - 2800.0f * ease(std::clamp((t - 0.66f) / 0.34f, 0.0f, 1.0f));
        const sf::Vector2f at{ cross, MidY + 40.0f + std::sin(m_total * 0.7f) * 18.0f };

        plate(target, m_beast, at, 1.42f,
              dim(sf::Color(96, 88, 116), 0.12f + 0.18f * beam), -4.0f);

        const sf::Vector2f eye = onPlate(m_beast, at, 1.42f, -4.0f, { 0.895f, 0.460f });

        if (beam > 0.05f)
        {
            glow(eye, 70.0f, Ember, 0.30f * beam);
            disc(eye, 10.0f, fade(sf::Color(255, 226, 170), 0.9f * beam));
        }
    }

    plate(target, m_bell, bellAt, 0.46f, sf::Color(140, 152, 160), sway * 0.3f);

    // FORE. Ledges tearing past while the fall lasts, and still once it stops.
    fore(target, { -380.0f, Top - 120.0f + std::fmod(m_scroll * 900.0f, 1400.0f) },
         1.6f, Shade);
    fore(target, { W + 380.0f, Top - 700.0f + std::fmod(m_scroll * 900.0f, 1400.0f) },
         1.6f, Shade, 12.0f);

    // Silt reverses when the fall does: it was streaming up past a falling
    // boat, and a boat that has stopped is just hanging in it.
    motes(m_total, { 0.0f, -1500.0f + 1340.0f * stopped },
          90.0f - 72.0f * stopped, fade(Sky, 0.22f), 70 - static_cast<int>(30.0f * stopped));

    flush(target);

    if (dying > 0.0f)
    {
        m_fill.setSize({ W, H });
        m_fill.setPosition({ 0.0f, 0.0f });
        m_fill.setFillColor(fade(sf::Color::Black, dying * 0.9f));
        target.draw(m_fill);
    }
}

void Cinematic::drawNerith(sf::RenderTarget& target, float t)
{
    // The station, whole, and one of its doors large at frame left from the
    // first frame - so the wide shot and the close shot are one shot instead
    // of a cut. The push holds wide for a moment first: the player has to see
    // all of it before it is taken away from them.
    const float push = 1.0f + 0.55f * beat(t, 0.18f, 0.90f);
    const float pan = 150.0f * ease(t);
    const float floor = Bottom - 40.0f;

    wash(sf::Color(26, 40, 48), sf::Color(7, 13, 17));

    // BACK. The floor, one long rake of light across it, and the black band
    // that is the lip of the thing the next shot opens.
    m_mesh.append(sf::Vertex{ { 0.0f, floor }, sf::Color(44, 54, 58) });
    m_mesh.append(sf::Vertex{ { W, floor }, sf::Color(44, 54, 58) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(14, 19, 21) });
    m_mesh.append(sf::Vertex{ { 0.0f, floor }, sf::Color(44, 54, 58) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(14, 19, 21) });
    m_mesh.append(sf::Vertex{ { 0.0f, H }, sf::Color(14, 19, 21) });

    wedge({ -60.0f, MidY - 130.0f }, { 1.0f, 0.22f }, 1100.0f, 260.0f, fade(Lamp, 0.15f),
          fade(Lamp, 0.0f));

    // MID. The station itself, planted on the floor line as the push grows.
    const sf::Vector2f at{ W * 0.52f + pan * BackPan, floor - 93.6f * push };

    plate(target, m_station, at, push, sf::Color(168, 182, 190));

    // The one lit window - and at more than half way through, something in
    // there turns it off while the camera is looking at it.
    const float lit = 0.30f * (1.0f - beat(t, 0.55f, 0.67f));

    if (lit > 0.001f)
        glow(onPlate(m_station, at, push, 0.0f, { 0.370f, 0.440f }), 88.0f * push, Ember, lit);

    // A second man, a long way back, face down, with no lamp on. One is a
    // tragedy and two is a pattern.
    plate(target, m_diver, { W * 0.30f + pan * 0.4f, MidY + 40.0f }, 0.38f,
          dim(sf::Color(128, 138, 146), 0.75f), -64.0f);

    // MID-NEAR. One door of it, close, growing across the shot.
    const float door = 0.70f + 0.55f * ease(t);
    const sf::Vector2f doorAt{ W * 0.12f + pan * 1.4f, MidY + 30.0f };

    plate(target, m_hatch, doorAt, door, sf::Color(122, 134, 142));

    wedge({ W + 60.0f, Top + 30.0f }, { -1.0f, 0.26f }, 1000.0f, 230.0f, fade(Lamp, 0.14f),
          fade(Lamp, 0.0f));

    // The drifting man, with his lamp still burning.
    const float turn = std::sin(m_total * 0.35f) * 22.0f - 12.0f;
    const float lift = std::sin(m_total * 0.5f) * 14.0f;
    const sf::Vector2f manAt{ W * 0.74f + pan * 0.6f, MidY - 110.0f + lift };

    plate(target, m_diver, manAt, 0.92f * push * 0.55f, sf::Color(96, 104, 110), turn);
    glow(onPlate(m_diver, manAt, 0.92f * push * 0.55f, turn, { 0.70f, 0.20f }), 26.0f, Lamp,
         0.45f);

    // FORE. Two shoulders sliding out of frame as the camera comes in.
    fore(target, { W * 0.04f - pan * ForePan * 0.3f, ledgeAt(1.8f) }, 1.8f,
         sf::Color(5, 8, 10));
    fore(target, { W * 1.00f + pan * ForePan * 0.3f, ledgeAt(2.0f, 20.0f) }, 2.0f,
         sf::Color(5, 8, 10), 10.0f);

    motes(m_total, { -30.0f, 50.0f }, 9.0f, fade(Sky, 0.12f), 60);
    flush(target);
}

void Cinematic::drawTrench(sf::RenderTarget& target, float t)
{
    // The floor under the station comes apart. What is in it is never shown:
    // two lights come on a long way apart, and then two more, very much
    // further apart than the first pair.
    const float gap = 30.0f + 470.0f * ease(t);
    const float floor = MidY + 60.0f - 90.0f * ease(t);
    const float push = 1.0f + 0.18f * beat(t, 0.70f, 1.0f);

    wash(sf::Color(7, 12, 16), sf::Color(0, 0, 0));

    // BACK. The underside of the station across the top, so the hole opens
    // under something the player recognises rather than in nothing at all.
    plate(target, m_station, { W * 0.46f, Top - 120.0f }, 1.30f, sf::Color(22, 28, 32));

    // MID. The floor, torn rather than parted: the inner edge of each slab
    // steps instead of running straight.
    const auto slab = [this, floor](float from, float to, float seed) {
        constexpr int Steps = 9;

        for (int i = 0; i < Steps; ++i)
        {
            const float a = from + (to - from) * static_cast<float>(i) / Steps;
            const float b = from + (to - from) * static_cast<float>(i + 1) / Steps;
            const float ya = floor - wiggle(seed + static_cast<float>(i) * 2.7f) * 26.0f;
            const float yb = floor - wiggle(seed + static_cast<float>(i + 1) * 2.7f) * 26.0f;

            m_mesh.append(sf::Vertex{ { a, ya }, sf::Color(24, 30, 34) });
            m_mesh.append(sf::Vertex{ { b, yb }, sf::Color(24, 30, 34) });
            m_mesh.append(sf::Vertex{ { b, H }, sf::Color(6, 9, 11) });

            m_mesh.append(sf::Vertex{ { a, ya }, sf::Color(24, 30, 34) });
            m_mesh.append(sf::Vertex{ { b, H }, sf::Color(6, 9, 11) });
            m_mesh.append(sf::Vertex{ { a, H }, sf::Color(6, 9, 11) });
        }
    };

    slab(-20.0f, W * 0.5f - gap * 0.5f, 3.1f);
    slab(W + 20.0f, W * 0.5f + gap * 0.5f, 11.7f);

    // Fourteen pieces of floor going into it. A hole is only deep if
    // something falls in it.
    for (int i = 0; i < 14; ++i)
    {
        const float seed = static_cast<float>(i) * 1.9f;
        const float side = (i % 2) == 0 ? -1.0f : 1.0f;
        const float drop = std::clamp((t - 0.30f - wiggle(seed) * 0.30f) / 0.40f, 0.0f, 1.0f);

        if (drop <= 0.0f)
            continue;

        disc({ W * 0.5f + side * (gap * 0.5f - wiggle(seed + 0.4f) * 40.0f),
               floor - 10.0f + 380.0f * drop },
             4.0f + wiggle(seed + 0.8f) * 9.0f, sf::Color(20, 26, 30), 8);
    }

    // Two eyes, and then two more, very much wider apart.
    const float near = beat(t, 0.55f, 0.78f);
    const float far = beat(t, 0.80f, 1.0f);

    for (const float side : { -1.0f, 1.0f })
    {
        if (near > 0.0f)
        {
            const sf::Vector2f eye{ W * 0.5f + side * gap * 0.26f, floor + 120.0f };
            glow(eye, 86.0f, Ember, 0.24f * near);
            disc(eye, 11.0f, fade(sf::Color(255, 216, 158), 0.75f * near));
        }

        if (far > 0.0f)
        {
            const sf::Vector2f eye{ W * 0.5f + side * gap * 0.62f, floor + 168.0f };
            glow(eye, 110.0f, Ember, 0.20f * far);
            disc(eye, 13.0f, fade(sf::Color(255, 216, 158), 0.70f * far));
        }
    }

    // The bell backs away from it.
    const sf::Vector2f bellAt{ W * 0.5f, MidY - 170.0f - 30.0f * ease(t) };

    wedge(bellAt + sf::Vector2f{ 0.0f, 20.0f }, { 0.0f, 1.0f }, 300.0f, 120.0f,
          fade(Lamp, 0.13f), fade(Lamp, 0.0f));
    flush(target);

    plate(target, m_bell, bellAt, 0.40f / push, sf::Color(130, 142, 150));

    // FORE.
    fore(target, { W * 0.08f, ledgeAt(1.8f) - 40.0f * ease(t) }, 1.8f, Shade);
    fore(target, { W * 0.96f, ledgeAt(2.0f, 16.0f) - 40.0f * ease(t) }, 2.0f, Shade, -8.0f);

    motes(m_total, { 0.0f, 120.0f }, 10.0f, fade(Sky, 0.12f), 50);
    flush(target);
}

void Cinematic::drawMark(sf::RenderTarget& target, float t)
{
    // Not a blob for five seconds. The animal is at the threshold of visible,
    // filling the bottom of the frame, with nothing lit on it at all.
    wash(Deep, sf::Color(0, 0, 0));
    glow({ W * 0.5f, MidY }, 460.0f - 160.0f * ease(t), Water, 0.40f);
    flush(target);

    const bool descent = m_shots.size() > 5;

    if (descent)
    {
        plate(target, m_beast, { W * 0.5f, MidY + 150.0f - 40.0f * ease(t) }, 2.0f,
              sf::Color(14, 17, 22), 2.0f);
    }
    else
    {
        plate(target, m_rig, { W * 0.5f, Top + 130.0f }, 0.90f, sf::Color(14, 17, 22));
    }

    motes(m_time, { 0.0f, -90.0f }, 12.0f, fade(Sky, 0.10f), 40);
    flush(target);
}

// ---------------------------------------------------------------------------
//  The way back up
// ---------------------------------------------------------------------------

void Cinematic::drawAscent(sf::RenderTarget& target, float t)
{
    // The floor opens underneath and then falls away, and the climb starts
    // from a standstill. Everything that lived down there leaves at the same
    // time you do, and most of it is faster.
    const float opening = beat(t, 0.0f, 0.30f);
    const float away = beat(t, 0.30f, 0.80f);

    wash(sf::Color(20 + static_cast<int>(10.0f * away), 30 + static_cast<int>(16.0f * away),
                   38 + static_cast<int>(18.0f * away)),
         sf::Color(7, 13, 17));
    flush(target);

    // BACK. Rock, once the climb has started.
    if (away > 0.0f)
    {
        rock(target, m_scroll * 540.0f, 2.1f, dim(sf::Color(140, 154, 162), away));
        rock(target, m_scroll * 980.0f - 140.0f, 3.4f,
             dim(sf::Color(52, 62, 70, 190), away));
    }

    // MID. The floor, opening and then dropping out of frame.
    const float floor = MidY - 30.0f + 900.0f * away;
    const float gap = 120.0f + 560.0f * opening;

    const auto slab = [this, floor](float from, float to) {
        m_mesh.append(sf::Vertex{ { from, floor }, sf::Color(58, 68, 74) });
        m_mesh.append(sf::Vertex{ { to, floor }, sf::Color(58, 68, 74) });
        m_mesh.append(sf::Vertex{ { to, Top }, sf::Color(18, 24, 28) });
        m_mesh.append(sf::Vertex{ { from, floor }, sf::Color(58, 68, 74) });
        m_mesh.append(sf::Vertex{ { to, Top }, sf::Color(18, 24, 28) });
        m_mesh.append(sf::Vertex{ { from, Top }, sf::Color(18, 24, 28) });
    };

    if (away < 0.98f)
    {
        slab(-20.0f, W * 0.5f - gap * 0.5f);
        slab(W + 20.0f, W * 0.5f + gap * 0.5f);

        for (int i = 0; i < 5; ++i)
        {
            const float x = W * 0.5f + (static_cast<float>(i) - 2.0f) * gap * 0.22f;
            wedge({ x, floor }, { 0.0f, 1.0f }, 560.0f, 150.0f,
                  fade(Ember, 0.18f * opening * (1.0f - away)), fade(Ember, 0.0f));
        }
    }

    flush(target);

    // The traffic. Twelve circles with a dot on them was the cheapest thing in
    // either reel; twenty silhouettes at twenty sizes and twenty rotations is
    // the same code and it is an evacuation.
    const int count = 6 + static_cast<int>(14.0f * ease(t));

    for (int i = 0; i < count; ++i)
    {
        const float seed = static_cast<float>(i) * 3.3f;
        const float x = 100.0f + wiggle(seed) * (W - 200.0f);
        const float speed = 200.0f + wiggle(seed + 0.5f) * 520.0f;
        const float y = H + 140.0f
                        - std::fmod(m_total * speed + wiggle(seed + 0.9f) * 1100.0f,
                                    H + 340.0f);

        plate(target, m_beast, { x, y }, 0.10f + wiggle(seed + 0.2f) * 0.22f,
              fade(sf::Color(20, 18, 26), 0.90f), wiggle(seed + 0.4f) * 360.0f);
    }

    const float sway = std::sin(m_total * 1.3f) * 14.0f;
    const sf::Vector2f bellAt{ W * 0.5f + sway, MidY + 20.0f };

    wedge(bellAt, { 0.0f, -1.0f }, 300.0f, 110.0f, fade(Lamp, 0.14f), fade(Lamp, 0.0f));
    flush(target);

    plate(target, m_bell, bellAt, 0.46f, sf::Color(140, 152, 160), sway * 0.3f);

    // FORE.
    fore(target, { -380.0f, Top - 200.0f + std::fmod(-m_scroll * 900.0f, 1400.0f) },
         1.6f, Shade);
    fore(target, { W + 380.0f, Top - 800.0f + std::fmod(-m_scroll * 900.0f, 1400.0f) },
         1.6f, Shade, -10.0f);

    motes(m_total, { 0.0f, 1200.0f * away }, 70.0f, fade(Sky, 0.18f), 60);
    flush(target);
}

void Cinematic::drawDaylight(sf::RenderTarget& target, float t)
{
    // The surface from underneath, with the rig standing in it. The first warm
    // light in the whole game, and the first living thing in it that is not
    // trying to kill you.
    const float lid = Top + 104.0f - 62.0f * ease(t);

    wash(sf::Color(22 + static_cast<int>(30.0f * ease(t)),
                   52 + static_cast<int>(40.0f * ease(t)),
                   66 + static_cast<int>(38.0f * ease(t))),
         sf::Color(6, 14, 20));

    // BACK. The ceiling of the world, and the sun coming through it.
    for (int i = 0; i < 30; ++i)
    {
        const float x = static_cast<float>(i) * (W / 29.0f);
        const float lift = std::sin(m_total * 1.2f + static_cast<float>(i) * 0.5f) * 9.0f;

        bar({ x, lid + lift }, { x + W / 29.0f, lid + lift }, 6.0f,
            fade(sf::Color(226, 206, 168), 0.72f), fade(sf::Color(226, 206, 168), 0.72f));
    }

    for (int i = 0; i < 9; ++i)
    {
        const float x = 60.0f + static_cast<float>(i) * 150.0f;
        wedge({ x, lid }, { -0.08f, 1.0f }, 620.0f, 70.0f,
              fade(sf::Color(236, 214, 170), 0.16f), fade(Ember, 0.0f));
    }

    flush(target);

    // MID. The rig itself rather than four bars, so its real legs and their
    // bracing hang down into the frame.
    plate(target, m_rig, { W * 0.5f, lid - 6.0f }, 0.62f, sf::Color(40, 50, 58));

    // A shoal, going the other way. Nothing in this game has ever done that.
    for (int i = 0; i < 26; ++i)
    {
        const float seed = static_cast<float>(i) * 2.1f;
        const float x = std::fmod(wiggle(seed) * W + W + 200.0f - m_total * 90.0f, W + 240.0f)
                        - 120.0f;
        const float y = lid + 120.0f + wiggle(seed + 0.6f) * 300.0f
                        + std::sin(m_total * 1.6f + seed) * 10.0f;

        disc({ x, y }, 3.0f + wiggle(seed + 0.3f) * 4.0f,
             fade(sf::Color(210, 196, 158), 0.50f), 7);
    }

    const sf::Vector2f bellAt{ W * 0.5f - 120.0f, Bottom - 60.0f - 300.0f * ease(t) };

    // Its lamp goes out on the way up. The first time in the game that not
    // needing the lamp is a thing that happens.
    glow(onPlate(m_bell, bellAt, 0.68f, 0.0f, { 0.50f, 0.70f }), 60.0f, Lamp,
         0.40f - 0.35f * ease(t));
    flush(target);

    plate(target, m_bell, bellAt, 0.68f, sf::Color(168, 180, 186));

    // FORE. The last of the dark leaving the bottom of frame.
    fore(target, { W * 0.14f, ledgeAt(1.7f) + 70.0f * ease(t) }, 1.7f,
         sf::Color(8, 12, 16));

    motes(m_total, { 10.0f, -320.0f }, 18.0f, fade(sf::Color(226, 214, 180), 0.30f), 80);
    flush(target);
}

void Cinematic::drawCount(sf::RenderTarget& target, float t)
{
    // The deck, in the rain, with the hold being weighed. This is the only
    // shot in the game where anything goes right, and it lasts seven seconds.
    const float pan = -56.0f * ease(t);
    const float deck = MidY + 110.0f;

    wash(sf::Color(56, 62, 74), sf::Color(24, 30, 38));

    // BACK. Dawn, and the sea beyond the edge of the plate.
    glow({ W * 0.78f + pan * BackPan, deck - 30.0f }, 320.0f, sf::Color(236, 178, 118), 0.34f);

    m_mesh.append(sf::Vertex{ { 0.0f, deck - 120.0f }, sf::Color(40, 58, 70) });
    m_mesh.append(sf::Vertex{ { W, deck - 120.0f }, sf::Color(40, 58, 70) });
    m_mesh.append(sf::Vertex{ { W, deck }, sf::Color(22, 34, 44) });
    m_mesh.append(sf::Vertex{ { 0.0f, deck - 120.0f }, sf::Color(40, 58, 70) });
    m_mesh.append(sf::Vertex{ { W, deck }, sf::Color(22, 34, 44) });
    m_mesh.append(sf::Vertex{ { 0.0f, deck }, sf::Color(22, 34, 44) });

    for (int i = 0; i < 7; ++i)
    {
        const float y = deck - 110.0f + static_cast<float>(i) * 15.0f;
        const float slide = std::sin(m_total * 0.8f + static_cast<float>(i)) * 16.0f;
        bar({ -20.0f + slide, y }, { W + 20.0f + slide, y }, 1.6f, fade(Sky, 0.10f),
            fade(Sky, 0.04f));
    }

    // The plate itself.
    m_mesh.append(sf::Vertex{ { 0.0f, deck }, sf::Color(74, 80, 84) });
    m_mesh.append(sf::Vertex{ { W, deck }, sf::Color(74, 80, 84) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(42, 48, 52) });
    m_mesh.append(sf::Vertex{ { 0.0f, deck }, sf::Color(74, 80, 84) });
    m_mesh.append(sf::Vertex{ { W, H }, sf::Color(42, 48, 52) });
    m_mesh.append(sf::Vertex{ { 0.0f, H }, sf::Color(42, 48, 52) });

    // MID. A derrick drawn from bars, because the deck's crane is a top down
    // sprite and this shot is in profile.
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

    // The hold, landing one crate at a time in a stack rather than scattered.
    constexpr std::array<sf::Vector2f, 6> stack{ {
        { 0.40f, -26.0f }, { 0.52f, -26.0f }, { 0.64f, -26.0f },
        { 0.46f, -76.0f }, { 0.58f, -76.0f }, { 0.52f, -126.0f },
    } };

    for (std::size_t i = 0; i < stack.size(); ++i)
    {
        const float show = std::clamp((t - 0.10f - static_cast<float>(i) * 0.10f) * 5.0f,
                                      0.0f, 1.0f);
        if (show <= 0.0f)
            continue;

        const sf::Vector2f at{ W * stack[i].x + pan,
                               deck + stack[i].y - 52.0f * (1.0f - show) };

        plate(target, m_crate, at, 1.35f, dim(sf::Color(160, 166, 170), show));
    }

    // Two men: one counting, one on the crane pedestal.
    plate(target, m_diver, { W * 0.86f + pan, deck - 92.0f }, 0.95f, sf::Color(142, 148, 136),
          std::sin(m_total * 0.6f) * 3.0f);
    plate(target, m_diver, { W * 0.26f + pan, deck - 70.0f }, 0.62f, sf::Color(110, 116, 106),
          4.0f);

    // FORE. The camera is standing behind the rail watching this.
    fore(target, { W * 0.24f + pan * ForePan * 0.2f, ledgeAt(1.6f, 30.0f) }, 1.6f,
         sf::Color(22, 26, 32));
    fore(target, { W * 0.92f + pan * ForePan * 0.2f, ledgeAt(1.8f, 44.0f) }, 1.8f,
         sf::Color(18, 22, 28), 6.0f);

    // At the end of it, a light on the water beyond the deck edge, which is
    // the next shot arriving before the cut does.
    if (t > 0.90f)
        glow({ W * 0.64f, deck - 96.0f }, 60.0f, sf::Color(40, 54, 76), 0.35f);

    motes(m_total, { -70.0f, 700.0f }, 24.0f, fade(Sky, 0.24f), 90);
    flush(target);
}

void Cinematic::drawWake(sf::RenderTarget& target, float t)
{
    // Black, and then not black. The rig is a shape a long way up with the
    // bell back on its hook - you are out of the water and safe, which is the
    // joke - and the thing under it is most of the frame.
    const float lid = Top + 76.0f;

    wash(sf::Color(22, 34, 44), sf::Color(4, 6, 9));

    for (int i = 0; i < 20; ++i)
    {
        const float x = static_cast<float>(i) * (W / 19.0f);
        const float lift = std::sin(m_total * 0.9f + static_cast<float>(i) * 0.7f) * 5.0f;

        bar({ x, lid + lift }, { x + W / 19.0f, lid + lift }, 3.0f,
            fade(sf::Color(180, 176, 156), 0.30f), fade(sf::Color(180, 176, 156), 0.30f));
    }

    flush(target);

    plate(target, m_rig, { W * 0.5f, lid - 30.0f }, 0.40f, sf::Color(88, 100, 112));
    plate(target, m_bell, { W * 0.5f + 84.0f, lid + 14.0f }, 0.10f, sf::Color(70, 80, 90));

    // FORE. It comes up out of the bottom of the frame and stops. Lit just
    // enough to be a shape: a thing you cannot see at all is not frightening,
    // it is a black screen.
    const float lift = 400.0f - 340.0f * ease(std::min(1.0f, t * 1.25f));
    const sf::Vector2f at{ W * 0.5f, Bottom + lift };

    glow(at, 660.0f, sf::Color(62, 84, 116), 0.72f);
    flush(target);

    plate(target, m_beast, at, 1.70f, sf::Color(86, 80, 108), 3.0f);

    // One eye opens. Then a second, half the size, further along the body -
    // and the last beat of the game is that there were always two.
    const float first = beat(t, 0.50f, 0.80f);
    const float second = beat(t, 0.82f, 1.0f);

    if (first > 0.0f)
    {
        const sf::Vector2f eye = onPlate(m_beast, at, 1.70f, 3.0f, { 0.895f, 0.460f });

        glow(eye, 150.0f, Ember, 0.26f * first);
        disc(eye, 26.0f * first, fade(sf::Color(255, 222, 168), 0.9f * first));
        disc(eye, 9.0f * first, fade(sf::Color::Black, 0.9f * first));
    }

    if (second > 0.0f)
    {
        const sf::Vector2f eye = onPlate(m_beast, at, 1.70f, 3.0f, { 0.845f, 0.590f });

        glow(eye, 90.0f, Ember, 0.22f * second);
        disc(eye, 13.0f * second, fade(sf::Color(255, 222, 168), 0.9f * second));
        disc(eye, 5.0f * second, fade(sf::Color::Black, 0.9f * second));
    }

    flush(target);
}

// ---------------------------------------------------------------------------
//  The two transits
// ---------------------------------------------------------------------------

void Cinematic::drawFall(sf::RenderTarget& target, float)
{
    wash(sf::Color(14, 20, 24), Deep);
    flush(target);

    rock(target, -m_scroll * 620.0f, 1.9f, sf::Color(76, 86, 92));
    rock(target, -m_scroll * 1150.0f + 220.0f, 3.1f, sf::Color(26, 32, 36, 190));

    const float sway = std::sin(m_total * 1.6f) * 16.0f;
    const sf::Vector2f bellAt{ W * 0.5f + sway, MidY - 30.0f };

    wedge(bellAt + sf::Vector2f{ 0.0f, 10.0f }, { 0.0f, 1.0f }, 340.0f, 130.0f,
          fade(Lamp, 0.16f), fade(Lamp, 0.0f));
    flush(target);

    plate(target, m_bell, bellAt, 0.46f, sf::Color(140, 152, 160), sway * 0.3f);
    fore(target, { -380.0f, Top - 120.0f + std::fmod(m_scroll * 900.0f, 1400.0f) },
         1.6f, Shade);

    motes(m_total, { 0.0f, -1500.0f }, 90.0f, fade(Sky, 0.22f), 70);
    flush(target);
}

void Cinematic::drawClimb(sf::RenderTarget& target, float)
{
    wash(sf::Color(12, 22, 28), Deep);
    flush(target);

    rock(target, m_scroll * -540.0f, 2.1f, sf::Color(50, 58, 64));
    rock(target, m_scroll * -980.0f - 140.0f, 3.4f, sf::Color(22, 28, 32, 190));

    const float sway = std::sin(m_total * 1.3f) * 14.0f;
    const sf::Vector2f bellAt{ W * 0.5f + sway, MidY + 20.0f };

    wedge(bellAt, { 0.0f, -1.0f }, 300.0f, 110.0f, fade(Lamp, 0.14f), fade(Lamp, 0.0f));
    flush(target);

    plate(target, m_bell, bellAt, 0.46f, sf::Color(140, 152, 160), sway * 0.3f);
    fore(target, { W + 380.0f, Top - 300.0f + std::fmod(-m_scroll * 900.0f, 1400.0f) },
         1.6f, Shade, -10.0f);

    motes(m_total, { 0.0f, 1200.0f }, 70.0f, fade(Sky, 0.18f), 60);
    flush(target);
}

void Cinematic::drawScene(sf::RenderTarget& target)
{
    const float t = shotTime();

    switch (current().scene)
    {
        case Scene::Rig:      drawRig(target, t);      break;
        case Scene::Shutter:  drawShutter(target, t);  break;
        case Scene::Sounding: drawSounding(target, t); break;
        case Scene::Nerith:   drawNerith(target, t);   break;
        case Scene::Trench:   drawTrench(target, t);   break;
        case Scene::Mark:     drawMark(target, t);     break;
        case Scene::Ascent:   drawAscent(target, t);   break;
        case Scene::Daylight: drawDaylight(target, t); break;
        case Scene::Count:    drawCount(target, t);    break;
        case Scene::Wake:     drawWake(target, t);     break;
        case Scene::Fall:     drawFall(target, t);     break;
        case Scene::Climb:    drawClimb(target, t);    break;
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

        // Six shots is the descent; anything shorter is the way back up.
        const std::string under = m_shots.size() > 5 ? "YOU ARE THE FOURTH"
                                                     : "NOBODY IS SENDING A FIFTH";
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
