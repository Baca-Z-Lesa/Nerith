#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
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

    void update(float deltaTime);
    void draw(sf::RenderTarget& target) const;

private:
    struct Prop
    {
        sf::Sprite sprite;
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

    void buildSand();
    void scatterProps();
    void buildReef();
    void addProp(const sf::Texture& texture, sf::Vector2f position, float scale,
                 sf::Color tint, float swayAmount);
    void spawnCritter();
    void updateCritters(float deltaTime);

    const Assets* m_assets;
    sf::Vector2f  m_center;
    float         m_arenaRadius;
    float         m_time;

    sf::VertexArray      m_sand;  // base gradient plus grain, one draw call
    std::vector<Prop>    m_props; // sorted back to front
    std::vector<Critter> m_critters;
    float                m_critterTimer;
};
