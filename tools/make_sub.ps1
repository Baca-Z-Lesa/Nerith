Add-Type -AssemblyName System.Drawing

$OutPath     = "$PSScriptRoot\..\assets\textures\player_sub.png"
$PreviewPath = "$PSScriptRoot\preview\preview_sub.png"

$W = 128; $H = 80; $SS = 4
$bw = $W * $SS; $bh = $H * $SS   # 512 x 320

function P($x, $y) { New-Object System.Drawing.PointF -ArgumentList ([float]$x, [float]$y) }
function C($r, $g, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }

# --- palette (dark steel hull + cyan accents, matches the game background) ---
$colHull      = C 44  72  86
$colHullLight = C 62  96 112
$colHullDark  = C 26  44  55
$colOutline   = C 12  24  31
$colTower     = C 74 116 132
$colAccent    = C 122 214 205
$colGlow      = C 168 240 232
$colThruster  = C 20  36  46

$bmp = New-Object System.Drawing.Bitmap -ArgumentList $bw, $bh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::Transparent)

$penOutline = New-Object System.Drawing.Pen -ArgumentList $colOutline, 7.0
$penOutline.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

# ---------- stern fins (drawn first so the hull overlaps them) ----------
$finBrush = New-Object System.Drawing.SolidBrush -ArgumentList $colHullDark
foreach ($fin in @(
    ,@((P 168 138), (P 108 138), (P 74 56),  (P 132 70))
    ,@((P 168 182), (P 108 182), (P 74 264), (P 132 250))
)) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddPolygon([System.Drawing.PointF[]]$fin)
    $g.FillPath($finBrush, $path)
    $g.DrawPath($penOutline, $path)
    $path.Dispose()
}

# ---------- rear thruster ----------
$thrusterRect = New-Object System.Drawing.RectangleF -ArgumentList 46, 134, 44, 52
$g.FillEllipse((New-Object System.Drawing.SolidBrush -ArgumentList $colThruster), $thrusterRect)
$g.DrawEllipse($penOutline, $thrusterRect)

# ---------- hull ----------
$hull = [System.Drawing.PointF[]]@(
    (P 478 160), (P 424 116), (P 330  98), (P 226 100),
    (P 138 118), (P  84 142), (P  74 160), (P  84 178),
    (P 138 202), (P 226 220), (P 330 222), (P 424 204)
)
$hullPath = New-Object System.Drawing.Drawing2D.GraphicsPath
$hullPath.AddClosedCurve($hull, 0.45)

$hullBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush -ArgumentList `
    (New-Object System.Drawing.PointF -ArgumentList ([float]0, [float]88)), `
    (New-Object System.Drawing.PointF -ArgumentList ([float]0, [float]232)), `
    $colHullLight, $colHullDark
$g.FillPath($hullBrush, $hullPath)
$g.DrawPath($penOutline, $hullPath)

# ---------- side weapon pods ----------
$podBrush = New-Object System.Drawing.SolidBrush -ArgumentList $colHull
foreach ($pod in @(
    (New-Object System.Drawing.RectangleF -ArgumentList 286,  76, 92, 30),
    (New-Object System.Drawing.RectangleF -ArgumentList 286, 214, 92, 30)
)) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddRectangle($pod)
    $g.FillPath($podBrush, $path)
    $g.DrawPath($penOutline, $path)
    $path.Dispose()
}

# ---------- conning tower ----------
$towerRect = New-Object System.Drawing.RectangleF -ArgumentList 186, 118, 116, 84
$g.FillEllipse((New-Object System.Drawing.SolidBrush -ArgumentList $colTower), $towerRect)
$g.DrawEllipse($penOutline, $towerRect)

$towerInner = New-Object System.Drawing.RectangleF -ArgumentList 214, 138, 60, 44
$g.FillEllipse((New-Object System.Drawing.SolidBrush -ArgumentList $colHullDark), $towerInner)

# ---------- cyan accent stripe along the spine ----------
$penAccent = New-Object System.Drawing.Pen -ArgumentList (C 122 214 205 150), 5.0
$g.DrawLine($penAccent, 310, 160, 352, 160)

# ---------- forward viewport / searchlight ----------
$glowRect = New-Object System.Drawing.RectangleF -ArgumentList 356, 128, 64, 64
$g.FillEllipse((New-Object System.Drawing.SolidBrush -ArgumentList (C 122 214 205 70)), $glowRect)
$portRect = New-Object System.Drawing.RectangleF -ArgumentList 370, 142, 36, 36
$g.FillEllipse((New-Object System.Drawing.SolidBrush -ArgumentList $colAccent), $portRect)
$coreRect = New-Object System.Drawing.RectangleF -ArgumentList 378, 150, 20, 20
$g.FillEllipse((New-Object System.Drawing.SolidBrush -ArgumentList $colGlow), $coreRect)

$hullPath.Dispose()
$g.Dispose()

# ---------- downsample to the final texture size ----------
function Save-Scaled($src, $w, $h, $path) {
    $dst = New-Object System.Drawing.Bitmap -ArgumentList $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gd = [System.Drawing.Graphics]::FromImage($dst)
    $gd.InterpolationMode    = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $gd.PixelOffsetMode      = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $gd.CompositingQuality   = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $gd.SmoothingMode        = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $gd.DrawImage($src, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $w, $h))
    $gd.Dispose()
    $dir = Split-Path -Parent $path
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $dst.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $dst.Dispose()
}

Save-Scaled $bmp $W $H $OutPath
Save-Scaled $bmp ($W * 3) ($H * 3) $PreviewPath
$bmp.Dispose()

"Saved $OutPath ($W x $H)"

