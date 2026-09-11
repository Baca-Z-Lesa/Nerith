Add-Type -AssemblyName System.Drawing

# THE HOLLOW - the creature that guards the abyss. Same conventions as
# make_creatures.ps1: everything faces right, drawn at 4x and downsampled, and
# the relief pass in make_3d.ps1 does all the lighting afterwards.
#
# It is a ring rather than a body: a lamp-bearing crown of bone with nothing
# inside it, trailing a column of hollow vertebrae. The fight is about the four
# lamps on the crown, which are what aims its spiral, and the pale mass behind
# them that is the only living thing left in it.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function P($x, $y) { New-Object System.Drawing.PointF -ArgumentList ([float]$x, [float]$y) }
function C($r, $g, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Brush($col) { New-Object System.Drawing.SolidBrush -ArgumentList $col }
function RectF($x, $y, $w, $h) { New-Object System.Drawing.RectangleF -ArgumentList ([float]$x, [float]$y, [float]$w, [float]$h) }
function Disc($g, $col, $cx, $cy, $rad) { $g.FillEllipse((Brush $col), (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2))) }
function Ring($g, $col, $cx, $cy, $rad, $width) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, $width
    $g.DrawEllipse($pen, (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2)))
    $pen.Dispose()
}

function NewCanvas($w, $h) {
    $bmp = New-Object System.Drawing.Bitmap -ArgumentList $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    return @($bmp, $g)
}

function Finish($bmp, $g, $outW, $outH, $name) {
    $g.Dispose()
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, ($outW * 2), ($outH * 2)))) {
        $dir = $target[0]
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
        $dst = New-Object System.Drawing.Bitmap -ArgumentList $target[1], $target[2], ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $gd = [System.Drawing.Graphics]::FromImage($dst)
        $gd.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $gd.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $gd.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $gd.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
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

"generating the hollow..."

# Bone that has been down here too long: grey-green, not white.
$bone   = C 156 166 152
$boneLo = C 96 108 104
$ink    = C 10 14 16
$lamp   = C 214 246 236
$lampLo = C 74 170 156
$flesh  = C 190 176 188

# =====================================================================
#  CROWN  -  the head. A ring of bone with four lamps on it and a hole
#  through the middle, facing right.
# =====================================================================
$outW = 164; $outH = 152
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $outW * $SS * 0.46
$cy = $outH * $SS / 2.0
$rad = $outH * $SS * 0.40

# The ring itself, thick enough to read as bone rather than as an outline.
Ring $g $boneLo $cx $cy ($rad + 6) 46
Ring $g $bone $cx $cy $rad 34

# Four horns, the front pair longer, so the thing has a facing.
foreach ($h in @(@(-26, 1.28), @(26, 1.28), @(-64, 1.06), @(64, 1.06))) {
    $a = [math]::PI * $h[0] / 180.0
    $len = $rad * $h[1]
    $tipX = $cx + [math]::Cos($a) * $len
    $tipY = $cy + [math]::Sin($a) * $len
    $baseX = $cx + [math]::Cos($a) * ($rad * 0.86)
    $baseY = $cy + [math]::Sin($a) * ($rad * 0.86)
    $nx = -[math]::Sin($a) * 22.0
    $ny = [math]::Cos($a) * 22.0

    Blob $g @( (P ($baseX + $nx) ($baseY + $ny)), (P $tipX $tipY),
               (P ($baseX - $nx) ($baseY - $ny)) ) $bone $ink 5.0 0.2
}

# The jaw plates hanging below the ring, where a face would be.
Blob $g @( (P ($cx + $rad * 0.55) ($cy + $rad * 0.35)), (P ($cx + $rad * 1.12) ($cy + 26)),
           (P ($cx + $rad * 0.62) ($cy + $rad * 0.72)), (P ($cx + $rad * 0.30) ($cy + $rad * 0.58)) ) `
    $boneLo $ink 6.0 0.3

# Teeth along the inside of the ring, pointing inwards at the hole.
for ($i = 0; $i -lt 16; $i++) {
    $a = 2 * [math]::PI * $i / 16.0
    $ox = $cx + [math]::Cos($a) * ($rad - 16)
    $oy = $cy + [math]::Sin($a) * ($rad - 16)
    $ix = $cx + [math]::Cos($a) * ($rad - 46)
    $iy = $cy + [math]::Sin($a) * ($rad - 46)
    $px = -[math]::Sin($a) * 11.0
    $py = [math]::Cos($a) * 11.0
    Blob $g @( (P ($ox + $px) ($oy + $py)), (P $ix $iy), (P ($ox - $px) ($oy - $py)) ) `
        (C 222 228 214) $null 0.0 0.1
}

