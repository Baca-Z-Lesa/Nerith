Add-Type -AssemblyName System.Drawing
. "$PSScriptRoot\organic.ps1"

# A salvage pod. The one man-made thing in the trench besides the sub, so it is
# deliberately the wrong shape for down here: straight sides, banded ribs, hard
# corners. It has to be findable in the dark from a long way off, which is what
# the beacon on the lid is for, and it has to read as heavy, which is what the
# skids and the drag bar are for.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function Finish($bmp, $g, $outW, $outH, $name) {
    $g.Dispose()
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, ($outW * 3), ($outH * 3)))) {
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

"generating the salvage pod..."

$outW = 56; $outH = 56
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$w = $outW * $SS
$cx = $w / 2.0
$cy = $outH * $SS / 2.0

$shellLo = C 52 60 68
$shell   = C 96 108 118
$shellHi = C 148 162 172
$strap   = C 168 126 48
$rust    = C 122 76 40

# Skids underneath, so it reads as something resting on the floor rather than
# floating. Drawn first and wider than the body, so they show at both ends.
Poly $g @( (P ($cx - $w * 0.40) ($cy - $w * 0.20)),
           (P ($cx - $w * 0.32) ($cy - $w * 0.26)),
           (P ($cx + $w * 0.32) ($cy - $w * 0.26)),
           (P ($cx + $w * 0.40) ($cy - $w * 0.20)) ) $shellLo
Poly $g @( (P ($cx - $w * 0.40) ($cy + $w * 0.20)),
           (P ($cx - $w * 0.32) ($cy + $w * 0.26)),
           (P ($cx + $w * 0.32) ($cy + $w * 0.26)),
           (P ($cx + $w * 0.40) ($cy + $w * 0.20)) ) $shellLo

# The drum. Chamfered at the corners rather than square: a perfect rectangle
# looks like a texture-atlas mistake at this size.
$bx = $w * 0.33
$by = $w * 0.23
$ch = $w * 0.07
Poly $g @( (P ($cx - $bx + $ch) ($cy - $by)),
           (P ($cx + $bx - $ch) ($cy - $by)),
           (P ($cx + $bx) ($cy - $by + $ch)),
           (P ($cx + $bx) ($cy + $by - $ch)),
           (P ($cx + $bx - $ch) ($cy + $by)),
           (P ($cx - $bx + $ch) ($cy + $by)),
           (P ($cx - $bx) ($cy + $by - $ch)),
           (P ($cx - $bx) ($cy - $by + $ch)) ) $shell

# Ribs along it. The relief pass reads the alpha rather than the colour, so
# these are here to be seen, and the height comes from the plate edges below.
for ($i = -2; $i -le 2; $i++) {
    $x = $cx + $i * ($bx * 0.34)
    Stroke $g $shellLo $x ($cy - $by * 0.92) $x ($cy + $by * 0.92) ($w * 0.022)
    Stroke $g $shellHi ($x + $w * 0.012) ($cy - $by * 0.92) ($x + $w * 0.012) ($cy + $by * 0.92) ($w * 0.010)
}

# End caps, because a drum has two ends and they catch the light differently
# from the barrel.
Poly $g @( (P ($cx - $bx) ($cy - $by + $ch)),
           (P ($cx - $bx + $ch * 1.3) ($cy - $by)),
           (P ($cx - $bx + $ch * 1.3) ($cy + $by)),
           (P ($cx - $bx) ($cy + $by - $ch)) ) $shellHi
Poly $g @( (P ($cx + $bx) ($cy - $by + $ch)),
           (P ($cx + $bx - $ch * 1.3) ($cy - $by)),
           (P ($cx + $bx - $ch * 1.3) ($cy + $by)),
           (P ($cx + $bx) ($cy + $by - $ch)) ) $shellLo

# The band round its middle, and the drag bar on top of that. The bar is the
# part the sub actually takes hold of.
Stroke $g $strap ($cx - $bx * 0.98) ($cy - $by * 0.55) ($cx + $bx * 0.98) ($cy - $by * 0.55) ($w * 0.030)
Stroke $g $rust  ($cx - $bx * 0.98) ($cy + $by * 0.58) ($cx + $bx * 0.98) ($cy + $by * 0.58) ($w * 0.024)

Stroke $g $shellHi ($cx - $bx * 0.42) ($cy - $by * 0.10) ($cx + $bx * 0.42) ($cy - $by * 0.10) ($w * 0.034)
Disc $g $shellLo ($cx - $bx * 0.42) ($cy - $by * 0.10) ($w * 0.030)
Disc $g $shellLo ($cx + $bx * 0.42) ($cy - $by * 0.10) ($w * 0.030)

# The beacon. Everything else about the pod is dull on purpose so that this is
# the only thing on it the eye finds across a dark basin.
Lamp $g $cx ($cy + $by * 0.16) ($w * 0.052) (C 255 196 96)

Finish $bmp $g $outW $outH "cargo.png"
