#include "ui/Hud.h"

#include "core/Config.h"
#include "entities/Boss.h"
#include "ui/PixelFont.h"
#include "world/LevelDef.h"

#include <algorithm>
#include <cstdint>

namespace
{
    sf::Color withAlpha(sf::Color color, float alpha)
    {
        color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
        return color;
    }

    float centredX(const std::string& text, float scale)
    {
        return (static_cast<float>(config::WindowWidth) - pixelfont::measure(text, scale)) / 2.0f;
    }

    // The colour of a body part says what breaking it will cost the creature.
    sf::Color colourFor(PartRole role)
    {
        switch (role)
        {
            case PartRole::Eye:      return sf::Color(255, 190, 90);
            case PartRole::Fin:      return sf::Color(120, 220, 210);
            case PartRole::Tentacle: return sf::Color(200, 160, 255);
            case PartRole::Armour:   return sf::Color(170, 185, 200);
            default:                 return config::HudBossColor;
        }
    }

    const char* labelFor(PartRole role)
    {
        switch (role)
        {
            case PartRole::Eye:      return "EYE";
            case PartRole::Fin:      return "FIN";
            case PartRole::Tentacle: return "ARM";
            case PartRole::Armour:   return "PLATE";
            default:                 return "ORGAN";
        }
    }
}

Hud::Hud()
    : m_pip(config::HudPipRadius, 6) // hexagons read as instrument panel lights
    , m_small(5.0f, 6)
    , m_text(sf::PrimitiveType::Triangles)
{
    m_pip.setOrigin({ config::HudPipRadius, config::HudPipRadius });
    m_pip.setOutlineThickness(2.0f);

    m_small.setOrigin({ 5.0f, 5.0f });
    m_small.setOutlineThickness(1.5f);

    m_veil.setSize({ static_cast<float>(config::WindowWidth),
                     static_cast<float>(config::WindowHeight) });
}

void Hud::drawHealth(sf::RenderTarget& target, int health, int maxHealth)
{
    // A long enough row of pips would run into the objective bar, so past a
    // point they tighten up instead of spreading further.
    const float spacing = std::min(config::HudPipSpacing,
                                   340.0f / static_cast<float>(std::max(1, maxHealth)));

    for (int i = 0; i < maxHealth; ++i)
    {
        const bool filled = i < health;

        m_pip.setFillColor(filled ? config::HudPipFullColor : config::HudPipEmptyColor);
        m_pip.setOutlineColor(filled ? config::HudPipFullEdge : config::HudPipEmptyEdge);
        m_pip.setPosition({ config::HudMargin + static_cast<float>(i) * spacing,
                            config::HudMargin });

        target.draw(m_pip);
    }
}

void Hud::drawSystems(sf::RenderTarget& target, const LevelInfo& level)
{
    float y = config::HudMargin + 24.0f;

    // Shield charges sit under the hull pips, because they are read together.
    for (int i = 0; i < level.maxShieldCharges; ++i)
    {
        const bool up = i < level.shieldCharges;
        m_small.setFillColor(up ? config::ShieldColor : config::HudPipEmptyColor);
        m_small.setOutlineColor(up ? config::HudPipFullEdge : config::HudPipEmptyEdge);
        m_small.setPosition({ config::HudMargin + static_cast<float>(i) * 16.0f, y });
        target.draw(m_small);
    }

    if (!level.hasDash)
        return;

    if (level.maxShieldCharges > 0)
        y += 18.0f;

    m_bar.setSize({ 70.0f, 4.0f });
    m_bar.setPosition({ config::HudMargin - 8.0f, y - 2.0f });
    m_bar.setFillColor(config::HudBarBackColor);
    target.draw(m_bar);

    m_bar.setSize({ 70.0f * std::clamp(level.dashReady, 0.0f, 1.0f), 4.0f });
    m_bar.setFillColor(level.dashReady >= 1.0f ? config::HudPipFullColor : config::HudDimTextColor);
    target.draw(m_bar);
}

