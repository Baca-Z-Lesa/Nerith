#include "ui/UpgradeScreen.h"

#include "core/Config.h"
#include "ui/PixelFont.h"
#include "world/LevelDef.h"

#include <SFML/Window/Keyboard.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
    // A closed node shows its initials, so "PRESSURE VALVE" reads as PV.
    std::string initialsOf(const std::string& name)
    {
        std::string icon;
        bool boundary = true;

        for (const char c : name)
        {
            if (c == ' ')
            {
                boundary = true;
                continue;
            }

            if (boundary && icon.size() < 3)
                icon += c;

            boundary = false;
        }

        return icon;
    }

    void appendLine(sf::VertexArray& vertices, sf::Vector2f from, sf::Vector2f to,
                    float thickness, sf::Color color)
    {
        const sf::Vector2f delta = to - from;
        if (delta.lengthSquared() <= 0.0f)
            return;

        const sf::Vector2f direction = delta.normalized();
        const sf::Vector2f normal{ -direction.y * thickness / 2.0f, direction.x * thickness / 2.0f };

        vertices.append(sf::Vertex{ from - normal, color });
        vertices.append(sf::Vertex{ from + normal, color });
        vertices.append(sf::Vertex{ to + normal, color });
        vertices.append(sf::Vertex{ from - normal, color });
        vertices.append(sf::Vertex{ to + normal, color });
        vertices.append(sf::Vertex{ to - normal, color });
    }
}

UpgradeScreen::UpgradeScreen()
    : m_pan(0.0f, 0.0f)
    , m_lines(sf::PrimitiveType::Triangles)
    , m_text(sf::PrimitiveType::Triangles)
{
    m_backdrop.setSize({ static_cast<float>(config::WindowWidth),
                         static_cast<float>(config::WindowHeight) });
    m_backdrop.setFillColor(config::ScreenBackdrop);

    m_box.setSize({ config::NodeWidth, config::NodeHeight });
    m_box.setOutlineThickness(2.0f);

    m_node.setPointCount(28);
    m_node.setOutlineThickness(2.5f);
}

sf::Vector2f UpgradeScreen::centreOf(const UpgradeNode& node) const
{
    // Panning happens in tree space and is then scaled with everything else,
    // so zooming keeps whatever the player had centred, centred.
    return config::TreeOrigin + (node.position + m_pan) * m_zoom;
}

void UpgradeScreen::zoomBy(float steps)
{
    m_zoom = std::clamp(m_zoom * std::pow(config::TreeZoomStep, steps),
                        config::TreeZoomMin, config::TreeZoomMax);
}

float UpgradeScreen::nodeRadius() const
{
    return config::NodeRadius * m_zoom;
}

sf::FloatRect UpgradeScreen::boundsOf(const UpgradeNode& node) const
{
    // Nodes are discs now, so this is only their bounding square.
    const sf::Vector2f centre = centreOf(node);
    const sf::Vector2f size{ config::NodeRadius * 2.0f, config::NodeRadius * 2.0f };
    return sf::FloatRect(centre - size / 2.0f, size);
}

void UpgradeScreen::update(float deltaTime)
{
    // The roots reach roughly 1100px out from the hull, so the whole tree can
    // never be on screen at once and the view has to be drivable.
    sf::Vector2f move{ 0.0f, 0.0f };

    const auto held = [](sf::Keyboard::Scancode a, sf::Keyboard::Scancode b) {
        return sf::Keyboard::isKeyPressed(a) || sf::Keyboard::isKeyPressed(b);
    };

    if (held(sf::Keyboard::Scancode::Left, sf::Keyboard::Scancode::A))  move.x += 1.0f;
    if (held(sf::Keyboard::Scancode::Right, sf::Keyboard::Scancode::D)) move.x -= 1.0f;
    if (held(sf::Keyboard::Scancode::Up, sf::Keyboard::Scancode::W))    move.y += 1.0f;
    if (held(sf::Keyboard::Scancode::Down, sf::Keyboard::Scancode::S))  move.y -= 1.0f;

    if (move.lengthSquared() > 0.0f)
        m_pan += move.normalized() * config::TreePanSpeed * deltaTime;

    m_pan.x = std::clamp(m_pan.x, -config::TreePanLimit, config::TreePanLimit);
    m_pan.y = std::clamp(m_pan.y, -config::TreePanLimit, config::TreePanLimit);
}

