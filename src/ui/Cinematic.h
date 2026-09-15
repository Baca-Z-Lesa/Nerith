#pragma once

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <string>
#include <vector>

struct Assets;

// The two things that bracket the game, and the two short rides between one
// floor and the next.
//
// What was here before the long pair was lore: five screens of typed prose
// between depths, which is a wall of text with a wobbling background behind
// it. Nobody reads the fifth one. So there are two, they are at the ends -
// where a player is actually willing to sit still - and neither of them is
// words over a pattern. They are shots.
//
// Every shot is built in three depth bands. There are no shaders here, so a
// foreground cannot be blurred, which means a foreground can only ever be a
// large near-black silhouette moving faster than everything behind it - and
// that parallax is the only depth a flat scene gets. BACK moves at a quarter
// of the camera, MID at one, FORE at two and a bit. Every shot names all
// three, because a shot with only a middle is a painting.
//
// Any key skips the whole thing. Always, from the first frame, with the way
// out printed in the corner: the second time a player sees a cutscene it is an
// obstacle, and a cutscene you cannot leave is the most resented thing a game
// has.
class Cinematic
{
public:
    enum class Reel
    {
        Descent, // before the first dive
        Surface, // after the thing at the bottom stops getting up

        // The two short ones, played every single time rather than once ever:
        // the ride down the shaft and the ride back up to the deck. They are a
        // beat rather than a story, which is the only way a transition a
        // player will see fifty times is allowed to be a cutscene at all.
        Down,
        Up,
    };

    explicit Cinematic(const Assets& assets);

    // caption replaces the reel's own line when it is given, which is how the
    // short reels say which floor they are arriving at.
    void begin(Reel reel, const std::string& caption = {});
    void update(float deltaTime);

    // Any key, any time. There is no "hold to skip" and no unskippable first
    // three seconds.
    void skip();

    bool isFinished() const { return m_finished; }

    // Which sting the shot that just came up wants under it, or Count for a
    // shot that wants nothing. The reel owns the cutting and has no business
    // owning the mixer, so it reports the cut and the caller plays it.
    enum class Sting { Horn, Fall, Climb, Dawn, Mark, Count };

    Sting takeSting();

    void draw(sf::RenderTarget& target);

private:
    // What a shot is a picture of. Six for the way down and five for the way
    // back up, where there used to be eight and six: fewer and longer, so a
    // shot has time to have something happen in it rather than only being a
    // thing that is shown.
    enum class Scene
    {
        Rig,      // the rig at night, in the rain, being signed for
        Shutter,  // the daylight closes over the bell
        Sounding, // the fall stops, and three dead boats come up past you
        Nerith,   // the station, whole, with its doors open
        Trench,   // the floor comes apart and there is something in it
        Mark,     // the title, resolving out of the dark

        Ascent,   // the floor opens under you and everything leaves at once
        Daylight, // the surface, from underneath, warm for the first time
        Count,    // the deck, and the hold being weighed and paid for
        Wake,     // and what came up on its own

        // The two transits, which are not part of either story.
        Fall,
        Climb,
    };

    struct Shot
    {
        Scene       scene;
        float       seconds;
        const char* line; // null on a title card
    };

    // What a scene sounds like when it arrives.
    static Sting stingFor(Scene scene);

    const Shot& current() const;
    float shotTime() const; // 0..1 through the shot on screen

    void drawScene(sf::RenderTarget& target);

    void drawRig(sf::RenderTarget& target, float t);
    void drawShutter(sf::RenderTarget& target, float t);
    void drawSounding(sf::RenderTarget& target, float t);
    void drawNerith(sf::RenderTarget& target, float t);
    void drawTrench(sf::RenderTarget& target, float t);
    void drawMark(sf::RenderTarget& target, float t);
    void drawAscent(sf::RenderTarget& target, float t);
    void drawDaylight(sf::RenderTarget& target, float t);
    void drawCount(sf::RenderTarget& target, float t);
    void drawWake(sf::RenderTarget& target, float t);
    void drawFall(sf::RenderTarget& target, float t);
    void drawClimb(sf::RenderTarget& target, float t);

    // The shared vocabulary. Everything drawn here that is not a plate is one
    // of these.
    void disc(sf::Vector2f at, float radius, sf::Color colour, int steps = 20);

    // A light rather than a circle: bright in the middle and gone at the rim.
    // A flat disc at low alpha is a brown ball hanging in the shot, which is
    // what the halos were until this existed.
    void glow(sf::Vector2f at, float radius, sf::Color colour, float strength,
              int steps = 34);
    void bar(sf::Vector2f from, sf::Vector2f to, float width, sf::Color a, sf::Color b);
    void wedge(sf::Vector2f from, sf::Vector2f along, float length, float spread,
               sf::Color core, sf::Color edge);
    void hull(sf::Vector2f at, float size, float lean, sf::Color colour);

    // A vertical gradient across the whole frame: the sky, the water or the
    // dark, depending on which two colours it is given.
    void wash(sf::Color top, sf::Color bottom);

    // Rain, silt, bubbles. The same falling motes with a different direction
    // and a different colour.
    void motes(float phase, sf::Vector2f drift, float length, sf::Color colour, int count);

    // Whatever is in the mesh, out to the card, and the mesh emptied. Plates
    // are sprites and so cannot be batched with it: anything drawn before one
    // has to be flushed before it, or it lands on top.
    void flush(sf::RenderTarget& target);

    // One plate, placed. Everything painted goes through here so that a shot
    // never has to think about origins.
    void plate(sf::RenderTarget& target, sf::Sprite& sprite, sf::Vector2f at, float scale,
               sf::Color tint, float turn = 0.0f);

    // Where a point on a plate lands on the screen once the plate has been put
    // somewhere. Without this, a lamp painted over a window is a pair of magic
    // numbers that go wrong the moment the camera moves - which is what every
    // halo in the first version of these reels was.
    sf::Vector2f onPlate(const sf::Sprite& sprite, sf::Vector2f at, float scale, float turn,
                         sf::Vector2f uv) const;

    // The rock face, laid across the screen and scrolled. offset is in pixels
    // and may be any size: the texture repeats.
    void rock(sf::RenderTarget& target, float offset, float scale, sf::Color tint);

    // What a plate was painted at against what its shots were framed against.
    // Keyed off the sprite itself so that growing a plate is a one line change
    // in the config and nothing in the shots has to move.
    float fitOf(const sf::Sprite& sprite) const;

    std::vector<Shot> m_shots;
    std::size_t       m_shot = 0;
    float             m_time = 0.0f;   // into the current shot
    float             m_total = 0.0f;  // into the whole reel, for drifting things
    bool              m_finished = true;

    // How far the rock has scrolled. Kept rather than worked out from the
    // clock, because the one shot that matters stops the fall dead in the
    // middle of itself, and a scroll cut from m_total cannot stop.
    float m_scroll = 0.0f;

    // Only the short reels use this: it stands in for the first shot's line.
    std::string m_caption;

    // What the shot on screen asked for when it started, taken once.
    Sting m_sting = Sting::Count;

    sf::RectangleShape m_fill;
    sf::VertexArray    m_mesh;
    sf::VertexArray    m_text;

    sf::Sprite m_rig;
    sf::Sprite m_bell;
    sf::Sprite m_wall;
    sf::Sprite m_station;
    sf::Sprite m_beast;
    sf::Sprite m_diver;
    sf::Sprite m_crate;
    sf::Sprite m_ledge;
    sf::Sprite m_wreck;
    sf::Sprite m_hatch;
};
