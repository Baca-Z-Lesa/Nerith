#include "world/LevelManager.h"

#include <algorithm>

LevelManager::LevelManager()
{
    start(1);
}

void LevelManager::buildQueue()
{
    m_queue.clear();
    m_queueIndex = 0;

    // Groups are expanded in the order the designer wrote them, so a level can
    // open with something soft and close with something that hurts.
    for (const SpawnGroup& group : m_def.script)
    {
        for (int i = 0; i < group.count; ++i)
            m_queue.push_back(group.type);
    }
}

void LevelManager::start(int number)
{
    m_number = std::max(1, number);
    m_def = levelFor(m_number);
    m_spawnTimer = 0.0f;
    m_bossObjective = nullptr;

    buildQueue();

    switch (m_def.objective)
    {
        case ObjectiveKind::ClearRoster:
            // The roster is the goal, so the target is however many the script
            // actually contains. No second number to keep in sync.
            m_objective = std::make_unique<ClearRosterObjective>(static_cast<int>(m_queue.size()));
            break;

        case ObjectiveKind::KillCount:
            m_objective = std::make_unique<KillCountObjective>(m_def.objectiveAmount);
            break;

        case ObjectiveKind::SurviveTime:
            m_objective = std::make_unique<SurviveTimeObjective>(
                static_cast<float>(m_def.objectiveAmount));
            break;

        case ObjectiveKind::DefeatBoss:
        {
            auto boss = std::make_unique<DefeatBossObjective>();
            m_bossObjective = boss.get();
            m_objective = std::move(boss);
            break;
        }
    }
}

void LevelManager::update(float deltaTime)
{
    if (m_spawnTimer > 0.0f)
        m_spawnTimer -= deltaTime;

    if (m_objective)
        m_objective->update(deltaTime);
}

std::optional<EnemyType> LevelManager::takeSpawn(std::size_t aliveEnemies)
{
    if (!m_objective || !m_objective->wantsSpawning() || m_def.isBoss || m_queue.empty())
        return std::nullopt;

    if (m_spawnTimer > 0.0f || aliveEnemies >= m_def.maxAlive)
        return std::nullopt;

    if (m_queueIndex >= m_queue.size())
    {
        // A fixed roster is sent once and the level ends when it is dead.
        // Timed and quota levels loop the script instead, so the pressure
        // never lets up until the objective closes it.
        if (m_def.objective == ObjectiveKind::ClearRoster)
            return std::nullopt;

        m_queueIndex = 0;
    }

    m_spawnTimer = m_def.spawnInterval;
    return m_queue[m_queueIndex++];
}

bool LevelManager::hasSpawnsLeft() const
{
    if (m_def.isBoss || !m_objective || !m_objective->wantsSpawning())
        return false;

    if (m_def.objective == ObjectiveKind::ClearRoster)
        return m_queueIndex < m_queue.size();

    return true;
}

void LevelManager::notifyEnemyKilled(EnemyType type)
{
    if (m_objective)
        m_objective->onEnemyKilled(type);
}

void LevelManager::notifyBossDefeated()
{
    if (m_objective)
        m_objective->onBossDefeated();
}

void LevelManager::notifyBossCore(float fraction)
{
    if (m_bossObjective)
        m_bossObjective->setCoreFraction(fraction);
}

bool LevelManager::isComplete() const
{
    return m_objective && m_objective->isComplete();
}

float LevelManager::getProgress() const
{
    return m_objective ? m_objective->getProgress() : 0.0f;
}

std::string LevelManager::getObjectiveLabel() const
{
    return m_objective ? m_objective->getLabel() : std::string();
}
