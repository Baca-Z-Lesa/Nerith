// ===========================================================================
//  THE BALANCE TABLE
//
//  Prints what the campaign asks of the player and what it pays them, level by
//  level, worked out from the same tables the game reads. It plays nothing and
//  simulates nothing: every number here is arithmetic over LevelDef, the enemy
//  table, the salvage table and the upgrade tree.
//
//  That is the point. A bot good enough to actually finish this game's
//  objectives is a second game, and the one written for this job spent five
//  minutes a level fighting whatever swam past instead of doing the job - so
//  every reading was a fact about the bot. Arithmetic over the tables cannot
//  be wrong about the tables.
//
//  What it cannot tell you is whether a level is fun, or whether a particular
//  creature is miserable to fight. That is what playing it is for.
//
//  Build and run it with tools/balance.ps1.
// ===========================================================================
#include "core/Config.h"
#include "upgrades/UpgradeTree.h"
#include "entities/BossDef.h"
#include "world/LevelDef.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <cstdio>

namespace
{
    // --- what the floor is worth down here -------------------------------
    //
    // A basin holds FindsBase + FindsPerDepth per floor, and what each piece
    // turns out to be is rolled against the shares of everything that exists
    // at that depth. The mean is what a sweep of the floor pays.
    float meanFindValue(int depth)
    {
        float weight = 0.0f;
        float total = 0.0f;

        for (const config::SalvageDef& def : config::SalvageTable)
        {
            if (def.fromDepth > depth)
                continue;

            weight += def.share;
            total += def.share * static_cast<float>(def.value);
        }

        return weight > 0.0f ? total / weight : 0.0f;
    }

    int findsAt(int depth)
    {
        return config::FindsBase + config::FindsPerDepth * (depth - 1);
    }

    // --- what the water is made of ---------------------------------------
    //
    // The script is a loop, so what matters is not its length but its average
    // member: one spawn every interval, forever, until the job is done.
    struct Water
    {
        float health = 0.0f;  // mean, after the level's own scaling
        float damage = 0.0f;  // mean contact damage per bite
        float salvage = 0.0f; // mean paid per kill
        int   members = 0;
    };

    Water waterOf(const LevelDef& def)
    {
        Water water;

        for (const SpawnGroup& group : def.script)
        {
            if (group.count <= 0)
                continue;

            const config::EnemyStats& stats =
                config::EnemyTable[static_cast<std::size_t>(group.type)];

            const int count = static_cast<int>(std::lround(static_cast<float>(group.count)
                                                           * config::GlobalCountScale));

            water.health += static_cast<float>(count * stats.health) * def.scaling.health;
            water.damage +=
                static_cast<float>(count * (stats.contactDamage + def.scaling.bonusDamage));
            water.salvage += static_cast<float>(count * stats.salvage);
            water.members += count;
        }

        if (water.members > 0)
        {
            const float n = static_cast<float>(water.members);
            water.health /= n;
            water.damage /= n;
            water.salvage /= n;
        }

        return water;
    }

    // How long a level lasts, which is the one thing the tables do not say.
    // Two of the objectives name their own clock; the rest are given the same
    // nominal minute and a half so that levels are compared against each other
    // rather than against a guess that changes per row.
    constexpr float NominalDive = 95.0f;

    float lengthOf(const LevelDef& def)
    {
        if (def.objective == ObjectiveKind::SurviveTime
            || def.objective == ObjectiveKind::RunDark)
        {
            return static_cast<float>(def.objectiveAmount);
        }

        return NominalDive;
    }

    // --- what one level pays ---------------------------------------------
    struct Pay
    {
        int clear = 0; // for opening the water
        int kills = 0;
        int extra = 0; // nests, pods, the creature at the bottom
        int floor = 0; // the hold, sold on the deck
        int total = 0;
    };

