#include "effects/ParticleSystem.h"

#include "core/Config.h"
#include "core/Random.h"

#include <SFML/Graphics/BlendMode.hpp>
#include <SFML/Graphics/RenderStates.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

ParticleSystem::ParticleSystem(const sf::Texture& texture)
    : m_texture(&texture)
    , m_vertices(sf::PrimitiveType::Triangles)
{
    m_particles.reserve(config::MaxParticles);
}

void ParticleSystem::emit(const ParticleSpec& spec)
{
    const float baseAngle = std::atan2(spec.direction.y, spec.direction.x);

    for (int i = 0; i < spec.count; ++i)
    {
        // A full pool silently drops new particles: dropping a few sparks is
        // always better than stalling the frame.
        if (m_particles.size() >= config::MaxParticles)
            return;

        const float angle = baseAngle + rng::range(-spec.spread, spec.spread);
        const float speed = rng::range(spec.speedMin, spec.speedMax);
        const float size = rng::range(spec.sizeMin, spec.sizeMax);
        const float life = rng::range(spec.lifeMin, spec.lifeMax);

        Particle particle;
        particle.position = spec.position;
        particle.velocity = { std::cos(angle) * speed, std::sin(angle) * speed };
        particle.life = life;
        particle.maxLife = life;
        particle.size = size;
        particle.endSize = size * spec.endSizeScale;
        particle.drag = spec.drag;
        particle.buoyancy = spec.buoyancy;
        particle.color = spec.color;

        m_particles.push_back(particle);
    }
}

void ParticleSystem::update(float deltaTime, sf::Vector2f drift)
{
    for (Particle& particle : m_particles)
    {
        particle.life -= deltaTime;
        particle.velocity -= particle.velocity * particle.drag * deltaTime; // water resists
        particle.velocity.y -= particle.buoyancy * deltaTime;               // bubbles rise

        // And the current. Added to the position rather than to the velocity:
        // a mote is not accelerated by the water, it is IN the water, and
        // integrating it into velocity would have the drag term fight it and
        // every fleck end up at a slightly different speed.
        particle.position += (particle.velocity + drift) * deltaTime;
    }

    std::erase_if(m_particles, [](const Particle& particle) { return particle.life <= 0.0f; });

    rebuildVertices();
}

void ParticleSystem::rebuildVertices()
{
    m_vertices.clear();

    const sf::Vector2f textureSize(m_texture->getSize());

    for (const Particle& particle : m_particles)
    {
        const float remaining = particle.life / particle.maxLife; // 1 at spawn, 0 at death

        // Fade out over the whole life but ramp in quickly, otherwise ambient
        // motes pop into existence in the middle of open water.
        const float fade = std::min(1.0f, (1.0f - remaining) * 6.0f) * remaining;

        sf::Color color = particle.color;
        float alpha = static_cast<float>(particle.color.a) * fade;

        // Is this mote in the light? A dot product and a length, per mote, and
        // what it buys is the beam becoming a thing you can see rather than a
        // hole in the dark that things are visible through.
        if (m_beam.reach > 0.0f && m_beam.strength > 0.0f)
        {
            const sf::Vector2f off = particle.position - m_beam.at;
            const float away = off.length();

            if (away < m_beam.reach && away > 0.5f)
            {
                const float facing = off.dot(m_beam.aim) / away;

                // Full inside the cone, falling off over the last of it, and a
                // weaker share everywhere inside the halo - the same shape the
                // lamp itself is built with, so the two agree.
                const float inCone =
                    std::clamp((facing - m_beam.cosHalf) / std::max(0.02f, 1.0f - m_beam.cosHalf),
                               0.0f, 1.0f);

                const float inHalo = away < m_beam.halo
                                         ? 0.45f * (1.0f - away / std::max(1.0f, m_beam.halo))
                                         : 0.0f;

                // And dimmer the further down the shaft it is, because that is
                // what the water is doing to the light.
                const float carry = 1.0f - away / m_beam.reach;
                const float lit = std::clamp(std::max(inCone * carry, inHalo), 0.0f, 1.0f)
                                  * m_beam.strength;

                if (lit > 0.001f)
                {
                    const auto mix = [lit](std::uint8_t from, std::uint8_t to) {
                        return static_cast<std::uint8_t>(
                            std::clamp(static_cast<float>(from)
                                           + (static_cast<float>(to) - from) * lit,
                                       0.0f, 255.0f));
                    };

                    color.r = mix(color.r, m_beam.tint.r);
                    color.g = mix(color.g, m_beam.tint.g);
                    color.b = mix(color.b, m_beam.tint.b);

                    alpha = std::min(255.0f, alpha * (1.0f + 3.2f * lit));
                }
            }
        }

        color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 255.0f));

        const float half = particle.endSize + (particle.size - particle.endSize) * remaining;

        const float left = particle.position.x - half;
        const float right = particle.position.x + half;
        const float top = particle.position.y - half;
        const float bottom = particle.position.y + half;

        const sf::Vertex topLeft{ { left, top }, color, { 0.0f, 0.0f } };
        const sf::Vertex topRight{ { right, top }, color, { textureSize.x, 0.0f } };
        const sf::Vertex bottomRight{ { right, bottom }, color, { textureSize.x, textureSize.y } };
        const sf::Vertex bottomLeft{ { left, bottom }, color, { 0.0f, textureSize.y } };

        m_vertices.append(topLeft);
        m_vertices.append(topRight);
        m_vertices.append(bottomRight);

        m_vertices.append(topLeft);
        m_vertices.append(bottomRight);
        m_vertices.append(bottomLeft);
    }
}

