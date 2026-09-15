Add-Type -AssemblyName System.Drawing

# The shared vocabulary. This file used to redefine its own shallow Disc/Ring/
# Stroke and never touched organic.ps1, which is the whole reason the deck
# measured 1.06 marks per thousand pixels while a lurker measures 19: there was
# no Panel, no Rivets and no Truss to reach for, so every built thing came out
# as one flat Box.
#
# Trap: the Blob defined below takes an edge colour and an edge width, and
# organic.ps1's Blob does not. The last definition wins in PowerShell, so the
# local one shadows the shared one for the rest of this script. That is
# deliberate - almost every shape on the deck wants an ink edge - but it means
# Body, Fin and Mottle from organic.ps1 MUST NOT be called from this file:
# they call Blob with three arguments and would hand a tension value to the
# Pen constructor. Countershading here is done with an explicit clip instead.
. "$PSScriptRoot\organic.ps1"

# The surface. Everything you see between dives: the plate you walk on, the
# man walking on it, and the three things on the deck worth walking to.
#
# It is drawn top down like the rest of the game, but it is the only place in
# Nerith with a floor rather than a seabed, so it is all straight edges, rivets
# and rust. The water round it is drawn live by the deck itself - it moves, and
# a moving sea under a still rig is most of what says "surface" without a
# caption having to.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

# Every structural width and radius below is written as a multiple of $SS so
# the number says how many output pixels the mark will actually have. Anything
# opaque and structural stays at or above 2 output pixels; below that a stroke
# either aliases into a dashed line or disappears entirely on downsample. Marks
# thinner than that are allowed only with alpha under about 110, where they
# dissolve into grain instead of breaking up.

function Box($g, $col, $x, $y, $span, $tall) {
    $g.FillRectangle((Brush $col), (RectF $x $y $span $tall))
}

# A rectangle's outline. Panel gives a plate its lit and shaded edges, but a
# thing sitting on the deck also needs a hard dark line round it or it reads as
# a stain rather than as an object standing on the floor.
function Edge($g, $col, $x, $y, $span, $tall, $width) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, ([float]$width)
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
    $g.DrawRectangle($pen, [float]$x, [float]$y, [float]$span, [float]$tall)
    $pen.Dispose()
}

# Same colour, different alpha. Written out longhand everywhere it was needed
# the last time and it made every texture line twice as long as the shape it
# was drawing.
function Fade($col, $alpha) { C $col.R $col.G $col.B $alpha }

function Finish($bmp, $g, $outW, $outH, $name) {
    $g.Dispose()
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, ($outW * 2), ($outH * 2)))) {
        $dir = $target[0]
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
        $dst = New-Object System.Drawing.Bitmap -ArgumentList $target[1], $target[2], ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $gd = [System.Drawing.Graphics]::FromImage($dst)
        $gd.InterpolationMode    = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $gd.PixelOffsetMode      = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $gd.CompositingQuality   = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $gd.SmoothingMode        = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $gd.DrawImage($bmp, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $target[1], $target[2]))
        $gd.Dispose()
        $dst.Save("$dir\$name", [System.Drawing.Imaging.ImageFormat]::Png)
        $dst.Dispose()
    }
    $bmp.Dispose()
    "  $name  ($outW x $outH)"
}

function Curve($pts, $tension = 0.5) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, $tension)
    return $path
}

function Blob($g, $pts, $fill, $edge, $width, $tension = 0.5) {
    $path = Curve $pts $tension
    if ($null -ne $fill) { $g.FillPath((Brush $fill), $path) }
    if ($null -ne $edge) {
        $pen = New-Object System.Drawing.Pen -ArgumentList $edge, $width
        $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($pen, $path)
        $pen.Dispose()
    }
    $path.Dispose()
}

"generating the deck..."

# The palette. The old one topped out at luma 120 while the sub tops out at
# 171, so after the relief pass multiplied it by ambient and occlusion nothing
# on the deck got past about 96 and the crane came out as a dark grey tile with
# a hole in it. The old greys were also nearly neutral - about eight points of
# blue over red - where the sub sits at twenty six. A rig built out of neutral
# grey next to a sub built out of blue-green steel reads as a different game.
#
# $steelTop is the value this file never had: it is what a wet horizontal
# surface returns straight back at the sky, and it is the only thing that makes
# a bevel legible once the relief pass has taken its cut.
$steelTop = C 172 184 194
$steelHi  = C 132 146 156
$steel    = C  96 106 116
$steelLo  = C  58  66  76
$steelDk  = C  30  36  44
$rust     = C 132  86  52
$rustLo   = C  84  48  26
$rustHi   = C 176 124  78
$grime    = C  38  42  50
$rubber   = C  40  40  46
$glass    = C 138 208 216
$lampWarm = C 250 216 150
$paintY   = C 190 162  58
$ink      = C  14  16  20

# =====================================================================
#  PLATE  -  the floor. Has to meet itself on all four sides, so every
#  mark goes down nine times: once where it belongs, and once for each
#  way the plate can wrap. Folding a finished bitmap onto itself paints
#  each copy over the last and gives mush.
# =====================================================================
$tile = 96
$W = $tile * $SS; $H = $tile * $SS
$res = NewCanvas $W $H; $bmp = $res[0]; $g = $res[1]
Box $g $steel 0 0 $W $H

$rand = New-Object System.Random -ArgumentList 4471
$offsets = @(@(-$W, -$H), @(-$W, 0), @(-$W, $H), @(0, -$H), @(0, 0), @(0, $H), @($W, -$H), @($W, 0), @($W, $H))

# Four plates to the tile, each with its own lit and shaded edges. This is what
# gives the floor a grid at any zoom: the diamond tread underneath it is only
# eight screen pixels across and turns into noise the moment the camera pulls
# back, but a panel seam a metre long survives.
$halfW = $W / 2.0
foreach ($px in @(0.0, $halfW)) {
    foreach ($py in @(0.0, $halfW)) {
        Panel $g $px $py $halfW $halfW $steel $steelHi $steelDk (3 * $SS)
    }
}

