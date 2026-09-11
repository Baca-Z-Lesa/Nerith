#include "ui/MainMenu.h"

#include "core/Config.h"
#include "ui/PixelFont.h"
#include "world/LevelDef.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr float FirstEntryY = 372.0f;
    constexpr float EntryStep = 62.0f;
    constexpr float EntryWidth = 420.0f;
    constexpr float EntryHeight = 46.0f;

    float centredX(const std::string& text, float scale)
    {
        return (static_cast<float>(config::WindowWidth) - pixelfont::measure(text, scale)) / 2.0f;
    }
}

MainMenu::MainMenu()
    : m_selected(0)
    , m_text(sf::PrimitiveType::Triangles)
{
    m_veil.setSize({ static_cast<float>(config::WindowWidth),
                     static_cast<float>(config::WindowHeight) });
    m_veil.setFillColor(sf::Color(2, 6, 10, 208));

    m_box.setSize({ EntryWidth, EntryHeight });
    m_box.setOutlineThickness(2.0f);

    build(false, 1, 0, 0);
}

void MainMenu::build(bool hasRun, int resumeLevel, int bestLevel, int bestDepth)
{
    m_entries.clear();

    // Progress is permanent now, so there is no new-or-continue choice to make:
    // there is only ever the one dive, and the hub decides where it goes.
    m_entries.push_back({ "DIVE",
                          "LEVEL " + std::to_string(resumeLevel) + "  -  "
                              + depthName(depthOf(resumeLevel)),
                          Choice::Dive });

    m_entries.push_back({ "CONTROLS", "WHAT EVERY KEY DOES", Choice::Controls });
    m_entries.push_back({ "OPTIONS", "SOUND AND HOW HARD IT BITES", Choice::Options });
    m_entries.push_back({ "CREDITS", "WHO BUILT IT", Choice::Credits });

    if (hasRun)
        m_entries.push_back({ "SCUTTLE", "ERASE ALL SALVAGE AND UPGRADES", Choice::Reset });

    m_entries.push_back({ "SURFACE", "LEAVE", Choice::Quit });

    m_record = bestLevel > 0 ? "DEEPEST: LEVEL " + std::to_string(bestLevel) + "  -  "
                                   + depthName(std::max(1, bestDepth))
                             : "NOTHING HAS COME BACK UP YET";

    m_selected = 0;
}

void MainMenu::moveSelection(int delta)
{
    if (m_entries.empty())
        return;

    const int count = static_cast<int>(m_entries.size());
    int next = static_cast<int>(m_selected) + delta;

    // Wrapping keeps a three item list navigable without hunting for the end.
    while (next < 0)
        next += count;

    m_selected = static_cast<std::size_t>(next % count);
}

sf::FloatRect MainMenu::boundsOf(std::size_t index) const
{
    const float y = FirstEntryY + EntryStep * static_cast<float>(index);
    return sf::FloatRect({ (static_cast<float>(config::WindowWidth) - EntryWidth) / 2.0f, y },
                         { EntryWidth, EntryHeight });
}

void MainMenu::setCursor(sf::Vector2f screenPosition)
{
    for (std::size_t i = 0; i < m_entries.size(); ++i)
    {
        if (boundsOf(i).contains(screenPosition))
        {
            m_selected = i;
            return;
        }
    }
}

MainMenu::Choice MainMenu::getSelected() const
{
    return m_selected < m_entries.size() ? m_entries[m_selected].choice : Choice::Quit;
}

void MainMenu::draw(sf::RenderTarget& target, float time)
{
    target.draw(m_veil);

    m_text.clear();

    // The title breathes very slowly, like something a long way underwater.
    const float glow = 0.72f + 0.28f * std::sin(time * 0.9f);
    sf::Color titleColour = config::HudTextColor;
    titleColour.a = static_cast<std::uint8_t>(255.0f * glow);

    pixelfont::append(m_text, "NERITH", { centredX("NERITH", 11.0f), 150.0f }, 11.0f, titleColour);
    pixelfont::append(m_text, "SOMETHING IS DOWN THERE",
                      { centredX("SOMETHING IS DOWN THERE", 2.2f), 262.0f }, 2.2f,
                      config::HudDimTextColor);
    pixelfont::append(m_text, m_record, { centredX(m_record, 1.6f), 300.0f }, 1.6f,
                      config::HudDimTextColor);

    target.draw(m_text);
    m_text.clear();

    for (std::size_t i = 0; i < m_entries.size(); ++i)
    {
        const bool picked = i == m_selected;
        const sf::FloatRect bounds = boundsOf(i);

        m_box.setPosition(bounds.position);
        m_box.setFillColor(picked ? config::NodeUnlockedFill : config::NodeLockedFill);
        m_box.setOutlineColor(picked ? sf::Color::White : config::NodeLockedEdge);
        target.draw(m_box);

        const Entry& entry = m_entries[i];
        pixelfont::append(m_text, entry.label,
                          { centredX(entry.label, 2.2f), bounds.position.y + 8.0f }, 2.2f,
                          picked ? config::HudTextColor : config::HudDimTextColor);
        pixelfont::append(m_text, entry.note,
                          { centredX(entry.note, 1.3f), bounds.position.y + 30.0f }, 1.3f,
                          config::HudDimTextColor);
    }

    pixelfont::append(m_text, "ARROWS OR MOUSE   -   ENTER TO DIVE",
                      { centredX("ARROWS OR MOUSE   -   ENTER TO DIVE", 1.6f), 660.0f }, 1.6f,
                      config::HudDimTextColor);

    target.draw(m_text);
}
