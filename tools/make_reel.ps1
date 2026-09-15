Add-Type -AssemblyName System.Drawing

# The cutscene plates.
#
# The two reels used to be drawn out of triangles, which is fine for a beam of
# light and hopeless for a rig, a station, or an animal the length of the
# screen. These are painted once and then moved, panned and lit by the reel -
# which is what a shot is: something drawn well, and a camera that does not
# hold still.
#
# Everything here is in profile. The game is top down, so a side view is the
# cheapest signal there is that what you are watching is not the game.
#
# Two things were wrong with the first version and both are fixed here.
#
# The first is density. reel_beast carried 0.11 drawing marks per thousand
# pixels where a creature carries 19 - a factor of a hundred and seventy five -
# because this file never touched organic.ps1 and so had no Body, no Mottle, no
# Eye, no Fin, no Truss and no Panel to reach for. It does now.
#
# The second is resolution. These plates are stretched: the station is drawn at
# nearly three times its own width in the shot that goes close on its hatches,
# and a 560 pixel texture blown up to 1624 is a blurred texture no matter how
# well it was drawn. Every plate is now composed at the size it is actually
# seen at, and the reel divides the scale each shot asks for by ReelFit so none
# of the framing had to be re-derived.
. "$PSScriptRoot\organic.ps1"

$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"

# Four, like every other script in the project. It was three here - the only
# exception anywhere - which cost a quarter of the edge quality on the largest
# art in the game for no reason anybody wrote down.
$SS = 4

function Box($g, $col, $x, $y, $span, $tall) {
    $g.FillRectangle((Brush $col), (RectF $x $y $span $tall))
}

function Edge($g, $col, $x, $y, $span, $tall, $width) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, ([float]$width)
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
    $g.DrawRectangle($pen, [float]$x, [float]$y, [float]$span, [float]$tall)
    $pen.Dispose()
}

function Fade($col, $alpha) { C $col.R $col.G $col.B $alpha }

function Curve($pts, $tension = 0.5) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, $tension)
    return $path
}

# Shadows the three argument Blob in organic.ps1 on purpose: nearly every shape
# in a cutscene plate wants an ink edge. It also means Body, Fin and Mottle
# from organic.ps1 must not be called from this file - they pass three
# arguments and the fourth would land on a Pen constructor.
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

# Countershading without organic.ps1's Body: clip to the silhouette, then lay a
# light band down the top of it and a dark one along the bottom. Three fills in
# one outline is the single thing that separates every creature in this game
# from a coloured shape, and no plate in this file used to have it.
function Shade($g, $pts, $tension, $lightCol, $darkCol, $top, $bottom) {
    $path = Curve $pts $tension
    $saved = $g.Save()
    $g.SetClip($path, [System.Drawing.Drawing2D.CombineMode]::Intersect)
    $bounds = $path.GetBounds()
    $g.FillRectangle((Brush $lightCol), (RectF $bounds.X $bounds.Y $bounds.Width ($bounds.Height * $top)))
    $g.FillRectangle((Brush $darkCol), (RectF $bounds.X ($bounds.Y + $bounds.Height * (1 - $bottom)) $bounds.Width ($bounds.Height * $bottom)))
    $g.Restore($saved)
    $path.Dispose()
}

# Grain, clipped to a shape. Two hundred low alpha specks is what stops a big
# flat fill reading as a big flat fill once it is stretched over half a screen.
function Speck($g, $pts, $tension, $count, $seed, $colA, $colB, $rad) {
    $path = Curve $pts $tension
    $saved = $g.Save()
    $g.SetClip($path, [System.Drawing.Drawing2D.CombineMode]::Intersect)
    $bounds = $path.GetBounds()
    $rand = New-Object System.Random -ArgumentList $seed
    for ($i = 0; $i -lt $count; $i++) {
        $sx = $bounds.X + $rand.NextDouble() * $bounds.Width
        $sy = $bounds.Y + $rand.NextDouble() * $bounds.Height
        if (($i % 2) -eq 0) { $col = $colA } else { $col = $colB }
        Disc $g $col $sx $sy ($rad * (0.5 + $rand.NextDouble()))
    }
    $g.Restore($saved)
    $path.Dispose()
}

