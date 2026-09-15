Add-Type -AssemblyName System.Drawing
. "$PSScriptRoot\organic.ps1"

# The deck dressing. Fourteen props that turn the plate from a car park into the
# working deck of a dive support vessel: the gas, the lifting gear, the sat
# system, the workshop and the safety kit.
#
# All of it is seen from directly above, which is the one thing that decides
# every shape in here. A man from above is shoulders; a cylinder from above is
# a bright line up its middle with both long edges falling away dark; a rack of
# bottles is sixteen circles and a frame. Anything that only reads from the side
# - a sign, a windsock, a ladder - either got rejected in the spec or is drawn
# as the hole it goes into.
#
# The palette is deliberately the sub's steel rather than the old deck steel.
# The old deck greys topped out at luma 120 and were nearly neutral, so after
# the relief pass in make_3d.ps1 nothing on the deck cleared 96 and the whole
# thing read as one grey slab. These have a top value at 181 and a blue bias of
# about +22 on B minus R, which is what the hull is built from, so the dressing
# and the sub look like they came out of the same yard.
#
# Every structural width and radius below is written as a multiple of $SS. At
# $SS = 4 that means the number is the width in output pixels, and it cannot
# quietly fall under a pixel and vanish the way the old winch windings did.
# Anything thinner than 2 output pixels is wear, and wear carries an alpha
# under about 110 so it dissolves into grain on the downsample instead of
# aliasing into a hard line.

$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

# Lifted verbatim from make_deck.ps1 rather than rewritten, because the deck and
# its dressing have to come out of the same downsampler or the props will carry
# a different edge softness from the plate they stand on.
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

# ---------------------------------------------------------------------------
#  Local vocabulary. organic.ps1 gives us Disc, Ring, Stroke, Poly, Panel,
#  Rivets and Truss; these are the few shapes a steel deck needs that a living
#  thing never did.
# ---------------------------------------------------------------------------

function Box($g, $col, $x, $y, $span, $tall) {
    $g.FillRectangle((Brush $col), (RectF $x $y $span $tall))
}

# organic.ps1's Blob cannot draw an outline, and half the small props here are
# a silhouette that needs an ink edge to survive being 30 pixels wide.
function Blob($g, $pts, $fill, $edge, $wide, $tension = 0.5) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, [float]$tension)
    if ($null -ne $fill) { $g.FillPath((Brush $fill), $path) }
    if ($null -ne $edge) {
        $pen = New-Object System.Drawing.Pen -ArgumentList $edge, ([float]$wide)
        $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($pen, $path)
        $pen.Dispose()
    }
    $path.Dispose()
}

function Oval($g, $col, $cx, $cy, $long, $short) {
    $g.FillEllipse((Brush $col), (RectF ($cx - $long) ($cy - $short) ($long * 2) ($short * 2)))
}

# A part of a ring. Every round thing here gets its highlight as an arc on the
# up-left quadrant rather than as a whole ring: a full bright ring reads as a
# washer, an arc reads as a curved surface catching the light.
function Arc($g, $col, $cx, $cy, $rad, $from, $sweep, $wide) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, ([float]$wide)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawArc($pen, (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2)), [float]$from, [float]$sweep)
    $pen.Dispose()
}

# One rivet on its own. Rivets() needs a line to walk, and a corner casting or a
# hold-down bolt is a single mark in a place nothing else goes.
function Bolt($g, $cx, $cy, $rad, $seat, $cap) {
    Disc $g $seat ($cx + $rad * 0.26) ($cy + $rad * 0.26) $rad
    Disc $g $cap  ($cx - $rad * 0.26) ($cy - $rad * 0.26) ($rad * 0.66)
}

# A cylinder lying on the deck, seen from above. The crown of it is a line up
# the middle, not an edge, so the bright band runs down the centre and both long
# sides fall away dark. Light it like a wall instead - bright at one edge, dark
# at the other - and the pipe reads as a plank, which is what the old reel did.
function Tube($g, $x, $y, $span, $tall, $face, $crown, $edge) {
    Box $g $face  $x $y $span $tall
    Box $g $edge  $x $y $span ($tall * 0.17)
    Box $g $edge  $x ($y + $tall * 0.80) $span ($tall * 0.20)
    Box $g $crown $x ($y + $tall * 0.30) $span ($tall * 0.14)
}

# The outline of a capsule: two domes and the barrel between them, as one
# region. FillMode has to be Winding or the overlap between the domes and the
# barrel is punched back out again and the thing comes out as two rings.
function CapsulePath($x0, $x1, $cy, $rad) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.FillMode = [System.Drawing.Drawing2D.FillMode]::Winding
    $path.AddEllipse((RectF ($x0 - $rad) ($cy - $rad) ($rad * 2) ($rad * 2)))
    $path.AddEllipse((RectF ($x1 - $rad) ($cy - $rad) ($rad * 2) ($rad * 2)))
    $path.AddRectangle((RectF $x0 ($cy - $rad) ($x1 - $x0) ($rad * 2)))
    return $path
}

# One link of chain, laid along a bearing. GDI only draws axis-aligned
# ellipses, so the link is built at the origin and then carried into place by
# the matrix; without that every link in the heap points the same way and the
# pile reads as a doily.
function Link($g, $col, $cx, $cy, $long, $short, $deg, $wide) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddEllipse((RectF (-$long) (-$short) ($long * 2) ($short * 2)))
    $mat = New-Object System.Drawing.Drawing2D.Matrix
    $mat.Translate([float]$cx, [float]$cy)
    $mat.Rotate([float]$deg)
    $path.Transform($mat)
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, ([float]$wide)
    $g.DrawPath($pen, $path)
    $pen.Dispose(); $mat.Dispose(); $path.Dispose()
}

# Scuffs and rust, seeded so a re-run gives the same deck. Nothing offshore is
# clean and nothing offshore is evenly dirty, so the rust goes down as three
# marks - a wide stain, a dark heart and a bright bloom offset into the light -
# rather than as one flat patch, which is the difference between corrosion and
# a coffee ring.
function Wear($g, $seed, $x, $y, $span, $tall, $scuffs, $rusts) {
    $rnd = New-Object System.Random -ArgumentList ([int]$seed)
    for ($i = 0; $i -lt $scuffs; $i++) {
        $sx = $x + $rnd.NextDouble() * $span
        $sy = $y + $rnd.NextDouble() * $tall
        $len = (0.05 + $rnd.NextDouble() * 0.12) * $span
        Stroke $g (C 178 190 200 (46 + $rnd.Next(0, 34))) $sx $sy ($sx + $len) ($sy - $SS * $rnd.NextDouble()) (0.55 * $SS)
    }
    for ($i = 0; $i -lt $rusts; $i++) {
        $sx = $x + $rnd.NextDouble() * $span
        $sy = $y + $rnd.NextDouble() * $tall
        $rad = (1.0 + $rnd.NextDouble() * 3.2) * $SS
        Disc $g (C 132 86 52 30) $sx $sy $rad
        Disc $g (C  84 48 26 38) ($sx + $rad * 0.16) ($sy + $rad * 0.16) ($rad * 0.54)
        Disc $g (C 176 124 78 34) ($sx - $rad * 0.34) ($sy - $rad * 0.34) ($rad * 0.24)
    }
}

"generating the deck dressing..."

# The steel, blue-biased to match the hull rather than the old neutral deck.
$steelTop = C 172 184 194
$steelHi  = C 132 146 156
$steel    = C  96 106 116
$steelLo  = C  58  66  76
$steelDk  = C  30  36  44
$grime    = C  38  42  50
$rubber   = C  40  40  46
$ink      = C  14  16  20

$rust     = C 132  86  52
$rustLo   = C  84  48  26
$rustHi   = C 176 124  78

$glass    = C 138 208 216
$lampWarm = C 250 216 150
$paintY   = C 190 162  58
$brass    = C 188 150  92

$timber   = C 132 108  70
$timberHi = C 170 144 100
$timberLo = C  82  64  38

$fire     = C 138  48  40
$fireHi   = C 182  78  62
$fireLo   = C  86  28  24

$plantY   = C 168 138  48
$plantYHi = C 214 184  82
$plantYLo = C 104  84  28

$grp      = C 196 200 198
$grpHi    = C 232 236 234
$grpLo    = C 130 138 140
$orange   = C 196 104  34

# =====================================================================
#  QUAD  -  sixteen bottles in a pallet frame. The brown shoulders are
#  helium and the pale ones oxygen, which is the only way a gas rack
#  says what it is from above: you cannot see a label, only a shoulder.
# =====================================================================
$outW = 76; $outH = 76
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

$helium   = C 118  76  44
$heliumHi = C 158 108  68
$oxy      = C 172 182 186
$oxyHi    = C 214 222 224

Panel $g 0 0 $W $H $steelLo $steelHi $steelDk (3 * $SS)
Box $g $grime ($W * 0.085) ($H * 0.085) ($W * 0.83) ($H * 0.83)

# The four frame rails, laid over the well so the bottles sit down inside it.
$rail = $W * 0.085
Panel $g 0 0 $W $rail $steel $steelTop $steelDk (2 * $SS)
Panel $g 0 ($H - $rail) $W $rail $steel $steelTop $steelDk (2 * $SS)
Panel $g 0 0 $rail $H $steel $steelTop $steelDk (2 * $SS)
Panel $g ($W - $rail) 0 $rail $H $steel $steelTop $steelDk (2 * $SS)
Rivets $g ($W * 0.14) ($rail * 0.5) ($W * 0.86) ($rail * 0.5) 7 (1.3 * $SS) $steelDk $steelTop
Rivets $g ($W * 0.14) ($H - $rail * 0.5) ($W * 0.86) ($H - $rail * 0.5) 7 (1.3 * $SS) $steelDk $steelTop
Rivets $g ($rail * 0.5) ($H * 0.14) ($rail * 0.5) ($H * 0.86) 7 (1.3 * $SS) $steelDk $steelTop
Rivets $g ($W - $rail * 0.5) ($H * 0.14) ($W - $rail * 0.5) ($H * 0.86) 7 (1.3 * $SS) $steelDk $steelTop

