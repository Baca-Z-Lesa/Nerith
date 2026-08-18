#pragma once

#include <string>
#include <vector>

// Run progress on disk. Plain text on purpose: it stays readable and hand
// editable while balancing, and a corrupt or missing file simply means a fresh
// run rather than an error the player has to deal with.
struct SaveGame
{
    int level = 1;
    int salvage = 0;
    int bestLevel = 0;
    int bestDepth = 0;
    std::vector<std::string> unlocked;

    static const char* path();

    static SaveGame load();
    void save() const;

    // Wipes the run but keeps the records, which is what dying should cost.
    static void clearRun();

    bool hasRun() const { return level > 1 || !unlocked.empty(); }
};
