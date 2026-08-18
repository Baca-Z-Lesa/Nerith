#pragma once

#include <string>
#include <vector>

// Progress on disk. Plain text on purpose: it stays readable and hand editable
// while balancing, and a corrupt or missing file simply means a fresh start
// rather than an error the player has to deal with.
//
// Nothing here is ever taken away by dying. Drowning costs the attempt, not the
// salvage and not the tree; the difficulty lives in how hard a level is to beat
// in the first place, not in how much it destroys when it beats you.
struct SaveGame
{
    int selectedLevel = 1; // what the hub will dive into next
    int unlockedLevel = 1; // the deepest level that may be picked
    int salvage = 0;
    int bestLevel = 0;
    int bestDepth = 0;
    int storySeen = 0; // bitmask of chapters already shown
    std::vector<std::string> unlocked;

    static const char* path();

    static SaveGame load();
    void save() const;

    // Only ever called deliberately from the menu.
    static void wipe();

    bool hasRun() const { return unlockedLevel > 1 || !unlocked.empty(); }
};
