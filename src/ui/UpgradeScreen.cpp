#include "ui/UpgradeScreen.h"

#include "core/Config.h"
#include "ui/PixelFont.h"
#include "upgrades/Stats.h"
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
    , m_icons(sf::PrimitiveType::Triangles)
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

    const auto held = [](sf::Keyboard::Scancode key) {
        return sf::Keyboard::isKeyPressed(key);
    };

    // Only WASD pans. The arrow keys walk the roots instead, which would be
    // the same gesture doing two jobs at once if they dragged the view too.
    if (held(sf::Keyboard::Scancode::A)) move.x += 1.0f;
    if (held(sf::Keyboard::Scancode::D)) move.x -= 1.0f;
    if (held(sf::Keyboard::Scancode::W)) move.y += 1.0f;
    if (held(sf::Keyboard::Scancode::S)) move.y -= 1.0f;

    if (move.lengthSquared() > 0.0f)
    {
        // Taking the wheel by hand stops the view chasing the picked node.
        m_keyboard = false;
        m_pan += move.normalized() * config::TreePanSpeed * deltaTime;
    }
    else if (m_keyboard)
    {
        // Eased rather than snapped: a jump across the tree loses you, a glide
        // shows you which way you went.
        m_pan += (m_panTarget - m_pan)
                 * std::min(1.0f, config::TreeFollowRate * deltaTime);
    }

    m_pan.x = std::clamp(m_pan.x, -config::TreePanLimit, config::TreePanLimit);
    m_pan.y = std::clamp(m_pan.y, -config::TreePanLimit, config::TreePanLimit);

    // The card unrolls quickly and shuts faster. Shutting faster matters: a
    // cursor crossing the tree should not drag a trail of half open cards.
    const float target = m_hovered ? 1.0f : 0.0f;
    const float rate = m_hovered ? config::CardOpenRate : config::CardShutRate;

    if (m_openness < target)
        m_openness = std::min(target, m_openness + rate * deltaTime);
    else
        m_openness = std::max(target, m_openness - rate * deltaTime);
}

void UpgradeScreen::recentre()
{
    m_pan = { 0.0f, 0.0f };
    m_panTarget = { 0.0f, 0.0f };
    m_zoom = 1.0f;
    m_keyboard = false;
}

void UpgradeScreen::focusOn(std::size_t index, const UpgradeTree& tree)
{
    if (index >= tree.getNodes().size())
        return;

    if (!m_hovered || *m_hovered != index)
        m_openness = 0.0f;

    m_hovered = index;
    m_hoveredLevel.reset();
    m_keyboard = true;

    // Pan is applied before the zoom, so the negated position is exactly the
    // offset that puts this node under the origin whatever the zoom is.
    m_panTarget = -tree.getNodes()[index].position;
}

void UpgradeScreen::moveFocus(sf::Vector2f direction, const UpgradeTree& tree)
{
    const std::vector<UpgradeNode>& nodes = tree.getNodes();
    if (nodes.empty() || direction.lengthSquared() <= 0.0f)
        return;

    // Nothing picked yet: start at the hull, which is the one node every root
    // grows out of.
    if (!m_hovered || *m_hovered >= nodes.size())
    {
        focusOn(0, tree);
        return;
    }

    const sf::Vector2f from = nodes[*m_hovered].position;
    const sf::Vector2f side{ -direction.y, direction.x };

    std::size_t best = *m_hovered;
    float bestScore = 0.0f;
    bool found = false;

    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        if (i == *m_hovered)
            continue;

        const sf::Vector2f delta = nodes[i].position - from;
        const float forward = delta.dot(direction);
        if (forward <= 0.0f)
            continue; // behind you, or exactly sideways

        const float sideways = std::abs(delta.dot(side));
        if (sideways > forward * 1.8f)
            continue; // more of a turn than a step

        // Drift across the press costs double, so a step keeps to the limb it
        // started on whenever anything at all lies straight ahead.
        const float score = forward + sideways * 2.0f;
        if (!found || score < bestScore)
        {
            found = true;
            bestScore = score;
            best = i;
        }
    }

    if (found)
        focusOn(best, tree);
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
    // The mouse is polled every frame whether it moved or not, so without this
    // a keyboard pick would be wiped again on the very next frame.
    const bool moved = (worldPosition - m_mouse).lengthSquared() > 1.0f;
    m_mouse = worldPosition;

    if (m_keyboard && !moved)
        return;

    m_keyboard = false;

    const std::optional<std::size_t> previous = m_hovered;

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

            // Moving to a different node starts its card from shut, so one
            // never finishes another's animation.
            if (previous != m_hovered)
                m_openness = 0.0f;
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

                // The wire carries the branch's colour too, so an unlocked run
                // reads as a lit circuit through one system rather than as a
                // row of discs that happen to be joined. Links that cross
                // columns take the colour of where they arrive, which is what
                // makes a deliberate synergy visible as one.
                const sf::Color branch = UpgradeTree::getBranchColour(nodes[i].column);
                const float amount = open ? 0.90f : 0.26f;

                const sf::Color color(static_cast<std::uint8_t>(branch.r * amount),
                                      static_cast<std::uint8_t>(branch.g * amount),
                                      static_cast<std::uint8_t>(branch.b * amount),
                                      open ? 230 : 130);

                appendLine(m_lines, centreOf(nodes[j]), centreOf(nodes[i]),
                           (open ? 3.0f : 2.0f) * m_zoom, color);
            }
        }
    }

    target.draw(m_lines);
}

