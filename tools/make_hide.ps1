Add-Type -AssemblyName System.Drawing

# The skin the drawn bosses are made of.
#
# Every other creature in the game gets its surface from the relief pass: a
# grain, a rim, and a highlight always from the same corner. A boss that is
# built out of shapes at runtime cannot go through that pass, so it would be
# flat colour next to thirty sprites that are not - which reads as a different
# game in the middle of this one.
#
# So the shapes get a sheet instead. It is near white with the detail cut into
# it darker, because SFML multiplies the texture by the vertex colour: white
# leaves the colour exactly as the art intended it and everything below white
# is grain, scale and shadow on top of that. One sheet serves hide, plate, fin
# and jaw - they differ by tint and by shape, exactly as they do on the
# sprites, and they share a surface for the same reason those all share the
# relief pass.
#
# It tiles, and it tiles the only way that actually works: every mark is drawn
# nine times, once at each wrap offset, so a scale that runs off one edge comes
# back on the other as the same scale. Folding the finished sheet onto itself
# does not work - each copy is opaque and paints out the one before it.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"

$Size = 128
$SS = 4

function C($r, $g, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Brush($col) { New-Object System.Drawing.SolidBrush -ArgumentList $col }
function RectF($x, $y, $w, $h) { New-Object System.Drawing.RectangleF -ArgumentList ([float]$x, [float]$y, [float]$w, [float]$h) }

"generating boss hide..."

$W = $Size * $SS
$bmp = New-Object System.Drawing.Bitmap -ArgumentList $W, $W, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear((C 244 244 244))

$rand = New-Object System.Random -ArgumentList 4242

# One mark, everywhere it would also be if the sheet were laid next to itself.
function Wrapped($g, $col, $cx, $cy, $rx, $ry) {
    foreach ($dx in @(-$W, 0, $W)) {
        foreach ($dy in @(-$W, 0, $W)) {
            $g.FillEllipse((Brush $col), (RectF ($cx + $dx - $rx) ($cy + $dy - $ry) ($rx * 2) ($ry * 2)))
        }
    }
}

function WrappedArc($g, $col, $cx, $cy, $rx, $ry, $from, $sweep, $width) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, ([float]$width)
    foreach ($dx in @(-$W, 0, $W)) {
        foreach ($dy in @(-$W, 0, $W)) {
            $g.DrawArc($pen, (RectF ($cx + $dx - $rx) ($cy + $dy - $ry) ($rx * 2) ($ry * 2)),
                       [float]$from, [float]$sweep)
        }
    }
    $pen.Dispose()
}

# --- scales -----------------------------------------------------------
# Rows of overlapping plates, every other row offset by half a plate, which is
# how scales actually sit and is what stops the sheet reading as a grid.
$cols = 6
$rows = 8
$cw = $W / $cols
$ch = $W / $rows

# Bottom row first, so every plate is overlapped at the top by the one above it
# and its own lower rim survives. Drawing the shadow before the plate does not
# work: the next row paints over it and all that is left is a scalloped band
# along whichever edge happened to be last.
for ($row = $rows - 1; $row -ge 0; $row--) {
    for ($col = 0; $col -lt $cols; $col++) {
        $x = ($col + (($row % 2) * 0.5)) * $cw
        $y = $row * $ch

        # Each plate is a little different, or the sheet is wallpaper.
        $wobble = 0.90 + $rand.NextDouble() * 0.20
        $rx = $cw * 0.68 * $wobble
        $ry = $ch * 0.86 * $wobble

        # The plate, barely off the base so the colour underneath is still the
        # colour the art asked for.
        Wrapped $g (C 240 240 240 255) $x $y $rx $ry

        # The light on its shoulder, up and left like everything else here.
        Wrapped $g (C 255 255 255 200) ($x - $rx * 0.30) ($y - $ry * 0.34) ($rx * 0.52) ($ry * 0.44)

        # And its own lower rim, drawn as an arc rather than as an offset copy
        # so it is a line under a scale instead of a second scale.
        WrappedArc $g (C 122 122 122 170) $x $y $rx $ry 18 164 (2.6 * $SS)
    }
}

# --- grain ------------------------------------------------------------
# Fine mottling over the lot. Without it the plates are clean vector shapes and
# clean is the one thing nothing down there is.
for ($i = 0; $i -lt 300; $i++) {
    $x = $rand.NextDouble() * $W
    $y = $rand.NextDouble() * $W
    $r = (0.5 + $rand.NextDouble() * 1.3) * $SS
    $dark = 150 + $rand.Next(0, 60)

    Wrapped $g (C $dark $dark $dark 56) $x $y $r $r
}

# A few pits, so the surface has been somewhere.
for ($i = 0; $i -lt 14; $i++) {
    $x = $rand.NextDouble() * $W
    $y = $rand.NextDouble() * $W
    $r = (1.4 + $rand.NextDouble() * 2.2) * $SS

    Wrapped $g (C 140 140 140 150) $x $y $r $r
    Wrapped $g (C 255 255 255 120) ($x - $r * 0.35) ($y - $r * 0.35) ($r * 0.55) ($r * 0.55)
}

$g.Dispose()

foreach ($target in @(@($TexDir, $Size), @($PreviewDir, ($Size * 2)))) {
    $dir = $target[0]
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }

    $dst = New-Object System.Drawing.Bitmap -ArgumentList $target[1], $target[1], ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gd = [System.Drawing.Graphics]::FromImage($dst)
    $gd.InterpolationMode    = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gd.PixelOffsetMode      = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $gd.CompositingQuality   = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $gd.DrawImage($bmp, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $target[1], $target[1]))
    $gd.Dispose()
    $dst.Save("$dir\boss_hide.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose()
}

$bmp.Dispose()
"  boss_hide.png  ($Size x $Size, tiling)"
