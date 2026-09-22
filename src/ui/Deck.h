#pragma once

#include "entities/ShipVisuals.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

struct Assets;
class Stats;

// The surface. Everything between two dives happens up here, and it is a place
// rather than a menu: a rig deck in a black sea at night, in the rain, with a
// man in a suit walking about on it under five floodlights.
//
// Three things on it are worth walking to. The crane buys what the hold came
// up with, the bench is the tree, and the bell is the way back down. Which one
// you are standing at is all this class decides; what standing there means is
// the caller's business, so the deck owns no money, no tree and no save.
//
// The night is built out of two meshes rather than one. m_mesh is everything
// that covers up - the sea, the plate markings, the puddles, the shadows, the
// vignette - and m_glow is everything that adds, drawn with sf::BlendAdd the
// same way the arena draws its lamp. Light that paints over the deck instead
// of adding to it is a grey circle, and a grey circle is what the first
// version of this looked like.
class Deck
{
public:
    enum class Post
    {
        Crane, // sell the hold
        Bench, // the upgrade tree
        Bell,  // the way down
        Count,
    };

    explicit Deck(const Assets& assets);

    // Come up the shaft. Puts the diver by the bell, facing the deck.
    void enter();

    // drive is the movement axis, each component -1 to 1.
    void update(float deltaTime, sf::Vector2f drive);
    void draw(sf::RenderTarget& target);

    // What the prompt is about, or Count when he is standing in the middle of
    // nothing.
    Post standingAt() const { return m_at; }

    // True on the frame a boot lands, so the caller can put a sound under it.
    // The deck owns the walk cycle and has no business owning the mixer, so it
    // reports the step rather than playing one.
    bool tookStep() const { return m_stepped; }

    // Where he is. Nothing in the game asks; the headless preview does, and a
    // getter is cheaper than a preview that has to guess.
    sf::Vector2f getDiver() const { return m_diver; }

    // One line under a station's name: what it is offering right now. The
    // numbers belong to the caller, so they are handed in rather than worked
    // out here.
    void setNote(Post which, std::string note);

    // And whether it can be used at all. A shut station still shows its note -
    // "NOTHING IN THE HOLD" is the answer to the question, not the absence of
    // one - but it is drawn dead and the prompt does not appear.
    void setOpen(Post which, bool open);

    // The sub parked on the deck is the player's own: it grows the same
    // hardware their stat block has fitted. Handed in rather than owned, so the
    // deck still knows nothing about the tree - it only draws what it is given.
    // Call whenever the fitting could have changed (surfacing, buying).
    void setSub(const Stats& stats);

private:
    struct Station
    {
        sf::Vector2f at;
        float        solid;  // how much of it you cannot walk through
        const char*  name;
        const char*  verb;
        std::string  note;
        bool         open = true;
    };

    Station&       station(Post which) { return m_stations[static_cast<std::size_t>(which)]; }
    const Station& station(Post which) const
    {
        return m_stations[static_cast<std::size_t>(which)];
    }

    void keepOnDeck();

    // How lit a spot on the deck is, 0 to 1. The rain, the shadows, the puddle
    // mirrors and every sprite tint all ask this same question, because the
    // only way to guarantee that four systems agree with the picture is for
    // all four to be reading from the one function that draws it.
    float lightAt(sf::Vector2f where) const;

    // What a lamp is doing this instant. Flicker is two sines that do not
    // share a period, and the port quarter light additionally drops out for a
    // fifth of a second every five: one bad lamp on a rig is worth more than
    // the other four put together.
    float lampNow(std::size_t index) const;

    // The deck itself, as a shaped sheet of plating rather than a rectangular
    // sprite, and everything welded to it that never moves: the bulwark round
    // the edge with its freeing ports, the butt seams between plates, the
    // lashing points and the hazard border. Both are built once.
    void buildHull();
    void buildFixed();

    void drawSea();
    void drawPlate(sf::RenderTarget& target);
    void drawWet();
    void drawLightPass();
    void drawShadows();
    void drawStations(sf::RenderTarget& target);
    void drawSub(sf::RenderTarget& target);
    void drawDiver(sf::RenderTarget& target);
    void drawWeather();
    void drawVignette();
    void drawSigns();

