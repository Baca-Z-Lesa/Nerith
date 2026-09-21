// Tests for the parts of the game that are pure arithmetic over tables: the
// stat block, the campaign table and the upgrade tree. Those are where a wrong
// number is invisible until someone plays twenty levels and finds the damage is
// half what it should be; everything else in the game is easier to check by
// looking at it than by asserting about it.
//
// No framework on purpose. Run it with tests/run_tests.ps1.

#include "upgrades/Stats.h"
#include "upgrades/UpgradeTree.h"
#include "world/LevelDef.h"
#include "core/Config.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <string>

namespace
{
    int failures = 0;
    int checks = 0;

    void check(bool condition, const char* what)
    {
        ++checks;
        if (condition)
            return;

        ++failures;
        std::printf("  FAIL  %s\n", what);
    }

    void checkNear(float actual, float expected, const char* what)
    {
        check(std::abs(actual - expected) < 0.0005f, what);
        if (std::abs(actual - expected) >= 0.0005f)
            std::printf("        expected %g, got %g\n", expected, actual);
    }

    // ---------------------------------------------------------------- stats
    void testStats()
    {
        std::printf("stats\n");

        Stats stats;
        stats.clear();
        checkNear(stats.get(Stat::ProjectileDamage, 3.0f), 3.0f,
                  "an empty block returns the fallback");

        // value = (base + flat) * (1 + percent), which is the rule the whole
        // tree is balanced against. Flat first, so a percentage upgrade is
        // worth more the more flat damage you already have.
        stats.apply({ Stat::ProjectileDamage, 2.0f, 0.0f });
        stats.apply({ Stat::ProjectileDamage, 0.0f, 0.50f });
        checkNear(stats.get(Stat::ProjectileDamage, 4.0f), 9.0f,
                  "flat is applied before percent");

        // Percentages from separate upgrades add rather than compound, so ten
        // of them cannot quietly become a multiplier of six.
        stats.clear();
        stats.apply({ Stat::FireRate, 0.0f, 0.25f });
        stats.apply({ Stat::FireRate, 0.0f, 0.25f });
        checkNear(stats.get(Stat::FireRate, 1.0f), 1.5f, "percentages add, not compound");

        stats.clear();
        stats.apply({ Stat::MoveSpeed, 0.0f, -3.0f });
        check(stats.get(Stat::MoveSpeed, 100.0f) >= 0.0f,
              "a big negative percent cannot drive a stat below zero");

        stats.clear();
        check(!stats.has(Trait::Torpedoes), "traits start off");
        stats.grant(Trait::Torpedoes);
        check(stats.has(Trait::Torpedoes), "a granted trait reads back");
        check(!stats.fitted(ShipPart::TorpedoTube), "hardware is separate from traits");
        stats.fit(ShipPart::TorpedoTube);
        check(stats.fitted(ShipPart::TorpedoTube), "fitted hardware reads back");
        stats.clear();
        check(!stats.has(Trait::Torpedoes) && !stats.fitted(ShipPart::TorpedoTube),
              "clear takes the traits and the hardware with it");
    }

