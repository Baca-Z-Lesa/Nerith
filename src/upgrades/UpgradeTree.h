#pragma once

#include "upgrades/Stats.h"

#include <SFML/System/Vector2.hpp>

#include <cstddef>
#include <string>
#include <vector>

// Everything about an upgrade is data: what it costs, how deep you must have
// been to buy it, what numbers it moves, what behaviour it switches on and what
// it bolts onto the hull. Adding one is a row, never a change to the systems
// that read it.
struct UpgradeNode
{
    std::string id;
    std::string name;
    std::string description;
    int cost = 0;
    int requiredDepth = 1; // cannot be bought until the run has reached it
    int column = -1;       // branch column the screen lays it out in
    std::vector<std::string>  prerequisites;
    std::vector<StatModifier> effects;
    std::vector<Trait>        traits;
    std::vector<ShipPart>     visuals;
    sf::Vector2f position;
};

inline constexpr int UpgradeBranchCount = 7;

// A directed graph of upgrades plus which of them the player has bought.
class UpgradeTree
{
public:
    UpgradeTree();

    const std::vector<UpgradeNode>& getNodes() const { return m_nodes; }
    static const char* getBranchName(int column);

    bool isUnlocked(std::size_t index) const;
    bool arePrerequisitesMet(std::size_t index) const;
    bool isDepthMet(std::size_t index, int depth) const;
    bool canAfford(std::size_t index, int salvage) const;
    bool canUnlock(std::size_t index, int salvage, int depth) const;

    // Spends the cost and unlocks, if it is allowed. False changes nothing.
    bool tryUnlock(std::size_t index, int& salvage, int depth);

    void reset();

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

    std::vector<UpgradeNode> m_nodes;
    // char rather than bool: std::vector<bool> is a bitfield and hands out
    // proxies instead of references, which is a trap waiting to happen.
    std::vector<char> m_unlocked;
};
