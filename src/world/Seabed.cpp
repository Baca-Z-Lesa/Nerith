#include "world/Seabed.h"

#include "core/Assets.h"
#include "world/Arena.h"
#include "core/Config.h"
#include "core/Mesh.h"
#include "core/Random.h"

#include <SFML/System/Angle.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace
{
    // The sand is drawn well past the view so nothing ever shows an edge, even
    // when the window is letterboxed into an odd aspect ratio.
    const float WorldLeft = config::WindowWidth / 2.0f - config::WorldExtent;
    const float WorldTop = config::WindowHeight / 2.0f - config::WorldExtent;
    const float WorldRight = config::WindowWidth / 2.0f + config::WorldExtent;
    const float WorldBottom = config::WindowHeight / 2.0f + config::WorldExtent;

    sf::Color randomTint(sf::Color base, float spread)
    {
        const auto shift = [spread](std::uint8_t channel) {
            const float value = static_cast<float>(channel) * rng::range(1.0f - spread, 1.0f + spread);
            return static_cast<std::uint8_t>(std::clamp(value, 0.0f, 255.0f));
        };
        return sf::Color(shift(base.r), shift(base.g), shift(base.b), base.a);
    }
}

Seabed::Seabed(const Assets& assets, sf::Vector2f center, float arenaRadius)
    : m_assets(&assets)
    , m_center(center)
    , m_arenaRadius(arenaRadius)
    , m_time(0.0f)
    , m_sand(sf::PrimitiveType::Triangles)
    , m_theme(&themeFor(1))
    , m_critterTimer(rng::range(1.0f, 3.0f))
{
    buildSand();
    scatterProps(nullptr);
    buildReef(nullptr);
    buildParallax();

    // Painter's order: whatever sits lower on the screen is nearer, so it goes
    // on top. Sorting once here means draw() never has to think about it.
    std::sort(m_props.begin(), m_props.end(), [](const Prop& a, const Prop& b) {
        return a.sprite.getPosition().y < b.sprite.getPosition().y;
    });
}

void Seabed::buildSand()
{
    m_sand.clear();

    const auto quad = [this](sf::Vector2f topLeft, sf::Vector2f size,
                             sf::Color topColor, sf::Color bottomColor) {
        const sf::Vector2f a{ topLeft.x, topLeft.y };
        const sf::Vector2f b{ topLeft.x + size.x, topLeft.y };
        const sf::Vector2f c{ topLeft.x + size.x, topLeft.y + size.y };
        const sf::Vector2f d{ topLeft.x, topLeft.y + size.y };

        m_sand.append(sf::Vertex{ a, topColor });
        m_sand.append(sf::Vertex{ b, topColor });
        m_sand.append(sf::Vertex{ c, bottomColor });
        m_sand.append(sf::Vertex{ a, topColor });
        m_sand.append(sf::Vertex{ c, bottomColor });
        m_sand.append(sf::Vertex{ d, bottomColor });
    };

    // This is rock, not floor. The basin cuts the floor out of it, so what is
    // painted here is what the player is looking at wherever they cannot go -
    // and it has to be clearly darker than the sand, or the edge of the map
    // does not read at all.
    const sf::Color rockFar = tintBy(m_theme->sandFar, config::RockTint);
    const sf::Color rockNear = tintBy(m_theme->sandNear, config::RockTint);

    quad({ WorldLeft, WorldTop },
         { WorldRight - WorldLeft, WorldBottom - WorldTop },
         rockFar, rockNear);

    // Grain, so the floor is not a flat wash of one colour.
    for (std::size_t i = 0; i < config::SandGrainCount; ++i)
    {
        const sf::Vector2f position{ rng::range(WorldLeft, WorldRight),
                                     rng::range(WorldTop, WorldBottom) };
        const float size = rng::range(1.5f, 4.5f);
        const auto alpha = static_cast<std::uint8_t>(rng::range(10.0f, 46.0f));

        const sf::Color grit(m_theme->light.r * 2, m_theme->light.g * 2, m_theme->light.b * 2, alpha);
        const sf::Color speck = rng::range(0.0f, 1.0f) < 0.5f ? grit : sf::Color(0, 0, 0, alpha);

        quad(position, { size, size }, speck, speck);
    }

    // The floor does not move once it is painted, so it goes to the card
    // rather than across the bus every frame. A hundred thousand vertices of
    // grain is most of a millisecond a frame when it does not.
    m_sandOnCard = mesh::parkOnCard(m_sand, m_sandCard);
}