    // ----------------------------------------------------------- level table
    void testLevels()
    {
        std::printf("levels\n");

        check(depthOf(1) == 1 && depthOf(5) == 1, "the first five levels are depth one");
        check(depthOf(6) == 2, "the sixth level is depth two");
        check(levelWithinDepth(6) == 1, "and it is the first of that depth");
        check(isBossLevel(5) && isBossLevel(20), "every fifth level is a boss");
        check(!isBossLevel(4) && !isBossLevel(6), "and nothing else is");

        check(authoredLevelCount() % LevelsPerDepth == 0,
              "the authored campaign ends on a boss");

        // Every authored level has to be finishable: an objective that asks for
        // a count needs a positive count, and one that asks for a roster needs
        // something in the script to kill.
        for (int level = 1; level <= authoredLevelCount(); ++level)
        {
            const LevelDef def = levelFor(level);
            const std::string where = "level " + std::to_string(level);

            check(def.title != nullptr && def.title[0] != '\0',
                  (where + " has a title").c_str());
            check(def.isBoss == isBossLevel(level),
                  (where + " agrees with isBossLevel").c_str());
            check(def.maxAlive > 0, (where + " lets something be alive").c_str());
            check(def.scaling.health > 0.0f, (where + " does not zero out health").c_str());

            int scripted = 0;
            for (const SpawnGroup& group : def.script)
                scripted += group.count;

            switch (def.objective)
            {
                case ObjectiveKind::LiftQuota:
                    check(def.objectiveAmount > 0,
                          (where + " asks for some salvage").c_str());

                    // The floor of a basin holds FindsBase plus three a depth.
                    // A quota above that is a level that cannot be finished,
                    // which is the one bug in a goal nobody finds until they
                    // have played it for ten minutes.
                    check(def.objectiveAmount
                              <= config::FindsBase
                                     + config::FindsPerDepth * (depthOf(level) - 1),
                          (where + " does not ask for more salvage than the floor holds")
                              .c_str());
                    break;
                case ObjectiveKind::RunDark:
                    check(def.objectiveAmount > 0, (where + " has a clock").c_str());
                    break;
                case ObjectiveKind::OpenWrecks:
                    check(def.objectiveAmount > 0,
                          (where + " asks for at least one hull").c_str());

                    // A basin only ever places OffersMost of anything, and a
                    // wreck is one of three kinds it rolls between.
                    check(def.objectiveAmount <= 3,
                          (where + " does not ask for more hulls than a basin rolls")
                              .c_str());
                    break;
                case ObjectiveKind::RecoverRig:
                    check(def.objectiveAmount == 0,
                          (where + " needs no number to find one man").c_str());
                    break;
                case ObjectiveKind::SurviveTime:
                    check(def.objectiveAmount > 0, (where + " has a clock").c_str());
                    break;
                case ObjectiveKind::DestroyNests:
                    check(def.objectiveAmount > 0,
                          (where + " hides at least one nest").c_str());
                    check(def.objectiveAmount <= 9,
                          (where + " does not hide more nests than fit round the basin").c_str());
                    break;
                case ObjectiveKind::HaulCargo:
                    check(def.objectiveAmount > 0,
                          (where + " drops at least one pod").c_str());
                    check(def.objectiveAmount <= config::RoomCount,
                          (where + " does not drop more pods than there are rooms").c_str());
                    check(def.spawnInterval > 0.0f,
                          (where + " keeps hunting you while you haul").c_str());
                    check(scripted > 0,
                          (where + " has something in the water to haul past").c_str());
                    break;
                case ObjectiveKind::Hunt:
                    check(def.objectiveAmount == 0,
                          (where + " needs no number to name one creature").c_str());
                    check(scripted > 0,
                          (where + " has a creature in the script to mark").c_str());
                    break;
                case ObjectiveKind::Restore:
                    check(def.objectiveAmount > 0,
                          (where + " sets out at least one beacon").c_str());

                    // One beacon per chamber, so a level asking for more than
                    // the basin has rooms would stack two in the same place
                    // and read as one that cannot be finished.
                    check(def.objectiveAmount <= config::RoomCount,
                          (where + " does not set out more beacons than there are rooms")
                              .c_str());
                    break;
                case ObjectiveKind::Defend:
                    check(def.objectiveAmount > 0, (where + " has a lift to finish").c_str());

                    // Every bite costs WardSetback, so a lift shorter than one
                    // setback could be finished before anything reached it.
                    check(static_cast<float>(def.objectiveAmount) > config::WardSetback * 4.0f,
                          (where + " lifts for long enough to be interrupted").c_str());
                    check(scripted > 0,
                          (where + " has something in the water to keep off it").c_str());
                    break;
                case ObjectiveKind::Escape:
                    check(def.objectiveAmount == 0,
                          (where + " needs no number to reach the mouth").c_str());
                    break;
                case ObjectiveKind::DefeatBoss:
                    check(def.isBoss, (where + " is flagged as a boss level").c_str());
                    break;
            }
        }

        // There is nothing past the campaign any more, and asking for a level
        // that does not exist has to be safe rather than undefined: the save
        // file is a text file a player can edit, and it names a level number.
        for (const int level : { authoredLevelCount() + 1, authoredLevelCount() + 500, 100000 })
        {
            const LevelDef def = levelFor(level);
            check(def.title == levelFor(authoredLevelCount()).title,
                  "past the campaign, every level is the last one");
        }

        check(levelFor(-5).title == levelFor(1).title, "and below it, the first");
    }

