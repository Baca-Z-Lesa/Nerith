Add-Type -AssemblyName System.Drawing

# The six things worth picking up off the floor.
#
# They used to be three concentric rings in the kind's colour, which told you
# there was something there and nothing whatever about what it was. A player
# crossing a dark basin for a piece of salvage should be able to tell from the
# edge of the lamp whether it was worth the swim, and that is a picture, not a
# hue.
#
# Every one of them is drawn as a thing somebody could pick up and carry: an
# object with a lit top, a shaded underside and an ink edge, in the palette the
# config table gives it so the colour still reads at a glance.
. "$PSScriptRoot\organic.ps1"

$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function Box($g, $col, $x, $y, $span, $tall) {
    $g.FillRectangle((Brush $col), (RectF $x $y $span $tall))
}

function Fade($col, $alpha) { C $col.R $col.G $col.B $alpha }

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

function Finish($bmp, $g, $outW, $outH, $name) {
    $g.Dispose()
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, ($outW * 3), ($outH * 3)))) {
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

"generating the salvage..."

$ink = C 12 14 18

# =====================================================================
#  SCRAP  -  a torn piece of somebody else's hull. The cheapest thing
#  down there and the only one that is obviously rubbish at a glance.
# =====================================================================
$outW = 44; $outH = 44
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$W = $outW * $SS; $H = $outH * $SS

$steel   = C 132 142 148
$steelHi = C 186 196 202
$steelLo = C  72  80  86

Blob $g @( (P ($W * 0.18) ($H * 0.30)), (P ($W * 0.62) ($H * 0.14)),
           (P ($W * 0.88) ($H * 0.44)), (P ($W * 0.70) ($H * 0.84)),
           (P ($W * 0.26) ($H * 0.76)) ) $steel $ink (2 * $SS) 0.05
# The torn edge: a line of short bright nicks along the top, which is what
# says "this was ripped off something" rather than "this is a pebble".
foreach ($t in @(0.24, 0.38, 0.52, 0.66, 0.78)) {
    Stroke $g $steelHi ($W * $t) ($H * (0.20 + 0.12 * [math]::Abs($t - 0.5))) `
        ($W * ($t + 0.05)) ($H * (0.27 + 0.10 * [math]::Abs($t - 0.5))) (1.2 * $SS)
}
Stroke $g $steelLo ($W * 0.26) ($H * 0.70) ($W * 0.74) ($H * 0.62) (2 * $SS)
Rivets $g ($W * 0.32) ($H * 0.46) ($W * 0.76) ($H * 0.40) 3 (1.6 * $SS) $steelLo $steelHi
foreach ($i in 0..5) {
    Stroke $g (Fade $steelHi 70) ($W * (0.24 + 0.1 * $i)) ($H * 0.34) `
        ($W * (0.30 + 0.1 * $i)) ($H * 0.64) (0.6 * $SS)
}
Finish $bmp $g $outW $outH "find_scrap.png"

# =====================================================================
#  COPPER LINE  -  a coil of stripped cable. Warm against everything
#  else on the floor, which is most of why it is findable.
# =====================================================================
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]
$c = $W / 2.0

$copper   = C 190 128  86
$copperHi = C 236 178 124
$copperLo = C 112  66  38

for ($i = 4; $i -ge 0; $i--) {
    $rad = $W * (0.16 + 0.055 * $i)
    Ring $g $copperLo $c $c $rad (3.4 * $SS)
    Ring $g $copper $c $c $rad (2.4 * $SS)
    Ring $g (Fade $copperHi 180) ($c - 1.2 * $SS) ($c - 1.2 * $SS) $rad (0.9 * $SS)
}
# The loose end, tucked under the coil and coming out at the bottom right.
Stroke $g $copperLo ($c + $W * 0.20) ($c + $W * 0.16) ($W * 0.92) ($H * 0.86) (3.4 * $SS)
Stroke $g $copper ($c + $W * 0.20) ($c + $W * 0.16) ($W * 0.92) ($H * 0.86) (2.2 * $SS)
Disc $g (C 220 222 226) ($W * 0.92) ($H * 0.86) (1.6 * $SS)
Finish $bmp $g $outW $outH "find_copper.png"

# =====================================================================
#  CORE ROD  -  a fuel rod out of something that had a reactor. It is
#  the first find that is obviously not scrap, so it gets a lamp.
# =====================================================================
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]

$clad   = C  92 106 112
$cladHi = C 150 166 172
$core   = C 126 200 184
$coreHi = C 208 250 236

