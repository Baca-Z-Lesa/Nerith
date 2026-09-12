#include "core/SaveGame.h"

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

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream stream(line);
        std::string key;
        if (!(stream >> key))
            continue;

        // Unknown keys are skipped rather than rejected, so an older save still
        // loads after new fields are added.
        if (key == "level")
            stream >> data.selectedLevel;
        else if (key == "unlocked_level")
            stream >> data.unlockedLevel;
        else if (key == "salvage")
            stream >> data.salvage;
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
        else if (key == "story")
            stream >> data.storySeen;
        else if (key == "beaten")
        {
            int flag = 0;
            stream >> flag;
            data.campaignBeaten = flag != 0;
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

    data.selectedLevel = std::max(1, data.selectedLevel);
    data.salvage = std::max(0, data.salvage);
    data.difficulty = std::clamp(data.difficulty, 0, 2);
    data.sfxVolume = std::clamp(data.sfxVolume, 0, 10);
    data.musicVolume = std::clamp(data.musicVolume, 0, 10);

    // A save written before levels could be replayed has no unlocked_level, so
    // fall back to however far it had reached.
    data.unlockedLevel = std::max({ 1, data.unlockedLevel, data.bestLevel, data.selectedLevel });
    data.selectedLevel = std::min(data.selectedLevel, data.unlockedLevel);

    return data;
}

void SaveGame::save() const
{
    std::ofstream file(path(), std::ios::trunc);
    if (!file)
        return; // read only directory: not worth interrupting the game over

    file << "nerith 2\n";
    file << "best_level " << bestLevel << '\n';
    file << "best_depth " << bestDepth << '\n';
    file << "story " << storySeen << '\n';
    file << "unlocked_level " << unlockedLevel << '\n';
    file << "level " << selectedLevel << '\n';
    file << "salvage " << salvage << '\n';
    file << "beaten " << (campaignBeaten ? 1 : 0) << '\n';

    if (!killed.empty())
    {
        file << "killed";
        for (const int count : killed)
            file << ' ' << count;
        file << '\n';
    }

    file << "difficulty " << difficulty << '\n';
    file << "sfx " << sfxVolume << '\n';
    file << "music " << musicVolume << '\n';
    file << "muted " << (muted ? 1 : 0) << '\n';
    file << "fullscreen " << (fullscreen ? 1 : 0) << '\n';

    for (const std::string& id : unlocked)
        file << "upgrade " << id << '\n';
}

void SaveGame::wipe()
{
    std::remove(path());
}
