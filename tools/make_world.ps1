Add-Type -AssemblyName System.Drawing

# Seabed props, background critters and the second enemy type. Everything is
# drawn in a light-ish tone where it is meant to be tinted per instance in code.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function P($x, $y) { New-Object System.Drawing.PointF -ArgumentList ([float]$x, [float]$y) }
function C($r, $g, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Brush($col) { New-Object System.Drawing.SolidBrush -ArgumentList $col }
function RectF($x, $y, $w, $h) { New-Object System.Drawing.RectangleF -ArgumentList ([float]$x, [float]$y, [float]$w, [float]$h) }
function Disc($g, $col, $cx, $cy, $r) { $g.FillEllipse((Brush $col), (RectF ($cx - $r) ($cy - $r) ($r * 2) ($r * 2))) }

function NewCanvas($w, $h) {
    $bmp = New-Object System.Drawing.Bitmap -ArgumentList $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    return @($bmp, $g)
}

function Finish($bmp, $g, $w, $h, $name) {
    $g.Dispose()
    foreach ($target in @(@($TexDir, $w, $h), @($PreviewDir, ($w * 4), ($h * 4)))) {
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
    "  $name  ($w x $h)"
}

function Blob($g, $pts, $col) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, 0.5)
    $g.FillPath((Brush $col), $path)
    $path.Dispose()
}

# A tapered ribbon along a curve: the only reliable way to draw an organic limb
# without it breaking into beads.
function Ribbon($g, $col, $cx, $cy, $baseDeg, $curl, $start, $len, $w0, $w1) {
    $N = 16
    $px = @(); $py = @(); $ww = @()
    for ($i = 0; $i -le $N; $i++) {
        $f = $i / [double]$N
        $ang = ([math]::PI * $baseDeg / 180.0) + $curl * $f * $f
        $d = $start + $f * $len
        $px += ($cx + [math]::Cos($ang) * $d)
        $py += ($cy + [math]::Sin($ang) * $d)
        $ww += ($w0 + ($w1 - $w0) * $f)
    }
    $left = @(); $right = @()
    for ($i = 0; $i -le $N; $i++) {
        $a = [math]::Max($i - 1, 0); $b = [math]::Min($i + 1, $N)
        $dx = $px[$b] - $px[$a]; $dy = $py[$b] - $py[$a]
        $l = [math]::Sqrt($dx * $dx + $dy * $dy); if ($l -eq 0) { $l = 1 }
        $nx = -$dy / $l; $ny = $dx / $l
        $left  += ,(P ($px[$i] + $nx * $ww[$i]) ($py[$i] + $ny * $ww[$i]))
        $right += ,(P ($px[$i] - $nx * $ww[$i]) ($py[$i] - $ny * $ww[$i]))
    }
    [array]::Reverse($right)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]($left + $right), 0.3)
    $g.FillPath((Brush $col), $path)
    $path.Dispose()
    # rounded tip
    Disc $g $col $px[$N] $py[$N] $ww[$N]
}

"generating world textures..."

# =====================================================================
#  ROCKS  -  two silhouettes; variety comes from scale/rotation in code
# =====================================================================
$rng = New-Object System.Random 20260817
foreach ($variant in @(@("rock_a.png", 1.0), @("rock_b.png", 0.72))) {
    $W = 84; $H = 60
    $r = NewCanvas ($W * $SS) ($H * $SS); $bmp = $r[0]; $g = $r[1]
    $cx = $W * $SS / 2.0; $cy = $H * $SS * 0.55
    $base = $W * $SS * 0.33 * $variant[1]

    $pts = @()
    for ($i = 0; $i -lt 11; $i++) {
        $a = 2.0 * [math]::PI * $i / 11.0
        $rad = $base * (0.76 + 0.34 * $rng.NextDouble())
        $pts += ,(P ($cx + [math]::Cos($a) * $rad) ($cy + [math]::Sin($a) * $rad * 0.62))
    }
    Blob $g $pts (C 116 132 142)          # tinted dark per instance in code
    # a slightly lighter cap so the rock catches the light from above
    $cap = @()
    for ($i = 0; $i -lt 9; $i++) {
        $a = [math]::PI + [math]::PI * $i / 8.0
        $rad = $base * 0.58 * (0.8 + 0.26 * $rng.NextDouble())
        $cap += ,(P ($cx + [math]::Cos($a) * $rad) ($cy - 10 + [math]::Sin($a) * $rad * 0.5))
    }
    Blob $g $cap (C 144 162 174)
    Finish $bmp $g $W $H $variant[0]
}