Panel $g ($W * 0.14) ($H * 0.38) ($W * 0.72) ($H * 0.24) $clad $cladHi (C 40 48 52) (2 * $SS)
$pen = New-Object System.Drawing.Pen -ArgumentList $ink, (1.6 * $SS)
$g.DrawRectangle($pen, [float]($W * 0.14), [float]($H * 0.38), [float]($W * 0.72), [float]($H * 0.24))
$pen.Dispose()

# The window in the cladding, and what is behind it.
Box $g (C 16 22 24) ($W * 0.30) ($H * 0.43) ($W * 0.40) ($H * 0.14)
Box $g $core ($W * 0.31) ($H * 0.44) ($W * 0.38) ($H * 0.12)
Box $g (Fade $coreHi 200) ($W * 0.31) ($H * 0.445) ($W * 0.38) ($H * 0.035)
Lamp $g ($W * 0.50) ($H * 0.50) (3.2 * $SS) $core

# End caps and the collar bands, so it is a made object and not a bar.
foreach ($x in @(0.10, 0.82)) {
    Panel $g ($W * $x) ($H * 0.34) ($W * 0.08) ($H * 0.32) $clad $cladHi (C 40 48 52) (1.5 * $SS)
}
foreach ($x in @(0.26, 0.72)) {
    Stroke $g (C 40 48 52) ($W * $x) ($H * 0.38) ($W * $x) ($H * 0.62) (1.6 * $SS)
    Stroke $g (Fade $cladHi 170) ($W * ($x + 0.012)) ($H * 0.38) ($W * ($x + 0.012)) ($H * 0.62) (0.7 * $SS)
}
Finish $bmp $g $outW $outH "find_rod.png"

# =====================================================================
#  BLACK GLASS  -  a shard of whatever the trench is lined with. Sharp,
#  faceted, and it does not reflect the lamp so much as swallow it.
# =====================================================================
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]

$glass   = C 118 102 168
$glassHi = C 186 172 236
$glassLo = C  46  38  78

Poly $g @( (P ($W * 0.50) ($H * 0.08)), (P ($W * 0.86) ($H * 0.44)),
           (P ($W * 0.66) ($H * 0.90)), (P ($W * 0.22) ($H * 0.78)),
           (P ($W * 0.14) ($H * 0.34)) ) $glassLo
Poly $g @( (P ($W * 0.50) ($H * 0.12)), (P ($W * 0.80) ($H * 0.46)),
           (P ($W * 0.62) ($H * 0.84)), (P ($W * 0.26) ($H * 0.72)),
           (P ($W * 0.20) ($H * 0.36)) ) $glass
# Facets. A shard with no internal edges is a pebble; these are what make the
# light look like it is coming out of the inside of the thing.
Poly $g @( (P ($W * 0.50) ($H * 0.12)), (P ($W * 0.80) ($H * 0.46)),
           (P ($W * 0.48) ($H * 0.52)) ) (Fade $glassHi 110)
Poly $g @( (P ($W * 0.50) ($H * 0.12)), (P ($W * 0.20) ($H * 0.36)),
           (P ($W * 0.48) ($H * 0.52)) ) (Fade $glassHi 60)
Poly $g @( (P ($W * 0.48) ($H * 0.52)), (P ($W * 0.62) ($H * 0.84)),
           (P ($W * 0.26) ($H * 0.72)) ) (Fade $glassLo 190)
$pen = New-Object System.Drawing.Pen -ArgumentList $ink, (1.8 * $SS)
$pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$path.AddPolygon([System.Drawing.PointF[]]@(
    (P ($W * 0.50) ($H * 0.08)), (P ($W * 0.86) ($H * 0.44)),
    (P ($W * 0.66) ($H * 0.90)), (P ($W * 0.22) ($H * 0.78)),
    (P ($W * 0.14) ($H * 0.34)) ))
$g.DrawPath($pen, $path)
$pen.Dispose(); $path.Dispose()
Stroke $g (Fade (C 255 255 255) 150) ($W * 0.54) ($H * 0.20) ($W * 0.70) ($H * 0.42) (1.1 * $SS)
Finish $bmp $g $outW $outH "find_glass.png"

# =====================================================================
#  GRAVID SAC  -  an egg case with something still in it. The only find
#  that is alive, and the only one anybody would hesitate over.
# =====================================================================
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]

$sac   = C 216 152 162
$sacHi = C 248 206 208
$sacLo = C 128  68  84
$vein  = C  92  40  56

Blob $g @( (P ($W * 0.50) ($H * 0.08)), (P ($W * 0.86) ($H * 0.36)),
           (P ($W * 0.78) ($H * 0.82)), (P ($W * 0.30) ($H * 0.88)),
           (P ($W * 0.12) ($H * 0.44)) ) $sacLo $ink (2 * $SS) 0.45
