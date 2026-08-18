#include "ui/StoryScreen.h"

#include "core/Config.h"
#include "ui/PixelFont.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace
{
    constexpr float TypeSpeed = 26.0f;  // characters per second
    constexpr float LinePause = 0.55f;  // held between lines
    constexpr float EndHold = 2.2f;     // after the last one

    constexpr float FirstLineY = 300.0f;
    constexpr float LineStep = 40.0f;
    constexpr float LineScale = 2.1f;

    struct Chapter
    {
        StoryScreen::Motif motif;
        const char* title;
        const char* lines[6];
    };

    // Five beats: one before the first dive, then one for each depth cleared.
    const Chapter Chapters[] = {
        { StoryScreen::Motif::Descent, "ELEVEN DAYS",
          { "NERITH STATION STOPPED ANSWERING ELEVEN DAYS AGO.",
            "YOU ARE THE FOURTH SUB SENT DOWN.",
            "THE OTHER THREE REPORTED NOTHING WRONG.",
            "THEN THEY STOPPED REPORTING.",
            nullptr, nullptr } },

        { StoryScreen::Motif::Eyes, "IT WAS WAITING",
          { "THE THING IN THE SHALLOWS WAS NOT A FISH.",
            "IT HAD BEEN SITTING WHERE THE LIGHT ENDS.",
            "IT WAS NOT GUARDING THE STATION.",
            "IT WAS KEEPING SOMETHING IN.",
            nullptr, nullptr } },

        { StoryScreen::Motif::Reach, "THE GARDEN",
          { "THE SHELF IS NOT ROCK.",
            "IT IS BUILT. SOMETHING STACKED IT.",
            "THE STATION LOGS CALL THIS DEPTH THE GARDEN.",
            "THE LAST ENTRY IS ONE WORD. RUN.",
            nullptr, nullptr } },

        { StoryScreen::Motif::Pulse, "IT IS LEARNING",
          { "YOUR SONAR RETURNS ARE WRONG.",
            "THE FLOOR ANSWERS BEFORE THE PING ARRIVES.",
            "IT KNOWS THE SHAPE OF YOUR HULL NOW.",
            "IT IS LEARNING FASTER THAN YOU ARE.",
            nullptr, nullptr } },

        { StoryScreen::Motif::Stare, "NERITH",
          { "THE PRESSURE GAUGE READS ZERO.",
            "THERE IS NO WATER HERE. THERE IS ONLY IT.",
            "NERITH WAS NEVER A STATION.",
            "IT IS A NAME SOMETHING GAVE ITSELF.",
            "AND IT HAS BEEN AWAKE THE WHOLE TIME.",
            nullptr } },
    };

    constexpr int ChapterTotal = static_cast<int>(sizeof(Chapters) / sizeof(Chapters[0]));

    float centredX(const std::string& text, float scale)
    {
        return (static_cast<float>(config::WindowWidth) - pixelfont::measure(text, scale)) / 2.0f;
    }

    void appendQuad(sf::VertexArray& array, sf::Vector2f topLeft, sf::Vector2f size, sf::Color colour)
    {
        const sf::Vector2f a = topLeft;
        const sf::Vector2f b{ topLeft.x + size.x, topLeft.y };
        const sf::Vector2f c = topLeft + size;
        const sf::Vector2f d{ topLeft.x, topLeft.y + size.y };

        array.append(sf::Vertex{ a, colour });
        array.append(sf::Vertex{ b, colour });
        array.append(sf::Vertex{ c, colour });
        array.append(sf::Vertex{ a, colour });
        array.append(sf::Vertex{ c, colour });
        array.append(sf::Vertex{ d, colour });
    }

    // Deterministic per-index noise, so the same eye is always in the same
    // place without needing to store anything.
    float hash(int index, float salt)
    {
        const float value = std::sin(static_cast<float>(index) * 127.1f + salt * 311.7f) * 43758.5453f;
        return value - std::floor(value);
    }
}

int StoryScreen::chapterCount()
{
    return ChapterTotal;
}