for ($row = 0; $row -lt 4; $row++) {
    for ($col = 0; $col -lt 4; $col++) {
        $bx = $W * (0.2225 + 0.185 * $col)
        $by = $H * (0.2225 + 0.185 * $row)
        if ((($row + $col) % 2) -eq 0) { $face = $helium; $lit = $heliumHi }
        else                           { $face = $oxy;    $lit = $oxyHi }

        Disc $g $steelDk ($bx + 0.9 * $SS) ($by + 0.9 * $SS) (5.9 * $SS)
        Disc $g $steel    $bx $by (5.7 * $SS)
        Disc $g $face     ($bx - 0.4 * $SS) ($by - 0.4 * $SS) (4.4 * $SS)
        Arc  $g $lit      ($bx - 0.4 * $SS) ($by - 0.4 * $SS) (3.3 * $SS) 170 120 (1.1 * $SS)
        Ring $g $ink      $bx $by (5.7 * $SS) (1.1 * $SS)
        Disc $g $steelDk  $bx $by (1.7 * $SS)
        Disc $g $steelTop ($bx - 0.4 * $SS) ($by - 0.4 * $SS) (0.9 * $SS)
    }
}

# Pad eyes at the corners. A quad is landed by crane onto a moving deck and
# then never touched again, so the eyes are the only part of it that is bright.
foreach ($ex in @(0.062, 0.938)) {
    foreach ($ey in @(0.062, 0.938)) {
        Disc $g $steelDk ($W * $ex + $SS * 0.5) ($H * $ey + $SS * 0.5) (3.6 * $SS)
        Disc $g $steelHi ($W * $ex) ($H * $ey) (3.4 * $SS)
        Disc $g $ink     ($W * $ex) ($H * $ey) (1.5 * $SS)
        Arc  $g $steelTop ($W * $ex) ($H * $ey) (2.5 * $SS) 175 130 (1.0 * $SS)
    }
}

# A stencil on the rail. Short bars, not letters: at 76 pixels a legible word is
# a grey smear, but the rhythm of stencilled text still reads as stencilled text.
foreach ($sx in @(0.30, 0.42, 0.54)) {
    Stroke $g (C 190 162 58 110) ($W * $sx) ($H * 0.043) ($W * ($sx + 0.075)) ($H * 0.043) (1.0 * $SS)
}
Wear $g 3301 ($W * 0.03) ($H * 0.03) ($W * 0.94) ($H * 0.94) 14 9
Finish $bmp $g $outW $outH "deck_quad.png"

# =====================================================================
#  BASKET  -  a subsea cargo basket. Open lattice sides, a mesh floor
#  that goes black in the middle, and four slings already made up to a
#  master link so the crane can take it without anybody leaning in.
# =====================================================================
$outW = 88; $outH = 62
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Box $g $steelDk 0 0 $W $H
Box $g (C 18 22 26) ($W * 0.04) ($H * 0.06) ($W * 0.92) ($H * 0.88)

# The mesh floor. Drawn light on black and then dulled back down the middle,
# because a basket is a hole with a grid over it and the eye wants the grid to
# fade as it goes in.
for ($i = 0; $i -lt 12; $i++) {
    $mx = $W * (0.06 + 0.0764 * $i)
    Stroke $g (C 104 116 126 255) $mx ($H * 0.07) $mx ($H * 0.93) (1.2 * $SS)
    Stroke $g (C 8 10 14 200) ($mx + 1.1 * $SS) ($H * 0.07) ($mx + 1.1 * $SS) ($H * 0.93) (0.6 * $SS)
}
for ($i = 0; $i -lt 8; $i++) {
    $my = $H * (0.09 + 0.117 * $i)
    Stroke $g (C 86 98 108 255) ($W * 0.05) $my ($W * 0.95) $my (1.0 * $SS)
}
Oval $g (C 10 12 16 90) ($W * 0.50) ($H * 0.50) ($W * 0.32) ($H * 0.30)

# Sandbags, stowed at one end. They are subsea consumables and they live in a
# basket, and they are the only soft thing in the whole dressing - which is why
# they go in a corner rather than the middle, where the slings land.
foreach ($bag in @(@(0.20, 0.30), @(0.24, 0.47), @(0.19, 0.64))) {
    $bx = $W * $bag[0]; $by = $H * $bag[1]
    Blob $g @( (P ($bx - 10 * $SS) $by), (P $bx ($by - 5.5 * $SS)),
               (P ($bx + 10 * $SS) $by), (P $bx ($by + 5.5 * $SS)) ) (C 104 100 82) (C 52 50 40) (1.0 * $SS) 0.9
    Stroke $g (C 140 134 112 140) ($bx - 6 * $SS) ($by - 1.8 * $SS) ($bx + 6 * $SS) ($by - 1.8 * $SS) (1.0 * $SS)
    Stroke $g (C 40 38 30 150) ($bx - 3 * $SS) ($by + 2.4 * $SS) ($bx + 4 * $SS) ($by + 2.2 * $SS) (0.7 * $SS)
}

# The frame. Lattice rails rather than bars: it is the single cue that says
# offshore steel rather than shopping trolley, and it costs one call a side.
Truss $g ($W * 0.02) ($H * 0.075) ($W * 0.98) ($H * 0.075) (5.5 * $SS) 7 $steelHi $steel $steelDk (2.1 * $SS)
Truss $g ($W * 0.02) ($H * 0.925) ($W * 0.98) ($H * 0.925) (5.5 * $SS) 7 $steelHi $steel $steelDk (2.1 * $SS)
Panel $g 0 ($H * 0.02) ($W * 0.055) ($H * 0.96) $steel $steelTop $steelDk (1.6 * $SS)
Panel $g ($W * 0.945) ($H * 0.02) ($W * 0.055) ($H * 0.96) $steel $steelTop $steelDk (1.6 * $SS)
Rivets $g ($W * 0.027) ($H * 0.12) ($W * 0.027) ($H * 0.88) 8 (1.1 * $SS) $steelDk $steelTop
Rivets $g ($W * 0.972) ($H * 0.12) ($W * 0.972) ($H * 0.88) 8 (1.1 * $SS) $steelDk $steelTop

foreach ($cx in @(0.045, 0.955)) {
    foreach ($cy in @(0.075, 0.925)) {
        Panel $g ($W * $cx - 4.5 * $SS) ($H * $cy - 4.5 * $SS) (9 * $SS) (9 * $SS) $steelLo $steelTop $steelDk (1.4 * $SS)
        Bolt $g ($W * $cx) ($H * $cy) (1.5 * $SS) $steelDk $steelTop
    }
}

# Four slings to a master link. Wire rope is a dark core with a bright lay on
# top of it, and each one gets a shadow thrown down and right onto the mesh
# below: the slings are made up above the basket, and without the shadow the
# four of them read as a cross painted on the floor.
foreach ($cx in @(0.09, 0.91)) {
    foreach ($cy in @(0.14, 0.86)) {
        Stroke $g (C 6 8 12 150) ($W * $cx + 2 * $SS) ($H * $cy + 2.4 * $SS) ($W * 0.50 + 2 * $SS) ($H * 0.50 + 2.4 * $SS) (2.2 * $SS)
        Stroke $g $steelDk ($W * $cx) ($H * $cy) ($W * 0.50) ($H * 0.50) (1.8 * $SS)
        Stroke $g $steelHi ($W * $cx) ($H * $cy) ($W * 0.50) ($H * 0.50) (0.8 * $SS)
        Ring $g $steelTop ($W * $cx) ($H * $cy) (2.6 * $SS) (1.2 * $SS)
        Disc $g $steelDk ($W * $cx) ($H * $cy) (1.0 * $SS)
    }
}
Disc $g $steelDk ($W * 0.50 + $SS) ($H * 0.50 + $SS) (6.2 * $SS)
Disc $g $steel    ($W * 0.50) ($H * 0.50) (6.0 * $SS)
Disc $g (C 18 22 26) ($W * 0.50) ($H * 0.50) (2.6 * $SS)
Ring $g $ink      ($W * 0.50) ($H * 0.50) (6.0 * $SS) (1.4 * $SS)
Arc  $g $steelTop ($W * 0.50) ($H * 0.50) (4.4 * $SS) 165 140 (1.4 * $SS)
Stroke $g $steelTop ($W * 0.50) ($H * 0.36) ($W * 0.50) ($H * 0.44) (1.6 * $SS)

Wear $g 5507 ($W * 0.02) ($H * 0.02) ($W * 0.96) ($H * 0.96) 12 8
Finish $bmp $g $outW $outH "deck_basket.png"

# =====================================================================
#  CHAIN  -  a heap of it, the way it actually ends up: snaked out of
#  the locker, never coiled, walked over for a week. Links alternate
#  flat and edge-on, which is the whole reason a chain reads as chain
#  and not as a row of rings.
# =====================================================================
$outW = 48; $outH = 40
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Oval $g (C 12 14 18 110) ($W * 0.52) ($H * 0.54) ($W * 0.44) ($H * 0.40)

