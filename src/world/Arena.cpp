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

    // A passage is now cut from hundreds of small mouthfuls, and a mouthful
    // seventy pixels across does not need ninety-six sides to look round. The
    // count follows the size, which is the difference between the basin
    // costing a hundred thousand vertices and costing half a million.
    std::size_t segmentsFor(float radius)
    {
        return static_cast<std::size_t>(std::clamp(radius / 3.0f, 24.0f,
                                                   static_cast<float>(Segments)));
    }

    sf::Angle segmentAngle(std::size_t index, std::size_t count)
    {
        return sf::degrees(360.0f * static_cast<float>(index) / static_cast<float>(count));
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
    m_rooms.clear();
    m_links.clear();
    m_nextHop.clear();

    // A room is not a circle. It is two to four lobes of different sizes
    // pushed off a common centre, unioned into one cavern - which is what
    // stops a basin reading as a diagram of overlapping discs and starts it
    // reading as something water carved.
    const auto addRoom = [&](sf::Vector2f centre, float radius, ChamberKind kind) {
        const int index = static_cast<int>(m_rooms.size());
        m_rooms.push_back({ centre, radius, kind });
        m_links.emplace_back();

        // Every lobe gets its own outline, not the same one turned round.
        const auto warped = [&](sf::Vector2f at, float size) {
            Chamber lobe{ at, size, kind, random.range(0.0f, 6.28f), {}, 0.0f };

            lobe.warp[0] = random.range(0.34f, 0.62f);
            lobe.warp[1] = random.range(0.16f, 0.38f);
            lobe.warp[2] = random.range(0.06f, 0.20f);
            lobe.warp[3] = random.range(0.03f, 0.12f);

            // Every term pulling the same way at once, which is the worst case
            // the field has to allow for.
            const float swing = lobe.warp[0] + lobe.warp[1] + lobe.warp[2] + lobe.warp[3];
            lobe.reach = size * (1.0f + config::ChamberLean * lobe.warp[0])
                         * (1.0f + config::ChamberRipple * swing);

            m_chambers.push_back(lobe);
        };

        warped(centre, radius);

        const int lobes = static_cast<int>(random.range(
            static_cast<float>(config::LobesMin), static_cast<float>(config::LobesMax) + 0.999f));

        for (int i = 0; i < lobes; ++i)
        {
            const float angle = random.range(0.0f, 360.0f);
            const float out = radius * random.range(config::LobeOffsetMin, config::LobeOffsetMax);
            const float size = radius * random.range(config::LobeScaleMin, config::LobeScaleMax);

            warped(centre + sf::Vector2f(out, sf::degrees(angle)), size);
        }

        return index;
    };

    // A passage bends. Cut as a chain of segments with the middle ones pushed
    // sideways, so getting from one room to the next is a route rather than a
    // sight line down which everything can shoot you.
    const auto addCorridor = [&](int from, int to, float radius) {
        if (from < 0 || to < 0 || from == to)
            return;

        m_links[static_cast<std::size_t>(from)].push_back(to);
        m_links[static_cast<std::size_t>(to)].push_back(from);

        const sf::Vector2f a = m_rooms[static_cast<std::size_t>(from)].centre;
        const sf::Vector2f b = m_rooms[static_cast<std::size_t>(to)].centre;
        const sf::Vector2f span = b - a;
        const float length = span.length();
        if (length <= 1.0f)
            return;

        const sf::Vector2f across = sf::Vector2f{ -span.y, span.x } / length;

        const int pieces = static_cast<int>(random.range(
            static_cast<float>(config::CorridorPiecesMin),
            static_cast<float>(config::CorridorPiecesMax) + 0.999f));

        sf::Vector2f previous = a;
        for (int i = 1; i <= pieces; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(pieces);

            // The kink is strongest in the middle and nothing at the ends, so a
            // passage always meets its rooms square on.
            const float bow = std::sin(t * 3.14159265f) * config::CorridorBend
                              * random.range(-1.0f, 1.0f);

            const sf::Vector2f point = i == pieces ? b : a + span * t + across * bow;
            const float width = radius * random.range(config::CorridorWidthMin,
                                                      config::CorridorWidthMax);

            m_corridors.push_back({ previous, point, width });

            // The passage itself is cut as a run of overlapping wobbled
            // mouthfuls rather than as a capsule. A capsule has two perfectly
            // straight walls, and a cave has none: this is what stops every
            // corridor in the basin reading as a drilled tube.
            const sf::Vector2f leg = point - previous;
            const float legLength = leg.length();
            const int bites = std::max(2, static_cast<int>(legLength
                                                           / (width * config::BiteSpacing)));

            for (int step = 0; step <= bites; ++step)
            {
                const float along = static_cast<float>(step) / static_cast<float>(bites);
                const float size = width * random.range(0.82f, 1.22f);

                Chamber bite{ previous + leg * along, size, ChamberKind::Plain,
                              random.range(0.0f, 6.28f), {}, 0.0f };

                // Gentler than a room: a passage that wobbles as hard as a
                // cavern stops being watertight between one mouthful and the
                // next.
                bite.warp[0] = random.range(0.16f, 0.30f);
                bite.warp[1] = random.range(0.08f, 0.18f);
                bite.warp[2] = random.range(0.04f, 0.10f);
                bite.warp[3] = random.range(0.02f, 0.06f);

                const float swing = bite.warp[0] + bite.warp[1] + bite.warp[2] + bite.warp[3];
                bite.reach = size * (1.0f + config::ChamberLean * bite.warp[0])
                             * (1.0f + config::ChamberRipple * swing);

                m_chambers.push_back(bite);
            }

            previous = point;
        }
    };

    const int start = addRoom(m_center, config::ChamberStartRadius, ChamberKind::Plain);
    m_start = m_center;

    if (bossRoom)
    {
        // A boss needs one room it can actually turn in. It gets a hall off to
        // one side, so arriving at it is still an approach rather than a cut.
        const float angle = random.range(0.0f, 360.0f);
        const sf::Vector2f hall = m_center
                                  + sf::Vector2f(config::HallDistance, sf::degrees(angle));

        const int room = addRoom(hall, config::HallRadius, ChamberKind::Pillars);
        addCorridor(start, room, config::CorridorRadius * 1.5f);

        // And two alcoves off the hall, so there is somewhere to break line of
        // fire in a room that is otherwise one big opening.
        for (int i = 0; i < 2; ++i)
        {
            const float side = angle + (i == 0 ? 130.0f : -130.0f) + random.range(-20.0f, 20.0f);
            const int pocket = addRoom(hall + sf::Vector2f(config::HallRadius * 0.95f,
                                                           sf::degrees(side)),
                                       random.range(130.0f, 190.0f), ChamberKind::Plain);
            addCorridor(room, pocket, config::CorridorRadius);
        }
    }
    else
    {
        // A walk of rooms joined nose to tail, with branches hanging off it and
        // loops tied back across it. The walk gives the map a length, the
        // branches give it dead ends, and the loops are what turn it from a
        // corridor into something you can get lost in.
        float heading = random.range(0.0f, 360.0f);
        int previous = start;

        for (int i = 1; i < config::RoomCount; ++i)
        {
            heading += random.range(-config::ChamberTurn, config::ChamberTurn);

            const float step = random.range(config::ChamberStepMin, config::ChamberStepMax);
            sf::Vector2f next = m_rooms[static_cast<std::size_t>(previous)].centre
                                + sf::Vector2f(step, sf::degrees(heading));

            // Folded back if the walk would leave the world the seabed covers.
            if ((next - m_center).length() > config::MapExtent)
            {
                heading += 180.0f + random.range(-60.0f, 60.0f);
                next = m_rooms[static_cast<std::size_t>(previous)].centre
                       + sf::Vector2f(step, sf::degrees(heading));
            }

            float radius = random.range(config::ChamberRadiusMin, config::ChamberRadiusMax);

            // Every so often a room much larger than the walk has been cutting.
            // A basin of rooms all one size has no rhythm: you never arrive
            // anywhere, you only continue.
            if (random.next() < config::GreatHallChance)
                radius *= config::GreatHallScale;

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
                radius = std::min(radius, config::ChamberRadiusMin * 0.85f);
                corridor *= config::ThroatCorridorScale;
            }

            const int room = addRoom(next, radius, kind);
            addCorridor(previous, room, corridor);

            // A dead end worth swimming down: where a nest ends up, and where
            // a hunt gets its length from.
            if (random.next() < config::BranchChance)
            {
                const float side = heading + random.range(70.0f, 150.0f)
                                   * (random.next() < 0.5f ? -1.0f : 1.0f);

                const int stub = addRoom(next + sf::Vector2f(random.range(340.0f, 620.0f),
                                                             sf::degrees(side)),
                                         random.range(140.0f, 230.0f), ChamberKind::Plain);
                addCorridor(room, stub, config::CorridorRadius * 0.8f);
            }

            previous = room;
        }

        // And then tie it back across itself. One loop makes a ring; several
        // make a map you have to remember rather than retrace.
        const auto alreadyLinked = [&](int a, int b) {
            for (const int other : m_links[static_cast<std::size_t>(a)])
            {
                if (other == b)
                    return true;
            }
            return false;
        };

        const int rooms = static_cast<int>(m_rooms.size());

        for (int attempt = 0; attempt < config::LoopAttempts && rooms > 4; ++attempt)
        {
            const int a = static_cast<int>(random.range(0.0f, static_cast<float>(rooms)))
                          % rooms;
            const int b = static_cast<int>(random.range(0.0f, static_cast<float>(rooms)))
                          % rooms;

            if (a == b || alreadyLinked(a, b))
                continue;

            const float gap = (m_rooms[static_cast<std::size_t>(a)].centre
                               - m_rooms[static_cast<std::size_t>(b)].centre).length();

            // Near enough to be a shortcut, far enough that it is not just a
            // second doorway into the room next door.
            if (gap < config::LoopGapMin || gap > config::LoopGapMax)
                continue;

            addCorridor(a, b, config::CorridorRadius * random.range(0.6f, 0.95f));
        }
    }

    // The hall is simply the biggest room, however the map came out.
    m_hallCentre = m_rooms.front().centre;
    m_hallRadius = m_rooms.front().radius;

    for (const Room& room : m_rooms)
    {
        if (room.radius > m_hallRadius)
        {
            m_hallRadius = room.radius;
            m_hallCentre = room.centre;
        }
    }

    m_boundsMin = m_chambers.front().centre;
    m_boundsMax = m_chambers.front().centre;

    for (const Chamber& chamber : m_chambers)
    {
        m_boundsMin.x = std::min(m_boundsMin.x, chamber.centre.x - chamber.radius);
        m_boundsMin.y = std::min(m_boundsMin.y, chamber.centre.y - chamber.radius);
        m_boundsMax.x = std::max(m_boundsMax.x, chamber.centre.x + chamber.radius);
        m_boundsMax.y = std::max(m_boundsMax.y, chamber.centre.y + chamber.radius);
    }

    // Furnishing, once the rooms are final. Pillars and pits both need to know
    // where the walls ended up, so neither can be decided during the walk.
    for (const Room& room : m_rooms)
    {
        if (room.kind == ChamberKind::Pillars)
        {
            const int count = static_cast<int>(random.range(
                static_cast<float>(config::PillarCountMin),
                static_cast<float>(config::PillarCountMax) + 0.999f));

            for (int i = 0; i < count; ++i)
            {
                const float angle = random.range(0.0f, 360.0f);
                const float reach = room.radius * random.range(0.34f, 0.82f);
                const float size = random.range(config::PillarRadiusMin,
                                                config::PillarRadiusMax);

                m_pillars.push_back({ room.centre + sf::Vector2f(reach, sf::degrees(angle)),
                                      size });
            }
        }
        else if (room.kind == ChamberKind::Pit)
        {
            m_currents.push_back({ room.centre, room.radius * 1.15f,
                                   config::PitStrength * random.range(0.8f, 1.25f)
                                       * (random.next() < 0.5f ? -1.0f : 1.0f) });
        }
    }

    // Rubble: lumps of rock bitten out of the wall itself, at a scale between
    // the ripple on the rim and a column standing in the room. Without them
    // every wall is a smooth curve however much it wanders, which is the one
    // thing a cave wall never is.
    for (const Chamber& chamber : m_chambers)
    {
        // Rooms only, and only against their walls. Scattered through a room
        // they stop being rock and become an obstacle course; scattered down a
        // passage there are hundreds of them.
        if (chamber.radius < config::BoulderMinChamber)
            continue;

        const int lumps = static_cast<int>(random.range(0.0f, config::RubblePerChamber + 0.999f));

        for (int i = 0; i < lumps; ++i)
        {
            const sf::Vector2f out = sf::Vector2f(1.0f, sf::degrees(random.range(0.0f, 360.0f)));
            const float size = random.range(config::RubbleRadiusMin, config::RubbleRadiusMax);
            const float reach = edgeRadius(chamber, out) * random.range(0.88f, 1.06f);
            const sf::Vector2f at = chamber.centre + out * reach;

            // A lump is allowed to bite into a passage - that is most of what
            // makes a passage look cut rather than drilled - but never far
            // enough to seal it. What has to survive is a gap the sub fits
            // through, and that is checked rather than assumed.
            bool blocks = false;
            for (const Corridor& corridor : m_corridors)
            {
                const sf::Vector2f on = closestOnSegment(corridor.from, corridor.to, at);
                const float axis = (at - on).length();

                if (axis - size < -corridor.radius + config::RubbleClearance)
                {
                    blocks = true;
                    break;
                }
            }

            if (!blocks)
                m_pillars.push_back({ at, size });
        }
    }

    buildRoutes();
    buildNavGrid();
    buildGround();
}