StoryScreen::StoryScreen()
    : m_chapter(0)
    , m_motif(Motif::Descent)
    , m_line(0)
    , m_typed(0.0f)
    , m_time(0.0f)
    , m_hold(0.0f)
    , m_finished(true)
    , m_disc(1.0f, 40)
    , m_shapes(sf::PrimitiveType::Triangles)
    , m_text(sf::PrimitiveType::Triangles)
{
    m_veil.setSize({ static_cast<float>(config::WindowWidth),
                     static_cast<float>(config::WindowHeight) });
}

void StoryScreen::begin(int chapter)
{
    m_chapter = std::clamp(chapter, 0, ChapterTotal - 1);

    const Chapter& source = Chapters[m_chapter];
    m_motif = source.motif;
    m_title = source.title;

    m_lines.clear();
    for (const char* line : source.lines)
    {
        if (line == nullptr)
            break;
        m_lines.emplace_back(line);
    }

    m_line = 0;
    m_typed = 0.0f;
    m_time = 0.0f;
    m_hold = EndHold;
    m_finished = false;
}

void StoryScreen::update(float deltaTime)
{
    m_time += deltaTime;

    if (m_finished)
        return;

    if (m_line < m_lines.size())
    {
        m_typed += TypeSpeed * deltaTime;

        if (m_typed >= static_cast<float>(m_lines[m_line].size()) + LinePause * TypeSpeed)
        {
            ++m_line;
            m_typed = 0.0f;
        }
        return;
    }

    m_hold -= deltaTime;
    if (m_hold <= 0.0f)
        m_finished = true;
}

void StoryScreen::advance()
{
    if (m_finished)
        return;

    // Half typed: finish this line. Already whole: move to the next one, or end.
    if (m_line < m_lines.size())
    {
        const float whole = static_cast<float>(m_lines[m_line].size());
        if (m_typed < whole)
            m_typed = whole;
        else
            ++m_line;

        if (m_line >= m_lines.size())
            m_hold = 0.6f;

        m_typed = 0.0f;
        return;
    }

    m_finished = true;
}

// ---------------------------------------------------------------------------
//  Motifs
// ---------------------------------------------------------------------------

void StoryScreen::drawDescent(sf::RenderTarget& target)
{
    // Streaks rushing upward past the viewport. Nothing is rising: you are
    // going down, and fast.
    m_shapes.clear();

    for (int i = 0; i < 150; ++i)
    {
        const float speed = 180.0f + hash(i, 1.0f) * 620.0f;
        const float x = hash(i, 2.0f) * static_cast<float>(config::WindowWidth);
        const float span = static_cast<float>(config::WindowHeight) + 260.0f;

        float y = std::fmod(hash(i, 3.0f) * span + m_time * speed, span);
        y = static_cast<float>(config::WindowHeight) - y;

        const float length = 12.0f + hash(i, 4.0f) * 60.0f;
        const auto alpha = static_cast<std::uint8_t>(26.0f + hash(i, 5.0f) * 70.0f);

        appendQuad(m_shapes, { x, y }, { 1.0f + hash(i, 6.0f) * 2.0f, length },
                   sf::Color(150, 200, 210, alpha));
    }

    target.draw(m_shapes);
}