# Chain is never coiled by hand, it is flaked down out of the locker, so the
# path is a slack spiral working its way across the deck with the radius
# wandering. A clean mathematical curve came out as a doily: the jitter is what
# makes it a heap somebody dropped.
$rnd = New-Object System.Random -ArgumentList 4409
$path = @()
for ($i = 0; $i -le 31; $i++) {
    $t = $i / 31.0
    $a = $t * 6.283 * 2.35 + 0.8
    $rad = 0.30 + 0.07 * [math]::Sin($t * 9.1 + 1.4)
    $path += , @( ($W * (0.50 + $rad * [math]::Cos($a)) + ($rnd.NextDouble() - 0.5) * 5 * $SS),
                  ($H * (0.26 + 0.50 * $t + ($rad * 0.62) * [math]::Sin($a)) + ($rnd.NextDouble() - 0.5) * 4 * $SS) )
}
for ($i = 0; $i -lt 31; $i++) {
    $ax = $path[$i][0];     $ay = $path[$i][1]
    $bx = $path[$i + 1][0]; $by = $path[$i + 1][1]
    $deg = [math]::Atan2($by - $ay, $bx - $ax) * 180.0 / [math]::PI
    # Real chain alternates flat link, edge-on link, all the way down. Draw them
    # all the same way round and you get a row of rings instead.
    if (($i % 2) -eq 0) { $short = 3.0 * $SS } else { $short = 1.4 * $SS }
    Link $g (C 10 12 16 170) ($ax + 1.0 * $SS) ($ay + 1.2 * $SS) (5.0 * $SS) $short $deg (2.4 * $SS)
    Link $g $steelLo $ax $ay (5.0 * $SS) $short $deg (2.2 * $SS)
    Link $g $steel   ($ax - 0.3 * $SS) ($ay - 0.35 * $SS) (4.8 * $SS) ($short * 0.92) $deg (1.4 * $SS)
    Link $g (C 180 192 202 160) ($ax - 0.7 * $SS) ($ay - 0.8 * $SS) (4.4 * $SS) ($short * 0.74) $deg (0.7 * $SS)
}

# Two shackles. The bow is an arc, the pin is a bar across its open end and the
# pin is what tells you it is a shackle rather than one more link.
foreach ($sh in @(@(0.20, 0.20, 3.6), @(0.79, 0.80, 3.0))) {
    $sx = $W * $sh[0]; $sy = $H * $sh[1]; $rad = $sh[2] * $SS
    Arc  $g $steelDk ($sx + 0.6 * $SS) ($sy + 0.6 * $SS) $rad 150 240 (2.6 * $SS)
    Arc  $g $steel    $sx $sy $rad 150 240 (2.4 * $SS)
    Arc  $g $steelTop $sx $sy $rad 175 110 (0.9 * $SS)
    Stroke $g $steelHi ($sx - $rad * 0.98) ($sy + $rad * 0.52) ($sx + $rad * 0.98) ($sy + $rad * 0.52) (2.2 * $SS)
    Disc $g $steelTop ($sx + $rad * 0.98) ($sy + $rad * 0.52) (1.4 * $SS)
    Disc $g $ink      ($sx + $rad * 0.98) ($sy + $rad * 0.52) (0.5 * $SS)
}

Wear $g 7717 ($W * 0.10) ($H * 0.12) ($W * 0.80) ($H * 0.76) 8 10
Finish $bmp $g $outW $outH "deck_chain.png"

# =====================================================================
#  DRUMS  -  a pallet of them on timber dunnage. The boards are the only
#  wood in the game, which is exactly why they are worth drawing: one
#  warm non-steel colour on a deck of grey reads as cargo instantly.
# =====================================================================
$outW = 52; $outH = 44
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Box $g (C 16 18 22 160) 0 0 $W $H
for ($i = 0; $i -lt 6; $i++) {
    $by = $H * (0.025 + 0.163 * $i)
    $tall = $H * 0.125
    Panel $g 0 $by $W $tall $timber $timberHi $timberLo (1.4 * $SS)
    for ($k = 0; $k -lt 3; $k++) {
        $gy = $by + $tall * (0.24 + 0.26 * $k)
        Stroke $g (C 82 64 38 90) ($W * 0.04) $gy ($W * 0.96) ($gy + $SS * 0.3) (0.6 * $SS)
    }
    Bolt $g ($W * 0.08) ($by + $tall * 0.5) (1.0 * $SS) $timberLo $steelHi
    Bolt $g ($W * 0.92) ($by + $tall * 0.5) (1.0 * $SS) $timberLo $steelHi
}

# Four drum tops. Two blue and two red, because a deck never has four of the
# same thing on one pallet and the pair of colours does the work of a label.
$drumA = C 52 74 96; $drumAHi = C 88 118 144
$drumB = C 118 60 46; $drumBHi = C 162 92 68
foreach ($dr in @(@(0.27, 0.27, 0), @(0.73, 0.27, 1), @(0.27, 0.73, 1), @(0.73, 0.73, 0))) {
    $dx = $W * $dr[0]; $dy = $H * $dr[1]
    if ($dr[2] -eq 0) { $face = $drumA; $lit = $drumAHi } else { $face = $drumB; $lit = $drumBHi }

    Disc $g (C 10 12 16 150) ($dx + 1.2 * $SS) ($dy + 1.4 * $SS) (10.4 * $SS)
    Disc $g $steelLo $dx $dy (10.2 * $SS)
    Disc $g $face    $dx $dy (9.0 * $SS)
    Arc  $g $lit     $dx $dy (6.6 * $SS) 168 128 (1.6 * $SS)
    Ring $g $steelDk $dx $dy (9.6 * $SS) (1.4 * $SS)
    Ring $g $steelTop $dx $dy (10.2 * $SS) (0.9 * $SS)
    Ring $g (C 132 86 52 90) $dx $dy (8.0 * $SS) (2.2 * $SS)
    # The two bungs. A drum has a big one and a small one and they are never
    # opposite each other, which is the detail that stops the top reading as a tin.
    Disc $g $steelDk ($dx + 4.4 * $SS) ($dy - 3.0 * $SS) (2.2 * $SS)
    Disc $g $brass   ($dx + 4.2 * $SS) ($dy - 3.2 * $SS) (1.5 * $SS)
    Disc $g $steelDk ($dx - 3.6 * $SS) ($dy + 4.2 * $SS) (1.4 * $SS)
    Disc $g $steelHi ($dx - 3.7 * $SS) ($dy + 4.1 * $SS) (0.9 * $SS)
}
Wear $g 9109 ($W * 0.02) ($H * 0.02) ($W * 0.96) ($H * 0.96) 12 10
Finish $bmp $g $outW $outH "deck_drums.png"

# =====================================================================
#  CHAMBER  -  the deck decompression chamber, which is the one thing on
#  this deck that exists nowhere else in the game. Two capsules, a
#  transfer trunk between them, and six little lit ports: the crew are
#  inside it for a month at a time and the lights being on is the whole
#  story of the prop.
# =====================================================================
$outW = 200; $outH = 104
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Panel $g 0 0 $W $H $steelLo $steelHi $steelDk (3 * $SS)
Truss $g ($W * 0.03) ($H * 0.045) ($W * 0.97) ($H * 0.045) (6 * $SS) 14 $steelHi $steel $steelDk (2.0 * $SS)
Truss $g ($W * 0.03) ($H * 0.955) ($W * 0.97) ($H * 0.955) (6 * $SS) 14 $steelHi $steel $steelDk (2.0 * $SS)
foreach ($px in @(0.03, 0.97)) {
    Panel $g ($W * $px - 3 * $SS) ($H * 0.04) (6 * $SS) ($H * 0.92) $steel $steelTop $steelDk (1.4 * $SS)
    Rivets $g ($W * $px) ($H * 0.10) ($W * $px) ($H * 0.90) 9 (1.1 * $SS) $steelDk $steelTop
}

