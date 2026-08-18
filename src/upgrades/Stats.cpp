#include "upgrades/Stats.h"

#include <algorithm>
#include <cmath>

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

float Stats::get(Stat stat, float base) const
{
    const auto index = static_cast<std::size_t>(stat);
    if (index >= StatCount)
        return base;

    // Never let a stacked malus flip a stat negative.
    return std::max(0.0f, (base + m_flat[index]) * (1.0f + m_percent[index]));
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
