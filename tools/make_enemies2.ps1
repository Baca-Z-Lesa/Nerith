Add-Type -AssemblyName System.Drawing

# The second wave of creatures. Same conventions as make_enemies.ps1: everything
# faces right, and the sprite is drawn at 4x then downsampled.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function P($x, $y) { New-Object System.Drawing.PointF -ArgumentList ([float]$x, [float]$y) }
function C($r, $g, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Brush($col) { New-Object System.Drawing.SolidBrush -ArgumentList $col }
function RectF($x, $y, $w, $h) { New-Object System.Drawing.RectangleF -ArgumentList ([float]$x, [float]$y, [float]$w, [float]$h) }
function Disc($g, $col, $cx, $cy, $rad) { $g.FillEllipse((Brush $col), (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2))) }

function NewCanvas($w, $h) {
    $bmp = New-Object System.Drawing.Bitmap -ArgumentList $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    return @($bmp, $g)
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

function Blob($g, $pts, $fill, $edge = $null, $width = 0.0) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, 0.5)
    if ($null -ne $fill) { $g.FillPath((Brush $fill), $path) }
    if ($null -ne $edge) {
        $pen = New-Object System.Drawing.Pen -ArgumentList $edge, $width
        $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($pen, $path)
        $pen.Dispose()
    }
    $path.Dispose()
}

function Poly($g, $pts, $fill, $edge = $null, $width = 0.0) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddPolygon([System.Drawing.PointF[]]$pts)
    if ($null -ne $fill) { $g.FillPath((Brush $fill), $path) }
    if ($null -ne $edge) {
        $pen = New-Object System.Drawing.Pen -ArgumentList $edge, $width
        $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($pen, $path)
        $pen.Dispose()
    }
    $path.Dispose()
}

$ink = C 10 14 12

"generating second wave creatures..."

# =====================================================================
#  SPAWNLING  -  barely a creature. One jet and it is gone.
# =====================================================================
$outW = 28; $outH = 20
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0
Poly $g @( (P 8 $cy), (P 40 ($cy - 20)), (P 40 ($cy + 20)) ) (C 118 150 96)
Blob $g @( (P 100 $cy), (P 78 ($cy - 22)), (P 44 ($cy - 16)), (P 30 $cy),
           (P 44 ($cy + 16)), (P 78 ($cy + 22)) ) (C 150 186 118) $ink 5
Disc $g (C 220 240 170 220) 78 ($cy - 8) 7
Disc $g (C 220 240 170 220) 78 ($cy + 8) 7
Disc $g (C 20 26 18) 80 ($cy - 8) 3.5
Disc $g (C 20 26 18) 80 ($cy + 8) 3.5
Finish $bmp $g $outW $outH "enemy_spawnling.png"

# =====================================================================
#  BROODER  -  a sac with eggs showing through it
# =====================================================================
$outW = 98; $outH = 88
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $outW * $SS / 2.0; $cy = $outH * $SS / 2.0

# stubby legs, drawn first so the sac covers their roots
foreach ($side in @(-1, 1)) {
    foreach ($a in @(150, 200, 250)) {
        $rad = [math]::PI * $a / 180.0
        $x = $cx + [math]::Cos($rad) * 120
        $y = $cy + [math]::Sin($rad) * $side * 92
        Poly $g @( (P $x $y), (P ($x - 54) ($y + $side * 44)), (P ($x - 16) ($y + $side * 58)) ) (C 74 82 48)
    }
}

Blob $g @( (P 328 $cy), (P 286 ($cy - 118)), (P 168 ($cy - 156)), (P 62 ($cy - 96)),
           (P 40 $cy), (P 62 ($cy + 96)), (P 168 ($cy + 156)), (P 286 ($cy + 118)) ) (C 126 138 78) $ink 8

# eggs inside, the tell that it is a source
$rng = New-Object System.Random 4242
for ($i = 0; $i -lt 9; $i++) {
    $a = 2.0 * [math]::PI * $rng.NextDouble()
    $d = 96 * [math]::Sqrt($rng.NextDouble())
    Disc $g (C 178 200 116 210) ($cx + [math]::Cos($a) * $d * 1.15) ($cy + [math]::Sin($a) * $d) (18 + 10 * $rng.NextDouble())
}
Disc $g (C 214 236 150 120) ($cx + 40) $cy 62
Disc $g (C 40 48 26) 296 $cy 26
Disc $g (C 226 250 160) 302 $cy 12
Finish $bmp $g $outW $outH "enemy_brooder.png"

# =====================================================================
#  LANCER  -  a needle that commits
# =====================================================================
$outW = 78; $outH = 28
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0

foreach ($side in @(-1, 1)) {
    Poly $g @( (P 120 ($cy + $side * 12)), (P 54 ($cy + $side * 46)), (P 22 ($cy + $side * 14)) ) (C 88 104 126)
}
Poly $g @( (P 308 $cy), (P 176 ($cy - 12)), (P 176 ($cy + 12)) ) (C 214 228 240)
Blob $g @( (P 200 $cy), (P 164 ($cy - 26)), (P 96 ($cy - 24)), (P 20 $cy),
           (P 96 ($cy + 24)), (P 164 ($cy + 26)) ) (C 120 142 168) $ink 5
Disc $g (C 255 90 80 90) 150 $cy 20
Disc $g (C 255 110 90) 150 $cy 10
Finish $bmp $g $outW $outH "enemy_lancer.png"