void StoryScreen::drawEyes(sf::RenderTarget& target)
{
    // Pairs open, hold on you, blink, and fade. Never all at once, so the dark
    // between them never feels empty.
    for (int i = 0; i < 9; ++i)
    {
        const float cycle = 5.0f + hash(i, 7.0f) * 4.0f;
        const float phase = std::fmod(m_time + hash(i, 8.0f) * cycle, cycle) / cycle;

        // Fade in, hold, fade out.
        const float presence = std::min({ phase / 0.25f, 1.0f, (1.0f - phase) / 0.25f });
        if (presence <= 0.0f)
            continue;

        const float x = 90.0f + hash(i, 9.0f) * (config::WindowWidth - 180.0f);
        const float y = 90.0f + hash(i, 10.0f) * (config::WindowHeight - 180.0f);
        const float size = 9.0f + hash(i, 11.0f) * 13.0f;

        // A quick blink partway through: the lid squashes the eye to nothing.
        const float blinkAt = 0.55f + hash(i, 12.0f) * 0.2f;
        float lid = 1.0f;
        if (std::abs(phase - blinkAt) < 0.035f)
            lid = std::abs(phase - blinkAt) / 0.035f;

        const auto alpha = static_cast<std::uint8_t>(presence * 210.0f);

        for (const float side : { -1.0f, 1.0f })
        {
            m_disc.setRadius(size);
            m_disc.setOrigin({ size, size });
            m_disc.setScale({ 1.0f, std::max(0.05f, lid) });
            m_disc.setPosition({ x + side * size * 1.9f, y });
            m_disc.setFillColor(sf::Color(210, 160, 80, alpha));
            target.draw(m_disc);

            m_disc.setRadius(size * 0.42f);
            m_disc.setOrigin({ size * 0.42f, size * 0.42f });
            m_disc.setFillColor(sf::Color(10, 6, 8, alpha));
            target.draw(m_disc);
        }
    }

    m_disc.setScale({ 1.0f, 1.0f });
}

void StoryScreen::drawReach(sf::RenderTarget& target)
{
    // Tendrils growing in from every edge, always a little further than the
    // last time you looked.
    m_shapes.clear();

    const float grown = std::min(1.0f, m_time / 9.0f);

    for (int i = 0; i < 26; ++i)
    {
        const bool vertical = hash(i, 13.0f) < 0.5f;
        const bool far = hash(i, 14.0f) < 0.5f;

        const float along = hash(i, 15.0f) * (vertical ? config::WindowWidth : config::WindowHeight);
        const float reach = (90.0f + hash(i, 16.0f) * 260.0f) * grown;

        const int steps = 16;
        for (int s = 0; s < steps; ++s)
        {
            const float t = static_cast<float>(s) / steps;
            const float depth = reach * t;
            const float wobble = std::sin(m_time * (0.6f + hash(i, 17.0f)) + t * 5.0f) * 22.0f * t;
            const float thick = 7.0f * (1.0f - t) + 1.0f;

            sf::Vector2f point;
            if (vertical)
                point = { along + wobble, far ? config::WindowHeight - depth : depth };
            else
                point = { far ? config::WindowWidth - depth : depth, along + wobble };

            appendQuad(m_shapes, point - sf::Vector2f{ thick, thick },
                       { thick * 2.0f, thick * 2.0f },
                       sf::Color(24, 10, 30, static_cast<std::uint8_t>(230 * (1.0f - t * 0.5f))));
        }
    }

    target.draw(m_shapes);
}

void StoryScreen::drawPulse(sf::RenderTarget& target)
{
    // Two quick beats and a rest, the frame tightening on every one.
    const float beat = std::fmod(m_time, 1.5f);
    const float thump = std::exp(-beat * 6.0f) + std::exp(-std::abs(beat - 0.26f) * 9.0f);

    m_shapes.clear();

    const int rings = 26;
    for (int i = 0; i < rings; ++i)
    {
        const float t = static_cast<float>(i) / rings;
        const float inset = t * (150.0f + thump * 70.0f);
        const auto alpha = static_cast<std::uint8_t>(9.0f + 9.0f * thump);

        appendQuad(m_shapes, { inset, inset },
                   { static_cast<float>(config::WindowWidth) - inset * 2.0f, 3.0f },
                   sf::Color(120, 16, 22, alpha));
        appendQuad(m_shapes, { inset, static_cast<float>(config::WindowHeight) - inset - 3.0f },
                   { static_cast<float>(config::WindowWidth) - inset * 2.0f, 3.0f },
                   sf::Color(120, 16, 22, alpha));
        appendQuad(m_shapes, { inset, inset }, { 3.0f, static_cast<float>(config::WindowHeight) - inset * 2.0f },
                   sf::Color(120, 16, 22, alpha));
        appendQuad(m_shapes, { static_cast<float>(config::WindowWidth) - inset - 3.0f, inset },
                   { 3.0f, static_cast<float>(config::WindowHeight) - inset * 2.0f },
                   sf::Color(120, 16, 22, alpha));
    }

    target.draw(m_shapes);
}