# =====================================================================
#  BRANCHING CORAL  -  origin sits at the bottom centre
# =====================================================================
$W = 76; $H = 92
$r = NewCanvas ($W * $SS) ($H * $SS); $bmp = $r[0]; $g = $r[1]
$bx = $W * $SS / 2.0; $by = $H * $SS - 8

# Branches are chains of overlapping discs. A tapered ribbon kept pinching
# itself where the two offset edges met; discs cannot go wrong as long as they
# step closer together than their own radius.
# each arm: angle in degrees, start distance, length, radius at base, radius at tip
$arms = @(@(-90, 0, 248, 30, 12), @(-112, 40, 190, 24, 10), @(-68, 42, 194, 24, 10),
          @(-134, 30, 148, 19, 8), @(-46, 32, 150, 19, 8))

foreach ($arm in $arms) {
    $ang = [math]::PI * $arm[0] / 180.0
    for ($i = 0; $i -le 34; $i++) {
        $f = $i / 34.0
        $d = $arm[1] + $f * $arm[2]
        $rad = $arm[3] + ($arm[4] - $arm[3]) * $f
        Disc $g (C 150 120 160) ($bx + [math]::Cos($ang) * $d) ($by + [math]::Sin($ang) * $d) $rad
    }
    # pale polyp tip
    $tip = $arm[1] + $arm[2]
    Disc $g (C 214 180 220) ($bx + [math]::Cos($ang) * $tip) ($by + [math]::Sin($ang) * $tip) ($arm[4] + 4)
}
Finish $bmp $g $W $H "coral_a.png"

