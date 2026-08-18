#pragma once

#include "upgrades/UpgradeTree.h"

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <optional>
#include <string>

// Draws the upgrade tree and works out what the cursor is over. It owns no game
// state: the tree, the salvage and how deep the run has reached are handed in,
// and unlocking is left to the caller so the screen stays a pure view.
class UpgradeScreen
{
public:
    UpgradeScreen();

    // Arrow keys or WASD drag the view across the roots; nothing else fits.
    void update(float deltaTime);
    void recentre() { m_pan = { 0.0f, 0.0f }; }

    void setCursor(sf::Vector2f worldPosition, const UpgradeTree& tree);

    // Forgets what was under the cursor, so a hover from the last time the
    // screen was open cannot be clicked on the frame it reopens.
    void clearCursor() { m_hovered.reset(); }

    std::optional<std::size_t> getHovered() const { return m_hovered; }

    void draw(sf::RenderTarget& target, const UpgradeTree& tree, int salvage, int depth,
              const std::string& prompt);

private:
    sf::Vector2f centreOf(const UpgradeNode& node) const;
    sf::FloatRect boundsOf(const UpgradeNode& node) const;

    void drawLinks(sf::RenderTarget& target, const UpgradeTree& tree);
    void drawNodes(sf::RenderTarget& target, const UpgradeTree& tree, int salvage, int depth);
    void drawChrome(sf::RenderTarget& target, const UpgradeTree& tree, int salvage, int depth,
                    const std::string& prompt);

    std::optional<std::size_t> m_hovered;
    sf::Vector2f               m_pan;

    sf::RectangleShape m_backdrop;
    sf::RectangleShape m_box;
    sf::VertexArray    m_lines;
    sf::VertexArray    m_text;
};
