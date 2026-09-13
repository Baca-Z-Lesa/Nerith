#include "world/LevelDef.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr auto AuthoredCount = static_cast<int>(AuthoredLevels.size());
}

int authoredLevelCount()
{
    return AuthoredCount;
}

const char* depthName(int depth)
{
    switch (depth)
    {
        case 1:  return "THE SHALLOWS";
        case 2:  return "THE SHELF";
        case 3:  return "THE TRENCH";
        case 4:  return "THE ABYSS";
        default: return "THE DARK";
    }
}

LevelDef levelFor(int number)
{
    number = std::max(1, number);

    if (number <= AuthoredCount)
        return AuthoredLevels[static_cast<std::size_t>(number - 1)];

    // Beyond the authored campaign, reuse the shape of the last depth and keep
    // turning the screws. Stat creep stays capped: the pressure comes from the
    // count, the pace and the mix, exactly as it does in the authored levels.
    const int beyond = number - AuthoredCount;
    const float steps = static_cast<float>(beyond);

    LevelDef def{};
    def.scaling.health = std::min(3.0f, 1.25f + 0.06f * steps);
    def.scaling.speed = std::min(1.45f, 1.12f + 0.015f * steps);
    def.scaling.bonusDamage = 1 + beyond / 10;

    if (isBossLevel(number))
    {
        def.title = "THE WARDEN";
        def.brief = "IT IS BIGGER EVERY TIME.";
        def.objective = ObjectiveKind::DefeatBoss;
        def.objectiveAmount = 0;
        def.spawnInterval = 0.0f;
        def.maxAlive = 1;
        def.isBoss = true;
        def.scaling.health = std::min(5.0f, 2.1f + 0.35f * steps);
        return def;
    }

    def.title = "THE DARK";
    def.brief = "NOTHING DOWN HERE HAS A NAME.";
    def.spawnInterval = std::max(0.55f, 0.85f - 0.02f * steps);
    def.maxAlive = std::min<std::size_t>(14, 10 + static_cast<std::size_t>(beyond / 4));

    // Alternate the goal so a long run does not become one repeated chore.
    switch (beyond % 3)
    {
        case 0:
            def.objective = ObjectiveKind::SurviveTime;
            def.objectiveAmount = 60 + beyond * 2;
            break;
        case 1:
            def.objective = ObjectiveKind::KillCount;
            def.objectiveAmount = 26 + beyond * 2;
            break;
        default:
            def.objective = ObjectiveKind::ClearRoster;
            def.objectiveAmount = 0;
            break;
    }

    // Past the campaign the deep creatures stop being a guest appearance and
    // become the water. They ramp in one at a time so that a long run keeps
    // introducing something rather than only multiplying what is there, and
    // the shallow roster thins out underneath them.
    const int wave = 4 + beyond / 2;
    const int deep = 1 + beyond / 3;

    if (beyond < 4)
    {
        def.script = { {
            { EnemyType::Lancer, std::min(7, wave) },
            { EnemyType::Warden, std::min(4, deep) },
            { EnemyType::Weaver, std::min(6, 2 + beyond / 3) },
            { EnemyType::Lurker, std::min(9, wave) },
            { EnemyType::Bloom, std::min(5, deep + 1) },
        } };
    }
    else if (beyond < 9)
    {
        def.script = { {
            { EnemyType::Revenant, std::min(5, deep) },
            { EnemyType::Bloom, std::min(6, deep + 2) },
            { EnemyType::Herald, std::min(3, 1 + beyond / 6) },
            { EnemyType::Lancer, std::min(8, wave) },
            { EnemyType::Warden, std::min(4, deep) },
            { EnemyType::Weaver, std::min(5, 1 + beyond / 4) },
        } };
    }
    else
    {
        def.script = { {
            { EnemyType::Lancer, std::min(8, wave) },
            { EnemyType::Bloom, std::min(5, deep - 1) },
            { EnemyType::Herald, std::min(4, 1 + beyond / 6) },
            { EnemyType::Warden, std::min(6, deep) },
            { EnemyType::Revenant, std::min(6, deep) },
            { EnemyType::Weaver, std::min(7, deep + 2) },
        } };
    }

    return def;
}
