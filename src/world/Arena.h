#pragma once

#include "world/DepthTheme.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

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
    void setLamp(float bonus, float factor);

    void update(float deltaTime);

    // The rock, the lip along its edge and the floor inside it. Drawn under
    // everything, including the seabed dressing.
    void drawGround(sf::RenderTarget& target) const;

    // Additive light over the seabed. Drawn before the entities.
    void drawLight(sf::RenderTarget& target, sf::Vector2f lampCentre) const;
    // Darkness swallowing the distance. Drawn after the seabed, before entities.
    void drawGloom(sf::RenderTarget& target) const;

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
    };

    struct Corridor { sf::Vector2f from; sf::Vector2f to; float radius; };

    // A column of rock standing in the water: subtracted from the field, so it
    // stops the sub, the creatures and every shot exactly as the wall does.
    struct Pillar { sf::Vector2f centre; float radius; };

    // The turning water in a pit, as a disc with a pull that dies at its rim.
    struct Current { sf::Vector2f centre; float radius; float strength; };

    const std::vector<Chamber>& getChambers() const { return m_chambers; }
    const std::vector<Corridor>& getCorridors() const { return m_corridors; }
    const std::vector<Pillar>& getPillars() const { return m_pillars; }

    // What the water is doing to anything at that point, in pixels per second.
    // Zero everywhere except inside a pit.
    sf::Vector2f currentAt(sf::Vector2f position) const;

private:
    // Where a chamber's rim actually is in a given direction. Both the field
    // and the drawing go through here, so what is seen and what is collided
    // with can never drift apart.
    float edgeRadius(const Chamber& chamber, sf::Vector2f offset) const;

    void buildWallShading();
    void buildSwirl();
    void buildLightPool();
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
    std::vector<Current>  m_currents;

    sf::Vector2f m_start;
    sf::Vector2f m_hallCentre;
    float        m_hallRadius = 0.0f;
    sf::Vector2f m_boundsMin;
    sf::Vector2f m_boundsMax;

    sf::VertexArray m_rock;   // the shelf and the cut lip, drawn under the floor
    sf::VertexArray m_floor;
    sf::VertexArray m_shade;  // the rim light and the shadow the wall casts inward
    sf::VertexArray m_swirl;  // the turning water in a pit, redrawn as it turns
    sf::VertexArray m_lightPool;
    sf::VertexArray m_caustics;
    sf::VertexArray m_gloom;
    const DepthTheme* m_theme;
    float             m_lampBonus = 0.0f;
    float             m_lampFactor = 1.0f;
};