function Finish($bmp, $g, $outW, $outH, $name) {
    $g.Dispose()
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, $outW, $outH))) {
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

"generating the reel plates..."

$steelTop = C 172 184 194
$steelHi  = C 122 132 138
$steel    = C  78  86  92
$steelLo  = C  44  50  56
$steelDk  = C  20  25  30
$rust     = C 104  72  50
$rustHi   = C 150 108  72
$rustLo   = C  66  42  26
$ink      = C  12  16  19
$lit      = C 236 196 120
$cold     = C 128 202 214
$rock     = C  38  44  48
$rockLo   = C  22  27  30
$rockHi   = C  68  78  82
$meat     = C  44  40  52
$meatHi   = C  86  78 100
$meatLo   = C  26  22  34
$meatPale = C 118 108 132
$rubber   = C  40  38  42
$suit     = C 128 112  54
$suitLo   = C  82  70  32
$suitHi   = C 176 158  88
$brass    = C 188 150  92
$paintY   = C 190 162  58
$growth   = C  70  84  64
$silt     = C  52  60  56

# =====================================================================
#  RIG  -  the thing on the surface, in profile. A deck on four lattice
#  legs with a derrick over the moon pool and a crane off the side. It
#  is the first thing in the game and the last.
# =====================================================================
$outW = 640; $outH = 320
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$deck = $H * 0.50
$rand = New-Object System.Random -ArgumentList 6101

# The legs. One stroke each is a table; a truss is offshore steel.
foreach ($x in @(0.13, 0.33, 0.67, 0.87)) {
    Truss $g ($W * $x) $deck ($W * ($x + 0.018)) $H (11 * $SS) 7 $steel $steelLo $steelDk (2.4 * $SS)
}
foreach ($pair in @(@(0.13, 0.33), @(0.33, 0.67), @(0.67, 0.87))) {
    Stroke $g $steelLo ($W * $pair[0]) ($deck + 30 * $SS) ($W * $pair[1]) ($H - 12 * $SS) (2 * $SS)
    Stroke $g $steelLo ($W * $pair[1]) ($deck + 30 * $SS) ($W * $pair[0]) ($H - 12 * $SS) (2 * $SS)
}

# The deck: a plate with a lit top edge, a shadowed underside and a row of
# rivets along the bottom flange.
Panel $g ($W * 0.05) ($deck - 11 * $SS) ($W * 0.90) (11 * $SS) $steel $steelTop $steelDk (1.6 * $SS)
Box $g $steelDk ($W * 0.05) $deck ($W * 0.90) (3 * $SS)
Rivets $g ($W * 0.06) ($deck + 1.4 * $SS) ($W * 0.94) ($deck + 1.4 * $SS) 34 (1.1 * $SS) $steelDk $steelHi

# Accommodation, two storeys, with the lights on. Five identical bright
# rectangles were the old version; a window is a dark frame with a lit pane in
# it and a halo that the plate itself cannot paint.
Panel $g ($W * 0.11) ($deck - 30 * $SS) ($W * 0.24) (19 * $SS) $steel $steelTop $steelDk (1.8 * $SS)
Edge $g $ink ($W * 0.11) ($deck - 30 * $SS) ($W * 0.24) (19 * $SS) (1.2 * $SS)
Panel $g ($W * 0.135) ($deck - 44 * $SS) ($W * 0.19) (14 * $SS) $steelLo $steelHi $steelDk (1.6 * $SS)
Edge $g $ink ($W * 0.135) ($deck - 44 * $SS) ($W * 0.19) (14 * $SS) (1.2 * $SS)
foreach ($row in @(-26, -40)) {
    foreach ($i in 0..4) {
        $wx = $W * (0.145 + 0.042 * $i)
        Box $g $steelDk $wx ($deck + $row * $SS) (5 * $SS) (5 * $SS)
        Box $g $lit ($wx + 0.8 * $SS) ($deck + ($row + 0.8) * $SS) (3.4 * $SS) (3.4 * $SS)
        Disc $g (Fade $lit 26) ($wx + 2.5 * $SS) ($deck + ($row + 2.5) * $SS) (6 * $SS)
    }
}
# Three vents on the roof, because a building with nothing on its roof is a box.
foreach ($i in 0..2) {
    $vx = $W * (0.16 + 0.07 * $i)
    Panel $g $vx ($deck - 50 * $SS) (7 * $SS) (6 * $SS) $steelLo $steelHi $steelDk (1.2 * $SS)
    Stroke $g $steelTop $vx ($deck - 50 * $SS) ($vx + 7 * $SS) ($deck - 50 * $SS) (1 * $SS)
}

# The derrick over the middle, which is where the bell goes over the side.
Truss $g ($W * 0.47) $deck ($W * 0.505) ($deck - 62 * $SS) (14 * $SS) 9 $steelHi $steel $steelDk (2.2 * $SS)
Truss $g ($W * 0.58) $deck ($W * 0.545) ($deck - 62 * $SS) (14 * $SS) 9 $steelHi $steel $steelDk (2.2 * $SS)
Panel $g ($W * 0.492) ($deck - 68 * $SS) ($W * 0.066) (6 * $SS) $steel $steelTop $steelDk (1.4 * $SS)
Disc $g $steelDk ($W * 0.525) ($deck - 65 * $SS) (2.4 * $SS)
Ring $g (Fade $steelTop 190) ($W * 0.525) ($deck - 65 * $SS) (2 * $SS) (0.8 * $SS)
Stroke $g (Fade $steelHi 170) ($W * 0.525) ($deck - 63 * $SS) ($W * 0.525) $deck (0.7 * $SS)
Lamp $g ($W * 0.525) ($deck - 72 * $SS) (2.2 * $SS) (C 255 96 84)

# The crane off the starboard side: pedestal, house, counterweight, boom, and a
# fall that leaves the frame rather than stopping short of it.
Panel $g ($W * 0.755) ($deck - 24 * $SS) ($W * 0.065) (24 * $SS) $steel $steelTop $steelDk (1.6 * $SS)
Panel $g ($W * 0.742) ($deck - 36 * $SS) ($W * 0.09) (13 * $SS) $steelLo $steelHi $steelDk (1.4 * $SS)
Box $g $steelDk ($W * 0.752) ($deck - 33 * $SS) (7 * $SS) (5 * $SS)
Box $g $cold ($W * 0.754) ($deck - 32.6 * $SS) (6 * $SS) (4 * $SS)
Panel $g ($W * 0.716) ($deck - 33 * $SS) ($W * 0.028) (9 * $SS) $steelLo $steelHi $steelDk (1.2 * $SS)
Truss $g ($W * 0.80) ($deck - 32 * $SS) ($W * 0.985) ($deck - 12 * $SS) (9 * $SS) 8 $steelHi $steel $steelDk (1.9 * $SS)
Stroke $g (Fade $steelHi 180) ($W * 0.975) ($deck - 13 * $SS) ($W * 0.975) ($deck + 14 * $SS) (0.8 * $SS)
Disc $g $steelDk ($W * 0.975) ($deck + 16 * $SS) (2.6 * $SS)
Lamp $g ($W * 0.748) ($deck - 38 * $SS) (1.8 * $SS) $lit

# Rust running down from every seam, and rain over the whole plate. The rain is
# baked in because this plate is only ever seen in the one shot where it is
# raining, and forty streaks cost nothing.
for ($i = 0; $i -lt 22; $i++) {
    $rx = $W * (0.06 + $rand.NextDouble() * 0.88)
    $len = (6 + $rand.Next(0, 22)) * $SS
    Stroke $g (Fade $rust 60) $rx ($deck + 2 * $SS) $rx ($deck + $len) (1.2 * $SS)
}
for ($i = 0; $i -lt 40; $i++) {
    $rx = $rand.NextDouble() * $W
    $ry = $rand.NextDouble() * $H
    Stroke $g (C 170 182 190 34) $rx $ry ($rx - 3 * $SS) ($ry + 14 * $SS) (0.6 * $SS)
}
Finish $bmp $g $outW $outH "reel_rig.png"

# =====================================================================
#  BELL  -  in profile, on the end of a cable. The thing the player is
#  inside for both of the rides that bracket the game.
# =====================================================================
$outW = 240; $outH = 280
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS; $c = $W / 2.0

# The cable, and the umbilical running beside it. The umbilical is what makes
# it a bell rather than a wrecking ball.
Stroke $g $steelDk $c 0 $c ($H * 0.24) (2.6 * $SS)
Stroke $g (Fade $steelHi 150) ($c - 0.8 * $SS) 0 ($c - 0.8 * $SS) ($H * 0.24) (0.8 * $SS)
Stroke $g $rubber ($c + 5 * $SS) 0 ($c + 8 * $SS) ($H * 0.26) (2.2 * $SS)
foreach ($i in 0..5) {
    Disc $g $steelLo ($c + (5 + $i * 0.6) * $SS) ($H * (0.03 + 0.04 * $i)) (1.4 * $SS)
}

# The sphere, then its countershading. A single fill is a barrel; two offset
# discs are a ball.
Disc $g $steelDk $c ($H * 0.56) ($W * 0.43)
Disc $g $steel $c ($H * 0.56) ($W * 0.40)
Disc $g (C 158 170 180 70) ($c - $W * 0.09) ($H * 0.50) ($W * 0.33)
Disc $g (C 14 18 24 95) ($c + $W * 0.08) ($H * 0.63) ($W * 0.33)
Ring $g $ink $c ($H * 0.56) ($W * 0.43) (1.8 * $SS)

# Eight meridian ribs, each a dark groove with a lit edge beside it.
for ($i = 0; $i -lt 8; $i++) {
    $a = [math]::PI * ($i / 8.0)
    $dx = [math]::Cos($a) * $W * 0.40
    $dy = [math]::Sin($a) * $W * 0.40
    Stroke $g $steelDk ($c - $dx) ($H * 0.56 - $dy) ($c + $dx) ($H * 0.56 + $dy) (1.6 * $SS)
}

# The waist band and a row of rivets round it.
Box $g $steelHi ($c - $W * 0.43) ($H * 0.545) ($W * 0.86) (4 * $SS)
Box $g $steelDk ($c - $W * 0.43) ($H * 0.545 + 4 * $SS) ($W * 0.86) (1.2 * $SS)
Rivets $g ($c - $W * 0.40) ($H * 0.556) ($c + $W * 0.40) ($H * 0.556) 20 (1.1 * $SS) $steelDk $steelTop

# Three ports with real lamps in them, and screws round each.
foreach ($x in @(-0.25, 0.0, 0.25)) {
    $px = $c + $W * $x
    $py = $H * 0.44
    Disc $g $steelDk $px $py (7.5 * $SS)
    Ring $g $steelTop $px $py (7 * $SS) (1.4 * $SS)
    Lamp $g $px $py (4.2 * $SS) (C 150 226 238)
    for ($s = 0; $s -lt 8; $s++) {
        $sa = [math]::PI * 2 * $s / 8.0
        Disc $g $steelHi ($px + [math]::Cos($sa) * 7 * $SS) ($py + [math]::Sin($sa) * 7 * $SS) (0.7 * $SS)
    }
}

# The yoke, drawn AFTER the sphere. It used to be drawn before and was almost
# entirely hidden behind it, which is why the bell looked like it was falling
# rather than hanging.
foreach ($side in @(-1, 1)) {
    Stroke $g $steelDk ($c + $side * $W * 0.34) ($H * 0.44) $c ($H * 0.23) (2.6 * $SS)
    Stroke $g (Fade $steelTop 150) ($c + $side * $W * 0.34) ($H * 0.44) $c ($H * 0.23) (0.9 * $SS)
    Disc $g $steelLo ($c + $side * $W * 0.34) ($H * 0.44) (2.6 * $SS)
    Disc $g $steelTop ($c + $side * $W * 0.34 - 0.6 * $SS) ($H * 0.44 - 0.6 * $SS) (1.2 * $SS)
}
Disc $g $steelLo $c ($H * 0.23) (3.4 * $SS)
Ring $g $steelTop $c ($H * 0.23) (2.6 * $SS) (1 * $SS)

# Skids, oxygen bottles on the flank, and the lamp on the front.
Stroke $g $steelDk ($c - $W * 0.30) ($H * 0.845) ($c + $W * 0.30) ($H * 0.845) (3 * $SS)
Stroke $g (Fade $steelHi 150) ($c - $W * 0.30) ($H * 0.838) ($c + $W * 0.30) ($H * 0.838) (1 * $SS)
foreach ($side in @(-1, 1)) {
    Stroke $g $steelLo ($c + $side * $W * 0.20) ($H * 0.79) ($c + $side * $W * 0.25) ($H * 0.845) (2.4 * $SS)
}
foreach ($i in 0..2) {
    $bx = $c - $W * 0.36 + $i * 5 * $SS
    Panel $g $bx ($H * 0.62) (4 * $SS) (13 * $SS) $steelLo $steelHi $steelDk (1 * $SS)
    Disc $g $brass ($bx + 2 * $SS) ($H * 0.62) (1.4 * $SS)
}
Lamp $g $c ($H * 0.71) (4.5 * $SS) $lit
Finish $bmp $g $outW $outH "reel_bell.png"

# =====================================================================
#  WALL  -  rock face, tiling top to bottom, for everything that falls
#  or climbs. Only the vertical seam has to meet, so each mark goes down
#  three times rather than nine.
# =====================================================================
$outW = 420; $outH = 560
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$rand = New-Object System.Random -ArgumentList 9931

Box $g $rockLo 0 0 $W $H

# Sedimentary banding first, so the ledges have something to sit against.
for ($i = 0; $i -lt 11; $i++) {
    $by = $rand.NextDouble() * $H
    $tall = (8 + $rand.Next(0, 22)) * $SS
    foreach ($dy in @(-$H, 0, $H)) {
        Box $g (C 58 68 74 30) 0 ($by + $dy) $W $tall
    }
}

# Ledges. A shelf with a lit top and a black underside is what says "going
# past at speed"; a gradient says nothing at all.
for ($i = 0; $i -lt 26; $i++) {
    $lx = $rand.Next(-120, $W)
    $ly = $rand.Next(0, $H)
    $span = (180 + $rand.Next(0, 420))
    $tall = (50 + $rand.Next(0, 130))

    foreach ($dy in @(-$H, 0, $H)) {
        Blob $g @( (P ($lx - $span * 0.5) ($ly + $tall * 0.7 + $dy)),
                   (P ($lx - $span * 0.3) ($ly - $tall * 0.5 + $dy)),
                   (P ($lx + $span * 0.1) ($ly - $tall * 0.7 + $dy)),
                   (P ($lx + $span * 0.5) ($ly + $tall * 0.2 + $dy)),
                   (P ($lx + $span * 0.2) ($ly + $tall * 0.9 + $dy)) ) $rock $null 0 0.25
        Stroke $g $rockHi ($lx - $span * 0.30) ($ly - $tall * 0.46 + $dy) `
            ($lx + $span * 0.10) ($ly - $tall * 0.64 + $dy) (2.6 * $SS)
        Stroke $g $rockHi ($lx + $span * 0.10) ($ly - $tall * 0.64 + $dy) `
            ($lx + $span * 0.46) ($ly + $tall * 0.16 + $dy) (2.2 * $SS)
        Stroke $g (C 0 0 0 170) ($lx - $span * 0.46) ($ly + $tall * 0.72 + $dy) `
            ($lx + $span * 0.22) ($ly + $tall * 0.92 + $dy) (4 * $SS)
    }
}

# Cracks, in four segments each so they are not perfectly straight, with a
# light companion stroke alongside.
for ($i = 0; $i -lt 20; $i++) {
    $cx = $rand.Next(0, $W)
    $cy = $rand.Next(0, $H)
    foreach ($dy in @(-$H, 0, $H)) {
        $px = $cx; $py = $cy + $dy
        for ($s = 0; $s -lt 4; $s++) {
            $nx = $px + ($rand.NextDouble() - 0.5) * 30 * $SS
            $ny = $py + (12 + $rand.NextDouble() * 22) * $SS
            Stroke $g (C 0 0 0 150) $px $py $nx $ny ((1.4 - 0.25 * $s) * $SS)
            Stroke $g (C 90 102 108 60) ($px - 1.4 * $SS) ($py - 1.4 * $SS) `
                ($nx - 1.4 * $SS) ($ny - 1.4 * $SS) (0.6 * $SS)
            $px = $nx; $py = $ny
        }
    }
}

