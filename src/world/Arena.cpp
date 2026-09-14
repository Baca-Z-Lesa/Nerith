#include "world/Arena.h"

#include "core/Config.h"
#include "core/Mesh.h"
#include "core/Random.h"

#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/RenderStates.hpp>
#include <SFML/System/Angle.hpp>

#include <algorithm>
#include <array>
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
    , m_dark(sf::PrimitiveType::Triangles)
    , m_theme(&themeFor(1))
{
    generate(1, false);
    buildLightPool();
    buildDark();
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

            settleRim(lobe);
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

        // One roll for the whole passage, not one per piece: a corridor whose
        // pieces each flowed their own way would be a washing machine rather
        // than a route. Which way it runs is rolled too, so a shortcut can be
        // easy in the direction you happen to need or the other one.
        const bool flows = random.next() < config::CorridorCurrentChance;
        const bool reversed = random.next() < 0.5f;
        const float push = config::CurrentStrength * random.range(0.85f, 1.15f);

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

            if (flows)
            {
                m_currents.push_back(reversed
                                         ? Current{ point, previous, width, push }
                                         : Current{ previous, point, width, push });
            }

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

                settleRim(bite);
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

            // Sized against the room they stand in rather than against a flat
            // pair of numbers. Seventy six pixels of rock is cover in a great
            // hall and most of the floor in the smallest room the generator
            // makes, and it was allowed in both.
            const float widest = std::min(config::PillarRadiusMax,
                                          room.radius * config::PillarRoomShare);

            if (widest < config::PillarRadiusMin)
                continue;

            for (int i = 0; i < count; ++i)
            {
                const float angle = random.range(0.0f, 360.0f);
                const float size = random.range(config::PillarRadiusMin, widest);

                // In a band, not anywhere. Off the middle because the middle
                // of the first room is where the dive starts, and off the wall
                // because a column against one pinches the gap beside it.
                const float inner = size + config::PillarCentreGap;
                const float outer = room.radius - size - config::PillarWallGap;

                if (outer <= inner)
                    continue; // no room in this room for a column

                const float reach = random.range(inner, outer);

                m_pillars.push_back({ room.centre + sf::Vector2f(reach, sf::degrees(angle)),
                                      size });
            }
        }
        else if (room.kind == ChamberKind::Pit)
        {
            m_currents.push_back({ room.centre, room.centre, room.radius * 1.15f,
                                   config::PitStrength * random.range(0.8f, 1.25f)
                                       * (random.next() < 0.5f ? -1.0f : 1.0f) });
        }
    }

    // Rubble: lumps of rock bitten out of the wall itself, at a scale between
    // the ripple on the rim and a column standing in the room. Without them
    // every wall is a smooth curve however much it wanders, which is the one
    // thing a cave wall never is.
    //
    // Everything above this line is structure a room was designed around and
    // is never taken away; everything below it is scenery, and scenery is what
    // gets removed when the basin turns out not to join up.
    m_rubbleFrom = m_pillars.size();

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
            // enough to seal it, and never anywhere near a mouth. A rock in
            // the middle of a room is scenery; the same rock in a doorway is
            // a level that cannot be finished.
            bool blocks = false;
            for (const Corridor& corridor : m_corridors)
            {
                const sf::Vector2f on = closestOnSegment(corridor.from, corridor.to, at);
                const float axis = (at - on).length();

                if (axis - size < corridor.radius + config::RubbleClearance)
                {
                    blocks = true;
                    break;
                }
            }

            if (!blocks)
                m_pillars.push_back({ at, size });
        }
    }

    // The index first: everything below asks the field where the rock is,
    // and until the shapes are filed each of those questions is a walk over
    // the whole basin.
    buildShapeIndex();
    buildRoutes();
    buildNavGrid();

    // And then check, rather than hope. Every rule above is a rule about one
    // lump at a time, and a level is not made unfinishable by one lump - it is
    // made unfinishable by two of them either side of the same gap, which no
    // amount of care about each of them separately will ever catch.
    clearBlockedRoutes();

    // And if the walk itself left something stranded, cut the hole rather
    // than ship a level with a room nobody can get to.
    joinStrandedRooms();

    // The ground is not built here. It is cut from the theme as much as from
    // the shapes, and applyTheme always follows a generate, so building it
    // now only means building it twice.
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

bool Arena::allRoomsReachable()
{
    // The sweep the creatures already use, run once from the mouth. Anything
    // it does not colour in cannot be swum to from where the dive starts.
    updateFlow(m_start);

    for (const Room& room : m_rooms)
    {
        const int cell = nearestOpenCell(room.centre);

        if (cell < 0 || m_flowFrom[static_cast<std::size_t>(cell)] < 0)
            return false;
    }

    return true;
}

int Arena::unreachableRooms()
{
    updateFlow(m_start);

    int missing = 0;
    for (const Room& room : m_rooms)
    {
        const int cell = nearestOpenCell(room.centre);
        if (cell < 0 || m_flowFrom[static_cast<std::size_t>(cell)] < 0)
            ++missing;
    }

    return missing;
}

void Arena::carveTo(sf::Vector2f from, sf::Vector2f to, float width, unsigned int seed)
{
    // The same overlapping mouthfuls a passage is made of, cut straight. It is
    // only ever used to join something the walk left stranded, so it does not
    // need to wander - it needs to be there.
    const sf::Vector2f leg = to - from;
    const float length = leg.length();
    if (length <= 1.0f)
        return;

    Seeded random{ seed };

    const int bites = std::max(2, static_cast<int>(length / (width * config::BiteSpacing)));

    for (int i = 0; i <= bites; ++i)
    {
        const float along = static_cast<float>(i) / static_cast<float>(bites);
        const float size = width * random.range(0.88f, 1.14f);

        Chamber bite{ from + leg * along, size, ChamberKind::Plain,
                      random.range(0.0f, 6.28f), {}, 0.0f };

        bite.warp[0] = random.range(0.12f, 0.24f);
        bite.warp[1] = random.range(0.06f, 0.14f);
        bite.warp[2] = random.range(0.03f, 0.08f);
        bite.warp[3] = random.range(0.02f, 0.05f);

        settleRim(bite);
        m_chambers.push_back(bite);
    }

    m_corridors.push_back({ from, to, width });
}

