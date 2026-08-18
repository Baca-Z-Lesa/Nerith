#include "core/Game.h"

#include <exception>
#include <iostream>

int main()
{
    try
    {
        Game game;
        game.run();
    }
    catch (const std::exception& error)
    {
        // Most likely a missing asset: SFML's resource constructors throw.
        std::cerr << "Nerith failed to start: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