# =====================================================================
#  WEAVER  -  a ring of legs around one eye
# =====================================================================
$outW = 68; $outH = 68
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $outW * $SS / 2.0; $cy = $outH * $SS / 2.0

for ($i = 0; $i -lt 8; $i++) {
    $a = 2.0 * [math]::PI * $i / 8.0 + 0.2
    for ($j = 0; $j -le 12; $j++) {
        $f = $j / 12.0
        $d = 52 + $f * 74
        $wob = [math]::Sin($f * 5.0 + $i) * 0.2 * $f
        Disc $g (C 62 92 108) ($cx + [math]::Cos($a + $wob) * $d) ($cy + [math]::Sin($a + $wob) * $d) (11 * (1 - 0.72 * $f))
    }
}
Disc $g $ink $cx $cy 74
Disc $g (C 92 124 142) $cx $cy 66
Disc $g (C 58 84 100) $cx $cy 46
Disc $g (C 190 130 255 90) ($cx + 14) $cy 34
Disc $g (C 206 150 255) ($cx + 16) $cy 18
Disc $g (C 250 235 255) ($cx + 18) $cy 8
Finish $bmp $g $outW $outH "enemy_weaver.png"

# =====================================================================
#  HUSK  -  a cracked shell full of pressure
# =====================================================================
$outW = 62; $outH = 56
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $outW * $SS / 2.0; $cy = $outH * $SS / 2.0

Blob $g @( (P 218 $cy), (P 186 ($cy - 78)), (P 96 ($cy - 92)), (P 30 ($cy - 40)),
           (P 26 $cy), (P 30 ($cy + 40)), (P 96 ($cy + 92)), (P 186 ($cy + 78)) ) (C 96 104 88) $ink 8

# seams, glowing because there is something behind them
foreach ($seam in @(@(70, -70, 150, -14), @(60, 34, 140, 84), @(150, -6, 206, 30))) {
    $pen = New-Object System.Drawing.Pen -ArgumentList (C 255 150 70 230), 9.0
    $g.DrawLine($pen, ($cx + $seam[0] - 124), ($cy + $seam[1]), ($cx + $seam[2] - 124), ($cy + $seam[3]))
    $pen.Dispose()
}
Disc $g (C 255 150 70 60) $cx $cy 58
Disc $g (C 40 34 28) 190 $cy 20
Disc $g (C 255 190 110) 192 $cy 9
Finish $bmp $g $outW $outH "enemy_husk.png"

# =====================================================================
#  ANGLER  -  all maw, with a lure hanging in front of it
# =====================================================================
$outW = 76; $outH = 44
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0

foreach ($side in @(-1, 1)) {
    Poly $g @( (P 92 ($cy + $side * 26)), (P 30 ($cy + $side * 76)), (P 14 ($cy + $side * 22)) ) (C 24 28 38)
}
Blob $g @( (P 226 $cy), (P 196 ($cy - 64)), (P 108 ($cy - 70)), (P 26 ($cy - 22)),
           (P 12 $cy), (P 26 ($cy + 22)), (P 108 ($cy + 70)), (P 196 ($cy + 64)) ) (C 32 38 50) $ink 7

# The body is a profile, with the lure arcing over the top, so the mouth has to
# be a profile too: an upper and a lower jaw hinged at the back, not a ring of
# teeth around a hole seen from above.
$hinge = 138.0
$jut = 262.0   # the lower jaw juts further forward, the way an angler's does

# gullet first, so both jaws close over it
Poly $g @( (P $hinge ($cy - 8)), (P 236 ($cy - 62)), (P $jut ($cy + 44)), (P $hinge ($cy + 20)) ) (C 4 6 9)

# upper jaw: a wedge above the gullet, teeth hanging down from its edge
Poly $g @( (P $hinge ($cy - 14)), (P 232 ($cy - 76)), (P 248 ($cy - 52)), (P $hinge ($cy + 2)) ) (C 30 36 48) $ink 6
for ($i = 0; $i -lt 6; $i++) {
    $t = $i / 5.0
    $x = $hinge + $t * 100
    $y = ($cy - 12) - $t * 50
    Poly $g @( (P ($x - 9) $y), (P ($x + 3) ($y + 26 - $t * 6)), (P ($x + 9) $y) ) (C 214 222 226)
}

# lower jaw: hinged at the same point but thrown further forward and open
Poly $g @( (P $hinge ($cy + 4)), (P $jut ($cy + 36)), (P ($jut - 14) ($cy + 58)), (P $hinge ($cy + 26)) ) (C 26 32 42) $ink 6
for ($i = 0; $i -lt 6; $i++) {
    $t = $i / 5.0
    $x = $hinge + $t * 116
    $y = ($cy + 8) + $t * 34
    Poly $g @( (P ($x - 9) $y), (P ($x + 3) ($y - 24 + $t * 5)), (P ($x + 9) $y) ) (C 200 210 214)
}

# the lure: a stalk arcing forward with a light on the end
$pen = New-Object System.Drawing.Pen -ArgumentList (C 40 48 60), 8.0
$g.DrawBezier($pen, (P 150 ($cy - 56)), (P 240 ($cy - 116)), (P 288 ($cy - 78)), (P 282 ($cy - 30)))
$pen.Dispose()
Disc $g (C 190 235 255 70) 282 ($cy - 30) 34
Disc $g (C 170 230 255 190) 282 ($cy - 30) 16
Disc $g (C 250 255 255) 282 ($cy - 30) 7
Disc $g (C 255 120 90 200) 176 ($cy - 22) 11
Finish $bmp $g $outW $outH "enemy_angler.png"

"done"