    // ----------------------------------------------------------- upgrade tree
    void testTree()
    {
        std::printf("upgrade tree\n");

        UpgradeTree tree;
        const std::vector<UpgradeNode>& nodes = tree.getNodes();

        check(!nodes.empty(), "the tree has nodes");

        // Overlap is ruled out by the layout's construction rather than by
        // eye. This is the assertion that keeps it that way: two discs of
        // NodeRadius cannot touch, with room left over for the outlines.
        const float needed = config::NodeRadius * 2.0f + 12.0f;
        const float closest = tree.getClosestPair();
        check(closest >= needed, "no two nodes come within a node's width of each other");
        if (closest < needed)
            std::printf("        closest pair is %.1f apart, needs %.1f\n", closest, needed);

        // Every node carries a drawn mark. A node with no icon would be a
        // blank disc, which is what the initials were replaced for.
        for (const UpgradeNode& node : nodes)
        {
            check(node.icon != Icon::None && node.icon != Icon::Count,
                  ("node " + node.id + " has an icon").c_str());
        }

        check(static_cast<int>(nodes.size())
                  == 1 + UpgradeBranchCount * (UpgradeTrunkLength + 2 * UpgradeLimbLength),
              "the table fills every slot the layout provides");

        std::set<std::string> ids;
        for (const UpgradeNode& node : nodes)
        {
            check(ids.insert(node.id).second, ("id " + node.id + " is unique").c_str());
            check(!node.name.empty(), ("node " + node.id + " has a name").c_str());
            check(!node.description.empty(),
                  ("node " + node.id + " says what it does").c_str());
            check(node.cost >= 0, ("node " + node.id + " costs nothing negative").c_str());
        }

        // Every prerequisite has to name a node that exists, or a branch is
        // silently unreachable and nobody notices until they try to buy it.
        for (const UpgradeNode& node : nodes)
        {
            for (const std::string& id : node.prerequisites)
            {
                check(ids.count(id) == 1,
                      ("node " + node.id + " requires a node that exists").c_str());
            }
        }

        // Only the root is free, and it is the only node with no prerequisite.
        // It also starts owned: it is the sub the player already has.
        int free = 0;
        int rootless = 0;
        for (const UpgradeNode& node : nodes)
        {
            if (node.cost == 0) ++free;
            if (node.prerequisites.empty()) ++rootless;
        }
        check(free == 1, "exactly one node is free");
        check(rootless == 1, "exactly one node hangs off nothing");
        check(tree.getUnlockedCount() == 1, "a fresh tree owns only the root");

        // Everything has to be reachable from the root by buying nodes one at a
        // time. This is the check that catches a typo in a prerequisite id.
        int salvage = 1000000;
        for (int pass = 0; pass < static_cast<int>(nodes.size()) + 1; ++pass)
        {
            bool progress = false;
            for (std::size_t i = 0; i < nodes.size(); ++i)
            {
                if (tree.isUnlocked(i) || !tree.canUnlock(i, salvage))
                    continue;

                tree.tryUnlock(i, salvage);
                progress = true;
            }
            if (!progress)
                break;
        }
        check(tree.getUnlockedCount() == nodes.size(),
              "every node is reachable from the root");

        // What it all cost has to match what came out of the wallet, or the
        // refund would hand back a different number than was spent.
        const int spent = 1000000 - salvage;
        check(tree.getSpent() == spent, "getSpent matches what was actually paid");

        const int refund = tree.stripForRefund();
        check(refund > 0 && refund < spent, "stripping the hull gives back most, not all");
        check(tree.getUnlockedCount() == 1, "and leaves nothing but the root installed");

        // Ids round trip, because a save holds ids rather than indices so that
        // inserting a node does not shuffle somebody's progress.
        UpgradeTree other;
        int wallet = 4000;
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            if (other.canUnlock(i, wallet))
                other.tryUnlock(i, wallet);
        }
        const std::vector<std::string> saved = other.getUnlockedIds();
        check(saved.size() > 1, "something was bought to round trip");