    Pay payOf(int level)
    {
        const LevelDef& def = levelFor(level);
        const int depth = depthOf(level);
        const Water water = waterOf(def);

        Pay pay;
        pay.clear = config::FirstClearBase + config::FirstClearPerLevel * level;

        if (!def.isBoss && water.members > 0)
        {
            const float interval = def.spawnInterval * config::GlobalIntervalScale;
            const float spawns = lengthOf(def) / std::max(0.05f, interval);

            // Capped by how many the basin will hold at once: a level cannot
            // pay for more creatures than it is willing to put in the water.
            const float reachable =
                std::min(spawns, static_cast<float>(def.maxAlive + config::GlobalMaxAliveBonus)
                                     * (lengthOf(def) / 12.0f));

            pay.kills = static_cast<int>(reachable * water.salvage);
        }

        switch (def.objective)
        {
            case ObjectiveKind::DestroyNests:
                pay.extra = def.objectiveAmount * config::NestSalvage;
                break;
            case ObjectiveKind::HaulCargo:
                pay.extra = def.objectiveAmount * static_cast<int>(config::CargoSalvage);
                break;
            default:
                break;
        }

        pay.floor = static_cast<int>(static_cast<float>(findsAt(depth)) * meanFindValue(depth));
        pay.total = pay.clear + pay.kills + pay.extra + pay.floor;

        return pay;
    }

    // --- what one level asks ----------------------------------------------
    //
    // Two numbers, both per second and both against a sub of a known size, so
    // that "harder" means something you can put on an axis.
    struct Ask
    {
        float bite = 0.0f;  // hull points a second the water can land
        float grind = 0.0f; // enemy health a second you have to get through
        float alive = 0.0f;
    };

    Ask askOf(int level)
    {
        const LevelDef& def = levelFor(level);
        const Water water = waterOf(def);

        Ask ask;
        ask.alive = static_cast<float>(def.maxAlive + config::GlobalMaxAliveBonus);

        if (def.isBoss || water.members == 0)
            return ask;

        const float interval = def.spawnInterval * config::GlobalIntervalScale;

        // A creature bites about once a second while it is on you, and the
        // invulnerability window is what decides how much of that lands.
        ask.bite = ask.alive * water.damage
                   / std::max(0.2f, config::PlayerInvulnerabilityDuration + 0.4f);
        ask.grind = water.health / std::max(0.05f, interval) * ask.alive;

        return ask;
    }

    // --- how long a boss fight actually takes -----------------------------
    //
    // The boss table was balanced against an economy where a clean run bought
    // the tree six times over, so by the first boss a player owned most of the
    // weapon line. They now own about eight of the cheapest nodes, and nothing
    // in the boss table moved - which is why the first creature in the game
    // was a three minute fight at four hull.
    //
    // Every hit anywhere on a boss goes straight to one pool (Boss::tryHit),
    // so the length of a fight is health over damage over the fraction of the
    // time it can be hit at all.

    // What the sub is putting out by the time it meets each one. Base is one
    // damage every 0.21s, which is 4.8 a second; the rest is what the running
    // income up to that level can buy off the weapon line at the prices in
    // UpgradeTree. Re-derive these if either the income or priceOf moves.
    constexpr std::array<float, 6> DamagePerSecond{ { 7.0f, 11.0f, 15.0f, 20.0f, 26.0f, 32.0f } };

    // And how much of a fight the creature can be shot during. A burrower is
    // under the floor for a good part of its own fight, and health it cannot
    // lose is health that only shows up as length.
    float exposureOf(BossMind mind)
    {
        return mind == BossMind::Burrower ? 0.55f : 1.0f;
    }

    // What each fight should take. Rising, because the first boss is teaching
    // and the last one is the end of the game.
    constexpr std::array<float, 6> WantedSeconds{
        { 45.0f, 55.0f, 70.0f, 85.0f, 100.0f, 120.0f }
    };
}

