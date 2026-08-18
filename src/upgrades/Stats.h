#pragma once

#include <array>
#include <bitset>
#include <cstddef>

// Numbers an upgrade can move. Adding one is an entry here plus a row in the
// tree, never a branch in the gameplay code.
enum class Stat
{
    ProjectileDamage,
    ProjectileSpeed,
    FireRate,            // multiplier: 1.2 means shots come 20% faster
    Pierce,              // extra creatures a jet passes through
    SplitCount,          // fragments spawned on impact
    MaxHealth,
    MoveSpeed,
    Armour,              // flat damage soaked, never down to zero
    InvulnerabilityTime, // seconds of mercy after a hit
    TorpedoDamage,
    TorpedoRate,
    ShieldCharges,
    ShieldRecharge,      // seconds to regrow one charge
    DashCooldown,
    SonarRange,
    DroneCount,
    MineDamage,
    BleedDamage,         // damage per second applied by a hit
    BleedTargetDamage,   // bonus multiplier against something already bleeding
    WeakPointDamage,     // bonus against boss parts and organs
    SalvageBonus,
    Count
};

// Behaviour an upgrade can switch on. A system asks for the one flag it cares
// about, so there is never a central switch listing every upgrade.
enum class Trait
{
    SplitShot,
    Torpedoes,
    HomingTorpedoes,
    ClusterTorpedoes,
    Dash,
    PressureBurst,   // dashing sends out a shockwave
    DashInvuln,      // dashing makes you briefly untouchable
    Shield,
    ReactiveArmour,  // a breaking shield hurts whatever is close
    EmergencyVent,   // survive one lethal hit per level
    Sonar,
    Bleed,
    ChainReaction,   // a destroyed boss part damages its neighbours
    PredatorInstinct,
    Drones,
    Mines,
    Count
};

// Hardware the sub visibly grows. Purely cosmetic on its own: the gameplay
// effect always comes from the stats and traits on the same node.
enum class ShipPart
{
    HeavyCannon,
    TwinCannon,
    TorpedoTube,
    SonarDish,
    ArmourPlates,
    BigEngine,
    ShieldBubble,
    Drone,
    Count
};

// One upgrade's contribution to one stat.
struct StatModifier
{
    Stat  stat{};
    float flat = 0.0f;
    float percent = 0.0f; // 0.25 means +25%
};

// A bag of accumulated modifiers, granted traits and fitted hardware. It knows
// nothing about upgrades, so the same container works for pickups or debuffs.
class Stats
{
public:
    Stats();

    void clear();
    void apply(const StatModifier& modifier);
    void grant(Trait trait);
    void fit(ShipPart part);

    // base is the unmodified value from Config: (base + flat) * (1 + percent).
    float get(Stat stat, float base) const;
    int getInt(Stat stat, int base) const;

    bool has(Trait trait) const;
    bool fitted(ShipPart part) const;

private:
    static constexpr std::size_t StatCount = static_cast<std::size_t>(Stat::Count);

    std::array<float, StatCount> m_flat{};
    std::array<float, StatCount> m_percent{};

    std::bitset<static_cast<std::size_t>(Trait::Count)>    m_traits;
    std::bitset<static_cast<std::size_t>(ShipPart::Count)> m_parts;
};