void Arena::joinStrandedRooms()
{
    // Whatever is still cut off after the scenery has been taken away is the
    // walk's own doing - two rooms whose passage came out too thin somewhere
    // along its length, or a loop-back that never quite met. Rather than roll
    // the whole basin again and hope, the hole is simply cut.
    for (int pass = 0; pass < config::CarveAttempts; ++pass)
    {
        updateFlow(m_start);

        int stranded = -1;
        for (std::size_t i = 0; i < m_rooms.size(); ++i)
        {
            const int cell = nearestOpenCell(m_rooms[i].centre);

            if (cell < 0 || m_flowFrom[static_cast<std::size_t>(cell)] < 0)
            {
                stranded = static_cast<int>(i);
                break;
            }
        }

        if (stranded < 0)
            return;

        // To the nearest room that CAN be reached, so the cut is as short as
        // it can be and lands somewhere the player was going anyway.
        int best = -1;
        float nearest = 1.0e9f;

        for (std::size_t i = 0; i < m_rooms.size(); ++i)
        {
            if (static_cast<int>(i) == stranded)
                continue;

            const int cell = nearestOpenCell(m_rooms[i].centre);
            if (cell < 0 || m_flowFrom[static_cast<std::size_t>(cell)] < 0)
                continue;

            const float gap = (m_rooms[i].centre
                               - m_rooms[static_cast<std::size_t>(stranded)].centre).length();

            if (gap < nearest)
            {
                nearest = gap;
                best = static_cast<int>(i);
            }
        }

        if (best < 0)
            return; // nothing is reachable at all, which the mouth rules out

        carveTo(m_rooms[static_cast<std::size_t>(stranded)].centre,
                m_rooms[static_cast<std::size_t>(best)].centre,
                config::CorridorRadius, static_cast<unsigned int>(pass) * 2654435761u + 17u);

        m_links[static_cast<std::size_t>(stranded)].push_back(best);
        m_links[static_cast<std::size_t>(best)].push_back(stranded);

        buildShapeIndex();
        buildRoutes();
        buildNavGrid();
    }
}

void Arena::clearBlockedRoutes()
{
    if (m_rooms.empty() || m_rubbleFrom >= m_pillars.size())
        return;

    // Rubble is the only thing here that is purely decoration, so rubble is
    // what goes. Worst first: how close a lump sits to the line a passage
    // runs along is a good enough proxy for how likely it is to be standing
    // in the one gap that matters.
    const auto toAxis = [this](const Pillar& lump) {
        float nearest = 1.0e9f;

        for (const Corridor& corridor : m_corridors)
        {
            const sf::Vector2f on = closestOnSegment(corridor.from, corridor.to, lump.centre);
            nearest = std::min(nearest, (lump.centre - on).length() - lump.radius);
        }

        return nearest;
    };

    // Removed in batches rather than one at a time: rebuilding the grid to
    // test costs about as much as cutting the basin did, and a level with a
    // dozen bad lumps would spend a second finding that out.
    // Sorted once: worst last, so a pass is a resize rather than a search.
    std::sort(m_pillars.begin() + static_cast<std::ptrdiff_t>(m_rubbleFrom), m_pillars.end(),
              [&toAxis](const Pillar& a, const Pillar& b) { return toAxis(a) > toAxis(b); });

    while (m_pillars.size() > m_rubbleFrom)
    {
        if (allRoomsReachable())
            return;

        const std::size_t rubble = m_pillars.size() - m_rubbleFrom;
        const std::size_t keep = m_rubbleFrom + rubble / 2;

        m_pillars.resize(keep);

        buildShapeIndex();
        buildNavGrid();
    }

    // Still cut off with every lump gone means the walk itself produced a
    // basin that does not join up, which the loop-backs make vanishingly
    // unlikely - but a level nobody can finish is worse than a bare one.
    if (!allRoomsReachable())
    {
        m_pillars.resize(m_rubbleFrom);
        buildShapeIndex();
        buildNavGrid();
    }
}

bool Arena::isNavOpen(int index) const
{
    return index >= 0 && index < static_cast<int>(m_navOpen.size())
           && m_navOpen[static_cast<std::size_t>(index)] != 0u;
}

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

    // The sweep is now about a grid that no longer exists, and updateFlow
    // skips its work when it is asked for a source it thinks it already swept
    // from. Without this line a freshly cut basin keeps the last one's answer
    // until the sub happens to cross a cell boundary, and every creature in it
    // spends that time believing there is no way through to anywhere.
    m_flowSource = -1;
    m_flowFrom.assign(cells, -1);

    m_navPoint.assign(cells, { 0.0f, 0.0f });

    // A cell is open if ANY of it is open, not if its middle happens to be.
    // A throat is eighty pixels across and a cell is seventy, so a passage can
    // very easily pass between two sample points and leave the grid believing
    // there is no way through - which is how a level ends up with rooms that
    // simply cannot be reached, and creatures convinced of the same.
    //
    // Where the open point turned out to be is kept, because that is the point
    // everything is later told to swim to. Snapping to the geometric middle
    // would send it back into the rock this search was done to avoid.
    static const sf::Vector2f Probes[] = {
        { 0.0f, 0.0f },
        { -0.32f, 0.0f }, { 0.32f, 0.0f }, { 0.0f, -0.32f }, { 0.0f, 0.32f },
        { -0.32f, -0.32f }, { 0.32f, -0.32f }, { -0.32f, 0.32f }, { 0.32f, 0.32f },
    };

    for (int y = 0; y < m_navHeight; ++y)
    {
        for (int x = 0; x < m_navWidth; ++x)
        {
            const std::size_t index = static_cast<std::size_t>(y * m_navWidth + x);

            const sf::Vector2f middle =
                m_navOrigin + sf::Vector2f{ (static_cast<float>(x) + 0.5f) * config::NavCell,
                                            (static_cast<float>(y) + 0.5f) * config::NavCell };

            m_navPoint[index] = middle;

            // Most of a basin's bounding box is solid rock, and asking nine
            // questions about each of those cells is eight questions wasted.
            // One tells you whether any part of the cell could be water.
            if (depthAt(middle) > config::NavCell)
                continue;

            for (const sf::Vector2f& probe : Probes)
            {
                const sf::Vector2f at = middle + probe * config::NavCell;

                if (!isOpen(at, config::NavClearance))
                    continue;

                m_navOpen[index] = 1u;
                m_navPoint[index] = at;
                break;
            }
        }
    }
}

