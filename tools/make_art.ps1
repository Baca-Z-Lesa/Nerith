Add-Type -AssemblyName System.Drawing

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

function Save-Scaled($src, $w, $h, $path) {
    $dst = New-Object System.Drawing.Bitmap -ArgumentList $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gd = [System.Drawing.Graphics]::FromImage($dst)
    $gd.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gd.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $gd.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $gd.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $gd.DrawImage($src, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $w, $h))
    $gd.Dispose()
    $dir = Split-Path -Parent $path
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $dst.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose()
}

function Blob($g, $pts, $col) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, 0.5)
    $g.FillPath((Brush $col), $path)
    $path.Dispose()
}

# =====================================================================
#  WATER JET  -  a slug of pressurised water, blunt front, wispy tail
# =====================================================================
$JW = 48; $JH = 24
$r = NewCanvas ($JW * $SS) ($JH * $SS); $bmp = $r[0]; $g = $r[1]

# trailing bubbles first, so the jet body sits on top of them
foreach ($b in @(@(26,30,7), @(12,62,5.5), @(48,70,4.5), @(60,24,4))) {
    Disc $g (C 168 240 232 60) $b[0] $b[1] $b[2]
    $penB = New-Object System.Drawing.Pen -ArgumentList (C 200 250 244 150), 2.0
    $g.DrawEllipse($penB, (RectF ($b[0] - $b[2]) ($b[1] - $b[2]) ($b[2] * 2) ($b[2] * 2)))
    $penB.Dispose()
}

Blob $g @( (P 188 48), (P 178 28), (P 158 22), (P 122 27), (P 82 35), (P 44 43),
           (P  22 48), (P  44 53), (P  82 61), (P 122 69), (P 158 74), (P 178 68) ) (C 122 214 205 75)

Blob $g @( (P 181 48), (P 172 33), (P 154 29), (P 122 33), (P 86 40), (P 52 45),
           (P  32 48), (P  52 51), (P  86 56), (P 122 63), (P 154 67), (P 172 63) ) (C 158 238 231 140)

Blob $g @( (P 174 48), (P 166 38), (P 152 35), (P 124 38), (P 96 43), (P 70 46),
           (P  54 48), (P  70 50), (P  96 53), (P 124 58), (P 152 61), (P 166 58) ) (C 232 255 253 240)

$g.Dispose()
Save-Scaled $bmp $JW $JH "$TexDir\water_jet.png"
Save-Scaled $bmp ($JW * 6) ($JH * 6) "$PreviewDir\preview_jet.png"
$bmp.Dispose()

# =====================================================================
#  LURKER  -  deep sea creature: dark body, tentacles, one glowing eye
# =====================================================================
$EW = 80; $EH = 80
$r = NewCanvas ($EW * $SS) ($EH * $SS); $bmp = $r[0]; $g = $r[1]
$cx = 160.0; $cy = 160.0

$penOutline = New-Object System.Drawing.Pen -ArgumentList (C 10 6 14), 6.0
$penOutline.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

# --- tentacles: a chain of shrinking discs curling backwards ---
# each entry: base angle in degrees (180 = straight back), curl direction
# A chain of discs leaves a beaded silhouette once they shrink, so build a real
# tapered ribbon: walk the curve, offset each step along its normal, close it.
function Tentacle($g, $col, $cx, $cy, $baseDeg, $curl, $len, $w0, $w1) {
    $N = 14
    $px = @(); $py = @(); $ww = @()
    for ($i = 0; $i -le $N; $i++) {
        $f = $i / [double]$N
        $ang = ([math]::PI * $baseDeg / 180.0) + $curl * $f * $f
        $d = 50 + $f * $len
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
}

foreach ($t in @(@(133, -0.42), @(153, -0.26), @(172, -0.09), @(190, 0.09), @(209, 0.26), @(229, 0.42))) {
    Tentacle $g (C 52 26 46) $cx $cy $t[0] $t[1] 98 14 2.0
}

# --- body ---
$bodyPts = @( (P 268 160), (P 244 120), (P 206 100), (P 158 100), (P 112 120),
              (P  88 160), (P 112 200), (P 158 220), (P 206 220), (P 244 200) )
$bodyPath = New-Object System.Drawing.Drawing2D.GraphicsPath
$bodyPath.AddClosedCurve([System.Drawing.PointF[]]$bodyPts, 0.5)
$bodyBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush -ArgumentList `
    (P 0 90), (P 0 230), (C 86 44 72), (C 34 16 32)
$g.FillPath($bodyBrush, $bodyPath)
$g.DrawPath($penOutline, $bodyPath)
$bodyPath.Dispose()

# --- a pale membrane fin sweeping over the back and belly ---
foreach ($fin in @(
    ,@((P 130 108), (P 190 104), (P 214 68), (P 154 76))
    ,@((P 130 212), (P 190 216), (P 214 252), (P 154 244))
)) {
    $fp = New-Object System.Drawing.Drawing2D.GraphicsPath
    $fp.AddClosedCurve([System.Drawing.PointF[]]$fin, 0.4)
    $g.FillPath((Brush (C 62 30 54)), $fp)
    $g.DrawPath($penOutline, $fp)
    $fp.Dispose()
}

# --- the eye: the only bright thing on it ---
Disc $g (C 255 110 60 55)  222 160 44
Disc $g (C 255 120 70 110) 222 160 30
Disc $g (C 255 150 80 255) 222 160 19
Disc $g (C 255 232 190 255) 222 160 9

$g.Dispose()
Save-Scaled $bmp $EW $EH "$TexDir\enemy_lurker.png"
Save-Scaled $bmp ($EW * 4) ($EH * 4) "$PreviewDir\preview_lurker.png"
$bmp.Dispose()

"done: water_jet.png ($JW x $JH), enemy_lurker.png ($EW x $EH)"