void Seabed::addProp(const sf::Texture& texture, sf::Vector2f position, float scale,
                     sf::Color tint, float swayAmount)
{
    Prop prop{ sf::Sprite(texture), tint, 0.0f, swayAmount, rng::range(0.35f, 0.8f),
               rng::range(0.0f, 6.28f) };

    const sf::Vector2f size(texture.getSize());

    // Origin at the foot of the sprite: props stand on the seabed, and anything
    // that sways has to pivot where it is rooted.
    prop.sprite.setOrigin({ size.x / 2.0f, size.y });
    prop.sprite.setPosition(position);
    prop.sprite.setScale({ scale, scale });
    // The scattered tint is kept so the depth theme can be reapplied on top of
    // it later without the two multiplying together every time.
    prop.sprite.setColor(tintBy(tint, m_theme->propTint));

    prop.baseRotation = rng::range(-4.0f, 4.0f);
    prop.sprite.setRotation(sf::degrees(prop.baseRotation));

    m_props.push_back(std::move(prop));
}

void Seabed::reshape(const Arena& arena)
{
    // The basin changed shape, so everything that was dressing the old one is
    // in the wrong place. Cheaper and far clearer than trying to move it.
    m_props.clear();
    scatterProps(&arena);
    buildReef(&arena);

    std::sort(m_props.begin(), m_props.end(), [](const Prop& a, const Prop& b) {
        return a.sprite.getPosition().y < b.sprite.getPosition().y;
    });
}

void Seabed::scatterProps(const Arena* arena)
{
    m_props.reserve(config::ScatterPropCount + config::ReefPropCount);

    for (std::size_t i = 0; i < config::ScatterPropCount; ++i)
    {
        // Even area coverage out to the field radius, then skip anything that
        // would land in the middle of the fight.
        const float distance = config::PropFieldRadius * std::sqrt(rng::range(0.0f, 1.0f));
        const sf::Vector2f position =
            m_center + sf::Vector2f(distance, sf::degrees(rng::range(0.0f, 360.0f)));

        // With a cut basin, "inside" is a question about the map rather than
        // about a radius: only flat dressing goes in the water, everything
        // taller belongs in the rock where it cannot hide an incoming shot.
        const bool insideArena = arena != nullptr ? arena->isOpen(position, 10.0f)
                                                  : distance < m_arenaRadius;
        const float roll = rng::range(0.0f, 1.0f);

        if (insideArena)
        {
            // Only small flat things inside, so nothing hides an incoming shot.
            addProp(roll < 0.6f ? m_assets->rockB : m_assets->coralB, position,
                    rng::range(0.18f, 0.36f),
                    randomTint(sf::Color(30, 50, 60), 0.25f), 0.0f);
            continue;
        }

        // Whatever this depth has that the others do not, first: bones, pipe,
        // block, a mat of worms. Rolled before the rock so it is a share of
        // the whole scatter rather than of what is left over, and the share
        // rises with depth - near the surface the floor is mostly floor, and
        // at the bottom most of what is down there is evidence.
        if (roll < m_theme->dressShare)
        {
            const char* pick = nullptr;
            int found = 0;

            for (const char* path : m_theme->dressing)
            {
                if (path == nullptr)
                    continue;

                // Reservoir of one: every entry gets an equal chance without
                // the list having to be counted first.
                ++found;
                if (rng::range(0.0f, static_cast<float>(found)) < 1.0f)
                    pick = path;
            }

            if (pick != nullptr)
            {
                addProp(m_assets->byPath(pick), position, rng::range(0.42f, 0.95f),
                        randomTint(sf::Color(34, 40, 46), 0.22f), 0.0f);
                continue;
            }
        }

        // Everything out here sits outside the light pool, so it is tinted
        // darker than the reef. That contrast is what makes the reef read as
        // the edge of the lit area rather than just more scenery.
        if (roll < 0.34f)
        {
            addProp(rng::range(0.0f, 1.0f) < 0.5f ? m_assets->rockA : m_assets->rockB,
                    position, rng::range(0.35f, 0.95f),
                    randomTint(sf::Color(26, 42, 50), 0.3f), 0.0f);
        }
        else if (roll < 0.62f)
        {
            addProp(rng::range(0.0f, 1.0f) < 0.5f ? m_assets->coralA : m_assets->coralB,
                    position, rng::range(0.3f, 0.7f),
                    randomTint(sf::Color(40, 32, 50), 0.3f), 1.6f);
        }
        else
        {
            addProp(m_assets->kelp, position, rng::range(0.35f, 0.85f),
                    randomTint(sf::Color(28, 50, 40), 0.3f), 5.5f);
        }
    }
}

