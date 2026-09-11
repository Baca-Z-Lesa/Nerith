Add-Type -AssemblyName System.Drawing
. "$PSScriptRoot\organic.ps1"

# The four creatures that guard a depth, drawn as four different kinds of
# animal rather than four recolours of one. Each part is a part of a body: a
# head has a skull under it, a fin has finger bones in it, an armour scute
# grows out of the back rather than being bolted to it, and an exposed organ
# looks like something that should not be outside.
#
#   THE LEVIATHAN     a mosasaur. Bone, jaws, flippers, a dorsal scute and a
#                     heart underneath it. Evolved to run something down.
#   THE BLOOM         a siphonophore. No head, no brain worth the name, a bell
#   SOVEREIGN         of jelly and metres of feeding arms. Evolved to fill water.
#   THE TRENCH WORM   a polychaete. Rings, chaetae, a radial mouth of hooks and
#                     gill tufts it cannot armour. Evolved to live in rock.
#   THE HOLLOW        bone that outlived whatever wore it. A crown, a column of
#                     vertebrae, four lamps, and something pale still inside.
#
# Everything faces right, drawn at 4x, downsampled. make_3d.ps1 lights it.

$TexDir     = "$PSScriptRoot\..\assets\textures"
$PreviewDir = "$PSScriptRoot\preview"
$SS = 4

