#include "world/Arena.h"

#include "core/Config.h"
#include "core/Random.h"

#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/System/Angle.hpp>

#include <algorithm>
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

namespace
{
    // Closest point on a segment to a point. The corridors are capsules, and
    // this is the whole of what makes one a capsule rather than a rectangle.
    sf::Vector2f closestOnSegment(sf::Vector2f from, sf::Vector2f to, sf::Vector2f point)
    {
        const sf::Vector2f span = to - from;
        const float lengthSquared = span.lengthSquared();

        if (lengthSquared <= 0.0001f)
            return from;

        const float t = std::clamp((point - from).dot(span) / lengthSquared, 0.0f, 1.0f);
        return from + span * t;
    }

    // A deterministic little generator, so a level is the same map every time
    // it is dived and a different one from every other level. Kept out of
    // rng:: on purpose: the global stream is used by spawning and effects, and
    // a map that changed because a particle was emitted would be a nightmare.
    struct Seeded
    {
        unsigned int state;

        float next()
        {
            state = state * 1664525u + 1013904223u;
            return static_cast<float>((state >> 8) & 0xFFFFFFu) / 16777216.0f;
        }

        float range(float low, float high) { return low + next() * (high - low); }
    };
}

Arena::Arena(sf::Vector2f center, float radius)
    : m_center(center)
    , m_radius(radius)
    , m_time(0.0f)
    , m_start(center)
    , m_hallCentre(center)
    , m_boundsMin(center)
    , m_boundsMax(center)
    , m_rock(sf::PrimitiveType::Triangles)
    , m_floor(sf::PrimitiveType::Triangles)
    , m_lightPool(sf::PrimitiveType::Triangles)
    , m_caustics(sf::PrimitiveType::Triangles)
    , m_gloom(sf::PrimitiveType::Triangles)
    , m_theme(&themeFor(1))
{
    generate(1, false);
    buildLightPool();
    buildGloom();
    updateCaustics();
}

void Arena::generate(unsigned int seed, bool bossRoom)
{
    Seeded random{ seed * 2654435761u + 12345u };

    m_chambers.clear();
    m_corridors.clear();

    // The first chamber is where the dive starts, and it is always roomy: an
    // opening thirty pixels wide with something already in it is not a fight.
    sf::Vector2f cursor = m_center;
    m_chambers.push_back({ cursor, config::ChamberStartRadius });
    m_start = cursor;

    if (bossRoom)
    {
        // A boss needs one room it can actually turn in. It gets a hall off to
        // one side, so arriving at it is still an approach rather than a cut.
        const float angle = random.range(0.0f, 360.0f);
        const sf::Vector2f hall = cursor
                                  + sf::Vector2f(config::HallDistance, sf::degrees(angle));

        m_chambers.push_back({ hall, config::HallRadius });
        m_corridors.push_back({ cursor, hall, config::CorridorRadius * 1.5f });

        // And two alcoves off the hall, so there is somewhere to break line of
        // fire in a room that is otherwise a circle.
        for (int i = 0; i < 2; ++i)
        {
            const float side = angle + (i == 0 ? 130.0f : -130.0f) + random.range(-20.0f, 20.0f);
            const sf::Vector2f pocket =
                hall + sf::Vector2f(config::HallRadius * 0.95f, sf::degrees(side));

            m_chambers.push_back({ pocket, random.range(130.0f, 190.0f) });
        }
    }
    else
    {
        // A walk of chambers joined nose to tail, with the occasional branch
        // hanging off it. The walk is what makes the map a place to move
        // through; the branches are what stop it being a corridor.
        float heading = random.range(0.0f, 360.0f);

        const int count = config::ChamberCount;
        for (int i = 1; i < count; ++i)
        {
            heading += random.range(-config::ChamberTurn, config::ChamberTurn);

            const float step = random.range(config::ChamberStepMin, config::ChamberStepMax);
            sf::Vector2f next = cursor + sf::Vector2f(step, sf::degrees(heading));

            // Folded back if the walk would leave the world the seabed covers.
            const sf::Vector2f offset = next - m_center;
            if (offset.length() > config::MapExtent)
            {
                heading += 180.0f + random.range(-50.0f, 50.0f);
                next = cursor + sf::Vector2f(step, sf::degrees(heading));
            }

            const float radius = random.range(config::ChamberRadiusMin, config::ChamberRadiusMax);
            m_chambers.push_back({ next, radius });
            m_corridors.push_back({ cursor, next, config::CorridorRadius });

            // Every so often, a dead end worth swimming down: this is where a
            // nest ends up, and where a hunt gets its length from.
            if (random.next() < config::BranchChance)
            {
                const float side = heading + random.range(70.0f, 150.0f)
                                   * (random.next() < 0.5f ? -1.0f : 1.0f);
                const sf::Vector2f pocket =
                    next + sf::Vector2f(random.range(240.0f, 380.0f), sf::degrees(side));

                m_chambers.push_back({ pocket, random.range(150.0f, 240.0f) });
                m_corridors.push_back({ next, pocket, config::CorridorRadius * 0.85f });
            }

            cursor = next;
        }

        // One loop back to somewhere earlier, so the map is not a line. A map
        // you can go round is a map you can lose something in.
        if (m_chambers.size() > 4)
        {
            const std::size_t back = static_cast<std::size_t>(
                random.range(1.0f, static_cast<float>(m_chambers.size()) * 0.4f));

            m_corridors.push_back({ m_chambers.back().centre,
                                    m_chambers[back].centre,
                                    config::CorridorRadius * 0.8f });
        }
    }

    // The hall is simply the biggest room, however the map came out.
    m_hallCentre = m_chambers.front().centre;
    m_hallRadius = m_chambers.front().radius;

    m_boundsMin = m_chambers.front().centre;
    m_boundsMax = m_chambers.front().centre;

    for (const Chamber& chamber : m_chambers)
    {
        if (chamber.radius > m_hallRadius)
        {
            m_hallRadius = chamber.radius;
            m_hallCentre = chamber.centre;
        }

        m_boundsMin.x = std::min(m_boundsMin.x, chamber.centre.x - chamber.radius);
        m_boundsMin.y = std::min(m_boundsMin.y, chamber.centre.y - chamber.radius);
        m_boundsMax.x = std::max(m_boundsMax.x, chamber.centre.x + chamber.radius);
        m_boundsMax.y = std::max(m_boundsMax.y, chamber.centre.y + chamber.radius);
    }

    buildGround();
}