sf::Vector2f Arena::cellCentre(int index) const
{
    if (m_navWidth <= 0)
        return m_center;

    // Where the open water in this cell actually was, which for a cell in a
    // narrow passage is not its middle. Everything that routes is told to swim
    // here, so handing back the middle would aim it at the rock the search
    // went to the trouble of avoiding.
    if (index >= 0 && index < static_cast<int>(m_navPoint.size()))
        return m_navPoint[static_cast<std::size_t>(index)];

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

void Arena::settleRim(Chamber& chamber)
{
    // Every term pulling the same way at once, which is the worst case the
    // field has to allow for.
    const float swing = chamber.warp[0] + chamber.warp[1]
                        + chamber.warp[2] + chamber.warp[3];

    chamber.reach = chamber.radius * (1.0f + config::ChamberLean * chamber.warp[0])
                    * (1.0f + config::ChamberRipple * swing);

    // sin(n*theta + phi) * w is the imaginary part of z^n times w*e^(i*phi),
    // so the phase and the weight of each harmonic collapse into one complex
    // number here and never have to be looked at again. The four offsets are
    // the ones the outline was always made of.
    const float offsets[4] = { chamber.phase, chamber.phase * -1.7f,
                               chamber.phase * 2.3f, chamber.phase * -3.1f };

    for (int i = 0; i < 4; ++i)
    {
        chamber.harmonic[i][0] = chamber.warp[i] * std::cos(offsets[i]);
        chamber.harmonic[i][1] = chamber.warp[i] * std::sin(offsets[i]);
    }

    // And the same trick for the squash, which is a cosine of one theta.
    const float pull = config::ChamberLean * chamber.warp[0];
    chamber.lean[0] = pull * std::cos(chamber.phase * -0.7f);
    chamber.lean[1] = pull * std::sin(chamber.phase * -0.7f);
}

float Arena::edgeRadius(const Chamber& chamber, sf::Vector2f offset) const
{
    // Four harmonics rather than two, with weights rolled per chamber and
    // frequencies that do not divide into each other, so the rim never comes
    // back round to where it started. Two waves gave a wobbled circle; this
    // gives an outline with bays, noses and a grain of roughness on top - the
    // difference between a shape and a wall.
    //
    // Written as powers of the direction rather than as angles: with the phases
    // already folded into the weights, the whole outline is eight complex
    // multiplications and no trigonometry at all. The same curve, and the one
    // change that took the field from being the frame to being a rounding
    // error in it.
    const float span = offset.lengthSquared();
    float ux = 1.0f;
    float uy = 0.0f;

    if (span > 1.0e-12f)
    {
        const float scale = 1.0f / std::sqrt(span);
        ux = offset.x * scale;
        uy = offset.y * scale;
    }

    const auto times = [](float ar, float ai, float br, float bi, float& r, float& i) {
        const float real = ar * br - ai * bi;
        i = ar * bi + ai * br;
        r = real;
    };

    float x2 = 0.0f, y2 = 0.0f, x3 = 0.0f, y3 = 0.0f, x4 = 0.0f, y4 = 0.0f;
    float x5 = 0.0f, y5 = 0.0f, x8 = 0.0f, y8 = 0.0f, x11 = 0.0f, y11 = 0.0f;
    float x16 = 0.0f, y16 = 0.0f, x19 = 0.0f, y19 = 0.0f;

    times(ux, uy, ux, uy, x2, y2);
    times(x2, y2, ux, uy, x3, y3);
    times(x2, y2, x2, y2, x4, y4);
    times(x4, y4, ux, uy, x5, y5);
    times(x4, y4, x4, y4, x8, y8);
    times(x8, y8, x3, y3, x11, y11);
    times(x8, y8, x8, y8, x16, y16);
    times(x16, y16, x3, y3, x19, y19);

    // The imaginary part of each power against its own weight, which is the
    // sine term it stands for.
    const float wave = x3 * chamber.harmonic[0][1] + y3 * chamber.harmonic[0][0]
                       + x5 * chamber.harmonic[1][1] + y5 * chamber.harmonic[1][0]
                       + x11 * chamber.harmonic[2][1] + y11 * chamber.harmonic[2][0]
                       + x19 * chamber.harmonic[3][1] + y19 * chamber.harmonic[3][0];

    // Squashed towards one side as well as rippled: a cave mouth is rarely in
    // the middle of the room it opens into. The real part, this time.
    const float lean = 1.0f + ux * chamber.lean[0] - uy * chamber.lean[1];

    return chamber.radius * lean * (1.0f + config::ChamberRipple * wave);
}

sf::Vector2f Arena::currentAt(sf::Vector2f position) const
{
    sf::Vector2f drag{ 0.0f, 0.0f };

    for (const Current& current : m_currents)
    {
        if (current.spins())
        {
            const sf::Vector2f offset = position - current.from;
            const float distance = offset.length();
            if (distance >= current.radius || distance <= 1.0f)
                continue;

            // Mostly round, a little inward, and dying at the rim so the edge
            // of a pit is somewhere you can hold rather than a line you fall
            // over.
            const float fall = 1.0f - distance / current.radius;
            const sf::Vector2f inward = -offset / distance;
            const sf::Vector2f around{ -inward.y, inward.x };

            drag += (around * 0.78f + inward * 0.34f) * (current.strength * fall);
            continue;
        }

        const sf::Vector2f axis = current.to - current.from;
        const float span = axis.lengthSquared();
        if (span <= 1.0f)
            continue;

        // Cheap rejection before the projection, because this is asked once
        // per moving thing per step and most of them are nowhere near.
        const sf::Vector2f low{ std::min(current.from.x, current.to.x) - current.radius,
                                std::min(current.from.y, current.to.y) - current.radius };
        const sf::Vector2f high{ std::max(current.from.x, current.to.x) + current.radius,
                                 std::max(current.from.y, current.to.y) + current.radius };

        if (position.x < low.x || position.x > high.x
            || position.y < low.y || position.y > high.y)
        {
            continue;
        }

        const float along = std::clamp((position - current.from).dot(axis) / span, 0.0f, 1.0f);
        const sf::Vector2f on = current.from + axis * along;
        const float off = (position - on).length();

        if (off >= current.radius)
            continue;

        // Dying at the walls, and easing in at both ends. Without the ends the
        // sub is punched sideways the instant it crosses the mouth, which
        // reads as a bug rather than as water.
        const float fall = 1.0f - off / current.radius;
        const float mouth = std::min(1.0f, std::min(along, 1.0f - along)
                                               / config::CurrentMouthTaper);

        drag += axis.normalized() * (current.strength * fall * mouth);
    }

    return drag;
}


void Arena::buildShapeIndex()
{
    // Wide enough for every rim at full stretch rather than for the centres
    // the bounds are measured from, and a margin past that so a query just
    // outside the basin still lands in a real cell.
    sf::Vector2f low{ 1.0e9f, 1.0e9f };
    sf::Vector2f high{ -1.0e9f, -1.0e9f };

    const auto cover = [&](sf::Vector2f centre, float radius) {
        low.x = std::min(low.x, centre.x - radius);
        low.y = std::min(low.y, centre.y - radius);
        high.x = std::max(high.x, centre.x + radius);
        high.y = std::max(high.y, centre.y + radius);
    };

    for (const Chamber& chamber : m_chambers)
        cover(chamber.centre, chamber.reach + config::ShapeMarginRock);

    for (const Pillar& pillar : m_pillars)
        cover(pillar.centre, pillar.radius + config::ShapeMarginWater);

    if (low.x > high.x)
    {
        m_gridWidth = 0;
        m_gridHeight = 0;
        m_chamberStart.clear();
        m_chamberList.clear();
        m_pillarStart.clear();
        m_pillarList.clear();
        return;
    }

    m_gridOrigin = low;
    m_gridWidth = static_cast<int>((high.x - low.x) / config::ShapeCell) + 1;
    m_gridHeight = static_cast<int>((high.y - low.y) / config::ShapeCell) + 1;

    const std::size_t cells = static_cast<std::size_t>(m_gridWidth)
                              * static_cast<std::size_t>(m_gridHeight);

    // Counted, turned into offsets, then filled: two passes over the shapes
    // and two allocations, rather than a vector per cell and a heap walk on
    // every query.
    const auto file = [&](std::vector<std::uint32_t>& start,
                          std::vector<std::uint32_t>& list,
                          const auto& shapes, auto radiusOf) {
        start.assign(cells + 1, 0u);

        const auto span = [&](sf::Vector2f centre, float radius,
                              int& x0, int& y0, int& x1, int& y1) {
            const sf::Vector2f from = (centre - sf::Vector2f{ radius, radius } - m_gridOrigin)
                                      / config::ShapeCell;
            const sf::Vector2f to = (centre + sf::Vector2f{ radius, radius } - m_gridOrigin)
                                    / config::ShapeCell;

            x0 = std::clamp(static_cast<int>(std::floor(from.x)), 0, m_gridWidth - 1);
            y0 = std::clamp(static_cast<int>(std::floor(from.y)), 0, m_gridHeight - 1);
            x1 = std::clamp(static_cast<int>(std::floor(to.x)), 0, m_gridWidth - 1);
            y1 = std::clamp(static_cast<int>(std::floor(to.y)), 0, m_gridHeight - 1);
        };

        for (const auto& shape : shapes)
        {
            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            span(shape.centre, radiusOf(shape), x0, y0, x1, y1);

            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                    ++start[static_cast<std::size_t>(y * m_gridWidth + x) + 1];
            }
        }

        for (std::size_t i = 1; i < start.size(); ++i)
            start[i] += start[i - 1];

        list.assign(start.back(), 0u);

        std::vector<std::uint32_t> cursor(start.begin(), start.end() - 1);

        for (std::size_t index = 0; index < shapes.size(); ++index)
        {
            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            span(shapes[index].centre, radiusOf(shapes[index]), x0, y0, x1, y1);

            for (int y = y0; y <= y1; ++y)
            {
                for (int x = x0; x <= x1; ++x)
                {
                    const std::size_t cell = static_cast<std::size_t>(y * m_gridWidth + x);
                    list[cursor[cell]++] = static_cast<std::uint32_t>(index);
                }
            }
        }
    };

    file(m_chamberStart, m_chamberList, m_chambers,
         [](const Chamber& chamber) { return chamber.reach + config::ShapeMarginRock; });

    file(m_pillarStart, m_pillarList, m_pillars,
         [](const Pillar& pillar) { return pillar.radius + config::ShapeMarginWater; });
}