void UpgradeScreen::setLevels(int selected, int unlocked)
{
    m_unlockedLevel = std::max(1, unlocked);
    m_selectedLevel = std::clamp(selected, 1, m_unlockedLevel);
}

sf::FloatRect UpgradeScreen::levelBounds(int level) const
{
    // Only the tail of the campaign is shown once it gets long: the early
    // levels stop being somewhere anybody chooses to go back to.
    const int first = std::max(1, m_unlockedLevel - config::LevelStripMax + 1);
    const int slot = level - first;

    const int shown = m_unlockedLevel - first + 1;
    const float total = static_cast<float>(shown) * config::LevelChipWidth
                        + static_cast<float>(shown - 1) * config::LevelChipGap;
    const float left = (static_cast<float>(config::WindowWidth) - total) / 2.0f;

    return sf::FloatRect({ left + static_cast<float>(slot)
                                      * (config::LevelChipWidth + config::LevelChipGap),
                           config::LevelStripY },
                         { config::LevelChipWidth, config::LevelChipHeight });
}

void UpgradeScreen::setCursor(sf::Vector2f worldPosition, const UpgradeTree& tree)
{
    m_hovered.reset();
    m_hoveredLevel.reset();

    const int first = std::max(1, m_unlockedLevel - config::LevelStripMax + 1);
    for (int level = first; level <= m_unlockedLevel; ++level)
    {
        if (levelBounds(level).contains(worldPosition))
        {
            m_hoveredLevel = level;
            return; // the strip sits over the tree, so it wins the cursor
        }
    }

    const std::vector<UpgradeNode>& nodes = tree.getNodes();
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        const sf::Vector2f delta = worldPosition - centreOf(nodes[i]);
        if (delta.lengthSquared() <= nodeRadius() * nodeRadius())
        {
            m_hovered = i;
            return;
        }
    }
}

void UpgradeScreen::drawLevelStrip(sf::RenderTarget& target)
{
    m_text.clear();

    const int first = std::max(1, m_unlockedLevel - config::LevelStripMax + 1);

    // Its own solid band, because the roots pan straight through this row.
    m_box.setOutlineColor(sf::Color::Transparent);
    m_box.setFillColor(sf::Color(2, 8, 14, 246));
    m_box.setSize({ static_cast<float>(config::WindowWidth), config::LevelChipHeight + 34.0f });
    m_box.setPosition({ 0.0f, config::LevelStripY - 26.0f });
    target.draw(m_box);

    m_box.setOutlineThickness(2.0f);

    const std::string caption = "DIVE INTO";
    pixelfont::append(m_text, caption,
                      { (static_cast<float>(config::WindowWidth)
                         - pixelfont::measure(caption, 1.5f)) / 2.0f,
                        config::LevelStripY - 20.0f },
                      1.5f, config::HudDimTextColor);

    for (int level = first; level <= m_unlockedLevel; ++level)
    {
        const sf::FloatRect bounds = levelBounds(level);
        const bool picked = level == m_selectedLevel;
        const bool boss = isBossLevel(level);

        m_box.setSize(bounds.size);
        m_box.setPosition(bounds.position);
        m_box.setFillColor(picked ? config::NodeUnlockedFill : config::NodeLockedFill);

        sf::Color edge = boss ? config::HudBossColor : config::NodeLockedEdge;
        if (picked)
            edge = sf::Color::White;
        else if (m_hoveredLevel && *m_hoveredLevel == level)
            edge = config::NodeReadyEdge;

        m_box.setOutlineColor(edge);
        target.draw(m_box);

        const std::string number = std::to_string(level);
        pixelfont::append(m_text, number,
                          { bounds.position.x + (bounds.size.x - pixelfont::measure(number, 1.6f)) / 2.0f,
                            bounds.position.y + 9.0f },
                          1.6f, picked ? config::HudTextColor
                                       : (boss ? config::HudBossColor : config::HudDimTextColor));
    }

    m_box.setSize({ config::NodeWidth, config::NodeHeight });
    target.draw(m_text);
}

