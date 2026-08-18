#include "world/Arena.h"

#include "core/Config.h"
#include "core/Random.h"

#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/System/Angle.hpp>

#include <cmath>
#include <cstdint>

namespace
{
    constexpr std::size_t Segments = 96;

    sf::Angle segmentAngle(std::size_t index)
    {
        return sf::degrees(360.0f * static_cast<float>(index) / static_cast<float>(Segments));
    }

    // Appends one quad of a ring as two triangles.
    void appendRingQuad(sf::VertexArray& array,
                        sf::Vector2f inner0, sf::Vector2f inner1,
                        sf::Vector2f outer0, sf::Vector2f outer1,
                        sf::Color innerColor, sf::Color outerColor)
    {
        array.append(sf::Vertex{ inner0, innerColor });
        array.append(sf::Vertex{ outer0, outerColor });
        array.append(sf::Vertex{ outer1, outerColor });

        array.append(sf::Vertex{ inner0, innerColor });
        array.append(sf::Vertex{ outer1, outerColor });
        array.append(sf::Vertex{ inner1, innerColor });
    }
}

Arena::Arena(sf::Vector2f center, float radius)
    : m_center(center)
    , m_radius(radius)
    , m_time(0.0f)
    , m_lightPool(sf::PrimitiveType::Triangles)
    , m_caustics(sf::PrimitiveType::Triangles)
    , m_gloom(sf::PrimitiveType::Triangles)
{
    buildLightPool();
    buildGloom();
    updateCaustics();
}

void Arena::buildLightPool()
{
    // A soft disc of light sitting over the sand, brightest in the middle and
    // gone by the time it reaches the reef. Blended additively when drawn.
    const sf::Color core = config::ArenaLightColor;
    const sf::Color edge(core.r, core.g, core.b, 0);

    for (std::size_t i = 0; i < Segments; ++i)
    {
        const sf::Angle a0 = segmentAngle(i);
        const sf::Angle a1 = segmentAngle(i + 1);

        // Built around the origin so drawLight can carry it to the sub.
        appendRingQuad(m_lightPool,
                       { 0.0f, 0.0f }, { 0.0f, 0.0f },
                       sf::Vector2f(config::ArenaLightRadius, a0),
                       sf::Vector2f(config::ArenaLightRadius, a1),
                       core, edge);
    }
}

void Arena::buildGloom()
{
    // An ellipse matched to the window's aspect ratio, so the darkness reaches
    // every screen edge at the same distance and the fade looks deliberate.
    const sf::Color clear(config::BackgroundColor.r, config::BackgroundColor.g,
                          config::BackgroundColor.b, 0);
    const sf::Color dark(config::BackgroundColor.r, config::BackgroundColor.g,
                         config::BackgroundColor.b, config::GloomMaxAlpha);

    const auto onEllipse = [this](float semiX, float semiY, std::size_t index) {
        const float radians = segmentAngle(index).asRadians();
        return m_center + sf::Vector2f(semiX * std::cos(radians), semiY * std::sin(radians));
    };

    for (std::size_t i = 0; i < Segments; ++i)
    {
        // Band one: transparent at the inner ellipse, full darkness at the outer.
        appendRingQuad(m_gloom,
                       onEllipse(config::GloomInnerX, config::GloomInnerY, i),
                       onEllipse(config::GloomInnerX, config::GloomInnerY, i + 1),
                       onEllipse(config::GloomOuterX, config::GloomOuterY, i),
                       onEllipse(config::GloomOuterX, config::GloomOuterY, i + 1),
                       clear, dark);

        // Band two: constant darkness out past any window we could be shown in,
        // which is also what clips the caustics back to the lit area.
        appendRingQuad(m_gloom,
                       onEllipse(config::GloomOuterX, config::GloomOuterY, i),
                       onEllipse(config::GloomOuterX, config::GloomOuterY, i + 1),
                       onEllipse(config::GloomOuterX * 4.0f, config::GloomOuterY * 4.0f, i),
                       onEllipse(config::GloomOuterX * 4.0f, config::GloomOuterY * 4.0f, i + 1),
                       dark, dark);
    }
}

void Arena::updateCaustics()
{
    m_caustics.clear();

    const sf::Angle bandAngle = sf::degrees(config::ArenaCausticAngle);
    const sf::Vector2f along(std::cos(bandAngle.asRadians()), std::sin(bandAngle.asRadians()));
    const sf::Vector2f across(-along.y, along.x);

    const float length = m_radius * 2.2f;

    for (std::size_t i = 0; i < config::ArenaCausticBands; ++i)
    {
        // Every band drifts and breathes at its own pace, so they never line up
        // into an obvious repeating pattern.
        const float phase = static_cast<float>(i) * 1.37f;
        const float drift = std::sin(m_time * (0.10f + 0.024f * static_cast<float>(i)) + phase);
        const float offset = drift * m_radius * 0.95f;
        const float width = m_radius * (0.06f + 0.026f * std::sin(m_time * 0.35f + phase));

        const float pulse = 0.45f + 0.55f * std::sin(m_time * 0.5f + phase * 2.1f);
        const auto alpha = static_cast<std::uint8_t>(7.0f + 19.0f * pulse);

        const sf::Color bright(80, 175, 185, alpha);
        const sf::Color fade(80, 175, 185, 0);

        const sf::Vector2f middle = m_center + across * offset;
        const sf::Vector2f start = middle - along * length;
        const sf::Vector2f end = middle + along * length;
        const sf::Vector2f edge = across * width;

        // Two quads per band: transparent edge into a bright core, both sides.
        appendRingQuad(m_caustics, start, end, start - edge, end - edge, bright, fade);
        appendRingQuad(m_caustics, start, end, start + edge, end + edge, bright, fade);
    }
}

void Arena::update(float deltaTime)
{
    m_time += deltaTime;
    updateCaustics();
}

void Arena::drawLight(sf::RenderTarget& target, sf::Vector2f lampCentre) const
{
    sf::RenderStates states;
    states.blendMode = sf::BlendAdd; // light adds to the sand, it does not paint over it

    // The pool is the sub's own lamp, so it travels with the player rather
    // than sitting on the middle of a basin far larger than the screen.
    sf::RenderStates lamp = states;
    lamp.transform.translate(lampCentre);
    target.draw(m_lightPool, lamp);
    target.draw(m_caustics, states);
}

void Arena::drawGloom(sf::RenderTarget& target) const
{
    target.draw(m_gloom);
}

bool Arena::overlaps(sf::Vector2f position, float radius) const
{
    // Compared squared, so there is no square root on a per-projectile check.
    const float reach = m_radius + radius;
    return (position - m_center).lengthSquared() <= reach * reach;
}

sf::Vector2f Arena::clampInside(sf::Vector2f position, float radius) const
{
    const sf::Vector2f delta = position - m_center;
    const float maxDistance = m_radius - radius;

    if (delta.lengthSquared() <= maxDistance * maxDistance)
        return position;

    // Already outside: keep the direction, shorten the vector back to the wall.
    return m_center + delta.normalized() * maxDistance;
}

sf::Vector2f Arena::randomPointInside() const
{
    // The square root keeps the distribution even; without it everything
    // bunches up in the middle.
    const float distance = m_radius * std::sqrt(rng::range(0.0f, 1.0f));
    return m_center + sf::Vector2f(distance, sf::degrees(rng::range(0.0f, 360.0f)));
}
