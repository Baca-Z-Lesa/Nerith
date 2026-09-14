Add-Type -AssemblyName System.Drawing

# The icon on the desktop shortcut. Drawn here rather than dropped in as a
# file, for the same reason every other picture in this game is: there is one
# place the art comes from and it is a script.
#
# It has to survive being 16 pixels across, so it is not the sub. It is what
# the game is: a cone of light thrown into black water, with something in the
# water that the light has found.
$IcoPath = "$PSScriptRoot\..\assets\nerith.ico"
$PreviewDir = "$PSScriptRoot\preview"

function C($r, $g, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function P($x, $y) { New-Object System.Drawing.PointF -ArgumentList ([float]$x, [float]$y) }
function Brush($col) { New-Object System.Drawing.SolidBrush -ArgumentList $col }
function RectF($x, $y, $w, $h) { New-Object System.Drawing.RectangleF -ArgumentList ([float]$x, [float]$y, [float]$w, [float]$h) }

function Draw-Icon([int]$size) {
    # Everything is drawn on a big canvas and scaled down at the end, so the
    # 16 pixel version is a shrunk drawing rather than a different one.
    $S = 512
    $bmp = New-Object System.Drawing.Bitmap -ArgumentList $S, $S, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    # The water: a rounded square rather than a circle, because a circle at 16
    # pixels next to a row of square icons reads as a bullet point.
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $r = 96
    $path.AddArc(0, 0, $r, $r, 180, 90)
    $path.AddArc(($S - $r), 0, $r, $r, 270, 90)
    $path.AddArc(($S - $r), ($S - $r), $r, $r, 0, 90)
    $path.AddArc(0, ($S - $r), $r, $r, 90, 90)
    $path.CloseFigure()

    $deep = New-Object System.Drawing.Drawing2D.LinearGradientBrush -ArgumentList `
        (P 0 0), (P 0 $S), (C 16 34 44), (C 5 11 16)
    $g.FillPath($deep, $path)
    $deep.Dispose()

    $g.SetClip($path)

    # The beam, thrown from the lower left corner across the face of it. Three
    # passes of the same wedge, each wider and fainter, which is the cheapest
    # thing that reads as light rather than as a painted triangle.
    $ox = 96.0; $oy = 400.0
    foreach ($pass in @(@(0.30, 34), @(0.46, 30), @(0.66, 22))) {
        $spread = $pass[0]
        $alpha = [int]$pass[1]
        $wedge = @(
            (P $ox $oy),
            (P ($ox + 560) ($oy - 560 * (1.0 - $spread) - 40)),
            (P ($ox + 560) ($oy - 560 * (1.0 + $spread) + 40))
        )
        $g.FillPolygon((Brush (C 120 226 226 $alpha)), [System.Drawing.PointF[]]$wedge)
    }

    # The lamp itself, at the root of the beam.
    for ($i = 5; $i -ge 1; $i--) {
        $rad = 14 * $i
        $g.FillEllipse((Brush (C 150 240 236 (18 + 12 * (5 - $i)))), (RectF ($ox - $rad) ($oy - $rad) ($rad * 2) ($rad * 2)))
    }
    $g.FillEllipse((Brush (C 226 255 250)), (RectF ($ox - 15) ($oy - 15) 30 30))

    # And what the beam found: a shape, not an animal. Two curves and a row of
    # teeth is all that survives sixteen pixels, and all that needs to.
    $jaw = New-Object System.Drawing.Drawing2D.GraphicsPath
    $jaw.AddClosedCurve([System.Drawing.PointF[]]@(
        (P 470 96), (P 330 104), (P 236 168), (P 268 236),
        (P 372 250), (P 462 208)), 0.45)
    $g.FillPath((Brush (C 12 24 28)), $jaw)
    $pen = New-Object System.Drawing.Pen -ArgumentList (C 96 150 148), 10.0
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($pen, $jaw)
    $pen.Dispose()
    $jaw.Dispose()

    # Teeth along the near edge of it.
    for ($i = 0; $i -lt 5; $i++) {
        $t = $i / 4.0
        $x = 276 + $t * 168
        $y = 234 - $t * 34
        $g.FillPolygon((Brush (C 214 232 224)), [System.Drawing.PointF[]]@(
            (P ($x - 15) $y), (P $x ($y + 40)), (P ($x + 15) $y)))
    }

    # One eye, lit, because a thing looking back is the whole game.
    $g.FillEllipse((Brush (C 255 186 74)), (RectF 352 122 64 64))
    $g.FillEllipse((Brush (C 20 12 8)), (RectF 372 136 26 40))

    $g.ResetClip()

    # A rim, so the icon has an edge of its own on a pale desktop.
    $edge = New-Object System.Drawing.Pen -ArgumentList (C 92 136 140 200), 12.0
    $g.DrawPath($edge, $path)
    $edge.Dispose()
    $path.Dispose()
    $g.Dispose()

    $out = New-Object System.Drawing.Bitmap -ArgumentList $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gd = [System.Drawing.Graphics]::FromImage($out)
    $gd.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gd.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $gd.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $gd.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $gd.DrawImage($bmp, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $size, $size))
    $gd.Dispose()
    $bmp.Dispose()
    return $out
}

"generating the icon..."

# Every size Explorer asks for, packed by hand. An .ico is a six byte header,
# a sixteen byte directory entry per image, and then the images; Windows has
# taken PNG payloads inside one since Vista, so each entry is just the PNG.
$sizes = @(256, 128, 64, 48, 32, 24, 16)
$blobs = @()

foreach ($size in $sizes) {
    $img = Draw-Icon $size
    $stream = New-Object System.IO.MemoryStream
    $img.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $blobs += ,$stream.ToArray()
    $stream.Dispose()
    if ($size -eq 256) {
        if (-not (Test-Path $PreviewDir)) { New-Item -ItemType Directory -Force -Path $PreviewDir | Out-Null }
        $img.Save("$PreviewDir\nerith_icon.png", [System.Drawing.Imaging.ImageFormat]::Png)
    }
    if ($size -eq 64) {
        # The same picture again as a plain PNG, because the window wants one
        # and SFML has never been able to read an .ico. Not in textures/: the
        # relief pass lights everything in there and this is already lit.
        $img.Save("$PSScriptRoot\..\assets\icon.png", [System.Drawing.Imaging.ImageFormat]::Png)
    }
    $img.Dispose()
}

$out = New-Object System.IO.MemoryStream
$w = New-Object System.IO.BinaryWriter -ArgumentList $out

$w.Write([uint16]0)             # reserved
$w.Write([uint16]1)             # 1 is an icon, 2 would be a cursor
$w.Write([uint16]$sizes.Count)

# The images start after the header and the whole directory.
$offset = 6 + 16 * $sizes.Count

for ($i = 0; $i -lt $sizes.Count; $i++) {
    $size = $sizes[$i]
    $bytes = $blobs[$i]

    # 256 is written as 0: the field is one byte and 256 does not fit in it.
    $dim = $size
    if ($dim -ge 256) { $dim = 0 }

    $w.Write([byte]$dim)
    $w.Write([byte]$dim)
    $w.Write([byte]0)           # no palette
    $w.Write([byte]0)           # reserved
    $w.Write([uint16]1)         # planes
    $w.Write([uint16]32)        # bits per pixel
    $w.Write([uint32]$bytes.Length)
    $w.Write([uint32]$offset)

    $offset += $bytes.Length
}

foreach ($bytes in $blobs) { $w.Write($bytes) }

$w.Flush()
[System.IO.File]::WriteAllBytes($IcoPath, $out.ToArray())
$w.Dispose(); $out.Dispose()

"  nerith.ico  ($($sizes -join ', '))"
