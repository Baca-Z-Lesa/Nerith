#include "core/Assets.h"

#include "core/Config.h"
#include "entities/BossDef.h"

#include <stdexcept>
#include <string>

namespace
{
    void load(sf::Texture& texture, const char* path)
    {
        if (!texture.loadFromFile(path))
            throw std::runtime_error(std::string("failed to load texture: ") + path);
        texture.setSmooth(true);
    }
}

const char* Assets::texturePathFor(ShipPart part)
{
    switch (part)
    {
        case ShipPart::HeavyCannon:  return "assets/textures/att_cannon.png";
        case ShipPart::TwinCannon:   return "assets/textures/att_twin.png";
        case ShipPart::TorpedoTube:  return "assets/textures/att_torpedo.png";
        case ShipPart::SonarDish:    return "assets/textures/att_sonar.png";
        case ShipPart::ArmourPlates: return "assets/textures/att_armour.png";
        case ShipPart::BigEngine:    return "assets/textures/att_engine.png";
        case ShipPart::Drone:        return "assets/textures/att_drone.png";
        case ShipPart::ShieldBubble: return nullptr; // drawn as a ring, not a sprite
        default:                     return nullptr;
    }
}

Assets::Assets()
{
    load(player, config::PlayerTexturePath);
    load(waterJet, config::WaterJetTexturePath);
    load(enemyOrb, config::EnemyOrbTexturePath);
    load(particle, config::ParticleTexturePath);
    load(torpedo, config::TorpedoTexturePath);
    load(mine, config::MineTexturePath);

    load(rockA, "assets/textures/rock_a.png");
    load(rockB, "assets/textures/rock_b.png");
    load(coralA, "assets/textures/coral_a.png");
    load(coralB, "assets/textures/coral_b.png");
    load(kelp, "assets/textures/kelp.png");
    load(fish, "assets/textures/fish.png");

    // The creature textures come from the same table that defines their stats,
    // so a new enemy type never needs a line here.
    for (std::size_t i = 0; i < enemies.size(); ++i)
        load(enemies[i], config::EnemyTable[i].texturePath);

    for (std::size_t i = 0; i < attachments.size(); ++i)
    {
        if (const char* path = texturePathFor(static_cast<ShipPart>(i)))
            load(attachments[i], path);
    }

    // Walk every boss definition and pull in whatever it names. Duplicates
    // collapse, so shared sprites are only loaded once.
    const auto want = [this](const char* path) {
        if (path == nullptr || m_bossTextures.count(path) != 0)
            return;
        sf::Texture texture;
        load(texture, path);
        m_bossTextures.emplace(path, std::move(texture));
    };

    for (int i = 0; i < static_cast<int>(BossId::Count); ++i)
    {
        const BossDef& def = bossDefFor(static_cast<BossId>(i));
        want(def.headTexture);
        want(def.shotTexture);
        want(def.segmentTexture);
        for (std::size_t p = 0; p < def.partCount; ++p)
            want(def.parts[p].texture);
    }

    // And whatever each creature throws, through the same cache, so a new
    // creature with a new shot needs no line in here either.
    for (std::size_t i = 0; i < EnemyTypeCount; ++i)
        want(config::EnemyTable[i].shotPath);
}

const sf::Texture& Assets::shotFor(EnemyType type) const
{
    const char* path = config::enemyStats(type).shotPath;
    return path != nullptr ? boss(path) : enemyOrb;
}

const sf::Texture& Assets::boss(const char* path) const
{
    const auto found = m_bossTextures.find(path);
    if (found == m_bossTextures.end())
        throw std::runtime_error(std::string("boss texture not preloaded: ") + path);

    return found->second;
}
