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
| left mouse | water jet, held down |
| right mouse | torpedo, once the tube is fitted |
| `Shift` | dash, once the engine takes it |
| `Tab` | the upgrade tree, mid dive |
| wheel, `+` `-` | zoom the tree |
| `R` | strip the hull back, in the hub |
| `Esc` or `P` | hold at depth |
| `M` | mute |
| `F11` | fullscreen |

The title screen has the same list, plus options for difficulty and volume.

## How it is put together

Salvage is the only thing between you and any upgrade — there are no timed or
depth-gated nodes. Prices climb steeply along each of the seven branches, and a
clean first run through the campaign pays for roughly half the tree, so which
branches you commit to is the build. Clearing water nobody has cleared pays a
bonus that grows with the level; going back over old ground pays a third of the
kills and no bonus, so the way to afford anything is downward.

Bosses are animals rather than health bars. Each is a body towing segments with
eyes, fins, tentacles, plating and organs bolted to it, and breaking a part takes
something away: its aim, its speed, one of the arms feeding its attack, or the
shell over the thing keeping it alive. Below a third health every one of them
changes its mind — more in every volley, sooner, and wider.

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
