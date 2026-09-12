#include "upgrades/UpgradeTree.h"

#include "core/Config.h"

#include <algorithm>
#include <cmath>

namespace
{
    // The costs in the table below are a shape rather than a price. Stretching
    // that shape is what makes a branch a commitment instead of a queue: the
    // first node of a line is pocket change and the last is most of a depth's
    // takings, so nine branches cannot all be paid for. Nothing else gates a
    // node, which means this one curve is the whole difficulty of the tree and
    // the only number worth turning.
    int priceOf(int weight)
    {
        if (weight <= 0)
            return 0;

        // Tuned against the campaign's takings: a clean first run through all
        // twenty levels pays for roughly half the tree, so half of it is
        // always a road not taken.
        return static_cast<int>(
            std::lround(std::pow(static_cast<float>(weight), 1.22f) * 1.25f));
    }

    // ------------------------------------------------------------------------
    //  Layout
    //
    //  Nine roots leave the hull, run out three nodes, then fork into two limbs
    //  of three. Overlap is ruled out by construction rather than by nudging
    //  positions until it looks right:
    //
    //    - every subtree owns a disjoint wedge of angle, and a node sits at the
    //      middle of its own slot inside that wedge;
    //    - the circumference a wedge covers grows with distance, so the further
    //      out a node is, the more room its slot has;
    //    - consecutive rings are RingStep apart, which is already more than two
    //      node diameters.
    //
    //  The wobble is bounded to a fraction of a slot so it can bend a root
    //  without ever eating the gap the construction guarantees, and it is
    //  hashed from the node's own address in the tree so the shape is the same
    //  every run. UpgradeTree::getClosestPair reports the tightest pair and the
    //  tests assert on it.
    // ------------------------------------------------------------------------
    constexpr float FirstRing = 152.0f;
    constexpr float RingStep = 122.0f;
    constexpr float Sector = 360.0f / static_cast<float>(UpgradeBranchCount);

    constexpr float TrunkDrift = 0.055f; // per node, as a fraction of a sector
    constexpr float LimbSplay = 0.200f;  // how far a limb sits off the trunk
    constexpr float LimbSpread = 0.055f; // ...growing by this much per node
    constexpr float Wobble = 0.045f;     // bound on the hashed bend

    float hashedWobble(int branch, int limb, int index)
    {
        unsigned int h = static_cast<unsigned int>(branch) * 73856093u
                         ^ static_cast<unsigned int>(limb) * 19349663u
                         ^ static_cast<unsigned int>(index) * 83492791u;
        h ^= h >> 13;
        h *= 1274126177u;
        h ^= h >> 16;

        return static_cast<float>(h & 0xFFFFu) / 65535.0f * 2.0f - 1.0f;
    }

    // limb 0 is the trunk; 1 and 2 are the two limbs it forks into.
    sf::Vector2f cell(int branch, int limb, int index)
    {
        const int ring = limb == 0 ? index : UpgradeTrunkLength + index;
        const float distance = FirstRing + RingStep * static_cast<float>(ring);

        // All roots swirl the same way, so the drift never closes the gap
        // between one root and the next.
        float angle = -90.0f + Sector * static_cast<float>(branch)
                      + Sector * TrunkDrift * static_cast<float>(ring);

        if (limb != 0)
        {
            const float side = limb == 1 ? -1.0f : 1.0f;
            angle += side * Sector * (LimbSplay + LimbSpread * static_cast<float>(index));
        }

        angle += hashedWobble(branch, limb, index) * Sector * Wobble;

        return sf::Vector2f(distance, sf::degrees(angle));
    }

    const char* const BranchNames[UpgradeBranchCount] = {
        "WEAPONS", "TORPEDOES", "HULL", "ENGINE", "SONAR",
        "DEFENSE", "MUTATION", "SALVAGE", "CURRENT"
    };
}

const char* UpgradeTree::getBranchName(int column)
{
    if (column < 0 || column >= UpgradeBranchCount)
        return "";
    return BranchNames[column];
}

