# Nerith

A 2D top-down underwater horror shooter. You drive a submarine down a trench in
twenty hand-authored levels, four depths, a creature waiting at the bottom of
each one, and a trench that keeps generating more of itself after that. The
upgrade tree is permanent and is never taken away; what a dive earns is not
yours until you surface with it.

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
| `Shift` | dash, once the engine takes it — and the way out of a mouth |
| `E` | cycle the fitted guns |
| `F` | kill the lamp and run dark |
| `Tab` | the upgrade tree, mid dive |
| arrows | walk the roots, in the tree |
| `Space` | install what the roots landed on |
| `,` `.` | pick the dive, in the hub |
| wheel, `+` `-` | zoom the tree |
| `R` | strip the hull back, in the hub |
| `Esc` or `P` | hold at depth — and `Q` from there surfaces with the hold |
| `M` | mute |
| `F11` | fullscreen |

Every key on that list can be rebound. Options also carries difficulty, how hard
the camera follows, the two volumes, fullscreen, vsync, a frame cap, brightness,
and separate sliders for screen shake and flashes - how much of each, not
whether, because somebody who cannot take a shaking frame should not have to
give up the hit feedback to stop it.

## How it is put together

Salvage is the only thing between you and any upgrade — there are no timed or
depth-gated nodes. Nine roots leave the hull, each running out three nodes
before forking into two limbs of three, so committing to a root still leaves a
choice inside it. Prices climb steeply and a clean first run through the
campaign pays for roughly half the tree. Clearing water nobody has cleared pays
a bonus that grows with the level; going back over old ground pays a third of
the kills and no bonus, so the way to afford anything is downward.

No two dives are in the same water. The basin is generated per level from a
seed and it is a maze rather than a chain: sixteen rooms walked outward from
the mouth, each cut from two to four lobes so no room is a circle, passages
that bend and vary in width between them, dead ends hanging off them, and
several loops tied back across the whole thing. One room in five is far larger
than the rest, so the map has a rhythm instead of only a length.

Nothing in it is a straight line. Every rim is four harmonics with weights
rolled per room, leaning off its own centre; passages are cut as runs of
overlapping wobbled mouthfuls rather than as capsules; and lumps of rock are
bitten out of the walls, biting into the passages too but never far enough to
seal one. It all resolves into one signed distance field, so rock is solid,
corners are real cover and a creature on the other side of a wall has to come
around it.

The lamp is a beam rather than a bubble, thrown along the aim with a short halo
close in. That is what makes the darkness cost something: looking where you are
going and looking where you are shooting are two different decisions. A fifth of
passages run one way, so a route you took easily is not the route back, and the
map in the corner starts dark and fills in as you swim or ping - it is a record
of where you have been, not a survey handed to you at the mouth.

The water has an acoustic shape too. Sounds play where they happen, a real
low-pass takes the top off anything with rock in the way, and an open cavern
hands a sound back to you a fraction of a second later. Nothing in the HUD tells
you where a creature is: the frame darkens on the side one is closing from and a
call tightens as it nears, and that is all.

Coming around it is a graph and a grid. The rooms and their passages are a
graph, walked once at generation time so every pair of rooms knows its next
hop; underneath it a grid of open cells gets a breadth-first sweep out from the
sub a few times a second, so anything that has lost its line reads its own cell
and swims the way that leads somewhere. Without the grid the graph is right
until both ends are in the same room with a pillar between them, and that case
used to leave one creature alive and unreachable for the rest of a level.

A basin cut that finely is a thousand shapes, and three things keep that from
being what the game spends its time on. The shapes are filed into a coarse grid
once, so asking where the rock is reads a handful of them rather than walking
all thousand — and files each one with a margin, so the bucket's answer is
exact rather than close, with a full sweep as the fallback for the rare query
too deep in rock for the margin to cover. A rim is evaluated as powers of the
direction instead of an angle, which is the same curve to a thousandth of a
pixel with no trigonometry in it at all. And the basin, the seabed and the
minimap are all cut once and parked on the graphics card, because a vertex
array lives in system memory and is handed to the driver again on every frame
it appears — half a million vertices of rock that has not moved since the level
was built. Between them, building a dive went from about eight hundred
milliseconds to about a hundred, and the drawing a frame does on the processor
from thirteen milliseconds to under three.

The dressing gets the same treatment from the other end: the seabed scatters
nearly three thousand props over a field far wider than the window, and each is
a sprite and a shadow, so only the ones near the view are drawn at all.

Boss levels open into one hall instead.

Nine kinds of creature, and one that no level puts there. There used to be
seventeen, and half of them were the same animal with different numbers - three
ways for something to appear beside you, two burst-chargers on the same state
machine, two things that held a range and threw a fan. A list that long reads as
variety on the logbook page and as one animal in the water. What was worth
keeping out of the ones that went was folded into whichever survivor was already
nearest. The rule for adding a tenth is that it has to need an answer none of
these need.

The tenth is the Stalker, and nothing spawns it. Leave three creatures alive
below the shallows and one of them follows you down, and keeps following, and
gets worse every dive it survives.