void UpgradeScreen::drawNodes(sf::RenderTarget& target, const UpgradeTree& tree,
                              const Stats& stats, int salvage)
{
    m_text.clear();
    m_icons.clear();

    const std::vector<UpgradeNode>& nodes = tree.getNodes();

    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        const UpgradeNode& node = nodes[i];
        const sf::Vector2f centre = centreOf(node);

        const bool unlocked = tree.isUnlocked(i);
        const bool ready = tree.arePrerequisitesMet(i);
        const bool affordable = tree.canAfford(i, salvage);

        // The branch decides the hue and the state decides how much of it there
        // is. Ninety-nine discs in one colour meant the only way to find out
        // which part of the sub a node belonged to was to put the cursor on it.
        const sf::Color branch = UpgradeTree::getBranchColour(node.column);

        const auto lit = [](sf::Color colour, float amount, std::uint8_t alpha = 255) {
            return sf::Color(static_cast<std::uint8_t>(colour.r * amount),
                             static_cast<std::uint8_t>(colour.g * amount),
                             static_cast<std::uint8_t>(colour.b * amount), alpha);
        };

        sf::Color fill = lit(branch, 0.10f);
        sf::Color edge = lit(branch, 0.34f);
        sf::Color label = config::HudDimTextColor;

        if (unlocked)
        {
            // Bought: the branch at full strength, and the one state that is
            // meant to be visible from the other side of the screen.
            fill = lit(branch, 0.42f);
            edge = lit(branch, 1.0f);
            label = config::HudTextColor;
        }
        else if (ready)
        {
            fill = lit(branch, 0.18f);

            // Prerequisites met but too expensive keeps the branch and loses
            // the light, so the reason a node is out of reach is never
            // ambiguous and never stops telling you what it is.
            edge = affordable ? lit(branch, 0.82f) : lit(branch, 0.46f);
            label = affordable ? config::HudTextColor : config::HudDimTextColor;
        }

        const bool hovered = m_hovered && *m_hovered == i;
        if (hovered)
            edge = sf::Color::White;

        // Eighty boxes of text is a wall. Closed, a node is a disc with a mark
        // on it; what it actually does is only spelled out for the one the
        // cursor is on, and that one swells to say so.
        float radius = nodeRadius();
        if (hovered)
            radius *= 1.0f + 0.16f * m_openness;

        m_node.setPosition(centre);
        m_node.setFillColor(fill);
        m_node.setOutlineColor(edge);
        m_node.setRadius(radius);
        m_node.setOrigin({ radius, radius });
        m_node.setOutlineThickness((hovered ? 3.2f : 2.5f) * m_zoom);
        target.draw(m_node);

        // A ring opening outward from whatever the cursor is on, so the eye is
        // never hunting for which disc the card belongs to.
        if (hovered && m_openness > 0.01f)
        {
            const float halo = radius + 10.0f * m_zoom * m_openness;
            sf::Color ringColour = edge;
            ringColour.a = static_cast<std::uint8_t>(150.0f * (1.0f - m_openness * 0.5f));

            m_node.setRadius(halo);
            m_node.setOrigin({ halo, halo });
            m_node.setFillColor(sf::Color::Transparent);
            m_node.setOutlineColor(ringColour);
            m_node.setOutlineThickness(1.8f * m_zoom);
            target.draw(m_node);
        }

        // A drawn mark rather than initials: "PV" told nobody anything.
        icons::append(m_icons, node.icon, centre, radius * 1.15f, label);

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

    target.draw(m_icons);
    target.draw(m_text);

    if (m_hovered)
        drawNodeCard(target, tree, stats, nodes[*m_hovered], salvage);
}

