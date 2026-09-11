#pragma once

#include "core/Config.h"
#include "world/LevelDef.h"
#include "world/Objective.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Owns the level currently being played: its spawn script, its objective and
// its pacing. It never touches a creature. It only says what should arrive,
// when, and whether the level is over.
class LevelManager
{
public:
    LevelManager();

    // The player's difficulty choice, as an index into config::Difficulties.
    // It only ever lands on the spawn count and the scaling handed to
    // creatures, so nothing downstream has to know the setting exists.
    void setDifficulty(int index);

    void start(int number);
    void update(float deltaTime);

    // One creature per call, when the release timer is up and there is room.
    std::optional<EnemyType> takeSpawn(std::size_t aliveEnemies);

    void notifyEnemyKilled(EnemyType type);
    void notifyBossDefeated();
    void notifyBossCore(float fraction);

    bool isComplete() const;

    int getNumber() const { return m_number; }
    int getDepth() const { return depthOf(m_number); }
    int getLevelInDepth() const { return levelWithinDepth(m_number); }
    bool isBoss() const { return m_def.isBoss; }

    const LevelDef& getDef() const { return m_def; }
    EnemyScaling getScaling() const;

    float getProgress() const;
    std::string getObjectiveLabel() const;

    // True while the script still has something left to release.
    bool hasSpawnsLeft() const;

private:
    void buildQueue();

    const config::DifficultyDef& difficulty() const;

    LevelDef m_def{};
    int      m_number = 1;
    int      m_difficulty = 1;

    std::vector<EnemyType> m_queue;
    std::size_t            m_queueIndex = 0;
    float                  m_spawnTimer = 0.0f;

    std::unique_ptr<Objective> m_objective;
    DefeatBossObjective*       m_bossObjective = nullptr; // non owning view of the above
};