Blob $g @( (P ($W * 0.50) ($H * 0.13)), (P ($W * 0.80) ($H * 0.38)),
           (P ($W * 0.72) ($H * 0.78)), (P ($W * 0.32) ($H * 0.82)),
           (P ($W * 0.18) ($H * 0.44)) ) $sac $null 0 0.45
Blob $g @( (P ($W * 0.44) ($H * 0.18)), (P ($W * 0.66) ($H * 0.34)),
           (P ($W * 0.56) ($H * 0.54)), (P ($W * 0.30) ($H * 0.44)) ) (Fade $sacHi 130) $null 0 0.5

# Three embryos, curled. This is the whole reason it is worth two hundred and
# ten: somebody on the surface wants to know what is growing down here.
foreach ($e in @(@(0.40, 0.42, 1.0), @(0.62, 0.56, 0.8), @(0.44, 0.68, 0.7))) {
    $ex = $W * $e[0]; $ey = $H * $e[1]; $k = $e[2]
    Disc $g (Fade $vein 210) $ex $ey (3.6 * $SS * $k)
    Disc $g (Fade $sacLo 220) ($ex + 1.0 * $SS * $k) ($ey + 1.0 * $SS * $k) (2.4 * $SS * $k)
    Stroke $g (Fade $vein 190) $ex $ey ($ex + 5.0 * $SS * $k) ($ey + 3.0 * $SS * $k) (1.4 * $SS * $k)
    Disc $g (Fade (C 255 255 255) 120) ($ex - 1.0 * $SS * $k) ($ey - 1.2 * $SS * $k) (0.9 * $SS * $k)
}
foreach ($i in 0..6) {
    $a = [math]::PI * 2 * $i / 7.0
    Stroke $g (Fade $vein 90) ($W * 0.5) ($H * 0.5) `
        ($W * 0.5 + [math]::Cos($a) * $W * 0.34) ($H * 0.5 + [math]::Sin($a) * $H * 0.36) (0.8 * $SS)
}
Finish $bmp $g $outW $outH "find_sac.png"

# =====================================================================
#  NERITH ORE  -  the thing the company is actually down here for. Warm,
#  which nothing at this depth has any business being.
# =====================================================================
$res = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $res[0]; $g = $res[1]

$rock   = C  58  54  50
$rockHi = C 104  96  88
$ore    = C 240 178  98
$oreHi  = C 255 232 176

Blob $g @( (P ($W * 0.50) ($H * 0.10)), (P ($W * 0.88) ($H * 0.40)),
           (P ($W * 0.74) ($H * 0.88)), (P ($W * 0.24) ($H * 0.84)),
           (P ($W * 0.10) ($H * 0.40)) ) $rock $ink (2 * $SS) 0.15
Blob $g @( (P ($W * 0.48) ($H * 0.16)), (P ($W * 0.76) ($H * 0.40)),
           (P ($W * 0.62) ($H * 0.60)), (P ($W * 0.28) ($H * 0.54)) ) (Fade $rockHi 150) $null 0 0.2

# The veins. Drawn as a dark channel with a hot line inside it, because a bare
# bright stroke on rock reads as a scratch and a channel reads as something
# running through the stone.
foreach ($v in @(@(0.16, 0.34, 0.44, 0.52, 0.82, 0.44),
                 @(0.30, 0.78, 0.50, 0.46, 0.70, 0.22),
                 @(0.22, 0.60, 0.56, 0.72, 0.80, 0.70))) {
    $pts = @( (P ($W * $v[0]) ($H * $v[1])), (P ($W * $v[2]) ($H * $v[3])),
              (P ($W * $v[4]) ($H * $v[5])) )
    for ($i = 0; $i -lt 2; $i++) {
        Stroke $g (C 30 20 12) ($pts[$i].X) ($pts[$i].Y) ($pts[$i + 1].X) ($pts[$i + 1].Y) (3.2 * $SS)
    }
    for ($i = 0; $i -lt 2; $i++) {
        Stroke $g $ore ($pts[$i].X) ($pts[$i].Y) ($pts[$i + 1].X) ($pts[$i + 1].Y) (1.8 * $SS)
    }
    for ($i = 0; $i -lt 2; $i++) {
        Stroke $g (Fade $oreHi 200) ($pts[$i].X) ($pts[$i].Y) ($pts[$i + 1].X) ($pts[$i + 1].Y) (0.7 * $SS)
    }
}
Lamp $g ($W * 0.50) ($H * 0.48) (3.0 * $SS) $ore
foreach ($i in 0..7) {
    Disc $g (Fade $oreHi 150) ($W * (0.22 + 0.08 * $i)) ($H * (0.30 + 0.06 * ($i % 4))) (0.8 * $SS)
}
Finish $bmp $g $outW $outH "find_ore.png"