# Growth on the upward faces, and grain everywhere. The sheet skips the relief
# pass - noise does not tile - so its grain has to be painted.
for ($i = 0; $i -lt 30; $i++) {
    $gx = $rand.Next(0, $W); $gy = $rand.Next(0, $H)
    foreach ($dy in @(-$H, 0, $H)) {
        Disc $g (Fade $growth 70) $gx ($gy + $dy) ((2 + $rand.NextDouble() * 4) * $SS)
    }
}
for ($i = 0; $i -lt 700; $i++) {
    $gx = $rand.Next(0, $W); $gy = $rand.Next(0, $H)
    if (($i % 2) -eq 0) { $col = C 92 104 110 26 } else { $col = C 8 10 14 34 }
    foreach ($dy in @(-$H, 0, $H)) {
        Disc $g $col $gx ($gy + $dy) ((0.3 + $rand.NextDouble() * 0.8) * $SS)
    }
}

# A dark wash down both sides so the plate can be laid next to itself across
# the screen without a hard vertical edge showing.
for ($i = 0; $i -lt 26; $i++) {
    $a = [int](2 + $i * 0.5)
    Box $g (C 0 0 0 $a) ($i * 4) 0 4 $H
    Box $g (C 0 0 0 $a) ($W - ($i + 1) * 4) 0 4 $H
}
Finish $bmp $g $outW $outH "reel_wall.png"

