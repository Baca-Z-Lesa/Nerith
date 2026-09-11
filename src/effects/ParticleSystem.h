#pragma once

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <vector>

// Describes one burst. Most fields are ranges the system randomises inside, so
// a preset is just an aggregate literal with the interesting values named.
struct ParticleSpec
{
    sf::Vector2f position;
    sf::Vector2f direction{ 1.0f, 0.0f };
    float spread = 3.15f;      // half-angle around direction, radians
    float speedMin = 20.0f;
    float speedMax = 60.0f;
    float lifeMin = 0.5f;
    float lifeMax = 1.0f;
    float sizeMin = 2.0f;
    float sizeMax = 4.0f;
    float endSizeScale = 0.4f; // size at death, relative to spawn size
    float drag = 2.0f;         // water resistance
    float buoyancy = 0.0f;     // upward pull; positive rises, negative sinks
    sf::Color color{ 255, 255, 255 };
    int count = 8;
};

// All live particles are packed into one additively blended vertex array, so
// however many are on screen it still costs a single draw call.
class ParticleSystem
{
public:
    explicit ParticleSystem(const sf::Texture& texture);

    void emit(const ParticleSpec& spec);
    void update(float deltaTime);
    void draw(sf::RenderTarget& target) const;

    void clear();

private:
    struct Particle
    {
        sf::Vector2f position;
        sf::Vector2f velocity;
        float life;
        float maxLife;
        float size;
        float endSize;
        float drag;
        float buoyancy;
        sf::Color color;
    };

    void rebuildVertices();

    const sf::Texture*    m_texture;
    std::vector<Particle> m_particles;
    sf::VertexArray       m_vertices;
};

// Game specific presets. They live next to the system so all the tuning is in
// one file instead of sprinkled through Game.
namespace effects
{
    void muzzleBurst(ParticleSystem& particles, sf::Vector2f position, sf::Vector2f direction);
    void hostileMuzzle(ParticleSystem& particles, sf::Vector2f position, sf::Vector2f direction);
    void thrusterBubbles(ParticleSystem& particles, sf::Vector2f position, sf::Vector2f direction);
    void impact(ParticleSystem& particles, sf::Vector2f position, sf::Vector2f direction);
    void enemyDeath(ParticleSystem& particles, sf::Vector2f position);
    void playerHurt(ParticleSystem& particles, sf::Vector2f position);
    void playerDeath(ParticleSystem& particles, sf::Vector2f position);
    void ventBurst(ParticleSystem& particles, sf::Vector2f position, float radius);
    void marineSnow(ParticleSystem& particles, sf::Vector2f position);
    void ambientBubble(ParticleSystem& particles, sf::Vector2f position);
}
