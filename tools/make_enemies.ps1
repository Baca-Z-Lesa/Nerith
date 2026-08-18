Add-Type -AssemblyName System.Drawing

# The three enemy types added alongside the lurker and the spitter.
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

function Blob($g, $pts, $col) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, 0.5)
    $g.FillPath((Brush $col), $path)
    $path.Dispose()
}

function Outline($g, $pts, $col, $width) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, 0.5)
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, $width
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($pen, $path)
    $pen.Dispose(); $path.Dispose()
}

"generating enemy textures..."

# =====================================================================
#  DARTER  -  small, fast, fragile. A blade with an eye.
# =====================================================================
$outW = 62; $outH = 30
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0

# swept-back fins first so the body covers their roots
foreach ($fin in @(
    ,@((P 90 $cy), (P 40 ($cy - 46)), (P 8 ($cy - 30)), (P 70 ($cy - 8)))
    ,@((P 90 $cy), (P 40 ($cy + 46)), (P 8 ($cy + 30)), (P 70 ($cy + 8)))
)) { Blob $g $fin (C 96 30 30) }

$body = @( (P 238 $cy), (P 200 ($cy - 20)), (P 130 ($cy - 26)), (P 60 ($cy - 16)),
           (P  20 $cy), (P  60 ($cy + 16)), (P 130 ($cy + 26)), (P 200 ($cy + 20)) )
Blob $g $body (C 150 48 44)
Outline $g $body (C 24 8 10) 6

Disc $g (C 255 150 90 90)  186 $cy 22
Disc $g (C 255 170 110 255) 186 $cy 11
Disc $g (C 255 240 210 255) 188 $cy 5
Finish $bmp $g $outW $outH "enemy_darter.png"

# =====================================================================
#  HULK  -  slow, huge, heavily plated. Reads as something to run from.
# =====================================================================
$outW = 108; $outH = 96
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $outW * $SS / 2.0; $cy = $outH * $SS / 2.0

# Rear legs. Straight polygons, not smoothed curves: a closed curve through
# these points bulges into one big diagonal band instead of six separate legs.
foreach ($side in @(-1, 1)) {
    foreach ($x in @(112, 178, 244)) {
        $pts = [System.Drawing.PointF[]]@(
            (P ($x - 20) ($cy + $side * 92)),
            (P ($x + 20) ($cy + $side * 92)),
            (P ($x - 4)  ($cy + $side * 166)),
            (P ($x - 44) ($cy + $side * 154))
        )
        $path = New-Object System.Drawing.Drawing2D.GraphicsPath
        $path.AddPolygon($pts)
        $g.FillPath((Brush (C 40 54 46)), $path)
        $pen = New-Object System.Drawing.Pen -ArgumentList (C 14 20 18), 6.0
        $g.DrawPath($pen, $path)
        $pen.Dispose(); $path.Dispose()
    }
}

$shell = @( (P 372 $cy), (P 340 ($cy - 96)), (P 250 ($cy - 140)), (P 140 ($cy - 128)),
            (P  60 ($cy - 66)), (P  44 $cy), (P  60 ($cy + 66)), (P 140 ($cy + 128)),
            (P 250 ($cy + 140)), (P 340 ($cy + 96)) )
Blob $g $shell (C 58 76 66)
Outline $g $shell (C 14 20 18) 9

# armour plates
foreach ($plate in @(@(120, 96), @(196, 84), @(268, 68))) {
    $pts = @( (P $plate[0] ($cy - $plate[1])), (P ($plate[0] + 54) ($cy - $plate[1] * 0.8)),
              (P ($plate[0] + 54) ($cy + $plate[1] * 0.8)), (P $plate[0] ($cy + $plate[1])) )
    Blob $g $pts (C 44 60 52)
    Outline $g $pts (C 18 26 22) 5
}

# jaw plate and the dull amber eye
$jaw = @( (P 330 ($cy - 52)), (P 392 ($cy - 22)), (P 392 ($cy + 22)), (P 330 ($cy + 52)) )
Blob $g $jaw (C 70 90 78)
Outline $g $jaw (C 14 20 18) 6
Disc $g (C 255 190 90 70)  356 $cy 34
Disc $g (C 240 168 70 255) 356 $cy 16
Disc $g (C 255 236 190 255) 356 $cy 7
Finish $bmp $g $outW $outH "enemy_hulk.png"

# =====================================================================
#  BLOOM  -  drifting jellyfish, radially symmetric because it spins
#            and fires a full ring of orbs.
# =====================================================================
$outW = 92; $outH = 92
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $outW * $SS / 2.0; $cy = $outH * $SS / 2.0

# trailing tentacles all the way round
for ($i = 0; $i -lt 12; $i++) {
    $a = 2.0 * [math]::PI * $i / 12.0
    for ($j = 0; $j -le 22; $j++) {
        $f = $j / 22.0
        $d = 86 + $f * 88
        $wob = [math]::Sin($f * 6.0 + $i) * 0.18 * $f
        $x = $cx + [math]::Cos($a + $wob) * $d
        $y = $cy + [math]::Sin($a + $wob) * $d
        Disc $g (C 96 70 150 190) $x $y (11 * (1 - 0.8 * $f))
    }
}

Disc $g (C 120 90 190 70)  $cx $cy 108
Disc $g (C 104 78 172 165) $cx $cy 92
Disc $g (C 138 108 205 190) $cx $cy 68

# the bell ribs
for ($i = 0; $i -lt 8; $i++) {
    $a = 2.0 * [math]::PI * $i / 8.0
    $pen = New-Object System.Drawing.Pen -ArgumentList (C 178 150 235 150), 6.0
    $g.DrawLine($pen, ($cx + [math]::Cos($a) * 26), ($cy + [math]::Sin($a) * 26),
                      ($cx + [math]::Cos($a) * 88), ($cy + [math]::Sin($a) * 88))
    $pen.Dispose()
}

Disc $g (C 200 170 255 200) $cx $cy 34
Disc $g (C 245 230 255 255) $cx $cy 18
Finish $bmp $g $outW $outH "enemy_bloom.png"

"done"