int Arena::gridIndex(sf::Vector2f position) const
{
    if (m_gridWidth <= 0 || m_gridHeight <= 0)
        return -1;

    const sf::Vector2f local = (position - m_gridOrigin) / config::ShapeCell;
    const int x = static_cast<int>(std::floor(local.x));
    const int y = static_cast<int>(std::floor(local.y));

    if (x < 0 || y < 0 || x >= m_gridWidth || y >= m_gridHeight)
        return -1;

    return y * m_gridWidth + x;
}

float Arena::depthScan(sf::Vector2f position) const
{
    float best = 1.0e9f;

    // A shape can only improve the answer if it is nearer than the best so far
    // plus its own reach, and that comparison can be made on squared distances
    // with no square root at all.
    for (const Chamber& chamber : m_chambers)
    {
        const sf::Vector2f offset = position - chamber.centre;
        const float slack = best + chamber.reach;

        if (slack <= 0.0f || offset.lengthSquared() > slack * slack)
            continue;

        best = std::min(best, offset.length() - edgeRadius(chamber, offset));
    }

    // Passages are cut as chambers too, so there is nothing else in the union:
    // one shape everywhere, and no straight walls anywhere in the basin.

    // Pillars are cut back out of whatever the rooms opened up, so they are a
    // max rather than a min. They are kept narrower than the corridors, which
    // is what stops one ever sealing a route instead of covering it.
    return againstPillars(position, best, 0u,
                          static_cast<std::uint32_t>(m_pillars.size()), false);
}