float Arena::depthAt(sf::Vector2f position) const
{
    // Negative inside the water, positive in rock, and the value is how far
    // from the nearest edge. Everything about the map's shape is this number.
    float best = 1.0e9f;

    for (const Chamber& chamber : m_chambers)
        best = std::min(best, (position - chamber.centre).length() - chamber.radius);

    for (const Corridor& corridor : m_corridors)
    {
        const sf::Vector2f on = closestOnSegment(corridor.from, corridor.to, position);
        best = std::min(best, (position - on).length() - corridor.radius);
    }

    return best;
}

bool Arena::isOpen(sf::Vector2f position, float radius) const
{
    return depthAt(position) <= -radius;
}

sf::Vector2f Arena::wallNormal(sf::Vector2f position, float radius) const
{
    if (isOpen(position, radius))
        return { 0.0f, 0.0f };

    // The gradient of the depth field, sampled rather than derived: the field
    // is a min over shapes, so it has creases, and sampling reads them
    // correctly where an analytic derivative would not.
    const float step = 3.0f;
    const float dx = depthAt(position + sf::Vector2f{ step, 0.0f })
                     - depthAt(position - sf::Vector2f{ step, 0.0f });
    const float dy = depthAt(position + sf::Vector2f{ 0.0f, step })
                     - depthAt(position - sf::Vector2f{ 0.0f, step });

    const sf::Vector2f gradient{ dx, dy };
    if (gradient.lengthSquared() <= 0.000001f)
        return { 0.0f, 0.0f };

    // The gradient points into the rock, so back into the water is the other
    // way round.
    return -gradient.normalized();
}

