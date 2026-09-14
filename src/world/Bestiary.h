#pragma once

#include "entities/EnemyType.h"

#include <array>

// What the logbook says about each animal. The art is the only place in the
// game where these things hold still long enough to be looked at, and until
// now the player had three seconds in the dark and no name to put to any of
// it. One line on what it is, one on what it does about you.
//
// Order must match EnemyType.
namespace bestiary
{
    struct Entry
    {
        const char* name;
        const char* what;  // what it is
        const char* how;   // what it does about you
    };

    inline constexpr std::array<Entry, EnemyTypeCount> Entries{ {
        { "LURKER",
          "A SOFT BOTTOM WALKER THAT HUNTS BY FEEL.",
          "IT NEVER HURRIES AND IT NEVER STOPS." },

        { "LANCER",
          "ALL TAIL AND ONE SPINE, HELD FOLDED UNTIL IT IS NOT.",
          "IT WINDS UP WHERE YOU CAN SEE IT, THEN CANNOT TURN." },

        { "WEAVER",
          "A RING OF ARMS THAT NEVER FACES YOU SQUARELY.",
          "IT CIRCLES AND RAKES. STANDING STILL IS THE MISTAKE." },

        { "BLOOM",
          "A TRANSLUCENT BELL THAT BARELY MOVES.",
          "IT FILLS THE WATER WITH RINGS, AND LETS GO OF ONE MORE AS IT DIES." },

        { "SPAWNLING",
          "BARELY FINISHED. IT HATCHED THIS MINUTE.",
          "WORTH NOTHING. THERE ARE ALWAYS MORE." },

        { "BROODER",
          "A SAC THAT WALKS, SPLITTING ALONG ONE SIDE.",
          "KILL THE SOURCE. THE SWARM IS NOT THE PROBLEM." },

        { "WARDEN",
          "PLATE ACROSS THE FRONT AND NOTHING ACROSS THE BACK.",
          "SHOOTING IT HEAD ON DOES NOTHING AT ALL. GET BEHIND IT." },

        { "REVENANT",
          "IT LIVES UNDER THE FLOOR AND COMES UP TO FEED.",
          "YOU CANNOT TOUCH IT DOWN THERE, AND IT COMES UP BESIDE YOU." },

        { "HERALD",
          "NO MOUTH. IT IS NOT HERE TO EAT.",
          "IT KEEPS YOU LIT AND HARDENS EVERYTHING AROUND IT." },

        { "MOTH",
          "NO EYES, NO MOUTH TO SPEAK OF, AND IT IS FASTER THAN YOU.",
          "IN THE DARK IT DRIFTS. IN THE BEAM IT ARRIVES. PUT THE LAMP OUT." },

        { "EFFIGY",
          "A STANDING THING THAT IS NOT ALWAYS WHERE IT WAS.",
          "IT MOVES WHILE YOU MOVE. STOP, AND SO DOES IT." },

        { "STALKER",
          "THE ONE YOU LEFT ALIVE. IT HAS BEEN FOLLOWING YOU DOWN.",
          "THE BODY BEHIND THE HEAD IS THE WEAPON, AND IT TAKES HOLD." },
    } };

    inline constexpr const Entry& entryFor(EnemyType type)
    {
        return Entries[static_cast<std::size_t>(type)];
    }
}