    // The shared vocabulary. Everything drawn here that is not a plate is one
    // of these. The ones that take a mesh take it because a mark is either an
    // absence of light or an addition to it, and those go to different arrays.
    void disc(sf::Vector2f at, float radius, sf::Color colour, int steps = 22);
    void oval(sf::Vector2f at, sf::Vector2f radii, sf::Color colour, int steps = 22);
    void ovalTurned(sf::Vector2f at, sf::Vector2f radii, float turn, sf::Color colour,
                    int steps = 20);
    void ring(sf::VertexArray& into, sf::Vector2f at, float inner, float outer,
              sf::Color colour, int steps = 30);
    void quad(sf::Vector2f at, sf::Vector2f size, sf::Color colour);

    // Any four-cornered patch, in order, into any mesh. The hull is built out
    // of hundreds of these and none of them are axis aligned.
    void slab(sf::VertexArray& into, sf::Vector2f a, sf::Vector2f b, sf::Vector2f c,
              sf::Vector2f d, sf::Color colour);

    // Light on a wet plate: an ellipse turned to face away from its own mast,
    // bright in the middle and with no rim to notice. Ring quads rather than a
    // triangle fan for the reason the arena's halo is ring quads - a fan meets
    // at one vertex, and one vertex can only be one brightness.
    void pool(sf::VertexArray& into, sf::Vector2f at, sf::Vector2f radii, float turn,
              sf::Color colour, float strength, int bands = 5, int steps = 28);

    // A quad that starts narrow and ends wide, with a colour at each end. The
    // light you can see standing in the air because the air is full of water
    // is this shape and nothing else.
    void smear(sf::VertexArray& into, sf::Vector2f from, sf::Vector2f to, float wide0,
               float wide1, sf::Color a, sf::Color b);

    // Contact plus two casts, one per nearest mast. Everything on a floodlit
    // rig has two shadows splaying off it, and nothing else says "five lamps"
    // as cheaply.
    void shadowFor(sf::Vector2f at, float across, float contact);

    std::array<Station, static_cast<std::size_t>(Post::Count)> m_stations;

    Post  m_at = Post::Count;
    float m_time = 0.0f;

    sf::Vector2f m_diver;
    sf::Vector2f m_speed;
    sf::Angle    m_face;
    float        m_stride = 0.0f;  // where he is in a step, so the boots swing
    float        m_effort = 0.0f;  // 0 standing, 1 walking flat out
    bool         m_stepped = false; // a boot landed on this frame

    sf::VertexArray m_mesh;  // everything that covers up
    sf::VertexArray m_glow;  // everything that adds
    sf::VertexArray m_text;

    // The hull plating, textured and cut to the outline, and everything welded
    // to it. Neither ever changes, so both are built once and only drawn.
    sf::VertexArray    m_hull;
    sf::VertexArray    m_fixed;
    const sf::Texture* m_plateTexture;
    sf::Sprite m_crane;
    sf::Sprite m_bench;
    sf::Sprite m_bell;
    sf::Sprite m_crate;
    sf::Sprite m_body;
    sf::Sprite m_boot;

    // The player's own sub: the hull plus every piece of hardware the tree has
    // bolted on. The stats it reads are borrowed, never owned, so a null
    // pointer just means "not told yet" and it is not drawn.
    //
    // It is composed into its own canvas at the hull's native resolution and
    // then drawn from that, smoothed, so that making it big is an enlargement
    // rather than a ragged pixel double. m_subCanvas must outlive m_subImage,
    // which borrows its texture - hence the order.
    sf::Sprite      m_subHull;
    ShipVisuals     m_subKit;
    sf::RenderTexture m_subCanvas;
    sf::Sprite      m_subImage;
    const Stats*    m_subStats = nullptr;

    // Everything lying about that is not a station. One sprite per placement
    // rather than per texture, because two of them are put down twice and a
    // sprite carries its own position.
    std::vector<sf::Sprite> m_props;
};
