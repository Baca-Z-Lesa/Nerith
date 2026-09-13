#include "world/LevelManager.h"

#include "core/Config.h"

#include <algorithm>
#include <cmath>

LevelManager::LevelManager()
{
    start(1);
}

const config::DifficultyDef& LevelManager::difficulty() const
{
    return config::Difficulties[static_cast<std::size_t>(
        std::clamp(m_difficulty, 0, static_cast<int>(config::Difficulties.size()) - 1))];
}

void LevelManager::setDifficulty(int index)
{
    m_difficulty = std::clamp(index, 0, static_cast<int>(config::Difficulties.size()) - 1);
}

EnemyScaling LevelManager::getScaling() const
{
    // Only health. What creatures hit for is scaled once, at the single door
    // every wound goes through, rather than being spread over the contact
    // damage, the orbs, the mines and the boss.
    EnemyScaling scaling = m_def.scaling;
    scaling.health *= difficulty().enemyHealth;

    // And then by how much sub the player is actually driving.
    //
    // The curve used to be written against the level number alone, which
    // assumes the player arrives at level fifteen with about as much hull as
    // the designer imagined. They do not: the tree is permanent and salvage
    // accumulates, so somebody who has ground the shallows arrives at the
    // abyss with four times the gun the curve was drawn for, and the deepest
    // water in the game is the easiest thing in it. Meanwhile a player who
    // came straight down meets the same numbers with nothing, which is why the
    // shelf was the hardest part of the campaign.
    //
    // This is the same number answering both: what a level asks for follows
    // what the player brought to it.
    const float power = std::clamp(m_power, 0.0f, 1.0f);

    scaling.health *= 1.0f + config::PowerScaleHealth * power;
    scaling.speed *= 1.0f + config::PowerScaleSpeed * power;
    scaling.bonusDamage += static_cast<int>(std::lround(config::PowerScaleDamage * power));

    return scaling;
}

void LevelManager::buildQueue()
{
    m_queue.clear();
    m_queueIndex = 0;

    // Groups are expanded in the order the designer wrote them, so a level can
    // open with something soft and close with something that hurts.
    // Every count is multiplied by the one global screw, so making the whole
    // campaign harder is a single number rather than twenty edits.
    for (const SpawnGroup& group : m_def.script)
    {
        const int count = static_cast<int>(std::lround(static_cast<float>(group.count)
                                                       * config::GlobalCountScale
                                                       * difficulty().countScale));
        for (int i = 0; i < std::max(group.count > 0 ? 1 : 0, count); ++i)
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

        case ObjectiveKind::DestroyNests:
            m_objective = std::make_unique<DestroyNestsObjective>(m_def.objectiveAmount);
            break;

        case ObjectiveKind::HaulCargo:
            m_objective = std::make_unique<HaulCargoObjective>(m_def.objectiveAmount);
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

    if (m_spawnTimer > 0.0f || aliveEnemies >= m_def.maxAlive + config::GlobalMaxAliveBonus)
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

    m_spawnTimer = m_def.spawnInterval * config::GlobalIntervalScale;
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

void LevelManager::notifyNestDestroyed()
{
    if (m_objective)
        m_objective->onNestDestroyed();
}

int LevelManager::getNestCount() const
{
    return m_def.objective == ObjectiveKind::DestroyNests ? std::max(1, m_def.objectiveAmount) : 0;
}

void LevelManager::notifyCargoDelivered()
{
    if (m_objective)
        m_objective->onCargoDelivered();
}

int LevelManager::getCargoCount() const
{
    return m_def.objective == ObjectiveKind::HaulCargo ? std::max(1, m_def.objectiveAmount) : 0;
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