// A column can only matter when it reaches further than the answer already
// found, and how far it reaches is known before any distance is worked out.
// That test is worth making because the alternative is a square root for every
// column in the room, and in open water almost none of them are close enough
// to change anything.
float Arena::againstPillars(sf::Vector2f position, float best,
                            std::uint32_t from, std::uint32_t to, bool indexed) const
{
    for (std::uint32_t i = from; i < to; ++i)
    {
        const Pillar& pillar = m_pillars[indexed ? m_pillarList[i] : i];

        const float slack = pillar.radius - best;
        if (slack <= 0.0f)
            continue;

        const float distanceSquared = (position - pillar.centre).lengthSquared();
        if (distanceSquared >= slack * slack)
            continue;

        best = pillar.radius - std::sqrt(distanceSquared);
    }

    return best;
}

float Arena::depthAt(sf::Vector2f position) const
{
    // Negative inside the water, positive in rock, and the value is how far
    // from the nearest edge. Everything about the map's shape is this number.
    const int cell = gridIndex(position);
    if (cell < 0)
        return depthScan(position);

    float best = 1.0e9f;

    const std::uint32_t chamberEnd = m_chamberStart[static_cast<std::size_t>(cell) + 1];

    for (std::uint32_t i = m_chamberStart[static_cast<std::size_t>(cell)];
         i < chamberEnd; ++i)
    {
        const Chamber& chamber = m_chambers[m_chamberList[i]];
        const sf::Vector2f offset = position - chamber.centre;
        const float slack = best + chamber.reach;

        if (slack <= 0.0f || offset.lengthSquared() > slack * slack)
            continue;

        best = std::min(best, offset.length() - edgeRadius(chamber, offset));
    }

    // Too far into the rock for this bucket to vouch for itself: a chamber
    // filed elsewhere could be nearer than anything filed here, so ask every
    // shape. Only points buried well inside rock get here, and a frame spends
    // no time there.
    if (best > config::ShapeMarginRock)
        return depthScan(position);

    // Water deeper than the margin the columns were filed with has the same
    // problem, but only for the columns - the rim is already settled. There
    // are a couple of hundred of those at most, and the reach test above
    // throws out nearly all of them without a square root, so the middle of a
    // great hall simply reads the whole list rather than falling back to the
    // whole basin.
    if (best < -config::ShapeMarginWater)
    {
        return againstPillars(position, best, 0u,
                              static_cast<std::uint32_t>(m_pillars.size()), false);
    }

    return againstPillars(position, best,
                          m_pillarStart[static_cast<std::size_t>(cell)],
                          m_pillarStart[static_cast<std::size_t>(cell) + 1], true);
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

    // How thick the stone shelf is at a given piece of the world. A band of
    // even width reads as a kerb rather than as rock, so it has to vary - but
    // it has to vary with the PLACE and not with the chamber.
    //
    // It used to be sin(5*theta) measured from each chamber's own centre and
    // offset by that chamber's own phase. Offsetting one outline outward by a
    // constant and offsetting another by the same constant gives two curves
    // that still meet cleanly, because a union offset outward is just the
    // union offset outward. Offset them by DIFFERENT amounts at the same piece
    // of wall and they cross instead, and two curves crossing is a cusp. A
    // room is one or two big mouthfuls and the cusps are rare; a passage is
    // forty small ones in a row, which is why every narrow corridor in the
    // basin had a comb of black spikes down both of its walls.
    //
    // Read off the world instead and every mouthful agrees about how thick the
    // shelf is here, so the band wanders without ever tearing.
    const auto shelfAt = [](sf::Vector2f at) {
        return std::sin(at.x * 0.0195f) * std::cos(at.y * 0.0163f)
               + 0.45f * std::sin((at.x + at.y) * 0.0087f);
    };

    // The rim of a chamber, wobbled, as a triangle fan. grow pushes it outward,
    // which is how the same outline serves as rock, as cut lip and as floor
    // without any of the three being able to disagree with the other two.
    const auto chamberFan = [this, &shelfAt](sf::VertexArray& into, const Chamber& chamber,
                                             float grow, float wave, sf::Color colour) {
        const auto reach = [&](sf::Vector2f d) {
            const float rim = edgeRadius(chamber, d);

            // The floor is the rim itself, so it never asks about the shelf.
            if (grow <= 0.0f)
                return rim;

            return rim + grow * (1.0f + wave * shelfAt(chamber.centre + d * rim));
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

    // Everything above is cut once and then looked at for the rest of the
    // dive, so it goes to the card now rather than across the bus every
    // frame. All three or none: a half-parked basin would draw its floor over
    // its own rock.
    m_onCard = mesh::parkOnCard(m_rock, m_rockCard)
               && mesh::parkOnCard(m_floor, m_floorCard)
               && mesh::parkOnCard(m_shade, m_shadeCard);
}

void Arena::buildWallShading()
{
    const sf::Color shade = config::WallShadeColor;
    const sf::Color rim = config::WallRimLight;

    // How much of a wall a rim point is, from nothing to all of it.
    //
    // This used to be a yes or a no, and it only asked one question: is there
    // rock a little way beyond? On a basin cut from a thousand overlapping
    // mouthfuls that is not enough. The rim of one mouthful buried deep inside
    // its neighbour still has rock somewhere out past it, so it answered yes,
    // and the shading was drawn along a rim that is nowhere near the wall you
    // can see - a bright line hanging in open water with nothing under it.
    //
    // So it asks two things now, and answers with a fraction rather than a
    // verdict. Is this point actually ON the surface of the union, or is it
    // buried in water some other chamber opened? And is what lies beyond it
    // rock, or is it the doorway into the next room? The fraction is what
    // stops a run of shading beginning and ending on a hard edge, which is
    // what a yes-or-no test produces every time the answer flips.
    const auto wallness = [this](sf::Vector2f at, sf::Vector2f out) {
        const float surface = depthAt(at);

        // Buried. Something else opened this water and this rim is imaginary.
        if (surface < -config::WallEdgeDepth)
            return 0.0f;

        const float beyond = depthAt(at + out * config::WallEdgeProbe);

        // A doorway: more water out there, so this is not an edge at all.
        if (beyond <= 0.0f)
            return 0.0f;

        const float onSurface = std::clamp(1.0f + surface / config::WallEdgeDepth, 0.0f, 1.0f);
        const float solid = std::clamp(beyond / config::WallEdgeProbe, 0.0f, 1.0f);

        return onSurface * solid;
    };

    // Sand in the shadow of the rock, fading back to open floor, and then a
    // bright line right on the waterline. Together they are what says this is
    // as far as you go, without anything having to look like a fence.
    //
    // Built by hand rather than through appendRingQuad because the two ends of
    // a quad need different strengths - that is the whole point of carrying a
    // fraction this far.
    const auto strip = [this](sf::Vector2f p0, sf::Vector2f p1,
                              sf::Vector2f in0, sf::Vector2f in1,
                              float width, sf::Color colour, float w0, float w1) {
        const auto tint = [&colour](float amount) {
            return sf::Color(colour.r, colour.g, colour.b,
                             static_cast<std::uint8_t>(static_cast<float>(colour.a)
                                                       * std::clamp(amount, 0.0f, 1.0f)));
        };

        const sf::Color clear(colour.r, colour.g, colour.b, 0);

        const sf::Vector2f a = p0 - in0 * width;
        const sf::Vector2f b = p1 - in1 * width;

        m_shade.append(sf::Vertex{ a, clear });
        m_shade.append(sf::Vertex{ p0, tint(w0) });
        m_shade.append(sf::Vertex{ p1, tint(w1) });

        m_shade.append(sf::Vertex{ a, clear });
        m_shade.append(sf::Vertex{ p1, tint(w1) });
        m_shade.append(sf::Vertex{ b, clear });
    };

    for (const Chamber& chamber : m_chambers)
    {
        const std::size_t steps = segmentsFor(chamber.radius);

        // How far in the shadow may fall: as far as there is water to fall on.
        //
        // This used to be half the radius of whichever mouthful happened to be
        // drawing it, and a passage is cut from forty small mouthfuls sitting
        // in a row inside two big ones. So the same stretch of wall got a
        // seventy pixel shadow where a room's rim passed behind it and a
        // twenty pixel one two steps later, which is the shadow stepping in
        // and out along a corridor for no reason the player can see.
        //
        // Asked of the field instead, and asked properly: walk in from the rim
        // and stop at the middle of the water.
        //
        // Walking straight in off a wall, the distance to the nearest rock
        // should be exactly how far you have walked - your own wall, behind
        // you. The moment it is less than that, some other wall is nearer and
        // you have crossed the middle of the passage. Past that point the quad
        // folds back through itself and comes out of the far side of the rock
        // as a black spike, which is what was hanging off every narrow wall in
        // the basin.
        const auto span = [this](sf::Vector2f at, sf::Vector2f out) {
            constexpr float Step = 9.0f;

            // The common case in one question rather than eight: if the point
            // a whole shadow's width in is still that far from anything, the
            // only thing near it is the wall we started from, and there is a
            // whole shadow's worth of water to fall on.
            if (-depthAt(at - out * config::WallShadeWidth)
                >= config::WallShadeWidth * 0.94f)
            {
                return config::WallShadeWidth;
            }

            float walked = Step;

            while (walked + Step <= config::WallShadeWidth)
            {
                if (-depthAt(at - out * (walked + Step)) < walked)
                    break;

                walked += Step;
            }

            return std::clamp(walked, config::WallRimWidth, config::WallShadeWidth);
        };

        // The weight and the reach at each rim point, worked out once and
        // shared by the two segments that meet there - so neighbouring strips
        // always agree about the corner between them and there is no seam
        // down it.
        std::vector<float> weight(steps + 1, 0.0f);
        std::vector<float> deep(steps + 1, config::WallShadeWidth);
        std::vector<sf::Vector2f> rimAt(steps + 1);

        for (std::size_t i = 0; i <= steps; ++i)
        {
            const sf::Vector2f d = sf::Vector2f(1.0f, segmentAngle(i % steps, steps));

            rimAt[i] = chamber.centre + d * edgeRadius(chamber, d);
            weight[i] = wallness(rimAt[i], d);

            // And never past the middle of the mouthful that is drawing it,
            // whatever the field says: a fan that reaches its own centre is
            // already as far as a fan can go.
            if (weight[i] > 0.0f)
                deep[i] = std::min(span(rimAt[i], d), chamber.radius * 0.8f);
        }

        for (std::size_t i = 0; i < steps; ++i)
        {
            if (weight[i] <= 0.0f && weight[i + 1] <= 0.0f)
                continue;

            const sf::Vector2f d0 = sf::Vector2f(1.0f, segmentAngle(i, steps));
            const sf::Vector2f d1 = sf::Vector2f(1.0f, segmentAngle(i + 1, steps));

            const float reach = std::min(deep[i], deep[i + 1]);

            strip(rimAt[i], rimAt[i + 1], d0, d1, reach, shade,
                  weight[i], weight[i + 1]);
            strip(rimAt[i], rimAt[i + 1], d0, d1, std::min(reach, config::WallRimWidth), rim,
                  weight[i], weight[i + 1]);
        }
    }

    // Columns last
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
                           shade, sf::Color(shade.r, shade.g, shade.b, 0));
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
        if (!current.spins())
        {
            // A flowing passage says so with streaks running down it. Without
            // them the water is invisible and the sub simply steers badly,
            // which is the difference between a current and a bug.
            const sf::Vector2f axis = current.to - current.from;
            const float length = axis.length();
            if (length <= 1.0f)
                continue;

            const sf::Vector2f along = axis / length;
            const sf::Vector2f across{ -along.y, along.x };
            const int streaks = std::max(2, static_cast<int>(length / 26.0f));
            const float drift = std::fmod(m_time * current.strength * 0.35f, 40.0f);

            for (int i = 0; i < streaks; ++i)
            {
                const float head = std::fmod(static_cast<float>(i) * 40.0f + drift, length);
                const float tail = std::max(0.0f, head - 17.0f);

                // Off the axis by a fixed amount per streak, so the passage
                // reads as full of moving water rather than as one line down
                // the middle of it.
                const float side = (static_cast<float>((i * 7) % 5) / 4.0f - 0.5f)
                                   * current.radius * 1.30f;

                const sf::Vector2f a = current.from + along * tail + across * side;
                const sf::Vector2f b = current.from + along * head + across * side;

                const sf::Color faint(config::FlowMarkColor.r, config::FlowMarkColor.g,
                                      config::FlowMarkColor.b, 0);

                appendRingQuad(m_swirl,
                               a - across * 2.0f, b - across * 2.0f,
                               a + across * 2.0f, b + across * 2.0f,
                               faint, config::FlowMarkColor);
            }

            continue;
        }

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
                                   current.from + d0 * (radius - width),
                                   current.from + d1 * (radius - width),
                                   current.from + d0 * (radius + width),
                                   current.from + d1 * (radius + width),
                                   tint, tint);
                }
            }
        }
    }
}