void UpgradeScreen::drawLinks(sf::RenderTarget& target, const UpgradeTree& tree)
{
    m_lines.clear();

    const std::vector<UpgradeNode>& nodes = tree.getNodes();

    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        for (const std::string& prerequisiteId : nodes[i].prerequisites)
        {
            for (std::size_t j = 0; j < nodes.size(); ++j)
            {
                if (nodes[j].id != prerequisiteId)
                    continue;

                // A lit link means that prerequisite is satisfied, which is the
                // whole story about why a node is or is not reachable. Links
                // that cross columns are the deliberate synergies.
                const bool open = tree.isUnlocked(j);
                const sf::Color color = open ? config::LinkOpenColor : config::LinkShutColor;

                appendLine(m_lines, centreOf(nodes[j]), centreOf(nodes[i]),
                           (open ? 3.0f : 2.0f) * m_zoom, color);
            }
        }
    }

    target.draw(m_lines);
}

void UpgradeScreen::drawNodes(sf::RenderTarget& target, const UpgradeTree& tree, int salvage)
{
    m_text.clear();

    const std::vector<UpgradeNode>& nodes = tree.getNodes();

    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        const UpgradeNode& node = nodes[i];
        const sf::Vector2f centre = centreOf(node);

        const bool unlocked = tree.isUnlocked(i);
        const bool ready = tree.arePrerequisitesMet(i);
        const bool affordable = tree.canAfford(i, salvage);

        sf::Color fill = config::NodeLockedFill;
        sf::Color edge = config::NodeLockedEdge;
        sf::Color label = config::HudDimTextColor;

        if (unlocked)
        {
            fill = config::NodeUnlockedFill;
            edge = config::NodeUnlockedEdge;
            label = config::HudTextColor;
        }
        else if (ready)
        {
            fill = config::NodeReadyFill;
            // Prerequisites met but too expensive gets its own colour, so the
            // reason a node is out of reach is never ambiguous.
            edge = affordable ? config::NodeReadyEdge : config::NodePricyEdge;
            label = affordable ? config::HudTextColor : config::HudDimTextColor;
        }

        const bool hovered = m_hovered && *m_hovered == i;
        if (hovered)
            edge = sf::Color::White;

        // Fifty six boxes of text is a wall. Closed, a node is a disc with its
        // initials on it; what it actually does is only spelled out for the one
        // the cursor is on.
        m_node.setPosition(centre);
        m_node.setFillColor(fill);
        m_node.setOutlineColor(edge);
        const float radius = nodeRadius();
        m_node.setRadius(radius);
        m_node.setOrigin({ radius, radius });
        m_node.setOutlineThickness(2.5f * m_zoom);
        target.draw(m_node);

        const float iconScale = 1.8f * m_zoom;
        const std::string icon = initialsOf(node.name);
        pixelfont::append(m_text, icon,
                          { centre.x - pixelfont::measure(icon, iconScale) / 2.0f,
                            centre.y - 3.5f * iconScale },
                          iconScale, label);

        // Under the disc sits the only thing worth knowing at a glance now that
        // nothing is time locked: the price, and whether it is within reach.
        if (!unlocked)
        {
            // Gold means the click would work right now. A price the player
            // could pay on a node whose root is still missing is not that, so
            // it stays dim.
            const std::string tag = std::to_string(node.cost);
            const sf::Color tagColour = (ready && affordable) ? config::HudSalvageColor
                                                              : config::HudDimTextColor;

            const float tagScale = 1.3f * m_zoom;
            pixelfont::append(m_text, tag,
                              { centre.x - pixelfont::measure(tag, tagScale) / 2.0f,
                                centre.y + radius + 5.0f },
                              tagScale, tagColour);
        }
    }

    target.draw(m_text);

    if (m_hovered)
        drawNodeCard(target, tree, nodes[*m_hovered], salvage);
}

