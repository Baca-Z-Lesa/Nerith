#pragma once

#include "world/DepthTheme.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <vector>

struct Assets;

// The ocean floor the fight happens on: sand, scattered rock and coral, a reef
// ring where the arena wall is, swaying kelp and the odd fish drifting past.
// None of it collides with anything. It exists so the arena reads as a place
// instead of a circle, and so the world visibly continues past the fight.
class Seabed
{
public:
    Seabed(const Assets& assets, sf::Vector2f center, float arenaRadius);

    void applyTheme(const DepthTheme& theme);

    void update(float deltaTime);

    // The floor and everything standing on it. Drawn under the fight.
    void draw(sf::RenderTarget& target) const;

    // Two extra layers that are what actually make a flat top-down scene read
    // as having depth:
    //
    //   drawFar    a band of rock receding past the reef, drawn through a view
    //              that lags the camera, so it slides more slowly than the
    //              floor and therefore reads as further away;
    //   drawNear   silt and the tips of things that are above the sub, drawn
    //              through a view that leads the camera, so they slide faster
    //              and pass over it.
    //
    // Both take the camera centre rather than reading it, because the parallax
    // offset is the whole trick and it belongs in one place.
    void drawFar(sf::RenderTarget& target, sf::Vector2f cameraCentre) const;
    void drawNear(sf::RenderTarget& target, sf::Vector2f cameraCentre) const;

private:
    struct Prop
    {
        sf::Sprite sprite;
        sf::Color  baseTint; // before the depth theme is multiplied in
        float baseRotation; // degrees
        float swayAmount;   // degrees; zero for anything rigid
        float swaySpeed;
        float swayPhase;
    };

    struct Critter
    {
        sf::Sprite   sprite;
        sf::Vector2f position;
        sf::Vector2f velocity;
        float        wobblePhase;
    };

    // Anything that only exists to sell distance: a boulder in the far band or
    // a mote in the near one. They never collide with anything.
    struct Layer
    {
        sf::Sprite   sprite;
        sf::Vector2f anchor;  // where it sits in world space
        float        drift;   // how fast it wanders, for the near layer
        float        phase;
    };

    void buildSand();
    void scatterProps();
    void buildReef();
    void buildParallax();
    void addProp(const sf::Texture& texture, sf::Vector2f position, float scale,
                 sf::Color tint, float swayAmount);
    void spawnCritter();
    void updateCritters(float deltaTime);

    const Assets* m_assets;
    sf::Vector2f  m_center;
    float         m_arenaRadius;
    float         m_time;

    sf::VertexArray      m_sand;  // base gradient plus grain, one draw call
    const DepthTheme*    m_theme;
    std::vector<Prop>    m_props; // sorted back to front
    std::vector<Critter> m_critters;
    float                m_critterTimer;

    std::vector<Layer> m_far;
    std::vector<Layer> m_near;

    // One shape reused for every prop's contact shadow. A shadow on the sand
    // under a thing is the cheapest possible cue that the thing is standing on
    // something rather than floating in front of it.
    mutable sf::CircleShape m_shadow;
};