# Treadplate. Three strokes to a diamond, in the order shadow, body, crest -
# one stroke is a scratch, three is a raised bar. Six by six put eight screen
# pixels between diamonds, which is noise; three by three is a tread you can
# actually see your boot landing on.
for ($row = 0; $row -lt 3; $row++) {
    for ($col = 0; $col -lt 3; $col++) {
        $cx = ($col + 0.5) * $W / 3.0
        $cy = ($row + 0.5) * $H / 3.0
        if ((($row + $col) % 2) -eq 0) { $dx = 30 } else { $dx = -30 }
        foreach ($o in $offsets) {
            Stroke $g $steelDk ($cx - $dx + $o[0]) ($cy - 13 + $o[1]) ($cx + $dx + $o[0]) ($cy + 27 + $o[1]) (3 * $SS)
            Stroke $g $steelHi ($cx - $dx + $o[0]) ($cy - 20 + $o[1]) ($cx + $dx + $o[0]) ($cy + 20 + $o[1]) (2.5 * $SS)
            Stroke $g (Fade $steelTop 150) ($cx - $dx + $o[0]) ($cy - 23 + $o[1]) ($cx + $dx + $o[0]) ($cy + 17 + $o[1]) (1 * $SS)
        }
    }
}

# The seams between the plates, lit as a groove rather than as a proud weld.
# That choice is forced: Panel has already put a shaded edge on the left plate
# and a lit edge on the right plate of every joint, which is exactly how a
# recess reads under a light coming from up and to the left. Bead the weld the
# other way and the two cancel to a smear. So: shadow on the near wall, grime
# in the bottom, and the far wall catching the light.
foreach ($s in @(0.0, 0.5)) {
    $sx = $W * $s
    $sy = $H * $s
    foreach ($o in $offsets) {
        Stroke $g $steelDk ($sx - 2 * $SS + $o[0]) (-10 + $o[1]) ($sx - 2 * $SS + $o[0]) ($H + 10 + $o[1]) (2.5 * $SS)
        Stroke $g $grime ($sx + $o[0]) (-10 + $o[1]) ($sx + $o[0]) ($H + 10 + $o[1]) (1.5 * $SS)
        Stroke $g (Fade $steelTop 150) ($sx + 2 * $SS + $o[0]) (-10 + $o[1]) ($sx + 2 * $SS + $o[0]) ($H + 10 + $o[1]) (1 * $SS)

        Stroke $g $steelDk (-10 + $o[0]) ($sy - 2 * $SS + $o[1]) ($W + 10 + $o[0]) ($sy - 2 * $SS + $o[1]) (2.5 * $SS)
        Stroke $g $grime (-10 + $o[0]) ($sy + $o[1]) ($W + 10 + $o[0]) ($sy + $o[1]) (1.5 * $SS)
        Stroke $g (Fade $steelTop 150) (-10 + $o[0]) ($sy + 2 * $SS + $o[1]) ($W + 10 + $o[0]) ($sy + 2 * $SS + $o[1]) (1 * $SS)
    }
}

# Rivets on the seams, because that is where a rivet goes. The old block put
# thirty of them at random and gave each one a dark core offset one pixel UP,
# which lights it from below: every rivet on the floor read as a bubble or a
# drain. Rivets domes them the right way round, and eleven to a seam divides
# the tile exactly, so the row carries on across the wrap instead of doubling
# up at the join.
foreach ($s in @(0.0, 0.5)) {
    $sx = $W * $s
    $sy = $H * $s
    foreach ($o in $offsets) {
        Rivets $g ($sx + $o[0]) ($o[1]) ($sx + $o[0]) ($H + $o[1]) 11 (1.6 * $SS) $steelDk $steelTop
        Rivets $g ($o[0]) ($sy + $o[1]) ($W + $o[0]) ($sy + $o[1]) 11 (1.6 * $SS) $steelDk $steelTop
    }
}

# Grain. A sheet never goes through the relief pass - the pass lights edges,
# and the edges of a floor tile are the middle of the floor - so the speckle
# every other texture gets for free has to be painted here. Alternating a light
# and a dark mote keeps the average value where the palette put it instead of
# slowly fogging the plate one way.
for ($i = 0; $i -lt 220; $i++) {
    $gx = $rand.Next(0, $W); $gy = $rand.Next(0, $H)
    $rad = (0.4 + $rand.NextDouble() * 0.8) * $SS
    if (($i % 2) -eq 0) { $mote = C 168 180 190 24 } else { $mote = C 18 22 28 30 }
    foreach ($o in $offsets) { Disc $g $mote ($gx + $o[0]) ($gy + $o[1]) $rad }
}

# Rust, where the sea has been getting in. Three marks to a patch - a wide
# stain, a wet core, and a bright crust up-light of it - because a single
# flat disc of brown is a coffee ring. Still kept low in alpha: the tile is
# laid seventeen across by eleven down, so a patch that reads as weathering
# on its own reads as carpet by the time the floor is finished.
for ($i = 0; $i -lt 14; $i++) {
    $rx = $rand.Next(0, $W); $ry = $rand.Next(0, $H)
    $rad = (3 + $rand.NextDouble() * 6) * $SS
    foreach ($o in $offsets) {
        Disc $g (Fade $rust 26) ($rx + $o[0]) ($ry + $o[1]) $rad
        Disc $g (Fade $rustLo 32) ($rx + $o[0]) ($ry + $o[1]) ($rad * 0.5)
        Disc $g (Fade $rustHi 30) ($rx - $rad * 0.3 + $o[0]) ($ry - $rad * 0.3 + $o[1]) ($rad * 0.22)
    }
}

