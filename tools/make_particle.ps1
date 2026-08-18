Add-Type -AssemblyName System.Drawing

# A soft round dot. Every particle in the game is this texture tinted and scaled,
# which keeps the whole system down to a single draw call.
$OutPath = "$PSScriptRoot\..\assets\textures\particle_dot.png"
$N = 64
$c = ($N - 1) / 2.0

$bmp = New-Object System.Drawing.Bitmap -ArgumentList $N, $N, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

for ($y = 0; $y -lt $N; $y++) {
    for ($x = 0; $x -lt $N; $x++) {
        $dx = ($x - $c) / $c
        $dy = ($y - $c) / $c
        $d = [math]::Sqrt($dx * $dx + $dy * $dy)

        $f = 1.0 - $d
        if ($f -lt 0) { $f = 0 }
        # Squared falloff reads as a glow rather than a flat disc.
        $a = [int]([math]::Round($f * $f * 255))
        if ($a -gt 255) { $a = 255 }

        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($a, 255, 255, 255))
    }
}

$dir = Split-Path -Parent $OutPath
if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
$bmp.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

"Saved $OutPath ($N x $N)"
