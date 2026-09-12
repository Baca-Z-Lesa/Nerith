#pragma once

#include "entities/EnemyType.h"

#include <string>

// What finishing a level actually means. A level owns exactly one of these, so
// a new kind of goal (protect a wreck, kill a named creature, reach a place) is
// a new subclass plus a row in the level table. Game never learns about it.
class Objective
{
public:
    virtual ~Objective() = default;

    virtual void onEnemyKilled(EnemyType) {}
    virtual void onBossDefeated() {}
    virtual void onNestDestroyed() {}
    virtual void onCargoDelivered() {}
    virtual void update(float) {}

    virtual bool isComplete() const = 0;

    // 0..1, for the progress bar.
    virtual float getProgress() const = 0;

    // Short line shown under the bar, e.g. "KILLS 7/12".
    virtual std::string getLabel() const = 0;

    // False once the level should stop feeding creatures in.
    virtual bool wantsSpawning() const { return true; }
};

// Kill everything the level sends. The default, and the one that makes a level
// feel like a fixed encounter rather than a tap that never closes.
class ClearRosterObjective : public Objective
{
public:
    explicit ClearRosterObjective(int total);

    void onEnemyKilled(EnemyType) override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

private:
    int m_total;
    int m_killed;
};

// Kill a quota while the level keeps sending more.
class KillCountObjective : public Objective
{
public:
    explicit KillCountObjective(int target);

    void onEnemyKilled(EnemyType) override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

private:
    int m_target;
    int m_killed;
};

// Stay alive for a set time. Spawning stops the moment the clock runs out, so
// the level ends on a breath rather than mid swarm.
class SurviveTimeObjective : public Objective
{
public:
    explicit SurviveTimeObjective(float seconds);

    void update(float deltaTime) override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;
    bool wantsSpawning() const override;

private:
    float m_target;
    float m_elapsed;
};

// Find and burn out every nest in the basin. Nothing about this one is solved
// by holding the middle and turning: the nests are cemented to rock out at the
// edges, they are not all in sight at once, and they keep hatching until they
// are dead. It is the level type that makes the arena somewhere to go through
// rather than somewhere to stand.
class DestroyNestsObjective : public Objective
{
public:
    explicit DestroyNestsObjective(int total);

    void onNestDestroyed() override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

    // The creatures a nest hatches are not the level's roster; the level stops
    // sending its own the moment the last nest is out.
    bool wantsSpawning() const override;

private:
    int m_total;
    int m_destroyed;
};

// Haul the salvage back. The pods are dropped at the far ends of the basin and
// the mouth is where the dive started, so the whole level is spent crossing
// water you have already crossed, slowly, lit up, with your hands full. Every
// other level type is answered by fighting well; this one is answered by
// deciding when to fight at all.
class HaulCargoObjective : public Objective
{
public:
    explicit HaulCargoObjective(int total);

    void onCargoDelivered() override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

private:
    int m_total;
    int m_delivered;
};

// Boss levels. Nothing else spawns and nothing else counts.
class DefeatBossObjective : public Objective
{
public:
    DefeatBossObjective();

    void onBossDefeated() override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;
    bool wantsSpawning() const override;

    void setCoreFraction(float fraction);

private:
    bool  m_defeated;
    float m_coreFraction; // 1 at full health, 0 when the core is gone
};
