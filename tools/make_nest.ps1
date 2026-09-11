Add-Type -AssemblyName System.Drawing
. "$PSScriptRoot\organic.ps1"

# A nest. Not a building and not a creature: a mass of egg cases cemented to
# the rock, with something still moving in the middle of it. It has to read as
# a target from a long way off in the dark, which is why the throat glows.
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

"generating the nest..."

$outW = 104; $outH = 104
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

$crust   = C 74 62 48
$crustLo = C 38 32 24
$case    = C 132 146 78
$caseHi  = C 196 206 126

# The cemented base it is stuck to the rock with.
Disc $g $crustLo $c $c ($c * 0.94)
Disc $g $crust $c $c ($c * 0.84)

$rand = New-Object System.Random -ArgumentList 404

# The egg cases: a heap of them, packed and overlapping, biggest at the rim.
for ($i = 0; $i -lt 26; $i++) {
    $a = $rand.NextDouble() * 6.283
    $d = [math]::Sqrt($rand.NextDouble()) * $c * 0.74
    $x = $c + [math]::Cos($a) * $d
    $y = $c + [math]::Sin($a) * $d
    $rad = $c * (0.10 + $rand.NextDouble() * 0.11)

    Disc $g $crustLo $x $y ($rad * 1.16)
    Disc $g $case $x $y $rad
    Disc $g $caseHi ($x - $rad * 0.3) ($y - $rad * 0.3) ($rad * 0.42)
    # The embryo, curled.
    Stroke $g (C 62 72 36 190) ($x - $rad * 0.3) ($y + $rad * 0.1) ($x + $rad * 0.35) ($y - $rad * 0.2) 5
}

# The throat: an opening in the middle that whatever is inside comes out of.
Disc $g (C 20 16 10) $c $c ($c * 0.30)
for ($i = 0; $i -lt 9; $i++) {
    $a = 2 * [math]::PI * $i / 9.0
    Poly $g @( (P ($c + [math]::Cos($a) * ($c * 0.34)) ($c + [math]::Sin($a) * ($c * 0.34))),
               (P ($c + [math]::Cos($a + 0.3) * ($c * 0.16)) ($c + [math]::Sin($a + 0.3) * ($c * 0.16))),
               (P ($c + [math]::Cos($a - 0.3) * ($c * 0.16)) ($c + [math]::Sin($a - 0.3) * ($c * 0.16))) ) `
        (C 106 118 64)
}
Lamp $g $c $c ($c * 0.15) (C 198 240 118)
Finish $bmp $g $outW $outH "nest.png"