UpgradeTree::UpgradeTree()
{
    build();
    reset();
}

float UpgradeTree::getClosestPair() const
{
    float closest = 1.0e9f;

    for (std::size_t i = 0; i < m_nodes.size(); ++i)
    {
        for (std::size_t k = i + 1; k < m_nodes.size(); ++k)
        {
            const float gap = (m_nodes[i].position - m_nodes[k].position).length();
            closest = std::min(closest, gap);
        }
    }

    return m_nodes.size() < 2 ? 0.0f : closest;
}

void UpgradeTree::build()
{
    // Nine roots of nine. Nobody finishes more than two or three of them, so
    // which roots you commit to and which way each one forks is the whole
    // build. The only thing between the player and any node is the price.
    m_nodes = {
        { "core", "THE SUB", "What you start with. Everything hangs off it.",
          0, -1, Icon::Sub, {}, {}, {}, {}, { 0.0f, 0.0f } },

        // ==================== WEAPONS ====================
        // The jet stream. Trunk builds the gun; one limb makes it spray, the
        // other makes it a needle.
        { "wpn1", "GUN BAY", "+15% jet damage. Opens the weapon line.",
          3, 0, Icon::Muzzle, { "core" },
          { { Stat::ProjectileDamage, 0.0f, 0.15f } }, {}, {}, cell(0, 0, 0) },
        { "wpn2", "PRESSURE VALVE", "+25% jet damage.",
          6, 0, Icon::Jet, { "wpn1" },
          { { Stat::ProjectileDamage, 0.0f, 0.25f } }, {}, {}, cell(0, 0, 1) },
        { "wpn3", "RAPID PUMP", "+25% fire rate.",
          10, 0, Icon::RapidFire, { "wpn2" },
          { { Stat::FireRate, 0.0f, 0.25f } }, {}, {}, cell(0, 0, 2) },

        { "wpn4", "SPLITSHOT", "A jet that hits bursts into three fragments.",
          14, 0, Icon::Split, { "wpn3" },
          { { Stat::SplitCount, 3.0f, 0.0f } }, { Trait::SplitShot }, {}, cell(0, 1, 0) },
        { "wpn5", "SCATTER", "+2 fragments and +20% fire rate.",
          20, 0, Icon::Cluster, { "wpn4" },
          { { Stat::SplitCount, 2.0f, 0.0f }, { Stat::FireRate, 0.0f, 0.20f } },
          {}, {}, cell(0, 1, 1) },
        // The end of each weapon limb is a different gun rather than another
        // number. E cycles whatever is fitted, so which one is right is decided
        // in the water instead of in the hub.
        { "wpn6", "HARPOON RIG", "+1 damage. Fits the harpoon: a head on a line that drags you to what it hits.",
          28, 0, Icon::Harpoon, { "wpn5" },
          { { Stat::ProjectileDamage, 1.0f, 0.0f }, { Stat::FireRate, 0.0f, 0.20f } },
          { Trait::Harpoon }, { ShipPart::TwinCannon }, cell(0, 1, 2) },

        { "wpn7", "NARROW BORE", "+40% jet speed. Easier to lead a darter.",
          14, 0, Icon::Pierce, { "wpn3" },
          { { Stat::ProjectileSpeed, 0.0f, 0.40f } }, {}, {}, cell(0, 2, 0) },
        { "wpn8", "LANCE", "Jets punch through two extra creatures.",
          20, 0, Icon::Crosshair, { "wpn7" },
          { { Stat::Pierce, 2.0f, 0.0f } }, {}, {}, cell(0, 2, 1) },
        { "wpn9", "PRESSURE PULSE", "+2 damage. Fits the pulse: no shot at all, the water itself, and it leaves them cold.",
          28, 0, Icon::Pulse, { "wpn8" },
          { { Stat::ProjectileDamage, 2.0f, 0.0f }, { Stat::FireRate, 0.0f, -0.20f } },
          { Trait::Pulse }, { ShipPart::HeavyCannon }, cell(0, 2, 2) },

        // ==================== TORPEDOES ====================
        { "tor1", "TORPEDO TUBE", "Right mouse fires a slow, heavy torpedo that detonates.",
          3, 1, Icon::Torpedo, { "core" },
          {}, { Trait::Torpedoes }, { ShipPart::TorpedoTube }, cell(1, 0, 0) },
        { "tor2", "WARHEAD", "+50% torpedo damage.",
          6, 1, Icon::Warhead, { "tor1" },
          { { Stat::TorpedoDamage, 0.0f, 0.50f } }, {}, {}, cell(1, 0, 1) },
        { "tor3", "FAST LOAD", "+40% torpedo reload speed.",
          10, 1, Icon::Reload, { "tor2" },
          { { Stat::TorpedoRate, 0.0f, 0.40f } }, {}, {}, cell(1, 0, 2) },

        { "tor4", "CLUSTER", "A torpedo scatters six jets where it lands.",
          14, 1, Icon::Cluster, { "tor3" },
          {}, { Trait::ClusterTorpedoes }, {}, cell(1, 1, 0) },
        { "tor5", "SHAPED CHARGE", "+70 blast radius on every torpedo.",
          20, 1, Icon::Blast, { "tor4" },
          { { Stat::TorpedoBlast, 70.0f, 0.0f } }, {}, {}, cell(1, 1, 1) },
        { "tor6", "DEPTH CHARGE", "+90% torpedo damage.",
          28, 1, Icon::Warhead, { "tor5" },
          { { Stat::TorpedoDamage, 0.0f, 0.90f } }, {}, {}, cell(1, 1, 2) },

        { "tor7", "SEEKER", "Torpedoes steer towards whatever is closest.",
          14, 1, Icon::Seeker, { "tor3" },
          {}, { Trait::HomingTorpedoes }, {}, cell(1, 2, 0) },
        { "tor8", "SALVO", "+70% torpedo reload speed.",
          20, 1, Icon::Reload, { "tor7" },
          { { Stat::TorpedoRate, 0.0f, 0.70f } }, {}, {}, cell(1, 2, 1) },
        { "tor9", "DETONATOR", "+4 flat torpedo damage.",
          28, 1, Icon::Blast, { "tor8" },
          { { Stat::TorpedoDamage, 4.0f, 0.0f } }, {}, {}, cell(1, 2, 2) },

        // ==================== HULL ====================
        { "hul1", "BALLAST", "+2 maximum HP.",
          3, 2, Icon::Hull, { "core" },
          { { Stat::MaxHealth, 2.0f, 0.0f } }, {}, {}, cell(2, 0, 0) },
        { "hul2", "FRAME", "+3 maximum HP.",
          6, 2, Icon::Heart, { "hul1" },
          { { Stat::MaxHealth, 3.0f, 0.0f } }, {}, {}, cell(2, 0, 1) },
        { "hul3", "PRESSURE HULL", "+4 maximum HP.",
          10, 2, Icon::Hull, { "hul2" },
          { { Stat::MaxHealth, 4.0f, 0.0f } }, {}, {}, cell(2, 0, 2) },

        { "hul4", "ABLATIVE PLATE", "Soak 1 damage per hit. Bolts plating on.",
          14, 2, Icon::Plate, { "hul3" },
          { { Stat::Armour, 1.0f, 0.0f } }, {}, { ShipPart::ArmourPlates }, cell(2, 1, 0) },
        { "hul5", "SECOND SKIN", "Soak another 1 damage per hit.",
          20, 2, Icon::Plate, { "hul4" },
          { { Stat::Armour, 1.0f, 0.0f } }, {}, {}, cell(2, 1, 1) },
        { "hul6", "DEEP PLATE", "+6 maximum HP and 1 more soaked per hit.",
          28, 2, Icon::Plate, { "hul5" },
          { { Stat::MaxHealth, 6.0f, 0.0f }, { Stat::Armour, 1.0f, 0.0f } },
          {}, {}, cell(2, 1, 2) },

        { "hul7", "DAMAGE CONTROL", "+0.7s of mercy after a hit.",
          14, 2, Icon::Mercy, { "hul3" },
          { { Stat::InvulnerabilityTime, 0.7f, 0.0f } }, {}, {}, cell(2, 2, 0) },
        { "hul8", "EMERGENCY VENT", "Survive one otherwise fatal hit each level.",
          20, 2, Icon::Vent, { "hul7" },
          {}, { Trait::EmergencyVent }, {}, cell(2, 2, 1) },
        { "hul9", "REFIT CREW", "The hull comes back between dives: +4 HP repaired.",
          28, 2, Icon::Repair, { "hul8" },
          { { Stat::RepairPerLevel, 4.0f, 0.0f } }, { Trait::Refit }, {}, cell(2, 2, 2) },

        // ==================== ENGINE ====================
        { "eng1", "TRIM", "+12% move speed.",
          3, 3, Icon::Propeller, { "core" },
          { { Stat::MoveSpeed, 0.0f, 0.12f } }, {}, {}, cell(3, 0, 0) },
        { "eng2", "TWIN SCREW", "+18% move speed.",
          6, 3, Icon::Propeller, { "eng1" },
          { { Stat::MoveSpeed, 0.0f, 0.18f } }, {}, {}, cell(3, 0, 1) },
        { "eng3", "THRUST VECTOR", "Shift dashes. Fits a heavier engine block.",
          10, 3, Icon::Dash, { "eng2" },
          {}, { Trait::Dash }, { ShipPart::BigEngine }, cell(3, 0, 2) },

        { "eng4", "QUICK VENT", "-40% dash cooldown.",
          14, 3, Icon::Reload, { "eng3" },
          { { Stat::DashCooldown, 0.0f, -0.40f } }, {}, {}, cell(3, 1, 0) },
        { "eng5", "PRESSURE BURST", "A dash sends out a shockwave that hurts what it passes.",
          20, 3, Icon::Shock, { "eng4" },
          {}, { Trait::PressureBurst }, {}, cell(3, 1, 1) },
        { "eng6", "CAVITATION", "+25% move speed and -20% dash cooldown.",
          28, 3, Icon::Propeller, { "eng5" },
          { { Stat::MoveSpeed, 0.0f, 0.25f }, { Stat::DashCooldown, 0.0f, -0.20f } },
          {}, {}, cell(3, 1, 2) },

        { "eng7", "GHOST", "Nothing can touch you mid dash.",
          14, 3, Icon::Ghost, { "eng3" },
          {}, { Trait::DashInvuln }, {}, cell(3, 2, 0) },
        { "eng8", "SECOND WIND", "Below a quarter health you fire and move far faster.",
          20, 3, Icon::Dash, { "eng7" },
          {}, { Trait::SecondWind }, {}, cell(3, 2, 1) },
        { "eng9", "OVERDRIVE", "+30% fire rate and +15% move speed.",
          28, 3, Icon::RapidFire, { "eng8" },
          { { Stat::FireRate, 0.0f, 0.30f }, { Stat::MoveSpeed, 0.0f, 0.15f } },
          {}, {}, cell(3, 2, 2) },

        // ==================== SONAR ====================
        { "son1", "HYDROPHONE", "A ping every few seconds marks what the lamp cannot reach.",
          3, 4, Icon::Sonar, { "core" },
          { { Stat::SonarRange, 240.0f, 0.0f } }, { Trait::Sonar },
          { ShipPart::SonarDish }, cell(4, 0, 0) },
        { "son2", "ARRAY", "+200 sonar range.",
          6, 4, Icon::Sonar, { "son1" },
          { { Stat::SonarRange, 200.0f, 0.0f } }, {}, {}, cell(4, 0, 1) },
        { "son3", "DEEP ARRAY", "+280 sonar range.",
          10, 4, Icon::Map, { "son2" },
          { { Stat::SonarRange, 280.0f, 0.0f } }, {}, {}, cell(4, 0, 2) },

        { "son4", "WIDE SWEEP", "+340 sonar range and +12% jet speed.",
          14, 4, Icon::Map, { "son3" },
          { { Stat::SonarRange, 340.0f, 0.0f }, { Stat::ProjectileSpeed, 0.0f, 0.12f } },
          {}, {}, cell(4, 1, 0) },
        { "son5", "LANTERN", "A wider lamp. You see more of what is coming.",
          20, 4, Icon::Lamp, { "son4" },
          { { Stat::LampRadius, 120.0f, 0.0f } }, {}, {}, cell(4, 1, 1) },
        { "son6", "FLOOD LAMP", "A much wider lamp, and +260 sonar range with it.",
          28, 4, Icon::Beacon, { "son5" },
          { { Stat::LampRadius, 200.0f, 0.0f }, { Stat::SonarRange, 260.0f, 0.0f } },
          {}, {}, cell(4, 1, 2) },

        { "son7", "MARK", "+60% damage against boss parts and organs.",
          14, 4, Icon::Mark, { "son3" },
          { { Stat::WeakPointDamage, 0.0f, 0.60f } }, {}, {}, cell(4, 2, 0) },
        { "son8", "KILLSHOT", "A quarter of your jets land double.",
          20, 4, Icon::Crit, { "son7" },
          { { Stat::CritChance, 0.25f, 0.0f }, { Stat::CritDamage, 0.0f, 0.0f } },
          { Trait::Crit }, {}, cell(4, 2, 1) },
        { "son9", "COLD EYE", "+20% crit chance, and a crit does half again as much.",
          28, 4, Icon::Eye, { "son8" },
          { { Stat::CritChance, 0.20f, 0.0f }, { Stat::CritDamage, 0.50f, 0.0f } },
          {}, {}, cell(4, 2, 2) },

        // ==================== DEFENSE ====================
        { "def1", "FIELD COIL", "A bubble that eats one hit, then regrows.",
          3, 5, Icon::Shield, { "core" },
          { { Stat::ShieldCharges, 1.0f, 0.0f } }, { Trait::Shield },
          { ShipPart::ShieldBubble }, cell(5, 0, 0) },
        { "def2", "CAPACITOR", "-30% shield regrowth time.",
          6, 5, Icon::Reload, { "def1" },
          { { Stat::ShieldRecharge, 0.0f, -0.30f } }, {}, {}, cell(5, 0, 1) },
        { "def3", "SECOND CELL", "+1 shield charge.",
          10, 5, Icon::ShieldTwo, { "def2" },
          { { Stat::ShieldCharges, 1.0f, 0.0f } }, {}, {}, cell(5, 0, 2) },

        { "def4", "REACTIVE PLATING", "A breaking shield hurts whatever is standing on you.",
          14, 5, Icon::Reactive, { "def3" },
          {}, { Trait::ReactiveArmour }, {}, cell(5, 1, 0) },
        { "def5", "BULWARK", "+1 shield charge and -25% regrowth time.",
          20, 5, Icon::ShieldTwo, { "def4" },
          { { Stat::ShieldCharges, 1.0f, 0.0f }, { Stat::ShieldRecharge, 0.0f, -0.25f } },
          {}, {}, cell(5, 1, 1) },
        { "def6", "DEAD MAN", "Being bitten fires a ring of jets back out of the hull.",
          28, 5, Icon::Shock, { "def5" },
          {}, { Trait::Retaliate }, {}, cell(5, 1, 2) },

        { "def7", "MOTES", "Three motes circle the sub and eat incoming orbs.",
          14, 5, Icon::Orbit, { "def3" },
          { { Stat::OrbitCount, 3.0f, 0.0f } }, { Trait::OrbitShield }, {}, cell(5, 2, 0) },
        { "def8", "SWARM MOTES", "+3 motes.",
          20, 5, Icon::Orbit, { "def7" },
          { { Stat::OrbitCount, 3.0f, 0.0f } }, {}, {}, cell(5, 2, 1) },
        { "def9", "IRON TIDE", "+2 soaked per hit and +1s of mercy.",
          28, 5, Icon::Plate, { "def8" },
          { { Stat::Armour, 2.0f, 0.0f }, { Stat::InvulnerabilityTime, 1.0f, 0.0f } },
          {}, {}, cell(5, 2, 2) },

        // ==================== MUTATION ====================
        { "mut1", "SPORE", "Your hits leave a wound that keeps bleeding.",
          3, 6, Icon::Spore, { "core" },
          { { Stat::BleedDamage, 1.0f, 0.0f } }, { Trait::Bleed }, {}, cell(6, 0, 0) },
        { "mut2", "RUPTURE", "+160% bleed damage.",
          6, 6, Icon::Bleed, { "mut1" },
          { { Stat::BleedDamage, 0.0f, 1.60f } }, {}, {}, cell(6, 0, 1) },
        { "mut3", "CHAIN REACTION", "A destroyed boss part takes its neighbours with it.",
          10, 6, Icon::Corpse, { "mut2" },
          {}, { Trait::ChainReaction }, {}, cell(6, 0, 2) },

        { "mut4", "ESCORT", "A drone circles you and fires on its own.",
          14, 6, Icon::Drone, { "mut3" },
          { { Stat::DroneCount, 1.0f, 0.0f } }, { Trait::Drones },
          { ShipPart::Drone }, cell(6, 1, 0) },
        { "mut5", "HIVE", "+2 escort drones.",
          20, 6, Icon::Hive, { "mut4" },
          { { Stat::DroneCount, 2.0f, 0.0f } }, {}, {}, cell(6, 1, 1) },
        { "mut6", "DRONE TEETH", "+3 flat drone damage.",
          28, 6, Icon::Fang, { "mut5" },
          { { Stat::DroneDamage, 3.0f, 0.0f } }, {}, {}, cell(6, 1, 2) },

        { "mut7", "PREDATOR EYE", "Below half health you hit far harder.",
          14, 6, Icon::Predator, { "mut3" },
          {}, { Trait::PredatorInstinct }, {}, cell(6, 2, 0) },
        { "mut8", "CULL", "Anything already under a fifth health dies when you touch it.",
          20, 6, Icon::Fang, { "mut7" },
          { { Stat::ExecuteThreshold, 0.20f, 0.0f } }, { Trait::Execute }, {}, cell(6, 2, 1) },
        { "mut9", "ABYSSAL FORM", "+20% damage and 2 soaked, but -10% speed.",
          28, 6, Icon::Abyssal, { "mut8" },
          { { Stat::ProjectileDamage, 0.0f, 0.20f }, { Stat::Armour, 2.0f, 0.0f },
            { Stat::MoveSpeed, 0.0f, -0.10f } }, {}, {}, cell(6, 2, 2) },

        // ==================== SALVAGE ====================
        // Pays for the rest of the tree, and turns the wrecks you leave behind
        // into something other than debris.
        { "sal1", "MAGNET", "+20% salvage from everything you kill.",
          3, 7, Icon::Salvage, { "core" },
          { { Stat::SalvageBonus, 0.20f, 0.0f } }, {}, {}, cell(7, 0, 0) },
        { "sal2", "SCAVENGE", "+30% salvage.",
          6, 7, Icon::Scrap, { "sal1" },
          { { Stat::SalvageBonus, 0.30f, 0.0f } }, {}, {}, cell(7, 0, 1) },
        { "sal3", "WRECKER", "+40% salvage.",
          10, 7, Icon::Crate, { "sal2" },
          { { Stat::SalvageBonus, 0.40f, 0.0f } }, {}, {}, cell(7, 0, 2) },

        { "sal4", "CORPSE BLOOM", "A creature you kill bursts and hurts what was next to it.",
          14, 7, Icon::Corpse, { "sal3" },
          {}, { Trait::CorpseBloom }, {}, cell(7, 1, 0) },
        { "sal5", "MINELAYER", "You leave armed mines in your wake.",
          20, 7, Icon::Blast, { "sal4" },
          { { Stat::MineDamage, 4.0f, 0.0f }, { Stat::MineCount, 1.0f, 0.0f } },
          { Trait::Mines }, {}, cell(7, 1, 1) },
        { "sal6", "DEEP MINES", "+6 mine damage and they come twice as often.",
          28, 7, Icon::Blast, { "sal5" },
          { { Stat::MineDamage, 6.0f, 0.0f }, { Stat::MineCount, 1.0f, 0.0f } },
          {}, {}, cell(7, 1, 2) },

        { "sal7", "PARASITE", "A third of your kills give a point of hull back.",
          14, 7, Icon::Spore, { "sal3" },
          { { Stat::LifestealChance, 0.33f, 0.0f } }, { Trait::Lifesteal }, {}, cell(7, 2, 0) },
        { "sal8", "GLUTTON", "Kills heal you twice as often.",
          20, 7, Icon::Fang, { "sal7" },
          { { Stat::LifestealChance, 0.33f, 0.0f } }, {}, {}, cell(7, 2, 1) },
        { "sal9", "HOARD", "+60% salvage and +4 maximum HP.",
          28, 7, Icon::Crate, { "sal8" },
          { { Stat::SalvageBonus, 0.60f, 0.0f }, { Stat::MaxHealth, 4.0f, 0.0f } },
          {}, {}, cell(7, 2, 2) },

        // ==================== CURRENT ====================
        // What the water itself can be made to do: chilled, bounced, or wound
        // up and let go all at once.
        { "cur1", "CHILL", "Your jets leave whatever they hit slower than it was.",
          3, 8, Icon::Blackout, { "core" },
          { { Stat::SlowStrength, 0.30f, 0.0f } }, { Trait::Frost }, {}, cell(8, 0, 0) },
        { "cur2", "DEEP FREEZE", "+25% chill, and it holds longer.",
          6, 8, Icon::Blackout, { "cur1" },
          { { Stat::SlowStrength, 0.25f, 0.0f } }, {}, {}, cell(8, 0, 1) },
        { "cur3", "BRITTLE", "+70% damage against anything already bleeding.",
          10, 8, Icon::Crit, { "cur2" },
          { { Stat::BleedTargetDamage, 0.0f, 0.70f } }, {}, {}, cell(8, 0, 2) },

        { "cur4", "RICOCHET", "Jets bounce off the basin wall instead of dying on it.",
          14, 8, Icon::Ghost, { "cur3" },
          { { Stat::RicochetCount, 1.0f, 0.0f } }, { Trait::Ricochet }, {}, cell(8, 1, 0) },
        { "cur5", "CAROM", "+2 bounces.",
          20, 8, Icon::Ghost, { "cur4" },
          { { Stat::RicochetCount, 2.0f, 0.0f } }, {}, {}, cell(8, 1, 1) },
        { "cur6", "UNDERTOW", "+35% jet damage and +20% jet speed.",
          28, 8, Icon::Jet, { "cur5" },
          { { Stat::ProjectileDamage, 0.0f, 0.35f }, { Stat::ProjectileSpeed, 0.0f, 0.20f } },
          {}, {}, cell(8, 1, 2) },

        { "cur7", "CHARGE COIL", "Hold fire and the next jet comes out three times as hard.",
          14, 8, Icon::Charge, { "cur3" },
          { { Stat::ChargeMultiplier, 3.0f, 0.0f } }, { Trait::ChargedShot }, {},
          cell(8, 2, 0) },
        { "cur8", "OVERPRESSURE", "A charged jet hits five times as hard and pierces.",
          20, 8, Icon::Charge, { "cur7" },
          { { Stat::ChargeMultiplier, 2.0f, 0.0f }, { Stat::Pierce, 1.0f, 0.0f } },
          {}, {}, cell(8, 2, 1) },
        { "cur9", "THE TRENCH ITSELF", "+40% jet damage, +1 soaked, +10% speed.",
          28, 8, Icon::Abyssal, { "cur8" },
          { { Stat::ProjectileDamage, 0.0f, 0.40f }, { Stat::Armour, 1.0f, 0.0f },
            { Stat::MoveSpeed, 0.0f, 0.10f } }, {}, {}, cell(8, 2, 2) },
    };

    for (UpgradeNode& node : m_nodes)
        node.cost = priceOf(node.cost);
}