bool UpgradeScreen::alreadyAtTheTop(const Stats& stats, const UpgradeNode& node)
{
    // A node that switches something on or bolts something to the hull is
    // never wasted, whatever its numbers do.
    if (!node.traits.empty() || !node.visuals.empty() || node.effects.empty())
        return false;

    // Applied to a copy and compared: exact, rather than a guess about which
    // stats have ceilings and what their bases are. The base is unknown here
    // - it lives in config next to whatever reads the stat - so both sides
    // are measured from zero, which can only ever under-report.
    Stats after = stats;
    for (const StatModifier& modifier : node.effects)
        after.apply(modifier);

    for (const StatModifier& modifier : node.effects)
    {
        if (after.get(modifier.stat, 0.0f) > stats.get(modifier.stat, 0.0f) + 0.0005f)
            return false;
    }

    return true;
}

void UpgradeScreen::drawNodeCard(sf::RenderTarget& target, const UpgradeTree& tree,
                                 const Stats& stats, const UpgradeNode& node, int salvage)
{
    const sf::Vector2f centre = centreOf(node);

    // It unrolls out of the disc rather than appearing: width first, then the
    // text written into it. Under about a fifth open there is nothing to read
    // yet, so nothing is drawn and a cursor sweeping across the tree does not
    // strobe.
    if (m_openness < 0.02f)
        return;

    const float width = config::NodeCardWidth * (0.34f + 0.66f * m_openness);
    // Tall enough for whatever it has to say. The card used to be one fixed
    // height, which was exactly right until it grew a line per number the node
    // moves - and then a node that changes three of them printed its third
    // straight through the status line at the bottom.
    int readouts = 0;
    for (const StatModifier& effect : node.effects)
    {
        if (std::abs(stats.get(effect.stat, 1.0f)
                     - (stats.get(effect.stat, 1.0f) + effect.flat)) > 0.001f
            || std::abs(effect.percent) > 0.001f || std::abs(effect.flat) > 0.001f)
        {
            ++readouts;
        }
    }

    const float height = config::NodeCardHeight + 17.0f * static_cast<float>(readouts);

    sf::Vector2f corner{ centre.x + nodeRadius() + 14.0f, centre.y - height / 2.0f };
    if (corner.x + config::NodeCardWidth > static_cast<float>(config::WindowWidth) - 10.0f)
        corner.x = centre.x - nodeRadius() - 14.0f - width;

    corner.y = std::clamp(corner.y, 96.0f, static_cast<float>(config::WindowHeight) - height - 120.0f);

    const auto fade = [this](sf::Color colour) {
        colour.a = static_cast<std::uint8_t>(static_cast<float>(colour.a) * m_openness);
        return colour;
    };

    m_box.setSize({ width, height });
    m_box.setPosition(corner);
    m_box.setFillColor(fade(sf::Color(6, 16, 22, 250)));
    m_box.setOutlineColor(fade(sf::Color::White));
    target.draw(m_box);
    m_box.setSize({ config::NodeWidth, config::NodeHeight });

    // The mark again, large, at the head of the card: the disc and the card
    // are visibly the same thing.
    m_icons.clear();
    icons::append(m_icons, node.icon, corner + sf::Vector2f{ 30.0f, 30.0f },
                  38.0f * m_openness, fade(config::HudTextColor));
    target.draw(m_icons);

    m_text.clear();

    pixelfont::append(m_text, node.name, corner + sf::Vector2f{ 58.0f, 14.0f }, 1.9f,
                      fade(config::HudTextColor));

    // The description is written out a character at a time as the card opens,
    // which is what makes it read as unrolling rather than as a popup.
    const std::size_t shown = static_cast<std::size_t>(
        static_cast<float>(node.description.size()) * std::min(1.0f, m_openness * 1.45f));

    pixelfont::append(m_text, node.description.substr(0, shown),
                      corner + sf::Vector2f{ 12.0f, 46.0f }, 1.3f,
                      fade(config::HudDimTextColor));

    // What the number actually becomes.
    //
    // "+25% JET DAMAGE" is a promise, not a figure: a quarter more of what, on
    // top of what else already bought, against a ceiling nobody mentioned. So
    // the card works it out - the block as it stands, the block with this node
    // applied, and the two printed side by side. It is the one line that turns
    // buying into a decision instead of a hope.
    {
        struct Readout { Stat stat; const char* label; float base; bool percent; };

        static const Readout Readouts[] = {
            { Stat::ProjectileDamage, "JET DAMAGE", static_cast<float>(config::WaterJetDamage), false },
            { Stat::FireRate,         "FIRE RATE",  1.0f, true },
            { Stat::ProjectileSpeed,  "JET SPEED",  config::WaterJet.speed, false },
            { Stat::Pierce,           "PIERCE",     0.0f, false },
            { Stat::RicochetCount,    "BOUNCES",    0.0f, false },
            { Stat::CritChance,       "CRIT",       0.0f, true },
            { Stat::MaxHealth,        "HULL",       static_cast<float>(config::PlayerMaxHealth), false },
            { Stat::Armour,           "ARMOUR",     0.0f, false },
            { Stat::MoveSpeed,        "SPEED",      config::PlayerSpeed, false },
            { Stat::ShieldCharges,    "SHIELD",     0.0f, false },
            { Stat::TorpedoDamage,    "TORPEDO",    static_cast<float>(config::TorpedoBaseDamage), false },
            { Stat::MineDamage,       "MINE",       static_cast<float>(config::MineBaseDamage), false },
            { Stat::DroneCount,       "DRONES",     0.0f, false },
            { Stat::MineCount,        "MINES",      0.0f, false },
            { Stat::SonarRange,       "SONAR",      config::SonarBaseRange, false },
            { Stat::SalvageBonus,     "SALVAGE",    0.0f, true },
        };

        Stats after = stats;
        for (const StatModifier& effect : node.effects)
            after.apply(effect);

        float row = 66.0f;

        for (const Readout& read : Readouts)
        {
            // Only the numbers this node actually touches.
            bool touched = false;
            for (const StatModifier& effect : node.effects)
                touched = touched || effect.stat == read.stat;

            if (!touched)
                continue;

            const float was = stats.get(read.stat, read.base);
            const float now = after.get(read.stat, read.base);

            if (std::abs(now - was) < 0.001f)
                continue; // already at its ceiling: the status line says so

            const auto show = [&read](float value) {
                if (read.percent)
                    return std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%";

                return std::to_string(static_cast<int>(std::lround(value * 10.0f) / 10))
                       + (std::abs(value - std::lround(value)) > 0.05f
                              ? "." + std::to_string(static_cast<int>(std::lround(value * 10.0f)) % 10)
                              : "");
            };

            pixelfont::append(m_text, std::string(read.label) + "  " + show(was) + " > " + show(now),
                              corner + sf::Vector2f{ 12.0f, row }, 1.3f,
                              fade(config::HudSalvageColor));

            row += 17.0f;
        }
    }

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
    else if (alreadyAtTheTop(stats, node))
    {
        // Nine roots feed the same handful of numbers and most of them have a
        // ceiling, so a player who buys "+3 bounces" and gets one has found a
        // bug unless the screen said so first.
        status = "EVERY NUMBER ON IT IS ALREADY AT ITS LIMIT";
        statusColour = config::NodeLockedEdge;
    }
    else
    {
        status = "CLICK TO INSTALL   -   " + std::to_string(node.cost) + " SALVAGE";
    }

    pixelfont::append(m_text, status, corner + sf::Vector2f{ 12.0f, height - 24.0f }, 1.4f,
                      fade(statusColour));

    target.draw(m_text);
}

