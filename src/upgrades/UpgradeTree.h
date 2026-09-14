#pragma once

#include "ui/UpgradeIcons.h"
#include "upgrades/Stats.h"

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <string>
#include <vector>

// Everything about an upgrade is data: what it costs, what numbers it moves,
// what behaviour it switches on and what it bolts onto the hull. Adding one is
// a row, never a change to the systems that read it.
//
// The cost written in the table is a weight rather than a price; the tree
// stretches it into the real price when it builds, so the whole economy is one
// curve in one place.
struct UpgradeNode
{
    std::string id;
    std::string name;
    std::string description;
    int cost = 0;
    int column = -1;          // branch the screen lays it out in
    Icon icon = Icon::None;   // the mark on the disc, instead of initials
    std::vector<std::string>  prerequisites;
    std::vector<StatModifier> effects;
    std::vector<Trait>        traits;
    std::vector<ShipPart>     visuals;
    sf::Vector2f position;
};

// Nine roots, and each one splits in two partway out, so a branch is a choice
// rather than a queue: committing to a root still leaves a fork in it. Three
// nodes of trunk and four of limb, which is eleven a branch and ninety nine in
// all: the last ring went in with the ten levels under the abyss, because a
// depth that asks more of the sub has to have more sub to ask for.
inline constexpr int UpgradeBranchCount = 9;
inline constexpr int UpgradeTrunkLength = 3;
inline constexpr int UpgradeLimbLength = 4;

// A directed graph of upgrades plus which of them the player has bought.
class UpgradeTree
{
public:
    UpgradeTree();

    const std::vector<UpgradeNode>& getNodes() const { return m_nodes; }
    static const char* getBranchName(int column);

    // The closest any two nodes come to each other. The screen draws discs of a
    // fixed radius, so this is what proves they cannot overlap; the test suite
    // asserts on it rather than trusting the layout by eye.
    float getClosestPair() const;

    bool isUnlocked(std::size_t index) const;
    bool arePrerequisitesMet(std::size_t index) const;
    bool canAfford(std::size_t index, int salvage) const;
    bool canUnlock(std::size_t index, int salvage) const;

    // Spends the cost and unlocks, if it is allowed. False changes nothing.
    bool tryUnlock(std::size_t index, int& salvage);

    void reset();

    // What everything currently bolted on cost, and stripping it all off for
    // most of that back. A bad build is recoverable, at a price.
    int getSpent() const;

    // What the whole tree costs, so what is bolted on can be read as a
    // fraction of a finished sub rather than as a number on its own.
    int getTotalCost() const;
    int stripForRefund();

    std::size_t getUnlockedCount() const;

    // Round trips through the save file. Ids are used rather than indices so a
    // save survives nodes being reordered or inserted.
    std::vector<std::string> getUnlockedIds() const;
    void setUnlockedIds(const std::vector<std::string>& ids);

    // Pours every unlocked node's effects, traits and hardware into the block.
    void applyTo(Stats& stats) const;

private:
    void build();
    std::size_t indexOf(const std::string& id) const;

public:
    // The screen needs to look a node up by id when it draws the detail card.
    std::size_t indexOfNode(const std::string& id) const { return indexOf(id); }

private:

    std::vector<UpgradeNode> m_nodes;
    // char rather than bool: std::vector<bool> is a bitfield and hands out
    // proxies instead of references, which is a trap waiting to happen.
    std::vector<char> m_unlocked;
};
