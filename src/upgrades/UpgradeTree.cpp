#include "upgrades/UpgradeTree.h"

#include <algorithm>

namespace
{
    // The tree grows outward from the hull in the middle: seven roots, each
    // pushing further into the dark. Node positions are centres, relative to
    // the root, and the screen pans across them because no run will ever see
    // all of it at once anyway.
    constexpr float FirstRing = 168.0f;
    constexpr float RingStep = 132.0f;

    sf::Vector2f cell(int branch, int index)
    {
        const float base = -90.0f + 360.0f * static_cast<float>(branch)
                                        / static_cast<float>(UpgradeBranchCount);

        // Alternating curl so the roots bend rather than radiating like spokes,
        // but linear and small: squaring it swung the far end of a branch a
        // full segment across and straight through its neighbours.
        const float curl = (branch % 2 == 0 ? 1.0f : -1.0f) * 3.6f;
        const float angle = base + curl * static_cast<float>(index);
        const float distance = FirstRing + RingStep * static_cast<float>(index);

        return sf::Vector2f(distance, sf::degrees(angle));
    }

    const char* const BranchNames[UpgradeBranchCount] = {
        "WEAPONS", "TORPEDOES", "HULL", "ENGINE", "SONAR", "DEFENSE", "MUTATION"
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

void UpgradeTree::build()
{
    // Seven branches of eight. Nobody finishes more than a couple of them in a
    // run, so which columns you commit to is the actual build decision. Depth
    // gates keep the deep half of every branch out of reach early on.
    m_nodes = {
        { "core", "THE SUB", "What you start with. Everything hangs off it.",
          0, 1, -1, {}, {}, {}, {}, { 0.0f, 0.0f } },

        // ================= WEAPONS =================
        { "wpn1", "GUN BAY", "+12% jet damage. Opens the weapon line.",
          3, 1, 0, { "core" }, { { Stat::ProjectileDamage, 0.0f, 0.12f } }, {}, {}, cell(0, 0) },
        { "wpn2", "PRESSURE VALVE", "+25% jet damage.",
          6, 1, 0, { "wpn1" }, { { Stat::ProjectileDamage, 0.0f, 0.25f } }, {}, {}, cell(0, 1) },
        { "wpn3", "RAPID PUMP", "+25% fire rate.",
          8, 1, 0, { "wpn2" }, { { Stat::FireRate, 0.0f, 0.25f } }, {}, {}, cell(0, 2) },
        { "wpn4", "NARROW BORE", "+35% jet speed. Easier to lead a darter.",
          10, 2, 0, { "wpn3" }, { { Stat::ProjectileSpeed, 0.0f, 0.35f } }, {}, {}, cell(0, 3) },
        { "wpn5", "LANCE", "Jets punch through one extra creature.",
          14, 2, 0, { "wpn4" }, { { Stat::Pierce, 1.0f, 0.0f } }, {}, {}, cell(0, 4) },
        { "wpn6", "SPLITSHOT", "A jet that hits bursts into three fragments.",
          18, 3, 0, { "wpn5" }, { { Stat::SplitCount, 3.0f, 0.0f } },
          { Trait::SplitShot }, {}, cell(0, 5) },
        { "wpn7", "HEAVY CANNON", "+1 flat damage, -15% fire rate. Fits a bigger gun.",
          24, 3, 0, { "wpn6" },
          { { Stat::ProjectileDamage, 1.0f, 0.0f }, { Stat::FireRate, 0.0f, -0.15f } },
          {}, { ShipPart::HeavyCannon }, cell(0, 6) },
        { "wpn8", "TWIN BORE", "+1 flat damage and +15% fire rate. A second barrel.",
          32, 4, 0, { "wpn7" },
          { { Stat::ProjectileDamage, 1.0f, 0.0f }, { Stat::FireRate, 0.0f, 0.15f } },
          {}, { ShipPart::TwinCannon }, cell(0, 7) },

        // ================= TORPEDOES =================
        { "tor1", "TORPEDO TUBE", "Right mouse fires a slow, heavy torpedo.",
          6, 1, 1, { "core" }, {}, { Trait::Torpedoes }, { ShipPart::TorpedoTube }, cell(1, 0) },
        { "tor2", "WARHEAD", "+50% torpedo damage.",
          9, 2, 1, { "tor1" }, { { Stat::TorpedoDamage, 0.0f, 0.50f } }, {}, {}, cell(1, 1) },
        { "tor3", "FAST LOAD", "+40% torpedo reload speed.",
          12, 2, 1, { "tor2" }, { { Stat::TorpedoRate, 0.0f, 0.40f } }, {}, {}, cell(1, 2) },
        { "tor4", "SEEKER", "Torpedoes steer towards whatever is closest.",
          16, 3, 1, { "tor3" }, {}, { Trait::HomingTorpedoes }, {}, cell(1, 3) },
        { "tor5", "CLUSTER", "A torpedo scatters six jets where it lands.",
          20, 3, 1, { "tor4" }, {}, { Trait::ClusterTorpedoes }, {}, cell(1, 4) },
        { "tor6", "DEPTH CHARGE", "+80% torpedo damage.",
          26, 4, 1, { "tor5" }, { { Stat::TorpedoDamage, 0.0f, 0.80f } }, {}, {}, cell(1, 5) },
        { "tor7", "SALVO", "+60% torpedo reload speed.",
          34, 4, 1, { "tor6" }, { { Stat::TorpedoRate, 0.0f, 0.60f } }, {}, {}, cell(1, 6) },
        { "tor8", "DETONATOR", "+3 flat torpedo damage.",
          42, 5, 1, { "tor7" }, { { Stat::TorpedoDamage, 3.0f, 0.0f } }, {}, {}, cell(1, 7) },

        // ================= HULL =================
        { "hul1", "BALLAST", "+2 maximum HP.",
          3, 1, 2, { "core" }, { { Stat::MaxHealth, 2.0f, 0.0f } }, {}, {}, cell(2, 0) },
        { "hul2", "FRAME", "+3 maximum HP.",
          7, 1, 2, { "hul1" }, { { Stat::MaxHealth, 3.0f, 0.0f } }, {}, {}, cell(2, 1) },
        { "hul3", "ABLATIVE PLATE", "Soak 1 damage per hit. Bolts plating on.",
          11, 2, 2, { "hul2" }, { { Stat::Armour, 1.0f, 0.0f } },
          {}, { ShipPart::ArmourPlates }, cell(2, 2) },
        { "hul4", "PRESSURE HULL", "+4 maximum HP.",
          15, 2, 2, { "hul3" }, { { Stat::MaxHealth, 4.0f, 0.0f } }, {}, {}, cell(2, 3) },
        { "hul5", "DAMAGE CONTROL", "+0.6s of mercy after a hit.",
          19, 3, 2, { "hul4" }, { { Stat::InvulnerabilityTime, 0.6f, 0.0f } }, {}, {}, cell(2, 4) },
        { "hul6", "SECOND SKIN", "Soak another 1 damage per hit.",
          26, 3, 2, { "hul5" }, { { Stat::Armour, 1.0f, 0.0f } }, {}, {}, cell(2, 5) },
        { "hul7", "EMERGENCY VENT", "Survive one otherwise fatal hit each level.",
          34, 4, 2, { "hul6" }, {}, { Trait::EmergencyVent }, {}, cell(2, 6) },
        { "hul8", "DEEP PLATE", "+6 maximum HP and 1 more soaked per hit.",
          46, 5, 2, { "hul7" },
          { { Stat::MaxHealth, 6.0f, 0.0f }, { Stat::Armour, 1.0f, 0.0f } }, {}, {}, cell(2, 7) },

        // ================= ENGINE =================
        { "eng1", "TRIM", "+10% move speed.",
          3, 1, 3, { "core" }, { { Stat::MoveSpeed, 0.0f, 0.10f } }, {}, {}, cell(3, 0) },
        { "eng2", "TWIN SCREW", "+18% move speed.",
          7, 1, 3, { "eng1" }, { { Stat::MoveSpeed, 0.0f, 0.18f } }, {}, {}, cell(3, 1) },
        { "eng3", "THRUST VECTOR", "Shift dashes. Fits a heavier engine block.",
          12, 2, 3, { "eng2" }, {}, { Trait::Dash }, { ShipPart::BigEngine }, cell(3, 2) },
        { "eng4", "SUPERCHARGER", "+18% move speed.",
          16, 2, 3, { "eng3" }, { { Stat::MoveSpeed, 0.0f, 0.18f } }, {}, {}, cell(3, 3) },
        { "eng5", "QUICK VENT", "-35% dash cooldown.",
          20, 3, 3, { "eng4" }, { { Stat::DashCooldown, 0.0f, -0.35f } }, {}, {}, cell(3, 4) },
        { "eng6", "PRESSURE BURST", "Dashing sends out a shockwave that hurts.",
          26, 3, 3, { "eng5" }, {}, { Trait::PressureBurst }, {}, cell(3, 5) },
        { "eng7", "SLIPSTREAM", "+22% move speed.",
          32, 4, 3, { "eng6" }, { { Stat::MoveSpeed, 0.0f, 0.22f } }, {}, {}, cell(3, 6) },
        { "eng8", "CAVITATION", "You cannot be touched while dashing.",
          40, 4, 3, { "eng7" }, {}, { Trait::DashInvuln }, {}, cell(3, 7) },

        // ================= SONAR =================
        { "son1", "HYDROPHONE", "Pings the dark and shows what is out there.",
          6, 2, 4, { "core" }, {}, { Trait::Sonar }, { ShipPart::SonarDish }, cell(4, 0) },
        { "son2", "WIDE ARRAY", "+60% sonar range.",
          10, 2, 4, { "son1" }, { { Stat::SonarRange, 0.0f, 0.60f } }, {}, {}, cell(4, 1) },
        { "son3", "WEAK POINT SCAN", "+30% damage to boss parts and organs.",
          15, 3, 4, { "son2" }, { { Stat::WeakPointDamage, 0.0f, 0.30f } }, {}, {}, cell(4, 2) },
        { "son4", "SALVAGE SCAN", "+30% salvage from everything you kill.",
          20, 3, 4, { "son3" }, { { Stat::SalvageBonus, 0.0f, 0.30f } }, {}, {}, cell(4, 3) },
        { "son5", "DEEP PING", "+70% sonar range.",
          26, 4, 4, { "son4" }, { { Stat::SonarRange, 0.0f, 0.70f } }, {}, {}, cell(4, 4) },
        { "son6", "BLOOD SCENT", "+35% damage to anything already bleeding.",
          32, 4, 4, { "son5" }, { { Stat::BleedTargetDamage, 0.0f, 0.35f } },
          {}, {}, cell(4, 5) },
        { "son7", "RESONANCE", "+40% damage to boss parts and organs.",
          40, 5, 4, { "son6" }, { { Stat::WeakPointDamage, 0.0f, 0.40f } }, {}, {}, cell(4, 6) },
        { "son8", "APEX PREDATOR", "+20% jet damage and +20% salvage.",
          50, 5, 4, { "son7" },
          { { Stat::ProjectileDamage, 0.0f, 0.20f }, { Stat::SalvageBonus, 0.0f, 0.20f } },
          {}, {}, cell(4, 7) },

        // ================= DEFENSE =================
        { "def1", "COUNTERMEASURE", "+0.4s of mercy after a hit.",
          5, 2, 5, { "core" }, { { Stat::InvulnerabilityTime, 0.4f, 0.0f } }, {}, {}, cell(5, 0) },
        { "def2", "STATIC FIELD", "A shield that eats one hit and regrows.",
          12, 2, 5, { "def1" }, { { Stat::ShieldCharges, 1.0f, 0.0f } },
          { Trait::Shield }, { ShipPart::ShieldBubble }, cell(5, 1) },
        { "def3", "CAPACITOR", "+1 shield charge.",
          18, 3, 5, { "def2" }, { { Stat::ShieldCharges, 1.0f, 0.0f } }, {}, {}, cell(5, 2) },
        { "def4", "FAST CYCLE", "-40% shield regrow time.",
          24, 3, 5, { "def3" }, { { Stat::ShieldRecharge, 0.0f, -0.40f } }, {}, {}, cell(5, 3) },
        { "def5", "REACTIVE PLATE", "A breaking shield blasts whatever is close.",
          30, 4, 5, { "def4" }, {}, { Trait::ReactiveArmour }, {}, cell(5, 4) },
        { "def6", "MINE LAYER", "Drops proximity mines in your wake.",
          36, 4, 5, { "def5" }, {}, { Trait::Mines }, {}, cell(5, 5) },
        { "def7", "DENSE CHARGES", "+60% mine damage.",
          44, 5, 5, { "def6" }, { { Stat::MineDamage, 0.0f, 0.60f } }, {}, {}, cell(5, 6) },
        { "def8", "DEEP SHELL", "+2 shield charges.",
          56, 5, 5, { "def7" }, { { Stat::ShieldCharges, 2.0f, 0.0f } }, {}, {}, cell(5, 7) },

        // ================= MUTATION =================
        { "mut1", "SYMBIOSIS", "+1 maximum HP and +6% move speed.",
          8, 2, 6, { "core" },
          { { Stat::MaxHealth, 1.0f, 0.0f }, { Stat::MoveSpeed, 0.0f, 0.06f } }, {}, {}, cell(6, 0) },
        { "mut2", "PARASITE", "Hits leave something behind that keeps eating.",
          16, 3, 6, { "mut1" }, { { Stat::BleedDamage, 1.0f, 0.0f } },
          { Trait::Bleed }, {}, cell(6, 1) },
        { "mut3", "CHAIN REACTION", "Breaking a boss part wounds the parts beside it.",
          22, 3, 6, { "mut2" }, {}, { Trait::ChainReaction }, {}, cell(6, 2) },
        { "mut4", "RUPTURE", "+150% bleed damage.",
          28, 4, 6, { "mut3" }, { { Stat::BleedDamage, 0.0f, 1.50f } }, {}, {}, cell(6, 3) },
        { "mut5", "ESCORT", "A drone circles you and fires on its own.",
          34, 4, 6, { "mut4" }, { { Stat::DroneCount, 1.0f, 0.0f } },
          { Trait::Drones }, { ShipPart::Drone }, cell(6, 4) },
        { "mut6", "HIVE", "+1 escort drone.",
          44, 5, 6, { "mut5" }, { { Stat::DroneCount, 1.0f, 0.0f } }, {}, {}, cell(6, 5) },
        { "mut7", "PREDATOR EYE", "Below half health you hit far harder.",
          52, 5, 6, { "mut6" }, {}, { Trait::PredatorInstinct }, {}, cell(6, 6) },
        { "mut8", "ABYSSAL FORM", "+15% damage and 2 soaked, but -10% speed.",
          64, 5, 6, { "mut7" },
          { { Stat::ProjectileDamage, 0.0f, 0.15f }, { Stat::Armour, 2.0f, 0.0f },
            { Stat::MoveSpeed, 0.0f, -0.10f } }, {}, {}, cell(6, 7) },
    };
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

bool UpgradeTree::isDepthMet(std::size_t index, int depth) const
{
    return index < m_nodes.size() && depth >= m_nodes[index].requiredDepth;
}

bool UpgradeTree::canAfford(std::size_t index, int salvage) const
{
    return index < m_nodes.size() && salvage >= m_nodes[index].cost;
}

bool UpgradeTree::canUnlock(std::size_t index, int salvage, int depth) const
{
    return index < m_nodes.size()
           && !isUnlocked(index)
           && arePrerequisitesMet(index)
           && isDepthMet(index, depth)
           && canAfford(index, salvage);
}

bool UpgradeTree::tryUnlock(std::size_t index, int& salvage, int depth)
{
    if (!canUnlock(index, salvage, depth))
        return false;

    salvage -= m_nodes[index].cost;
    m_unlocked[index] = 1;
    return true;
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
    // survive a reset or get counted twice.
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