void Arena::drawGround(sf::RenderTarget& target) const
{
    if (m_onCard)
    {
        target.draw(m_rockCard);
        target.draw(m_floorCard);
        target.draw(m_shadeCard);
    }
    else
    {
        target.draw(m_rock);
        target.draw(m_floor);
        target.draw(m_shade);
    }

    // The swirl is not parked: a pit is turning, so it is cut again every
    // frame and would only be uploaded again every frame anyway.
    target.draw(m_swirl);
}

float Arena::getLampReach() const
{
    return (m_theme->lightRadius + m_lampBonus) * m_lampFactor;
}

void Arena::buildLightPool()
{
    // A lamp, not a bubble. It used to be an even disc around the sub, which
    // meant the darkness cost the player nothing: everything within the radius
    // was equally visible whichever way they were pointing, so the one thing
    // the whole game is about - not being able to see - had no bearing on
    // anything they did. A beam makes looking where you are going and looking
    // where you are shooting two different choices.
    //
    // Both halves are built around the origin and pointing along +X, so
    // drawLight can carry them to the sub and turn them to the aim without any
    // of this having to be rebuilt.
    const sf::Color core = m_theme->light;
    const float reach = (m_theme->lightRadius + m_lampBonus) * m_lampFactor;

    // The halo first: the light that spills off a lamp rather than a second
    // light source. Without something close in a pure beam is unplayable - the
    // sub sits in a hole it cannot see out of sideways, and anything that
    // closes from behind is unanswerable rather than frightening.
    //
    // It was an even disc, which is to say it was a circle with a beam stuck
    // on the front of it, and the circle was the thing the eye went to. Two
    // things fix that. It is weighted by angle, so it is brightest where the
    // beam is and only a wash astern, which makes the two halves one shape.
    // And it falls off in steps of a square rather than in one straight run to
    // the rim, so there is no edge left to see.
    const float haloReach = reach * config::LampHaloFactor;

    const auto facing = [](sf::Angle angle) {
        const float ahead = 0.5f + 0.5f * std::cos(angle.asRadians());
        return config::LampHaloBack + (1.0f - config::LampHaloBack) * ahead;
    };

    const auto spill = [&core](float amount) {
        return sf::Color(core.r, core.g, core.b,
                         static_cast<std::uint8_t>(static_cast<float>(core.a)
                                                   * config::LampHaloBrightness
                                                   * std::clamp(amount, 0.0f, 1.0f)));
    };

    for (std::size_t i = 0; i < Segments; ++i)
    {
        const sf::Angle a0 = segmentAngle(i);
        const sf::Angle a1 = segmentAngle(i + 1);
        const float w0 = facing(a0);
        const float w1 = facing(a1);

        for (int ring = 0; ring < config::LampHaloRings; ++ring)
        {
            const float t0 = static_cast<float>(ring) / config::LampHaloRings;
            const float t1 = static_cast<float>(ring + 1) / config::LampHaloRings;
            const float f0 = (1.0f - t0) * (1.0f - t0);
            const float f1 = (1.0f - t1) * (1.0f - t1);

            const sf::Vector2f in0(haloReach * t0, a0);
            const sf::Vector2f in1(haloReach * t0, a1);
            const sf::Vector2f out0(haloReach * t1, a0);
            const sf::Vector2f out1(haloReach * t1, a1);

            m_lightPool.append(sf::Vertex{ in0, spill(f0 * w0) });
            m_lightPool.append(sf::Vertex{ out0, spill(f1 * w0) });
            m_lightPool.append(sf::Vertex{ out1, spill(f1 * w1) });

            m_lightPool.append(sf::Vertex{ in0, spill(f0 * w0) });
            m_lightPool.append(sf::Vertex{ out1, spill(f1 * w1) });
            m_lightPool.append(sf::Vertex{ in1, spill(f0 * w1) });
        }
    }

    // Then the beam. Quads rather than a fan: a fan meets at one vertex, and
    // one vertex can only be one brightness, so a fan cannot say that the beam
    // is dimmer at its edges than down its middle - which is the whole shape
    // of a beam.
    //
    // It is banded along its length as well as split across, because a beam
    // that starts at full strength on a ring somewhere out from the sub draws
    // that ring: an arc of light hanging in the water with nothing throwing
    // it. Rising out of the lamp and falling away at the far end is one shape
    // with the halo, which is what a lamp actually looks like.
    const std::size_t steps = Segments / 2;
    const float half = config::LampConeHalfAngle + m_lampSpread;

    // Cosine across the beam rather than a straight taper, so the edges wash
    // out instead of ending on a line. A line reads as a cone drawn on the
    // water; a wash reads as light.
    const auto across = [](float t) {
        return std::pow(std::cos(std::clamp(t, -1.0f, 1.0f) * 1.5707963f),
                        config::LampConeEdgeFade);
    };

    const auto beam = [&core](float amount) {
        return sf::Color(core.r, core.g, core.b,
                         static_cast<std::uint8_t>(static_cast<float>(core.a)
                                                   * std::clamp(amount, 0.0f, 1.0f)));
    };

    // Where the beam is along its own length, and how strong it is there.
    // Climbing out of the lamp, then falling the rest of the way.
    const float throat = haloReach * 0.8f;
    const std::array<std::pair<float, float>, 6> band{ {
        { 0.0f, 0.30f },
        { throat, 1.00f },
        { throat + (reach - throat) * 0.25f, 0.72f },
        { throat + (reach - throat) * 0.50f, 0.44f },
        { throat + (reach - throat) * 0.75f, 0.20f },
        { reach, 0.00f },
    } };

    for (std::size_t i = 0; i < steps; ++i)
    {
        const float t0 = static_cast<float>(i) / static_cast<float>(steps) * 2.0f - 1.0f;
        const float t1 = static_cast<float>(i + 1) / static_cast<float>(steps) * 2.0f - 1.0f;

        const sf::Angle a0 = sf::degrees(t0 * half);
        const sf::Angle a1 = sf::degrees(t1 * half);

        const float e0 = across(t0);
        const float e1 = across(t1);

        for (std::size_t b = 0; b + 1 < band.size(); ++b)
        {
            const auto [r0, s0] = band[b];
            const auto [r1, s1] = band[b + 1];

            const sf::Vector2f in0(r0, a0);
            const sf::Vector2f in1(r0, a1);
            const sf::Vector2f out0(r1, a0);
            const sf::Vector2f out1(r1, a1);

            m_lightPool.append(sf::Vertex{ in0, beam(e0 * s0) });
            m_lightPool.append(sf::Vertex{ out0, beam(e0 * s1) });
            m_lightPool.append(sf::Vertex{ out1, beam(e1 * s1) });

            m_lightPool.append(sf::Vertex{ in0, beam(e0 * s0) });
            m_lightPool.append(sf::Vertex{ out1, beam(e1 * s1) });
            m_lightPool.append(sf::Vertex{ in1, beam(e1 * s0) });
        }
    }
}