void Arena::buildRoutes()
{
    // The map is generated as a graph, so anything that has to cross it should
    // walk the graph rather than push at a wall and hope. One breadth-first
    // search per room, once per level, gives every pair of rooms its next hop:
    // after that, asking which way to go is a table lookup.
    const int rooms = static_cast<int>(m_rooms.size());
    m_nextHop.assign(static_cast<std::size_t>(rooms * rooms), -1);

    if (rooms <= 0)
        return;

    std::vector<int> came(static_cast<std::size_t>(rooms));
    std::vector<int> queue(static_cast<std::size_t>(rooms));

    for (int source = 0; source < rooms; ++source)
    {
        std::fill(came.begin(), came.end(), -1);
        came[static_cast<std::size_t>(source)] = source;

        int head = 0;
        int tail = 0;
        queue[static_cast<std::size_t>(tail++)] = source;

        while (head < tail)
        {
            const int at = queue[static_cast<std::size_t>(head++)];

            for (const int next : m_links[static_cast<std::size_t>(at)])
            {
                if (came[static_cast<std::size_t>(next)] != -1)
                    continue;

                came[static_cast<std::size_t>(next)] = at;
                queue[static_cast<std::size_t>(tail++)] = next;
            }
        }

        // Walk each destination back to the room next to the source: that is
        // the hop anything standing in source should take to reach it.
        for (int target = 0; target < rooms; ++target)
        {
            if (came[static_cast<std::size_t>(target)] == -1)
                continue;

            int step = target;
            while (came[static_cast<std::size_t>(step)] != source && step != source)
                step = came[static_cast<std::size_t>(step)];

            m_nextHop[static_cast<std::size_t>(source * rooms + target)] = step;
        }
    }
}

