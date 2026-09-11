#pragma once

#include "entities/EnemyType.h"
#include "upgrades/Stats.h"

#include <SFML/Graphics/Texture.hpp>

#include <array>
#include <map>
#include <string>

// Every texture the game needs, loaded once. Sprites only borrow their texture,
// so whoever owns this must outlive everything that is drawn.
struct Assets
{
    Assets(); // throws if a file is missing

    sf::Texture player;
    sf::Texture waterJet;
    sf::Texture enemyOrb;
    sf::Texture particle;
    sf::Texture torpedo;
    sf::Texture mine;

    // Indexed by EnemyType, so adding a creature is a row in the config table
    // and nothing here changes.
    std::array<sf::Texture, EnemyTypeCount> enemies;

    // Hardware bolted onto the sub. Some parts are drawn with shapes rather
    // than a sprite, and those entries stay empty.
    std::array<sf::Texture, static_cast<std::size_t>(ShipPart::Count)> attachments;

    // Seabed dressing. Drawn in pale tones on purpose so each instance can be
    // tinted to a different shade of the same rock or coral.
    sf::Texture rockA;
    sf::Texture rockB;
    sf::Texture coralA;
    sf::Texture coralB;
    sf::Texture kelp;
    sf::Texture fish;

    const sf::Texture& forEnemy(EnemyType type) const
    {
        return enemies[static_cast<std::size_t>(type)];
    }

    const sf::Texture& forPart(ShipPart part) const
    {
        return attachments[static_cast<std::size_t>(part)];
    }

    // Anything named by a path in one of the data tables: boss bodies, boss
    // parts, and the shot every creature fires. A new creature is a new row
    // there and needs no line in here.
    const sf::Texture& boss(const char* path) const;

    // The shot this creature throws. Every kind that fires has its own, so the
    // water tells you what is shooting at you before you find what fired it.
    const sf::Texture& shotFor(EnemyType type) const;

    // nullptr for hardware that is drawn with shapes instead of a sprite.
    static const char* texturePathFor(ShipPart part);

private:
    std::map<std::string, sf::Texture> m_bossTextures;
};
