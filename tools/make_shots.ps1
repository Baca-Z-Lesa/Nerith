Add-Type -AssemblyName System.Drawing

# What each creature throws. Nothing here is a recoloured ball: a spitter spits
# something caustic, a bloom sheds stingers, a weaver rakes filaments, a husk
# comes apart into pieces of itself, and each boss throws something that could
# only have come out of that particular animal. Facing right, 4x, downsampled.
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
function Line($g, $col, $x0, $y0, $x1, $y1, $w) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, $w
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawLine($pen, [float]$x0, [float]$y0, [float]$x1, [float]$y1)
    $pen.Dispose()
}
function Poly($g, $pts, $fill) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddPolygon([System.Drawing.PointF[]]$pts)
    $g.FillPath((Brush $fill), $path)
    $path.Dispose()
}
function Blob($g, $pts, $fill) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, 0.6)
    $g.FillPath((Brush $fill), $path)
    $path.Dispose()
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
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, ($outW * 4), ($outH * 4)))) {
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

# A soft halo so every shot reads through the dark without a light source.
function Halo($g, $cx, $cy, $rad, $col) {
    for ($i = 6; $i -ge 1; $i--) {
        $t = $i / 6.0
        $a = [int](34 * (1.0 - $t) * (1.0 - $t) * 3.0)
        Disc $g (C $col.R $col.G $col.B $a) $cx $cy ($rad * $t * 1.6)
    }
}

"generating shots..."

# =====================================================================
#  SPITTER  -  a gob of something caustic, still coming apart in flight
# =====================================================================
$outW = 26; $outH = 20
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $outW * $SS * 0.56; $cy = $outH * $SS / 2.0

Halo $g $cx $cy 26 (C 180 240 120)
Blob $g @( (P ($cx + 30) $cy), (P ($cx + 6) ($cy - 26)), (P ($cx - 26) ($cy - 14)),
           (P ($cx - 34) $cy), (P ($cx - 22) ($cy + 18)), (P ($cx + 8) ($cy + 24)) ) `
    (C 126 196 78)
Blob $g @( (P ($cx + 20) $cy), (P ($cx + 2) ($cy - 16)), (P ($cx - 16) ($cy - 6)),
           (P ($cx - 12) ($cy + 12)), (P ($cx + 6) ($cy + 14)) ) (C 190 246 130)
Disc $g (C 236 255 208) ($cx + 4) ($cy - 4) 7
# the tail it is shedding
Disc $g (C 126 196 78 170) ($cx - 44) ($cy + 6) 6
Disc $g (C 126 196 78 120) ($cx - 56) ($cy - 4) 4
Finish $bmp $g $outW $outH "shot_spitter.png"

# =====================================================================
#  BLOOM  -  a stinger. A barbed seed, spinning as it drifts.
# =====================================================================
$outW = 22; $outH = 22
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

Halo $g $c $c 20 (C 196 150 255)
for ($i = 0; $i -lt 6; $i++) {
    $a = 2 * [math]::PI * $i / 6.0
    Poly $g @( (P ($c + [math]::Cos($a) * 36) ($c + [math]::Sin($a) * 36)),
               (P ($c + [math]::Cos($a + 0.5) * 13) ($c + [math]::Sin($a + 0.5) * 13)),
               (P ($c + [math]::Cos($a - 0.5) * 13) ($c + [math]::Sin($a - 0.5) * 13)) ) `
        (C 168 122 236)
}
Disc $g (C 196 154 252) $c $c 15
Disc $g (C 246 228 255) ($c - 3) ($c - 3) 7
Finish $bmp $g $outW $outH "shot_bloom.png"

# =====================================================================
#  WEAVER  -  a filament. Long, thin, and it cuts rather than hits.
# =====================================================================
$outW = 34; $outH = 12
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0

Halo $g ($outW * $SS * 0.5) $cy 10 (C 130 240 226)
Line $g (C 86 206 196 150) 8 $cy ($outW * $SS - 8) $cy 9
Line $g (C 168 250 240) 26 $cy ($outW * $SS - 10) $cy 4
Poly $g @( (P ($outW * $SS - 4) $cy), (P ($outW * $SS - 26) ($cy - 11)),
           (P ($outW * $SS - 26) ($cy + 11)) ) (C 214 255 250)
Finish $bmp $g $outW $outH "shot_weaver.png"