void UpgradeScreen::drawBranchLabels(sf::RenderTarget& target, const UpgradeTree& tree)
{
    // Each root is labelled at the node where it leaves the hull, so the name
    // travels with the branch as the view pans instead of sitting in a header
    // row that the roots grow straight through.
    //
    // A label belongs to the TREE, not to the screen, so it is drawn with the
    // tree - before the nodes, the hovered node's card and the chrome strips.
    // It used to be drawn with the chrome, which is last of all, so a label
    // that happened to pan under the open card was printed straight across the
    // description the player was reading: WEAPONS and TORPEDOES sat inside the
    // GUN BAY card, which reads as a rendering fault rather than as a label.
    m_text.clear();
    m_box.setOutlineColor(sf::Color::Transparent);

    for (const UpgradeNode& node : tree.getNodes())
    {
        if (node.column < 0 || node.position.lengthSquared() > 200.0f * 200.0f)
            continue; // only the first node of each root carries the name

        const char* name = UpgradeTree::getBranchName(node.column);
        const float width = pixelfont::measure(name, 1.4f);

        // Clear of the circle plus a constant gap: the label text does not
        // shrink with the zoom, so the gap must not shrink with it either.
        // Pushed sideways rather than straight out, because straight out is
        // where the rest of the root is.
        const sf::Vector2f along = node.position.normalized();
        const sf::Vector2f across{ -along.y, along.x };
        const sf::Vector2f label = centreOf(node)
                                   + along * (nodeRadius() + 10.0f)
                                   + across * (nodeRadius() + 20.0f);

        m_box.setSize({ width + 14.0f, 18.0f });
        m_box.setPosition({ label.x - width / 2.0f - 7.0f, label.y - 4.0f });
        m_box.setFillColor(config::ScreenBackdrop);
        target.draw(m_box);

        // In its own branch's colour, so the name and the run of discs it names
        // are obviously the same thing.
        pixelfont::append(m_text, name, { label.x - width / 2.0f, label.y }, 1.4f,
                          UpgradeTree::getBranchColour(node.column));
    }

    target.draw(m_text);
    m_text.clear();

    m_box.setSize({ config::NodeWidth, config::NodeHeight }); // restore for the node pass
}

