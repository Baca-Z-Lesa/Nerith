#pragma once

#include "world/DepthTheme.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <vector>

// The water you are allowed to be in, and the light that makes it readable.
//
// It used to be one circle. It is now a basin cut out of rock: a chain of
// chambers joined by corridors, generated fresh for every level from a seed, so
// no two dives are the same shape and a level is always the same shape twice.
// The rock is solid - the sub, the creatures and every shot stop at it - which
// is what turns the map from a boundary into terrain: cover to break a line of
// fire, corners to lose something round, corridors that are a commitment.
//
// One chamber is always much larger than the others. That is where a boss
// fight happens, because a creature that size in a corridor is not a fight,
// it is a wall.
class Arena
{
public:
    Arena(sf::Vector2f center, float radius);

    // Cuts a new basin. Same seed, same map: a level is repeatable, and the
    // level number is the seed, so every level in the campaign is its own
    // place. bossRoom asks for one dominant chamber instead of a chain.
    void generate(unsigned int seed, bool bossRoom);

    // Swapped in when the depth changes; rebuilds the lamp and the darkness.
    void applyTheme(const DepthTheme& theme);

    // The lamp, as the theme sets it, plus whatever was bought in the tree and
    // times whatever the sub is currently running at. Bonus is kept separate
    // from the theme so descending still dims the light - a bigger lamp makes
    // a dark depth survivable rather than making it look like a shallow one -
    // and the factor is what running dark turns down.
    // How far the beam carries, how wide it opens, and how much of it is
    // working at all. Spread is degrees of extra half-angle on the cone.
    void setLamp(float bonus, float spread, float factor);

    // How far the beam currently throws, for anything that needs to know what
    // is lit without reading the mesh back off the graphics card.
    float getLampReach() const;

    // And what colour it is, which is the depth's rather than the lamp's.
    sf::Color getLampColour() const { return m_theme->light; }

    void update(float deltaTime);

    // The rock, the lip along its edge and the floor inside it. Drawn under
    // everything, including the seabed dressing.
    void drawGround(sf::RenderTarget& target) const;

    // Additive light over the seabed. Drawn before the entities.
    void drawLight(sf::RenderTarget& target, sf::Vector2f lampCentre, sf::Angle aim) const;
    // Darkness swallowing the distance. Drawn after the seabed, before entities.
    void drawGloom(sf::RenderTarget& target) const;

    // The dark, as the exact complement of the lamp: everything the beam does
    // not reach is painted out. Drawn in the basin's own space and after
    // everything in it, because a creature standing in the black has to be
    // covered by the black rather than tinted by it.
    // What the water takes out of the light on the way to your eye: red
    // first, then green, blue last. Multiplied over everything, between the
    // entities and the dark.
    void drawAbsorb(sf::RenderTarget& target, sf::Vector2f lampCentre) const;

    void drawDark(sf::RenderTarget& target, sf::Vector2f lampCentre, sf::Angle aim) const;

    // Rock stops light. The lamp is a mesh built once around the origin, so it
    // knows nothing about what is in front of it: standing against a wall used
    // to light the room on the far side of it. This casts one ray per segment
    // from wherever the sub actually is and paints the far side of whatever it
    // hits, which is the only thing about the light that has to be worked out
    // per frame rather than per lamp.
    //
    // Call it once a frame before drawing; it is a render concern and touches
    // nothing the simulation reads.
    void castShadows(sf::Vector2f from, sf::Angle aim);
    void drawShadows(sf::RenderTarget& target, sf::Vector2f from, sf::Angle aim) const;

    sf::Vector2f getCenter() const { return m_center; }
    float getRadius() const { return m_radius; }

    // Where a dive starts, and where a boss fight is held.
    sf::Vector2f getStart() const { return m_start; }
    sf::Vector2f getHallCentre() const { return m_hallCentre; }
    float getHallRadius() const { return m_hallRadius; }

    // The box the whole basin fits in, for the camera and the map.
    sf::Vector2f getBoundsMin() const { return m_boundsMin; }
    sf::Vector2f getBoundsMax() const { return m_boundsMax; }

