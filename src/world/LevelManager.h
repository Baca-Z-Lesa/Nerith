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

    // How much sub the player is driving, 0 for a bare hull and 1 for a tree
    // that has nothing left to buy. What a level asks for follows it.
    void setPower(float power) { m_power = power; }

    void start(int number);
    void update(float deltaTime);

    // One creature per call, when the release timer is up and there is room.
    std::optional<EnemyType> takeSpawn(std::size_t aliveEnemies);

    void notifyEnemyKilled(EnemyType type);
    void notifyNestDestroyed();
    void notifyCargoDelivered();

    // Things the world already did that an objective may now be counting.
    void notifySalvageLifted();
    void notifyWreckOpened();
    void notifyRigFound();
    void notifyQuarryKilled();
    void notifyMarkLit();
    void notifyWardBitten();
    void notifyReachedMouth();

    // Escape levels want to show how far there is left to go, and only the
    // caller knows where the sub and the mouth are.
    void setEscapeDistance(float fraction);

    // Whether the lamp is off this instant. Pushed rather than pulled: the
    // level owns no pointer to the sub and is not about to grow one.
    void setLampOff(bool off) { m_lampOff = off; }

    // How many pods this level scatters, zero on anything that is not a haul.
    int  getCargoCount() const;
    void notifyBossDefeated();

    // How many nests this level hides, and whether it hides any at all.
    int getNestCount() const;
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
    float    m_power = 0.0f;
    bool     m_lampOff = false;

    std::vector<EnemyType> m_queue;
    std::size_t            m_queueIndex = 0;
    float                  m_spawnTimer = 0.0f;

    std::unique_ptr<Objective> m_objective;
    DefeatBossObjective*       m_bossObjective = nullptr; // non owning view of the above
};
