# Nerith

A 2D top-down underwater horror shooter. You drive a submarine down a trench in
twenty hand-authored levels, four depths, a creature waiting at the bottom of
each one. Dying costs the attempt and nothing else — the salvage and the upgrade
tree are permanent, and the difficulty lives in how hard a level is to beat in
the first place.

Written in C++20 against SFML 3 with no game engine. Every texture and every
sound in the game is generated from source by the scripts in `tools/`; no asset
was downloaded.

## Playing it

Grab `dist/Nerith.zip` (or build it, below), unzip it anywhere, run
`Nerith.exe`. Progress is written to `nerith_save.txt` next to the executable.

| | |
|---|---|
| `W A S D` | drive |
| mouse | aim — the hull turns to follow it |
| left mouse | water jet, held down (held *and released*, with a charge coil) |
| right mouse | torpedo, once the tube is fitted |
| `Shift` | dash, once the engine takes it |
| `F` | kill the lamp and run dark |
| `Tab` | the upgrade tree, mid dive |
| arrows | walk the roots, in the tree |
| `Space` | install what the roots landed on |
| `,` `.` | pick the dive, in the hub |
| wheel, `+` `-` | zoom the tree |
| `R` | strip the hull back, in the hub |
| `Esc` or `P` | hold at depth |
| `M` | mute |
| `F11` | fullscreen |

The title screen has the same list, plus options for difficulty and volume.

## How it is put together

Salvage is the only thing between you and any upgrade — there are no timed or
depth-gated nodes. Nine roots leave the hull, each running out three nodes
before forking into two limbs of three, so committing to a root still leaves a
choice inside it. Prices climb steeply and a clean first run through the
campaign pays for roughly half the tree. Clearing water nobody has cleared pays
a bonus that grows with the level; going back over old ground pays a third of
the kills and no bonus, so the way to afford anything is downward.

No two dives are in the same water. The basin is generated per level from a
seed: nine chambers walked outward from the mouth with corridors cut between
them and side branches off roughly half, all resolved into one signed distance
field, so rock is solid, corners are real cover and a creature on the other side
of a wall has to come around it. Boss levels open into one hall instead. A
minimap in the corner is the only reason any of it is navigable, and arrows at
the screen edge point at what is hunting you off it.

There are three ways to play a level. Most are fought: hold what ground you can
and kill what arrives. A hunt is not — the nests are cemented to rock out at
the reef, never all in sight at once, hatching until they are burned out, so you
have to cross the basin and crossing it costs. And any of them can be run dark:
`F` kills the lamp, which leaves you nearly blind but leaves everything hunting
you blind too. A creature that has lost the sub wanders and holds its fire
entirely. Firing gives you away for a couple of seconds either way, so going
quiet is a way to move rather than a way to shoot for free.

A hit that never lands is worth as much noise as one that does. A shield
popping, a dash timed into a shot, armour soaking a blow, the vent pulling you
out of a death: each has its own colour and ring, its own weight of hit stop,
and a dodge drops the world into slow motion for a moment. Taking real damage
resets the streak, so the counter on the HUD is a live claim rather than a
score, and the kill sound climbs in pitch while it holds.

The music is three loops of the same length over the same root, played together
and faded independently. The bed is always on; the pulse follows how many
creatures have noticed you, the dread follows whether a boss is alive. Nothing
in it is a tune, so it can sit under a fight for as long as the fight lasts.

Bosses are animals rather than health bars. Each is a body towing segments with
eyes, fins, tentacles, plating and organs bolted to it, and breaking a part takes
something away: its aim, its speed, one of the arms feeding its attack, or the
shell over the thing keeping it alive. Every one has three acts, and crossing
into one is permanent: nothing is replaced, it is added. The last creature in
the campaign spends its second act pulling its own dead off its column and its
third closing the basin with shots that come inward off the rim.

## Building it

Needs Visual Studio 2022 with the C++ desktop workload, and
[SFML 3.0.2](https://www.sfml-dev.org/download/sfml/3.0.2/) for MSVC x64
unpacked at `C:\SFML-3.0.2`. Point `SfmlDir` in `Nerith.vcxproj` (or the
`SFML_DIR` environment variable, for the scripts) somewhere else if you keep it
elsewhere.

```
MSBuild Nerith.vcxproj /p:Configuration=Release /p:Platform=x64
```

Or open `Nerith.sln` and press F5. The build copies the SFML DLLs and the assets
next to the executable, so a fresh checkout runs without any manual staging.

Only the x64 configurations exist. There is no 32-bit build and nothing depends
on one.

## Layout

```
src/core        the window, the main loop, the state machine, the save file
                and Config.h, which is every balance number in the game
src/entities    the sub, the creatures, the bosses, the projectiles
src/world       the arena, the seabed, the depth themes, the level table
src/upgrades    the stat block and the upgrade tree
src/ui          the HUD, the menus, the upgrade screen, the pixel font
src/effects     the particle system
assets/         generated textures and audio
tools/          the scripts that generate them
tests/          the test suite
```

`src/core/Config.h` is the balance hub. Enemy stats, level pacing, the economy,
the difficulty table, boss timings and every colour are tables in there, so
changing how the game plays usually means changing a number rather than code.

There is no font asset: `src/ui/PixelFont.cpp` holds a 5x7 bitmap font baked into
the binary and appends text as quads into one vertex array.

## Generating the art and audio

```
powershell -ExecutionPolicy Bypass -File tools\make_3d.ps1
powershell -ExecutionPolicy Bypass -File tools\make_audio.ps1
```

`make_3d.ps1` runs each drawing script and then lights every sprite through one
relief pass, which reads a sprite's own alpha as a height map and shades it from
an upper-left key light. Run it rather than the individual drawing scripts: the
relief pass is not idempotent, and going through there draws flat and lights
once. `tools/relief.ps1` holds the pass and the material table.

Everything alive is drawn through `tools/organic.ps1`, which builds a body from
a centre line and a width profile so it tapers instead of ending, grows fins out
of flanks with rays through them, bends limbs at joints, and countershades — the
one rule every animal down here follows. Two PowerShell 5.1 traps are documented
in that file's header and are worth reading before editing any of the drawing
scripts: variable names are case insensitive (`$w` destroys `$W`), and there is
no ternary operator.

`make_audio.ps1` synthesises every WAV from oscillators and filtered noise.

## Tests

```
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

Covers the table-driven parts: the stat block's arithmetic, every authored and
generated level being finishable, every upgrade being reachable from the root
with no dangling prerequisites, and the economy still being in the range the
pricing curve was tuned for. No framework — it compiles four translation units
and returns non-zero if anything failed.

## Packaging a release

```
powershell -ExecutionPolicy Bypass -File tools\package.ps1
```

Builds Release and writes `dist/Nerith.zip` with the executable, the SFML
runtime DLLs and the assets.
