#pragma once

// One shared generator for the whole game, so we can seed it deterministically
// later if we ever want a repeatable run.
namespace rng
{
    float range(float min, float max);
}
