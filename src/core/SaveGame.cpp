#include "core/SaveGame.h"

#include "world/LevelDef.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

const char* SaveGame::path()
{
    return "nerith_save.txt";
}

SaveGame SaveGame::load()
{
    SaveGame data;

    std::ifstream file(path());
    if (!file)
        return data; // no save yet, which is a perfectly normal first launch

    // The logbook is a list in EnemyType order, so it only means anything to
    // the roster it was written against. When the roster changed the entries
    // did not become wrong, they became about different animals - which is
    // worse, because nothing looks broken. A save written before that point
    // keeps its progress and starts its book again.
    bool sameRoster = false;

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream stream(line);
        std::string key;
        if (!(stream >> key))
            continue;

        if (key == "nerith")
        {
            int version = 0;
            stream >> version;
            sameRoster = version >= 3;
            continue;
        }

        // Unknown keys are skipped rather than rejected, so an older save still
        // loads after new fields are added.
        if (key == "level")
            stream >> data.selectedLevel;
        else if (key == "unlocked_level")
            stream >> data.unlockedLevel;
        else if (key == "salvage")
            stream >> data.salvage;
        else if (key == "landed")
        {
            int count = 0;
            while (stream >> count)
                data.landed.push_back(std::max(0, count));
        }
        else if (key == "best_level")
            stream >> data.bestLevel;
        else if (key == "best_depth")
            stream >> data.bestDepth;
        else if (key == "killed")
        {
            // One line for the whole logbook, in EnemyType order. A save from
            // before the logbook existed simply has none, which reads as an
            // empty book rather than as an error.
            int count = 0;
            while (stream >> count)
                data.killed.push_back(std::max(0, count));
        }
        else if (key == "logs")
        {
            int index = 0;
            while (stream >> index)
                data.logs.push_back(std::max(0, index));
        }
        else if (key == "story")
            stream >> data.storySeen;
        else if (key == "beaten")
        {
            int flag = 0;
            stream >> flag;
            data.campaignBeaten = flag != 0;
        }
        else if (key == "stalker")
        {
            int flag = 0;
            stream >> flag;
            data.stalkerAlive = flag != 0;
        }
        else if (key == "stalker_levels")
            stream >> data.stalkerLevels;
        else if (key == "vsync")
        {
            int flag = 0;
            stream >> flag;
            data.vsync = flag != 0;
        }
        else if (key == "framecap")
            stream >> data.frameCap;
        else if (key == "brightness")
            stream >> data.brightness;
        else if (key == "shake")
            stream >> data.shake;
        else if (key == "flashes")
            stream >> data.flashes;
        else if (key == "fps")
        {
            int flag = 0;
            stream >> flag;
            data.showFps = flag != 0;
        }
        else if (key == "camera")
            stream >> data.cameraTight;
        else if (key == "keys")
        {
            int code = 0;
            while (stream >> code)
                data.keys.push_back(code);
        }
        else if (key == "difficulty")
            stream >> data.difficulty;
        else if (key == "sfx")
            stream >> data.sfxVolume;
        else if (key == "music")
            stream >> data.musicVolume;
        else if (key == "muted")
        {
            int flag = 0;
            stream >> flag;
            data.muted = flag != 0;
        }
        else if (key == "fullscreen")
        {
            int flag = 0;
            stream >> flag;
            data.fullscreen = flag != 0;
        }
        else if (key == "upgrade")
        {
            std::string id;
            if (stream >> id)
                data.unlocked.push_back(id);
        }
    }

    if (!sameRoster)
        data.killed.clear();

    data.selectedLevel = std::max(1, data.selectedLevel);
    data.salvage = std::max(0, data.salvage);
    data.difficulty = std::clamp(data.difficulty, 0, 2);
    data.sfxVolume = std::clamp(data.sfxVolume, 0, 10);
    data.musicVolume = std::clamp(data.musicVolume, 0, 10);
    data.brightness = std::clamp(data.brightness, 0, 10);
    data.shake = std::clamp(data.shake, 0, 10);
    data.flashes = std::clamp(data.flashes, 0, 10);
    data.cameraTight = std::clamp(data.cameraTight, 0, 10);
    data.frameCap = std::max(0, data.frameCap);

    // A save written before levels could be replayed has no unlocked_level, so
    // fall back to however far it had reached - and a save written while the
    // trench still built its own levels can name one that no longer exists.
    data.unlockedLevel = std::max({ 1, data.unlockedLevel, data.bestLevel, data.selectedLevel });
    data.unlockedLevel = std::min(data.unlockedLevel, authoredLevelCount());
    data.bestLevel = std::min(data.bestLevel, authoredLevelCount());
    data.selectedLevel = std::clamp(data.selectedLevel, 1, data.unlockedLevel);

    return data;
}

void SaveGame::save() const
{
    std::ofstream file(path(), std::ios::trunc);
    if (!file)
        return; // read only directory: not worth interrupting the game over

    file << "nerith 3\n";
    file << "best_level " << bestLevel << '\n';
    file << "best_depth " << bestDepth << '\n';
    file << "story " << storySeen << '\n';
    file << "unlocked_level " << unlockedLevel << '\n';
    file << "level " << selectedLevel << '\n';
    file << "salvage " << salvage << '\n';

    if (!landed.empty())
    {
        file << "landed";
        for (const int count : landed)
            file << ' ' << count;
        file << '\n';
    }
    file << "beaten " << (campaignBeaten ? 1 : 0) << '\n';

    if (!killed.empty())
    {
        file << "killed";
        for (const int count : killed)
            file << ' ' << count;
        file << '\n';
    }

    if (!logs.empty())
    {
        file << "logs";
        for (const int index : logs)
            file << ' ' << index;
        file << '\n';
    }

    file << "stalker " << (stalkerAlive ? 1 : 0) << '\n';
    file << "stalker_levels " << stalkerLevels << '\n';

    file << "difficulty " << difficulty << '\n';
    file << "sfx " << sfxVolume << '\n';
    file << "music " << musicVolume << '\n';
    file << "muted " << (muted ? 1 : 0) << '\n';
    file << "fullscreen " << (fullscreen ? 1 : 0) << '\n';
    file << "vsync " << (vsync ? 1 : 0) << '\n';
    file << "framecap " << frameCap << '\n';
    file << "brightness " << brightness << '\n';
    file << "shake " << shake << '\n';
    file << "flashes " << flashes << '\n';
    file << "camera " << cameraTight << '\n';
    file << "fps " << (showFps ? 1 : 0) << '\n';

    // Only written once something has actually been rebound, so a save made
    // with the defaults keeps working if the defaults ever change.
    if (!keys.empty())
    {
        file << "keys";
        for (const int code : keys)
            file << ' ' << code;
        file << '\n';
    }

    for (const std::string& id : unlocked)
        file << "upgrade " << id << '\n';
}

void SaveGame::wipe()
{
    std::remove(path());
}