void UpgradeScreen::drawNodeCard(sf::RenderTarget& target, const UpgradeTree& tree,
                                 const UpgradeNode& node, int salvage)
{
    const sf::Vector2f centre = centreOf(node);

    // Opens beside the disc, flipped whenever it would run off the screen.
    const float width = config::NodeCardWidth;
    const float height = config::NodeCardHeight;

    sf::Vector2f corner{ centre.x + nodeRadius() + 12.0f, centre.y - height / 2.0f };
    if (corner.x + width > static_cast<float>(config::WindowWidth) - 10.0f)
        corner.x = centre.x - nodeRadius() - 12.0f - width;

    corner.y = std::clamp(corner.y, 96.0f, static_cast<float>(config::WindowHeight) - height - 120.0f);

    m_box.setSize({ width, height });
    m_box.setPosition(corner);
    m_box.setFillColor(sf::Color(6, 16, 22, 250));
    m_box.setOutlineColor(sf::Color::White);
    target.draw(m_box);
    m_box.setSize({ config::NodeWidth, config::NodeHeight });

    m_text.clear();

    pixelfont::append(m_text, node.name, corner + sf::Vector2f{ 12.0f, 12.0f }, 1.9f,
                      config::HudTextColor);
    pixelfont::append(m_text, node.description, corner + sf::Vector2f{ 12.0f, 36.0f }, 1.3f,
                      config::HudDimTextColor);

    const std::size_t index = tree.indexOfNode(node.id);
    std::string status;
    sf::Color statusColour = config::HudSalvageColor;

    if (tree.isUnlocked(index))
    {
        status = "INSTALLED";
        statusColour = config::HudDimTextColor;
    }
    else if (!tree.arePrerequisitesMet(index))
    {
        status = "LOCKED BEHIND THE ROOT BEFORE IT";
        statusColour = config::NodeLockedEdge;
    }
    else if (!tree.canAfford(index, salvage))
    {
        status = "COSTS " + std::to_string(node.cost) + " SALVAGE";
        statusColour = config::NodePricyEdge;
    }
    else
    {
        status = "CLICK TO INSTALL   -   " + std::to_string(node.cost) + " SALVAGE";
    }

    pixelfont::append(m_text, status, corner + sf::Vector2f{ 12.0f, height - 24.0f }, 1.4f,
                      statusColour);

    target.draw(m_text);
}