float Arena::lampReachAt(float degreesOffAim) const
{
    const float reach = getLampReach();
    const float halo = reach * config::LampHaloFactor;

    // The spill: a teardrop, brightest where the lamp points and never quite
    // nothing astern. Same weighting the pool itself is built with.
    const float facing = 0.5f + 0.5f * std::cos(sf::degrees(degreesOffAim).asRadians());
    const float spill = halo * (config::LampHaloBack
                                + (1.0f - config::LampHaloBack) * facing);

    // And the beam on top of it, eased in over the last few degrees of the
    // cone so the edge of the light is an edge of light and not a cut.
    const float off = std::abs(degreesOffAim);
    const float half = config::LampConeHalfAngle + m_lampSpread;
    const float t = std::clamp((half + config::DarkFeather - off) / config::DarkFeather,
                               0.0f, 1.0f);

    return std::max(config::DarkFloor, spill + (reach - spill) * (t * t * (3.0f - 2.0f * t)));
}

void Arena::buildDark()
{
    // Everything the lamp does not reach, as one mesh around the origin and
    // pointing along +X, so drawDark can carry it to the sub and turn it to
    // the aim without a vertex being rebuilt - exactly like the pool it is the
    // complement of.
    m_dark.clear();

    const sf::Color clear(0, 0, 0, 0);
    const sf::Color black(0, 0, 0, 255);

    for (std::size_t i = 0; i < Segments; ++i)
    {
        const sf::Angle a0 = segmentAngle(i);
        const sf::Angle a1 = segmentAngle(i + 1);

        const float r0 = lampReachAt(a0.wrapSigned().asDegrees());
        const float r1 = lampReachAt(a1.wrapSigned().asDegrees());

        // The black arriving, and then the black.
        appendRingQuad(m_dark,
                       sf::Vector2f(r0, a0), sf::Vector2f(r1, a1),
                       sf::Vector2f(r0 + config::DarkFade, a0),
                       sf::Vector2f(r1 + config::DarkFade, a1),
                       clear, black);

        appendRingQuad(m_dark,
                       sf::Vector2f(r0 + config::DarkFade, a0),
                       sf::Vector2f(r1 + config::DarkFade, a1),
                       sf::Vector2f(config::DarkReach, a0),
                       sf::Vector2f(config::DarkReach, a1),
                       black, black);
    }
}

