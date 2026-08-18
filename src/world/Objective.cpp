#include "world/Objective.h"

#include <algorithm>

// --- clear the roster ---------------------------------------------------

ClearRosterObjective::ClearRosterObjective(int total)
    : m_total(std::max(1, total))
    , m_killed(0)
{
}

void ClearRosterObjective::onEnemyKilled(EnemyType)
{
    ++m_killed;
}

bool ClearRosterObjective::isComplete() const
{
    return m_killed >= m_total;
}

float ClearRosterObjective::getProgress() const
{
    return std::clamp(static_cast<float>(m_killed) / static_cast<float>(m_total), 0.0f, 1.0f);
}

std::string ClearRosterObjective::getLabel() const
{
    return "CLEAR " + std::to_string(m_killed) + "/" + std::to_string(m_total);
}

// --- kill a quota -------------------------------------------------------

KillCountObjective::KillCountObjective(int target)
    : m_target(std::max(1, target))
    , m_killed(0)
{
}

void KillCountObjective::onEnemyKilled(EnemyType)
{
    ++m_killed;
}

bool KillCountObjective::isComplete() const
{
    return m_killed >= m_target;
}

float KillCountObjective::getProgress() const
{
    return std::clamp(static_cast<float>(m_killed) / static_cast<float>(m_target), 0.0f, 1.0f);
}

std::string KillCountObjective::getLabel() const
{
    return "KILLS " + std::to_string(m_killed) + "/" + std::to_string(m_target);
}

// --- survive ------------------------------------------------------------

SurviveTimeObjective::SurviveTimeObjective(float seconds)
    : m_target(std::max(1.0f, seconds))
    , m_elapsed(0.0f)
{
}

void SurviveTimeObjective::update(float deltaTime)
{
    m_elapsed += deltaTime;
}

bool SurviveTimeObjective::isComplete() const
{
    return m_elapsed >= m_target;
}

float SurviveTimeObjective::getProgress() const
{
    return std::clamp(m_elapsed / m_target, 0.0f, 1.0f);
}

std::string SurviveTimeObjective::getLabel() const
{
    const int left = static_cast<int>(std::max(0.0f, m_target - m_elapsed)) + 1;
    return "HOLD " + std::to_string(left) + "S";
}

bool SurviveTimeObjective::wantsSpawning() const
{
    // Stop feeding a few seconds early, so the level ends on a lull instead of
    // dumping a fresh creature one frame before the clock expires.
    return m_elapsed < m_target - 3.0f;
}

// --- boss ---------------------------------------------------------------

DefeatBossObjective::DefeatBossObjective()
    : m_defeated(false)
    , m_coreFraction(1.0f)
{
}

void DefeatBossObjective::onBossDefeated()
{
    m_defeated = true;
    m_coreFraction = 0.0f;
}

void DefeatBossObjective::setCoreFraction(float fraction)
{
    m_coreFraction = std::clamp(fraction, 0.0f, 1.0f);
}

bool DefeatBossObjective::isComplete() const
{
    return m_defeated;
}

float DefeatBossObjective::getProgress() const
{
    return 1.0f - m_coreFraction;
}

std::string DefeatBossObjective::getLabel() const
{
    return m_defeated ? "IT IS DEAD" : "KILL IT";
}

bool DefeatBossObjective::wantsSpawning() const
{
    return false;
}