void Seabed::buildReef(const Arena* arena)
{
    // Rock and coral packed along the wall, wherever the wall now is. The
    // player never sees an abstract boundary, they see something they cannot
    // swim through.
    for (std::size_t i = 0; i < config::ReefPropCount; ++i)
    {
        sf::Vector2f position;

        if (arena != nullptr)
        {
            // Just outside the water: picked at the lip of a chamber and then
            // pushed out until it is actually in rock, so the reef traces
            // whatever shape the basin came out as.
            position = arena->randomEdgePoint(-config::ReefOffset);

            if (arena->isOpen(position, 0.0f))
                continue; // it landed in water after all; the next one will not
        }
        else
        {
            const float angle = 360.0f * static_cast<float>(i)
                                    / static_cast<float>(config::ReefPropCount)
                                + rng::range(-3.0f, 3.0f);
            const float distance = m_arenaRadius + config::ReefOffset + rng::range(-8.0f, 26.0f);
            position = m_center + sf::Vector2f(distance, sf::degrees(angle));
        }

        const float roll = rng::range(0.0f, 1.0f);

        if (roll < 0.45f)
        {
            addProp(rng::range(0.0f, 1.0f) < 0.5f ? m_assets->rockA : m_assets->rockB,
                    position, rng::range(0.75f, 1.3f),
                    randomTint(sf::Color(48, 70, 80), 0.22f), 0.0f);
        }
        else if (roll < 0.78f)
        {
            addProp(rng::range(0.0f, 1.0f) < 0.5f ? m_assets->coralA : m_assets->coralB,
                    position, rng::range(0.55f, 0.95f),
                    randomTint(sf::Color(78, 62, 92), 0.25f), 1.8f);
        }
        else
        {
            addProp(m_assets->kelp, position, rng::range(0.6f, 1.1f),
                    randomTint(sf::Color(50, 86, 66), 0.25f), 6.0f);
        }
    }
}

void Seabed::spawnCritter()
{
    Critter critter{ sf::Sprite(m_assets->fish), {}, {}, rng::range(0.0f, 6.28f) };

    const sf::Vector2f size(m_assets->fish.getSize());
    critter.sprite.setOrigin(size / 2.0f);
    critter.sprite.setScale({ rng::range(0.5f, 1.1f), rng::range(0.5f, 1.1f) });
    critter.sprite.setColor(tintBy(randomTint(sf::Color(46, 74, 84), 0.3f), m_theme->propTint));

    // In from one side, out through the other, always crossing the view.
    const bool fromLeft = rng::range(0.0f, 1.0f) < 0.5f;
    critter.position = { fromLeft ? WorldLeft + 40.0f : WorldRight - 40.0f,
                         rng::range(WorldTop + 100.0f, WorldBottom - 100.0f) };

    const float speed = rng::range(38.0f, 78.0f);
    critter.velocity = { fromLeft ? speed : -speed, rng::range(-14.0f, 14.0f) };

    m_critters.push_back(std::move(critter));
}