void Arena::buildGround()
{
    m_rock.clear();
    m_floor.clear();

    // Every shape is drawn twice: once inflated, in the colour of cut rock, and
    // once at its own size in sand. Because all the lips go down first and all
    // the floors on top, a lip only survives where no floor covers it - which
    // is exactly the outline of the basin, for free, with no boundary tracing.
    const auto disc = [](sf::VertexArray& into, sf::Vector2f centre, float radius,
                         sf::Color colour) {
        for (std::size_t i = 0; i < Segments; ++i)
        {
            const sf::Angle a0 = segmentAngle(i);
            const sf::Angle a1 = segmentAngle(i + 1);

            into.append(sf::Vertex{ centre, colour });
            into.append(sf::Vertex{ centre + sf::Vector2f(radius, a0), colour });
            into.append(sf::Vertex{ centre + sf::Vector2f(radius, a1), colour });
        }
    };

    const auto capsule = [&disc](sf::VertexArray& into, sf::Vector2f from, sf::Vector2f to,
                                 float radius, sf::Color colour) {
        const sf::Vector2f span = to - from;
        if (span.lengthSquared() <= 0.0001f)
            return;

        const sf::Vector2f across = sf::Vector2f{ -span.y, span.x }.normalized() * radius;

        into.append(sf::Vertex{ from - across, colour });
        into.append(sf::Vertex{ from + across, colour });
        into.append(sf::Vertex{ to + across, colour });
        into.append(sf::Vertex{ from - across, colour });
        into.append(sf::Vertex{ to + across, colour });
        into.append(sf::Vertex{ to - across, colour });

        // The round ends are what make a corridor a capsule, which is what the
        // collision field already assumes it is.
        disc(into, from, radius, colour);
        disc(into, to, radius, colour);
    };

    for (const Chamber& chamber : m_chambers)
    {
        disc(m_rock, chamber.centre, chamber.radius + config::WallLip, config::WallLipColor);
        disc(m_floor, chamber.centre, chamber.radius, m_theme->sandNear);
    }

    for (const Corridor& corridor : m_corridors)
    {
        capsule(m_rock, corridor.from, corridor.to, corridor.radius + config::WallLip,
                config::WallLipColor);
        capsule(m_floor, corridor.from, corridor.to, corridor.radius, m_theme->sandNear);
    }
}

void Arena::drawGround(sf::RenderTarget& target) const
{
    target.draw(m_rock);
    target.draw(m_floor);
}

void Arena::buildLightPool()
{
    // A soft disc of light sitting over the sand, brightest in the middle and
    // gone by the time it reaches the reef. Blended additively when drawn.
    const sf::Color core = m_theme->light;
    const sf::Color edge(core.r, core.g, core.b, 0);

    for (std::size_t i = 0; i < Segments; ++i)
    {
        const sf::Angle a0 = segmentAngle(i);
        const sf::Angle a1 = segmentAngle(i + 1);

        // Built around the origin so drawLight can carry it to the sub.
        appendRingQuad(m_lightPool,
                       { 0.0f, 0.0f }, { 0.0f, 0.0f },
                       sf::Vector2f((m_theme->lightRadius + m_lampBonus) * m_lampFactor, a0),
                       sf::Vector2f((m_theme->lightRadius + m_lampBonus) * m_lampFactor, a1),
                       core, edge);
    }
}