$capRad = $H * 0.205
$capX0  = $W * 0.135
$capX1  = $W * 0.865
foreach ($cyf in @(0.27, 0.73)) {
    $cy = $H * $cyf
    $path = CapsulePath $capX0 $capX1 $cy $capRad
    $g.FillPath((Brush $steel), $path)

    $saved = $g.Save()
    $g.SetClip($path, [System.Drawing.Drawing2D.CombineMode]::Intersect)

    # Countershading for a horizontal cylinder from above, clipped so the bands
    # follow the domes round instead of running off the ends as stripes.
    Box $g (C 22 28 36 170) 0 ($cy - $capRad) $W ($capRad * 0.40)
    Box $g (C 22 28 36 190) 0 ($cy + $capRad * 0.56) $W ($capRad * 0.46)
    Box $g (C 172 184 194 110) 0 ($cy - $capRad * 0.26) $W ($capRad * 0.30)
    Box $g (C 214 224 232 70) 0 ($cy - $capRad * 0.16) $W ($capRad * 0.12)

    # Rib rings. Each is a shadow and a light side, offset against the light,
    # so the hoop stands off the shell rather than being painted on it.
    for ($i = 0; $i -lt 9; $i++) {
        $rx = $W * (0.17 + 0.0825 * $i)
        Stroke $g (C 26 32 40 200) $rx ($cy - $capRad) $rx ($cy + $capRad) (2.0 * $SS)
        Stroke $g (C 172 184 194 140) ($rx - 1.5 * $SS) ($cy - $capRad) ($rx - 1.5 * $SS) ($cy + $capRad) (0.8 * $SS)
    }
    Rivets $g ($capX0) ($cy - $capRad * 0.46) ($capX1) ($cy - $capRad * 0.46) 26 (1.0 * $SS) $steelDk $steelTop
    Rivets $g ($capX0) ($cy + $capRad * 0.52) ($capX1) ($cy + $capRad * 0.52) 26 (1.0 * $SS) $steelDk $steelTop
    Wear $g (1200 + $cyf * 1000) ($capX0 - $capRad) ($cy - $capRad) ($capX1 - $capX0 + $capRad * 2) ($capRad * 2) 16 10

    $g.Restore($saved)
    $pen = New-Object System.Drawing.Pen -ArgumentList $steelDk, ([float](1.0 * $SS))
    $g.DrawPath($pen, $path)
    $pen.Dispose()
    $path.Dispose()

    # Only the outboard end carries a door. Ringing both ends the same way is
    # what made the pair read as two cassette reels rather than two pressure
    # vessels: a chamber is entered from one end, and the far end is a plain
    # dished head with a lug on it for the day it has to come off the ship.
    Ring $g $steelHi $capX0 $cy ($capRad * 0.54) (1.4 * $SS)
    Ring $g (C 26 32 40 190) $capX0 $cy ($capRad * 0.40) (0.9 * $SS)
    for ($i = 0; $i -lt 8; $i++) {
        $a = [math]::PI * 2 * $i / 8.0 + 0.3
        Bolt $g ($capX0 + [math]::Cos($a) * $capRad * 0.54) ($cy + [math]::Sin($a) * $capRad * 0.54) (1.0 * $SS) $steelDk $steelTop
    }
    Disc $g $steelDk $capX0 $cy (2.6 * $SS)
    Ring $g $steelTop $capX0 $cy (2.6 * $SS) (0.9 * $SS)
    for ($i = 0; $i -lt 3; $i++) {
        $a = [math]::PI * $i / 3.0 + 0.4
        Stroke $g $steelTop ($capX0 - [math]::Cos($a) * 2.4 * $SS) ($cy - [math]::Sin($a) * 2.4 * $SS) `
                            ($capX0 + [math]::Cos($a) * 2.4 * $SS) ($cy + [math]::Sin($a) * 2.4 * $SS) (0.7 * $SS)
    }
    Arc  $g (C 206 218 226 110) $capX1 $cy ($capRad * 0.60) 150 140 (1.4 * $SS)
    Arc  $g (C 22 28 36 150) $capX1 $cy ($capRad * 0.60) 330 140 (1.2 * $SS)
    Ring $g (C 26 32 40 150) $capX1 $cy ($capRad * 0.34) (0.9 * $SS)
    Disc $g $steelDk ($capX1 + $capRad * 0.06) ($cy - $capRad * 0.56) (2.0 * $SS)
    Ring $g $steelTop ($capX1 + $capRad * 0.06) ($cy - $capRad * 0.56) (1.8 * $SS) (0.8 * $SS)

    # Three lit viewports. Lamp rather than a flat disc, because a light behind
    # thick glass has a halo and a flat disc has never once looked lit.
    foreach ($vx in @(0.22, 0.355, 0.78)) {
        $px = $W * $vx
        Disc $g $steelDk $px $cy (4.6 * $SS)
        Disc $g $steelHi $px $cy (4.0 * $SS)
        Lamp $g $px $cy (2.2 * $SS) $lampWarm
        Ring $g $ink $px $cy (4.0 * $SS) (1.1 * $SS)
        for ($i = 0; $i -lt 6; $i++) {
            $a = [math]::PI * 2 * $i / 6.0
            Disc $g $steelTop ($px + [math]::Cos($a) * 4.8 * $SS) ($cy + [math]::Sin($a) * 4.8 * $SS) (0.7 * $SS)
        }
    }
}

# Pipe runs from one chamber to the other. Two capsules with clear deck between
# them read as two separate drums that happen to be parked together; a pair of
# gas lines crossing the gap is what makes the eye take them as one system.
foreach ($px in @(0.255, 0.695)) {
    Stroke $g $steelDk ($W * $px) ($H * 0.38) ($W * $px) ($H * 0.62) (2.8 * $SS)
    Stroke $g (C 172 184 194 130) ($W * $px - 0.9 * $SS) ($H * 0.38) ($W * $px - 0.9 * $SS) ($H * 0.62) (0.8 * $SS)
    Disc $g $steelLo ($W * $px) ($H * 0.50) (2.2 * $SS)
    Ring $g $brass   ($W * $px) ($H * 0.50) (1.7 * $SS) (0.8 * $SS)
}

# The transfer trunk: the short fat cylinder the divers crawl through between
# the living chamber and the one that mates to the bell.
Tube $g ($W * 0.435) ($H * 0.325) ($W * 0.13) ($H * 0.35) $steel (C 200 212 220 110) (C 24 30 38 180)
Box $g $ink ($W * 0.435) ($H * 0.325) (0.6 * $SS) ($H * 0.35)
Box $g $ink ($W * 0.565 - 0.6 * $SS) ($H * 0.325) (0.6 * $SS) ($H * 0.35)
Rivets $g ($W * 0.445) ($H * 0.34) ($W * 0.555) ($H * 0.34) 7 (1.0 * $SS) $steelDk $steelTop
Rivets $g ($W * 0.445) ($H * 0.655) ($W * 0.555) ($H * 0.655) 7 (1.0 * $SS) $steelDk $steelTop

Disc $g $steelDk ($W * 0.50 + $SS) ($H * 0.50 + $SS) (7.4 * $SS)
Disc $g $steelHi ($W * 0.50) ($H * 0.50) (7.2 * $SS)
Ring $g $ink     ($W * 0.50) ($H * 0.50) (7.2 * $SS) (1.4 * $SS)
Arc  $g $steelTop ($W * 0.50) ($H * 0.50) (5.6 * $SS) 170 130 (1.2 * $SS)
for ($i = 0; $i -lt 8; $i++) {
    $a = [math]::PI * 2 * $i / 8.0 + 0.4
    Bolt $g ($W * 0.50 + [math]::Cos($a) * 5.8 * $SS) ($H * 0.50 + [math]::Sin($a) * 5.8 * $SS) (1.2 * $SS) $steelDk $steelTop
}
Disc $g $steel ($W * 0.50) ($H * 0.50) (2.6 * $SS)
for ($i = 0; $i -lt 4; $i++) {
    $a = [math]::PI * $i / 4.0
    Stroke $g $steelDk ($W * 0.50 - [math]::Cos($a) * 2.4 * $SS) ($H * 0.50 - [math]::Sin($a) * 2.4 * $SS) `
                       ($W * 0.50 + [math]::Cos($a) * 2.4 * $SS) ($H * 0.50 + [math]::Sin($a) * 2.4 * $SS) (0.8 * $SS)
}

# The gas manifold down one end. A sat system is mostly pipework and the bank
# of little handwheels is where the man on watch actually stands.
Panel $g ($W * 0.055) ($H * 0.435) ($W * 0.055) ($H * 0.13) $steelLo $steelTop $steelDk (1.2 * $SS)
for ($i = 0; $i -lt 3; $i++) {
    $vx = $W * (0.068 + 0.019 * $i)
    Disc $g $steelDk $vx ($H * 0.50) (1.6 * $SS)
    Ring $g $brass   $vx ($H * 0.50) (1.4 * $SS) (0.8 * $SS)
}
foreach ($hy in @(0.46, 0.50, 0.54)) {
    Stroke $g $rubber ($W * 0.095) ($H * $hy) ($W * 0.135) ($H * ($hy * 0.6 + 0.18)) (1.6 * $SS)
}
Stroke $g $rubber ($W * 0.62) ($H * 0.50) ($W * 0.63) ($H * 0.50) (2.4 * $SS)

Wear $g 4211 ($W * 0.02) ($H * 0.02) ($W * 0.96) ($H * 0.10) 10 6
Wear $g 4212 ($W * 0.02) ($H * 0.88) ($W * 0.96) ($H * 0.10) 10 6
Finish $bmp $g $outW $outH "deck_chamber.png"

# =====================================================================
#  REEL  -  the diver's umbilical. Thirteen wraps of black hose between
#  two flanges, which from above is just a barrel of stripes, so the
#  flanges and the crank are what have to carry it.
# =====================================================================
$outW = 92; $outH = 64
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Panel $g 0 ($H * 0.10) $W ($H * 0.80) $steelLo $steelHi $steelDk (2.5 * $SS)
Truss $g ($W * 0.05) ($H * 0.145) ($W * 0.95) ($H * 0.145) (4 * $SS) 9 $steelHi $steel $steelDk (1.6 * $SS)
Truss $g ($W * 0.05) ($H * 0.855) ($W * 0.95) ($H * 0.855) (4 * $SS) 9 $steelHi $steel $steelDk (1.6 * $SS)

# The wound hose. A drum of coiled rubber from above is a barrel of stripes, and
# stripes alone came out as a radiator grille the first time. What fixes it is
# lighting the stripes like a barrel: both long edges dark, one bright band just
# above the middle, laid over the top of every wrap at once.
$coilTop  = $H * 0.185
$coilTall = $H * 0.63
Box $g (C 26 25 30) ($W * 0.145) $coilTop ($W * 0.71) $coilTall
for ($i = 0; $i -lt 13; $i++) {
    $wx = $W * (0.163 + 0.0545 * $i)
    Stroke $g $rubber $wx ($coilTop + $SS) $wx ($coilTop + $coilTall - $SS) (3.0 * $SS)
    Stroke $g (C 16 15 19 230) ($wx + 1.7 * $SS) ($coilTop + $SS) ($wx + 1.7 * $SS) ($coilTop + $coilTall - $SS) (1.1 * $SS)
}
Box $g (C 10 11 14 180) ($W * 0.145) $coilTop ($W * 0.71) ($coilTall * 0.16)
Box $g (C 10 11 14 200) ($W * 0.145) ($coilTop + $coilTall * 0.79) ($W * 0.71) ($coilTall * 0.21)
Box $g (C 150 158 172 80) ($W * 0.145) ($coilTop + $coilTall * 0.27) ($W * 0.71) ($coilTall * 0.11)
Box $g (C 200 208 220 55) ($W * 0.145) ($coilTop + $coilTall * 0.31) ($W * 0.71) ($coilTall * 0.045)

# The flanges. They have to stand proud of the coil at top and bottom or the
# drum has no ends; the dark bands at their extremes are the rim rolling away
# from the eye, which is the only part of "this is a wheel" that survives being
# seen exactly edge on.
foreach ($fx in @(0.035, 0.885)) {
    Panel $g ($W * $fx) ($H * 0.055) ($W * 0.085) ($H * 0.89) $steel $steelTop $steelDk (1.8 * $SS)
    Box $g (C 24 30 38 160) ($W * $fx) ($H * 0.055) ($W * 0.085) ($H * 0.075)
    Box $g (C 24 30 38 180) ($W * $fx) ($H * 0.86) ($W * 0.085) ($H * 0.085)
    Box $g (C 198 208 218 70) ($W * $fx) ($H * 0.40) ($W * 0.085) ($H * 0.055)
    for ($i = 0; $i -lt 4; $i++) {
        $hy = $H * (0.24 + 0.175 * $i)
        Disc $g $steelDk ($W * ($fx + 0.0425)) $hy (2.2 * $SS)
        Arc  $g $steelTop ($W * ($fx + 0.0425)) $hy (2.2 * $SS) 0 170 (0.8 * $SS)
    }
    Rivets $g ($W * ($fx + 0.0425)) ($H * 0.10) ($W * ($fx + 0.0425)) ($H * 0.90) 11 (0.9 * $SS) $steelDk $steelTop
}

# The crank. A hub, an arm out of it and a handle on the end, drawn proud of the
# flange so it reads as the thing you turn.
Disc $g $steelDk ($W * 0.0775 + $SS) ($H * 0.50 + $SS) (5.4 * $SS)
Disc $g $steel    ($W * 0.0775) ($H * 0.50) (5.2 * $SS)
Ring $g $ink      ($W * 0.0775) ($H * 0.50) (5.2 * $SS) (1.2 * $SS)
Arc  $g $steelTop ($W * 0.0775) ($H * 0.50) (3.4 * $SS) 170 130 (1.2 * $SS)
Stroke $g $steelDk ($W * 0.0775) ($H * 0.50) ($W * 0.018) ($H * 0.235) (2.6 * $SS)
Stroke $g $steelHi ($W * 0.0775) ($H * 0.50) ($W * 0.018) ($H * 0.235) (1.4 * $SS)
Disc $g $steelDk ($W * 0.018) ($H * 0.235) (2.4 * $SS)
Disc $g $brass   ($W * 0.015) ($H * 0.23) (1.6 * $SS)

# The stub of hose leaving it. The run all the way to the pool is drawn live by
# the deck, so this only has to make the join believable.
Stroke $g $rubber ($W * 0.48) ($H * 0.83) ($W * 0.56) ($H * 1.02) (3.4 * $SS)
Stroke $g (C 118 122 132 110) ($W * 0.475) ($H * 0.84) ($W * 0.553) ($H * 1.0) (0.9 * $SS)
Ring $g $brass ($W * 0.50) ($H * 0.90) (2.2 * $SS) (1.0 * $SS)

Wear $g 6603 ($W * 0.02) ($H * 0.10) ($W * 0.96) ($H * 0.80) 14 9
Finish $bmp $g $outW $outH "deck_reel.png"

# =====================================================================
#  SHACK  -  dive control, which from above is a container roof and
#  nothing else. All the interest has to come off the roof: the air
#  conditioner, the gland box, the cable bundle leaving it and the lit
#  eave over the window wall that faces the pool.
# =====================================================================
$outW = 150; $outH = 96
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Panel $g 0 0 $W $H $steelDk $steelLo (C 8 10 14) (2 * $SS)
Panel $g ($W * 0.025) ($H * 0.03) ($W * 0.95) ($H * 0.94) $steel $steelTop $steelDk (2.2 * $SS)

# Corrugations. A container roof is corrugated and the corrugation pitch is the
# single detail that makes a grey rectangle read as a container.
for ($i = 0; $i -lt 17; $i++) {
    $rx = $W * (0.06 + 0.0537 * $i)
    Stroke $g (C 26 32 40 190) $rx ($H * 0.05) $rx ($H * 0.95) (2.0 * $SS)
    Stroke $g (C 172 184 194 120) ($rx - 1.6 * $SS) ($H * 0.05) ($rx - 1.6 * $SS) ($H * 0.95) (0.9 * $SS)
}
Stroke $g (C 22 28 36 150) ($W * 0.03) ($H * 0.50) ($W * 0.97) ($H * 0.50) (1.6 * $SS)

# Corner castings. Standard fittings, and the reason a container can be picked
# up at all, so they are the four brightest corners on the prop.
foreach ($cx in @(0.045, 0.955)) {
    foreach ($cy in @(0.06, 0.94)) {
        Panel $g ($W * $cx - 4 * $SS) ($H * $cy - 4 * $SS) (8 * $SS) (8 * $SS) $steelHi $steelTop $steelDk (1.4 * $SS)
        Disc $g $ink ($W * $cx) ($H * $cy) (1.8 * $SS)
        Arc  $g $steelTop ($W * $cx) ($H * $cy) (1.8 * $SS) 180 130 (0.7 * $SS)
    }
}

# The air conditioner. A sat control room is a sealed box full of heat, so the
# unit on the roof is the biggest single object up there.
Panel $g ($W * 0.075) ($H * 0.10) ($W * 0.25) ($H * 0.29) $steelHi $steelTop $steelDk (2 * $SS)
Panel $g ($W * 0.085) ($H * 0.12) ($W * 0.23) ($H * 0.25) $steelLo $steelHi $steelDk (1.2 * $SS)
for ($i = 0; $i -lt 6; $i++) {
    $ly = $H * (0.135 + 0.038 * $i)
    Stroke $g (C 20 24 30 190) ($W * 0.095) $ly ($W * 0.145) $ly (0.9 * $SS)
}
Disc $g $steelDk ($W * 0.245) ($H * 0.245) (7.6 * $SS)
Ring $g $steelTop ($W * 0.245) ($H * 0.245) (7.6 * $SS) (1.2 * $SS)
for ($i = 0; $i -lt 5; $i++) {
    $a = [math]::PI * 2 * $i / 5.0 + 0.5
    Blob $g @( (P ($W * 0.245) ($H * 0.245)),
               (P ($W * 0.245 + [math]::Cos($a) * 7.0 * $SS) ($H * 0.245 + [math]::Sin($a) * 7.0 * $SS)),
               (P ($W * 0.245 + [math]::Cos($a + 0.7) * 6.4 * $SS) ($H * 0.245 + [math]::Sin($a + 0.7) * 6.4 * $SS)) ) `
        (C 120 132 142 200) $null 0 0.2
}
Disc $g $steelDk ($W * 0.245) ($H * 0.245) (2.0 * $SS)
Disc $g $steelTop ($W * 0.2425) ($H * 0.2425) (1.0 * $SS)
Rivets $g ($W * 0.08) ($H * 0.105) ($W * 0.32) ($H * 0.105) 9 (1.0 * $SS) $steelDk $steelTop

# The gland box and the bundle out of it. Every cable in the dive spread comes
# up through this one box, and the bundle leaving the frame is what says the
# shack is wired into the rest of the ship.
Panel $g ($W * 0.44) ($H * 0.11) ($W * 0.19) ($H * 0.15) $rubber (C 84 84 92) (C 14 14 18) (1.6 * $SS)
for ($i = 0; $i -lt 4; $i++) {
    $gx = $W * (0.462 + 0.045 * $i)
    Disc $g (C 20 20 24) $gx ($H * 0.185) (1.8 * $SS)
    Ring $g $brass $gx ($H * 0.185) (1.6 * $SS) (0.9 * $SS)
}
Stroke $g (C 16 16 20 230) ($W * 0.63) ($H * 0.185) ($W * 1.0) ($H * 0.30) (3.6 * $SS)
Stroke $g (C 110 114 124 110) ($W * 0.63) ($H * 0.175) ($W * 1.0) ($H * 0.29) (1.0 * $SS)
foreach ($tx in @(0.70, 0.80, 0.90)) {
    Stroke $g $steelHi ($W * $tx) ($H * (0.185 + ($tx - 0.63) * 0.31 - 0.022)) ($W * $tx) ($H * (0.185 + ($tx - 0.63) * 0.31 + 0.022)) (1.2 * $SS)
}

# The roof hatch, dogged shut.
Disc $g $steelDk ($W * 0.775) ($H * 0.735) (6.2 * $SS)
Disc $g $steelHi ($W * 0.77) ($H * 0.73) (5.6 * $SS)
Ring $g $ink     ($W * 0.77) ($H * 0.73) (5.6 * $SS) (1.2 * $SS)
Arc  $g $steelTop ($W * 0.77) ($H * 0.73) (4.0 * $SS) 170 130 (1.0 * $SS)
for ($i = 0; $i -lt 4; $i++) {
    $a = [math]::PI * 2 * $i / 4.0 + 0.6
    Stroke $g $steelDk ($W * 0.77 + [math]::Cos($a) * 3.0 * $SS) ($H * 0.73 + [math]::Sin($a) * 3.0 * $SS) `
                       ($W * 0.77 + [math]::Cos($a) * 6.4 * $SS) ($H * 0.73 + [math]::Sin($a) * 6.4 * $SS) (1.4 * $SS)
}
Panel $g ($W * 0.70) ($H * 0.715) (3 * $SS) (3 * $SS) $steelLo $steelTop $steelDk (0.8 * $SS)

# The eave, and the light under it. This is the pool-facing side, it is the only
# warm light in the dressing, and it is the reason the shack reads as manned.
Panel $g ($W * 0.09) ($H * 0.885) ($W * 0.62) ($H * 0.075) $steelLo $steelHi $steelDk (1.4 * $SS)
Stroke $g (C 30 36 44 220) ($W * 0.10) ($H * 0.962) ($W * 0.70) ($H * 0.962) (2.6 * $SS)
Stroke $g $glass ($W * 0.11) ($H * 0.962) ($W * 0.69) ($H * 0.962) (1.4 * $SS)
foreach ($lx in @(0.20, 0.40, 0.60)) {
    Lamp $g ($W * $lx) ($H * 0.962) (1.6 * $SS) $lampWarm
}
Stroke $g (C 190 162 58 140) ($W * 0.03) ($H * 0.985) ($W * 0.97) ($H * 0.985) (1.6 * $SS)

Wear $g 8801 ($W * 0.03) ($H * 0.03) ($W * 0.94) ($H * 0.92) 20 14
Finish $bmp $g $outW $outH "deck_shack.png"

# =====================================================================
#  HATCH  -  standing open, which is the only state worth drawing: a
#  closed hatch is a plate with a ring on it. Open, it is a black hole
#  with rungs going away from you, and it is the only hole in the deck
#  apart from the one the bell goes down.
# =====================================================================
$outW = 40; $outH = 52
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

# The cover, folded back on its hinges and lying on the deck.
Panel $g ($W * 0.02) 0 ($W * 0.96) ($H * 0.50) $steel $steelTop $steelDk (2 * $SS)
for ($i = 0; $i -lt 5; $i++) {
    $sx = $W * (0.13 + 0.185 * $i)
    Stroke $g (C 26 32 40 190) $sx ($H * 0.04) $sx ($H * 0.46) (1.8 * $SS)
    Stroke $g (C 172 184 194 130) ($sx - 1.4 * $SS) ($H * 0.04) ($sx - 1.4 * $SS) ($H * 0.46) (0.8 * $SS)
}
Rivets $g ($W * 0.08) ($H * 0.035) ($W * 0.92) ($H * 0.035) 8 (1.1 * $SS) $steelDk $steelTop
Stroke $g $steelDk ($W * 0.30) ($H * 0.22) ($W * 0.70) ($H * 0.22) (2.6 * $SS)
Stroke $g $steelHi ($W * 0.30) ($H * 0.215) ($W * 0.70) ($H * 0.215) (1.2 * $SS)
Disc $g $steelDk ($W * 0.30) ($H * 0.22) (1.6 * $SS)
Disc $g $steelDk ($W * 0.70) ($H * 0.22) (1.6 * $SS)
foreach ($hx in @(0.10, 0.90)) {
    Panel $g ($W * $hx - 2.6 * $SS) ($H * 0.475) (5.2 * $SS) (5.0 * $SS) $steelHi $steelTop $steelDk (1.0 * $SS)
    Disc $g $ink ($W * $hx) ($H * 0.505) (1.2 * $SS)
}

# The coaming. Raised, so it gets a bright top and a dark inner face, and that
# pair is the whole reason the hole looks like a hole and not like paint.
Panel $g 0 ($H * 0.52) $W ($H * 0.48) $steelLo $steelHi $steelDk (2.2 * $SS)
Box $g $steelDk ($W * 0.10) ($H * 0.575) ($W * 0.80) ($H * 0.375)
Box $g (C 10 13 17) ($W * 0.135) ($H * 0.60) ($W * 0.73) ($H * 0.33)
Box $g (C 6 8 11) ($W * 0.175) ($H * 0.635) ($W * 0.65) ($H * 0.28)
Rivets $g ($W * 0.06) ($H * 0.555) ($W * 0.94) ($H * 0.555) 9 (1.1 * $SS) $steelDk $steelTop
Rivets $g ($W * 0.06) ($H * 0.965) ($W * 0.94) ($H * 0.965) 9 (1.1 * $SS) $steelDk $steelTop

# Four rungs falling away. They get dimmer and shorter as they go, because that
# is the only way a ladder reads as going down rather than as a grating.
$rungCols = @((C 150 160 170), (C 112 122 132), (C 82 90 100), (C 58 64 74))
Stroke $g (C 96 104 114) ($W * 0.245) ($H * 0.615) ($W * 0.255) ($H * 0.915) (1.6 * $SS)
Stroke $g (C 96 104 114) ($W * 0.755) ($H * 0.615) ($W * 0.745) ($H * 0.915) (1.6 * $SS)
for ($i = 0; $i -lt 4; $i++) {
    $ry = $H * (0.655 + 0.072 * $i)
    $inset = 0.245 + 0.012 * $i
    Stroke $g $rungCols[$i] ($W * $inset) $ry ($W * (1.0 - $inset)) $ry (1.8 * $SS)
    Stroke $g (C 200 212 222 (90 - 20 * $i)) ($W * $inset) ($ry - 0.9 * $SS) ($W * (1.0 - $inset)) ($ry - 0.9 * $SS) (0.6 * $SS)
}

# The trip line. A hole in a deck at night is exactly what kills people, so it
# gets the only unbroken run of yellow anywhere in the dressing.
foreach ($ty in @(0.535, 0.985)) {
    Stroke $g (C 30 36 44 200) ($W * 0.02) ($H * $ty) ($W * 0.98) ($H * $ty) (2.6 * $SS)
    Stroke $g $paintY ($W * 0.02) ($H * $ty) ($W * 0.98) ($H * $ty) (1.6 * $SS)
}
foreach ($tx in @(0.025, 0.975)) {
    Stroke $g $paintY ($W * $tx) ($H * 0.535) ($W * $tx) ($H * 0.985) (1.6 * $SS)
}
Wear $g 2213 ($W * 0.02) 0 ($W * 0.96) ($H * 0.52) 8 6
Wear $g 2214 ($W * 0.02) ($H * 0.52) ($W * 0.96) ($H * 0.06) 6 5
Finish $bmp $g $outW $outH "deck_hatch.png"

# =====================================================================
#  WEIGHTS  -  spare bell ballast, three slabs stacked with an offset.
#  The offset is the prop: three flush slabs are one slab, and the
#  stepped edge is the only thing that gives it height from above.
# =====================================================================
$outW = 44; $outH = 40
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

for ($slab = 0; $slab -lt 3; $slab++) {
    $ox = $slab * 3.6 * $SS
    $oy = $slab * 3.2 * $SS
    Box $g (C 10 12 16 130) ($W * 0.06 + $ox + 1.4 * $SS) ($H * 0.08 + $oy + 1.6 * $SS) ($W * 0.68) ($H * 0.62)
    Panel $g ($W * 0.06 + $ox) ($H * 0.08 + $oy) ($W * 0.68) ($H * 0.62) $steel $steelTop $steelDk (2.2 * $SS)
    Stroke $g (C 22 28 36 150) ($W * 0.09 + $ox) ($H * 0.16 + $oy) ($W * 0.71 + $ox) ($H * 0.16 + $oy) (1.0 * $SS)
    Stroke $g (C 172 184 194 110) ($W * 0.09 + $ox) ($H * 0.145 + $oy) ($W * 0.71 + $ox) ($H * 0.145 + $oy) (0.7 * $SS)
    # Cast weights are never smooth: the sand leaves a mottle that catches light
    # differently from rolled plate, and without it a slab is a grey rectangle.
    Wear $g (3140 + $slab) ($W * 0.06 + $ox) ($H * 0.08 + $oy) ($W * 0.68) ($H * 0.62) 7 5
    foreach ($sx in @(0.18, 0.30, 0.42)) {
        Stroke $g (C 190 162 58 100) ($W * ($sx) + $ox) ($H * 0.60 + $oy) ($W * ($sx + 0.07) + $ox) ($H * 0.60 + $oy) (0.9 * $SS)
    }
}

# Two lifting eyes on the top slab, because that is the one the crane takes.
foreach ($ey in @(@(0.34, 0.28), @(0.62, 0.56))) {
    $ex = $W * $ey[0] + 7.2 * $SS
    $eyy = $H * $ey[1] + 6.4 * $SS
    Disc $g $steelDk ($ex + 0.6 * $SS) ($eyy + 0.6 * $SS) (3.6 * $SS)
    Disc $g $steelHi $ex $eyy (3.4 * $SS)
    Disc $g $ink     $ex $eyy (1.5 * $SS)
    Arc  $g $steelTop $ex $eyy (2.5 * $SS) 175 135 (1.0 * $SS)
}
Finish $bmp $g $outW $outH "deck_weights.png"

# =====================================================================
#  CHEST  -  the rolling tool chest. The only saturated red on the deck
#  apart from the fire kit, and the one prop that says somebody works
#  here rather than that somebody stores things here.
# =====================================================================
$outW = 56; $outH = 34
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Box $g (C 10 12 16 140) ($W * 0.02 + 1.6 * $SS) ($H * 0.04 + 1.8 * $SS) ($W * 0.96) ($H * 0.90)
Panel $g ($W * 0.02) ($H * 0.04) ($W * 0.96) ($H * 0.90) $fire $fireHi $fireLo (2.2 * $SS)

# The lid, with the raised lip along its front edge.
Panel $g ($W * 0.04) ($H * 0.07) ($W * 0.92) ($H * 0.36) (C 156 60 50) $fireHi $fireLo (1.6 * $SS)
Stroke $g (C 220 130 110 120) ($W * 0.07) ($H * 0.115) ($W * 0.93) ($H * 0.115) (1.0 * $SS)
Stroke $g (C 60 20 18 200) ($W * 0.04) ($H * 0.44) ($W * 0.96) ($H * 0.44) (1.6 * $SS)
Stroke $g (C 210 120 100 110) ($W * 0.04) ($H * 0.425) ($W * 0.96) ($H * 0.425) (0.8 * $SS)

# Four drawer fronts, each with a chrome pull. A pull is a dark slot with a
# bright bar in it; drawn as one bright bar it reads as a stripe of paint.
for ($i = 0; $i -lt 4; $i++) {
    $dy = $H * (0.48 + 0.115 * $i)
    Panel $g ($W * 0.05) $dy ($W * 0.90) ($H * 0.095) (C 122 42 36) (C 168 70 58) (C 62 20 18) (1.0 * $SS)
    Stroke $g (C 40 14 12 210) ($W * 0.28) ($dy + $H * 0.048) ($W * 0.72) ($dy + $H * 0.048) (1.8 * $SS)
    Stroke $g $steelTop ($W * 0.28) ($dy + $H * 0.042) ($W * 0.72) ($dy + $H * 0.042) (1.0 * $SS)
    Disc $g $steelHi ($W * 0.28) ($dy + $H * 0.045) (1.0 * $SS)
    Disc $g $steelHi ($W * 0.72) ($dy + $H * 0.045) (1.0 * $SS)
}

# Castors, and a padlock on the hasp because a tool chest on a working deck is
# always locked and every hand on board knows who has the key.
foreach ($cx in @(0.12, 0.88)) {
    Panel $g ($W * $cx - 2.4 * $SS) ($H * 0.90) (4.8 * $SS) (3.0 * $SS) $steelLo $steelHi $steelDk (0.8 * $SS)
    Disc $g $ink    ($W * $cx) ($H * 0.955) (2.6 * $SS)
    Disc $g $rubber ($W * $cx) ($H * 0.955) (2.0 * $SS)
    Arc  $g (C 120 124 134 150) ($W * $cx) ($H * 0.955) (1.2 * $SS) 170 130 (0.7 * $SS)
}
Panel $g ($W * 0.455) ($H * 0.40) (4.4 * $SS) (4.0 * $SS) $steelHi $steelTop $steelDk (0.9 * $SS)
Disc $g $steelDk ($W * 0.50) ($H * 0.455) (1.6 * $SS)
Arc  $g $brass   ($W * 0.50) ($H * 0.435) (1.4 * $SS) 180 180 (0.8 * $SS)
foreach ($sx in @(0.10, 0.22, 0.34)) {
    Stroke $g (C 214 214 208 120) ($W * $sx) ($H * 0.22) ($W * ($sx + 0.08)) ($H * 0.22) (1.0 * $SS)
}

# Chipped paint. Red kit on a steel deck loses its corners first and the bare
# steel underneath is what makes the red look like paint rather than plastic.
$rnd = New-Object System.Random -ArgumentList 6011
for ($i = 0; $i -lt 10; $i++) {
    $cx = $W * (0.04 + $rnd.NextDouble() * 0.92)
    $cy = $H * (0.06 + $rnd.NextDouble() * 0.86)
    Disc $g (C 150 158 168 110) $cx $cy (0.5 * $SS + $rnd.NextDouble() * $SS)
}
Wear $g 6012 ($W * 0.02) ($H * 0.04) ($W * 0.96) ($H * 0.90) 10 7
Finish $bmp $g $outW $outH "deck_chest.png"

# =====================================================================
#  WELDER  -  an engine-driven set on a skid. Yellow, because every one
#  of them ever built is yellow, and the coiled lead on top is what
#  stops it reading as a generator.
# =====================================================================
$outW = 62; $outH = 44
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Panel $g 0 ($H * 0.02) $W ($H * 0.96) $steelDk $steelLo (C 8 10 14) (2 * $SS)
Panel $g ($W * 0.04) ($H * 0.08) ($W * 0.92) ($H * 0.84) $plantY $plantYHi $plantYLo (2.4 * $SS)

# Fork pockets in the skid, so the thing can actually be moved.
foreach ($fy in @(0.20, 0.72)) {
    Box $g (C 22 20 12 200) ($W * 0.0) ($H * $fy) ($W * 0.055) ($H * 0.10)
    Box $g (C 22 20 12 200) ($W * 0.945) ($H * $fy) ($W * 0.055) ($H * 0.10)
}

# The radiator. Louvres are 5 SS wide, which is why this prop is hardware and
# not metal in make_3d: metal's plate seams would cut across them at a third
# their pitch and turn the grille into moire.
Panel $g ($W * 0.07) ($H * 0.13) ($W * 0.20) ($H * 0.74) (C 138 112 38) $plantYHi $plantYLo (1.4 * $SS)
for ($i = 0; $i -lt 9; $i++) {
    $lx = $W * (0.085 + 0.0215 * $i)
    Stroke $g (C 18 16 10 210) $lx ($H * 0.17) $lx ($H * 0.83) (1.2 * $SS)
    Stroke $g (C 222 196 110 110) ($lx - 0.9 * $SS) ($H * 0.17) ($lx - 0.9 * $SS) ($H * 0.83) (0.6 * $SS)
}
Rivets $g ($W * 0.08) ($H * 0.115) ($W * 0.27) ($H * 0.115) 7 (0.9 * $SS) $plantYLo $plantYHi

# The engine cover, with its lifting handle and latches.
Panel $g ($W * 0.31) ($H * 0.14) ($W * 0.33) ($H * 0.50) (C 140 114 40) $plantYHi $plantYLo (1.8 * $SS)
Stroke $g (C 80 64 20 200) ($W * 0.34) ($H * 0.24) ($W * 0.61) ($H * 0.24) (1.0 * $SS)
Stroke $g (C 226 200 120 110) ($W * 0.34) ($H * 0.228) ($W * 0.61) ($H * 0.228) (0.6 * $SS)
Stroke $g $steelDk ($W * 0.40) ($H * 0.44) ($W * 0.56) ($H * 0.44) (2.2 * $SS)
Stroke $g $steelTop ($W * 0.40) ($H * 0.432) ($W * 0.56) ($H * 0.432) (1.0 * $SS)
foreach ($lx in @(0.34, 0.61)) {
    Disc $g $steelDk ($W * $lx) ($H * 0.60) (1.4 * $SS)
    Disc $g $steelHi ($W * $lx - 0.4 * $SS) ($H * 0.60 - 0.4 * $SS) (0.8 * $SS)
}

# Exhaust stub with the soot round it, and the control panel beside it.
Disc $g $steelDk ($W * 0.70) ($H * 0.22) (3.2 * $SS)
Disc $g (C 12 12 14) ($W * 0.70) ($H * 0.22) (1.8 * $SS)
Ring $g $steelTop ($W * 0.70) ($H * 0.22) (3.2 * $SS) (0.9 * $SS)
Disc $g (C 10 10 12 70) ($W * 0.70) ($H * 0.22) (5.4 * $SS)
Panel $g ($W * 0.79) ($H * 0.12) ($W * 0.16) ($H * 0.26) (C 46 44 40) (C 96 92 84) (C 16 16 14) (1.2 * $SS)
foreach ($dx in @(0.825, 0.885)) {
    Disc $g (C 18 18 20) ($W * $dx) ($H * 0.24) (1.8 * $SS)
    Ring $g $steelTop ($W * $dx) ($H * 0.24) (1.8 * $SS) (0.6 * $SS)
    Stroke $g $steelTop ($W * $dx) ($H * 0.24) ($W * $dx + 1.2 * $SS) ($H * 0.24 - 1.0 * $SS) (0.6 * $SS)
}
Lamp $g ($W * 0.925) ($H * 0.155) (1.2 * $SS) (C 255 96 84)

# The coiled lead. Three laps of hot black rubber with the holder tucked into
# the middle of them, which is exactly how one gets left at the end of a shift.
Ring $g (C 14 14 18 220) ($W * 0.72) ($H * 0.70) (7.4 * $SS) (3.0 * $SS)
Ring $g (C 62 60 68 180) ($W * 0.72) ($H * 0.695) (7.4 * $SS) (1.0 * $SS)
Ring $g (C 14 14 18 220) ($W * 0.72) ($H * 0.70) (4.6 * $SS) (2.6 * $SS)
Ring $g (C 62 60 68 160) ($W * 0.72) ($H * 0.695) (4.6 * $SS) (0.9 * $SS)
Stroke $g (C 14 14 18 220) ($W * 0.655) ($H * 0.76) ($W * 0.50) ($H * 0.90) (2.8 * $SS)
Panel $g ($W * 0.695) ($H * 0.665) (4.0 * $SS) (3.2 * $SS) (C 122 34 30) (C 176 70 58) (C 60 16 14) (0.9 * $SS)
Stroke $g $steelTop ($W * 0.745) ($H * 0.685) ($W * 0.775) ($H * 0.685) (0.9 * $SS)

Wear $g 7401 ($W * 0.04) ($H * 0.08) ($W * 0.92) ($H * 0.84) 14 10
Finish $bmp $g $outW $outH "deck_welder.png"

# =====================================================================
#  HOSE  -  a fire hose reel. Red drum, coils going in, and the nozzle
#  lead laid off it. Three of these are placed round the deck, so it has
#  to read at 38 pixels without help from anything next to it.
# =====================================================================
$outW = 38; $outH = 38
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Panel $g ($W * 0.05) ($H * 0.82) ($W * 0.90) ($H * 0.14) $steelLo $steelHi $steelDk (1.4 * $SS)
Rivets $g ($W * 0.12) ($H * 0.89) ($W * 0.88) ($H * 0.89) 5 (1.1 * $SS) $steelDk $steelTop

Disc $g (C 10 12 16 150) ($W * 0.50 + 1.4 * $SS) ($H * 0.50 + 1.6 * $SS) (17.0 * $SS)
Disc $g $fireLo ($W * 0.50) ($H * 0.50) (17.0 * $SS)
Disc $g $fire   ($W * 0.50) ($H * 0.50) (15.8 * $SS)
Ring $g $ink    ($W * 0.50) ($H * 0.50) (17.0 * $SS) (1.4 * $SS)
Arc  $g (C 214 126 104 130) ($W * 0.50) ($H * 0.50) (16.4 * $SS) 170 120 (1.2 * $SS)

# Five coils. Each one is a dark hose body with a light lay along its upper
# side, so the spiral reads as rubber going round rather than as a target.
for ($i = 0; $i -lt 5; $i++) {
    $rad = (14.0 - 2.5 * $i) * $SS
    Ring $g (C 96 32 28 235) ($W * 0.50) ($H * 0.50) $rad (2.0 * $SS)
    Arc  $g (C 178 74 60 150) ($W * 0.50) ($H * 0.50) ($rad - 0.5 * $SS) 165 135 (0.8 * $SS)
    Arc  $g (C 40 12 10 150) ($W * 0.50) ($H * 0.50) ($rad + 0.7 * $SS) 0 120 (0.7 * $SS)
}

Disc $g $steelDk ($W * 0.50 + 0.5 * $SS) ($H * 0.50 + 0.5 * $SS) (4.0 * $SS)
Disc $g $steelHi ($W * 0.50) ($H * 0.50) (3.8 * $SS)
Ring $g $ink     ($W * 0.50) ($H * 0.50) (3.8 * $SS) (1.0 * $SS)
for ($i = 0; $i -lt 4; $i++) {
    $a = [math]::PI * $i / 4.0 + 0.3
    Stroke $g $steelDk ($W * 0.50 - [math]::Cos($a) * 3.2 * $SS) ($H * 0.50 - [math]::Sin($a) * 3.2 * $SS) `
                       ($W * 0.50 + [math]::Cos($a) * 3.2 * $SS) ($H * 0.50 + [math]::Sin($a) * 3.2 * $SS) (0.8 * $SS)
}
Disc $g $steelTop ($W * 0.50 - 0.4 * $SS) ($H * 0.50 - 0.4 * $SS) (1.4 * $SS)

# The lead off it, and the brass nozzle on the end. The nozzle is the only
# thing on the prop that is not red or grey, and it is what names it.
Stroke $g $ink   ($W * 0.62) ($H * 0.62) ($W * 0.95) ($H * 0.80) (3.2 * $SS)
Stroke $g (C 96 32 28) ($W * 0.62) ($H * 0.62) ($W * 0.95) ($H * 0.79) (2.2 * $SS)
Stroke $g (C 178 74 60 130) ($W * 0.63) ($H * 0.605) ($W * 0.94) ($H * 0.775) (0.7 * $SS)
Stroke $g $steelDk ($W * 0.88) ($H * 0.765) ($W * 0.97) ($H * 0.81) (3.4 * $SS)
Stroke $g $brass   ($W * 0.885) ($H * 0.765) ($W * 0.965) ($H * 0.805) (2.2 * $SS)
Disc $g (C 236 206 144) ($W * 0.965) ($H * 0.805) (1.4 * $SS)

Wear $g 5115 ($W * 0.20) ($H * 0.20) ($W * 0.60) ($H * 0.60) 8 6
Finish $bmp $g $outW $outH "deck_hose.png"

# =====================================================================
#  RAFT  -  a life raft canister in its cradle. White glassfibre against
#  grey steel, which makes it the brightest object on the deck, and the
#  painter line trailing off it is the detail that says it is armed.
# =====================================================================
$outW = 54; $outH = 34
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

# The cradle first, because the canister sits in it and hides its middle.
Panel $g ($W * 0.10) ($H * 0.10) ($W * 0.80) ($H * 0.80) $steelLo $steelHi $steelDk (1.6 * $SS)
foreach ($sx in @(0.19, 0.78)) {
    Panel $g ($W * $sx) 0 ($W * 0.07) $H $steel $steelTop $steelDk (1.2 * $SS)
    Bolt $g ($W * ($sx + 0.035)) ($H * 0.075) (1.2 * $SS) $steelDk $steelTop
    Bolt $g ($W * ($sx + 0.035)) ($H * 0.925) (1.2 * $SS) $steelDk $steelTop
}
Rivets $g ($W * 0.14) ($H * 0.13) ($W * 0.86) ($H * 0.13) 9 (0.9 * $SS) $steelDk $steelTop
Rivets $g ($W * 0.14) ($H * 0.87) ($W * 0.86) ($H * 0.87) 9 (0.9 * $SS) $steelDk $steelTop

$capRad = $H * 0.40
$path = CapsulePath ($W * 0.30) ($W * 0.70) ($H * 0.50) $capRad
$g.FillPath((Brush $grp), $path)
$saved = $g.Save()
$g.SetClip($path, [System.Drawing.Drawing2D.CombineMode]::Intersect)
Box $g (C 104 112 116 150) 0 ($H * 0.50 - $capRad) $W ($capRad * 0.34)
Box $g (C 104 112 116 190) 0 ($H * 0.50 + $capRad * 0.52) $W ($capRad * 0.52)
Box $g $grpHi 0 ($H * 0.50 - $capRad * 0.30) $W ($capRad * 0.34)
Stroke $g (C 176 92 30 235) 0 ($H * 0.50 + $capRad * 0.34) $W ($H * 0.50 + $capRad * 0.34) (3.2 * $SS)
Stroke $g (C 226 136 62 170) 0 ($H * 0.50 + $capRad * 0.22) $W ($H * 0.50 + $capRad * 0.22) (1.0 * $SS)
foreach ($sx in @(0.36, 0.44, 0.60)) {
    Stroke $g (C 70 76 78 150) ($W * $sx) ($H * 0.36) ($W * ($sx + 0.055)) ($H * 0.36) (0.8 * $SS)
}
Wear $g 3308 ($W * 0.26) ($H * 0.12) ($W * 0.50) ($H * 0.76) 10 5
$g.Restore($saved)
$pen = New-Object System.Drawing.Pen -ArgumentList $steelDk, ([float](1.2 * $SS))
$g.DrawPath($pen, $path)
$pen.Dispose(); $path.Dispose()

# The clamp band round its waist: the canister is two halves and this is what
# holds them together until the hydrostatic release lets go.
Stroke $g (C 96 104 108 220) ($W * 0.50) ($H * 0.13) ($W * 0.50) ($H * 0.87) (2.0 * $SS)
Stroke $g $steelTop ($W * 0.495) ($H * 0.13) ($W * 0.495) ($H * 0.87) (0.8 * $SS)
Disc $g $steelDk ($W * 0.50) ($H * 0.50) (2.2 * $SS)
Disc $g $steelTop ($W * 0.495) ($H * 0.495) (1.2 * $SS)

# Hydrostatic release and the painter trailing away over the deck edge.
Panel $g ($W * 0.755) ($H * 0.62) (4.0 * $SS) (3.4 * $SS) (C 40 44 48) (C 92 98 104) (C 12 14 16) (0.8 * $SS)
Stroke $g (C 214 218 214 210) ($W * 0.79) ($H * 0.72) ($W * 0.90) ($H * 0.86) (1.2 * $SS)
Stroke $g (C 214 218 214 210) ($W * 0.90) ($H * 0.86) ($W * 0.99) ($H * 0.80) (1.2 * $SS)
Stroke $g (C 150 156 152 160) ($W * 0.79) ($H * 0.73) ($W * 0.90) ($H * 0.87) (0.6 * $SS)
Finish $bmp $g $outW $outH "deck_raft.png"

# =====================================================================
#  BITTS  -  two posts on a base plate. The smallest prop in the set and
#  the one that needs the most care: at 30 by 22 it is four circles, so
#  the circles have to be domes and the plate has to have a bevel or the
#  whole thing is a smudge at the rail.
# =====================================================================
$outW = 30; $outH = 22
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

Box $g (C 10 12 16 130) ($W * 0.05 + 1.2 * $SS) ($H * 0.20 + 1.4 * $SS) ($W * 0.90) ($H * 0.66)
Panel $g ($W * 0.05) ($H * 0.20) ($W * 0.90) ($H * 0.66) $steelLo $steelHi $steelDk (1.8 * $SS)
Stroke $g (C 22 28 36 150) ($W * 0.08) ($H * 0.30) ($W * 0.92) ($H * 0.30) (0.8 * $SS)
foreach ($bx in @(0.12, 0.88)) {
    foreach ($by in @(0.30, 0.76)) {
        Bolt $g ($W * $bx) ($H * $by) (1.7 * $SS) $steelDk $steelTop
        Ring $g (C 22 28 36 160) ($W * $bx) ($H * $by) (2.2 * $SS) (0.6 * $SS)
    }
}

# The two posts. A bitt is a turned column with a lip on top, and the lip is
# what keeps a rope on it, so the cap ring is not decoration.
foreach ($bx in @(0.31, 0.69)) {
    $px = $W * $bx
    $py = $H * 0.53
    Disc $g (C 10 12 16 160) ($px + 1.0 * $SS) ($py + 1.2 * $SS) (4.6 * $SS)
    Disc $g $steelLo $px $py (4.4 * $SS)
    Disc $g $steel   $px $py (3.6 * $SS)
    Disc $g $steelHi ($px - 0.7 * $SS) ($py - 0.8 * $SS) (2.6 * $SS)
    Arc  $g $steelTop ($px - 0.3 * $SS) ($py - 0.3 * $SS) (1.8 * $SS) 170 130 (1.0 * $SS)
    Ring $g $ink $px $py (4.4 * $SS) (1.0 * $SS)
    Ring $g (C 22 28 36 170) $px $py (3.6 * $SS) (0.7 * $SS)
    # Rope polish: the one band of a bitt that never rusts, because a mooring
    # line has been dragged round it a thousand times.
    Arc $g (C 200 210 218 120) $px $py (4.0 * $SS) 20 140 (0.8 * $SS)
}
Wear $g 1909 ($W * 0.05) ($H * 0.20) ($W * 0.90) ($H * 0.66) 7 6
Finish $bmp $g $outW $outH "deck_bitts.png"

"done"