// ---------------------------------------------------------------------------
//  Getting across the basin
//
//  The room graph answers "which room next", which is right until both ends
//  are in the same room with a pillar or a lobe wall between them - and that
//  is exactly the case that used to leave one creature alive and unreachable
//  for the rest of a level. So underneath the graph there is a grid: every
//  cell marked open or not once when the map is cut, and a breadth-first sweep
//  out from the sub a few times a second that leaves every open cell pointing
//  at its neighbour one step nearer. Anything that has lost its line reads its
//  own cell and swims that way.
// ---------------------------------------------------------------------------

void Arena::buildNavGrid()
{
    const sf::Vector2f span = m_boundsMax - m_boundsMin;

    m_navOrigin = m_boundsMin - sf::Vector2f{ config::NavCell, config::NavCell };
    m_navWidth = static_cast<int>((span.x + config::NavCell * 4.0f) / config::NavCell) + 1;
    m_navHeight = static_cast<int>((span.y + config::NavCell * 4.0f) / config::NavCell) + 1;

    m_navWidth = std::max(1, m_navWidth);
    m_navHeight = std::max(1, m_navHeight);

    const std::size_t cells = static_cast<std::size_t>(m_navWidth)
                              * static_cast<std::size_t>(m_navHeight);

    m_navOpen.assign(cells, 0);
    m_flowFrom.assign(cells, -1);

    for (int y = 0; y < m_navHeight; ++y)
    {
        for (int x = 0; x < m_navWidth; ++x)
        {
            const sf::Vector2f at = cellCentre(y * m_navWidth + x);
            m_navOpen[static_cast<std::size_t>(y * m_navWidth + x)] =
                isOpen(at, config::NavClearance) ? 1u : 0u;
        }
    }
}

