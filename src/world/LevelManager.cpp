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
        case ObjectiveKind::LiftQuota:
            m_objective = std::make_unique<LiftQuotaObjective>(m_def.objectiveAmount);
            break;

        case ObjectiveKind::RunDark:
            m_objective = std::make_unique<RunDarkObjective>(
                static_cast<float>(m_def.objectiveAmount));
            break;

        case ObjectiveKind::OpenWrecks:
            m_objective = std::make_unique<OpenWrecksObjective>(m_def.objectiveAmount);
            break;

        case ObjectiveKind::RecoverRig:
            m_objective = std::make_unique<RecoverRigObjective>();
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

        case ObjectiveKind::Hunt:
            m_objective = std::make_unique<HuntObjective>();
            break;

        case ObjectiveKind::Restore:
            m_objective = std::make_unique<RestoreObjective>(m_def.objectiveAmount);
            break;

        case ObjectiveKind::Defend:
            m_objective = std::make_unique<DefendObjective>(
                static_cast<float>(m_def.objectiveAmount), config::WardSetback);
            break;

        case ObjectiveKind::Escape:
            m_objective = std::make_unique<EscapeObjective>();
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
        m_objective->update(deltaTime, m_lampOff);
}

std::optional<EnemyType> LevelManager::takeSpawn(std::size_t aliveEnemies)
{
    if (!m_objective || !m_objective->wantsSpawning() || m_def.isBoss || m_queue.empty())
        return std::nullopt;

    // The ceiling moves with the difficulty too, not only the length of the
    // queue. A setting that promises fewer of them and only shortens the queue
    // changes how long a level lasts and not how hard it is: what a player
    // feels is how many are on them at once, and that was the same number on
    // all three settings.
    const int ceiling = std::max(1, static_cast<int>(std::lround(
                                        static_cast<float>(m_def.maxAlive)
                                        * difficulty().countScale)))
                        + config::GlobalMaxAliveBonus;

    if (m_spawnTimer > 0.0f || static_cast<int>(aliveEnemies) >= ceiling)
        return std::nullopt;

    if (m_queueIndex >= m_queue.size())
    {
        // Every level loops its script now. Nothing finishes by emptying the
        // water any more - the goals are salvage, the dark, sealed hulls and
        // the men who came before - so the water has to keep being water for
        // as long as the job takes.
        m_queueIndex = 0;
    }

    m_spawnTimer = m_def.spawnInterval * config::GlobalIntervalScale;
    return m_queue[m_queueIndex++];
}

bool LevelManager::hasSpawnsLeft() const
{
    if (m_def.isBoss || !m_objective || !m_objective->wantsSpawning())
        return false;

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

void LevelManager::notifySalvageLifted()
{
    if (m_objective)
        m_objective->onSalvageLifted();
}

void LevelManager::notifyWreckOpened()
{
    if (m_objective)
        m_objective->onWreckOpened();
}

void LevelManager::notifyRigFound()
{
    if (m_objective)
        m_objective->onRigFound();
}

void LevelManager::notifyQuarryKilled()
{
    if (m_objective)
        m_objective->onQuarryKilled();
}

void LevelManager::notifyMarkLit()
{
    if (m_objective)
        m_objective->onMarkLit();
}

void LevelManager::notifyWardBitten()
{
    if (m_objective)
        m_objective->onWardBitten();
}

void LevelManager::notifyReachedMouth()
{
    if (m_objective)
        m_objective->onReachedMouth();
}

void LevelManager::setEscapeDistance(float fraction)
{
    // Only one kind of objective has anything to do with this, and it owns
    // the cast rather than the caller: nothing outside here should have to
    // know which class is behind the pointer.
    if (auto* escape = dynamic_cast<EscapeObjective*>(m_objective.get()))
        escape->setDistance(fraction);
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