void Seabed::updateCritters(float deltaTime)
{
    m_critterTimer -= deltaTime;
    if (m_critterTimer <= 0.0f)
    {
        m_critterTimer = rng::range(config::CritterSpawnIntervalMin, config::CritterSpawnIntervalMax);
        if (m_critters.size() < config::MaxCritters)
            spawnCritter();
    }

    for (Critter& critter : m_critters)
    {
        critter.position += critter.velocity * deltaTime;
        critter.sprite.setPosition(critter.position);

        // A little tail wag, so they do not slide across like cardboard cutouts.
        const float wobble = 7.0f * std::sin(m_time * 5.0f + critter.wobblePhase);
        critter.sprite.setRotation(critter.velocity.angle() + sf::degrees(wobble));
    }

    std::erase_if(m_critters, [](const Critter& critter) {
        return critter.position.x < WorldLeft - 80.0f || critter.position.x > WorldRight + 80.0f;
    });
}

void Seabed::applyTheme(const DepthTheme& theme)
{
    m_theme = &theme;

    // The sand is repainted outright; the props keep the shade they were
    // scattered with and get the depth multiplied over the top of it.
    buildSand();

    for (Prop& prop : m_props)
        prop.sprite.setColor(tintBy(prop.baseTint, m_theme->propTint));

}

void Seabed::update(float deltaTime)
{
    m_time += deltaTime;

    for (Prop& prop : m_props)
    {
        if (prop.swayAmount <= 0.0f)
            continue;

        const float sway = prop.swayAmount * std::sin(m_time * prop.swaySpeed + prop.swayPhase);
        prop.sprite.setRotation(sf::degrees(prop.baseRotation + sway));
    }

    updateCritters(deltaTime);
}

void Seabed::buildParallax()
{
    // The far band: boulders out past the reef, large and dim. They are never
    // reachable and never collide; they exist so the basin has somewhere to be
    // rather than ending at the wall.
    for (std::size_t i = 0; i < config::FarLayerCount; ++i)
    {
        const float angle = 360.0f * static_cast<float>(i) / config::FarLayerCount
                            + rng::range(-14.0f, 14.0f);
        const float distance = rng::range(m_arenaRadius * 1.25f, m_arenaRadius * 2.30f);

        Layer layer{ sf::Sprite(rng::range(0.0f, 1.0f) < 0.5f ? m_assets->rockA
                                                              : m_assets->rockB),
                     m_center + sf::Vector2f(distance, sf::degrees(angle)),
                     0.0f, rng::range(0.0f, 6.28f) };

        const sf::Vector2f size(layer.sprite.getTexture().getSize());
        layer.sprite.setOrigin(size / 2.0f);

        const float scale = rng::range(2.6f, 5.4f);
        layer.sprite.setScale({ scale, scale });
        layer.sprite.setRotation(sf::degrees(rng::range(0.0f, 360.0f)));

        m_far.push_back(std::move(layer));
    }

    // The near band: silt and scraps drifting between the camera and the
    // fight. Few, large and faint, because anything busy up here would fight
    // the thing the player is actually looking at.
    for (std::size_t i = 0; i < config::NearLayerCount; ++i)
    {
        Layer layer{ sf::Sprite(m_assets->particle),
                     m_center + sf::Vector2f(rng::range(-config::WorldExtent, config::WorldExtent),
                                             rng::range(-config::WorldExtent, config::WorldExtent)),
                     rng::range(6.0f, 22.0f), rng::range(0.0f, 6.28f) };

        const sf::Vector2f size(layer.sprite.getTexture().getSize());
        layer.sprite.setOrigin(size / 2.0f);

        const float scale = rng::range(0.5f, 1.9f);
        layer.sprite.setScale({ scale, scale });

        m_near.push_back(std::move(layer));
    }

    m_shadow.setPointCount(18);
}