void StoryScreen::drawStare(sf::RenderTarget& target)
{
    // One eye, far too large, taking most of a minute to finish opening.
    const sf::Vector2f centre{ config::WindowWidth / 2.0f, config::WindowHeight / 2.0f };
    const float open = std::min(1.0f, m_time / 7.0f);
    const float radius = 250.0f;

    // The lid is the gap between two dark slabs sliding apart.
    const float gap = radius * open;

    m_disc.setScale({ 1.0f, std::max(0.02f, open) });
    m_disc.setRadius(radius);
    m_disc.setOrigin({ radius, radius });
    m_disc.setPosition(centre);
    m_disc.setFillColor(sf::Color(70, 22, 14, 220));
    target.draw(m_disc);

    m_disc.setRadius(radius * 0.62f);
    m_disc.setOrigin({ radius * 0.62f, radius * 0.62f });
    m_disc.setFillColor(sf::Color(220, 140, 50, 235));
    target.draw(m_disc);

    // The pupil drifts, then locks on and shrinks. It has found you.
    const float lock = std::clamp((m_time - 9.0f) / 3.0f, 0.0f, 1.0f);
    const float wander = (1.0f - lock) * 46.0f;
    const float pupil = radius * (0.30f - 0.16f * lock);

    m_disc.setScale({ 1.0f, std::max(0.02f, open) });
    m_disc.setRadius(pupil);
    m_disc.setOrigin({ pupil, pupil });
    m_disc.setPosition(centre + sf::Vector2f{ std::sin(m_time * 0.7f) * wander,
                                              std::cos(m_time * 0.5f) * wander * 0.4f });
    m_disc.setFillColor(sf::Color(6, 3, 4, 255));
    target.draw(m_disc);

    m_disc.setScale({ 1.0f, 1.0f });
    (void)gap;
}

void StoryScreen::drawMotif(sf::RenderTarget& target)
{
    switch (m_motif)
    {
        case Motif::Descent: drawDescent(target); break;
        case Motif::Eyes:    drawEyes(target); break;
        case Motif::Reach:   drawReach(target); break;
        case Motif::Pulse:   drawPulse(target); break;
        case Motif::Stare:   drawStare(target); break;
    }
}

void StoryScreen::draw(sf::RenderTarget& target)
{
    m_veil.setFillColor(sf::Color(2, 4, 7, 255));
    target.draw(m_veil);

    drawMotif(target);

    // A band behind the words so the motif never eats them.
    m_bar.setSize({ static_cast<float>(config::WindowWidth), 260.0f });
    m_bar.setPosition({ 0.0f, FirstLineY - 78.0f });
    m_bar.setFillColor(sf::Color(2, 4, 7, 214));
    target.draw(m_bar);

    m_text.clear();

    pixelfont::append(m_text, m_title, { centredX(m_title, 3.4f), FirstLineY - 62.0f }, 3.4f,
                      config::HudBossColor);

    for (std::size_t i = 0; i < m_lines.size(); ++i)
    {
        const std::string& line = m_lines[i];
        const float y = FirstLineY + LineStep * static_cast<float>(i);

        if (i < m_line)
        {
            // Already read: dimmed, so the eye lands on the newest line.
            pixelfont::append(m_text, line, { centredX(line, LineScale), y }, LineScale,
                              config::HudDimTextColor);
        }
        else if (i == m_line)
        {
            const std::size_t shown = std::min<std::size_t>(
                line.size(), static_cast<std::size_t>(std::max(0.0f, m_typed)));

            const std::string visible = line.substr(0, shown);
            pixelfont::append(m_text, visible, { centredX(line, LineScale), y }, LineScale,
                              config::HudTextColor);
        }
    }

    if (m_line >= m_lines.size())
    {
        const char* prompt = "SPACE";
        pixelfont::append(m_text, prompt, { centredX(prompt, 1.6f), 620.0f }, 1.6f,
                          config::HudDimTextColor);
    }

    target.draw(m_text);
}
