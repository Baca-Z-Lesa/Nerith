#include "upgrades/Stats.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace
{
    constexpr float Uncapped = std::numeric_limits<float>::infinity();

    // What a stat is never allowed past, however many branches feed it.
    //
    // The tree has nine roots and most of them touch the same handful of
    // numbers, so a player who owns enough of it does not get a strong sub -
    // they get one nothing in the water can interact with. Six bounces on a
    // jet that also pierces clears a room without being aimed; an eight times
    // charged shot deletes a boss; seventy percent crit on top of both means
    // the numbers stopped meaning anything two branches ago.
    //
    // Every ceiling below is set so that finishing ONE line never reaches it.
    // What gets clipped is owning three lines at once, which is the thing the
    // pricing is already trying to prevent and this makes impossible rather
    // than merely expensive. Nothing here caps health, speed, damage or fire
    // rate: those scale a fight, they do not switch it off.
    constexpr std::array<float, static_cast<std::size_t>(Stat::Count)> Ceiling = [] {
        std::array<float, static_cast<std::size_t>(Stat::Count)> caps{};
        caps.fill(Uncapped);

        const auto cap = [&caps](Stat stat, float most) {
            caps[static_cast<std::size_t>(stat)] = most;
        };

        cap(Stat::Armour, 6.0f);              // all of them together came to 12
        cap(Stat::InvulnerabilityTime, 1.5f); // 2.5s of mercy is not a fight
        cap(Stat::ChargeMultiplier, 6.0f);    // was 8, against bosses of 40 health
        cap(Stat::RicochetCount, 4.0f);       // was 6, and they pierce as well
        cap(Stat::Pierce, 5.0f);              // was 7
        cap(Stat::CritChance, 0.50f);         // was 0.70
        cap(Stat::CritDamage, 1.20f);
        cap(Stat::LifestealChance, 0.66f); // was 0.99: every kill, guaranteed
        cap(Stat::ExecuteThreshold, 0.25f);
        cap(Stat::DroneCount, 5.0f);
        cap(Stat::MineCount, 5.0f);

        // An economy stat rather than a combat one, and the worst of them:
        // +230% on top of a streak multiplier of 2.2 is seven times the
        // salvage for the same dive.
        cap(Stat::SalvageBonus, 1.20f);

        return caps;
    }();
}

Stats::Stats()
{
    clear();
}

void Stats::clear()
{
    m_flat.fill(0.0f);
    m_percent.fill(0.0f);
    m_traits.reset();
    m_parts.reset();
}

void Stats::apply(const StatModifier& modifier)
{
    const auto index = static_cast<std::size_t>(modifier.stat);
    if (index >= StatCount)
        return;

    // Flats add, percentages add before being applied once. Two +20% upgrades
    // give +40%, not +44%, which keeps the tree easy to reason about.
    m_flat[index] += modifier.flat;
    m_percent[index] += modifier.percent;
}

void Stats::grant(Trait trait)
{
    m_traits.set(static_cast<std::size_t>(trait));
}

void Stats::fit(ShipPart part)
{
    m_parts.set(static_cast<std::size_t>(part));
}

float Stats::ceilingOf(Stat stat)
{
    const auto index = static_cast<std::size_t>(stat);
    return index < StatCount ? Ceiling[index] : Uncapped;
}

float Stats::get(Stat stat, float base) const
{
    const auto index = static_cast<std::size_t>(stat);
    if (index >= StatCount)
        return base;

    // Never let a stacked malus flip a stat negative, and never let a stacked
    // bonus run past its ceiling. Both rules live here rather than at the
    // twenty places that read a stat, because a rule you have to remember at
    // the call site is a rule that is already broken somewhere.
    const float value = std::max(0.0f, (base + m_flat[index]) * (1.0f + m_percent[index]));

    return std::min(value, Ceiling[index]);
}

int Stats::getInt(Stat stat, int base) const
{
    return static_cast<int>(std::lround(get(stat, static_cast<float>(base))));
}

bool Stats::has(Trait trait) const
{
    return m_traits.test(static_cast<std::size_t>(trait));
}

bool Stats::fitted(ShipPart part) const
{
    return m_parts.test(static_cast<std::size_t>(part));
}