void Arena::buildGloom()
{
    // An ellipse matched to the window's aspect ratio, so the darkness reaches
    // every screen edge at the same distance and the fade looks deliberate.
    const sf::Color clear(m_theme->background.r, m_theme->background.g,
                          m_theme->background.b, 0);
    const sf::Color dark(m_theme->background.r, m_theme->background.g,
                         m_theme->background.b, m_theme->vignetteAlpha);

    const auto onEllipse = [this](float semiX, float semiY, std::size_t index) {
        const float radians = segmentAngle(index).asRadians();
        return m_center + sf::Vector2f(semiX * std::cos(radians), semiY * std::sin(radians));
    };

    for (std::size_t i = 0; i < Segments; ++i)
    {
        // Band one: transparent at the inner ellipse, full darkness at the outer.
        appendRingQuad(m_gloom,
                       onEllipse(config::GloomInnerX * m_theme->vignetteTightness, config::GloomInnerY * m_theme->vignetteTightness, i),
                       onEllipse(config::GloomInnerX * m_theme->vignetteTightness, config::GloomInnerY * m_theme->vignetteTightness, i + 1),
                       onEllipse(config::GloomOuterX * m_theme->vignetteTightness, config::GloomOuterY * m_theme->vignetteTightness, i),
                       onEllipse(config::GloomOuterX * m_theme->vignetteTightness, config::GloomOuterY * m_theme->vignetteTightness, i + 1),
                       clear, dark);

        // Band two: constant darkness out past any window we could be shown in,
        // which is also what clips the caustics back to the lit area.
        appendRingQuad(m_gloom,
                       onEllipse(config::GloomOuterX * m_theme->vignetteTightness, config::GloomOuterY * m_theme->vignetteTightness, i),
                       onEllipse(config::GloomOuterX * m_theme->vignetteTightness, config::GloomOuterY * m_theme->vignetteTightness, i + 1),
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
        const auto alpha = static_cast<std::uint8_t>((7.0f + 19.0f * pulse) * m_theme->causticStrength);

        const sf::Color bright(m_theme->caustic.r, m_theme->caustic.g, m_theme->caustic.b, alpha);
        const sf::Color fade(m_theme->caustic.r, m_theme->caustic.g, m_theme->caustic.b, 0);

        const sf::Vector2f middle = m_center + across * offset;
        const sf::Vector2f start = middle - along * length;
        const sf::Vector2f end = middle + along * length;
        const sf::Vector2f edge = across * width;

        // Two quads per band: transparent edge into a bright core, both sides.
        appendRingQuad(m_caustics, start, end, start - edge, end - edge, bright, fade);
        appendRingQuad(m_caustics, start, end, start + edge, end + edge, bright, fade);
    }
}

void Arena::setLamp(float bonus, float factor)
{
    // Rebuilding the pool every frame would be pointless work, so nothing
    // happens unless the lamp has actually changed size.
    if (std::abs(bonus - m_lampBonus) < 0.5f && std::abs(factor - m_lampFactor) < 0.01f)
        return;

    m_lampBonus = std::max(0.0f, bonus);
    m_lampFactor = std::clamp(factor, 0.05f, 4.0f);
    m_lightPool.clear();
    buildLightPool();
}

void Arena::applyTheme(const DepthTheme& theme)
{
    m_theme = &theme;

    // The lamp and the darkness are the depth, so both are rebuilt outright
    // rather than tinted after the fact.
    m_lightPool.clear();
    m_gloom.clear();
    buildLightPool();
    buildGloom();
    updateCaustics();

    // The floor is the depth's own sand colour, so the ground is rebuilt too.
    buildGround();
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
    return depthAt(position) <= radius;
}

sf::Vector2f Arena::clampInside(sf::Vector2f position, float radius) const
{
    const float depth = depthAt(position);
    if (depth <= -radius)
        return position;

    // Walk out along the field until the circle fits. Two steps is enough for
    // anything moving at a sane speed; the loop is there for the case of a
    // thing that has somehow ended up well inside the rock.
    sf::Vector2f out = position;
    for (int attempt = 0; attempt < 6; ++attempt)
    {
        const float here = depthAt(out);
        if (here <= -radius)
            return out;

        const sf::Vector2f normal = wallNormal(out, radius);
        if (normal.lengthSquared() <= 0.000001f)
            break;

        out += normal * (here + radius + 1.0f);
    }

    // Hopelessly buried: drag it to the nearest chamber rather than leave it
    // stuck. This only happens to something that was spawned badly.
    const Chamber* nearest = nullptr;
    float bestDistance = 1.0e9f;

    for (const Chamber& chamber : m_chambers)
    {
        const float distance = (position - chamber.centre).length() - chamber.radius;
        if (distance < bestDistance)
        {
            bestDistance = distance;
            nearest = &chamber;
        }
    }

    if (nearest == nullptr)
        return position;

    const sf::Vector2f delta = position - nearest->centre;
    const float allowed = std::max(1.0f, nearest->radius - radius);

    if (delta.lengthSquared() <= allowed * allowed)
        return position;

    return nearest->centre + delta.normalized() * allowed;
}

sf::Vector2f Arena::randomPointInside() const
{
    if (m_chambers.empty())
        return m_center;

    // Weighted by area, so a big hall is as likely to be picked as its size
    // deserves rather than as often as a pocket.
    float total = 0.0f;
    for (const Chamber& chamber : m_chambers)
        total += chamber.radius * chamber.radius;

    float roll = rng::range(0.0f, total);
    const Chamber* pick = &m_chambers.front();

    for (const Chamber& chamber : m_chambers)
    {
        roll -= chamber.radius * chamber.radius;
        if (roll <= 0.0f)
        {
            pick = &chamber;
            break;
        }
    }

    const float distance = pick->radius * std::sqrt(rng::range(0.0f, 1.0f));
    return pick->centre + sf::Vector2f(distance, sf::degrees(rng::range(0.0f, 360.0f)));
}

sf::Vector2f Arena::randomEdgePoint(float inset) const
{
    if (m_chambers.empty())
        return m_center;

    // Out against a wall, in a chamber picked at random. Anything that belongs
    // against rock - a nest, a reef prop - wants this rather than open water.
    const Chamber& chamber =
        m_chambers[static_cast<std::size_t>(rng::range(0.0f, static_cast<float>(m_chambers.size())))
                   % m_chambers.size()];

    const float reach = std::max(10.0f, chamber.radius - inset);
    return chamber.centre + sf::Vector2f(reach, sf::degrees(rng::range(0.0f, 360.0f)));
}
