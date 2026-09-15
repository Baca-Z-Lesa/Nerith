Add-Type -AssemblyName System.Drawing
. "$PSScriptRoot\organic.ps1"

# The cutscene plates.
#
# The two reels used to be drawn out of triangles, which is fine for a beam of
# light and hopeless for a rig, a station or an animal the size of a station.
# These are painted once and then moved, panned and lit by the reel - which is
# what a shot is: something drawn well, and a camera that does not hold still.
#
# Everything here is in profile. The game is top down, so a side view is the
# cheapest signal there is that what you are watching is not the game.
#
# Two things were measured and found wanting before this file was rewritten,
# and both are worth writing down so nobody puts them back:
#
#   - Density. A lurker carries 19 drawing marks per thousand output pixels.
#     The beast carried 0.11 on a canvas twenty times the area, which is why it
#     read as a dark shape rather than as an animal. A plate shown across a
#     1280 px frame needs at least as many marks as a creature shown at 80 px,
#     not fewer.
#   - Vocabulary. The creatures had Body, Fin, Eye, Lamp and Mottle and the
#     hardware had nothing, so every rig leg was one stroke. organic.ps1 now
#     carries Panel, Rivets and Truss as well, and this file leans on all of
#     them rather than reimplementing shallow copies.
#
# $SS is 4 here, as it is in every other script in the project. It used to be
# 3, which quietly meant every raw pixel constant in the file was in a
# different unit from the rest of the art. The fix that stops that recurring is
# below: every structural width and radius is written as a multiple of $SS, so
# the number says how many OUTPUT pixels the mark will be, and it cannot drift
# under a pixel when somebody changes the supersample.
$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

# Disc, Ring, Stroke, P, C, Brush, RectF, Blob, NewCanvas, Body, BodyOutline,
# Fin, Limb, Eye, Lamp, Gills, Mottle, Panel, Rivets and Truss all come from
# organic.ps1 now. Only the four below are genuinely local: Box because there
# is no unrotated rectangle helper in there, and Curve/Outline because the
# hardware wants an explicit ink pen on a closed curve, which Blob deliberately
# does not do for flesh.
#
# Deliberately NOT called Blob: organic.ps1's Blob is ($g, $pts, $fill,
# $tension) and Body, Fin and Mottle all call it internally. Redefining Blob
# here with an edge parameter would bind the tension those callers pass into
# the edge colour and every creature helper in the file would throw.
function Box($g, $col, $x, $y, $wide, $tall) {
    $g.FillRectangle((Brush $col), (RectF $x $y $wide $tall))
}

function Curve($pts, $tension = 0.5) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, [float]$tension)
    return $path
}

function Outline($g, $pts, $col, $width, $tension = 0.5) {
    $path = Curve $pts $tension
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, ([float]$width)
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($pen, $path)
    $pen.Dispose(); $path.Dispose()
}

function Finish($bmp, $g, $outW, $outH, $name) {
    $g.Dispose()
    # The preview copy is written at 1x here, unlike every other script, which
    # writes 2x to 4x. A reel plate is already 150 to 760 px across, so there
    # is nothing to magnify. make_3d.ps1 overwrites this with a lit 3x version
    # for everything except reel_wall, which is a sheet and stays at 1x.
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, $outW, $outH))) {
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

"generating the reel plates..."

# The palette had no light values at all: it topped out at luma 132, and after
# the relief pass multiplies that by ambient and occlusion nothing survived
# above about 100. $steelTop is the value that was missing, and it is what
# makes an edge read as an edge rather than as a slightly different grey.
$steelTop = C 158 170 180
$steelHi  = C 122 132 138
$steel    = C  78  86  92
$steelLo  = C  44  50  56
$steelDk  = C  20  25  30
$rust     = C 104  72  50
$rustHi   = C 150 108  72
$rustLo   = C  66  42  26
$ink      = C  12  16  19
$lit      = C 236 196 120
$cold     = C 128 202 214
$rock     = C  38  44  48
$rockLo   = C  22  27  30
$rockHi   = C  68  78  82
$meat     = C  44  40  52
$meatHi   = C  86  78 100
$meatLo   = C  26  22  34
$meatPale = C 118 108 132
$rubber   = C  40  38  42
$suit     = C 128 112  54
$suitLo   = C  82  70  32
$suitHi   = C 176 158  88
$brass    = C 188 150  92
$paintY   = C 190 162  58
$growth   = C  70  84  64
$silt     = C  52  60  56