void UpgradeScreen::drawChrome(sf::RenderTarget& target, const UpgradeTree& tree, int salvage,
                               int depth, const std::string& prompt)
{
    m_text.clear();

    // Solid strips top and bottom. The roots pan underneath them, so without
    // these the tree would slide across the text the player is reading.
    //
    // Opaque, not almost: at 246 a branch label sitting under the bottom strip
    // still printed faintly through it, which reads as a smudge on the panel.
    m_box.setOutlineColor(sf::Color::Transparent);
    m_box.setFillColor(sf::Color(2, 8, 14));

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
            status = "CLICK OR SPACE TO INSTALL";
        }

        pixelfont::append(m_text, status, { config::HudMargin, y }, 1.6f, statusColour);
    }
    else
    {
        pixelfont::append(m_text, "ARROWS PICK.  SPACE INSTALLS.  , . PICK THE DIVE.  "
                          "WASD PANS.  WHEEL ZOOMS.  R STRIPS THE HULL.",
                          { config::HudMargin, y }, 1.6f, config::HudDimTextColor);
    }

    const float promptWidth = pixelfont::measure(prompt, 2.0f);
    pixelfont::append(m_text, prompt,
                      { static_cast<float>(config::WindowWidth) - config::HudMargin - promptWidth,
                        690.0f },
                      2.0f, config::HudSalvageColor);

    target.draw(m_text);
}

void UpgradeScreen::draw(sf::RenderTarget& target, const UpgradeTree& tree, const Stats& stats,
                         int salvage, int depth,
                         const std::string& prompt)
{
    target.draw(m_backdrop);

    if (m_summary)
    {
        drawSummary(target, tree, stats);
        drawChrome(target, tree, salvage, depth, prompt);
        drawLevelStrip(target);
        return;
    }

    drawLinks(target, tree);
    drawBranchLabels(target, tree);
    drawNodes(target, tree, stats, salvage);
    drawChrome(target, tree, salvage, depth, prompt);
    drawLevelStrip(target);
}

