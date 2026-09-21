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

    // A piece of salvage off the floor, and a thing in the water opened.
    // Both existed before any objective wanted them, which is why they arrive
    // as notifications rather than as something the objective goes looking for.
    virtual void onSalvageLifted() {}
    virtual void onWreckOpened() {}
    virtual void onRigFound() {}

    // The marked creature is dead, a beacon has been lit, something has taken
    // a bite out of the winch, and the sub is back at the mouth. Same rule as
    // the three above: the world already had all of these, so they arrive as
    // notifications rather than as something an objective reaches out for.
    virtual void onQuarryKilled() {}
    virtual void onMarkLit() {}
    virtual void onWardBitten() {}
    virtual void onReachedMouth() {}

    // deltaTime, and whether the lamp is off this instant. Every objective
    // that cares about the dark needs the second one and none of the others
    // are harmed by being handed it.
    virtual void update(float, bool) {}

    virtual bool isComplete() const = 0;

    // 0..1, for the progress bar.
    virtual float getProgress() const = 0;

    // Short line shown under the bar, e.g. "KILLS 7/12".
    virtual std::string getLabel() const = 0;

    // False once the level should stop feeding creatures in.
    virtual bool wantsSpawning() const { return true; }
};

// Fill the hold. Two thirds of the campaign used to be "kill everything" or
// "kill this many", which is the same level twice and neither of them is what
// anybody is down here for: nobody takes a submarine into a trench to reduce
// the population. This one asks for the thing the whole economy is built on,
// and it is answered by searching the basin rather than by holding the middle
// of it and turning.
class LiftQuotaObjective : public Objective
{
public:
    explicit LiftQuotaObjective(int target);

    void onSalvageLifted() override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

private:
    int m_target;
    int m_lifted;
};

// Work with the lamp off. The dark running mechanic has been in the game since
// the beginning and nothing has ever asked for it, so nobody finds out that
// with the lamp out a creature has to walk into you to know you are there.
// The clock only runs while it is off, so it is a level spent deciding how
// long you can stand not being able to see.
class RunDarkObjective : public Objective
{
public:
    explicit RunDarkObjective(float seconds);

    void update(float deltaTime, bool lampOff) override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

private:
    float m_target;
    float m_elapsed;
};

// Cut open the sealed hulls. They are the loudest thing a sub can do and they
// are always in the far rooms, so this is a level that has to be crossed and
// then survived at the other end.
class OpenWrecksObjective : public Objective
{
public:
    explicit OpenWrecksObjective(int total);

    void onWreckOpened() override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

private:
    int m_total;
    int m_opened;
};

// Find what is left of the diver who came down before you, and take their
// recorder off them. It is the only objective in the game that pays nothing
// and the only one that answers a question, which is exactly the trade: the
// company wants the ore and you want to know what happened to the third sub.
class RecoverRigObjective : public Objective
{
public:
    RecoverRigObjective();

    void onRigFound() override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

private:
    bool m_found;
};

// Stay alive for a set time. Spawning stops the moment the clock runs out, so
// the level ends on a breath rather than mid swarm.
class SurviveTimeObjective : public Objective
{
public:
    explicit SurviveTimeObjective(float seconds);

    void update(float deltaTime, bool lampOff) override;
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

// Hunt one creature that does not want to be found. Everything else in the
// basin behaves exactly as it always does; the quarry runs, routes round
// corners to keep doing it, and is the only thing in the water that counts.
// It is the one level type where the answer is to corner something rather
// than to survive it, so the arena reads as a place with somewhere to hide
// in it instead of as a room that fills up.
class HuntObjective : public Objective
{
public:
    HuntObjective();

    void onQuarryKilled() override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

private:
    bool m_killed;
};

// Light the beacons somebody else left behind. Each one is lit by holding
// station next to it, which is the one thing this game never otherwise asks
// for: every other objective is answered while moving, and standing still in
// open water for six seconds with the water filling up behind you is a
// different kind of nerve.
class RestoreObjective : public Objective
{
public:
    explicit RestoreObjective(int total);

    void onMarkLit() override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

private:
    int m_total;
    int m_lit;
};

// Keep them off the winch while it lifts. There is no losing it - a thing the
// player can fail without dying is a thing that wastes the last four minutes
// of their life - so instead every bite knocks the clock back. Defend it
// badly and the level simply goes on, which is its own punishment, and the
// pressure comes from watching the number go the wrong way.
class DefendObjective : public Objective
{
public:
    DefendObjective(float seconds, float setback);

    void update(float deltaTime, bool lampOff) override;
    void onWardBitten() override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

private:
    float m_target;
    float m_setback;
    float m_lifted;
};

// Get out. The sub is dropped at the far end of the basin instead of at the
// mouth, nothing down there has to be killed and nothing has to be carried -
// the entire level is the swim back, against water that is only getting
// worse. It is the shortest kind of level in the game and the only one whose
// answer is to refuse a fight.
class EscapeObjective : public Objective
{
public:
    EscapeObjective();

    void onReachedMouth() override;
    bool isComplete() const override;
    float getProgress() const override;
    std::string getLabel() const override;

    // Nothing about escaping thins the water out.
    void setDistance(float fraction);

private:
    bool  m_out;
    float m_fraction; // 1 at the far end, 0 at the mouth
};