# =====================================================================
#  MOUND CORAL  -  a lumpy brain-coral blob
# =====================================================================
$W = 80; $H = 62
$r = NewCanvas ($W * $SS) ($H * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $W * $SS / 2.0; $by = $H * $SS - 10
$rng = New-Object System.Random 771
for ($i = 0; $i -lt 16; $i++) {
    $a = [math]::PI + [math]::PI * $rng.NextDouble()
    $d = 70 * $rng.NextDouble()
    Disc $g (C 140 150 128) ($cx + [math]::Cos($a) * $d * 1.7) ($by + [math]::Sin($a) * $d * 0.75) (26 + 30 * $rng.NextDouble())
}
for ($i = 0; $i -lt 12; $i++) {
    $a = [math]::PI + [math]::PI * $rng.NextDouble()
    $d = 55 * $rng.NextDouble()
    Disc $g (C 184 196 168) ($cx + [math]::Cos($a) * $d * 1.6) ($by - 12 + [math]::Sin($a) * $d * 0.6) (11 + 12 * $rng.NextDouble())
}
Finish $bmp $g $W $H "coral_b.png"

# =====================================================================
#  KELP  -  a wavy strand, origin at the bottom so it can sway
# =====================================================================
$W = 44; $H = 150
$r = NewCanvas ($W * $SS) ($H * $SS); $bmp = $r[0]; $g = $r[1]
$bx = $W * $SS / 2.0; $by = $H * $SS - 6
$stem = @(); $stemL = @(); $stemR = @()
$N = 22
for ($i = 0; $i -le $N; $i++) {
    $f = $i / [double]$N
    $x = $bx + [math]::Sin($f * 4.2) * 26 * $f
    $y = $by - $f * ($H * $SS - 30)
    # NOTE: PowerShell variables are case insensitive, so this must not be $w
    # or it would clobber the canvas width $W.
    $halfWidth = 11 * (1 - 0.72 * $f)
    $stem += ,@($x, $y)
    $stemL += ,(P ($x - $halfWidth) $y)
    $stemR += ,(P ($x + $halfWidth) $y)
}
[array]::Reverse($stemR)
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$path.AddClosedCurve([System.Drawing.PointF[]]($stemL + $stemR), 0.3)
$g.FillPath((Brush (C 108 158 122)), $path)
$path.Dispose()
# blades alternating off the stem
for ($i = 3; $i -lt $N; $i += 3) {
    $side = if ($i % 2 -eq 0) { 1 } else { -1 }
    $node = $stem[$i]
    $bladeAngle = if ($side -eq 1) { -35 } else { -145 }
    Ribbon $g (C 128 182 140) $node[0] $node[1] $bladeAngle ($side * 0.5) 0 (60 - $i) 9 2
}
Finish $bmp $g $W $H "kelp.png"

# =====================================================================
#  FISH  -  top-down silhouette facing right, tinted in code
# =====================================================================
$W = 46; $H = 22
$r = NewCanvas ($W * $SS) ($H * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $H * $SS / 2.0
# tail
Blob $g @( (P 14 $cy), (P 46 ($cy - 26)), (P 62 $cy), (P 46 ($cy + 26)) ) (C 150 178 190)
# body
Blob $g @( (P 172 $cy), (P 150 ($cy - 17)), (P 110 ($cy - 21)), (P 70 ($cy - 14)),
           (P  48 $cy), (P  70 ($cy + 14)), (P 110 ($cy + 21)), (P 150 ($cy + 17)) ) (C 176 202 212)
# side fins
Blob $g @( (P 108 ($cy - 16)), (P 96 ($cy - 34)), (P 84 ($cy - 14)) ) (C 150 178 190)
Blob $g @( (P 108 ($cy + 16)), (P 96 ($cy + 34)), (P 84 ($cy + 14)) ) (C 150 178 190)
Disc $g (C 60 74 84) 150 ($cy - 5) 5
Finish $bmp $g $W $H "fish.png"

# =====================================================================
#  SPITTER  -  the ranged enemy: spiked urchin with a violet core
# =====================================================================
$W = 76; $H = 76
$r = NewCanvas ($W * $SS) ($H * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $W * $SS / 2.0; $cy = $H * $SS / 2.0
$penOutline = New-Object System.Drawing.Pen -ArgumentList (C 12 6 18), 6.0
$penOutline.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

for ($i = 0; $i -lt 14; $i++) {
    $a = 2.0 * [math]::PI * $i / 14.0
    $inner = 74; $outer = 132
    $spread = 0.11
    $tri = @(
        (P ($cx + [math]::Cos($a - $spread) * $inner) ($cy + [math]::Sin($a - $spread) * $inner)),
        (P ($cx + [math]::Cos($a) * $outer)           ($cy + [math]::Sin($a) * $outer)),
        (P ($cx + [math]::Cos($a + $spread) * $inner) ($cy + [math]::Sin($a + $spread) * $inner))
    )
    $sp = New-Object System.Drawing.Drawing2D.GraphicsPath
    $sp.AddPolygon([System.Drawing.PointF[]]$tri)
    $g.FillPath((Brush (C 46 26 60)), $sp)
    $sp.Dispose()
}

Disc $g (C 12 6 18)  $cx $cy 88
Disc $g (C 58 32 74) $cx $cy 82
Disc $g (C 40 20 54) $cx $cy 62

# The snout it spits from. It has to reach past the body radius, otherwise the
# creature reads as symmetrical and you cannot tell where it is aiming.
$snout = @( (P 130 ($cy - 34)), (P 214 ($cy - 20)), (P 232 $cy), (P 214 ($cy + 20)), (P 130 ($cy + 34)) )
$sp = New-Object System.Drawing.Drawing2D.GraphicsPath
$sp.AddClosedCurve([System.Drawing.PointF[]]$snout, 0.3)
$g.FillPath((Brush (C 74 40 92)), $sp)
$g.DrawPath($penOutline, $sp)
$sp.Dispose()

Disc $g (C 226 120 255 80)  212 $cy 40
Disc $g (C 226 120 255 210) 212 $cy 21
Disc $g (C 250 220 255 255) 212 $cy 10
Disc $g (C 226 120 255 90)  $cx $cy 26
Disc $g (C 250 215 255 210) $cx $cy 12
Finish $bmp $g $W $H "enemy_spitter.png"

# =====================================================================
#  HOSTILE ORB  -  violet, so it never reads as one of the player's jets
# =====================================================================
$W = 26; $H = 26
$r = NewCanvas ($W * $SS) ($H * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $W * $SS / 2.0; $cy = $H * $SS / 2.0
Disc $g (C 170 60 220 60)   $cx $cy 50
Disc $g (C 200 90 240 130)  $cx $cy 36
Disc $g (C 228 130 255 220) $cx $cy 24
Disc $g (C 250 220 255 255) $cx $cy 12
Finish $bmp $g $W $H "enemy_orb.png"

"done"