sf::Vector2f Arena::cellCentre(int index) const
{
    if (m_navWidth <= 0)
        return m_center;

    const int x = index % m_navWidth;
    const int y = index / m_navWidth;

    return m_navOrigin + sf::Vector2f{ (static_cast<float>(x) + 0.5f) * config::NavCell,
                                       (static_cast<float>(y) + 0.5f) * config::NavCell };
}

int Arena::cellIndex(sf::Vector2f position) const
{
    if (m_navWidth <= 0 || m_navHeight <= 0)
        return -1;

    const sf::Vector2f local = (position - m_navOrigin) / config::NavCell;
    const int x = static_cast<int>(std::floor(local.x));
    const int y = static_cast<int>(std::floor(local.y));

    if (x < 0 || y < 0 || x >= m_navWidth || y >= m_navHeight)
        return -1;

    return y * m_navWidth + x;
}

int Arena::nearestOpenCell(sf::Vector2f position) const
{
    const int direct = cellIndex(position);
    if (direct >= 0 && m_navOpen[static_cast<std::size_t>(direct)] != 0u)
        return direct;

    // Rings outward from where it asked. Anything standing in rock - which
    // happens to a creature still swimming in - gets the nearest water.
    const int cx = direct >= 0 ? direct % m_navWidth
                               : std::clamp(static_cast<int>((position.x - m_navOrigin.x)
                                                             / config::NavCell),
                                            0, m_navWidth - 1);
    const int cy = direct >= 0 ? direct / m_navWidth
                               : std::clamp(static_cast<int>((position.y - m_navOrigin.y)
                                                             / config::NavCell),
                                            0, m_navHeight - 1);

    for (int ring = 1; ring < 12; ++ring)
    {
        for (int dy = -ring; dy <= ring; ++dy)
        {
            for (int dx = -ring; dx <= ring; ++dx)
            {
                if (std::abs(dx) != ring && std::abs(dy) != ring)
                    continue;

                const int x = cx + dx;
                const int y = cy + dy;
                if (x < 0 || y < 0 || x >= m_navWidth || y >= m_navHeight)
                    continue;

                const int index = y * m_navWidth + x;
                if (m_navOpen[static_cast<std::size_t>(index)] != 0u)
                    return index;
            }
        }
    }

    return -1;
}

