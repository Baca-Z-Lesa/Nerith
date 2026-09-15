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
        case 5:  return "THE VENTS";
        case 6:  return "THE CRADLE";
        default: return "THE DARK";
    }
}

LevelDef levelFor(int number)
{
    number = std::max(1, number);

    if (number <= AuthoredCount)
        return AuthoredLevels[static_cast<std::size_t>(number - 1)];

    // There is nothing past the campaign. The trench used to build its own
    // levels forever, which meant the hardest hand-authored level was followed
    // by a generated one holding six fewer creatures - the game got EASIER at
    // level thirty one - and a generated lift quota eventually asked for more
    // salvage than a basin holds, so somewhere around level a hundred it
    // became unfinishable. Neither was ever going to be found by playing.
    //
    // Thirty levels, six depths, and the thing at the bottom is the end.
    return AuthoredLevels[static_cast<std::size_t>(AuthoredCount - 1)];
}
