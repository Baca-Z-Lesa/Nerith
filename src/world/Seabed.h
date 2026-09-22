#pragma once

#include "world/DepthTheme.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>
#include <SFML/System/Vector2.hpp>

#include <vector>

struct Assets;
class Arena;

// The ocean floor the fight happens on: sand, scattered rock and coral, a reef
// ring where the arena wall is, swaying kelp and the odd fish drifting past.
// None of it collides with anything. It exists so the arena reads as a place
// instead of a circle, and so the world visibly continues past the fight.
class Seabed
{
public:
    Seabed(const Assets& assets, sf::Vector2f center, float arenaRadius);

    void applyTheme(const DepthTheme& theme);

    // Re-dresses the floor for a newly cut basin. The props are scattered
    // against the map rather than against a radius, so a corridor has a wall
    // along it and a hall has a reef round it.
    void reshape(const Arena& arena);

    void update(float deltaTime);

    // One thing standing on the floor.
    struct Prop
    {
        sf::Sprite sprite;
        sf::Color  baseTint; // before the depth theme is multiplied in
        float baseRotation; // degrees
        float swayAmount;   // degrees; zero for anything rigid
        float swaySpeed;
        float swayPhase;
    };

    // What was actually scattered. Only the depth check in tools reads this:
    // whether one floor looks like another is a question about which textures
    // were handed out, and there is no other way to ask it.
    const std::vector<Prop>& propsForTest() const { return m_props; }

    // The floor and everything standing on it. Drawn under the fight.
    // The rock the whole world is made of, which the basin is then cut out
    // of. It has to go down before the arena draws its floor, or it paints
    // over the basin and the map has no visible edges at all.
    void drawRock(sf::RenderTarget& target) const;

    void draw(sf::RenderTarget& target) const;

    // Three extra layers that are what actually make a flat top-down scene read
    // as having depth:
    //
    //   drawDeep   enormous shapes at the limit of sight, barely moving with
    //              the camera at all, so the basin sits inside something rather
    //              than ending at the far rocks;
    //   drawFar    a band of rock receding past the reef, drawn through a view
    //              that lags the camera, so it slides more slowly than the
    //              floor and therefore reads as further away;
    //   drawNear   silt and the tips of things that are above the sub, drawn
    //              through a view that leads the camera, so they slide faster
    //              and pass over it.
    //
    // All three take the camera centre rather than reading it, because the
    // parallax offset is the whole trick and it belongs in one place.
    void drawDeep(sf::RenderTarget& target, sf::Vector2f cameraCentre) const;
    void drawFar(sf::RenderTarget& target, sf::Vector2f cameraCentre) const;
    void drawNear(sf::RenderTarget& target, sf::Vector2f cameraCentre) const;

private:
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
    void scatterProps(const Arena* arena);
    void buildReef(const Arena* arena);
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

    // The same mesh on the card. Sixteen thousand grains of sand is a hundred
    // thousand vertices, and none of them move: without this they are handed
    // to the driver again on every frame of the dive.
    sf::VertexBuffer     m_sandCard;
    bool                 m_sandOnCard = false;
    const DepthTheme*    m_theme;
    std::vector<Prop>    m_props; // sorted back to front
    std::vector<Critter> m_critters;
    float                m_critterTimer;

    std::vector<Layer> m_deep;
    std::vector<Layer> m_far;
    std::vector<Layer> m_near;

    // One shape reused for every prop's contact shadow. A shadow on the sand
    // under a thing is the cheapest possible cue that the thing is standing on
    // something rather than floating in front of it.
    mutable sf::CircleShape m_shadow;
};
