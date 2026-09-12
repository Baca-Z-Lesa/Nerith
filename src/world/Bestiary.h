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

        { "DARTER",
          "ALL TAIL. IT DIES TO ONE JET.",
          "IT CLOSES FASTER THAN YOU CAN BACK AWAY." },

        { "HULK",
          "OVERLAPPING PLATE AND EYES TOO SMALL TO MATTER.",
          "IT CANNOT BE OUTFOUGHT QUICKLY. IT CAN BE WALKED AWAY FROM." },

        { "SPITTER",
          "A GLAND WITH A NOZZLE WHERE A FACE SHOULD BE.",
          "HARMLESS UP CLOSE. LETHAL IF YOU LEAVE IT ALONE." },

        { "BLOOM",
          "A TRANSLUCENT BELL THAT BARELY MOVES.",
          "IT DOES NOT CHASE. IT FILLS THE WATER AND WAITS." },

        { "SPAWNLING",
          "A LARVA, BARELY FORMED, WORTH NOTHING.",
          "IT EXISTS TO SPEND THE SHOTS YOU WANTED ELSEWHERE." },

        { "BROODER",
          "A SAC YOU CAN SEE THE YOUNG MOVING IN.",
          "KILL THE SOURCE. THE SWARM IS NOT THE PROBLEM." },

        { "LANCER",
          "A RIGID ROSTRUM AND FINS THAT ONLY STABILISE.",
          "IT AIMS BEFORE IT MOVES, AND THEN IT CANNOT TURN." },

        { "WEAVER",
          "LONG LEGS AND A SPINNER, BUILT FOR RANGE.",
          "IT NEVER CLOSES. STANDING STILL IS THE MISTAKE." },

        { "HUSK",
          "SPLIT SEAMS WITH THE PRESSURE SHOWING THROUGH.",
          "SLOW, SOFT, AND IT EMPTIES ITSELF WHERE IT DIES." },

        { "ANGLER",
          "A LURE, AND NOTHING ELSE ON IT LIT.",
          "YOU DO NOT SEE IT UNTIL IT IS ALREADY INSIDE THE LAMP." },

        { "WARDEN",
          "ONE FUSED SHIELD OVER THE HEAD. THE BACK IS BARE.",
          "SHOOTING IT IN THE FACE DOES NOTHING. GO ROUND IT." },

        { "SIPHON",
          "A RASPING DISC OF HOOKED TEETH. NO JAW AT ALL.",
          "IT DOES NOT WANT TO KILL YOU. IT WANTS TO HOLD ON. DASH." },

        { "REVENANT",
          "FLAT AND SHOVEL HEADED, SILTED OVER.",
          "UNTOUCHABLE UNDER THE FLOOR. IT COMES UP BESIDE YOU." },

        { "HERALD",
          "LAMPS ON STALKS AND NOTHING TO BITE WITH.",
          "IT HOLDS A LIGHT UP FOR THE REST. KILL IT FIRST." },

        { "RIFT",
          "A FOLDED MEMBRANE ROUND A BODY HALF THERE.",
          "IT DOES NOT SWIM TO YOU. IT ARRIVES, AND THEN IT FIRES." },

        { "COILWORM",
          "A BORING HEAD TOWING A TRAIN OF RIBBED RINGS.",
          "ONLY THE HEAD CAN BE HURT. THE REST OF IT IS THE WEAPON." },
    } };

    inline constexpr const Entry& entryFor(EnemyType type)
    {
        return Entries[static_cast<std::size_t>(type)];
    }
}
