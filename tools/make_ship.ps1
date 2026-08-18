Add-Type -AssemblyName System.Drawing

# Attachments bolted onto the sub as upgrades are installed. They share the
# hull's palette so a fitted-out sub still reads as one machine, and every one
# of them points right, like every other directional sprite in the project.
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
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, ($outW * 4), ($outH * 4)))) {
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

function Poly($g, $pts, $fill, $edge, $width, $tension = 0.4) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, $tension)
    if ($null -ne $fill) { $g.FillPath((Brush $fill), $path) }
    if ($null -ne $edge) {
        $pen = New-Object System.Drawing.Pen -ArgumentList $edge, $width
        $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
        $g.DrawPath($pen, $path)
        $pen.Dispose()
    }
    $path.Dispose()
}

$hull   = C 62 96 112
$hullHi = C 96 138 156
$ink    = C 12 24 31
$accent = C 122 214 205
$glow   = C 168 240 232

"generating ship attachments..."

# --- heavy cannon: one thick barrel over the spine ----------------------
$outW = 60; $outH = 26
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0
Poly $g @( (P 16 ($cy - 40)), (P 150 ($cy - 44)), (P 150 ($cy + 44)), (P 16 ($cy + 40)) ) $hull $ink 8 0.1
Poly $g @( (P 140 ($cy - 26)), (P 226 ($cy - 22)), (P 226 ($cy + 22)), (P 140 ($cy + 26)) ) $hullHi $ink 7 0.1
Disc $g (C 122 214 205 90) 226 $cy 26
Disc $g $glow 226 $cy 11
Finish $bmp $g $outW $outH "att_cannon.png"

# --- twin pods: two smaller barrels either side -------------------------
$outW = 52; $outH = 44
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0
foreach ($side in @(-1, 1)) {
    $oy = $cy + $side * 54
    Poly $g @( (P 14 ($oy - 22)), (P 130 ($oy - 18)), (P 130 ($oy + 18)), (P 14 ($oy + 22)) ) $hull $ink 7 0.1
    Poly $g @( (P 122 ($oy - 11)), (P 190 ($oy - 9)), (P 190 ($oy + 9)), (P 122 ($oy + 11)) ) $hullHi $ink 6 0.1
    Disc $g $glow 190 $oy 8
}
Finish $bmp $g $outW $outH "att_twin.png"

# --- torpedo tube -------------------------------------------------------
$outW = 54; $outH = 24
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0
Poly $g @( (P 12 ($cy - 38)), (P 176 ($cy - 34)), (P 176 ($cy + 34)), (P 12 ($cy + 38)) ) (C 48 76 90) $ink 8 0.1
foreach ($bx in @(52, 92, 132)) {
    $pen = New-Object System.Drawing.Pen -ArgumentList (C 26 44 54), 6.0
    $g.DrawLine($pen, $bx, ($cy - 32), $bx, ($cy + 32))
    $pen.Dispose()
}
Disc $g (C 20 34 42) 180 $cy 30
Disc $g (C 226 130 90 200) 184 $cy 16
Finish $bmp $g $outW $outH "att_torpedo.png"

# --- sonar dish ---------------------------------------------------------
$outW = 38; $outH = 38
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0
Disc $g $ink    $c $c 68
Disc $g $hull   $c $c 58
for ($i = 0; $i -lt 3; $i++) {
    $pen = New-Object System.Drawing.Pen -ArgumentList (C 122 214 205 (180 - $i * 45)), 7.0
    $g.DrawArc($pen, (RectF ($c - 20 - $i * 16) ($c - 20 - $i * 16) ((20 + $i * 16) * 2) ((20 + $i * 16) * 2)), -52, 104)
    $pen.Dispose()
}
Disc $g $glow $c $c 14
Finish $bmp $g $outW $outH "att_sonar.png"

# --- armour plates draped over the hull ---------------------------------
$outW = 100; $outH = 62
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0
foreach ($side in @(-1, 1)) {
    $pts = @( (P 40 ($cy + $side * 46)), (P 200 ($cy + $side * 92)), (P 330 ($cy + $side * 70)),
              (P 330 ($cy + $side * 26)), (P 190 ($cy + $side * 44)) )
    Poly $g $pts (C 78 106 120) $ink 8
}
foreach ($bx in @(120, 200, 280)) {
    Disc $g (C 150 186 200) $bx ($cy - 62) 9
    Disc $g (C 150 186 200) $bx ($cy + 62) 9
}
Finish $bmp $g $outW $outH "att_armour.png"

# --- bigger engine block at the stern -----------------------------------
$outW = 44; $outH = 44
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0
Poly $g @( (P 30 ($cy - 62)), (P 132 ($cy - 46)), (P 132 ($cy + 46)), (P 30 ($cy + 62)) ) (C 40 66 78) $ink 8 0.1
foreach ($side in @(-1, 1)) {
    Disc $g $ink 46 ($cy + $side * 34) 30
    Disc $g (C 30 54 66) 46 ($cy + $side * 34) 22
    Disc $g (C 130 210 220 170) 40 ($cy + $side * 34) 11
}
Finish $bmp $g $outW $outH "att_engine.png"

# --- escort drone -------------------------------------------------------
$outW = 34; $outH = 24
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0
Poly $g @( (P 116 $cy), (P 76 ($cy - 30)), (P 20 ($cy - 22)), (P 8 $cy), (P 20 ($cy + 22)), (P 76 ($cy + 30)) ) $hull $ink 7
Disc $g (C 122 214 205 110) 92 $cy 20
Disc $g $glow 92 $cy 9
foreach ($side in @(-1, 1)) {
    Poly $g @( (P 40 ($cy + $side * 20)), (P 20 ($cy + $side * 44)), (P 62 ($cy + $side * 34)) ) (C 44 74 88) $ink 5 0.1
}
Finish $bmp $g $outW $outH "att_drone.png"

# --- torpedo projectile -------------------------------------------------
$outW = 42; $outH = 20
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0
foreach ($side in @(-1, 1)) {
    Poly $g @( (P 30 ($cy + $side * 14)), (P 6 ($cy + $side * 42)), (P 56 ($cy + $side * 30)) ) (C 40 66 78) $ink 5 0.1
}
Poly $g @( (P 156 $cy), (P 128 ($cy - 26)), (P 40 ($cy - 24)), (P 20 $cy),
           (P 40 ($cy + 24)), (P 128 ($cy + 26)) ) (C 74 104 118) $ink 7
Poly $g @( (P 158 $cy), (P 124 ($cy - 20)), (P 124 ($cy + 20)) ) (C 226 130 90) $ink 5 0.1
Disc $g (C 122 214 205 140) 30 $cy 16
Finish $bmp $g $outW $outH "torpedo.png"

# --- proximity mine -----------------------------------------------------
$outW = 30; $outH = 30
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0
for ($i = 0; $i -lt 10; $i++) {
    $a = 2 * [math]::PI * $i / 10.0
    Poly $g @(
        (P ($c + [math]::Cos($a - 0.2) * 34) ($c + [math]::Sin($a - 0.2) * 34)),
        (P ($c + [math]::Cos($a) * 56) ($c + [math]::Sin($a) * 56)),
        (P ($c + [math]::Cos($a + 0.2) * 34) ($c + [math]::Sin($a + 0.2) * 34))
    ) (C 46 72 84) $ink 5 0.1
}
Disc $g $ink  $c $c 40
Disc $g (C 54 84 96) $c $c 33
Disc $g (C 226 130 90 200) $c $c 15
Finish $bmp $g $outW $outH "mine.png"

"done"
