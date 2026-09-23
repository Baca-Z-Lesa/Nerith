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
    int selectedLevel = 1; // what the bell is set to drop you at next
    int unlockedLevel = 1; // the deepest level that may be picked
    int salvage = 0;

    // What is stacked by the crane waiting to be sold, in SalvageTable order.
    // It is progress rather than a setting: a player who quits with a full
    // deck has to find it still there, or surfacing with a hold is a thing
    // they will only ever do once.
    std::vector<int> landed;
    int bestLevel = 0;
    int bestDepth = 0;
    int storySeen = 0;     // bitmask of chapters already shown
    bool campaignBeaten = false; // the last authored level has been cleared once
    std::vector<std::string> unlocked;

    // Settings live here too. They are the player's, not the run's, so a reset
    // leaves them alone.
    // What has been killed and how often. Kept so the logbook has something to
    // show and so a creature met once is a creature remembered: it is the only
    // place in the game where these animals hold still long enough to look at.
    std::vector<int> killed;

    // And what has killed YOU, and how often. The logbook counts what a hunt
    // went well against; this counts the other column, so the death card can
    // say whether this is a new way to die or the same one again.
    std::vector<int> deaths;

    // How far a level got before the sub came back up, and which level it was.
    // Only one is kept: it is the floor you are working on, not a ledger of
    // every floor you have half done. Erased by dying, which is what makes
    // surfacing alive worth the swim.
    int progressLevel = 0;
    int progressDone = 0;

    // Which dead divers' recorders have been brought up, as indices into the
    // log table. Kept for good: a log is radioed up the moment it is read, so
    // it is the one thing a drowning does not take back.
    std::vector<int> logs;

    // Something followed you down. It is set when a level is left with the
    // water still occupied, and it stays set until the thing is killed - so
    // it is carried between dives rather than rolled at the start of one.
    bool stalkerAlive = false;
    int  stalkerLevels = 0;   // how many dives it has survived, and how much
                              // worse that has made it

    int  difficulty = 1;      // index into config::Difficulties
    int  sfxVolume = 10;      // tenths, 0..10
    int  musicVolume = 7;
    bool muted = false;
    bool fullscreen = false;

    // --- Display ---
    bool vsync = false;        // smoother pacing, one more frame of latency
    int  frameCap = 144;       // 0 is uncapped
    int  brightness = 5;       // 0..10, 5 is the art as it was made

    // --- Accessibility ---
    // Both are how MUCH, not whether: somebody who cannot take a shaking
    // frame should not have to give up the hit feedback to stop it.
    int  shake = 10;           // 0..10
    int  flashes = 10;         // 0..10

    // --- Feel ---
    int  cameraTight = 5;      // 0..10, how hard the view follows the sub
    bool showFps = false;      // the frame counter, off unless asked for

    // What each action is bound to, as SFML scancode numbers in the order of
    // the Action enum. Empty means nothing has been rebound and the defaults
    // stand, which is also what an older save looks like.
    std::vector<int> keys;

    static const char* path();

    static SaveGame load();
    void save() const;

    // Only ever called deliberately from the menu. Settings survive it, so
    // the caller hands in the ones to keep.
    static void wipe();

    bool hasRun() const { return unlockedLevel > 1 || !unlocked.empty(); }
};