function Finish($bmp, $g, $outW, $outH, $name) {
    $g.Dispose()
    foreach ($target in @(@($TexDir, $outW, $outH), @($PreviewDir, ($outW * 2), ($outH * 2)))) {
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

# A row of teeth along a line, alternating long and short the way a real jaw
# does, each one leaning back towards the throat.
function Teeth($g, $x0, $y0, $x1, $y1, $count, $length, $down, $col) {
    for ($i = 0; $i -lt $count; $i++) {
        $t = ($i + 0.5) / $count
        $x = $x0 + ($x1 - $x0) * $t
        $y = $y0 + ($y1 - $y0) * $t
        # Windows PowerShell 5.1 has no ternary operator, so this is if/else.
        if (($i % 2) -eq 0) { $l = $length } else { $l = $length * 0.66 }
        $wide = $length * 0.30
        Poly $g @( (P ($x - $wide) $y), (P ($x + $wide) $y),
                   (P ($x - $wide * 0.4) ($y + $down * $l)) ) $col
    }
}

"generating the bosses..."

# =====================================================================
#  THE LEVIATHAN
# =====================================================================
$bone   = C 46 68 66
$boneLo = C 22 38 38
$boneHi = C 120 156 148
$hideL  = C 38 60 58
$gum    = C 92 44 52

# --- head: a long skull, jaws apart, seen from above-behind ----------
$outW = 176; $outH = 126
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

# The gullet: drawn first so everything else stands in front of a hole.
Blob $g @( (P ($W * 0.52) ($cy - $H * 0.20)), (P ($W * 0.92) ($cy - $H * 0.11)),
           (P ($W * 0.96) ($cy + $H * 0.11)), (P ($W * 0.52) ($cy + $H * 0.20)) ) `
    (C 30 10 14) 0.3

# The two jaws, hinged at the back of the skull and open.
foreach ($side in @(-1, 1)) {
    $jaw = @()
    for ($i = 0; $i -le 8; $i++) {
        $t = $i / 8.0
        $x = $W * (0.50 + 0.48 * $t)
        $y = $cy + $side * ($H * (0.16 + 0.16 * $t))
        $jaw += , @($x, $y, ($H * (0.085 - 0.045 * $t)))
    }
    Body $g $jaw $bone $boneLo $boneHi 0.4
    Teeth $g ($W * 0.54) ($cy + $side * $H * 0.13) ($W * 0.95) ($cy + $side * $H * 0.27) `
        9 ($H * 0.10) (-$side) (C 236 240 226)
}

# The skull itself: a broad braincase tapering into the neck.
$spine = @()
for ($i = 0; $i -le 10; $i++) {
    $t = $i / 10.0
    $x = $W * (0.62 - 0.60 * $t)
    $half = $H * (0.16 + 0.24 * [math]::Sin([math]::PI * [math]::Pow(1.0 - $t, 0.7)))
    $spine += , @($x, $cy, $half)
}
Body $g $spine $hideL $boneLo (C 96 130 124) 0.45
Mottle $g $spine (C 20 34 34 150) 24 5

# Ridges over the brow, and the nostrils.
for ($i = 0; $i -lt 4; $i++) {
    $x = $W * (0.20 + 0.10 * $i)
    Stroke $g (C 88 118 112 160) $x ($cy - $H * 0.26) $x ($cy + $H * 0.26) 7
}
Disc $g (C 14 22 22) ($W * 0.56) ($cy - $H * 0.10) 9
Disc $g (C 14 22 22) ($W * 0.56) ($cy + $H * 0.10) 9

# The eyes sit high on the sides of the skull, well clear of the teeth: this
# is the thing that used to be wrong with it.
Eye $g ($W * 0.42) ($cy - $H * 0.30) 22 (C 236 178 72) $true
Eye $g ($W * 0.42) ($cy + $H * 0.30) 22 (C 236 178 72) $true
Finish $bmp $g $outW $outH "lev_head.png"

# --- body segment: a ribbed trunk with paddle stubs ------------------
$outW = 126; $outH = 106
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$spine = @()
for ($i = 0; $i -le 8; $i++) {
    $t = $i / 8.0
    $spine += , @(($W * (0.88 - 0.76 * $t)), $cy, ($H * (0.30 + 0.10 * [math]::Sin([math]::PI * $t))))
}
Body $g $spine $hideL $boneLo (C 92 126 120) 0.45
Mottle $g $spine (C 18 32 32 140) 20 9
for ($i = 0; $i -lt 5; $i++) {
    $x = $W * (0.22 + 0.15 * $i)
    Stroke $g (C 26 42 42 150) $x ($cy - $H * 0.32) $x ($cy + $H * 0.32) 8
    Stroke $g (C 102 138 130 90) ($x + 7) ($cy - $H * 0.30) ($x + 7) ($cy + $H * 0.30) 3
}
foreach ($side in @(-1, 1)) {
    Fin $g ($W * 0.50) ($cy + $side * $H * 0.28) ($W * 0.30) ($cy + $side * $H * 0.60) `
        22 (C 32 52 50 220) $boneHi 4
}
Finish $bmp $g $outW $outH "lev_body.png"

# --- flipper: a real limb, with finger bones through the web --------
$outW = 108; $outH = 74
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

Blob $g @( (P 8 $cy), (P ($W * 0.34) ($cy - $H * 0.40)), (P ($W * 0.86) ($cy - $H * 0.26)),
           (P ($W * 0.99) ($cy + $H * 0.06)), (P ($W * 0.54) ($cy + $H * 0.42)),
           (P ($W * 0.16) ($cy + $H * 0.30)) ) (C 34 54 52) 0.5
for ($i = 0; $i -lt 5; $i++) {
    $t = $i / 4.0
    Stroke $g (C 132 166 158 200) 20 $cy `
        ($W * (0.80 + 0.16 * (1.0 - [math]::Abs($t - 0.5) * 2))) `
        ($cy + ($t - 0.5) * $H * 0.62) 7
}
Stroke $g (C 22 36 36 170) 12 $cy ($W * 0.92) ($cy - $H * 0.10) 9
Finish $bmp $g $outW $outH "lev_fin.png"

# --- eye part: what you shoot off ------------------------------------
$outW = 52; $outH = 52
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0
Disc $g (C 26 40 40) $c $c ($c * 0.96)
Eye $g $c $c ($c * 0.74) (C 240 176 66) $true
Finish $bmp $g $outW $outH "lev_eye.png"

# --- dorsal scute: armour that grew, with a keel down it ------------
$outW = 128; $outH = 84
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

Blob $g @( (P ($W * 0.06) $cy), (P ($W * 0.26) ($cy - $H * 0.40)),
           (P ($W * 0.74) ($cy - $H * 0.42)), (P ($W * 0.96) $cy),
           (P ($W * 0.74) ($cy + $H * 0.42)), (P ($W * 0.26) ($cy + $H * 0.40)) ) `
    (C 52 76 72) 0.45
# The keel, and the growth rings either side of it.
Poly $g @( (P ($W * 0.10) $cy), (P ($W * 0.50) ($cy - $H * 0.16)),
           (P ($W * 0.92) $cy), (P ($W * 0.50) ($cy + $H * 0.16)) ) (C 108 142 134)
for ($i = 0; $i -lt 4; $i++) {
    $t = ($i + 1) / 5.0
    $pen = New-Object System.Drawing.Pen -ArgumentList (C 30 46 46 160), 5.0
    $g.DrawArc($pen, (RectF ($W * (0.12 + 0.09 * $i)) ($cy - $H * (0.36 - 0.06 * $i)) `
                            ($W * (0.76 - 0.18 * $i)) ($H * (0.72 - 0.12 * $i))), 0, 360)
    $pen.Dispose()
}
Finish $bmp $g $outW $outH "lev_plate.png"

# --- the heart: what is under the scute -----------------------------
$outW = 82; $outH = 82
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

Disc $g (C 92 18 26 150) $c $c ($c * 0.98)
Blob $g @( (P $c ($c - $c * 0.78)), (P ($c + $c * 0.72) ($c - $c * 0.30)),
           (P ($c + $c * 0.46) ($c + $c * 0.68)), (P $c ($c + $c * 0.86)),
           (P ($c - $c * 0.46) ($c + $c * 0.68)), (P ($c - $c * 0.72) ($c - $c * 0.30)) ) `
    (C 186 44 52) 0.55
# Great vessels going in and out of the top of it.
foreach ($off in @(-0.34, 0.0, 0.34)) {
    Stroke $g (C 132 26 34) ($c + $c * $off) ($c - $c * 0.40) ($c + $c * $off * 1.5) ($c - $c * 0.95) 16
    Stroke $g (C 224 96 96 170) ($c + $c * $off) ($c - $c * 0.40) ($c + $c * $off * 1.5) ($c - $c * 0.95) 6
}
Disc $g (C 240 140 128 150) ($c - $c * 0.16) ($c - $c * 0.10) ($c * 0.30)
Lamp $g $c $c ($c * 0.16) (C 255 210 190)
Finish $bmp $g $outW $outH "lev_organ.png"

# =====================================================================
#  THE BLOOM SOVEREIGN  -  a siphonophore the size of a house
# =====================================================================
$outW = 216; $outH = 216
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $c = $W / 2.0

# The bell, in layers, each one more transparent than the last so it reads as
# something you can see into.
Disc $g (C 84 58 150 50)   $c $c ($c * 0.96)
Disc $g (C 96 68 168 90)   $c $c ($c * 0.86)
Disc $g (C 112 80 190 120) $c $c ($c * 0.74)

# The gastrovascular system: canals that branch instead of spoking.
for ($i = 0; $i -lt 10; $i++) {
    $a = 2 * [math]::PI * $i / 10.0
    $mx = $c + [math]::Cos($a) * ($c * 0.36)
    $my = $c + [math]::Sin($a) * ($c * 0.36)
    Stroke $g (C 176 146 236 150) $c $c $mx $my 13
    foreach ($fork in @(-0.22, 0.22)) {
        Stroke $g (C 176 146 236 120) $mx $my `
            ($c + [math]::Cos($a + $fork) * ($c * 0.70)) ($c + [math]::Sin($a + $fork) * ($c * 0.70)) 8
    }
}

# The margin: a scalloped rim with a sense organ in each notch.
for ($i = 0; $i -lt 20; $i++) {
    $a = 2 * [math]::PI * $i / 20.0
    $x = $c + [math]::Cos($a) * ($c * 0.80)
    $y = $c + [math]::Sin($a) * ($c * 0.80)
    Disc $g (C 128 96 206 110) $x $y ($c * 0.085)
    Disc $g (C 216 190 255 130) $x $y ($c * 0.022)
}

# The oral arms, hanging from underneath and frilled.
for ($i = 0; $i -lt 6; $i++) {
    $a = 2 * [math]::PI * $i / 6.0 + 0.5
    [void](Limb $g ($c + [math]::Cos($a) * ($c * 0.22)) ($c + [math]::Sin($a) * ($c * 0.22)) `
        $a 4 ($c * 0.14) 24 (C 150 112 214 160) (C 202 172 250 130) 0.26)
}

Disc $g (C 154 118 224 170) $c $c ($c * 0.22)
Lamp $g $c $c ($c * 0.10) (C 240 216 255)
Finish $bmp $g $outW $outH "med_bell.png"

# --- feeding arm: a long tentacle with stinging batteries -----------
$outW = 48; $outH = 204
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cx = $W / 2.0

$spine = @()
for ($i = 0; $i -le 14; $i++) {
    $t = $i / 14.0
    $spine += , @(($cx + [math]::Sin($t * 4.2) * ($W * 0.22)), ($H * 0.04 + $H * 0.92 * $t),
                  ($W * (0.28 - 0.20 * $t)))
}
Body $g $spine (C 150 112 214) (C 96 66 152) (C 208 180 246) 0.6
# Nematocyst batteries: the bright beads that do the damage.
for ($i = 1; $i -le 12; $i++) {
    $s = $spine[$i]
    Disc $g (C 226 198 255 190) $s[0] $s[1] ($s[2] * 0.52)
    Disc $g (C 255 240 255 210) ($s[0] - 2) ($s[1] - 2) ($s[2] * 0.22)
}
Finish $bmp $g $outW $outH "med_tentacle.png"

# --- the nucleus: the cluster that keeps it alive -------------------
$outW = 92; $outH = 92
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

Disc $g (C 120 200 220 70) $c $c ($c * 0.96)
# Not one ball: a knot of gonads packed together, which is what a
# siphonophore's core actually is.
for ($i = 0; $i -lt 7; $i++) {
    $a = 2 * [math]::PI * $i / 7.0
    Disc $g (C 128 216 236) ($c + [math]::Cos($a) * ($c * 0.34)) ($c + [math]::Sin($a) * ($c * 0.34)) ($c * 0.30)
}
Disc $g (C 186 244 252) $c $c ($c * 0.34)
Lamp $g $c $c ($c * 0.20) (C 246 255 255)
Finish $bmp $g $outW $outH "med_nucleus.png"

# =====================================================================
#  THE TRENCH WORM  -  a polychaete. Rings, bristles, a radial mouth.
# =====================================================================
$grub   = C 118 74 46
$grubLo = C 64 38 24
$grubHi = C 186 132 88

# --- head: a ring of hooked jaws round a throat ---------------------
$outW = 146; $outH = 128
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$spine = @()
for ($i = 0; $i -le 8; $i++) {
    $t = $i / 8.0
    $spine += , @(($W * (0.66 - 0.60 * $t)), $cy, ($H * (0.34 - 0.06 * $t)))
}
Body $g $spine $grub $grubLo $grubHi 0.45
Mottle $g $spine (C 48 28 18 150) 22 13

# Chaetae: the bristles it grips rock with.
for ($i = 0; $i -lt 7; $i++) {
    $x = $W * (0.10 + 0.075 * $i)
    foreach ($side in @(-1, 1)) {
        Stroke $g (C 218 176 120 190) $x ($cy + $side * $H * 0.26) `
            ($x - 18) ($cy + $side * $H * 0.42) 5
    }
}

# The mouth: everted, a wheel of hooks round a black throat.
Disc $g $grubLo ($W * 0.74) $cy ($H * 0.40)
Disc $g (C 16 8 6) ($W * 0.76) $cy ($H * 0.27)
for ($i = 0; $i -lt 12; $i++) {
    $a = 2 * [math]::PI * $i / 12.0
    $ox = $W * 0.76 + [math]::Cos($a) * ($H * 0.34)
    $oy = $cy + [math]::Sin($a) * ($H * 0.34)
    $ix = $W * 0.76 + [math]::Cos($a) * ($H * 0.16)
    $iy = $cy + [math]::Sin($a) * ($H * 0.16)
    $px = -[math]::Sin($a) * 14.0
    $py = [math]::Cos($a) * 14.0
    Poly $g @( (P ($ox + $px) ($oy + $py)), (P $ix $iy), (P ($ox - $px) ($oy - $py)) ) (C 234 214 188)
}
Lamp $g ($W * 0.76) $cy ($H * 0.07) (C 255 138 70)
Finish $bmp $g $outW $outH "worm_head.png"

# --- segment: one ring, with parapodia either side ------------------
$outW = 116; $outH = 116
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

foreach ($side in @(-1, 1)) {
    [void](Limb $g $c ($c + $side * ($c * 0.58)) ($side * 1.57) 2 ($c * 0.26) 20 $grubLo (C 40 24 16) ($side * 0.4))
}
Disc $g $grubLo $c $c ($c * 0.86)
Disc $g $grub $c $c ($c * 0.74)
# The rings that make it a worm rather than a tube.
for ($i = 0; $i -lt 4; $i++) {
    Ring $g (C 74 44 28 170) $c $c ($c * (0.68 - 0.14 * $i)) 8
}
for ($i = 0; $i -lt 14; $i++) {
    $a = 2 * [math]::PI * $i / 14.0
    Stroke $g (C 208 166 116 150) ($c + [math]::Cos($a) * ($c * 0.74)) ($c + [math]::Sin($a) * ($c * 0.74)) `
        ($c + [math]::Cos($a) * ($c * 0.94)) ($c + [math]::Sin($a) * ($c * 0.94)) 5
}
Disc $g (C 46 26 18) $c $c ($c * 0.20)
Finish $bmp $g $outW $outH "worm_seg.png"

# --- gill tuft: the one soft thing on it ----------------------------
$outW = 70; $outH = 70
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$c = $outW * $SS / 2.0

Disc $g (C 120 36 28 140) $c $c ($c * 0.94)
# Filaments, not a disc: this is a gill, and it is why the thing has to keep
# coming back up.
for ($i = 0; $i -lt 18; $i++) {
    $a = 2 * [math]::PI * $i / 18.0
    Stroke $g (C 232 92 72 200) $c $c ($c + [math]::Cos($a) * ($c * 0.82)) ($c + [math]::Sin($a) * ($c * 0.82)) 11
    Disc $g (C 255 158 128 190) ($c + [math]::Cos($a) * ($c * 0.80)) ($c + [math]::Sin($a) * ($c * 0.80)) ($c * 0.09)
}
Disc $g (C 250 128 96) $c $c ($c * 0.30)
Lamp $g $c $c ($c * 0.17) (C 255 216 184)
Finish $bmp $g $outW $outH "worm_vent.png"
