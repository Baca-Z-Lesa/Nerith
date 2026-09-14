#pragma once

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <string>
#include <vector>

// The two things that bracket the game.
//
// What was here before was lore: five screens of typed prose between depths,
// which is a wall of text with a wobbling background behind it. Nobody reads
// the fifth one. So there are two now and they are at the ends, where a player
// is actually willing to sit still - one on the way down and one on the way
// back up - and neither of them is words over a pattern. They are shots.
//
// Every shot is drawn rather than written: a cable paying out, rock going past,
// something enormous crossing the beam, a floor coming open. The line of text
// underneath is a caption on a picture and not the other way round.
//
// Any key skips the whole thing. Always, from the first frame, with the way out
// printed in the corner - because the second time a player sees a cutscene it
// is an obstacle, and a cutscene you cannot leave is the most resented thing a
// game has.
class Cinematic
{
public:
    enum class Reel
    {
        Descent, // before the first dive
        Surface, // after the thing at the bottom stops getting up
    };

    Cinematic();

    void begin(Reel reel);
    void update(float deltaTime);

    // Any key, any time. There is no "hold to skip" and no unskippable first
    // three seconds.
    void skip();

    bool isFinished() const { return m_finished; }

    void draw(sf::RenderTarget& target);

private:
    // What a shot is a picture of. Each one is a handful of shapes with the
    // shot's own clock driving them.
    enum class Scene
    {
        Winch,   // the cable pays out and the daylight above closes up
        Fall,    // rock going past, faster than is comfortable
        Pass,    // something crosses the beam and the beam stops existing
        Station, // a dead structure with one light left in it
        Mark,    // the title, resolving out of the dark
        Hatch,   // the floor of the trench opening
        Climb,   // rising, through everything that came out of it
        Dawn,    // the surface, from underneath
        Wake,    // and what followed you up
    };

    struct Shot
    {
        Scene       scene;
        float       seconds;
        const char* line; // null on a title card
    };

    const Shot& current() const;
    float shotTime() const; // 0..1 through the shot on screen

    void drawScene(sf::RenderTarget& target);
    void drawWinch(float t);
    void drawFall(float t);
    void drawPass(float t);
    void drawStation(float t);
    void drawMark(float t);
    void drawHatch(float t);
    void drawClimb(float t);
    void drawDawn(float t);
    void drawWake(float t);

    // The shared vocabulary. Everything in here is one of these.
    void disc(sf::Vector2f at, float radius, sf::Color colour, int steps = 20);
    void bar(sf::Vector2f from, sf::Vector2f to, float width, sf::Color a, sf::Color b);
    void wedge(sf::Vector2f from, sf::Vector2f along, float length, float spread,
               sf::Color core, sf::Color edge);
    void hull(sf::Vector2f at, float size, float lean, sf::Color colour);

    std::vector<Shot> m_shots;
    std::size_t       m_shot = 0;
    float             m_time = 0.0f;   // into the current shot
    float             m_total = 0.0f;  // into the whole reel, for drifting things
    bool              m_finished = true;

    sf::RectangleShape m_fill;
    sf::VertexArray    m_mesh;
    sf::VertexArray    m_text;
};
