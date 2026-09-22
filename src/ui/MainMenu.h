#pragma once

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

// The screen the game opens on. It owns nothing but its own selection: what
// each entry means is decided by the caller.
class MainMenu
{
public:
    enum class Choice
    {
        Dive,     // into the hub, where the level and the upgrades are picked
        Controls, // what the keys do
        Bestiary, // what has been met, and what it was
        Options,  // sound and difficulty
        Credits,
        Reset,    // throw everything away, deliberately
        Quit,
    };

    MainMenu();

    // Rebuilt whenever the save changes, because Continue only exists when
    // there is a run to continue.
    void build(bool hasRun, int resumeLevel, int bestLevel, int bestDepth);

    void moveSelection(int delta);
    void setCursor(sf::Vector2f screenPosition);

    Choice getSelected() const;
    std::size_t getSelectedIndex() const { return m_selected; }

    void draw(sf::RenderTarget& target, float time);

private:
    struct Entry
    {
        std::string label;
        std::string note;
        Choice choice;

        // How far this row has come up, 0 to 1. Eased rather than switched, so
        // moving down the list is a thing that happens over a moment instead of
        // two rows changing colour on the same frame.
        float glow = 0.0f;
    };

    sf::FloatRect boundsOf(std::size_t index) const;

    // A filled rectangle straight into the mark mesh. The rows are drawn out of
    // these rather than out of more RectangleShapes because the brackets and
    // the edge bar want to be part of the same one draw call.
    void mark(sf::Vector2f at, sf::Vector2f size, sf::Color colour);

    std::vector<Entry> m_entries;
    std::size_t        m_selected;

    std::string m_record;

    // Where the clock was on the previous frame, so the eases have a delta
    // without the caller having to start passing one.
    float m_lastTime = 0.0f;

    sf::RectangleShape m_veil;
    sf::RectangleShape m_box;
    sf::VertexArray    m_marks;
    sf::VertexArray    m_text;
};
