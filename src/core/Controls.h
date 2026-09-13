#pragma once

#include <SFML/Window/Keyboard.hpp>

#include <array>
#include <cstddef>

// What the player can be holding, as things the game wants rather than as keys.
//
// Everything used to read sf::Keyboard::Scancode::W directly, which is fine
// until somebody's keyboard is not the one it was written on. The scancode
// already handles layout - W is wherever W physically sits on a QWERTZ board -
// but it cannot handle preference, and "I want dash on space" is not an
// unreasonable thing to want.
enum class Action
{
    Up,
    Down,
    Left,
    Right,
    Dash,
    Lamp,
    FireMode,
    Surface,    // leave with what is in the hold
    Pause,
    Upgrades,
    Count
};

inline constexpr std::size_t ActionCount = static_cast<std::size_t>(Action::Count);

// What each one is bound to out of the box. Scancodes rather than key codes, so
// the shape of the hand on the keyboard is the same on every layout.
inline constexpr std::array<sf::Keyboard::Scancode, ActionCount> DefaultKeys{ {
    sf::Keyboard::Scancode::W,
    sf::Keyboard::Scancode::S,
    sf::Keyboard::Scancode::A,
    sf::Keyboard::Scancode::D,
    sf::Keyboard::Scancode::LShift,
    sf::Keyboard::Scancode::F,
    sf::Keyboard::Scancode::E,
    sf::Keyboard::Scancode::Q,
    sf::Keyboard::Scancode::Escape,
    sf::Keyboard::Scancode::Tab,
} };

inline constexpr std::array<const char*, ActionCount> ActionNames{ {
    "SWIM UP",
    "SWIM DOWN",
    "SWIM LEFT",
    "SWIM RIGHT",
    "DASH",
    "LAMP",
    "CYCLE GUN",
    "SURFACE",
    "PAUSE",
    "UPGRADES",
} };

// A short name for a key, for the bindings list. Anything without one shows as
// its number, which is ugly but never wrong - and a player who has bound a key
// nobody has a name for still knows which one they pressed.
const char* keyName(sf::Keyboard::Scancode key);

// Which actions may not be left unbound, because a sub that cannot be steered
// is not a configuration, it is a broken game.
inline constexpr bool isEssential(Action action)
{
    return action == Action::Up || action == Action::Down
           || action == Action::Left || action == Action::Right;
}
