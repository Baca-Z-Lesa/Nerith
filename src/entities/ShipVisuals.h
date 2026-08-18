#pragma once

#include "upgrades/Stats.h"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <vector>

struct Assets;

// The hardware the sub visibly grows as it is upgraded. It reads nothing but
// the fitted parts on the stat block, so a node that bolts something onto the
// hull is a data change and never a change in here.
class ShipVisuals
{
public:
    explicit ShipVisuals(const Assets& assets);

    void update(sf::Vector2f position, sf::Angle heading, const Stats& stats, float deltaTime);

    // Split so the hull can be drawn between them: slung hardware underneath,
    // plating and guns on top.
    void drawUnder(sf::RenderTarget& target) const;
    void drawOver(sf::RenderTarget& target) const;

    void setShieldUp(bool up) { m_shieldUp = up; }

private:
    struct Mount
    {
        ShipPart     part;
        sf::Vector2f offset; // sub-local: x forward, y starboard
        bool         under;  // drawn beneath the hull
        sf::Sprite   sprite;
        bool         fitted;
    };

    std::vector<Mount> m_mounts;
    sf::CircleShape    m_shield;
    bool               m_shieldUp;
    float              m_time;
};