void Hud::draw(sf::RenderTarget& target, int health, int maxHealth,
               const LevelInfo& level, int salvage)
{
    drawHealth(target, health, maxHealth);
    drawSystems(target, level);

    m_text.clear();

    const std::string place = "DEPTH " + std::to_string(level.depth) + "  LEVEL "
                              + std::to_string(level.levelInDepth) + "/"
                              + std::to_string(LevelsPerDepth);
    pixelfont::append(m_text, place, { centredX(place, 1.6f), config::HudMargin - 22.0f },
                      1.6f, config::HudDimTextColor);

    const sf::Color accent = level.isBoss ? config::HudBossColor : config::HudTextColor;
    pixelfont::append(m_text, level.objective,
                      { centredX(level.objective, config::HudTextScale), config::HudMargin - 2.0f },
                      config::HudTextScale, accent);

    const std::string salvageLine = "SALVAGE " + std::to_string(salvage);
    pixelfont::append(m_text, salvageLine,
                      { static_cast<float>(config::WindowWidth) - config::HudMargin
                            - pixelfont::measure(salvageLine, config::HudTextScale),
                        config::HudMargin - 12.0f },
                      config::HudTextScale, config::HudSalvageColor);

    // What is in the hold and not yet in the bank. Drowning loses exactly
    // this, so it has to be on the screen the whole time rather than only in
    // the summary afterwards.
    if (level.held > 0)
    {
        const std::string held = "+" + std::to_string(level.held) + " IN THE HOLD";
        pixelfont::append(m_text, held,
                          { static_cast<float>(config::WindowWidth) - config::HudMargin
                                - pixelfont::measure(held, 1.5f),
                            config::HudMargin + 12.0f },
                          1.5f, config::HudBossColor);
    }
    else
    {
        pixelfont::append(m_text, "TAB UPGRADES",
                          { static_cast<float>(config::WindowWidth) - config::HudMargin
                                - pixelfont::measure("TAB UPGRADES", 1.5f),
                            config::HudMargin + 12.0f },
                          1.5f, config::HudDimTextColor);
    }

    // Momentum, once it is worth showing. Gold at first, then the boss colour
    // once it is long enough that losing it would actually hurt.
    if (level.streak > 0)
    {
        const std::string run = "UNTOUCHED x" + std::to_string(level.streak);
        const float scale = level.streakLoud ? 2.4f : 1.8f;

        pixelfont::append(m_text, run, { centredX(run, scale), 118.0f }, scale,
                          level.streakLoud ? config::HudBossColor : config::HudSalvageColor);

        // What it is worth, under it. Without the number the streak is a thing
        // to notice; with it, it is a thing to protect.
        if (level.streakBonus > 0)
        {
            const std::string paid = "+" + std::to_string(level.streakBonus) + "% SALVAGE";
            pixelfont::append(m_text, paid, { centredX(paid, 1.4f), 118.0f + scale * 9.0f },
                              1.4f, config::HudSalvageColor);
        }
    }

    target.draw(m_text);

    if (!level.isBoss)
    {
        const sf::Vector2f origin{ (static_cast<float>(config::WindowWidth) - config::HudBarWidth) / 2.0f,
                                   config::HudMargin + 24.0f };

        m_bar.setSize({ config::HudBarWidth, config::HudBarHeight });
        m_bar.setPosition(origin);
        m_bar.setFillColor(config::HudBarBackColor);
        target.draw(m_bar);

        m_bar.setSize({ config::HudBarWidth * std::clamp(level.progress, 0.0f, 1.0f),
                        config::HudBarHeight });
        m_bar.setFillColor(config::HudBarFillColor);
        target.draw(m_bar);
    }
}

void Hud::drawBossBar(sf::RenderTarget& target, const Boss& boss)
{
    const float left = (static_cast<float>(config::WindowWidth) - config::BossBarWidth) / 2.0f;

    // What is left of the animal itself. It reads dull while anything is still
    // shielding the organs, which is the one thing the player must understand.
    const bool reachable = boss.isVulnerable();

    m_bar.setSize({ config::BossBarWidth, config::BossBarHeight });
    m_bar.setPosition({ left, config::BossBarTop });
    m_bar.setFillColor(config::HudBarBackColor);
    target.draw(m_bar);

    m_bar.setSize({ config::BossBarWidth * boss.getCoreFraction(), config::BossBarHeight });
    m_bar.setFillColor(reachable ? config::HudBossColor : config::BossSealedTint);
    target.draw(m_bar);

    // One pip per body part, coloured by what losing it takes away. Organs are
    // already the bar above, so they are not repeated here.
    std::size_t shown = 0;
    for (std::size_t i = 0; i < boss.getPartCount(); ++i)
    {
        if (boss.getPartRole(i) != PartRole::Organ)
            ++shown;
    }

    if (shown > 0)
    {
        const float gap = 8.0f;
        const float width = (config::BossBarWidth - gap * static_cast<float>(shown - 1))
                            / static_cast<float>(shown);
        std::size_t slot = 0;

        for (std::size_t i = 0; i < boss.getPartCount(); ++i)
        {
            const PartRole role = boss.getPartRole(i);
            if (role == PartRole::Organ)
                continue;

            const float x = left + static_cast<float>(slot) * (width + gap);
            ++slot;

            m_bar.setSize({ width, 6.0f });
            m_bar.setPosition({ x, config::BossBarTop + config::BossBarHeight + 6.0f });
            m_bar.setFillColor(config::HudBarBackColor);
            target.draw(m_bar);

            if (!boss.isPartAlive(i))
                continue;

            m_bar.setSize({ width * boss.getPartFraction(i), 6.0f });
            m_bar.setFillColor(colourFor(role));
            target.draw(m_bar);
        }
    }

    m_text.clear();

    const std::string name = boss.getName();
    pixelfont::append(m_text, name, { centredX(name, 1.8f), config::BossBarTop - 22.0f },
                      1.8f, config::HudBossColor);

    const std::string state = boss.getStateLabel();
    pixelfont::append(m_text, state, { centredX(state, 1.6f), config::BossBarTop + 44.0f },
                      1.6f, reachable ? config::HudBossColor : config::HudDimTextColor);

    // A legend for the pips, so the colours mean something the first time.
    float x = left;
    const float gap = 8.0f;
    if (shown > 0)
    {
        const float width = (config::BossBarWidth - gap * static_cast<float>(shown - 1))
                            / static_cast<float>(shown);
        for (std::size_t i = 0; i < boss.getPartCount(); ++i)
        {
            const PartRole role = boss.getPartRole(i);
            if (role == PartRole::Organ)
                continue;

            const char* text = labelFor(role);
            pixelfont::append(m_text, text,
                              { x + (width - pixelfont::measure(text, 1.2f)) / 2.0f,
                                config::BossBarTop + config::BossBarHeight + 15.0f },
                              1.2f, boss.isPartAlive(i) ? colourFor(role) : config::HudPipEmptyEdge);
            x += width + gap;
        }
    }

    target.draw(m_text);
}

