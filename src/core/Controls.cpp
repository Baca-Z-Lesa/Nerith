#include "core/Controls.h"

namespace
{
    struct Named
    {
        sf::Keyboard::Scancode key;
        const char* name;
    };

    // Only the ones somebody is plausibly going to bind. Everything else falls
    // through to its number, which is not pretty but is never a lie.
    constexpr Named Names[] = {
        { sf::Keyboard::Scancode::A, "A" },
        { sf::Keyboard::Scancode::B, "B" },
        { sf::Keyboard::Scancode::C, "C" },
        { sf::Keyboard::Scancode::D, "D" },
        { sf::Keyboard::Scancode::E, "E" },
        { sf::Keyboard::Scancode::F, "F" },
        { sf::Keyboard::Scancode::G, "G" },
        { sf::Keyboard::Scancode::H, "H" },
        { sf::Keyboard::Scancode::I, "I" },
        { sf::Keyboard::Scancode::J, "J" },
        { sf::Keyboard::Scancode::K, "K" },
        { sf::Keyboard::Scancode::L, "L" },
        { sf::Keyboard::Scancode::M, "M" },
        { sf::Keyboard::Scancode::N, "N" },
        { sf::Keyboard::Scancode::O, "O" },
        { sf::Keyboard::Scancode::P, "P" },
        { sf::Keyboard::Scancode::Q, "Q" },
        { sf::Keyboard::Scancode::R, "R" },
        { sf::Keyboard::Scancode::S, "S" },
        { sf::Keyboard::Scancode::T, "T" },
        { sf::Keyboard::Scancode::U, "U" },
        { sf::Keyboard::Scancode::V, "V" },
        { sf::Keyboard::Scancode::W, "W" },
        { sf::Keyboard::Scancode::X, "X" },
        { sf::Keyboard::Scancode::Y, "Y" },
        { sf::Keyboard::Scancode::Z, "Z" },
        { sf::Keyboard::Scancode::Num0, "0" },
        { sf::Keyboard::Scancode::Num1, "1" },
        { sf::Keyboard::Scancode::Num2, "2" },
        { sf::Keyboard::Scancode::Num3, "3" },
        { sf::Keyboard::Scancode::Num4, "4" },
        { sf::Keyboard::Scancode::Num5, "5" },
        { sf::Keyboard::Scancode::Num6, "6" },
        { sf::Keyboard::Scancode::Num7, "7" },
        { sf::Keyboard::Scancode::Num8, "8" },
        { sf::Keyboard::Scancode::Num9, "9" },
        { sf::Keyboard::Scancode::Space, "SPACE" },
        { sf::Keyboard::Scancode::Enter, "ENTER" },
        { sf::Keyboard::Scancode::Escape, "ESCAPE" },
        { sf::Keyboard::Scancode::Tab, "TAB" },
        { sf::Keyboard::Scancode::LShift, "LSHIFT" },
        { sf::Keyboard::Scancode::RShift, "RSHIFT" },
        { sf::Keyboard::Scancode::LControl, "LCTRL" },
        { sf::Keyboard::Scancode::RControl, "RCTRL" },
        { sf::Keyboard::Scancode::LAlt, "LALT" },
        { sf::Keyboard::Scancode::RAlt, "RALT" },
        { sf::Keyboard::Scancode::Up, "UP" },
        { sf::Keyboard::Scancode::Down, "DOWN" },
        { sf::Keyboard::Scancode::Left, "LEFT" },
        { sf::Keyboard::Scancode::Right, "RIGHT" },
        { sf::Keyboard::Scancode::Backspace, "BACKSPC" },
        { sf::Keyboard::Scancode::Comma, "COMMA" },
        { sf::Keyboard::Scancode::Period, "PERIOD" },
        { sf::Keyboard::Scancode::Semicolon, "SEMICOL" },
        { sf::Keyboard::Scancode::Slash, "SLASH" },
        { sf::Keyboard::Scancode::Backslash, "BACKSL" },
        { sf::Keyboard::Scancode::LBracket, "LBRACK" },
        { sf::Keyboard::Scancode::RBracket, "RBRACK" },
        { sf::Keyboard::Scancode::Hyphen, "MINUS" },
        { sf::Keyboard::Scancode::Equal, "EQUALS" },
        { sf::Keyboard::Scancode::Grave, "GRAVE" },
        { sf::Keyboard::Scancode::F1, "F1" },
        { sf::Keyboard::Scancode::F2, "F2" },
        { sf::Keyboard::Scancode::F3, "F3" },
        { sf::Keyboard::Scancode::F4, "F4" },
        { sf::Keyboard::Scancode::F5, "F5" },
        { sf::Keyboard::Scancode::F6, "F6" },
        { sf::Keyboard::Scancode::F7, "F7" },
        { sf::Keyboard::Scancode::F8, "F8" },
        { sf::Keyboard::Scancode::F9, "F9" },
        { sf::Keyboard::Scancode::F10, "F10" },
        { sf::Keyboard::Scancode::F12, "F12" },
    };
}

const char* keyName(sf::Keyboard::Scancode key)
{
    for (const Named& named : Names)
    {
        if (named.key == key)
            return named.name;
    }

    return nullptr;
}