There are four kinds of dive. Most are fought: hold what ground you can
and kill what arrives. A hunt is not — the nests are cemented to rock out at
the reef, never all in sight at once, hatching until they are burned out, so you
have to cross the basin and crossing it costs. And any of them can be run dark:
`F` kills the lamp, which leaves you nearly blind but leaves everything hunting
you blind too. A creature that has lost the sub wanders and holds its fire
entirely. Firing gives you away for a couple of seconds either way, so going
quiet is a way to move rather than a way to shoot for free.

A haul is the odd one out: the salvage pods are dropped in the far rooms
and the mouth is where the dive started, so the whole level is spent crossing
water you have already crossed — slowly, lit up, with your hands full. A pod
costs you speed and can be heard half again as far, only one can be towed at a
time, and anything that lands a hit makes you let go of it exactly where you
are standing. Every other level type is answered by fighting well. This one is
answered by deciding when to fight at all.

Nothing a dive earns is banked until it surfaces. Clearing a level banks it,
walking out of one on purpose banks it, and drowning loses every bit of it —
the banner says how much. The tree survives either way, so the pressure lands
on the decision to push on rather than on the progress behind it.

A beating takes the sub apart rather than only the bar. Crossing each of four
thresholds knocks out the next system for the rest of the dive, least crippling
first: the sonar, then the torpedo tube, then the gun, which lands soft and
cycles slowly, and last the engine, which costs a third of the speed and the
dash. The HUD names them, because there is a difference between knowing you are
on low health and knowing you have no torpedoes and no dash.

The end of each weapon limb is a gun rather than a stat. The harpoon is a heavy
head on a line that bites into whatever it hits — creature or rock — and hauls
the sub to it. The pulse has no projectile at all: it empties the water around
the hull, kills the orbs in it, and leaves everything it touched cold. `E`
cycles whatever is fitted.

A hit that never lands is worth as much noise as one that does. A shield
popping, a dash timed into a shot, armour soaking a blow, the vent pulling you
out of a death: each has its own colour and ring, its own weight of hit stop,
and a dodge drops the world into slow motion for a moment. Taking real damage
resets the streak, so the counter on the HUD is a live claim rather than a
score, and the kill sound climbs in pitch while it holds. It is worth
something, too: every kill on the run pays five per cent more salvage, up to
double, and the first hit that lands takes all of it back at once.

Anything that can really hurt you announces itself first. A ring closes on the
creature and arrives tight and bright exactly as the blow does, a charge draws
the line it is about to take, and if it is winding up off the edge of the frame
the threat arrow takes the same colour and swells with it. Blocking is meant to
be a read rather than a guess.

The music is three loops of the same length over the same root, played together
and faded independently. The bed is always on; the pulse follows how many
creatures have noticed you, the dread follows whether a boss is alive. Nothing
in it is a tune, so it can sit under a fight for as long as the fight lasts.

Bosses are animals rather than health bars. Each is a body towing segments with
eyes, fins, tentacles, plating and organs bolted to it, and breaking a part takes
something away: its aim, its speed, one of the arms feeding its attack, or the
shell over the thing keeping it alive. Every one has three acts, and crossing
into one is permanent: nothing is replaced, it is added.

Each also has two moves that are not more bullets, alternating so neither
becomes the one you wait for. The leviathan sweeps its tail in an arc that
throws you, and grabs — a lunge announced as a charge that, if it connects,
holds on until you dash out of it. The sovereign plants pods that hatch if they
are left, and pushes the whole room away from itself. The worm blinds you with
silt and comes up through the floor exactly where you are standing, the floor
bulging first. The hollow pulls, and tears pieces out of the basin to throw.
Aggression compounds with every act, so a cornered creature is relentless
rather than merely faster.

Below the campaign the water changes. Six creatures live down there that are
rules rather than bigger numbers: one armoured only where it is looking, one
that latches onto the hull and drains until it is shaken off, one that is
untouchable under the floor, one that holds a light up for the rest so that
running dark stops working, one that arrives instead of swimming, and one whose
body is the weapon and whose head is only how it steers. The logbook on the
title screen is where they hold still long enough to be looked at.

The fight runs on a fixed step of a hundred and twenty a second and the frame
just draws what is between the last two of them. That is not only tidiness: with
a variable step the orbit shield ate one orb per FRAME, so its defence was worth
two and a half times more at 144 than at 60; every cooldown threw away the part
of a frame it had already run past, so the fire rate the player buys was
quantised to whatever the machine was doing; and a creature that lost you
wandered further on a slow machine than on a fast one. Input is sampled once a
frame, as late as the loop can manage, and the pacer waits at the TOP of the
loop rather than after the swap - so the spare time is spent before the keyboard
is read instead of after the picture is already on the screen.

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
                and the logbook text
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

The grain is seeded from each sprite's own name rather than from a running
counter, so adding one texture changes one file instead of reshuffling the noise
on every sprite that sorts after it.

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