void UpgradeTree::reset()
{
    m_unlocked.assign(m_nodes.size(), 0);

    // The root is the sub you already have, so it starts owned. Everything
    // else hangs off it, which gives the tree a single entry point.
    const std::size_t root = indexOf("core");
    if (root < m_unlocked.size())
        m_unlocked[root] = 1;
}

std::size_t UpgradeTree::indexOf(const std::string& id) const
{
    for (std::size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (m_nodes[i].id == id)
            return i;
    }
    return m_nodes.size();
}

bool UpgradeTree::isUnlocked(std::size_t index) const
{
    return index < m_unlocked.size() && m_unlocked[index] != 0;
}

bool UpgradeTree::arePrerequisitesMet(std::size_t index) const
{
    if (index >= m_nodes.size())
        return false;

    for (const std::string& id : m_nodes[index].prerequisites)
    {
        if (!isUnlocked(indexOf(id)))
            return false;
    }
    return true;
}

bool UpgradeTree::canAfford(std::size_t index, int salvage) const
{
    return index < m_nodes.size() && salvage >= m_nodes[index].cost;
}

bool UpgradeTree::canUnlock(std::size_t index, int salvage) const
{
    return index < m_nodes.size()
           && !isUnlocked(index)
           && arePrerequisitesMet(index)
           && canAfford(index, salvage);
}

