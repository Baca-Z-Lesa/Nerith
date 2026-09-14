#pragma once

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>

#include <array>
#include <cstddef>
#include <string>

struct Assets;

// The surface. Everything between two dives happens up here, and it is a place
// rather than a menu: a rig deck in a grey sea with a man in a suit walking
// about on it.
//
// Three things on it are worth walking to. The crane buys what the hold came
// up with, the bench is the tree, and the bell is the way back down. Which one
// you are standing at is all this class decides; what standing there means is
// the caller's business, so the deck owns no money, no tree and no save.
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

    // One line under a station's name: what it is offering right now. The
    // numbers belong to the caller, so they are handed in rather than worked
    // out here.
    void setNote(Post which, std::string note);

    // And whether it can be used at all. A shut station still shows its note -
    // "NOTHING IN THE HOLD" is the answer to the question, not the absence of
    // one - but it is drawn dead and the prompt does not appear.
    void setOpen(Post which, bool open);

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
    void drawSea();
    void drawPlate(sf::RenderTarget& target);
    void drawStations(sf::RenderTarget& target);
    void drawDiver(sf::RenderTarget& target);
    void drawSigns();

    // The shared vocabulary, the same three calls the cutscenes are built out
    // of: everything drawn here that is not a sprite is one of these.
    void disc(sf::Vector2f at, float radius, sf::Color colour, int steps = 22);
    void oval(sf::Vector2f at, sf::Vector2f radii, sf::Color colour, int steps = 22);
    void ring(sf::Vector2f at, float inner, float outer, sf::Color colour, int steps = 30);
    void quad(sf::Vector2f at, sf::Vector2f size, sf::Color colour);

    std::array<Station, static_cast<std::size_t>(Post::Count)> m_stations;

    Post  m_at = Post::Count;
    float m_time = 0.0f;

    sf::Vector2f m_diver;
    sf::Vector2f m_speed;
    sf::Angle    m_face;
    float        m_stride = 0.0f;  // where he is in a step, so the boots swing
    float        m_effort = 0.0f;  // 0 standing, 1 walking flat out

    sf::VertexArray m_mesh;
    sf::VertexArray m_text;

    sf::Sprite m_plate;
    sf::Sprite m_rail;
    sf::Sprite m_crane;
    sf::Sprite m_bench;
    sf::Sprite m_bell;
    sf::Sprite m_crate;
    sf::Sprite m_body;
    sf::Sprite m_boot;
};
