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
    , m_shade(sf::PrimitiveType::Triangles)
    , m_swirl(sf::PrimitiveType::Triangles)
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
    m_pillars.clear();
    m_currents.clear();

    // The first chamber is where the dive starts, and it is always roomy: an
    // opening thirty pixels wide with something already in it is not a fight.
    sf::Vector2f cursor = m_center;
    m_chambers.push_back({ cursor, config::ChamberStartRadius, ChamberKind::Plain,
                           random.range(0.0f, 6.28f) });
    m_start = cursor;

    if (bossRoom)
    {
        // A boss needs one room it can actually turn in. It gets a hall off to
        // one side, so arriving at it is still an approach rather than a cut.
        const float angle = random.range(0.0f, 360.0f);
        const sf::Vector2f hall = cursor
                                  + sf::Vector2f(config::HallDistance, sf::degrees(angle));

        m_chambers.push_back({ hall, config::HallRadius, ChamberKind::Pillars,
                               random.range(0.0f, 6.28f) });
        m_corridors.push_back({ cursor, hall, config::CorridorRadius * 1.5f });

        // And two alcoves off the hall, so there is somewhere to break line of
        // fire in a room that is otherwise a circle.
        for (int i = 0; i < 2; ++i)
        {
            const float side = angle + (i == 0 ? 130.0f : -130.0f) + random.range(-20.0f, 20.0f);
            const sf::Vector2f pocket =
                hall + sf::Vector2f(config::HallRadius * 0.95f, sf::degrees(side));

            m_chambers.push_back({ pocket, random.range(130.0f, 190.0f), ChamberKind::Plain,
                                   random.range(0.0f, 6.28f) });
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

            float radius = random.range(config::ChamberRadiusMin, config::ChamberRadiusMax);

            // What kind of room this is. Rolled once, here, so the corridor
            // into it can be narrowed in the same breath - a throat that is
            // entered through an ordinary mouth is not a throat.
            ChamberKind kind = ChamberKind::Plain;
            const float roll = random.next();
            if (roll < config::ThroatChance)
                kind = ChamberKind::Throat;
            else if (roll < config::ThroatChance + config::PillarChance)
                kind = ChamberKind::Pillars;
            else if (roll < config::ThroatChance + config::PillarChance + config::PitChance)
                kind = ChamberKind::Pit;

            float corridor = config::CorridorRadius;
            if (kind == ChamberKind::Throat)
            {
                // Small and hard to get out of again, down a neck too narrow
                // to turn and fight in. Everything in there has to be dealt
                // with in there.
                radius = std::min(radius, config::ChamberRadiusMin * 0.85f);
                corridor *= config::ThroatCorridorScale;
            }

            m_chambers.push_back({ next, radius, kind, random.range(0.0f, 6.28f) });
            m_corridors.push_back({ cursor, next, corridor });

            // Every so often, a dead end worth swimming down: this is where a
            // nest ends up, and where a hunt gets its length from.
            if (random.next() < config::BranchChance)
            {
                const float side = heading + random.range(70.0f, 150.0f)
                                   * (random.next() < 0.5f ? -1.0f : 1.0f);
                const sf::Vector2f pocket =
                    next + sf::Vector2f(random.range(240.0f, 380.0f), sf::degrees(side));

                m_chambers.push_back({ pocket, random.range(150.0f, 240.0f), ChamberKind::Plain,
                                       random.range(0.0f, 6.28f) });
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

    // Furnishing, once the rooms are final. Pillars and pits both need to know
    // where the walls ended up, so neither can be decided during the walk.
    for (const Chamber& chamber : m_chambers)
    {
        if (chamber.kind == ChamberKind::Pillars)
        {
            const int count = static_cast<int>(random.range(
                static_cast<float>(config::PillarCountMin),
                static_cast<float>(config::PillarCountMax) + 0.999f));

            for (int i = 0; i < count; ++i)
            {
                // Out on a ring rather than anywhere: a column in the middle of
                // a doorway is a blocked room, and a column against the wall is
                // not cover, it is scenery.
                const float angle = random.range(0.0f, 360.0f);
                const float reach = chamber.radius * random.range(0.34f, 0.72f);
                const float size = random.range(config::PillarRadiusMin,
                                                config::PillarRadiusMax);

                m_pillars.push_back({ chamber.centre + sf::Vector2f(reach, sf::degrees(angle)),
                                      size });
            }
        }
        else if (chamber.kind == ChamberKind::Pit)
        {
            m_currents.push_back({ chamber.centre, chamber.radius * 1.05f,
                                   config::PitStrength * random.range(0.8f, 1.25f)
                                       * (random.next() < 0.5f ? -1.0f : 1.0f) });
        }
    }

    buildGround();
}

float Arena::edgeRadius(const Chamber& chamber, sf::Vector2f offset) const
{
    // Two slow waves round the rim, which is enough to stop a basin looking
    // like a diagram of overlapping circles. Cheap on purpose: this runs for
    // every chamber on every query of the field.
    const float theta = std::atan2(offset.y, offset.x);
    const float wave = std::sin(theta * 3.0f + chamber.phase) * 0.62f
                       + std::sin(theta * 7.0f - chamber.phase * 1.7f) * 0.38f;

    return chamber.radius * (1.0f + config::ChamberRipple * wave);
}

sf::Vector2f Arena::currentAt(sf::Vector2f position) const
{
    sf::Vector2f drag{ 0.0f, 0.0f };

    for (const Current& current : m_currents)
    {
        const sf::Vector2f offset = position - current.centre;
        const float distance = offset.length();
        if (distance >= current.radius || distance <= 1.0f)
            continue;

        // Mostly round, a little inward, and dying at the rim so the edge of a
        // pit is somewhere you can hold rather than a line you fall over.
        const float fall = 1.0f - distance / current.radius;
        const sf::Vector2f inward = -offset / distance;
        const sf::Vector2f around{ -inward.y, inward.x };

        drag += (around * 0.78f + inward * 0.34f) * (current.strength * fall);
    }

    return drag;
}


float Arena::depthAt(sf::Vector2f position) const
{
    // Negative inside the water, positive in rock, and the value is how far
    // from the nearest edge. Everything about the map's shape is this number.
    float best = 1.0e9f;

    for (const Chamber& chamber : m_chambers)
    {
        const sf::Vector2f offset = position - chamber.centre;
        best = std::min(best, offset.length() - edgeRadius(chamber, offset));
    }

    for (const Corridor& corridor : m_corridors)
    {
        const sf::Vector2f on = closestOnSegment(corridor.from, corridor.to, position);
        best = std::min(best, (position - on).length() - corridor.radius);
    }

    // Pillars are cut back out of whatever the rooms opened up, so they are a
    // max rather than a min. They are kept narrower than the corridors, which
    // is what stops one ever sealing a route instead of covering it.
    for (const Pillar& pillar : m_pillars)
        best = std::max(best, pillar.radius - (position - pillar.centre).length());

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
    m_shade.clear();

    // The rim of a chamber, wobbled, as a triangle fan. grow pushes it outward,
    // which is how the same outline serves as rock, as cut lip and as floor
    // without any of the three being able to disagree with the other two.
    const auto chamberFan = [this](sf::VertexArray& into, const Chamber& chamber, float grow,
                                   float wave, sf::Color colour) {
        // wave varies the band thickness round the rim, which is what turns a
        // stone shelf into a stone shelf rather than a kerb of even width.
        const auto reach = [&](sf::Vector2f d) {
            const float theta = std::atan2(d.y, d.x);
            return edgeRadius(chamber, d)
                   + grow * (1.0f + wave * std::sin(theta * 5.0f + chamber.phase * 2.1f));
        };

        for (std::size_t i = 0; i < Segments; ++i)
        {
            const sf::Vector2f d0 = sf::Vector2f(1.0f, segmentAngle(i));
            const sf::Vector2f d1 = sf::Vector2f(1.0f, segmentAngle(i + 1));

            into.append(sf::Vertex{ chamber.centre, colour });
            into.append(sf::Vertex{ chamber.centre + d0 * reach(d0), colour });
            into.append(sf::Vertex{ chamber.centre + d1 * reach(d1), colour });
        }
    };

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

    // The mass of the rock is the same rock the seabed is painting everywhere
    // else, so it is mixed from the theme rather than being its own flat grey.
    // A constant here leaves every pocket the shelves do not reach reading as a
    // hole cut in the floor, which is exactly what it used to look like.
    const sf::Color deep(
        static_cast<std::uint8_t>(m_theme->sandNear.r * config::RockTint.r / 255),
        static_cast<std::uint8_t>(m_theme->sandNear.g * config::RockTint.g / 255),
        static_cast<std::uint8_t>(m_theme->sandNear.b * config::RockTint.b / 255));

    // Three passes outward in, all of the first before any of the second, or a
    // neighbouring room paints its rock over this one's cut edge. The mass of
    // the rock goes down darkest and widest, the cut sits on it, and the floor
    // covers both wherever there is water, so the outline of the basin falls
    // out of the draw order rather than having to be traced.
    for (const Chamber& chamber : m_chambers)
        chamberFan(m_rock, chamber, config::WallLip + config::WallShelf, 0.45f, deep);

    for (const Corridor& corridor : m_corridors)
        capsule(m_rock, corridor.from, corridor.to,
                corridor.radius + config::WallLip + config::WallShelf, deep);

    for (const Chamber& chamber : m_chambers)
        chamberFan(m_rock, chamber, config::WallLip, 0.70f, config::WallLipColor);

    for (const Corridor& corridor : m_corridors)
        capsule(m_rock, corridor.from, corridor.to, corridor.radius + config::WallLip,
                config::WallLipColor);

    // Boulders slumped along the foot of the wall, each about to be half buried
    // by the floor. They exist so that the rim is never a curve; nothing can
    // ever touch one, because they sit in rock.
    {
        Seeded rubble{ 0x9E3779B9u };

        for (const Chamber& chamber : m_chambers)
        {
            for (int i = 0; i < config::BouldersPerChamber; ++i)
            {
                const sf::Vector2f d = sf::Vector2f(1.0f, sf::degrees(rubble.range(0.0f, 360.0f)));
                const float out = edgeRadius(chamber, d) + rubble.range(-6.0f, config::WallLip);

                disc(m_rock, chamber.centre + d * out,
                     rubble.range(config::BoulderRadiusMin, config::BoulderRadiusMax),
                     config::WallLipColor);
            }
        }
    }

    for (const Chamber& chamber : m_chambers)
        chamberFan(m_floor, chamber, 0.0f, 0.0f, m_theme->sandNear);

    for (const Corridor& corridor : m_corridors)
        capsule(m_floor, corridor.from, corridor.to, corridor.radius, m_theme->sandNear);

    buildWallShading();
}

void Arena::buildWallShading()
{
    const sf::Color shade = config::WallShadeColor;
    const sf::Color shadeClear(shade.r, shade.g, shade.b, 0);
    const sf::Color rim = config::WallRimLight;
    const sf::Color rimClear(rim.r, rim.g, rim.b, 0);

    // There is only a wall here if there is rock just beyond it. A rim point
    // with more water on the other side is a doorway, and shading one would
    // paint a wall across the middle of open water - which is what a corridor
    // mouth used to get.
    const auto isEdge = [this](sf::Vector2f at, sf::Vector2f out) {
        return depthAt(at + out * 16.0f) > 0.0f;
    };

    // Sand in the shadow of the rock, fading back to open floor, and then a
    // bright line right on the waterline. Together they are what says this is
    // as far as you go, without anything having to look like a fence.
    const auto band = [&](sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f in0, sf::Vector2f in1) {
        appendRingQuad(m_shade,
                       p0 - in0 * config::WallShadeWidth, p1 - in1 * config::WallShadeWidth,
                       p0, p1, shadeClear, shade);

        appendRingQuad(m_shade,
                       p0 - in0 * config::WallRimWidth, p1 - in1 * config::WallRimWidth,
                       p0, p1, rimClear, rim);
    };

    for (const Chamber& chamber : m_chambers)
    {
        for (std::size_t i = 0; i < Segments; ++i)
        {
            const sf::Vector2f d0 = sf::Vector2f(1.0f, segmentAngle(i));
            const sf::Vector2f d1 = sf::Vector2f(1.0f, segmentAngle(i + 1));

            const sf::Vector2f p0 = chamber.centre + d0 * edgeRadius(chamber, d0);
            const sf::Vector2f p1 = chamber.centre + d1 * edgeRadius(chamber, d1);

            if (!isEdge((p0 + p1) * 0.5f, (d0 + d1) * 0.5f))
                continue;

            band(p0, p1, d0, d1);
        }
    }

    for (const Corridor& corridor : m_corridors)
    {
        const sf::Vector2f span = corridor.to - corridor.from;
        const float length = span.length();
        if (length <= 1.0f)
            continue;

        const sf::Vector2f along = span / length;
        const sf::Vector2f across{ -along.y, along.x };
        const auto steps = static_cast<std::size_t>(std::max(4.0f, length / 24.0f));

        for (std::size_t side = 0; side < 2; ++side)
        {
            const sf::Vector2f out = side == 0 ? across : -across;

            for (std::size_t i = 0; i < steps; ++i)
            {
                const float t0 = static_cast<float>(i) / static_cast<float>(steps);
                const float t1 = static_cast<float>(i + 1) / static_cast<float>(steps);

                const sf::Vector2f p0 = corridor.from + span * t0 + out * corridor.radius;
                const sf::Vector2f p1 = corridor.from + span * t1 + out * corridor.radius;

                if (!isEdge((p0 + p1) * 0.5f, out))
                    continue;

                band(p0, p1, out, out);
            }
        }
    }

    // Columns last, so they sit on the floor they are standing on: a contact
    // shadow, the shaft, then a smaller face offset up and left, lit from the
    // same corner every sprite in the game is lit from.
    for (const Pillar& pillar : m_pillars)
    {
        for (std::size_t i = 0; i < Segments; ++i)
        {
            const sf::Vector2f d0 = sf::Vector2f(1.0f, segmentAngle(i));
            const sf::Vector2f d1 = sf::Vector2f(1.0f, segmentAngle(i + 1));

            appendRingQuad(m_shade,
                           pillar.centre + d0 * pillar.radius,
                           pillar.centre + d1 * pillar.radius,
                           pillar.centre + d0 * (pillar.radius + 30.0f),
                           pillar.centre + d1 * (pillar.radius + 30.0f),
                           shade, shadeClear);
        }

        for (std::size_t i = 0; i < Segments; ++i)
        {
            const sf::Angle a0 = segmentAngle(i);
            const sf::Angle a1 = segmentAngle(i + 1);

            m_shade.append(sf::Vertex{ pillar.centre, config::RockDeepColor });
            m_shade.append(sf::Vertex{ pillar.centre + sf::Vector2f(pillar.radius, a0),
                                       config::RockDeepColor });
            m_shade.append(sf::Vertex{ pillar.centre + sf::Vector2f(pillar.radius, a1),
                                       config::RockDeepColor });
        }

        const sf::Vector2f lit = pillar.centre - sf::Vector2f{ 0.10f, 0.13f } * pillar.radius;

        for (std::size_t i = 0; i < Segments; ++i)
        {
            const sf::Angle a0 = segmentAngle(i);
            const sf::Angle a1 = segmentAngle(i + 1);

            m_shade.append(sf::Vertex{ lit, config::WallLipColor });
            m_shade.append(sf::Vertex{ lit + sf::Vector2f(pillar.radius * 0.84f, a0),
                                       config::WallLipColor });
            m_shade.append(sf::Vertex{ lit + sf::Vector2f(pillar.radius * 0.84f, a1),
                                       config::WallLipColor });
        }
    }
}

void Arena::buildSwirl()
{
    m_swirl.clear();

    // A pit drawn as water rather than as a marking on the floor: broken rings
    // turning at the speed the current is actually dragging at, so what is
    // about to happen to the sub is visible before it happens.
    for (const Current& current : m_currents)
    {
        const float spin = current.strength / 150.0f;

        for (int ring = 0; ring < 3; ++ring)
        {
            const float radius = current.radius * (0.34f + 0.24f * static_cast<float>(ring));
            const float turn = m_time * spin * (1.0f - 0.18f * static_cast<float>(ring));
            const float width = 5.0f + 2.0f * static_cast<float>(ring);

            for (int dash = 0; dash < 9; ++dash)
            {
                const float base = turn + static_cast<float>(dash) * (6.2832f / 9.0f);

                for (int step = 0; step < 5; ++step)
                {
                    const float a0 = base + static_cast<float>(step) * 0.038f;
                    const float a1 = base + static_cast<float>(step + 1) * 0.038f;

                    const sf::Vector2f d0{ std::cos(a0), std::sin(a0) };
                    const sf::Vector2f d1{ std::cos(a1), std::sin(a1) };

                    // Faded in at the head of a dash and out at its tail, so
                    // each one reads as a streak of moving water.
                    const float fade = std::sin(static_cast<float>(step + 1) / 6.0f * 3.1416f);
                    const sf::Color tint(config::PitMarkColor.r, config::PitMarkColor.g,
                                         config::PitMarkColor.b,
                                         static_cast<std::uint8_t>(
                                             static_cast<float>(config::PitMarkColor.a) * fade));

                    appendRingQuad(m_swirl,
                                   current.centre + d0 * (radius - width),
                                   current.centre + d1 * (radius - width),
                                   current.centre + d0 * (radius + width),
                                   current.centre + d1 * (radius + width),
                                   tint, tint);
                }
            }
        }
    }
}

void Arena::drawGround(sf::RenderTarget& target) const
{
    target.draw(m_rock);
    target.draw(m_floor);
    target.draw(m_shade);
    target.draw(m_swirl);
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
    buildSwirl();
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

    // Tried a few times, because a room can have columns standing in it and a
    // rim that is not a circle. Anything still in rock after that is handed to
    // clampInside by the caller, which is what it is for.
    sf::Vector2f best = pick->centre;

    for (int attempt = 0; attempt < 8; ++attempt)
    {
        const float distance = pick->radius * std::sqrt(rng::range(0.0f, 1.0f));
        best = pick->centre + sf::Vector2f(distance, sf::degrees(rng::range(0.0f, 360.0f)));

        if (depthAt(best) <= -40.0f)
            return best;
    }

    return best;
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

    // Against the rim as it actually is rather than as the radius says it is,
    // or half of what is put out at the wall ends up buried in it.
    const sf::Vector2f out = sf::Vector2f(1.0f, sf::degrees(rng::range(0.0f, 360.0f)));
    const float reach = std::max(10.0f, edgeRadius(chamber, out) - inset);

    return chamber.centre + out * reach;
}
