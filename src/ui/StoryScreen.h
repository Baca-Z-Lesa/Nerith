#pragma once

#include <SFML/Graphics/CircleShape.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <string>
#include <vector>

// The interludes between depths. Text arrives a letter at a time over a moving
// backdrop that gets worse the further down the chapter sits, so the story is
// something the player watches rather than a wall of prose to click past.
class StoryScreen
{
public:
    // What is crawling around behind the words.
    enum class Motif
    {
        Descent, // streaks rushing upward: you are falling
        Eyes,    // pairs of eyes opening in the dark and blinking at you
        Reach,   // tendrils growing in from every edge
        Pulse,   // a heartbeat closing the frame in
        Stare,   // one enormous eye opening in the middle
    };

    static int chapterCount();

    StoryScreen();

    void begin(int chapter);
    void update(float deltaTime);

    // Space or click: finish the current line instantly, or move on if it is
    // already whole. Nobody should be trapped watching text they have read.
    void advance();

    bool isFinished() const { return m_finished; }

    void draw(sf::RenderTarget& target);

private:
    void drawMotif(sf::RenderTarget& target);
    void drawDescent(sf::RenderTarget& target);
    void drawEyes(sf::RenderTarget& target);
    void drawReach(sf::RenderTarget& target);
    void drawPulse(sf::RenderTarget& target);
    void drawStare(sf::RenderTarget& target);

    int         m_chapter;
    Motif       m_motif;
    std::string m_title;
    std::vector<std::string> m_lines;

    std::size_t m_line;     // which line is being typed
    float       m_typed;    // characters revealed on that line
    float       m_time;     // seconds since the chapter opened
    float       m_hold;     // pause after the last line
    bool        m_finished;

    sf::RectangleShape m_veil;
    sf::RectangleShape m_bar;
    sf::CircleShape    m_disc;
    sf::VertexArray    m_shapes;
    sf::VertexArray    m_text;
};