bool UpgradeTree::tryUnlock(std::size_t index, int& salvage)
{
    if (!canUnlock(index, salvage))
        return false;

    salvage -= m_nodes[index].cost;
    m_unlocked[index] = 1;
    return true;
}

int UpgradeTree::getSpent() const
{
    int spent = 0;

    for (std::size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (isUnlocked(i))
            spent += m_nodes[i].cost;
    }
    return spent;
}

int UpgradeTree::stripForRefund()
{
    // Stripping the hull hands most of the salvage back. The part that is lost
    // is what stops a player from re-plumbing the whole sub between every
    // level, which would make the branch choice meaningless.
    const int refund = static_cast<int>(
        std::lround(static_cast<float>(getSpent()) * config::RespecRefund));

    reset();
    return refund;
}

std::size_t UpgradeTree::getUnlockedCount() const
{
    return static_cast<std::size_t>(std::count(m_unlocked.begin(), m_unlocked.end(), 1));
}

std::vector<std::string> UpgradeTree::getUnlockedIds() const
{
    std::vector<std::string> ids;

    for (std::size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (isUnlocked(i))
            ids.push_back(m_nodes[i].id);
    }
    return ids;
}

void UpgradeTree::setUnlockedIds(const std::vector<std::string>& ids)
{
    reset(); // back to just the root, then re-apply what the save knew about

    for (const std::string& id : ids)
    {
        const std::size_t index = indexOf(id);
        if (index < m_unlocked.size())
            m_unlocked[index] = 1;
    }
}

void UpgradeTree::applyTo(Stats& stats) const
{
    // Rebuilt from scratch every time, so there is no way for a stale bonus to
    // survive a respec or be counted twice.
    stats.clear();

    for (std::size_t i = 0; i < m_nodes.size(); ++i)
    {
        if (!isUnlocked(i))
            continue;

        const UpgradeNode& node = m_nodes[i];

        for (const StatModifier& modifier : node.effects)
            stats.apply(modifier);
        for (const Trait trait : node.traits)
            stats.grant(trait);
        for (const ShipPart part : node.visuals)
            stats.fit(part);
    }
}