# =====================================================================
#  HUSK  -  a shard of the thing that just died. Ragged, not glowing.
# =====================================================================
$outW = 20; $outH = 18
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cx = $outW * $SS / 2.0; $cy = $outH * $SS / 2.0

Halo $g $cx $cy 14 (C 200 120 90)
Poly $g @( (P ($cx + 28) ($cy - 4)), (P ($cx + 2) ($cy - 26)), (P ($cx - 24) ($cy - 8)),
           (P ($cx - 14) ($cy + 22)), (P ($cx + 14) ($cy + 18)) ) (C 132 74 58)
Poly $g @( (P ($cx + 14) ($cy - 2)), (P ($cx - 2) ($cy - 14)), (P ($cx - 12) ($cy + 2)),
           (P ($cx - 2) ($cy + 12)) ) (C 196 116 84)
Disc $g (C 252 186 120) ($cx + 1) ($cy - 1) 5
Finish $bmp $g $outW $outH "shot_husk.png"

# =====================================================================
#  LEVIATHAN  -  a slug of compressed water, driven rather than thrown
# =====================================================================
$outW = 30; $outH = 16
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$cy = $outH * $SS / 2.0

Halo $g ($outW * $SS * 0.5) $cy 16 (C 150 240 230)
Blob $g @( (P ($outW * $SS - 6) $cy), (P ($outW * $SS * 0.6) ($cy - 22)),
           (P 22 ($cy - 9)), (P 6 $cy), (P 22 ($cy + 9)),
           (P ($outW * $SS * 0.6) ($cy + 22)) ) (C 70 172 168)
Blob $g @( (P ($outW * $SS - 16) $cy), (P ($outW * $SS * 0.55) ($cy - 11)),
           (P 30 $cy), (P ($outW * $SS * 0.55) ($cy + 11)) ) (C 168 244 236)
Finish $bmp $g $outW $outH "shot_leviathan.png"

# =====================================================================
#  SOVEREIGN  -  a stinger the size of a fist, trailing its own thread
# =====================================================================
$outW = 24; $outH = 24
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

Halo $g $c $c 22 (C 220 140 255)
for ($i = 0; $i -lt 10; $i++) {
    $a = 2 * [math]::PI * $i / 10.0
    Line $g (C 190 118 240 190) ($c + [math]::Cos($a) * 12) ($c + [math]::Sin($a) * 12) `
        ($c + [math]::Cos($a) * 40) ($c + [math]::Sin($a) * 40) 6
}
Disc $g (C 202 128 248) $c $c 20
Disc $g (C 252 222 255) ($c - 4) ($c - 4) 9
Finish $bmp $g $outW $outH "shot_sovereign.png"

# =====================================================================
#  TRENCH WORM  -  scalding silt. A hot, dirty cloud with grit in it.
# =====================================================================
$outW = 24; $outH = 22
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0; $cy = $outH * $SS / 2.0

Halo $g $c $cy 22 (C 255 168 84)
Blob $g @( (P ($c + 34) $cy), (P ($c + 10) ($cy - 30)), (P ($c - 24) ($cy - 18)),
           (P ($c - 36) ($cy + 6)), (P ($c - 10) ($cy + 30)), (P ($c + 20) ($cy + 20)) ) `
    (C 176 92 44)
Blob $g @( (P ($c + 18) $cy), (P ($c + 2) ($cy - 16)), (P ($c - 16) ($cy - 4)),
           (P ($c - 6) ($cy + 16)), (P ($c + 12) ($cy + 10)) ) (C 246 160 70)
Disc $g (C 255 232 176) ($c + 2) ($cy - 2) 8
for ($i = 0; $i -lt 5; $i++) {
    $a = 2 * [math]::PI * $i / 5.0 + 0.6
    Disc $g (C 60 34 22) ($c + [math]::Cos($a) * 20) ($cy + [math]::Sin($a) * 18) 4
}
Finish $bmp $g $outW $outH "shot_worm.png"

# =====================================================================
#  THE HOLLOW  -  a mote of its own lamplight, cold and perfectly round
# =====================================================================
$outW = 20; $outH = 20
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

Halo $g $c $c 24 (C 150 255 238)
Ring $g (C 132 240 224 160) $c $c 28 5
Ring $g (C 180 252 240 200) $c $c 19 4
Disc $g (C 214 255 248) $c $c 11
Disc $g (C 255 255 255) $c $c 5
Finish $bmp $g $outW $outH "shot_hollow.png"