    // True when a circle of that radius at that point is entirely in water.
    bool isOpen(sf::Vector2f position, float radius) const;

    // True while any part of a circle at that position is still inside.
    bool overlaps(sf::Vector2f position, float radius) const;

    // Pushes a circle back into the water by the shortest route. Anything
    // hopelessly outside is dragged to the nearest chamber rather than left
    // stuck in rock.
    sf::Vector2f clampInside(sf::Vector2f position, float radius) const;

    // Which way the rock is facing at a point, pointing back into the water.
    // Zero when the point is not against anything, which is the caller's cue
    // that there was nothing to bounce off.
    sf::Vector2f wallNormal(sf::Vector2f position, float radius) const;

    // Evenly distributed over the water, for scattering things around.
    sf::Vector2f randomPointInside() const;

    // A point out at the edge of the water, for anything that belongs against
    // a wall: nests, and the reef dressing.
    sf::Vector2f randomEdgePoint(float inset) const;

    // What kind of room it is. A basin of nine identical discs is a shape
    // rather than a place, so every chamber past the first rolls for one of
    // these, and each changes how the room is fought and not only how it looks.
    enum class ChamberKind
    {
        Plain,   // open water
        Throat,  // entered down a neck too narrow to fight in
        Pillars, // rock columns standing in it, which break every line of fire
        Pit,     // water that turns, and will not let anything stand still
    };

    // The shapes themselves, for anything that has to draw the map rather than
    // ask questions about it.
    struct Chamber
    {
        sf::Vector2f centre;
        float        radius;
        ChamberKind  kind = ChamberKind::Plain;
        float        phase = 0.0f; // this rim's own offset, so no two match

        // How much of each harmonic this particular rim is made of. Rolled per
        // chamber rather than shared, because a single shape function with a
        // phase shift gives every cavern the same outline turned a bit, and
        // the eye reads that as one blob repeated rather than as rock.
        float        warp[4] = { 0.50f, 0.28f, 0.14f, 0.08f };

        // The furthest this rim can possibly reach, worked out once when the
        // room is cut. The field uses it to skip shapes without a square root,
        // and getting it wrong shows up as holes in the floor.
        float        reach = 0.0f;

        // The same outline, folded into the form the field actually wants.
        // Each harmonic becomes one complex weight, so a rim is read off
        // powers of the direction rather than off one atan2 and five calls to
        // sin. It is the same curve to the last decimal place, for about a
        // tenth of the time - and the field is asked for it a few hundred
        // thousand times a dive, so that is most of what a dive costs.
        float        harmonic[4][2] = {};
        float        lean[2] = {};
    };

    struct Corridor { sf::Vector2f from; sf::Vector2f to; float radius; };

    // A column of rock standing in the water: subtracted from the field, so it
    // stops the sub, the creatures and every shot exactly as the wall does.
    struct Pillar { sf::Vector2f centre; float radius; };

    // The turning water in a pit, as a disc with a pull that dies at its rim.
    // Water that is doing something to whatever is in it. A pit turns on the
    // spot and has from == to; a passage flows one way along its own axis and
    // does not, which is what makes a route easy in and slow back out.
    struct Current
    {
        sf::Vector2f from;
        sf::Vector2f to;
        float        radius;
        float        strength;

        bool spins() const { return from == to; }
    };

    const std::vector<Chamber>& getChambers() const { return m_chambers; }
    const std::vector<Corridor>& getCorridors() const { return m_corridors; }
    const std::vector<Pillar>& getPillars() const { return m_pillars; }

    // What the water is doing to anything at that point, in pixels per second.
    // Zero everywhere except inside a pit.
    sf::Vector2f currentAt(sf::Vector2f position) const;

    // --- Getting across it ---
    // The basin is generated as a graph of rooms joined by passages, so
    // anything crossing it walks that graph rather than pushing at a wall and
    // hoping. Without this a creature that loses its line simply grinds
    // against rock for the rest of the level, and a level built round killing
    // everything can stop being finishable.
    struct Room
    {
        sf::Vector2f centre;
        float        radius;
        ChamberKind  kind;
    };

