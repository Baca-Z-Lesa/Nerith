#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

// The gameplay bounds, plus the lighting that makes them readable: a pool of
// light over the fight, drifting caustics, and the darkness that closes in
// everywhere else. The visible wall itself is the reef, which Seabed draws.
class Arena
{
public:
    Arena(sf::Vector2f center, float radius);

    void update(float deltaTime);

    // Additive light over the seabed. Drawn before the entities.
    void drawLight(sf::RenderTarget& target) const;
    // Darkness swallowing the distance. Drawn after the seabed, before entities.
    void drawGloom(sf::RenderTarget& target) const;

    sf::Vector2f getCenter() const { return m_center; }
    float getRadius() const { return m_radius; }

    // True while any part of a circle at that position is still inside.
    bool overlaps(sf::Vector2f position, float radius) const;

    // Pulls a circle back so it stays fully inside the arena.
    sf::Vector2f clampInside(sf::Vector2f position, float radius) const;

    // Evenly distributed, for scattering things around the basin.
    sf::Vector2f randomPointInside() const;

private:
    void buildLightPool();
    void buildGloom();
    void updateCaustics();

    sf::Vector2f m_center;
    float        m_radius;
    float        m_time;

    sf::VertexArray m_lightPool;
    sf::VertexArray m_caustics;
    sf::VertexArray m_gloom;
};
