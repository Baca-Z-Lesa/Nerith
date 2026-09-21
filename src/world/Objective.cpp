#include "world/Objective.h"

#include <algorithm>
#include <cmath>

// --- fill the hold ------------------------------------------------------

LiftQuotaObjective::LiftQuotaObjective(int target)
    : m_target(std::max(1, target))
    , m_lifted(0)
{
}

void LiftQuotaObjective::onSalvageLifted()
{
    ++m_lifted;
}

bool LiftQuotaObjective::isComplete() const
{
    return m_lifted >= m_target;
}

float LiftQuotaObjective::getProgress() const
{
    return std::clamp(static_cast<float>(m_lifted) / static_cast<float>(m_target), 0.0f, 1.0f);
}

std::string LiftQuotaObjective::getLabel() const
{
    return "LIFT " + std::to_string(m_lifted) + "/" + std::to_string(m_target);
}

// --- work in the dark ---------------------------------------------------

RunDarkObjective::RunDarkObjective(float seconds)
    : m_target(std::max(1.0f, seconds))
    , m_elapsed(0.0f)
{
}

void RunDarkObjective::update(float deltaTime, bool lampOff)
{
    // Only while it is actually off. A clock that runs anyway is a survive
    // level wearing a different label, and the whole point is that the player
    // has to choose to stop being able to see.
    if (lampOff)
        m_elapsed += deltaTime;
}

bool RunDarkObjective::isComplete() const
{
    return m_elapsed >= m_target;
}

float RunDarkObjective::getProgress() const
{
    return std::clamp(m_elapsed / m_target, 0.0f, 1.0f);
}

std::string RunDarkObjective::getLabel() const
{
    const int left = static_cast<int>(std::max(0.0f, m_target - m_elapsed)) + 1;
    return "RUN DARK " + std::to_string(left) + "S";
}

// --- cut the hulls open -------------------------------------------------

OpenWrecksObjective::OpenWrecksObjective(int total)
    : m_total(std::max(1, total))
    , m_opened(0)
{
}

void OpenWrecksObjective::onWreckOpened()
{
    ++m_opened;
}

bool OpenWrecksObjective::isComplete() const
{
    return m_opened >= m_total;
}

float OpenWrecksObjective::getProgress() const
{
    return std::clamp(static_cast<float>(m_opened) / static_cast<float>(m_total), 0.0f, 1.0f);
}

std::string OpenWrecksObjective::getLabel() const
{
    return "CUT " + std::to_string(m_opened) + "/" + std::to_string(m_total) + " HULLS";
}

// --- find the one who came before ---------------------------------------

RecoverRigObjective::RecoverRigObjective()
    : m_found(false)
{
}

void RecoverRigObjective::onRigFound()
{
    m_found = true;
}

bool RecoverRigObjective::isComplete() const
{
    return m_found;
}

float RecoverRigObjective::getProgress() const
{
    return m_found ? 1.0f : 0.0f;
}

std::string RecoverRigObjective::getLabel() const
{
    return m_found ? "RECOVERED" : "FIND THE RIG";
}

// --- burn out the nests -------------------------------------------------

DestroyNestsObjective::DestroyNestsObjective(int total)
    : m_total(std::max(1, total))
    , m_destroyed(0)
{
}

void DestroyNestsObjective::onNestDestroyed()
{
    ++m_destroyed;
}

bool DestroyNestsObjective::isComplete() const
{
    return m_destroyed >= m_total;
}

float DestroyNestsObjective::getProgress() const
{
    return std::clamp(static_cast<float>(m_destroyed) / static_cast<float>(m_total), 0.0f, 1.0f);
}

std::string DestroyNestsObjective::getLabel() const
{
    return "NESTS " + std::to_string(m_destroyed) + "/" + std::to_string(m_total);
}

bool DestroyNestsObjective::wantsSpawning() const
{
    // The nests are the tap. The level's own roster is only there to make
    // crossing the basin cost something.
    return !isComplete();
}