void Arena::drawDark(sf::RenderTarget& target, sf::Vector2f lampCentre, sf::Angle aim) const
{
    sf::RenderStates states;
    states.transform.translate(lampCentre);
    states.transform.rotate(aim);

    target.draw(m_dark, states);
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

void Arena::setLamp(float bonus, float spread, float factor)
{
    // Rebuilding the pool every frame would be pointless work, so nothing
    // happens unless the lamp has actually changed size.
    if (std::abs(bonus - m_lampBonus) < 0.5f && std::abs(spread - m_lampSpread) < 0.25f
        && std::abs(factor - m_lampFactor) < 0.01f)
    {
        return;
    }

    m_lampBonus = std::max(0.0f, bonus);
    m_lampSpread = std::clamp(spread, 0.0f, 55.0f);
    m_lampFactor = std::clamp(factor, 0.05f, 4.0f);
    m_lightPool.clear();
    buildLightPool();
    buildDark();
}

void Arena::applyTheme(const DepthTheme& theme)
{
    m_theme = &theme;

    // The lamp and the darkness are the depth, so both are rebuilt outright
    // rather than tinted after the fact.
    m_lightPool.clear();
    m_gloom.clear();
    buildLightPool();
    buildDark();
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

void Arena::drawLight(sf::RenderTarget& target, sf::Vector2f lampCentre, sf::Angle aim) const
{
    sf::RenderStates states;
    states.blendMode = sf::BlendAdd; // light adds to the sand, it does not paint over it

    // The pool is the sub's own lamp, so it travels with the player rather
    // than sitting on the middle of a basin far larger than the screen.
    sf::RenderStates lamp = states;
    lamp.transform.translate(lampCentre);

    // Built pointing along +X and turned here, so the beam follows the aim
    // without a single vertex being rebuilt.
    lamp.transform.rotate(aim);
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