void ParticleSystem::draw(sf::RenderTarget& target) const
{
    if (m_vertices.getVertexCount() == 0)
        return;

    sf::RenderStates states;
    states.texture = m_texture;
    states.blendMode = sf::BlendAdd; // light adds up under water, it does not paint over

    target.draw(m_vertices, states);
}

void ParticleSystem::clear()
{
    m_particles.clear();
    m_vertices.clear();
}

// ---------------------------------------------------------------------------
//  Presets
// ---------------------------------------------------------------------------

void effects::muzzleBurst(ParticleSystem& particles, sf::Vector2f position, sf::Vector2f direction)
{
    particles.emit({ .position = position,
                     .direction = direction,
                     .spread = 0.45f,
                     .speedMin = 60.0f, .speedMax = 210.0f,
                     .lifeMin = 0.12f, .lifeMax = 0.32f,
                     .sizeMin = 2.0f, .sizeMax = 5.0f,
                     .endSizeScale = 0.15f,
                     .drag = 6.0f,
                     .color = { 175, 240, 236, 190 },
                     .count = 5 });
}

void effects::hostileMuzzle(ParticleSystem& particles, sf::Vector2f position, sf::Vector2f direction)
{
    // Violet, matching the orb, so a shot fired at you never reads as one of
    // your own even in a crowded frame.
    particles.emit({ .position = position,
                     .direction = direction,
                     .spread = 0.6f,
                     .speedMin = 40.0f, .speedMax = 150.0f,
                     .lifeMin = 0.15f, .lifeMax = 0.4f,
                     .sizeMin = 2.0f, .sizeMax = 5.5f,
                     .endSizeScale = 0.2f,
                     .drag = 5.0f,
                     .color = { 226, 130, 255, 190 },
                     .count = 6 });
}

void effects::thrusterBubbles(ParticleSystem& particles, sf::Vector2f position, sf::Vector2f direction)
{
    particles.emit({ .position = position,
                     .direction = direction,
                     .spread = 0.7f,
                     .speedMin = 15.0f, .speedMax = 55.0f,
                     .lifeMin = 0.6f, .lifeMax = 1.4f,
                     .sizeMin = 1.5f, .sizeMax = 3.5f,
                     .endSizeScale = 1.3f, // bubbles swell as the pressure drops
                     .drag = 1.6f,
                     .buoyancy = 22.0f,
                     .color = { 130, 200, 205, 110 },
                     .count = 2 });
}

void effects::impact(ParticleSystem& particles, sf::Vector2f position, sf::Vector2f direction)
{
    // Spray back along the jet, the way water does off a hard surface.
    particles.emit({ .position = position,
                     .direction = -direction,
                     .spread = 1.2f,
                     .speedMin = 70.0f, .speedMax = 260.0f,
                     .lifeMin = 0.18f, .lifeMax = 0.5f,
                     .sizeMin = 2.0f, .sizeMax = 6.0f,
                     .endSizeScale = 0.2f,
                     .drag = 5.0f,
                     .color = { 200, 250, 248, 220 },
                     .count = 12 });
}

void effects::ventBurst(ParticleSystem& particles, sf::Vector2f position, float radius)
{
    // Scalding water goes up and out fast, then the mineral cloud it carried
    // hangs there and drifts, which is what makes the vent read as hot rather
    // than as an explosion.
    particles.emit({ .position = position,
                     .spread = 3.15f,
                     .speedMin = radius * 0.9f, .speedMax = radius * 2.4f,
                     .lifeMin = 0.35f, .lifeMax = 0.95f,
                     .sizeMin = 4.0f, .sizeMax = 12.0f,
                     .endSizeScale = 0.3f,
                     .drag = 2.4f,
                     .color = { 255, 196, 120, 220 },
                     .count = 34 });

    particles.emit({ .position = position,
                     .spread = 3.15f,
                     .speedMin = 10.0f, .speedMax = radius * 0.8f,
                     .lifeMin = 1.1f, .lifeMax = 2.4f,
                     .sizeMin = 6.0f, .sizeMax = 18.0f,
                     .endSizeScale = 2.0f,
                     .drag = 1.4f,
                     .color = { 170, 120, 96, 120 },
                     .count = 26 });
}

