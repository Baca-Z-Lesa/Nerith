#pragma once

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>

#include <string>

class Boss;

// Everything drawn over the fight: health, the level's objective, the state of
// whatever systems the player has bought, the creature's body, and the banners
// between levels.
class Hud
{
public:
    Hud();

    struct LevelInfo
    {
        int depth = 1;
        int levelInDepth = 1;
        int number = 1;
        std::string objective;
        float progress = 0.0f;
        bool isBoss = false;

        int   shieldCharges = 0;
        int   maxShieldCharges = 0;
        float dashReady = 1.0f; // 0..1
        bool  hasDash = false;

        // Kills without being touched. Zero means do not draw it at all.
        int   streak = 0;
        bool  streakLoud = false;
    };

    void draw(sf::RenderTarget& target, int health, int maxHealth,
              const LevelInfo& level, int salvage);

    void drawBossBar(sf::RenderTarget& target, const Boss& boss);

    // Full screen title card. alpha fades the whole thing in and out.
    void drawBanner(sf::RenderTarget& target, const std::string& label,
                    const std::string& title, const std::string& brief,
                    sf::Color accent, float alpha);

    // What one attempt at a level actually came to. Sits under the clear
    // banner, and fades with it.
    struct Summary
    {
        int   kills = 0;
        int   salvage = 0;
        int   damageTaken = 0;
        int   blocks = 0;      // hits that never landed
        int   bestStreak = 0;  // the longest run without being touched
        float seconds = 0.0f;
        float accuracy = 0.0f; // 0..1
    };

    void drawSummary(sf::RenderTarget& target, const Summary& summary, float alpha);

private:
    void drawHealth(sf::RenderTarget& target, int health, int maxHealth);
    void drawSystems(sf::RenderTarget& target, const LevelInfo& level);

    sf::CircleShape    m_pip;
    sf::CircleShape    m_small;
    sf::RectangleShape m_bar;
    sf::RectangleShape m_veil;
    sf::VertexArray    m_text;
};
