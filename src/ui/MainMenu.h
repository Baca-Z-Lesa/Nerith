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
        Continue,
        NewDive,
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
    };

    sf::FloatRect boundsOf(std::size_t index) const;

    std::vector<Entry> m_entries;
    std::size_t        m_selected;

    std::string m_record;

    sf::RectangleShape m_veil;
    sf::RectangleShape m_box;
    sf::VertexArray    m_text;
};
