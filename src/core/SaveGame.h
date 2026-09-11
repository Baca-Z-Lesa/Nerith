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
    int storySeen = 0;     // bitmask of chapters already shown
    bool campaignBeaten = false; // the last authored level has been cleared once
    std::vector<std::string> unlocked;

    // Settings live here too. They are the player's, not the run's, so a reset
    // leaves them alone.
    int  difficulty = 1;      // index into config::Difficulties
    int  sfxVolume = 10;      // tenths, 0..10
    int  musicVolume = 7;
    bool muted = false;
    bool fullscreen = false;

    static const char* path();

    static SaveGame load();
    void save() const;

    // Only ever called deliberately from the menu. Settings survive it, so
    // the caller hands in the ones to keep.
    static void wipe();

    bool hasRun() const { return unlockedLevel > 1 || !unlocked.empty(); }
};