void Arena::updateFlow(sf::Vector2f target)
{
    if (m_navOpen.empty())
        return;

    const int source = nearestOpenCell(target);
    if (source < 0)
        return;

    // Nothing to do if the sweep already starts where it is being asked to.
    if (source == m_flowSource)
        return;

    m_flowSource = source;
    std::fill(m_flowFrom.begin(), m_flowFrom.end(), -1);

    m_flowQueue.clear();
    m_flowQueue.push_back(source);
    m_flowFrom[static_cast<std::size_t>(source)] = source;

    // Eight ways, so a diagonal is a step rather than a staircase.
    static const int OffsetX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    static const int OffsetY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

    for (std::size_t head = 0; head < m_flowQueue.size(); ++head)
    {
        const int at = m_flowQueue[head];
        const int x = at % m_navWidth;
        const int y = at / m_navWidth;

        for (int i = 0; i < 8; ++i)
        {
            const int nx = x + OffsetX[i];
            const int ny = y + OffsetY[i];

            if (nx < 0 || ny < 0 || nx >= m_navWidth || ny >= m_navHeight)
                continue;

            const int next = ny * m_navWidth + nx;
            if (m_navOpen[static_cast<std::size_t>(next)] == 0u
                || m_flowFrom[static_cast<std::size_t>(next)] != -1)
            {
                continue;
            }

            // A diagonal is only a step if both of the squares it cuts across
            // are open, or things clip the corners of pillars.
            if (i >= 4)
            {
                const int sideA = y * m_navWidth + nx;
                const int sideB = ny * m_navWidth + x;
                if (m_navOpen[static_cast<std::size_t>(sideA)] == 0u
                    || m_navOpen[static_cast<std::size_t>(sideB)] == 0u)
                {
                    continue;
                }
            }

            m_flowFrom[static_cast<std::size_t>(next)] = at;
            m_flowQueue.push_back(next);
        }
    }
}