void Hud::drawBanner(sf::RenderTarget& target, const std::string& label,
                     const std::string& title, const std::string& brief,
                     sf::Color accent, float alpha)
{
    sf::Color veil = config::BannerVeil;
    veil.a = static_cast<std::uint8_t>(static_cast<float>(veil.a) * std::clamp(alpha, 0.0f, 1.0f));
    m_veil.setFillColor(veil);
    target.draw(m_veil);

    m_text.clear();

    // Long creature names would run off the edge at the full title size.
    const float titleScale = pixelfont::measure(title, config::BannerTitleScale)
                                     > static_cast<float>(config::WindowWidth) - 80.0f
                                 ? config::BannerTitleScale * 0.62f
                                 : config::BannerTitleScale;

    float y = 250.0f;
    pixelfont::append(m_text, label, { centredX(label, config::BannerLabelScale), y },
                      config::BannerLabelScale, withAlpha(config::HudDimTextColor, alpha));

    y += 42.0f;
    pixelfont::append(m_text, title, { centredX(title, titleScale), y },
                      titleScale, withAlpha(accent, alpha));

    y += 68.0f;
    pixelfont::append(m_text, brief, { centredX(brief, config::BannerBriefScale), y },
                      config::BannerBriefScale, withAlpha(config::HudDimTextColor, alpha));

    target.draw(m_text);
}

void Hud::drawSummary(sf::RenderTarget& target, const Summary& summary, float alpha)
{
    m_text.clear();

    // Two columns of label and value, so the numbers line up under each other
    // instead of wandering with the width of the words.
    const std::string rows[7][2] = {
        { "KILLS",   std::to_string(summary.kills) },
        { "SALVAGE", "+" + std::to_string(summary.salvage) },
        { "TIME",    std::to_string(static_cast<int>(summary.seconds)) + "S" },
        { "ACCURACY", std::to_string(static_cast<int>(summary.accuracy * 100.0f + 0.5f)) + "%" },
        { "BEST RUN", "x" + std::to_string(summary.bestStreak) },
        { "BLOCKED", std::to_string(summary.blocks) },
        { "DAMAGE TAKEN", std::to_string(summary.damageTaken) },
    };

    const float scale = 1.7f;
    const float labelX = static_cast<float>(config::WindowWidth) / 2.0f - 150.0f;
    const float valueX = static_cast<float>(config::WindowWidth) / 2.0f + 120.0f;

    float y = 404.0f;
    for (const auto& row : rows)
    {
        pixelfont::append(m_text, row[0], { labelX, y }, scale,
                          withAlpha(config::HudDimTextColor, alpha));

        // Right aligned against a fixed column.
        pixelfont::append(m_text, row[1],
                          { valueX - pixelfont::measure(row[1], scale), y }, scale,
                          withAlpha(row[0] == "DAMAGE TAKEN" && summary.damageTaken == 0
                                        ? config::HudPipFullEdge
                                        : config::HudTextColor,
                                    alpha));
        y += 26.0f;
    }

    target.draw(m_text);
}