void UpgradeScreen::drawSummary(sf::RenderTarget& target, const UpgradeTree& tree,
                                const Stats& stats)
{
    // Ninety-nine nodes and nowhere that adds them up. A player who has spent
    // four campaigns in this tree knows what they bought and has no idea what
    // they are driving: the numbers are spread over a hundred cards, most of
    // them percentages of each other, and nothing anywhere says what the sub
    // actually does now. This is that page, in sentences.
    m_text.clear();

    const auto number = [](float value) {
        const long whole = std::lround(value * 10.0f);
        std::string out = std::to_string(whole / 10);

        if (whole % 10 != 0)
            out += "." + std::to_string(std::abs(whole % 10));

        return out;
    };

    const float rate = config::PlayerFireCooldown
                       / std::max(0.05f, stats.get(Stat::FireRate, 1.0f));
    const float damage = stats.get(Stat::ProjectileDamage,
                                   static_cast<float>(config::WaterJetDamage));

    std::vector<std::pair<std::string, std::string>> lines;

    lines.emplace_back("THE GUN",
                       "IT PUTS OUT " + number(damage / std::max(0.01f, rate))
                           + " DAMAGE A SECOND.");

    if (stats.getInt(Stat::Pierce, 0) > 0)
        lines.back().second += "  IT GOES THROUGH "
                               + std::to_string(stats.getInt(Stat::Pierce, 0) + 1) + ".";

    if (stats.has(Trait::Ricochet))
        lines.emplace_back("", "IT BOUNCES OFF ROCK "
                                   + std::to_string(stats.getInt(Stat::RicochetCount, 0))
                                   + " TIMES.");

    if (stats.has(Trait::Crit))
        lines.emplace_back("", "ONE SHOT IN "
                                   + number(1.0f / std::max(0.01f,
                                                            stats.get(Stat::CritChance, 0.0f)))
                                   + " LANDS DOUBLE.");

    const int hull = stats.getInt(Stat::MaxHealth, config::PlayerMaxHealth);
    const int armour = stats.getInt(Stat::Armour, 0);

    std::string skin = "THE HULL TAKES " + std::to_string(hull) + " HITS.";
    if (armour > 0)
        skin += "  " + std::to_string(armour) + " OFF EVERY WOUND, NEVER ALL OF IT.";

    lines.emplace_back("THE HULL", skin);

    if (stats.has(Trait::Shield))
        lines.emplace_back("", std::to_string(stats.getInt(Stat::ShieldCharges, 0))
                                   + " SHIELD CHARGES, REGROWN BETWEEN BITES.");

    lines.emplace_back("THE WATER",
                       "THE LAMP REACHES "
                           + number(config::CameraLampReference
                                    + stats.get(Stat::LampRadius, 0.0f))
                           + ".");

    if (stats.has(Trait::Sonar))
        lines.emplace_back("", "SONAR SWEEPS "
                                   + number(stats.get(Stat::SonarRange, config::SonarBaseRange))
                                   + ".");

    if (stats.has(Trait::Drones))
        lines.emplace_back("THE ESCORT",
                           std::to_string(stats.getInt(Stat::DroneCount, 0))
                               + " DRONES PICK THEIR OWN TARGETS.");

    if (stats.has(Trait::Mines))
        lines.emplace_back("", std::to_string(stats.getInt(Stat::MineCount, 0))
                                   + " MINES IN THE WATER AT ONCE.");

    if (stats.get(Stat::SalvageBonus, 0.0f) > 0.001f)
        lines.emplace_back("THE HOLD",
                           "EVERY PIECE IS WORTH "
                               + std::to_string(static_cast<int>(std::lround(
                                     100.0f + stats.get(Stat::SalvageBonus, 0.0f) * 100.0f)))
                               + "%.");

    // Tight enough that the longest build - every node in the tree, which is
    // the most lines this can ever have - still lands above the bottom strip.
    // At its first spacing the last group was printed underneath it.
    float y = 132.0f;
    const float x = 150.0f;

    // No count here: the chrome at the top left already says how many are
    // installed, and two counts on one screen that disagree by one is worse
    // than one count.
    pixelfont::append(m_text, "THE SUB", { x, 92.0f }, 3.4f, config::HudTextColor);

    for (const auto& line : lines)
    {
        if (!line.first.empty())
        {
            y += 12.0f;
            pixelfont::append(m_text, line.first, { x, y }, 1.6f, config::HudSalvageColor);
            y += 23.0f;
        }

        pixelfont::append(m_text, line.second, { x + 18.0f, y }, 1.5f, config::HudTextColor);
        y += 22.0f;
    }

    target.draw(m_text);
    m_text.clear();
}