sf::Vector2f Arena::flowAt(sf::Vector2f position) const
{
    if (m_flowFrom.empty())
        return { 0.0f, 0.0f };

    const int here = nearestOpenCell(position);
    if (here < 0)
        return { 0.0f, 0.0f };

    const int step = m_flowFrom[static_cast<std::size_t>(here)];
    if (step < 0)
        return { 0.0f, 0.0f }; // walled off from the sub entirely

    const sf::Vector2f to = cellCentre(step);
    const sf::Vector2f offset = to - position;

    // Standing on the cell it was told to move to: aim at the one after, so it
    // does not sit on a waypoint jittering.
    if (offset.lengthSquared() < config::NavCell * config::NavCell * 0.25f
        && m_flowFrom[static_cast<std::size_t>(step)] >= 0
        && m_flowFrom[static_cast<std::size_t>(step)] != step)
    {
        const sf::Vector2f onward =
            cellCentre(m_flowFrom[static_cast<std::size_t>(step)]) - position;

        if (onward.lengthSquared() > 1.0f)
            return onward.normalized();
    }

    return offset.lengthSquared() > 1.0f ? offset.normalized() : sf::Vector2f{ 0.0f, 0.0f };
}

int Arena::roomAt(sf::Vector2f position) const
{
    // Whichever room's own field this point is deepest inside, and failing
    // that whichever one is nearest: a point in a corridor belongs to the room
    // it is closest to, which is the right answer for routing.
    int best = -1;
    float bestDepth = 1.0e9f;

    for (std::size_t i = 0; i < m_rooms.size(); ++i)
    {
        const float distance = (position - m_rooms[i].centre).length() - m_rooms[i].radius;
        if (distance < bestDepth)
        {
            bestDepth = distance;
            best = static_cast<int>(i);
        }
    }

    return best;
}

bool Arena::clearLine(sf::Vector2f from, sf::Vector2f to, float radius) const
{
    const sf::Vector2f span = to - from;
    const float length = span.length();
    if (length <= 1.0f)
        return true;

    // Stepped at half the radius: fine enough that nothing slips through a
    // wall between two samples, coarse enough to be worth calling every frame.
    const float step = std::max(12.0f, radius * 0.5f);
    const auto steps = static_cast<int>(length / step) + 1;

    for (int i = 1; i <= steps; ++i)
    {
        const sf::Vector2f at = from + span * (static_cast<float>(i) / static_cast<float>(steps));
        if (!isOpen(at, radius))
            return false;
    }

    return true;
}

