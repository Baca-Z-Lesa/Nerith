# Shared drawing for anything alive.
#
# The creatures used to be assemblies: a disc for a body, some triangles bolted
# on for legs. This is the library that stops that. Everything here is built
# around a centre line with a width profile, the way an animal is, so a body
# tapers into a tail instead of ending, a fin grows out of a flank instead of
# being stuck to it, and a limb bends at joints instead of being a spike.
#
# Two rules the whole bestiary follows:
#
#   - Countershading. Every animal down here is darker on top than underneath.
#     It is the single strongest cue that a shape is a creature rather than a
#     symbol, and it costs one extra pass.
#   - Adaptation. What a creature does is written on its body. A thing that
#     charges has a rigid spine and stabilising fins; a thing that drifts has
#     none and trails threads; a thing that soaks damage wears overlapping
#     plate and has eyes too small to matter.
#
# Dot-sourced by the make_*.ps1 scripts. Nothing here writes a file.
#
# One trap worth knowing before editing any of the callers: PowerShell variable
# names are case insensitive, so $w and $W are the same variable. A script that
# holds the canvas width in $W and then uses $w for a half-width silently
# destroys the canvas width on the first loop iteration, and every shape after
# that comes out as a wedge. Half-widths are called $half throughout for that
# reason.

Add-Type -AssemblyName System.Drawing

function P($x, $y) { New-Object System.Drawing.PointF -ArgumentList ([float]$x, [float]$y) }
function C($r, $g, $b, $a = 255) { [System.Drawing.Color]::FromArgb($a, $r, $g, $b) }
function Brush($col) { New-Object System.Drawing.SolidBrush -ArgumentList $col }
function RectF($x, $y, $w, $h) { New-Object System.Drawing.RectangleF -ArgumentList ([float]$x, [float]$y, [float]$w, [float]$h) }

function Disc($g, $col, $cx, $cy, $rad) {
    $g.FillEllipse((Brush $col), (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2)))
}

function Ring($g, $col, $cx, $cy, $rad, $width) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, $width
    $g.DrawEllipse($pen, (RectF ($cx - $rad) ($cy - $rad) ($rad * 2) ($rad * 2)))
    $pen.Dispose()
}

function Stroke($g, $col, $x0, $y0, $x1, $y1, $w) {
    $pen = New-Object System.Drawing.Pen -ArgumentList $col, ([float]$w)
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

function Blob($g, $pts, $fill, $tension = 0.55) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$pts, [float]$tension)
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

# =====================================================================
#  The body itself
# =====================================================================

# Walks a centre line and returns the outline of a body of that shape: for each
# sample it offsets left and right by the width at that point, then comes back
# down the other side. `spine` is an array of @(x, y, halfWidth).
function BodyOutline($spine) {
    $left = @()
    $right = @()
    $n = $spine.Count

    for ($i = 0; $i -lt $n; $i++) {
        $x = [double]$spine[$i][0]
        $y = [double]$spine[$i][1]
        $w = [double]$spine[$i][2]

        # The normal comes from the neighbours, so the outline stays square to
        # the centre line even where it curves hard.
        $prev = $spine[[math]::Max(0, $i - 1)]
        $next = $spine[[math]::Min($n - 1, $i + 1)]
        $dx = [double]$next[0] - [double]$prev[0]
        $dy = [double]$next[1] - [double]$prev[1]
        $len = [math]::Sqrt($dx * $dx + $dy * $dy)
        if ($len -lt 0.0001) { $len = 1.0 }

        $nx = -$dy / $len
        $ny = $dx / $len

        $left += , (P ($x + $nx * $w) ($y + $ny * $w))
        $right = , (P ($x - $nx * $w) ($y - $ny * $w)) + $right
    }

    return $left + $right
}

# A body with countershading: the base colour, then a darker band along the top
# and a paler one along the belly, each clipped to the body itself.
function Body($g, $spine, $base, $dorsal, $ventral, $tension = 0.5) {
    $outline = BodyOutline $spine
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$outline, [float]$tension)

    $g.FillPath((Brush $base), $path)

    $saved = $g.Save()
    $g.SetClip($path, [System.Drawing.Drawing2D.CombineMode]::Intersect)

    # Top third dark, bottom third pale. Drawn as offset copies of the same
    # outline so they follow the animal rather than lying flat across it.
    if ($null -ne $dorsal) {
        $top = @()
        foreach ($s in $spine) { $top += , @($s[0], ($s[1] - $s[2] * 0.62), ($s[2] * 0.55)) }
        Blob $g (BodyOutline $top) $dorsal $tension
    }
    if ($null -ne $ventral) {
        $bottom = @()
        foreach ($s in $spine) { $bottom += , @($s[0], ($s[1] + $s[2] * 0.66), ($s[2] * 0.42)) }
        Blob $g (BodyOutline $bottom) $ventral $tension
    }

    $g.Restore($saved)
    $path.Dispose()
}