void Seabed::drawFar(sf::RenderTarget& target, sf::Vector2f cameraCentre) const
{
    // Lagging the camera is the whole trick: the further behind a layer slides,
    // the further away the eye puts it.
    const sf::Vector2f lag = (cameraCentre - m_center) * (1.0f - config::FarParallax);

    for (const Layer& layer : m_far)
    {
        sf::Sprite sprite = layer.sprite;
        sprite.setPosition(layer.anchor + lag);

        sf::Color tint = tintBy(config::FarLayerTint, m_theme->propTint);
        tint.a = config::FarLayerAlpha;
        sprite.setColor(tint);

        target.draw(sprite);
    }
}

void Seabed::drawNear(sf::RenderTarget& target, sf::Vector2f cameraCentre) const
{
    const sf::Vector2f lead = (cameraCentre - m_center) * (config::NearParallax - 1.0f);

    for (const Layer& layer : m_near)
    {
        sf::Sprite sprite = layer.sprite;

        // A slow wander on top of the parallax, so the near layer is water
        // rather than a pane of glass with specks painted on it.
        const sf::Vector2f wander{ std::cos(m_time * 0.23f + layer.phase) * layer.drift,
                                   std::sin(m_time * 0.17f + layer.phase) * layer.drift };

        sprite.setPosition(layer.anchor + lead + wander);
        sprite.setColor(config::NearLayerTint);
        target.draw(sprite);
    }
}

void Seabed::drawRock(sf::RenderTarget& target) const
{
    if (m_sandOnCard)
        target.draw(m_sandCard);
    else
        target.draw(m_sand);
}

namespace
{
    // The dressing is scattered over a five-thousand pixel field and the window
    // looks at a fraction of it at a time. Every prop is a sprite and every
    // sprite is a draw call, so without this the great majority of a frame's
    // calls go on painting things nobody can see.
    bool onScreen(const sf::RenderTarget& target, sf::Vector2f at)
    {
        const sf::View& view = target.getView();
        const sf::Vector2f eye = view.getCenter();
        const sf::Vector2f half = view.getSize() / 2.0f;

        return std::abs(at.x - eye.x) <= half.x + config::PropCullMargin
               && std::abs(at.y - eye.y) <= half.y + config::PropCullMargin;
    }
}

void Seabed::draw(sf::RenderTarget& target) const
{
    // A contact shadow under every prop, offset the way the key light falls.
    // Nothing else in the scene says "this is standing on the floor" as
    // cheaply, and without it the props look pasted on.
    for (const Prop& prop : m_props)
    {
        const sf::Vector2f at = prop.sprite.getPosition();
        if (!onScreen(target, at))
            continue;

        const float radius = prop.sprite.getScale().x
                             * static_cast<float>(prop.sprite.getTexture().getSize().x) * 0.30f;

        m_shadow.setRadius(radius);
        m_shadow.setOrigin({ radius, radius });
        m_shadow.setScale({ 1.0f, config::PropShadowSquash });
        m_shadow.setPosition(at + config::PropShadowOffset);
        m_shadow.setFillColor(config::PropShadowColor);
        target.draw(m_shadow);
    }

    for (const Prop& prop : m_props)
    {
        if (onScreen(target, prop.sprite.getPosition()))
            target.draw(prop.sprite);
    }

    for (const Critter& critter : m_critters)
    {
        if (onScreen(target, critter.sprite.getPosition()))
            target.draw(critter.sprite);
    }
}