int main()
{
    std::printf("NERITH BALANCE TABLE\n");
    std::printf("counts x%.2f  intervals x%.2f  +%zu alive  hull %d  mercy %.2fs\n\n",
                config::GlobalCountScale, config::GlobalIntervalScale,
                config::GlobalMaxAliveBonus, config::PlayerMaxHealth,
                config::PlayerInvulnerabilityDuration);

    std::printf("%3s %-18s %-12s %5s %5s %6s %6s %6s %6s %7s %8s\n",
                "LVL", "TITLE", "GOAL", "ALIVE", "BITE", "GRIND", "CLEAR", "KILLS", "FLOOR",
                "LEVEL", "RUNNING");

    // Indexed by ObjectiveKind, so the order has to match it.
    const char* goalName[]{ "lift",  "dark",    "wrecks", "rig",
                            "last",  "nests",   "haul",
                            "hunt",  "restore", "defend", "escape",
                            "boss" };

    int running = 0;

    for (int level = 1; level <= authoredLevelCount(); ++level)
    {
        const LevelDef& def = levelFor(level);
        const Pay pay = payOf(level);
        const Ask ask = askOf(level);

        running += pay.total;

        std::printf("%3d %-18.18s %-12s %5.0f %5.1f %6.1f %6d %6d %6d %7d %8d\n", level,
                    def.title, goalName[static_cast<std::size_t>(def.objective)], ask.alive,
                    ask.bite, ask.grind, pay.clear, pay.kills + pay.extra, pay.floor,
                    pay.total, running);

        if (level % LevelsPerDepth == 0)
            std::printf("\n");
    }

    UpgradeTree tree;
    int treeTotal = 0;
    int cheapest = 1000000;
    int dearest = 0;

    for (const UpgradeNode& node : tree.getNodes())
    {
        treeTotal += node.cost;

        if (node.cost > 0)
            cheapest = std::min(cheapest, node.cost);

        dearest = std::max(dearest, node.cost);
    }

    std::printf("THE TREE\n");
    std::printf("  %zu nodes, %d salvage in all\n", tree.getNodes().size(), treeTotal);
    std::printf("  the cheapest node is %d, the dearest %d\n\n", cheapest, dearest);

    std::printf("THE VERDICT\n");
    std::printf("  a clean campaign earns  %d\n", running);
    std::printf("  the whole tree costs    %d\n", treeTotal);
    std::printf("  so one run buys         %.0f%% of it\n\n",
                100.0f * static_cast<float>(running) / static_cast<float>(treeTotal));

    // Where that money comes from matters as much as how much of it there is:
    // if one channel is most of the income, the other two are decoration.
    int clear = 0;
    int kills = 0;
    int floorPay = 0;

    for (int level = 1; level <= authoredLevelCount(); ++level)
    {
        const Pay pay = payOf(level);
        clear += pay.clear;
        kills += pay.kills + pay.extra;
        floorPay += pay.floor;
    }

    const float all = static_cast<float>(std::max(1, clear + kills + floorPay));
    std::printf("\nTHE SIX\n");
    std::printf("%-20s %5s %5s %5s %7s %8s %8s\n", "", "DEPTH", "CORE", "LIVES", "SHOWN",
                "SECONDS", "WANTED");

    for (int i = 0; i < static_cast<int>(BossId::Count); ++i)
    {
        const BossDef& def = bossDefFor(static_cast<BossId>(i));
        const std::size_t depth = static_cast<std::size_t>(i);

        const float shown = exposureOf(def.mind);
        const float pool = static_cast<float>(def.coreHealth * std::max(1, def.lives));
        const float seconds = pool / (DamagePerSecond[depth] * shown);

        std::printf("%-20s %5zu %5d %5d %6.0f%% %8.0f %8.0f\n", def.name, depth + 1,
                    def.coreHealth, def.lives, shown * 100.0f, seconds, WantedSeconds[depth]);
    }

    std::printf("\n  core health that would hit those:  ");
    for (int i = 0; i < static_cast<int>(BossId::Count); ++i)
    {
        const BossDef& def = bossDefFor(static_cast<BossId>(i));
        const std::size_t depth = static_cast<std::size_t>(i);

        const float want = WantedSeconds[depth] * DamagePerSecond[depth] * exposureOf(def.mind)
                           / static_cast<float>(std::max(1, def.lives));

        std::printf("%d ", static_cast<int>(std::lround(want / 10.0f) * 10));
    }
    std::printf("\n");

    std::printf("WHERE IT COMES FROM\n");
    std::printf("  opening water  %6d  %4.0f%%\n", clear, 100.0f * clear / all);
    std::printf("  killing things %6d  %4.0f%%\n", kills, 100.0f * kills / all);
    std::printf("  the floor      %6d  %4.0f%%\n", floorPay, 100.0f * floorPay / all);

    return 0;
}
