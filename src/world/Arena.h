#pragma once

#include "world/DepthTheme.h"

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

    // Swapped in when the depth changes; rebuilds the lamp and the darkness.
    void applyTheme(const DepthTheme& theme);

    // The lamp, as the theme sets it, plus whatever was bought in the tree and
    // times whatever the sub is currently running at. Bonus is kept separate
    // from the theme so descending still dims the light - a bigger lamp makes
    // a dark depth survivable rather than making it look like a shallow one -
    // and the factor is what running dark turns down.
    void setLamp(float bonus, float factor);

    void update(float deltaTime);

    // Additive light over the seabed. Drawn before the entities.
    void drawLight(sf::RenderTarget& target, sf::Vector2f lampCentre) const;
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
    const DepthTheme* m_theme;
    float             m_lampBonus = 0.0f;
    float             m_lampFactor = 1.0f;
};
