#include "ui/UpgradeScreen.h"

#include "core/Config.h"
#include "ui/PixelFont.h"

#include <string>

namespace
{
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
    : m_lines(sf::PrimitiveType::Triangles)
    , m_text(sf::PrimitiveType::Triangles)
{
    m_backdrop.setSize({ static_cast<float>(config::WindowWidth),
                         static_cast<float>(config::WindowHeight) });
    m_backdrop.setFillColor(config::ScreenBackdrop);

    m_box.setSize({ config::NodeWidth, config::NodeHeight });
    m_box.setOutlineThickness(2.0f);
}

sf::Vector2f UpgradeScreen::centreOf(const UpgradeNode& node)
{
    return config::TreeOrigin + node.position;
}

sf::FloatRect UpgradeScreen::boundsOf(const UpgradeNode& node)
{
    const sf::Vector2f centre = centreOf(node);
    const sf::Vector2f size{ config::NodeWidth, config::NodeHeight };
    return sf::FloatRect(centre - size / 2.0f, size);
}

void UpgradeScreen::setCursor(sf::Vector2f worldPosition, const UpgradeTree& tree)
{
    m_hovered.reset();

    const std::vector<UpgradeNode>& nodes = tree.getNodes();
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        if (boundsOf(nodes[i]).contains(worldPosition))
        {
            m_hovered = i;
            return;
        }
    }
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

                appendLine(m_lines, centreOf(nodes[j]), centreOf(nodes[i]), open ? 3.0f : 2.0f, color);
            }
        }
    }

    target.draw(m_lines);
}

void UpgradeScreen::drawNodes(sf::RenderTarget& target, const UpgradeTree& tree, int salvage, int depth)
{
    m_text.clear();

    const std::vector<UpgradeNode>& nodes = tree.getNodes();

    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        const UpgradeNode& node = nodes[i];
        const sf::FloatRect bounds = boundsOf(node);

        const bool unlocked = tree.isUnlocked(i);
        const bool ready = tree.arePrerequisitesMet(i);
        const bool deepEnough = tree.isDepthMet(i, depth);
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
        else if (ready && deepEnough)
        {
            fill = config::NodeReadyFill;
            // Prerequisites met but too expensive gets its own colour, so the
            // reason a node is out of reach is never ambiguous.
            edge = affordable ? config::NodeReadyEdge : config::NodePricyEdge;
            label = affordable ? config::HudTextColor : config::HudDimTextColor;
        }

        if (m_hovered && *m_hovered == i)
            edge = sf::Color::White;

        m_box.setPosition(bounds.position);
        m_box.setFillColor(fill);
        m_box.setOutlineColor(edge);
        target.draw(m_box);

        pixelfont::append(m_text, node.name, bounds.position + sf::Vector2f{ 6.0f, 7.0f },
                          1.3f, label);

        std::string bottom;
        sf::Color bottomColour = config::HudDimTextColor;

        if (unlocked)
        {
            bottom = "OWNED";
        }
        else if (!deepEnough)
        {
            // Depth locks are the main reason the tree cannot be rushed, so
            // they say exactly how far down the node lives.
            bottom = "DEPTH " + std::to_string(node.requiredDepth);
            bottomColour = config::NodePricyEdge;
        }
        else
        {
            bottom = std::to_string(node.cost);
            bottomColour = affordable ? config::HudSalvageColor : config::HudDimTextColor;
        }

        pixelfont::append(m_text, bottom, bounds.position + sf::Vector2f{ 6.0f, 25.0f },
                          1.3f, bottomColour);
    }

    target.draw(m_text);
}

void UpgradeScreen::drawChrome(sf::RenderTarget& target, const UpgradeTree& tree, int salvage,
                               int depth, const std::string& prompt)
{
    m_text.clear();

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

    // Branch headers, so the seven columns read as seven decisions. Each gets a
    // solid backing because the root's links fan out straight through this row.
    target.draw(m_text);
    m_text.clear();

    for (int column = 0; column < UpgradeBranchCount; ++column)
    {
        const char* name = UpgradeTree::getBranchName(column);
        const float width = pixelfont::measure(name, 1.5f);
        const float centre = config::TreeOrigin.x + 75.0f + 168.0f * static_cast<float>(column);
        const float y = config::TreeOrigin.y - 36.0f;

        m_box.setSize({ width + 16.0f, 20.0f });
        m_box.setPosition({ centre - width / 2.0f - 8.0f, y - 5.0f });
        m_box.setFillColor(config::ScreenBackdrop);
        m_box.setOutlineColor(sf::Color::Transparent);
        target.draw(m_box);

        pixelfont::append(m_text, name, { centre - width / 2.0f, y }, 1.5f,
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
        else if (!tree.isDepthMet(*m_hovered, depth))
        {
            status = "SEALED UNTIL DEPTH " + std::to_string(node.requiredDepth);
            statusColour = config::NodePricyEdge;
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
        pixelfont::append(m_text, "HOVER A NODE FOR DETAILS. NO RUN REACHES ALL OF THIS.",
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
    drawNodes(target, tree, salvage, depth);
    drawChrome(target, tree, salvage, depth, prompt);
}
