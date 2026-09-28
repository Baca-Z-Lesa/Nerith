#include "effects/WaterPass.h"

#include "core/Config.h"

#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/View.hpp>

#include <algorithm>

namespace
{
    // GLSL 1.10, the oldest the fixed-function vertex path will take, so it
    // compiles on anything SFML itself runs on.
    //
    // Texture coordinates arrive with the top left at 0,0 whatever way up the
    // texture is stored: SFML folds the flip into the texture matrix.
    constexpr const char* Source = R"(
uniform sampler2D frame;
uniform vec2  size;        // pixels
uniform float scale;       // pixels per pixel of the 1280x720 frame
uniform float time;

uniform vec2  lamp;        // pixels, top left origin
uniform float reach;       // pixels
uniform vec3  absorbFar;
uniform float absorbClear; // fractions of the reach
uniform float absorbFull;

uniform float ripple;
uniform float scatter;
uniform float scatterRadius;
uniform float scatterFloor;

uniform float hurt;
uniform float hurtSplit;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec2 px = uv * size;
    vec2 base = px / scale;

    // The water between the glass and the scene. Two waves on each axis that
    // do not share a period, so it never visibly repeats.
    vec2 swim = vec2(sin(base.y * 0.043 + time * 1.30) + sin(base.y * 0.017 - time * 0.70),
                     cos(base.x * 0.039 + time * 1.10) + cos(base.x * 0.013 + time * 0.60));
    uv += swim * 0.5 * ripple * scale / size;

    // A bite: red one way, blue the other, out from the middle of the frame.
    vec2 out_ = uv - vec2(0.5);
    vec2 split = (length(out_) > 0.0001 ? normalize(out_) : vec2(0.0))
               * hurt * hurtSplit * scale / size;

    vec3 colour = vec3(texture2D(frame, uv + split).r,
                       texture2D(frame, uv).g,
                       texture2D(frame, uv - split).b);

    // Light off whatever is hanging in the water. Twelve taps on two rings,
    // only the part of each above the floor, so the dark stays dark and a
    // lit thing gets a halo of itself. Bluer on the way out: the water took
    // the red out of it before it got here.
    vec3 glow = vec3(0.0);
    float r = scatterRadius * scale;
    for (int i = 0; i < 12; ++i)
    {
        float a = float(i) * 0.5236 + 0.26;
        float ring = mod(float(i), 2.0) < 0.5 ? 0.5 : 1.0;
        vec2 o = vec2(cos(a), sin(a)) * r * ring / size;
        glow += max(texture2D(frame, uv + o).rgb - vec3(scatterFloor), vec3(0.0));
    }
    colour += glow / 12.0 * scatter * vec3(0.62, 0.88, 1.0);

    // What the water took out of the light on the way here. Nothing near the
    // sub, all of it past the end of the lamp - the same curve the mesh drew,
    // but per pixel, so it has no rings in it.
    float d = length(px - lamp);
    float k = smoothstep(absorbClear * reach, absorbFull * reach, d);
    colour *= mix(vec3(1.0), absorbFar, k);

    // Half a step of noise, so a slow gradient into the dark is a gradient.
    float n = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
    colour += (n - 0.5) / 255.0;

    gl_FragColor = vec4(colour, 1.0);
}
)";
}

WaterPass::WaterPass()
{
    if (!sf::Shader::isAvailable())
        return;

    m_ready = m_shader.loadFromMemory(Source, sf::Shader::Type::Fragment);
    if (!m_ready)
        return;

    // The parts that never change are set once.
    m_shader.setUniform("frame", sf::Shader::CurrentTexture);
    m_shader.setUniform("absorbFar", sf::Glsl::Vec3(config::AbsorbRed, config::AbsorbGreen,
                                                    config::AbsorbBlue));
    m_shader.setUniform("absorbClear", config::AbsorbClear);
    m_shader.setUniform("absorbFull", config::AbsorbFull);
    m_shader.setUniform("ripple", config::WaterRipple);
    m_shader.setUniform("scatter", config::WaterScatter);
    m_shader.setUniform("scatterRadius", config::WaterScatterRadius);
    m_shader.setUniform("scatterFloor", config::WaterScatterFloor);
    m_shader.setUniform("hurtSplit", config::WaterHurtSplit);
}

void WaterPass::apply(sf::RenderWindow& window, const Frame& frame)
{
    if (!m_ready)
        return;

    const sf::Vector2u size = window.getSize();
    if (m_grab.getSize() != size && !m_grab.resize(size))
        return;

    m_grab.update(window);
    draw(window, m_grab, frame);
}

void WaterPass::draw(sf::RenderTarget& target, const sf::Texture& picture, const Frame& frame)
{
    if (!m_ready)
        return;

    const sf::Vector2f size(target.getSize());

    // The frame is letterboxed into the window, so what a pixel of the 1280x720
    // frame is worth is the smaller of the two ratios.
    const float scale = std::min(size.x / static_cast<float>(config::WindowWidth),
                                 size.y / static_cast<float>(config::WindowHeight));

    m_shader.setUniform("size", sf::Glsl::Vec2(size));
    m_shader.setUniform("scale", scale);
    m_shader.setUniform("time", m_clock.getElapsedTime().asSeconds());
    m_shader.setUniform("lamp", sf::Glsl::Vec2(frame.lamp));
    m_shader.setUniform("reach", frame.reach);
    m_shader.setUniform("hurt", frame.hurt);

    // Pixel for pixel over the whole target, letterbox bars included: they are
    // black, and black through this is still black.
    const sf::View before = target.getView();
    target.setView(sf::View(sf::FloatRect({ 0.0f, 0.0f }, size)));

    sf::RenderStates states;
    states.shader = &m_shader;
    states.blendMode = sf::BlendNone;

    target.draw(sf::Sprite(picture), states);
    target.setView(before);
}
