#include "ui/MainMenu.h"

#include "core/Config.h"
#include "ui/PixelFont.h"
#include "world/LevelDef.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    // Six entries have to fit between the strap line and the footer hint, so
    // the rows are tighter than they were when there were three.
    // Sized for seven entries, which is what there are once the logbook and a
    // run to scuttle are both in the list. Tightened rather than scrolled: a
    // title screen that scrolls is a title screen that has lost the plot.
    constexpr float FirstEntryY = 318.0f;
    constexpr float EntryStep = 48.0f;
    constexpr float EntryWidth = 420.0f;
    constexpr float EntryHeight = 41.0f;

    float centredX(const std::string& text, float scale)
    {
        return (static_cast<float>(config::WindowWidth) - pixelfont::measure(text, scale)) / 2.0f;
    }

    sf::Color blend(sf::Color from, sf::Color to, float k)
    {
        const auto mix = [k](std::uint8_t a, std::uint8_t b) {
            return static_cast<std::uint8_t>(static_cast<float>(a)
                                             + (static_cast<float>(b) - static_cast<float>(a)) * k);
        };

        return { mix(from.r, to.r), mix(from.g, to.g), mix(from.b, to.b), mix(from.a, to.a) };
    }

    // How far a row reaches out when it comes up, and the corner marks that
    // arrive with it. Small numbers on purpose: this is the same row it always
    // was, paying attention.
    constexpr float Reach = 9.0f;      // pixels it widens either side
    constexpr float BarWide = 3.0f;    // the lit edge down its left
    constexpr float Bracket = 11.0f;   // arm length of a corner mark
    constexpr float EaseRate = 12.0f;  // per second, exponential
}

MainMenu::MainMenu()
    : m_selected(0)
    , m_marks(sf::PrimitiveType::Triangles)
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
    m_entries.push_back({ "LOGBOOK", "WHAT YOU HAVE MET DOWN THERE", Choice::Bestiary });
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

void MainMenu::mark(sf::Vector2f at, sf::Vector2f size, sf::Color colour)
{
    const sf::Vector2f a = at;
    const sf::Vector2f b = at + sf::Vector2f{ size.x, 0.0f };
    const sf::Vector2f c = at + size;
    const sf::Vector2f d = at + sf::Vector2f{ 0.0f, size.y };

    m_marks.append(sf::Vertex{ a, colour });
    m_marks.append(sf::Vertex{ b, colour });
    m_marks.append(sf::Vertex{ c, colour });

    m_marks.append(sf::Vertex{ a, colour });
    m_marks.append(sf::Vertex{ c, colour });
    m_marks.append(sf::Vertex{ d, colour });
}

void MainMenu::draw(sf::RenderTarget& target, float time)
{
    // The caller owns the clock and always has; this only needs the gap since
    // the last frame, and clamping it means coming back from a long stall does
    // not teleport every ease to its target.
    const float deltaTime = std::clamp(time - m_lastTime, 0.0f, 0.1f);
    m_lastTime = time;

    target.draw(m_veil);

    m_text.clear();
    m_marks.clear();

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

    // The one the selected row breathes on. Slower than a heartbeat, the same
    // way the title does, so the two are obviously the same screen.
    const float breath = 0.78f + 0.22f * std::sin(time * 2.4f);
    const float ease = 1.0f - std::exp(-EaseRate * deltaTime);

    for (std::size_t i = 0; i < m_entries.size(); ++i)
    {
        Entry& entry = m_entries[i];
        const bool picked = i == m_selected;

        entry.glow += ((picked ? 1.0f : 0.0f) - entry.glow) * ease;

        const float lit = entry.glow;
        const sf::FloatRect bounds = boundsOf(i);

        // It reaches out as it comes up rather than jumping to a new colour.
        const float grown = Reach * lit;
        const sf::Vector2f at{ bounds.position.x - grown, bounds.position.y };
        const sf::Vector2f size{ bounds.size.x + grown * 2.0f, bounds.size.y };

        m_box.setSize(size);
        m_box.setPosition(at);
        m_box.setFillColor(blend(config::NodeLockedFill, config::NodeUnlockedFill, lit));
        m_box.setOutlineThickness(2.0f + lit);
        m_box.setOutlineColor(blend(config::NodeLockedEdge, sf::Color::White, lit * breath));
        target.draw(m_box);

        if (lit > 0.01f)
        {
            // A lit edge down its left, grown from the middle out, and a
            // bracket at each corner. Together they are what says "this one"
            // without the row having to become a different shape.
            const sf::Color accent = blend(sf::Color(255, 255, 255, 0), config::HudBossColor,
                                           lit * breath);

            const float tall = size.y * lit;
            mark({ at.x - BarWide - 3.0f, at.y + (size.y - tall) * 0.5f }, { BarWide, tall },
                 accent);

            const float arm = Bracket * lit;
            for (const float sx : { 0.0f, 1.0f })
            {
                for (const float sy : { 0.0f, 1.0f })
                {
                    const float cx = at.x + size.x * sx;
                    const float cy = at.y + size.y * sy;
                    const float dx = sx > 0.5f ? -arm : 0.0f;
                    const float dy = sy > 0.5f ? -2.0f : 0.0f;

                    mark({ cx + dx, cy + dy }, { arm, 2.0f }, accent);
                    mark({ cx + (sx > 0.5f ? -2.0f : 0.0f), cy + (sy > 0.5f ? -arm : 0.0f) },
                         { 2.0f, arm }, accent);
                }
            }
        }

        // The one entry that cannot be undone fills rather than fires. It is
        // the only row in the game that asks for a second of commitment, and
        // it asks for it by filling up under the words so there is never a
        // moment where a player has scuttled a run without watching it happen.
        if (picked && entry.choice == Choice::Reset && m_hold > 0.0f)
        {
            const float done = std::clamp(m_hold, 0.0f, 1.0f);

            mark({ at.x + 2.0f, at.y + 2.0f }, { (size.x - 4.0f) * done, size.y - 4.0f },
                 sf::Color(126, 32, 30, 190));
        }

        pixelfont::append(m_text, entry.label,
                          { centredX(entry.label, 2.1f), bounds.position.y + 7.0f }, 2.1f,
                          blend(config::HudDimTextColor, config::HudTextColor, lit));

        const std::string note = (picked && entry.choice == Choice::Reset)
                                     ? "HOLD ENTER TO SCUTTLE"
                                     : entry.note;

        pixelfont::append(m_text, note, { centredX(note, 1.2f), bounds.position.y + 28.0f }, 1.2f,
                          config::HudDimTextColor);
    }

    target.draw(m_marks);

    pixelfont::append(m_text, "ARROWS OR MOUSE   -   ENTER TO DIVE",
                      { centredX("ARROWS OR MOUSE   -   ENTER TO DIVE", 1.6f), 660.0f }, 1.6f,
                      config::HudDimTextColor);

    target.draw(m_text);
}