# =====================================================================
#  STATION  -  Nerith itself, in profile: modules on lattice legs, a
#  mast, and one window still lit. Everything about it has to say
#  "intact, and nobody is in it". Drawn large because one shot goes
#  close on its hatches and a stretched plate is a blurred plate.
# =====================================================================
$outW = 1000; $outH = 464
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$floor = $H * 0.86
$rand = New-Object System.Random -ArgumentList 5519

# Legs and pads. The pads used to be centred on the wrong x and every leg
# stood on the right hand end of its own foot.
foreach ($x in @(0.16, 0.32, 0.62, 0.82)) {
    $footX = $W * ($x + 0.02)
    Truss $g ($W * $x) ($H * 0.58) $footX $floor (7 * $SS) 4 $steel $steelLo $steelDk (1.8 * $SS)
    Panel $g ($footX - 8 * $SS) ($floor - 2 * $SS) (16 * $SS) (4.5 * $SS) $steelLo $steelHi $steelDk (1.2 * $SS)
}

# The big module. Countershaded, specked, then ribbed and riveted.
$hull = @( (P ($W * 0.10) ($H * 0.50)), (P ($W * 0.18) ($H * 0.30)),
           (P ($W * 0.46) ($H * 0.28)), (P ($W * 0.54) ($H * 0.50)),
           (P ($W * 0.46) ($H * 0.62)), (P ($W * 0.18) ($H * 0.62)) )
Blob $g $hull $steel $ink (2 * $SS) 0.2
Shade $g $hull 0.2 (C 150 162 172 70) (C 10 14 18 105) 0.30 0.34
Speck $g $hull 0.2 260 771 (C 132 146 156 26) (C 8 10 14 40) (0.9 * $SS)

for ($i = 0; $i -lt 9; $i++) {
    $rx = $W * (0.155 + 0.038 * $i)
    Stroke $g $steelDk $rx ($H * 0.305) $rx ($H * 0.615) (1.8 * $SS)
    Stroke $g (Fade $steelTop 130) ($rx - 1.4 * $SS) ($H * 0.305) ($rx - 1.4 * $SS) ($H * 0.615) (0.7 * $SS)
}
foreach ($row in @(0.33, 0.58)) {
    Rivets $g ($W * 0.16) ($H * $row) ($W * 0.50) ($H * $row) 22 (1 * $SS) $steelDk $steelHi
}

# Windows: a frame, a dark pane, screws. One is lit and that is the whole shot.
foreach ($i in 0..4) {
    $wx = $W * (0.19 + 0.06 * $i)
    $wy = $H * 0.44
    Disc $g $steelDk $wx $wy (7 * $SS)
    Ring $g $steelTop $wx $wy (6.4 * $SS) (1.4 * $SS)
    if ($i -eq 3) {
        Lamp $g $wx $wy (4 * $SS) $lit
    } else {
        Disc $g (C 12 16 20) $wx $wy (4.4 * $SS)
        Disc $g (C 40 48 54 90) ($wx - 1.4 * $SS) ($wy - 1.4 * $SS) (2 * $SS)
    }
    for ($s = 0; $s -lt 6; $s++) {
        $sa = [math]::PI * 2 * $s / 6.0
        Disc $g $steelHi ($wx + [math]::Cos($sa) * 6.4 * $SS) ($wy + [math]::Sin($sa) * 6.4 * $SS) (0.7 * $SS)
    }
}

# The second module and the gantry between them.
$pod = @( (P ($W * 0.62) ($H * 0.52)), (P ($W * 0.66) ($H * 0.36)),
          (P ($W * 0.84) ($H * 0.34)), (P ($W * 0.88) ($H * 0.52)),
          (P ($W * 0.84) ($H * 0.62)), (P ($W * 0.66) ($H * 0.62)) )
Blob $g $pod $steelLo $ink (2 * $SS) 0.2
Shade $g $pod 0.2 (C 130 142 152 60) (C 8 12 16 110) 0.28 0.32
Speck $g $pod 0.2 140 913 (C 122 132 138 24) (C 8 10 14 36) (0.9 * $SS)
Truss $g ($W * 0.54) ($H * 0.47) ($W * 0.62) ($H * 0.47) (6 * $SS) 4 $steelHi $steel $steelDk (1.4 * $SS)
Box $g $steelLo ($W * 0.54) ($H * 0.485) ($W * 0.08) (2 * $SS)
Disc $g $steelDk ($W * 0.75) ($H * 0.47) (9 * $SS)
Ring $g $steelTop ($W * 0.75) ($H * 0.47) (8.4 * $SS) (1.6 * $SS)
Disc $g (C 10 14 18) ($W * 0.75) ($H * 0.47) (6 * $SS)

# The mast, with a dead beacon on it.
Truss $g ($W * 0.40) ($H * 0.30) ($W * 0.42) ($H * 0.05) (5 * $SS) 5 $steelHi $steel $steelDk (1.3 * $SS)
Stroke $g $steelLo ($W * 0.405) ($H * 0.20) ($W * 0.335) ($H * 0.115) (1.2 * $SS)
Stroke $g $steelLo ($W * 0.412) ($H * 0.145) ($W * 0.50) ($H * 0.075) (1.2 * $SS)
Ring $g $steelHi ($W * 0.45) ($H * 0.10) (7 * $SS) (1.2 * $SS)
Disc $g $rust ($W * 0.42) ($H * 0.04) (3 * $SS)

# Three hatches, all hanging open. This is the line the whole prologue turns
# on, so it has to be visible in the picture and not only in the caption.
# Not $h. PowerShell variables are case insensitive, so a loop variable called
# $h silently becomes the canvas height and every placement after this point
# lands inside a two element array instead of on the plate. It cost an hour.
foreach ($door in @(@(0.255, 0.62), @(0.42, 0.62), @(0.70, 0.62))) {
    $hx = $W * $door[0]; $hy = $H * $door[1]
    Disc $g (C 6 9 12) $hx ($hy + 1 * $SS) (7 * $SS)
    Ring $g $steelDk $hx ($hy + 1 * $SS) (7.6 * $SS) (1.6 * $SS)
    Panel $g ($hx + 4 * $SS) ($hy + 4 * $SS) (5 * $SS) (14 * $SS) $steelLo $steelHi $steelDk (1.1 * $SS)
    Stroke $g $steelDk ($hx + 4 * $SS) ($hy + 3 * $SS) ($hx + 9 * $SS) ($hy + 4 * $SS) (1.2 * $SS)
}

# Silt on every upward face - the single loudest way to say nobody has been
# here - and rust running down from every seam.
for ($i = 0; $i -lt 34; $i++) {
    $sx = $W * (0.10 + $rand.NextDouble() * 0.78)
    $sy = $H * (0.28 + $rand.NextDouble() * 0.06)
    Disc $g (Fade $silt 60) $sx $sy ((1 + $rand.NextDouble() * 4) * $SS)
}
for ($i = 0; $i -lt 18; $i++) {
    $rx = $W * (0.12 + $rand.NextDouble() * 0.74)
    Stroke $g (Fade $rust 55) $rx ($H * 0.62) $rx ($H * (0.63 + $rand.NextDouble() * 0.10)) (1.4 * $SS)
}
Finish $bmp $g $outW $outH "reel_station.png"

# =====================================================================
#  BEAST  -  the thing that goes past. Never lit, never explained: a
#  silhouette the length of the screen with one eye that catches the
#  lamp, which is worth more than any amount of detail. But it still
#  needs the detail, because a flat fill stretched over 1100 pixels is
#  a flat fill.
# =====================================================================
$outW = 1120; $outH = 412
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H * 0.52
$rand = New-Object System.Random -ArgumentList 3319

