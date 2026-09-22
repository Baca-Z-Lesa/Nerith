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
    // drift is the basin's own current, in pixels a second. Everything loose
    // in the water is carried by it - which is the difference between water
    // and a black background with motes painted on.
    void update(float deltaTime, sf::Vector2f drift = { 0.0f, 0.0f });
    void draw(sf::RenderTarget& target) const;

    // Where the beam is, so that silt inside it catches the light.
    //
    // Without this a lamp is a stencil: it reveals what is already there and
    // the water between the sub and the rock stays empty. With it the beam is
    // a shaft - you can see the light itself, in the dirt it is passing
    // through - which is what a lamp underwater actually looks like and the
    // single biggest thing separating water from a black background.
    struct Beam
    {
        sf::Vector2f at{ 0.0f, 0.0f };
        sf::Vector2f aim{ 1.0f, 0.0f };
        float reach = 0.0f;    // zero switches the whole thing off
        float cosHalf = 1.0f;  // cosine of the cone's half angle
        float halo = 0.0f;     // how far the spill round the sub carries
        float strength = 0.0f; // how much brighter a lit mote gets, 0..1
        sf::Color tint{ 255, 255, 255 };
    };

    void setBeam(const Beam& beam) { m_beam = beam; }

    void clear();

    // Only the water check in tools reads these. How far the one live mote
    // has been carried from where it was emitted, and the alpha it ended up
    // with - neither of which can be got at from a screenshot.
    float carriedForTest() const;
    int brightestForTest(bool aheadOfTheBeam) const;

private:
    Beam m_beam;

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
    // coat and size make the debris the colour and bulk of whatever died. The
    // defaults are for the things that are not creatures: nests, pods, hulls.
    void enemyDeath(ParticleSystem& particles, sf::Vector2f position,
                    sf::Color coat = sf::Color(0, 0, 0, 0), float size = 0.0f);
    void playerHurt(ParticleSystem& particles, sf::Vector2f position);
    void playerDeath(ParticleSystem& particles, sf::Vector2f position);
    void ventBurst(ParticleSystem& particles, sf::Vector2f position, float radius);
    void marineSnow(ParticleSystem& particles, sf::Vector2f position);
    void ambientBubble(ParticleSystem& particles, sf::Vector2f position);
}