sf::Vector2f Arena::waypointToward(sf::Vector2f from, sf::Vector2f to, float radius) const
{
    // If it can simply swim there, it should: routing round a room you are
    // already standing in looks like indecision.
    if (clearLine(from, to, radius))
        return to;

    const int here = roomAt(from);
    const int there = roomAt(to);
    const int rooms = static_cast<int>(m_rooms.size());

    if (here < 0 || there < 0 || rooms <= 0)
        return to;

    // Same room and no line means something inside it is in the way - a
    // pillar, or the neck between two lobes. The graph has nothing to say
    // about that, so fall through to the grid, which does.
    const int hop = here == there
                        ? -1
                        : m_nextHop[static_cast<std::size_t>(here * rooms + there)];

    if (hop >= 0 && hop != here)
    {
        const sf::Vector2f next = m_rooms[static_cast<std::size_t>(hop)].centre;
        if (clearLine(from, next, radius))
            return next;
    }

    const sf::Vector2f flow = flowAt(from);
    if (flow.lengthSquared() > 0.0f)
        return from + flow * (config::NavCell * 2.0f);

    return hop >= 0 ? m_rooms[static_cast<std::size_t>(hop)].centre : to;
}

float Arena::edgeRadius(const Chamber& chamber, sf::Vector2f offset) const
{
    // Four harmonics rather than two, with weights rolled per chamber and
    // frequencies that do not divide into each other, so the rim never comes
    // back round to where it started. Two waves gave a wobbled circle; this
    // gives an outline with bays, noses and a grain of roughness on top - the
    // difference between a shape and a wall.
    const float theta = std::atan2(offset.y, offset.x);

    const float wave = std::sin(theta * 3.0f + chamber.phase) * chamber.warp[0]
                       + std::sin(theta * 5.0f - chamber.phase * 1.7f) * chamber.warp[1]
                       + std::sin(theta * 11.0f + chamber.phase * 2.3f) * chamber.warp[2]
                       + std::sin(theta * 19.0f - chamber.phase * 3.1f) * chamber.warp[3];

    // Squashed towards one side as well as rippled: a cave mouth is rarely in
    // the middle of the room it opens into.
    const float lean = 1.0f + config::ChamberLean
                                 * std::cos(theta - chamber.phase * 0.7f) * chamber.warp[0];

    return chamber.radius * lean * (1.0f + config::ChamberRipple * wave);
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

    // A shape can only improve the answer if it is nearer than the best so far
    // plus its own reach, and that comparison can be made on squared distances
    // with no square root at all. With a room now being several lobes and a
    // passage several segments, this is the difference between the field being
    // cheap and the field being the frame.
    const auto tooFar = [&best](float distanceSquared, float reach) {
        const float slack = best + reach;
        return slack <= 0.0f || distanceSquared > slack * slack;
    };

    for (const Chamber& chamber : m_chambers)
    {
        const sf::Vector2f offset = position - chamber.centre;

        if (tooFar(offset.lengthSquared(), chamber.reach))
            continue;

        best = std::min(best, offset.length() - edgeRadius(chamber, offset));
    }

    // Passages are cut as chambers too, so there is nothing else in the union:
    // one shape everywhere, and no straight walls anywhere in the basin.

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

        const std::size_t steps = segmentsFor(chamber.radius + grow);

        for (std::size_t i = 0; i < steps; ++i)
        {
            const sf::Vector2f d0 = sf::Vector2f(1.0f, segmentAngle(i, steps));
            const sf::Vector2f d1 = sf::Vector2f(1.0f, segmentAngle(i + 1, steps));

            into.append(sf::Vertex{ chamber.centre, colour });
            into.append(sf::Vertex{ chamber.centre + d0 * reach(d0), colour });
            into.append(sf::Vertex{ chamber.centre + d1 * reach(d1), colour });
        }
    };

    const auto disc = [](sf::VertexArray& into, sf::Vector2f centre, float radius,
                         sf::Color colour) {
        const std::size_t steps = segmentsFor(radius);

        for (std::size_t i = 0; i < steps; ++i)
        {
            const sf::Angle a0 = segmentAngle(i, steps);
            const sf::Angle a1 = segmentAngle(i + 1, steps);

            into.append(sf::Vertex{ centre, colour });
            into.append(sf::Vertex{ centre + sf::Vector2f(radius, a0), colour });
            into.append(sf::Vertex{ centre + sf::Vector2f(radius, a1), colour });
        }
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



    for (const Chamber& chamber : m_chambers)
        chamberFan(m_rock, chamber, config::WallLip, 0.70f, config::WallLipColor);



    // Boulders slumped along the foot of the wall, each about to be half buried
    // by the floor. They exist so that the rim is never a curve; nothing can
    // ever touch one, because they sit in rock.
    {
        Seeded rubble{ 0x9E3779B9u };

        for (const Chamber& chamber : m_chambers)
        {
            // Rooms only. A passage is cut from hundreds of small mouthfuls
            // now, and fifteen boulders on each of those is a million
            // triangles and a basin that takes a minute to build.
            if (chamber.radius < config::BoulderMinChamber)
                continue;

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
    // Far enough out that the sliver where two overlapping mouthfuls meet is
    // not mistaken for a wall. A passage cut from hundreds of them has a great
    // many such slivers, and each one drawn would be a white line across open
    // water.
    const auto isEdge = [this](sf::Vector2f at, sf::Vector2f out) {
        return depthAt(at + out * 26.0f) > 0.0f;
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
        const std::size_t steps = segmentsFor(chamber.radius);

        for (std::size_t i = 0; i < steps; ++i)
        {
            const sf::Vector2f d0 = sf::Vector2f(1.0f, segmentAngle(i, steps));
            const sf::Vector2f d1 = sf::Vector2f(1.0f, segmentAngle(i + 1, steps));

            const sf::Vector2f p0 = chamber.centre + d0 * edgeRadius(chamber, d0);
            const sf::Vector2f p1 = chamber.centre + d1 * edgeRadius(chamber, d1);

            if (!isEdge((p0 + p1) * 0.5f, (d0 + d1) * 0.5f))
                continue;

            band(p0, p1, d0, d1);
        }
    }

    // Columns last, so they sit on the floor they are standing on: a contact
    // shadow, the shaft, then a smaller face offset up and left, lit from the
    // same corner every sprite in the game is lit from.
    for (const Pillar& pillar : m_pillars)
    {
        // A lump embedded in the wall is already drawn - it is the wall. Only
        // something standing clear of the rock gets a shaft and a lit face,
        // or every bite out of a corridor wall reads as a column floating on
        // top of the rock rather than as part of it.
        float water = 1.0e9f;
        for (const Chamber& chamber : m_chambers)
        {
            const sf::Vector2f offset = pillar.centre - chamber.centre;
            water = std::min(water, offset.length() - edgeRadius(chamber, offset));
        }

        if (water > -pillar.radius * 1.15f)
            continue;

        const std::size_t steps = segmentsFor(pillar.radius);

        for (std::size_t i = 0; i < steps; ++i)
        {
            const sf::Vector2f d0 = sf::Vector2f(1.0f, segmentAngle(i, steps));
            const sf::Vector2f d1 = sf::Vector2f(1.0f, segmentAngle(i + 1, steps));

            appendRingQuad(m_shade,
                           pillar.centre + d0 * pillar.radius,
                           pillar.centre + d1 * pillar.radius,
                           pillar.centre + d0 * (pillar.radius + 30.0f),
                           pillar.centre + d1 * (pillar.radius + 30.0f),
                           shade, shadeClear);
        }

        for (std::size_t i = 0; i < steps; ++i)
        {
            const sf::Angle a0 = segmentAngle(i, steps);
            const sf::Angle a1 = segmentAngle(i + 1, steps);

            m_shade.append(sf::Vertex{ pillar.centre, config::RockDeepColor });
            m_shade.append(sf::Vertex{ pillar.centre + sf::Vector2f(pillar.radius, a0),
                                       config::RockDeepColor });
            m_shade.append(sf::Vertex{ pillar.centre + sf::Vector2f(pillar.radius, a1),
                                       config::RockDeepColor });
        }

        const sf::Vector2f lit = pillar.centre - sf::Vector2f{ 0.10f, 0.13f } * pillar.radius;

        for (std::size_t i = 0; i < steps; ++i)
        {
            const sf::Angle a0 = segmentAngle(i, steps);
            const sf::Angle a1 = segmentAngle(i + 1, steps);

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