# A membrane fin: a curved sheet with ray struts running through it, growing out
# of the flank rather than pasted onto it.
function Fin($g, $rootX, $rootY, $tipX, $tipY, $spread, $web, $ray, $rays = 5) {
    $dx = [double]$tipX - $rootX
    $dy = [double]$tipY - $rootY
    $len = [math]::Sqrt($dx * $dx + $dy * $dy)
    if ($len -lt 0.001) { return }
    $ux = $dx / $len; $uy = $dy / $len
    $px = -$uy; $py = $ux

    # The trailing edge scallops, which is what stops a fin reading as a
    # triangle with a gradient on it.
    $pts = @( (P $rootX $rootY) )
    for ($i = 1; $i -le $rays; $i++) {
        $t = $i / [double]$rays
        $bow = [math]::Sin($t * [math]::PI) * $spread * 0.34
        $pts += , (P ($rootX + $ux * $len * $t + $px * ($spread * $t + $bow)) `
                     ($rootY + $uy * $len * $t + $py * ($spread * $t + $bow)))
    }
    $pts += , (P $tipX $tipY)
    Blob $g $pts $web 0.45

    for ($i = 1; $i -le $rays; $i++) {
        $t = $i / [double]$rays
        Stroke $g $ray $rootX $rootY `
            ($rootX + $ux * $len * $t + $px * $spread * $t * 0.92) `
            ($rootY + $uy * $len * $t + $py * $spread * $t * 0.92) 3.5
    }
}

# A limb that bends: segments that taper, with a visible joint between each.
function Limb($g, $x, $y, $angle, $segments, $length, $thick, $col, $joint, $curl = 0.34) {
    $a = [double]$angle
    $cx = [double]$x
    $cy = [double]$y

    for ($i = 0; $i -lt $segments; $i++) {
        $t = $i / [double]$segments
        $seg = $length * (1.0 - $t * 0.28)
        $w = $thick * (1.0 - $t * 0.62)

        $nx = $cx + [math]::Cos($a) * $seg
        $ny = $cy + [math]::Sin($a) * $seg

        Stroke $g $col $cx $cy $nx $ny ([math]::Max(2.0, $w))
        Disc $g $joint $nx $ny ([math]::Max(1.5, $w * 0.5))

        $cx = $nx; $cy = $ny
        $a += $curl
    }

    return @($cx, $cy)
}

# An eye that looks back: a dark orbit, a wet sclera, an iris, a slit or round
# pupil, and one small hard highlight. The highlight is what makes it read as
# an eye rather than a dot.
function Eye($g, $cx, $cy, $rad, $iris, $slit = $false) {
    Disc $g (C 8 10 12 210) $cx $cy ($rad * 1.28)
    Disc $g (C 236 238 230) $cx $cy $rad
    Disc $g $iris $cx $cy ($rad * 0.68)

    if ($slit) {
        $g.FillEllipse((Brush (C 6 8 10)),
            (RectF ($cx - $rad * 0.16) ($cy - $rad * 0.64) ($rad * 0.32) ($rad * 1.28)))
    } else {
        Disc $g (C 6 8 10) $cx $cy ($rad * 0.34)
    }

    Disc $g (C 255 255 255 226) ($cx - $rad * 0.3) ($cy - $rad * 0.34) ($rad * 0.22)
}

# A lit organ: something the animal makes light with. Halo first, then the
# lamp, so it reads as glowing rather than as a bright circle.
function Lamp($g, $cx, $cy, $rad, $col) {
    for ($i = 5; $i -ge 1; $i--) {
        $t = $i / 5.0
        Disc $g (C $col.R $col.G $col.B ([int](30 * (1 - $t) * 3))) $cx $cy ($rad * $t * 2.6)
    }
    Disc $g $col $cx $cy $rad
    Disc $g (C 255 255 255 200) ($cx - $rad * 0.24) ($cy - $rad * 0.24) ($rad * 0.42)
}

# Gill slits along a flank: short curved cuts, deeper at the front.
function Gills($g, $x, $y, $count, $spacing, $length, $col) {
    for ($i = 0; $i -lt $count; $i++) {
        $gx = $x + $i * $spacing
        $l = $length * (1.0 - $i * 0.13)
        Stroke $g $col $gx ($y - $l / 2) ($gx - $l * 0.16) ($y + $l / 2) 4.0
    }
}

# Overlapping carapace plates following a line, each one lipped at its trailing
# edge so the shell reads as segmented rather than striped.
function Plates($g, $spine, $shell, $lip, $count) {
    $n = $spine.Count
    for ($i = 0; $i -lt $count; $i++) {
        $t = ($i + 0.5) / [double]$count
        $k = [int][math]::Min($n - 1, [math]::Floor($t * ($n - 1)))
        $x = [double]$spine[$k][0]
        $y = [double]$spine[$k][1]
        $w = [double]$spine[$k][2]

        $g.FillEllipse((Brush $shell), (RectF ($x - $w * 0.86) ($y - $w * 0.96) ($w * 1.72) ($w * 1.92)))
        $pen = New-Object System.Drawing.Pen -ArgumentList $lip, 5.0
        $g.DrawArc($pen, (RectF ($x - $w * 0.86) ($y - $w * 0.96) ($w * 1.72) ($w * 1.92)), 40, 100)
        $pen.Dispose()
    }
}

# Mottling: irregular patches of a second tone, clipped to the body. What stops
# a flank from being one flat colour.
function Mottle($g, $spine, $col, $count, $seed) {
    $rand = New-Object System.Random -ArgumentList $seed
    $outline = BodyOutline $spine
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddClosedCurve([System.Drawing.PointF[]]$outline, 0.5)

    $saved = $g.Save()
    $g.SetClip($path, [System.Drawing.Drawing2D.CombineMode]::Intersect)

    for ($i = 0; $i -lt $count; $i++) {
        $s = $spine[$rand.Next(0, $spine.Count)]
        $x = [double]$s[0] + ($rand.NextDouble() - 0.5) * [double]$s[2] * 1.4
        $y = [double]$s[1] + ($rand.NextDouble() - 0.5) * [double]$s[2] * 1.5
        $r = [double]$s[2] * (0.16 + $rand.NextDouble() * 0.3)
        Disc $g $col $x $y $r
    }

    $g.Restore($saved)
    $path.Dispose()
}

# ---------------------------------------------------------------------------
#  Built things
#
#  The creatures got a vocabulary and the hardware never did, which is the
#  whole reason the deck and the cutscene plates look like placeholder art
#  next to an animal: a lurker is 19 marks per thousand pixels and the rig
#  was 0.34. These three are the hardware equivalents of Body, Mottle and
#  Lamp - one call that lays down enough marks for a thing to read as built
#  rather than as filled.
# ---------------------------------------------------------------------------

# A plate of steel from above: the face, a light edge on the two sides the
# relief pass lights from, and a dark edge on the other two. Five marks
# instead of one is the entire difference between a box and a piece of a ship.
function Panel($g, $x, $y, $w, $h, $face, $hi, $lo, $bevel) {
    $g.FillRectangle((Brush $face), (RectF $x $y $w $h))
    $g.FillRectangle((Brush $hi),   (RectF $x $y $w $bevel))
    $g.FillRectangle((Brush $hi),   (RectF $x $y $bevel $h))
    $g.FillRectangle((Brush $lo),   (RectF $x ($y + $h - $bevel) $w $bevel))
    $g.FillRectangle((Brush $lo),   (RectF ($x + $w - $bevel) $y $bevel $h))
}

# A row of rivets along a line. The cap is offset up and left of the seat, so
# each one reads as a dome; offset the other way - which is what the deck
# plate used to do - and the same two discs read as a hole.
function Rivets($g, $x0, $y0, $x1, $y1, $count, $rad, $seat, $cap) {
    for ($i = 0; $i -lt $count; $i++) {
        $t = ($i + 0.5) / $count
        $x = $x0 + ($x1 - $x0) * $t
        $y = $y0 + ($y1 - $y0) * $t
        Disc $g $seat ($x + $rad * 0.28) ($y + $rad * 0.28) $rad
        Disc $g $cap  ($x - $rad * 0.30) ($y - $rad * 0.30) ($rad * 0.62)
    }
}

# Lattice: two chords, N alternating diagonals, a node disc at every joint.
# A rig leg drawn as one stroke is a table leg; this is why offshore steel
# looks like offshore steel from a mile away.
function Truss($g, $x0, $y0, $x1, $y1, $span, $bays, $chord, $web, $node, $wide) {
    $dx = $x1 - $x0
    $dy = $y1 - $y0
    $len = [math]::Sqrt($dx * $dx + $dy * $dy)
    if ($len -lt 1) { return }

    $px = -$dy / $len * $span * 0.5
    $py = $dx / $len * $span * 0.5

    Stroke $g $chord ($x0 + $px) ($y0 + $py) ($x1 + $px) ($y1 + $py) $wide
    Stroke $g $chord ($x0 - $px) ($y0 - $py) ($x1 - $px) ($y1 - $py) $wide

    for ($i = 0; $i -lt $bays; $i++) {
        $a = $i / [double]$bays
        $b = ($i + 1) / [double]$bays
        if (($i % 2) -eq 0) { $s = 1 } else { $s = -1 }

        Stroke $g $web ($x0 + $dx * $a + $px * $s) ($y0 + $dy * $a + $py * $s) `
                       ($x0 + $dx * $b - $px * $s) ($y0 + $dy * $b - $py * $s) ($wide * 0.55)
        Disc $g $node ($x0 + $dx * $a + $px * $s) ($y0 + $dy * $a + $py * $s) ($wide * 0.7)
        Disc $g $node ($x0 + $dx * $a - $px * $s) ($y0 + $dy * $a - $py * $s) ($wide * 0.7)
    }
}