# The dark through the middle. This is the hole it is named for.
Disc $g (C 6 10 12 236) $cx $cy ($rad - 44)
Ring $g (C 40 64 62 180) $cx $cy ($rad - 44) 10

Finish $bmp $g $outW $outH "hol_crown.png"

# =====================================================================
#  VERTEBRA  -  one link of the column it tows.
# =====================================================================
$outW = 96; $outH = 96
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

Disc $g $boneLo $c $c ($c * 0.82)
Disc $g $bone $c $c ($c * 0.70)

# Spines, four of them, set square so the column reads as jointed.
for ($i = 0; $i -lt 4; $i++) {
    $a = 2 * [math]::PI * $i / 4.0 + 0.78
    $tipX = $c + [math]::Cos($a) * ($c * 0.98)
    $tipY = $c + [math]::Sin($a) * ($c * 0.98)
    $bx = $c + [math]::Cos($a) * ($c * 0.60)
    $by = $c + [math]::Sin($a) * ($c * 0.60)
    $nx = -[math]::Sin($a) * 26.0
    $ny = [math]::Cos($a) * 26.0
    Blob $g @( (P ($bx + $nx) ($by + $ny)), (P $tipX $tipY), (P ($bx - $nx) ($by - $ny)) ) `
        $boneLo $ink 4.0 0.2
}

Disc $g (C 8 12 14 220) $c $c ($c * 0.30)
Ring $g (C 60 92 88 160) $c $c ($c * 0.42) 9
Finish $bmp $g $outW $outH "hol_vertebra.png"

# =====================================================================
#  LAMP  -  the part you break. Four of them ride the crown, and they are
#  what the spiral is aimed with.
# =====================================================================
$outW = 46; $outH = 46
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

Disc $g (C 40 120 116 70)  $c $c ($c * 0.96)
Disc $g $lampLo $c $c ($c * 0.68)
Disc $g $lamp $c $c ($c * 0.40)
Disc $g (C 250 255 252) ($c - $c * 0.12) ($c - $c * 0.12) ($c * 0.17)
Ring $g (C 24 60 58 200) $c $c ($c * 0.70) 7
Finish $bmp $g $outW $outH "hol_lamp.png"

# =====================================================================
#  RIB CAGE  -  the plating over the mass. Has to come off first.
# =====================================================================
$outW = 118; $outH = 92
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$w = $outW * $SS; $h = $outH * $SS

Blob $g @( (P ($w * 0.10) ($h * 0.50)), (P ($w * 0.30) ($h * 0.14)),
           (P ($w * 0.72) ($h * 0.12)), (P ($w * 0.92) ($h * 0.50)),
           (P ($w * 0.72) ($h * 0.88)), (P ($w * 0.30) ($h * 0.86)) ) `
    $boneLo $ink 7.0 0.4

# Ribs across it, with gaps you can see the mass through.
for ($i = 0; $i -lt 5; $i++) {
    $x = $w * (0.22 + 0.14 * $i)
    $pen = New-Object System.Drawing.Pen -ArgumentList $bone, 26.0
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLine($pen, $x, ($h * 0.20), $x, ($h * 0.80))
    $pen.Dispose()
}
Finish $bmp $g $outW $outH "hol_ribs.png"

# =====================================================================
#  THE MASS  -  what is actually alive in there. Pale, veined, and it
#  should look like it does not belong inside all that bone.
# =====================================================================
$outW = 84; $outH = 84
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

Disc $g (C 120 64 108 90) $c $c ($c * 0.98)
Disc $g (C 168 108 150) $c $c ($c * 0.80)
Disc $g $flesh $c $c ($c * 0.62)

# Veins, wandering rather than radial, so it reads as tissue.
for ($i = 0; $i -lt 9; $i++) {
    $a = 2 * [math]::PI * $i / 9.0
    $pen = New-Object System.Drawing.Pen -ArgumentList (C 118 52 96 190), 8.0
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $mx = $c + [math]::Cos($a + 0.4) * ($c * 0.40)
    $my = $c + [math]::Sin($a + 0.4) * ($c * 0.40)
    $g.DrawLine($pen, $c, $c, $mx, $my)
    $g.DrawLine($pen, $mx, $my,
                ($c + [math]::Cos($a - 0.3) * ($c * 0.74)),
                ($c + [math]::Sin($a - 0.3) * ($c * 0.74)))
    $pen.Dispose()
}

Disc $g (C 236 208 228) ($c - $c * 0.10) ($c - $c * 0.10) ($c * 0.26)
Finish $bmp $g $outW $outH "hol_mass.png"