// --- haul ---------------------------------------------------------------

HaulCargoObjective::HaulCargoObjective(int total)
    : m_total(std::max(1, total))
    , m_delivered(0)
{
}

void HaulCargoObjective::onCargoDelivered()
{
    ++m_delivered;
}

bool HaulCargoObjective::isComplete() const
{
    return m_delivered >= m_total;
}

float HaulCargoObjective::getProgress() const
{
    return std::clamp(static_cast<float>(m_delivered) / static_cast<float>(m_total), 0.0f, 1.0f);
}

std::string HaulCargoObjective::getLabel() const
{
    return "HAULED " + std::to_string(m_delivered) + "/" + std::to_string(m_total);
}

// --- survive ------------------------------------------------------------

SurviveTimeObjective::SurviveTimeObjective(float seconds)
    : m_target(std::max(1.0f, seconds))
    , m_elapsed(0.0f)
{
}

void SurviveTimeObjective::update(float deltaTime, bool)
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

// --- corner the one that runs -------------------------------------------

HuntObjective::HuntObjective()
    : m_killed(false)
{
}

void HuntObjective::onQuarryKilled()
{
    m_killed = true;
}

bool HuntObjective::isComplete() const
{
    return m_killed;
}

float HuntObjective::getProgress() const
{
    return m_killed ? 1.0f : 0.0f;
}

std::string HuntObjective::getLabel() const
{
    return m_killed ? "TAKEN" : "IT IS STILL MOVING";
}

// --- light what somebody else left --------------------------------------

RestoreObjective::RestoreObjective(int total)
    : m_total(std::max(1, total))
    , m_lit(0)
{
}

void RestoreObjective::onMarkLit()
{
    ++m_lit;
}

bool RestoreObjective::isComplete() const
{
    return m_lit >= m_total;
}

float RestoreObjective::getProgress() const
{
    return std::clamp(static_cast<float>(m_lit) / static_cast<float>(m_total), 0.0f, 1.0f);
}

std::string RestoreObjective::getLabel() const
{
    return "LIT " + std::to_string(m_lit) + "/" + std::to_string(m_total);
}

// --- keep them off it ----------------------------------------------------

DefendObjective::DefendObjective(float seconds, float setback)
    : m_target(std::max(1.0f, seconds))
    , m_setback(std::max(0.0f, setback))
    , m_lifted(0.0f)
{
}

void DefendObjective::update(float deltaTime, bool)
{
    m_lifted = std::min(m_target, m_lifted + deltaTime);
}

void DefendObjective::onWardBitten()
{
    // Never below nothing: a run of bad luck early should cost the player the
    // progress they had, not put them into debt they cannot see the bottom of.
    m_lifted = std::max(0.0f, m_lifted - m_setback);
}

bool DefendObjective::isComplete() const
{
    return m_lifted >= m_target;
}

float DefendObjective::getProgress() const
{
    return std::clamp(m_lifted / m_target, 0.0f, 1.0f);
}

std::string DefendObjective::getLabel() const
{
    const int left = static_cast<int>(std::ceil(m_target - m_lifted));
    return "LIFTING - " + std::to_string(std::max(0, left)) + "S";
}

// --- the swim back -------------------------------------------------------

EscapeObjective::EscapeObjective()
    : m_out(false)
    , m_fraction(1.0f)
{
}

void EscapeObjective::onReachedMouth()
{
    m_out = true;
}

void EscapeObjective::setDistance(float fraction)
{
    m_fraction = std::clamp(fraction, 0.0f, 1.0f);
}

bool EscapeObjective::isComplete() const
{
    return m_out;
}

float EscapeObjective::getProgress() const
{
    return m_out ? 1.0f : 1.0f - m_fraction;
}

std::string EscapeObjective::getLabel() const
{
    return m_out ? "OUT" : "GET BACK TO THE MOUTH";
}