# Deck marking, worn to almost nothing. One saturated colour in a floor of grey
# is worth more than another ten marks of grey.
for ($i = 0; $i -lt 3; $i++) {
    $px = $W * 0.12 + $i * 10
    $py = $H * 0.70 + $i * 10
    foreach ($o in $offsets) {
        Stroke $g (Fade $paintY 34) ($px + $o[0]) ($py + $o[1]) `
            ($px + $W * 0.38 + $o[0]) ($py - $W * 0.32 + $o[1]) (2.5 * $SS)
    }
}
Finish $bmp $g $tile $tile "deck_plate.png"

# =====================================================================
#  DIVER  -  you, out of the sub. Drawn facing right like everything
#  else in the game, seen from above: helmet, shoulders, tank.
#
#  The canvas changed from 34x30 to 28x36. Seen from directly above a man
#  is wider across the shoulders than he is long, which the old comment
#  said and the old canvas contradicted - and at 30 pixels across, both
#  gloves were sliced off by one output pixel each, ring and all.
# =====================================================================
$suit   = C 168 146 62
$suitLo = C 110 92 38
$suitHi = C 214 196 104

$outW = 28; $outH = 36
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$rand = New-Object System.Random -ArgumentList 9173

# The tank and its hose, behind everything and so drawn first.
Blob $g @( (P ($W * 0.32) ($cy - 46)), (P ($W * 0.08) ($cy - 40)),
           (P ($W * 0.06) ($cy + 40)), (P ($W * 0.32) ($cy + 46)) ) $rubber $ink (1.2 * $SS) 0.3
foreach ($band in @(-30, 30)) {
    Stroke $g $steelHi ($W * 0.09) ($cy + $band) ($W * 0.30) ($cy + $band) (1.2 * $SS)
    Stroke $g (Fade $steelTop 150) ($W * 0.09) ($cy + $band - 2) ($W * 0.30) ($cy + $band - 2) (0.5 * $SS)
}
Disc $g $steelHi ($W * 0.14) $cy (1.4 * $SS)
Disc $g (Fade $steelTop 170) ($W * 0.13) ($cy - 1.5) (0.7 * $SS)

# The hose. Two straight strokes read as a bent bar; a hose is a hose because
# it sags between the points it is clipped at, and the knots are where it is
# clipped.
$hose = @( @(0.15, -44), @(0.26, -56), @(0.40, -58), @(0.50, -49), @(0.55, -35), @(0.57, -24) )
for ($i = 0; $i -lt $hose.Count - 1; $i++) {
    Stroke $g $rubber ($W * $hose[$i][0]) ($cy + $hose[$i][1]) `
        ($W * $hose[$i + 1][0]) ($cy + $hose[$i + 1][1]) (2 * $SS)
    if ($i -gt 0) { Disc $g (C 28 26 30) ($W * $hose[$i][0]) ($cy + $hose[$i][1]) (1.2 * $SS) }
}

# Arms, held out from the body the way a hard suit forces them, ending in
# gloves that are a different colour so the ends of him are findable.
foreach ($side in @(-1, 1)) {
    Stroke $g $suit ($W * 0.40) ($cy + $side * 42) ($W * 0.60) ($cy + $side * 54) (4.5 * $SS)
    Stroke $g $suitLo ($W * 0.40) ($cy + $side * 46) ($W * 0.58) ($cy + $side * 58) (1.5 * $SS)
    Stroke $g (Fade $suitHi 150) ($W * 0.41) ($cy + $side * 38) ($W * 0.58) ($cy + $side * 50) (0.8 * $SS)
    Disc $g $rubber ($W * 0.64) ($cy + $side * 54) 12
    Ring $g $ink ($W * 0.64) ($cy + $side * 54) 12 (1.5 * $SS)
    for ($f = -1; $f -le 1; $f++) {
        Stroke $g (Fade $ink 170) ($W * 0.66) ($cy + $side * 54 + $f * 5) `
            ($W * 0.70) ($cy + $side * 54 + $f * 6) (0.6 * $SS)
    }
}

# Shoulders: wide across, short along. Then the countershading every creature
# in the game gets and the hardware never did - a pale band where the light
# lands and a dark one under the far shoulder, both clipped to the silhouette
# so they bend round him instead of lying flat across him.
$shoulder = @( (P ($W * 0.58) $cy), (P ($W * 0.50) ($cy - 50)),
               (P ($W * 0.24) ($cy - 46)), (P ($W * 0.20) $cy),
               (P ($W * 0.24) ($cy + 46)), (P ($W * 0.50) ($cy + 50)) )
Blob $g $shoulder $suit $ink (1.5 * $SS) 0.3
$path = Curve $shoulder 0.3
$saved = $g.Save()
$g.SetClip($path, [System.Drawing.Drawing2D.CombineMode]::Intersect)
Box $g (Fade $suitHi 120) 0 ($cy - 52) $W 34
Box $g (Fade $suitLo 140) 0 ($cy + 16) $W 40
Stroke $g (Fade $suitHi 170) ($W * 0.26) ($cy - 26) ($W * 0.50) ($cy - 28) (1.5 * $SS)
Stroke $g (Fade $ink 90) ($W * 0.26) ($cy + 24) ($W * 0.50) ($cy + 26) (1.2 * $SS)
$g.Restore($saved)
$path.Dispose()

# The collar, and then the helmet sitting in it: narrower than the shoulders,
# which is the whole reason the shape reads as a man rather than a drum. The
# collar bolts go round it one at a time - Rivets walks a line, and a line of
# length zero is one rivet, which keeps every dome on the deck lit from the
# same corner without writing the offsets out again here.
Disc $g $suitLo ($W * 0.58) $cy 30
for ($i = 0; $i -lt 8; $i++) {
    $a = [math]::PI * 2 * $i / 8.0 + 0.3
    $bx = $W * 0.58 + [math]::Cos($a) * 27
    $by = $cy + [math]::Sin($a) * 27
    Rivets $g $bx $by $bx $by 1 (0.9 * $SS) $ink $suitHi
}
Disc $g $suitHi ($W * 0.62) $cy 26
Ring $g $ink ($W * 0.62) $cy 26 (1.2 * $SS)
Stroke $g $suitLo ($W * 0.62) ($cy - 26) ($W * 0.62) ($cy + 26) (1 * $SS)

# One window, in the front of it: the glass, a reflection of the sky in the top
# of it, and the shadow of the rim in the bottom. Flat glass is a sticker.
Blob $g @( (P ($W * 0.76) $cy), (P ($W * 0.69) ($cy - 15)),
           (P ($W * 0.62) ($cy - 12)), (P ($W * 0.62) ($cy + 12)),
           (P ($W * 0.69) ($cy + 15)) ) $glass $ink (1 * $SS) 0.2
Disc $g (C 255 255 255 90) ($W * 0.70) ($cy - 4) (2 * $SS)
Disc $g (C 20 44 52 150) ($W * 0.67) ($cy + 5) (2.5 * $SS)

# The head lamp. The only warm light anywhere on the deck, and therefore the
# thing the eye finds first and the cheapest possible arrow saying which way he
# is pointing. Kept at 2.2 rather than the 4 it wants, because Lamp throws a
# halo two and a half times its radius and at 4 the halo would be sheared off
# square by the right edge of the canvas.
Lamp $g ($W * 0.82) $cy (2.2 * $SS) $lampWarm

for ($i = 0; $i -lt 6; $i++) {
    Disc $g (C 128 82 48 55) ($W * (0.22 + $rand.NextDouble() * 0.34)) `
        ($cy + ($rand.NextDouble() - 0.5) * 84) ((0.8 + $rand.NextDouble() * 1.2) * $SS)
}
Finish $bmp $g $outW $outH "deck_diver.png"

# --- a boot, drawn twice and swung, which is what makes him walk ------
# 12x9 gave a lead boot three marks at nineteen screen pixels. 16x12 is room
# for a sole, a welt and a buckle, which is the difference between a foot and
# a smudge that moves.
$outW = 16; $outH = 12
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

$sole = @( (P ($W * 0.94) ($H * 0.5)), (P ($W * 0.44) ($H * 0.08)),
           (P ($W * 0.05) ($H * 0.32)), (P ($W * 0.05) ($H * 0.68)),
           (P ($W * 0.44) ($H * 0.92)) )
Blob $g $sole $rubber $ink (1 * $SS) 0.3
$path = Curve $sole 0.3
$saved = $g.Save()
$g.SetClip($path, [System.Drawing.Drawing2D.CombineMode]::Intersect)
Box $g (Fade $steelDk 200) 0 ($H * 0.74) $W ($H * 0.26)
Box $g (Fade $steelHi 110) 0 ($H * 0.06) $W ($H * 0.18)
for ($i = 0; $i -lt 4; $i++) {
    $gx = $W * (0.26 + 0.15 * $i)
    Stroke $g (Fade $steelDk 160) $gx ($H * 0.14) ($gx - 3) ($H * 0.86) (0.8 * $SS)
}
$g.Restore($saved)
$path.Dispose()
Stroke $g (Fade $steelHi 120) ($W * 0.78) ($H * 0.36) ($W * 0.90) ($H * 0.48) (1 * $SS)
Disc $g $lampWarm ($W * 0.32) ($H * 0.5) (1.2 * $SS)
Disc $g (Fade $ink 150) ($W * 0.32) ($H * 0.5) (0.5 * $SS)
Disc $g (C 128 82 48 60) ($W * 0.14) ($H * 0.62) (1.4 * $SS)
Finish $bmp $g $outW $outH "deck_boot.png"

# =====================================================================
#  CRANE  -  where the hold gets sold. A winch drum, a jib out over the
#  side, a hook on the end of it.
#
#  Drawn mirrored: base and drum on the RIGHT, jib reaching LEFT. It used
#  to be drawn the other way round and turned 180 degrees in Deck.cpp so
#  the jib hung off the port side, which also turned every baked highlight
#  on it to face down and right. The relief pass lights from up and left
#  and bakes that in, so the crane was the one object on the deck lit from
#  the opposite corner from the bench and the crates, and the eye finds
#  that immediately even when it cannot say what is wrong. This is drawn
#  as it will be drawn - unrotated.
# =====================================================================
$outW = 150; $outH = 112
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0
$rand = New-Object System.Random -ArgumentList 2281

# The pedestal. A bare filled rectangle with no outline and no bevel is why
# this used to read as a black tile: nothing on it said which face was up.
Panel $g ($W * 0.56) ($cy - 76) ($W * 0.41) 152 $steel $steelTop $steelDk (2.5 * $SS)
Edge $g $ink ($W * 0.56) ($cy - 76) ($W * 0.41) 152 (1.5 * $SS)
foreach ($row in @(@(0.575, -64, 0.955, -64), @(0.575, 64, 0.955, 64),
                   @(0.575, -64, 0.575, 64), @(0.955, -64, 0.955, 64))) {
    Rivets $g ($W * $row[0]) ($cy + $row[1]) ($W * $row[2]) ($cy + $row[3]) 7 (1.5 * $SS) $steelDk $steelTop
}

# The slew ring it turns on. One circle, and the whole base stops being a crate.
Ring $g $steelDk ($W * 0.76) $cy 66 (2 * $SS)
Ring $g (Fade $steelTop 160) ($W * 0.76) $cy 61 (1 * $SS)

# The jib. Twelve identical diagonals of one tone was a hatched rectangle;
# Truss gives it two chords, alternating lacing and a node at every joint,
# which is what makes offshore steel recognisable from a mile off.
Truss $g ($W * 0.62) $cy ($W * 0.07) $cy 66 7 $steelHi $steel $steelDk (3.5 * $SS)
Disc $g $steelDk ($W * 0.61) $cy (5 * $SS)
Ring $g (Fade $steelTop 180) ($W * 0.61) $cy (4 * $SS) (1 * $SS)

# The drum. Its axis has to lie across the jib or the cable cannot spool, so
# the windings run along x and step down y. Nine windings at 1.25 output pixels
# were invisible; six at 2 with a lit edge beside each are a drum.
Panel $g ($W * 0.63) ($cy - 72) ($W * 0.16) 144 $steelLo $steelHi $steelDk (2 * $SS)
foreach ($flange in @(-82, 66)) {
    Panel $g ($W * 0.615) ($cy + $flange) ($W * 0.19) 16 $steel $steelTop $steelDk (1.5 * $SS)
}
for ($i = 0; $i -lt 6; $i++) {
    $wy = $cy - 60 + $i * 24
    Stroke $g $steelDk ($W * 0.635) $wy ($W * 0.785) $wy (2 * $SS)
    Stroke $g (Fade $steelTop 190) ($W * 0.635) ($wy - 1.5 * $SS) ($W * 0.785) ($wy - 1.5 * $SS) (0.8 * $SS)
}

# The operator's house, on the pedestal beside the drum. Without it the crane
# has no focus: it is all structure and nothing that anyone sits in.
Panel $g ($W * 0.805) ($cy - 66) ($W * 0.145) 60 $steel $steelTop $steelDk (2 * $SS)
Edge $g $ink ($W * 0.805) ($cy - 66) ($W * 0.145) 60 (1.2 * $SS)
Blob $g @( (P ($W * 0.815) ($cy - 58)), (P ($W * 0.935) ($cy - 58)),
           (P ($W * 0.935) ($cy - 38)), (P ($W * 0.815) ($cy - 38)) ) $glass $ink (1.2 * $SS) 0.05
Lamp $g ($W * 0.875) ($cy - 16) (2 * $SS) $lampWarm

# The fall, off the drum and down the jib to the block. Thin and half
# transparent on purpose: a cable at full opacity and a quarter of a pixel wide
# comes out as a dashed line.
Stroke $g (C 150 158 166 170) ($W * 0.70) ($cy - 6) ($W * 0.075) ($cy - 2) (0.8 * $SS)

# The hook block, as far outboard as the canvas will carry it without shearing
# the sheave off square at the edge.
Disc $g $steelDk ($W * 0.055) $cy (4 * $SS)
Ring $g $steelTop ($W * 0.055) $cy (3.5 * $SS) (1.2 * $SS)
Disc $g $steel ($W * 0.055) $cy (2 * $SS)
Disc $g $steelDk ($W * 0.055) $cy (0.8 * $SS)

for ($i = 0; $i -lt 9; $i++) {
    Disc $g (C 132 86 52 40) ($W * (0.55 + $rand.NextDouble() * 0.42)) `
        ($cy + ($rand.NextDouble() - 0.5) * 150) ((1 + $rand.NextDouble() * 2) * $SS)
}
Finish $bmp $g $outW $outH "deck_crane.png"

# =====================================================================
#  BENCH  -  the workshop. Where the tree gets bought.
#
#  This was the emptiest sprite on the rig at one mark per thousand
#  pixels, which is eighteen times thinner than the animal you meet on
#  the first dive. A workbench is a flat plate covered in small bright
#  things, and it was a flat plate.
# =====================================================================
$outW = 146; $outH = 84
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$rand = New-Object System.Random -ArgumentList 6619

Panel $g 0 ($H * 0.08) $W ($H * 0.84) $steel $steelTop $steelDk (2.5 * $SS)
Edge $g $ink 0 ($H * 0.08) $W ($H * 0.84) (1.5 * $SS)

# Planks. Each one is a dark joint with a lit edge on the side the light comes
# from, which is the trick make_hull uses on the sub's frames: one line is a
# scratch, a pair is a step in the surface.
for ($i = 1; $i -lt 8; $i++) {
    $py = $H * (0.08 + 0.105 * $i)
    Stroke $g (Fade $steelDk 150) 0 $py $W $py (0.8 * $SS)
    Stroke $g (Fade $steelTop 110) 0 ($py - 1.6 * $SS) $W ($py - 1.6 * $SS) (0.5 * $SS)
}
Rivets $g ($W * 0.02) ($H * 0.88) ($W * 0.98) ($H * 0.88) 14 (1.2 * $SS) $steelDk $steelTop
Rivets $g ($W * 0.04) ($H * 0.12) ($W * 0.96) ($H * 0.12) 10 (1.2 * $SS) $steelDk $steelTop

# A vice at one end: two jaws, a screw between them and a handle across it.
Panel $g ($W * 0.05) ($H * 0.28) ($W * 0.06) ($H * 0.40) $steelHi $steelTop $steelDk (1.5 * $SS)
Panel $g ($W * 0.13) ($H * 0.30) ($W * 0.05) ($H * 0.36) $steelLo $steelHi $steelDk (1.5 * $SS)
Stroke $g $steelTop ($W * 0.11) ($H * 0.48) ($W * 0.145) ($H * 0.48) (2 * $SS)
Stroke $g $steelDk ($W * 0.17) ($H * 0.48) ($W * 0.25) ($H * 0.48) (2 * $SS)
Disc $g $steelHi ($W * 0.26) ($H * 0.48) (2 * $SS)
Ring $g $ink ($W * 0.26) ($H * 0.48) (2 * $SS) (1 * $SS)

# Eleven tools, each with a body and a highlight offset up and left. Five flat
# grey shapes on a flat grey plate is a stain; the highlight is what lifts each
# one off the bench.
$lift = 1 * $SS
Stroke $g $rubber ($W * 0.32) ($H * 0.30) ($W * 0.44) ($H * 0.27) (2.5 * $SS)
Stroke $g (Fade $steelTop 150) ($W * 0.32) ($H * 0.30 - $lift) ($W * 0.44) ($H * 0.27 - $lift) (0.8 * $SS)
Panel $g ($W * 0.44) ($H * 0.21) ($W * 0.05) ($H * 0.11) $steelHi $steelTop $steelDk (1.2 * $SS)

foreach ($sp in @(@(0.52, 0.30, 0.62, 0.26), @(0.34, 0.62, 0.46, 0.66))) {
    Stroke $g $steel ($W * $sp[0]) ($H * $sp[1]) ($W * $sp[2]) ($H * $sp[3]) (2 * $SS)
    Stroke $g (Fade $steelTop 150) ($W * $sp[0]) ($H * $sp[1] - $lift) ($W * $sp[2]) ($H * $sp[3] - $lift) (0.7 * $SS)
    Ring $g $steelHi ($W * $sp[0]) ($H * $sp[1]) (2 * $SS) (1 * $SS)
    Ring $g $steelHi ($W * $sp[2]) ($H * $sp[3]) (1.6 * $SS) (1 * $SS)
}

Stroke $g $rubber ($W * 0.50) ($H * 0.72) ($W * 0.56) ($H * 0.74) (2.2 * $SS)
Stroke $g $steelTop ($W * 0.56) ($H * 0.74) ($W * 0.62) ($H * 0.75) (1 * $SS)
Stroke $g $steelLo ($W * 0.36) ($H * 0.44) ($W * 0.48) ($H * 0.43) (1.8 * $SS)
for ($i = 0; $i -lt 5; $i++) {
    $fx = $W * (0.37 + 0.022 * $i)
    Stroke $g (Fade $steelTop 130) $fx ($H * 0.42) ($fx + 4) ($H * 0.46) (0.5 * $SS)
}
Stroke $g $steelDk ($W * 0.64) ($H * 0.62) ($W * 0.72) ($H * 0.68) (2 * $SS)
Stroke $g $steelDk ($W * 0.64) ($H * 0.68) ($W * 0.72) ($H * 0.62) (2 * $SS)
Stroke $g (Fade $steelTop 150) ($W * 0.65) ($H * 0.62 - $lift) ($W * 0.71) ($H * 0.66 - $lift) (0.6 * $SS)

foreach ($bolt in @(@(0.30, 0.50), @(0.33, 0.55), @(0.28, 0.58))) {
    Disc $g $steelHi ($W * $bolt[0]) ($H * $bolt[1]) (0.8 * $SS)
    Disc $g (Fade $steelTop 160) ($W * $bolt[0] - 1) ($H * $bolt[1] - 1) (0.4 * $SS)
}
Ring $g $rust ($W * 0.88) ($H * 0.66) (2.5 * $SS) (0.7 * $SS)
Ring $g $rustHi ($W * 0.88) ($H * 0.66) (1.6 * $SS) (0.5 * $SS)
Ring $g $rust ($W * 0.88) ($H * 0.66) (0.9 * $SS) (0.5 * $SS)

# The part waiting to go on, and the lamp over the bench. A cone of warm light
# on a grey plate does more for "somebody works here" than another ten tools.
Disc $g $glass ($W * 0.78) ($H * 0.42) 22
Ring $g $ink ($W * 0.78) ($H * 0.42) 22 (1.5 * $SS)
Disc $g (C 255 255 255 90) ($W * 0.765) ($H * 0.37) (2 * $SS)
Blob $g @( (P ($W * 0.66) ($H * 0.24)), (P ($W * 0.88) ($H * 0.14)),
           (P ($W * 0.90) ($H * 0.72)), (P ($W * 0.62) ($H * 0.40)) ) (C 250 216 150 26) $null 0 0.1
Lamp $g ($W * 0.66) ($H * 0.22) (3 * $SS) $lampWarm

for ($i = 0; $i -lt 6; $i++) {
    Disc $g (C 22 20 24 90) ($W * (0.24 + $rand.NextDouble() * 0.66)) `
        ($H * (0.18 + $rand.NextDouble() * 0.66)) ((1.5 + $rand.NextDouble() * 2.5) * $SS)
}
for ($i = 0; $i -lt 8; $i++) {
    Disc $g (Fade $rustHi 70) ($W * (0.06 + $rand.NextDouble() * 0.88)) `
        ($H * (0.80 + $rand.NextDouble() * 0.10)) (0.5 * $SS)
}
Finish $bmp $g $outW $outH "deck_bench.png"

# =====================================================================
#  BELL  -  the way down. It sits in the moon pool; the hole in the
#  deck is drawn live underneath it, so this is only the bell.
# =====================================================================
$outW = 132; $outH = 132
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $mid = $W / 2.0
$rand = New-Object System.Random -ArgumentList 5303

Disc $g $steelLo $mid $mid ($mid * 0.86)
Disc $g $steel $mid $mid ($mid * 0.78)

# Countershading, clipped to the shell. A disc of one flat tone is a coin no
# matter what is drawn on top of it; a pale offset copy up-light and a dark one
# down-light is the single cheapest thing that turns it into a sphere, and it
# does it without waiting for the relief pass - which cannot help much here
# anyway, because at 132 pixels the dome radius is wider than any rib on it.
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$path.AddEllipse((RectF ($mid - $mid * 0.86) ($mid - $mid * 0.86) ($mid * 1.72) ($mid * 1.72)))
$saved = $g.Save()
$g.SetClip($path, [System.Drawing.Drawing2D.CombineMode]::Intersect)
Disc $g (C 172 184 194 60) ($mid - $mid * 0.16) ($mid - $mid * 0.16) ($mid * 0.78)
Disc $g (C 20 24 30 80) ($mid + $mid * 0.14) ($mid + $mid * 0.14) ($mid * 0.74)
$g.Restore($saved)
$path.Dispose()
Ring $g $ink $mid $mid ($mid * 0.86) (1.8 * $SS)

# Eight ribs, each a shadow with a lit edge laid beside it against the light.
for ($i = 0; $i -lt 8; $i++) {
    $a = [math]::PI * 2 * $i / 8.0
    $ca = [math]::Cos($a); $sa = [math]::Sin($a)
    Stroke $g $steelDk ($mid + $ca * $mid * 0.24) ($mid + $sa * $mid * 0.24) `
        ($mid + $ca * $mid * 0.82) ($mid + $sa * $mid * 0.82) (2.5 * $SS)
    Stroke $g (Fade $steelTop 140) ($mid + $ca * $mid * 0.24 + $sa * 1.5 * $SS) ($mid + $sa * $mid * 0.24 - $ca * 1.5 * $SS) `
        ($mid + $ca * $mid * 0.82 + $sa * 1.5 * $SS) ($mid + $sa * $mid * 0.82 - $ca * 1.5 * $SS) (0.8 * $SS)
}

# Twenty four bolts round the shell, one Rivets call each so every dome on the
# deck is lit from the same corner.
for ($i = 0; $i -lt 24; $i++) {
    $a = [math]::PI * 2 * $i / 24.0 + 0.13
    $bx = $mid + [math]::Cos($a) * $mid * 0.80
    $by = $mid + [math]::Sin($a) * $mid * 0.80
    Rivets $g $bx $by $bx $by 1 (1.2 * $SS) $steelDk $steelTop
}

# The hatch on top, open, with its hinge and its dogs. An open hatch is the one
# thing on the bell that says it is waiting for you rather than parked.
Disc $g $steelHi $mid $mid ($mid * 0.40)
Ring $g $ink $mid $mid ($mid * 0.40) (1.5 * $SS)
Disc $g (C 12 20 26) $mid $mid ($mid * 0.30)
Disc $g (C 40 92 104 150) $mid ($mid * 0.94) ($mid * 0.14)
Panel $g ($mid - 3 * $SS) ($mid - $mid * 0.52) (6 * $SS) (14 * $SS) $steelLo $steelHi $steelDk (1.2 * $SS)
for ($i = 0; $i -lt 4; $i++) {
    $a = [math]::PI * 2 * $i / 4.0 + 0.78
    Stroke $g $steelTop ($mid + [math]::Cos($a) * $mid * 0.34) ($mid + [math]::Sin($a) * $mid * 0.34) `
        ($mid + [math]::Cos($a) * $mid * 0.48) ($mid + [math]::Sin($a) * $mid * 0.48) (1.5 * $SS)
}
Ring $g $steelHi $mid ($mid * 0.30) (3 * $SS) (1.5 * $SS)

# Four ports round the waist, lit from inside. A flat disc of cyan is a sticker;
# Lamp lays a halo down first, so the light looks like it is coming out.
for ($i = 0; $i -lt 4; $i++) {
    $a = [math]::PI * 2 * $i / 4.0 + 0.4
    $px = $mid + [math]::Cos($a) * $mid * 0.58
    $py = $mid + [math]::Sin($a) * $mid * 0.58
    Disc $g $steelHi $px $py (5.5 * $SS)
    Lamp $g $px $py (3.2 * $SS) (C 150 226 234)
    Ring $g $ink $px $py (5 * $SS) (1.5 * $SS)
    for ($k = 0; $k -lt 6; $k++) {
        $sa = [math]::PI * 2 * $k / 6.0
        Disc $g $steelTop ($px + [math]::Cos($sa) * 5.8 * $SS) ($py + [math]::Sin($sa) * 5.8 * $SS) (0.6 * $SS)
    }
}

# Rust along the waterline it sits at when it is hanging in the pool.
for ($i = 0; $i -lt 10; $i++) {
    $a = $rand.NextDouble() * [math]::PI * 2
    $rr = $mid * (0.70 + $rand.NextDouble() * 0.16)
    Disc $g (C 132 86 52 45) ($mid + [math]::Cos($a) * $rr) ($mid + [math]::Sin($a) * $rr) ((1 + $rand.NextDouble() * 2) * $SS)
}
Finish $bmp $g $outW $outH "deck_bell.png"

# =====================================================================
#  CRATE  -  dressing, and what the hold gets stacked into.
# =====================================================================
$outW = 46; $outH = 46
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS
$rand = New-Object System.Random -ArgumentList 7717

Panel $g 0 0 $W $W $steel $steelTop $steelDk (2 * $SS)

# The lid line, and the light catching its edge.
Stroke $g $steelDk 0 ($W * 0.18) $W ($W * 0.18) (1.2 * $SS)
Stroke $g (Fade $steelTop 130) 0 ($W * 0.18 - 1.2 * $SS) $W ($W * 0.18 - 1.2 * $SS) (0.6 * $SS)

# Banding. Two stripes are two stripes; four make a cage, and each needs a dark
# side and a lit side or the strap lies in the same plane as the lid.
foreach ($sx in @(0.22, 0.78)) {
    Stroke $g $rustLo ($W * $sx + 1.5 * $SS) 0 ($W * $sx + 1.5 * $SS) $W (4 * $SS)
    Stroke $g $rust ($W * $sx) 0 ($W * $sx) $W (3 * $SS)
    Stroke $g (Fade $rustHi 120) ($W * $sx - 1.5 * $SS) 0 ($W * $sx - 1.5 * $SS) $W (1 * $SS)
}
foreach ($sy in @(0.22, 0.78)) {
    Stroke $g $rustLo 0 ($W * $sy + 1.5 * $SS) $W ($W * $sy + 1.5 * $SS) (4 * $SS)
    Stroke $g $rust 0 ($W * $sy) $W ($W * $sy) (3 * $SS)
    Stroke $g (Fade $rustHi 120) 0 ($W * $sy - 1.5 * $SS) $W ($W * $sy - 1.5 * $SS) (1 * $SS)
}

# Corner fittings, three bolts to each. This is what stops a crate reading as a
# tile of floor that somebody dropped a colour onto.
foreach ($corner in @(@(0, 0), @(1, 0), @(0, 1), @(1, 1))) {
    $fx = $corner[0] * ($W - 8 * $SS)
    $fy = $corner[1] * ($W - 8 * $SS)
    Panel $g $fx $fy (8 * $SS) (8 * $SS) $steelLo $steelHi $steelDk (1.5 * $SS)
    Rivets $g ($fx + 2 * $SS) ($fy + 2 * $SS) ($fx + 6 * $SS) ($fy + 6 * $SS) 3 (1 * $SS) $steelDk $steelTop
}

# The lock plate in the middle of the lid.
Disc $g $steelDk ($W * 0.50) ($W * 0.58) (2.5 * $SS)
Ring $g $steelTop ($W * 0.50) ($W * 0.58) (2 * $SS) (0.8 * $SS)
Stroke $g $steelHi ($W * 0.50) ($W * 0.58) ($W * 0.50) ($W * 0.68) (1 * $SS)

# Stencilling. Three short marks at the size of a word read as a word; actual
# letters at this size read as dirt.
for ($i = 0; $i -lt 3; $i++) {
    $ty = $W * (0.42 + 0.05 * $i)
    Stroke $g (Fade $paintY 90) ($W * 0.28) $ty ($W * 0.46) $ty (0.8 * $SS)
}

Edge $g $ink 0 0 ($W - 1) ($W - 1) (1.5 * $SS)

for ($i = 0; $i -lt 8; $i++) {
    $sx = $W * $rand.NextDouble(); $sy = $W * $rand.NextDouble()
    Stroke $g (C 176 186 194 70) $sx $sy ($sx + 4 + $rand.Next(0, 14)) ($sy - 2 + $rand.Next(0, 8)) (0.5 * $SS)
}
for ($i = 0; $i -lt 5; $i++) {
    Disc $g (C 132 86 52 50) ($W * $rand.NextDouble()) ($W * $rand.NextDouble()) ((1 + $rand.NextDouble() * 2) * $SS)
}
Finish $bmp $g $outW $outH "deck_crate.png"

# =====================================================================
#  RAIL  -  the edge of the deck. Tiles left to right.
#
#  Drawn symmetrically about its own long axis, and that is not a style
#  choice. Deck.cpp lays this same strip four times, at 0, 90, 180 and
#  -90 degrees, one run down each edge of the plate. A highlight baked
#  along one long side therefore arrives on screen pointing four
#  different ways, and the deck ends up with four suns. Lit down the
#  crown of each bar instead - light in the middle of the bar, dark at
#  both its edges - it is a rail seen from directly overhead, which is
#  the one lighting that survives being turned through a right angle.
#  The height must stay 20: the C++ asks for a texture rect 20 pixels
#  tall and tiles the width to whatever the run needs.
# =====================================================================
$outW = 64; $outH = 20
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$rand = New-Object System.Random -ArgumentList 3331
$xoffsets = @(-$W, 0, $W)

# Toe boards, one at each long edge, so the rail stands on something instead
# of floating. Mirrored, like everything else here.
foreach ($ky in @(0.0, ($H - 6.4))) {
    Box $g $steelLo 0 $ky $W 6.4
    Box $g (Fade $steelDk 180) 0 $ky $W 2.0
    Box $g (Fade $steelDk 180) 0 ($ky + 4.4) $W 2.0
}

# The shadow the rail throws on the deck between its bars. Half transparent,
# because the plate underneath has to show through or the rail reads as a wall.
Box $g (Fade $ink 90) 0 ($H * 0.33) $W ($H * 0.34)

# The two bars. Five marks apiece, laid symmetrically about each bar's own
# centre line so the crown is the brightest thing and both edges fall away.
foreach ($by in @(6.4, 53.6)) {
    Box $g $steel 0 $by $W 20
    Box $g $steelHi 0 ($by + 3.6) $W 12.8
    Box $g $steelTop 0 ($by + 6.8) $W 6.4
    Box $g (Fade $steelDk 190) 0 $by $W 2.4
    Box $g (Fade $steelDk 190) 0 ($by + 17.6) $W 2.4
}

# Stanchions. The one at x = 0 straddles the join, so every mark on it goes
# down three times - at the tile, one width left and one width right - or the
# post loses half of itself at the seam.
foreach ($px in @(0.0, ($W * 0.5))) {
    foreach ($ox in $xoffsets) {
        $sx = $px + $ox
        Box $g $steelLo ($sx - 10 * $SS) 0 (20 * $SS) $H
        Box $g (Fade $steelDk 170) ($sx - 10 * $SS) 0 (2 * $SS) $H
        Box $g (Fade $steelDk 170) ($sx + 8 * $SS) 0 (2 * $SS) $H
        Box $g $steel ($sx - 2.5 * $SS) 0 (5 * $SS) $H
        Box $g $steelTop ($sx - 1 * $SS) 0 (2 * $SS) $H
        foreach ($ry in @(0.16, 0.84)) {
            foreach ($rx in @(-7, 7)) {
                Disc $g $steelDk ($sx + $rx * $SS) ($H * $ry) (1.2 * $SS)
                Disc $g $steelTop ($sx + $rx * $SS) ($H * $ry) (0.7 * $SS)
            }
        }
    }
}

# Grain and rust, kept inside the bars so nothing floats in the gap. Random
# light and dark motes have no light direction of their own, which is the only
# reason noise is allowed on a sprite that gets turned four ways.
for ($i = 0; $i -lt 20; $i++) {
    $gx = $rand.Next(0, $W)
    if (($i % 2) -eq 0) { $gy = 6.4 + $rand.NextDouble() * 20 } else { $gy = 53.6 + $rand.NextDouble() * 20 }
    $rad = (0.4 + $rand.NextDouble() * 0.6) * $SS
    if (($i % 3) -eq 0) { $mote = C 132 86 52 40 } else { $mote = C 168 180 190 26 }
    foreach ($ox in $xoffsets) { Disc $g $mote ($gx + $ox) $gy $rad }
}
Finish $bmp $g $outW $outH "deck_rail.png"