$body = @( (P ($W * 0.98) $cy),
           (P ($W * 0.86) ($cy - $H * 0.26)),
           (P ($W * 0.60) ($cy - $H * 0.30)),
           (P ($W * 0.30) ($cy - $H * 0.20)),
           (P ($W * 0.02) ($cy - $H * 0.06)),
           (P ($W * 0.04) ($cy + $H * 0.05)),
           (P ($W * 0.30) ($cy + $H * 0.22)),
           (P ($W * 0.62) ($cy + $H * 0.31)),
           (P ($W * 0.86) ($cy + $H * 0.25)) )

# Fins first so the body covers their roots and there is no hard seam where
# they meet it - which is exactly what the old version had.
foreach ($fin in @(
    ,@( (P ($W * 0.56) ($cy + $H * 0.24)), (P ($W * 0.44) ($cy + $H * 0.46)),
        (P ($W * 0.30) ($cy + $H * 0.44)), (P ($W * 0.36) ($cy + $H * 0.20)) )
    ,@( (P ($W * 0.84) ($cy + $H * 0.20)), (P ($W * 0.78) ($cy + $H * 0.42)),
        (P ($W * 0.66) ($cy + $H * 0.38)), (P ($W * 0.70) ($cy + $H * 0.18)) )
    ,@( (P ($W * 0.22) ($cy - $H * 0.16)), (P ($W * 0.12) ($cy - $H * 0.38)),
        (P ($W * 0.04) ($cy - $H * 0.30)), (P ($W * 0.10) ($cy - $H * 0.12)) )
)) {
    Blob $g $fin $meatLo $ink (1.6 * $SS) 0.4
    foreach ($ray in 0..4) {
        $t = $ray / 4.0
        Stroke $g (Fade $meatPale 130) $fin[0].X $fin[0].Y `
            ($fin[1].X + ($fin[2].X - $fin[1].X) * $t) ($fin[1].Y + ($fin[2].Y - $fin[1].Y) * $t) (1.1 * $SS)
    }
}

Blob $g $body $meat $ink (2.2 * $SS) 0.4
Shade $g $body 0.4 (C 118 108 132 60) (C 14 12 20 120) 0.34 0.36
Speck $g $body 0.4 420 4477 (C 118 108 132 34) (C 18 15 24 46) (1.1 * $SS)

# Mottling in two tones, then a lateral line with pores along it, then scars.
Speck $g $body 0.4 90 991 (C 26 22 34 150) (C 26 22 34 110) (5 * $SS)
Speck $g $body 0.4 60 1223 (C 118 108 132 80) (C 118 108 132 55) (3.4 * $SS)

Stroke $g (C 132 122 146 110) ($W * 0.10) ($cy + $H * 0.02) ($W * 0.50) ($cy - $H * 0.02) (1.2 * $SS)
Stroke $g (C 132 122 146 110) ($W * 0.50) ($cy - $H * 0.02) ($W * 0.86) ($cy - $H * 0.05) (1.2 * $SS)
for ($i = 0; $i -lt 22; $i++) {
    $px = $W * (0.10 + 0.036 * $i)
    Disc $g (C 20 17 26 140) $px ($cy + $H * (0.02 - 0.0016 * $i)) (1 * $SS)
}
for ($i = 0; $i -lt 9; $i++) {
    $sx = $W * (0.18 + $rand.NextDouble() * 0.60)
    $sy = $cy + ($rand.NextDouble() - 0.5) * $H * 0.34
    Stroke $g (C 150 140 160 80) $sx $sy ($sx + (10 + $rand.Next(0, 40)) * $SS) ($sy + (6 - $rand.Next(0, 14)) * $SS) (1 * $SS)
}

# Dorsal spines, each with a lit front and a dark back and a shadow behind it.
for ($i = 0; $i -lt 16; $i++) {
    $t = 0.10 + 0.055 * $i
    $sx = $W * $t
    $lift = $H * (0.30 - 0.20 * [math]::Abs($t - 0.6))
    Blob $g @( (P ($sx - 6 * $SS) ($cy - $lift * 0.72)),
               (P ($sx + 1 * $SS) ($cy - $lift - 9 * $SS)),
               (P ($sx + 7 * $SS) ($cy - $lift * 0.72)) ) (C 20 17 26 120) $null 0 0.2
    Blob $g @( (P ($sx - 7 * $SS) ($cy - $lift * 0.72)),
               (P $sx ($cy - $lift - 8 * $SS)),
               (P ($sx + 6 * $SS) ($cy - $lift * 0.72)) ) $meatHi $ink (1 * $SS) 0.2
    Stroke $g (Fade $meatPale 150) ($sx - 2 * $SS) ($cy - $lift * 0.74) $sx ($cy - $lift - 6 * $SS) (0.8 * $SS)
}

# Bioluminescent points along the belly. Eleven small lamps is the one thing
# that gives an 1100 pixel silhouette any depth at all.
for ($i = 0; $i -lt 11; $i++) {
    $bx = $W * (0.14 + 0.07 * $i)
    $by = $cy + $H * (0.16 + 0.06 * [math]::Sin($i * 1.7))
    Lamp $g $bx $by (1.6 * $SS) (C 96 180 196)
}

# The head grows out of the body rather than being stuck on it, which is what
# the old hard rectangular seam was.
Blob $g @( (P ($W * 0.995) ($cy - $H * 0.02)), (P ($W * 0.92) ($cy - $H * 0.23)),
           (P ($W * 0.79) ($cy - $H * 0.21)), (P ($W * 0.785) ($cy + $H * 0.21)),
           (P ($W * 0.92) ($cy + $H * 0.23)) ) $meatHi $ink (2 * $SS) 0.35
Speck $g @( (P ($W * 0.995) ($cy - $H * 0.02)), (P ($W * 0.92) ($cy - $H * 0.23)),
            (P ($W * 0.79) ($cy - $H * 0.21)), (P ($W * 0.785) ($cy + $H * 0.21)),
            (P ($W * 0.92) ($cy + $H * 0.23)) ) 0.35 120 707 (C 140 130 156 40) (C 20 17 26 50) (1.2 * $SS)

Stroke $g $ink ($W * 0.79) ($cy + $H * 0.09) ($W * 0.985) ($cy + $H * 0.02) (2.4 * $SS)
for ($i = 0; $i -lt 14; $i++) {
    $tx = $W * (0.80 + 0.0135 * $i)
    $ty = $cy + $H * (0.085 - 0.005 * $i)
    Blob $g @( (P ($tx - 1.6 * $SS) $ty), (P $tx ($ty - (3 + ($i % 3) * 2) * $SS)),
               (P ($tx + 1.6 * $SS) $ty) ) (C 196 190 176) $null 0 0.1
}
Gills $g ($W * 0.80) ($cy - $H * 0.02) 4 (6 * $SS) (16 * $SS) (C 18 14 24 200)
Eye $g ($W * 0.895) ($cy - $H * 0.06) (7 * $SS) $lit
Lamp $g ($W * 0.895) ($cy - $H * 0.06) (3 * $SS) $lit
Finish $bmp $g $outW $outH "reel_beast.png"

# =====================================================================
#  DIVER  -  a man in a hard suit, in profile, drifting. Used for what
#  is left of the ones who came before. It used to be a front view with
#  a profile helmet on it, which is a gingerbread man with a face on the
#  side of its head.
# =====================================================================
$outW = 210; $outH = 308
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

# Far leg and far arm first, smaller and darker, so the figure has a near side
# and a far side. That is the whole of the profile.
Stroke $g (C 96 84 40) ($W * 0.50) ($H * 0.62) ($W * 0.40) ($H * 0.88) (9 * $SS)
Stroke $g (C 32 30 34) ($W * 0.40) ($H * 0.88) ($W * 0.30) ($H * 0.93) (7 * $SS)
Stroke $g (C 96 84 40) ($W * 0.58) ($H * 0.44) ($W * 0.74) ($H * 0.58) (8 * $SS)
Stroke $g (C 32 30 34) ($W * 0.74) ($H * 0.58) ($W * 0.82) ($H * 0.63) (6.5 * $SS)

# The tank and its hose.
Panel $g ($W * 0.20) ($H * 0.40) ($W * 0.13) ($H * 0.24) $rubber (C 72 68 74) (C 20 18 22) (1.6 * $SS)
foreach ($band in @(0.46, 0.58)) {
    Stroke $g (C 72 68 74) ($W * 0.20) ($H * $band) ($W * 0.33) ($H * $band) (1.6 * $SS)
}
Disc $g $brass ($W * 0.265) ($H * 0.385) (2.4 * $SS)
Stroke $g $rubber ($W * 0.27) ($H * 0.38) ($W * 0.40) ($H * 0.30) (2.4 * $SS)

# The torso, countershaded.
$torso = @( (P ($W * 0.64) ($H * 0.40)), (P ($W * 0.62) ($H * 0.66)),
            (P ($W * 0.34) ($H * 0.66)), (P ($W * 0.32) ($H * 0.40)),
            (P ($W * 0.48) ($H * 0.34)) )
Blob $g $torso $suit $ink (2 * $SS) 0.3
Shade $g $torso 0.3 (C 176 158 88 80) (C 30 26 14 110) 0.30 0.34
foreach ($fold in @(0.46, 0.52, 0.58)) {
    Stroke $g (Fade $suitLo 190) ($W * 0.34) ($H * $fold) ($W * 0.62) ($H * $fold) (1.4 * $SS)
}
Rivets $g ($W * 0.35) ($H * 0.415) ($W * 0.61) ($H * 0.415) 8 (0.9 * $SS) (C 60 52 24) $suitHi

# Near arm and near leg, over the torso.
Stroke $g $suit ($W * 0.38) ($H * 0.44) ($W * 0.18) ($H * 0.54) (9.5 * $SS)
Stroke $g $rubber ($W * 0.18) ($H * 0.54) ($W * 0.09) ($H * 0.58) (7.5 * $SS)
foreach ($finger in @(-0.02, 0.0, 0.02)) {
    Stroke $g $ink ($W * 0.10) ($H * (0.575 + $finger)) ($W * 0.065) ($H * (0.585 + $finger)) (0.8 * $SS)
}
Stroke $g $suit ($W * 0.54) ($H * 0.62) ($W * 0.60) ($H * 0.90) (10 * $SS)
Stroke $g $rubber ($W * 0.60) ($H * 0.90) ($W * 0.70) ($H * 0.95) (8 * $SS)
Panel $g ($W * 0.62) ($H * 0.93) ($W * 0.10) ($H * 0.035) (C 120 124 128) (C 160 166 170) (C 40 42 46) (1 * $SS)

# The collar and the helmet, sunk into the shoulders.
Box $g $brass ($W * 0.40) ($H * 0.325) ($W * 0.24) ($H * 0.035)
Rivets $g ($W * 0.41) ($H * 0.342) ($W * 0.63) ($H * 0.342) 8 (1 * $SS) (C 118 92 54) (C 222 190 130)
Blob $g @( (P ($W * 0.66) ($H * 0.24)), (P ($W * 0.62) ($H * 0.12)),
           (P ($W * 0.42) ($H * 0.12)), (P ($W * 0.36) ($H * 0.24)),
           (P ($W * 0.42) ($H * 0.33)), (P ($W * 0.62) ($H * 0.33)) ) $suit $ink (2 * $SS) 0.3
Shade $g @( (P ($W * 0.66) ($H * 0.24)), (P ($W * 0.62) ($H * 0.12)),
            (P ($W * 0.42) ($H * 0.12)), (P ($W * 0.36) ($H * 0.24)),
            (P ($W * 0.42) ($H * 0.33)), (P ($W * 0.62) ($H * 0.33)) ) 0.3 `
      (C 176 158 88 90) (C 30 26 14 100) 0.32 0.30
Stroke $g (Fade $suitLo 200) ($W * 0.44) ($H * 0.16) ($W * 0.60) ($H * 0.16) (1.4 * $SS)

# One port, in the front of it, with a brass frame and a reflection.
Disc $g $brass ($W * 0.615) ($H * 0.215) (7.5 * $SS)
Disc $g (C 18 40 48) ($W * 0.615) ($H * 0.215) (6 * $SS)
Blob $g @( (P ($W * 0.585) ($H * 0.19)), (P ($W * 0.645) ($H * 0.19)),
           (P ($W * 0.635) ($H * 0.21)), (P ($W * 0.585) ($H * 0.21)) ) (C 200 240 246 90) $null 0 0.2
Disc $g (C 255 255 255 150) ($W * 0.60) ($H * 0.198) (1.2 * $SS)
Lamp $g ($W * 0.70) ($H * 0.20) (2.6 * $SS) (C 250 230 190)

# Growth and rust. He has been down here a while.
$rand = New-Object System.Random -ArgumentList 2087
for ($i = 0; $i -lt 14; $i++) {
    Disc $g (Fade $growth 70) ($W * (0.32 + $rand.NextDouble() * 0.36)) ($H * (0.35 + $rand.NextDouble() * 0.5)) ((1 + $rand.NextDouble() * 2.4) * $SS)
}
for ($i = 0; $i -lt 10; $i++) {
    Disc $g (Fade $rust 60) ($W * (0.34 + $rand.NextDouble() * 0.32)) ($H * (0.36 + $rand.NextDouble() * 0.44)) ((0.8 + $rand.NextDouble() * 2) * $SS)
}
Finish $bmp $g $outW $outH "reel_diver.png"

# =====================================================================
#  CRATE  -  in profile. The ending shot used to paste in the top down
#  deck crate, which reads as a box with a window in the middle of it.
# =====================================================================
$outW = 96; $outH = 78
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Panel $g ($W * 0.04) ($H * 0.14) ($W * 0.92) ($H * 0.80) $steel $steelTop $steelDk (2 * $SS)
Edge $g $ink ($W * 0.04) ($H * 0.14) ($W * 0.92) ($H * 0.80) (1.6 * $SS)

# Five horizontal boards, each with its own lit edge.
foreach ($i in 0..4) {
    $by = $H * (0.22 + 0.15 * $i)
    Stroke $g $steelDk ($W * 0.05) $by ($W * 0.95) $by (1.4 * $SS)
    Stroke $g (Fade $steelTop 120) ($W * 0.05) ($by + 1.2 * $SS) ($W * 0.95) ($by + 1.2 * $SS) (0.7 * $SS)
}

# The lid, overhanging on both sides, which is the one line that says profile.
Panel $g ($W * 0.01) ($H * 0.06) ($W * 0.98) ($H * 0.10) $steelHi $steelTop $steelDk (1.6 * $SS)
Edge $g $ink ($W * 0.01) ($H * 0.06) ($W * 0.98) ($H * 0.10) (1.4 * $SS)

# Corner castings, a strap, a lock, and a stencil.
foreach ($x in @(0.04, 0.82)) {
    Panel $g ($W * $x) ($H * 0.16) ($W * 0.14) ($H * 0.16) $steelLo $steelHi $steelDk (1.4 * $SS)
    Rivets $g ($W * ($x + 0.02)) ($H * 0.24) ($W * ($x + 0.12)) ($H * 0.24) 3 (1.1 * $SS) $steelDk $steelTop
    Panel $g ($W * $x) ($H * 0.76) ($W * 0.14) ($H * 0.16) $steelLo $steelHi $steelDk (1.4 * $SS)
}
Box $g $rustLo ($W * 0.42) ($H * 0.14) ($W * 0.12) ($H * 0.80)
Box $g $rust ($W * 0.435) ($H * 0.14) ($W * 0.09) ($H * 0.80)
Stroke $g (Fade $rustHi 140) ($W * 0.45) ($H * 0.14) ($W * 0.45) ($H * 0.94) (0.8 * $SS)
Disc $g $steelDk ($W * 0.48) ($H * 0.54) (4 * $SS)
Ring $g $steelTop ($W * 0.48) ($H * 0.54) (3.2 * $SS) (1.1 * $SS)
foreach ($i in 0..2) {
    Stroke $g (Fade $paintY 110) ($W * (0.16 + 0.08 * $i)) ($H * 0.46) ($W * (0.22 + 0.08 * $i)) ($H * 0.46) (1.4 * $SS)
}
Finish $bmp $g $outW $outH "reel_crate.png"

# =====================================================================
#  LEDGE  -  the foreground every deep shot was missing.
#
#  There are no shaders in this project, so a foreground cannot be blurred
#  and therefore must never be a detailed object: it is a large near-black
#  silhouette that slides past at twice the speed of everything else, and
#  that parallax is the only depth cue a flat scene has. It is drawn dark
#  because the reel tints it down to about {6,9,11} anyway, but with real
#  internal edges so it is not a blob the moment it crosses a lamp.
# =====================================================================
$outW = 640; $outH = 240
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$rand = New-Object System.Random -ArgumentList 7717

$mass = @( (P 0 ($H * 1.10)), (P ($W * 0.06) ($H * 0.52)),
           (P ($W * 0.22) ($H * 0.30)), (P ($W * 0.41) ($H * 0.40)),
           (P ($W * 0.58) ($H * 0.18)), (P ($W * 0.76) ($H * 0.34)),
           (P ($W * 0.92) ($H * 0.22)), (P $W ($H * 0.46)),
           (P $W ($H * 1.10)) )
Blob $g $mass $rock $ink (2.5 * $SS) 0.28

# The lit crest. Even a silhouette needs one, or it reads as a hole cut in
# the picture rather than as rock between the camera and the light.
for ($i = 0; $i -lt ($mass.Count - 2); $i++) {
    Stroke $g (Fade $rockHi 90) $mass[$i].X $mass[$i].Y $mass[$i+1].X $mass[$i+1].Y (2 * $SS)
}

# Fractures and blocks, so the mass has an inside.
for ($i = 0; $i -lt 22; $i++) {
    $fx = $rand.Next(0, $W)
    $fy = $H * (0.35 + $rand.NextDouble() * 0.6)
    $lean = ($rand.NextDouble() - 0.5) * 40 * $SS
    Stroke $g (C 0 0 0 170) $fx $fy ($fx + $lean) ($fy + 30 * $SS) (1.6 * $SS)
    Stroke $g (Fade $rockHi 50) ($fx - 1.6 * $SS) $fy ($fx - 1.6 * $SS + $lean) ($fy + 30 * $SS) (0.7 * $SS)
}
for ($i = 0; $i -lt 16; $i++) {
    $bx = $rand.Next(0, $W)
    $by = $H * (0.30 + $rand.NextDouble() * 0.5)
    Blob $g @( (P ($bx - 18 * $SS) ($by + 10 * $SS)), (P ($bx - 8 * $SS) ($by - 12 * $SS)),
               (P ($bx + 16 * $SS) ($by - 6 * $SS)), (P ($bx + 10 * $SS) ($by + 14 * $SS)) ) `
        (C 46 54 58 190) $null 0 0.2
    Stroke $g (Fade $rockHi 70) ($bx - 8 * $SS) ($by - 12 * $SS) ($bx + 16 * $SS) ($by - 6 * $SS) (1.2 * $SS)
}
for ($i = 0; $i -lt 26; $i++) {
    Disc $g (Fade $growth 60) ($rand.Next(0, $W)) ($H * (0.28 + $rand.NextDouble() * 0.25)) ((1.5 + $rand.NextDouble() * 4) * $SS)
}
Finish $bmp $g $outW $outH "reel_ledge.png"

# =====================================================================
#  WRECK  -  one of the three boats that stopped at the same depth.
#
#  It is the strongest thing the prologue has to say and it was being said
#  in a caption with nothing underneath it. Three hulls at one depth, all
#  their lights out, all their beacons still going, and every one of them
#  still on the cable that lowered it.
# =====================================================================
$outW = 340; $outH = 160
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H * 0.56
$rand = New-Object System.Random -ArgumentList 4241

$hull = @( (P ($W * 0.97) $cy), (P ($W * 0.84) ($cy - $H * 0.20)),
           (P ($W * 0.40) ($cy - $H * 0.24)), (P ($W * 0.08) ($cy - $H * 0.16)),
           (P ($W * 0.04) $cy), (P ($W * 0.08) ($cy + $H * 0.16)),
           (P ($W * 0.40) ($cy + $H * 0.24)), (P ($W * 0.84) ($cy + $H * 0.20)) )
Blob $g $hull $steel $ink (2.2 * $SS) 0.35
Shade $g $hull 0.35 (C 150 162 172 60) (C 8 12 16 120) 0.30 0.34
Speck $g $hull 0.35 180 331 (C 132 146 156 26) (C 8 10 14 42) (1 * $SS)

for ($i = 0; $i -lt 9; $i++) {
    $fx = $W * (0.14 + 0.084 * $i)
    Stroke $g $steelDk $fx ($cy - $H * 0.21) $fx ($cy + $H * 0.21) (1.4 * $SS)
    Stroke $g (Fade $steelTop 110) ($fx - 1.2 * $SS) ($cy - $H * 0.21) ($fx - 1.2 * $SS) ($cy + $H * 0.21) (0.6 * $SS)
}
Rivets $g ($W * 0.12) ($cy + $H * 0.15) ($W * 0.86) ($cy + $H * 0.15) 18 (0.9 * $SS) $steelDk $steelHi

Panel $g ($W * 0.40) ($cy - $H * 0.46) ($W * 0.18) ($H * 0.26) $steelLo $steelHi $steelDk (1.6 * $SS)
Edge $g $ink ($W * 0.40) ($cy - $H * 0.46) ($W * 0.18) ($H * 0.26) (1.2 * $SS)
Stroke $g $steelDk ($W * 0.49) ($cy - $H * 0.46) ($W * 0.49) ($cy - $H * 0.66) (1.6 * $SS)
Blob $g @( (P ($W * 0.30) ($cy + $H * 0.04)), (P ($W * 0.16) ($cy + $H * 0.30)),
           (P ($W * 0.08) ($cy + $H * 0.26)), (P ($W * 0.22) ($cy + $H * 0.02)) ) $steelLo $ink (1.4 * $SS) 0.3
Ring $g $steelLo ($W * 0.955) $cy ($H * 0.17) (3 * $SS)

# Five ports, every one of them dark. Nothing on this hull is allowed to be
# lit except the beacon: the lights being out is the shot.
foreach ($i in 0..4) {
    $px = $W * (0.22 + 0.11 * $i)
    Disc $g $steelDk $px ($cy - $H * 0.02) (4.2 * $SS)
    Ring $g (Fade $steelTop 150) $px ($cy - $H * 0.02) (3.8 * $SS) (1 * $SS)
    Disc $g (C 8 11 14) $px ($cy - $H * 0.02) (2.6 * $SS)
}

# The one thing still working. It has been flashing for eleven days into
# water with nobody left in it to see it.
Lamp $g ($W * 0.49) ($cy - $H * 0.40) (3.4 * $SS) (C 236 150 96)

for ($i = 0; $i -lt 12; $i++) {
    $sx = $W * (0.10 + $rand.NextDouble() * 0.78)
    Disc $g (Fade $silt 70) $sx ($cy - $H * (0.17 + $rand.NextDouble() * 0.05)) ((1 + $rand.NextDouble() * 3) * $SS)
}
for ($i = 0; $i -lt 9; $i++) {
    $sx = $W * (0.12 + $rand.NextDouble() * 0.72)
    $sy = $cy + ($rand.NextDouble() - 0.5) * $H * 0.3
    Stroke $g (C 150 140 150 70) $sx $sy ($sx + (6 + $rand.Next(0, 26)) * $SS) ($sy + (4 - $rand.Next(0, 9)) * $SS) (0.8 * $SS)
}
for ($i = 0; $i -lt 10; $i++) {
    Disc $g (Fade $growth 60) ($W * (0.08 + $rand.NextDouble() * 0.84)) ($cy + ($rand.NextDouble() - 0.5) * $H * 0.36) ((1 + $rand.NextDouble() * 2.6) * $SS)
}
Finish $bmp $g $outW $outH "reel_wreck.png"

# =====================================================================
#  HATCH  -  a close flank of Nerith with five doors standing open.
#
#  The prologue's best line is that every hatch is open from the inside,
#  and it was being carried by a caption over a wide shot in which not one
#  hatch was legible. This is that line as a picture: doors on the outside
#  of the hull, every one undogged, every wheel spun right back, and
#  nothing but black behind any of them.
# =====================================================================
$outW = 700; $outH = 400
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS
$rand = New-Object System.Random -ArgumentList 8863

$flank = @( (P 0 ($H * 0.14)), (P ($W * 0.5) ($H * 0.06)), (P $W ($H * 0.14)),
            (P $W ($H * 0.92)), (P ($W * 0.5) ($H * 1.00)), (P 0 ($H * 0.92)) )
Blob $g $flank $steel $ink (2.5 * $SS) 0.1
Shade $g $flank 0.1 (C 150 162 172 60) (C 8 12 16 110) 0.22 0.30
Speck $g $flank 0.1 420 553 (C 132 146 156 24) (C 8 10 14 40) (1.2 * $SS)

for ($i = 0; $i -lt 9; $i++) {
    $sy = $H * (0.14 + 0.093 * $i)
    Stroke $g $steelDk 0 $sy $W $sy (2 * $SS)
    Stroke $g (Fade $steelTop 120) 0 ($sy + 1.6 * $SS) $W ($sy + 1.6 * $SS) (0.8 * $SS)
    Rivets $g (10 * $SS) ($sy - 3 * $SS) ($W - 10 * $SS) ($sy - 3 * $SS) 30 (1 * $SS) $steelDk $steelHi
}
for ($i = 0; $i -lt 5; $i++) {
    $vx = $W * (0.09 + 0.205 * $i)
    Stroke $g $steelDk $vx ($H * 0.10) $vx ($H * 0.96) (2.2 * $SS)
    Stroke $g (Fade $steelTop 100) ($vx - 1.8 * $SS) ($H * 0.10) ($vx - 1.8 * $SS) ($H * 0.96) (0.8 * $SS)
}

foreach ($i in 0..4) {
    $dx = $W * (0.145 + 0.205 * $i)
    $dy = $H * (0.40 + 0.10 * [math]::Sin($i * 1.9))
    $rad = 26 * $SS

    Disc $g (C 4 7 9) $dx $dy $rad
    Ring $g $steelDk $dx $dy ($rad + 2.4 * $SS) (3 * $SS)
    Ring $g (Fade $steelTop 140) $dx $dy ($rad + 4.5 * $SS) (1.2 * $SS)
    Stroke $g (C 22 28 32 200) ($dx - $rad * 0.5) ($dy - $rad * 0.3) ($dx + $rad * 0.5) ($dy - $rad * 0.3) (1.6 * $SS)

    # The doors swing to alternating sides, so five of them read as five
    # separate decisions somebody made rather than as a pattern in the hull.
    if (($i % 2) -eq 0) { $side = 1 } else { $side = -1 }
    $leafX = $dx + $side * ($rad * 1.5)
    $leafY = $dy + $rad * 0.35

    Blob $g @( (P ($leafX - $side * $rad * 0.55) ($leafY - $rad * 0.85)),
               (P ($leafX + $side * $rad * 0.45) ($leafY - $rad * 0.60)),
               (P ($leafX + $side * $rad * 0.50) ($leafY + $rad * 0.80)),
               (P ($leafX - $side * $rad * 0.50) ($leafY + $rad * 0.95)) ) $steelLo $ink (2 * $SS) 0.12
    Rivets $g ($leafX - $side * $rad * 0.40) ($leafY - $rad * 0.6) ($leafX - $side * $rad * 0.40) ($leafY + $rad * 0.7) 4 (1 * $SS) $steelDk $steelHi

    # The wheel, spun right back. Nobody forces a door from outside by
    # turning the handle that is on the outside of it.
    Ring $g $steelHi $leafX $leafY ($rad * 0.42) (2.2 * $SS)
    foreach ($spoke in 0..3) {
        $a = [math]::PI * $spoke / 4.0 + 0.4
        Stroke $g $steelHi ($leafX - [math]::Cos($a) * $rad * 0.42) ($leafY - [math]::Sin($a) * $rad * 0.42) `
            ($leafX + [math]::Cos($a) * $rad * 0.42) ($leafY + [math]::Sin($a) * $rad * 0.42) (1.6 * $SS)
    }
    Disc $g $steelDk $leafX $leafY (3 * $SS)

    Stroke $g $steelDk ($dx + $side * $rad) ($dy - $rad * 0.4) $leafX ($leafY - $rad * 0.7) (2.4 * $SS)
    Stroke $g $steelDk ($dx + $side * $rad) ($dy + $rad * 0.4) $leafX ($leafY + $rad * 0.7) (2.4 * $SS)
}

for ($i = 0; $i -lt 40; $i++) {
    $rx = $rand.Next(0, $W)
    $ry = $H * (0.14 + $rand.NextDouble() * 0.7)
    Stroke $g (Fade $rust 55) $rx $ry $rx ($ry + (8 + $rand.Next(0, 40)) * $SS) (1.4 * $SS)
}
for ($i = 0; $i -lt 60; $i++) {
    Disc $g (Fade $silt 55) ($rand.Next(0, $W)) ($H * (0.14 + 0.093 * $rand.Next(0, 9)) - 2 * $SS) ((1 + $rand.NextDouble() * 3) * $SS)
}
Finish $bmp $g $outW $outH "reel_hatch.png"

