#pragma once

#include "upgrades/UpgradeTree.h"

#include <SFML/Graphics/CircleShape.hpp>
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

    // WASD drags the view across the roots; the arrow keys walk from node to
    // node instead, and the view follows whatever they land on.
    void update(float deltaTime);
    void recentre();

    // One step along the roots in that direction, from whatever is picked now.
    // Picks the hull first if nothing is.
    void moveFocus(sf::Vector2f direction, const UpgradeTree& tree);

    // Mouse wheel, or the plus and minus keys.
    void zoomBy(float steps);

    // The hub also picks which level to dive into, so the strip lives here.
    void setLevels(int selected, int unlocked);
    std::optional<int> getHoveredLevel() const { return m_hoveredLevel; }

    void setCursor(sf::Vector2f worldPosition, const UpgradeTree& tree);

    // Forgets what was under the cursor, so a hover from the last time the
    // screen was open cannot be clicked on the frame it reopens.
    void clearCursor() { m_hovered.reset(); }

    std::optional<std::size_t> getHovered() const { return m_hovered; }

    void draw(sf::RenderTarget& target, const UpgradeTree& tree, int salvage, int depth,
              const std::string& prompt);

private:
    void focusOn(std::size_t index, const UpgradeTree& tree);

    sf::Vector2f centreOf(const UpgradeNode& node) const;
    sf::FloatRect boundsOf(const UpgradeNode& node) const;
    float nodeRadius() const;
    void drawNodeCard(sf::RenderTarget& target, const UpgradeTree& tree,
                      const UpgradeNode& node, int salvage);

    void drawLinks(sf::RenderTarget& target, const UpgradeTree& tree);
    void drawBranchLabels(sf::RenderTarget& target, const UpgradeTree& tree);
    void drawNodes(sf::RenderTarget& target, const UpgradeTree& tree, int salvage);
    void drawLevelStrip(sf::RenderTarget& target);
    sf::FloatRect levelBounds(int level) const;
    void drawChrome(sf::RenderTarget& target, const UpgradeTree& tree, int salvage, int depth,
                    const std::string& prompt);

    std::optional<std::size_t> m_hovered;
    std::optional<int>         m_hoveredLevel;

    // 0 when the card is shut, 1 when it is fully unrolled. Reset the moment
    // the cursor moves to a different node, so each one opens for itself.
    float                      m_openness = 0.0f;
    int                        m_selectedLevel = 1;
    int                        m_unlockedLevel = 1;
    sf::Vector2f               m_pan;
    float                      m_zoom = 1.0f;

    // Where the view is being pulled while the keyboard is driving, and the
    // last place the mouse was seen. A cursor sitting still must not keep
    // taking the pick back off the arrow keys, so its position is remembered
    // and only a real move counts.
    sf::Vector2f               m_panTarget;
    sf::Vector2f               m_mouse;
    bool                       m_keyboard = false;

    sf::RectangleShape m_backdrop;
    sf::RectangleShape m_box;
    sf::CircleShape    m_node;
    sf::VertexArray    m_lines;
    sf::VertexArray    m_text;
    sf::VertexArray    m_icons;
};