        UpgradeTree loaded;
        loaded.setUnlockedIds(saved);
        check(loaded.getUnlockedIds() == saved, "unlocked ids round trip through a save");

        // A node nobody has heard of must be ignored rather than crash: an old
        // save can name an upgrade that has since been renamed.
        UpgradeTree stale;
        stale.setUnlockedIds({ "core", "no_such_upgrade" });
        check(stale.getUnlockedCount() == 1, "an unknown id in a save is skipped");
    }

    // --------------------------------------------------------------- balance
    void testBalance()
    {
        std::printf("balance\n");

        // How much of the tree a campaign pays for is worked out by the
        // balance table in tools, which has the whole income model in it and
        // prints it. It is not asserted twice here: a model kept in two places
        // is a model that disagrees with itself by the third edit. What IS
        // asserted here is the handful of things that are true regardless of
        // how the income is tuned, and that break the game silently.
        UpgradeTree tree;
        int treeTotal = 0;
        for (const UpgradeNode& node : tree.getNodes())
            treeTotal += node.cost;

        int clearBonuses = 0;
        for (int level = 1; level <= authoredLevelCount(); ++level)
            clearBonuses += config::FirstClearBase + config::FirstClearPerLevel * level;

        check(treeTotal > clearBonuses * 5,
              "opening water alone never comes close to paying for the tree");

        // Slow at the top and steep at the bottom: the water may never get
        // lighter as the campaign goes on. Bosses are skipped - they put one
        // creature in the basin and nothing else.
        std::size_t previous = 0;
        for (int level = 1; level <= authoredLevelCount(); ++level)
        {
            const LevelDef& def = levelFor(level);
            if (def.isBoss)
                continue;

            check(def.maxAlive >= previous, "no level holds less water than the one before it");
            previous = def.maxAlive;
        }

        // The ceilings in Stats are the only thing between the tree and a sub
        // nothing in the water can interact with, so they have to actually
        // bite: stacking every node that feeds a capped stat must land ON the
        // cap rather than under it.
        Stats everything;
        for (const UpgradeNode& node : tree.getNodes())
        {
            for (const StatModifier& modifier : node.effects)
                everything.apply(modifier);
        }

        checkNear(everything.get(Stat::Armour, 0.0f), 6.0f, "armour is capped, and the cap bites");
        checkNear(everything.get(Stat::RicochetCount, 0.0f), 4.0f, "bounces are capped");
        checkNear(everything.get(Stat::ChargeMultiplier, 1.0f), 6.0f, "the charge coil is capped");
        checkNear(everything.get(Stat::CritChance, 0.0f), 0.50f, "crit chance is capped");
        checkNear(everything.get(Stat::SalvageBonus, 0.0f), 1.20f, "the salvage bonus is capped");

        check(config::RerunSalvageFactor > 0.0f && config::RerunSalvageFactor < 1.0f,
              "repeat water pays something, but less");
        check(config::RespecRefund > 0.5f && config::RespecRefund < 1.0f,
              "a strip gives back most of the salvage, never all of it");

        // The middle difficulty has to be the campaign exactly as balanced,
        // because that is what every other number here was tuned against.
        const config::DifficultyDef& normal = config::Difficulties[1];
        checkNear(normal.enemyHealth, 1.0f, "the middle difficulty does not scale health");
        checkNear(normal.enemyDamage, 1.0f, "the middle difficulty does not scale damage");
        checkNear(normal.countScale, 1.0f, "the middle difficulty does not scale counts");

        for (std::size_t i = 1; i < config::Difficulties.size(); ++i)
        {
            check(config::Difficulties[i].enemyHealth > config::Difficulties[i - 1].enemyHealth,
                  "each difficulty is harder than the one before it");
        }
    }
}

int main()
{
    std::printf("nerith tests\n\n");

    testStats();
    testLevels();
    testTree();
    testBalance();

    std::printf("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