    // Which room a point belongs to, -1 only if there are none at all.
    int roomAt(sf::Vector2f position) const;

    // True when a circle of that radius can travel the whole way in water.
    bool clearLine(sf::Vector2f from, sf::Vector2f to, float radius) const;

    // Where to head next to get from one point to another: the target itself
    // when it can simply be swum to, otherwise the next room along the route.
    sf::Vector2f waypointToward(sf::Vector2f from, sf::Vector2f to, float radius) const;

    // The flow field, swept out from wherever the sub is a few times a second.
    // The room graph answers "which room next", which is right until both ends
    // are in the same room with a pillar between them - and that case is what
    // used to leave one creature alive and unreachable for a whole level.
    void updateFlow(sf::Vector2f target);
    sf::Vector2f flowAt(sf::Vector2f position) const;

    const std::vector<Room>& getRooms() const { return m_rooms; }

    // True when every room can be swum to from the mouth. The generator
    // already guarantees it; this is how that guarantee is checked.
    bool everyRoomReachable() { return allRoomsReachable(); }

    // How many rooms the sweep cannot colour in, for checking the above.
    int  unreachableRooms();
    std::size_t pillarCount() const { return m_pillars.size(); }
    std::size_t rubbleFrom() const { return m_rubbleFrom; }

    // The grid the routing already runs on, exposed so the map in the corner
    // can be drawn out of the same cells rather than out of a second idea of
    // where the water is. Seventy pixels a cell is about three and a half on
    // the minimap, which is the right grain for a pixel font.
    int  getNavWidth() const { return m_navWidth; }
    int  getNavHeight() const { return m_navHeight; }
    int  navCellAt(sf::Vector2f position) const { return cellIndex(position); }
    sf::Vector2f navCellCentre(int index) const { return cellCentre(index); }
    bool isNavOpen(int index) const;

private:
    // Where a chamber's rim actually is in a given direction. Both the field
    // and the drawing go through here, so what is seen and what is collided
    // with can never drift apart.
    float edgeRadius(const Chamber& chamber, sf::Vector2f offset) const;

    // Works out everything about a rim that follows from its weights and its
    // phase: how far it can reach, and the folded form the field reads.
    static void settleRim(Chamber& chamber);

    void buildRoutes();
    void buildNavGrid();

    // Whether every room can still be swum to from the mouth, and what to do
    // about it when one cannot. Rubble is scenery, so rubble is what gets
    // taken away rather than the map being regenerated from scratch.
    bool allRoomsReachable();
    void clearBlockedRoutes();

    // Cuts a passage where the walk failed to leave one. The last resort, and
    // the only thing that turns "almost always connected" into a guarantee.
    void carveTo(sf::Vector2f from, sf::Vector2f to, float width, unsigned int seed);
    void joinStrandedRooms();

    // --- The shape index ---
    // depthAt is the busiest function in the game: every collision, every
    // clamp, every wall normal and every vertex of the basin asks it where the
    // rock is. A basin cut from a thousand mouthfuls turned that into a walk
    // over a thousand shapes - forty kilobytes a query - so the cost stopped
    // being the arithmetic and became the walk itself.
    //
    // So the shapes are filed into a coarse grid once, and a query reads only
    // the handful filed under its own cell. Each shape is filed under every
    // cell its own reach can touch plus a margin, and that margin is what
    // makes the bucket answer exact rather than merely close: a shape left out
    // is further off than the margin, so it can only matter when the answer is
    // further out than the margin too - and that case asks everything instead.
    void buildShapeIndex();
    int  gridIndex(sf::Vector2f position) const;

    // Every shape, no index. The fallback, and what the index is measured
    // against when the basin is checked.
    float depthScan(sf::Vector2f position) const;

    // The column half of the field, over a run of the index or over the whole
    // list. Split out because the middle of a large room has to read every
    // column, and that is a different question from where the rim is.
    float againstPillars(sf::Vector2f position, float best,
                         std::uint32_t from, std::uint32_t to, bool indexed) const;