void effects::enemyDeath(ParticleSystem& particles, sf::Vector2f position)
{
    // Two layers: a warm flash from the eye, then a cloud of ink and bubbles.
    particles.emit({ .position = position,
                     .spread = 3.15f,
                     .speedMin = 40.0f, .speedMax = 240.0f,
                     .lifeMin = 0.3f, .lifeMax = 0.8f,
                     .sizeMin = 3.0f, .sizeMax = 8.0f,
                     .endSizeScale = 0.2f,
                     .drag = 3.5f,
                     .color = { 255, 140, 80, 200 },
                     .count = 18 });

    particles.emit({ .position = position,
                     .spread = 3.15f,
                     .speedMin = 10.0f, .speedMax = 90.0f,
                     .lifeMin = 0.8f, .lifeMax = 1.8f,
                     .sizeMin = 4.0f, .sizeMax = 11.0f,
                     .endSizeScale = 1.6f,
                     .drag = 2.2f,
                     .buoyancy = 12.0f,
                     .color = { 120, 60, 110, 90 },
                     .count = 14 });
}

void effects::playerHurt(ParticleSystem& particles, sf::Vector2f position)
{
    particles.emit({ .position = position,
                     .spread = 3.15f,
                     .speedMin = 60.0f, .speedMax = 200.0f,
                     .lifeMin = 0.25f, .lifeMax = 0.6f,
                     .sizeMin = 2.5f, .sizeMax = 6.0f,
                     .endSizeScale = 0.25f,
                     .drag = 4.0f,
                     .color = { 255, 110, 90, 210 },
                     .count = 14 });
}

void effects::playerDeath(ParticleSystem& particles, sf::Vector2f position)
{
    particles.emit({ .position = position,
                     .spread = 3.15f,
                     .speedMin = 60.0f, .speedMax = 330.0f,
                     .lifeMin = 0.5f, .lifeMax = 1.5f,
                     .sizeMin = 3.0f, .sizeMax = 10.0f,
                     .endSizeScale = 0.2f,
                     .drag = 2.6f,
                     .color = { 255, 170, 120, 220 },
                     .count = 40 });

    particles.emit({ .position = position,
                     .spread = 3.15f,
                     .speedMin = 20.0f, .speedMax = 120.0f,
                     .lifeMin = 1.2f, .lifeMax = 2.6f,
                     .sizeMin = 4.0f, .sizeMax = 14.0f,
                     .endSizeScale = 1.8f,
                     .drag = 1.8f,
                     .buoyancy = 26.0f,
                     .color = { 150, 210, 215, 110 },
                     .count = 30 });
}

void effects::marineSnow(ParticleSystem& particles, sf::Vector2f position)
{
    // Barely moving flecks of organic debris. The backbone of the ambience.
    particles.emit({ .position = position,
                     .spread = 3.15f,
                     .speedMin = 2.0f, .speedMax = 12.0f,
                     .lifeMin = 5.0f, .lifeMax = 11.0f,
                     .sizeMin = 0.8f, .sizeMax = 2.4f,
                     .endSizeScale = 1.0f,
                     .drag = 0.2f,
                     .buoyancy = -3.0f, // slowly settling towards the seabed
                     .color = { 150, 195, 205, 70 },
                     .count = 1 });
}

void effects::ambientBubble(ParticleSystem& particles, sf::Vector2f position)
{
    particles.emit({ .position = position,
                     .spread = 3.15f,
                     .speedMin = 3.0f, .speedMax = 14.0f,
                     .lifeMin = 3.0f, .lifeMax = 6.5f,
                     .sizeMin = 1.5f, .sizeMax = 4.0f,
                     .endSizeScale = 1.5f,
                     .drag = 0.6f,
                     .buoyancy = 18.0f,
                     .color = { 130, 205, 210, 80 },
                     .count = 1 });
}

float ParticleSystem::carriedForTest() const
{
    float furthest = 0.0f;
    for (const Particle& particle : m_particles)
        furthest = std::max(furthest, particle.position.length());

    return furthest;
}

int ParticleSystem::brightestForTest(bool aheadOfTheBeam) const
{
    // Read off the built vertices rather than off the particles, because what
    // the question is about is the colour that actually reaches the screen.
    int best = 0;

    for (std::size_t i = 0; i < m_vertices.getVertexCount(); ++i)
    {
        const sf::Vertex& vertex = m_vertices[i];
        const bool ahead = (vertex.position - m_beam.at).dot(m_beam.aim) > 0.0f;

        if (ahead == aheadOfTheBeam)
            best = std::max(best, static_cast<int>(vertex.color.a));
    }

    return best;
}
