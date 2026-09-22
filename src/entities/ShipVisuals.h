#pragma once

#include "upgrades/Stats.h"

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
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

    // Re-places the hardware for drawing, without advancing anything. The sub
    // is drawn between two simulation steps, and its guns have to be in the
    // same place as its hull. The scale is over and above the fitting scale, so
    // the deck can show the same sub blown up to hero size without a second set
    // of offsets.
    void present(sf::Vector2f position, sf::Angle heading, float scale = 1.0f);

    // Split so the hull can be drawn between them: slung hardware underneath,
    // plating and guns on top.
    void drawUnder(sf::RenderTarget& target) const;
    void drawOver(sf::RenderTarget& target) const;

    void setShieldUp(bool up) { m_shieldUp = up; }

    // A tint laid over every fitted part, so the hardware sinks into the night
    // deck the same way the hull does. White (the default) leaves the parts as
    // they are, which is what the arena wants.
    void setTint(sf::Color tint) { m_tint = tint; }

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
    sf::Color          m_tint{ sf::Color::White };
};