    int  cellIndex(sf::Vector2f position) const;
    int  nearestOpenCell(sf::Vector2f position) const;
    sf::Vector2f cellCentre(int index) const;
    void buildWallShading();
    void buildSwirl();
    void buildLightPool();
    void buildDark();
    void buildAbsorb();

    // How far the lamp carries at this angle off the aim. One function, so the
    // light and the dark can never disagree about where the beam ends.
    float lampReachAt(float degreesOffAim) const;
    void buildGloom();
    void updateCaustics();
    void buildGround();

    // How far a point is from open water, negative inside. Every query goes
    // through this one function, so open, clamp and normal can never disagree
    // about where the rock is.
    float depthAt(sf::Vector2f position) const;

    sf::Vector2f m_center;
    float        m_radius; // the old circular radius, kept for spawn margins
    float        m_time;

    std::vector<Chamber>  m_chambers;
    std::vector<Corridor> m_corridors;
    std::vector<Pillar>   m_pillars;

    // Where the decorative lumps begin in m_pillars. Everything before it is
    // structure a room was designed around and must not be removed.
    std::size_t           m_rubbleFrom = 0;
    std::vector<Current>  m_currents;

    // The graph: the rooms, who they join, and the precomputed next hop for
    // every pair of them. One breadth-first search per room at generation
    // time turns every later "which way" into a table lookup.
    std::vector<Room>             m_rooms;
    std::vector<std::vector<int>> m_links;
    std::vector<int>              m_nextHop;

    // The buckets, kept as counts turned into offsets rather than as a vector
    // of vectors: one allocation each, and a query reads one contiguous run
    // instead of chasing a pointer per cell.
    sf::Vector2f               m_gridOrigin;
    int                        m_gridWidth = 0;
    int                        m_gridHeight = 0;
    std::vector<std::uint32_t> m_chamberStart;
    std::vector<std::uint32_t> m_chamberList;
    std::vector<std::uint32_t> m_pillarStart;
    std::vector<std::uint32_t> m_pillarList;

    // The grid under the graph: open or not, and which neighbour is one step
    // nearer the sub. Rebuilt only when the sub has actually changed cell.
    sf::Vector2f              m_navOrigin;
    int                       m_navWidth = 0;
    int                       m_navHeight = 0;
    std::vector<std::uint8_t> m_navOpen;

    // Where the open water in each cell was found, which in a narrow passage
    // is not the middle of the cell.
    std::vector<sf::Vector2f> m_navPoint;
    std::vector<int>          m_flowFrom;
    std::vector<int>          m_flowQueue;
    int                       m_flowSource = -1;

    sf::Vector2f m_start;
    sf::Vector2f m_hallCentre;
    float        m_hallRadius = 0.0f;
    sf::Vector2f m_boundsMin;
    sf::Vector2f m_boundsMax;

    sf::VertexArray m_rock;   // the shelf and the cut lip, drawn under the floor
    sf::VertexArray m_floor;
    sf::VertexArray m_shade;  // the rim light and the shadow the wall casts inward

    // The same three meshes, parked on the graphics card. A vertex array is
    // system memory, so drawing one hands the whole thing to the driver again
    // on every frame it appears - and the basin is half a million vertices of
    // rock that has not moved since it was cut. Uploaded once, the same
    // triangles cost a call apiece instead of ten megabytes a frame.
    sf::VertexBuffer m_rockCard;
    sf::VertexBuffer m_floorCard;
    sf::VertexBuffer m_shadeCard;
    bool             m_onCard = false;
    sf::VertexArray m_swirl;  // the turning water in a pit, redrawn as it turns
    sf::VertexArray m_lightPool;
    sf::VertexArray m_caustics;
    sf::VertexArray m_gloom;
    sf::VertexArray m_dark;
    sf::VertexArray m_shadow;
    sf::VertexArray m_absorb;
    const DepthTheme* m_theme;
    float             m_lampBonus = 0.0f;
    float             m_lampFactor = 1.0f;

    // Degrees of extra half-angle the sub has bought. A wider beam is the one
    // lamp upgrade that changes how the water feels rather than how far you can
    // see down one line of it.
    float             m_lampSpread = 0.0f;
};