void UpgradeScreen::drawChrome(sf::RenderTarget& target, const UpgradeTree& tree, int salvage,
                               int depth, const std::string& prompt)
{
    m_text.clear();

    // Solid strips top and bottom. The roots pan underneath them, so without
    // these the tree would slide across the text the player is reading.
    m_box.setOutlineColor(sf::Color::Transparent);
    m_box.setFillColor(sf::Color(2, 8, 14, 246));

    m_box.setSize({ static_cast<float>(config::WindowWidth), 86.0f });
    m_box.setPosition({ 0.0f, 0.0f });
    target.draw(m_box);

    m_box.setSize({ static_cast<float>(config::WindowWidth), 112.0f });
    m_box.setPosition({ 0.0f, static_cast<float>(config::WindowHeight) - 112.0f });
    target.draw(m_box);

    pixelfont::append(m_text, "UPGRADE TREE", { config::HudMargin, 24.0f }, 2.6f,
                      config::HudTextColor);

    const std::string owned = "INSTALLED " + std::to_string(tree.getUnlockedCount() - 1) + "/"
                              + std::to_string(tree.getNodes().size() - 1);
    pixelfont::append(m_text, owned, { config::HudMargin, 54.0f }, 1.6f, config::HudDimTextColor);

    const std::string salvageLine = "SALVAGE " + std::to_string(salvage);
    pixelfont::append(m_text, salvageLine,
                      { static_cast<float>(config::WindowWidth) - config::HudMargin
                            - pixelfont::measure(salvageLine, 2.6f), 24.0f },
                      2.6f, config::HudSalvageColor);

    const std::string depthLine = "DEPTH " + std::to_string(depth);
    pixelfont::append(m_text, depthLine,
                      { static_cast<float>(config::WindowWidth) - config::HudMargin
                            - pixelfont::measure(depthLine, 1.6f), 54.0f },
                      1.6f, config::HudDimTextColor);

    // Each root is labelled at the node where it leaves the hull, so the name
    // travels with the branch as the view pans instead of sitting in a header
    // row that the roots grow straight through.
    target.draw(m_text);
    m_text.clear();

    for (const UpgradeNode& node : tree.getNodes())
    {
        if (node.column < 0 || node.position.lengthSquared() > 200.0f * 200.0f)
            continue; // only the first node of each root carries the name

        const char* name = UpgradeTree::getBranchName(node.column);
        const float width = pixelfont::measure(name, 1.4f);

        // Clear of the circle plus a constant gap: the label text does not
        // shrink with the zoom, so the gap must not shrink with it either.
        const sf::Vector2f label =
            centreOf(node) + node.position.normalized() * (nodeRadius() + 22.0f);

        m_box.setSize({ width + 14.0f, 18.0f });
        m_box.setPosition({ label.x - width / 2.0f - 7.0f, label.y - 4.0f });
        m_box.setFillColor(config::ScreenBackdrop);
        m_box.setOutlineColor(sf::Color::Transparent);
        target.draw(m_box);

        pixelfont::append(m_text, name, { label.x - width / 2.0f, label.y }, 1.4f,
                          config::HudDimTextColor);
    }

    m_box.setSize({ config::NodeWidth, config::NodeHeight }); // restore for the node pass

    float y = 640.0f;

    if (m_hovered)
    {
        const UpgradeNode& node = tree.getNodes()[*m_hovered];

        pixelfont::append(m_text, node.name, { config::HudMargin, y }, 2.2f, config::HudTextColor);
        y += 24.0f;
        pixelfont::append(m_text, node.description, { config::HudMargin, y }, 1.6f,
                          config::HudDimTextColor);
        y += 20.0f;

        std::string status;
        sf::Color statusColour = config::HudSalvageColor;

        if (tree.isUnlocked(*m_hovered))
        {
            status = "ALREADY INSTALLED";
            statusColour = config::HudDimTextColor;
        }
        else if (!tree.arePrerequisitesMet(*m_hovered))
        {
            status = "NEEDS: ";
            for (const std::string& id : node.prerequisites)
            {
                for (const UpgradeNode& other : tree.getNodes())
                {
                    if (other.id == id)
                        status += other.name + "  ";
                }
            }
            statusColour = config::NodeLockedEdge;
        }
        else if (!tree.canAfford(*m_hovered, salvage))
        {
            status = "NEEDS " + std::to_string(node.cost - salvage) + " MORE SALVAGE";
            statusColour = config::NodePricyEdge;
        }
        else
        {
            status = "CLICK TO INSTALL";
        }

        pixelfont::append(m_text, status, { config::HudMargin, y }, 1.6f, statusColour);
    }
    else
    {
        pixelfont::append(m_text, "WASD MOVES.  WHEEL OR +/- ZOOMS.  R STRIPS THE HULL.",
                          { config::HudMargin, y }, 1.6f, config::HudDimTextColor);
    }

    const float promptWidth = pixelfont::measure(prompt, 2.0f);
    pixelfont::append(m_text, prompt,
                      { static_cast<float>(config::WindowWidth) - config::HudMargin - promptWidth,
                        690.0f },
                      2.0f, config::HudSalvageColor);

    target.draw(m_text);
}

void UpgradeScreen::draw(sf::RenderTarget& target, const UpgradeTree& tree, int salvage, int depth,
                         const std::string& prompt)
{
    target.draw(m_backdrop);

    drawLinks(target, tree);
    drawNodes(target, tree, salvage);
    drawChrome(target, tree, salvage, depth, prompt);
    // Last, so the branch labels panning past cannot sit on top of it.
    drawLevelStrip(target);
}
