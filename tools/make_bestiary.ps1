Add-Type -AssemblyName System.Drawing
. "$PSScriptRoot\organic.ps1"

# Every small creature in the game, drawn as an animal rather than as an
# assembly of shapes. Each one is built for the water it lives in and for the
# one thing it does, and that is written on its body:
#
#   Lurker    walks you down          soft, low, mottled, camouflaged
#   Darter    closes in bursts        forked tail and a deep caudal fin
#   Hulk      soaks damage            overlapping plate, eyes too small to matter
#   Spitter   holds its range         a gland body with a nozzle for a face
#   Bloom     denies area             translucent bell, radial canals, threads
#   Spawnling never stops coming      a larva, all tail, barely formed
#   Brooder   is the source           a sac you can see the young moving in
#   Lancer    commits to a line       a rigid rostrum and stabilising fins
#   Weaver    circles and rakes       long legs and a spinner
#   Husk      dies on top of you      split seams with pressure showing through
#   Angler    is already there        a lure, and nothing else lit
#
# Everything faces right, is drawn at 4x and downsampled. make_3d.ps1 lights it.

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

"generating the bestiary..."

# =====================================================================
#  LURKER  -  the yardstick. A soft bottom-walker that never hurries and
#  never stops. Mottled to match silt, eyes small because it hunts by
#  feel, and a fringe of short legs it drags itself along on.
# =====================================================================
$outW = 80; $outH = 80
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$hide   = C 92 44 62
$hideLo = C 52 22 38
$belly  = C 150 96 110
$ink    = C 20 10 16

# The legs go under the body so only the working ends show.
for ($i = 0; $i -lt 5; $i++) {
    $t = $i / 4.0
    foreach ($side in @(-1, 1)) {
        $ax = $W * (0.30 + 0.34 * $t)
        $ay = $cy + $side * 30
        [void](Limb $g $ax $ay ($side * (0.55 + 0.5 * $t)) 3 ($W * 0.085) 13 $hideLo $ink ($side * 0.30))
    }
}

# A soft sac of a body, widest behind the head, tapering to a stub.
$spine = @()
for ($i = 0; $i -le 10; $i++) {
    $t = $i / 10.0
    $x = $W * (0.72 - 0.50 * $t)
    $half = $H * (0.055 + 0.30 * [math]::Sin([math]::PI * [math]::Pow($t, 0.78)))
    $spine += , @($x, ($cy + [math]::Sin($t * 2.2) * 6), $half)
}
Body $g $spine $hide $hideLo $belly 0.55
Mottle $g $spine (C 62 28 44 150) 26 7
Mottle $g $spine (C 128 74 92 90) 14 19

# The mouth end: a puckered ring, not a jaw. It grazes.
Disc $g $hideLo ($W * 0.735) $cy 26
Disc $g (C 30 12 20) ($W * 0.745) $cy 15
Ring $g (C 176 120 132 140) ($W * 0.745) $cy 15 5

# Two small eyes set far back, and the one thing that is bright on it.
Eye $g ($W * 0.66) ($cy - 30) 13 (C 214 122 70)
Eye $g ($W * 0.66) ($cy + 30) 13 (C 214 122 70)
Lamp $g ($W * 0.50) ($cy + 2) 17 (C 255 154 66)
Finish $bmp $g $outW $outH "enemy_lurker.png"

# =====================================================================
#  DARTER  -  built to cross a gap and nothing else. Fusiform, a deep
#  forked tail, a lateral line down the flank, and almost no head.
# =====================================================================
$outW = 72; $outH = 34
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$skin   = C 142 40 44
$skinLo = C 74 18 26
$pale   = C 214 118 104

# The tail first, so the body overlaps its root.
Fin $g ($W * 0.22) $cy ($W * 0.03) ($cy - 44) 26 (C 104 28 34 220) (C 178 74 70) 4
Fin $g ($W * 0.22) $cy ($W * 0.03) ($cy + 44) 26 (C 104 28 34 220) (C 178 74 70) 4

$spine = @()
for ($i = 0; $i -le 10; $i++) {
    $t = $i / 10.0
    $x = $W * (0.94 - 0.74 * $t)
    $half = $H * (0.46 * [math]::Sin([math]::PI * [math]::Pow($t, 0.62)) + 0.03)
    $spine += , @($x, $cy, $half)
}
Body $g $spine $skin $skinLo $pale 0.5

# Pectorals swept back: it is always in a hurry.
Fin $g ($W * 0.62) ($cy - 12) ($W * 0.42) ($cy - 40) 13 (C 120 36 40 210) (C 190 88 82) 3
Fin $g ($W * 0.62) ($cy + 12) ($W * 0.42) ($cy + 40) 13 (C 120 36 40 210) (C 190 88 82) 3

# The lateral line, which is how a thing this fast knows where you are.
Stroke $g (C 236 158 138 130) ($W * 0.24) $cy ($W * 0.88) $cy 4.0
Gills $g ($W * 0.74) $cy 3 14 30 (C 56 14 20 190)

Eye $g ($W * 0.86) ($cy - 9) 12 (C 250 196 88)
Lamp $g ($W * 0.80) ($cy + 9) 9 (C 255 128 60)
Finish $bmp $g $outW $outH "enemy_darter.png"

# =====================================================================
#  HULK  -  an isopod the size of a car. Nothing about it is quick. The
#  plates overlap so a shot skids, the legs are short and many, and the
#  eyes are vestigial because at this size nothing hunts it.
# =====================================================================
$outW = 110; $outH = 96
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$shell   = C 96 104 66
$shellLo = C 48 54 34
$shellHi = C 146 154 102
$chitin  = C 28 32 20

for ($i = 0; $i -lt 6; $i++) {
    $t = $i / 5.0
    foreach ($side in @(-1, 1)) {
        $ax = $W * (0.24 + 0.46 * $t)
        $ay = $cy + $side * ($H * 0.30)
        [void](Limb $g $ax $ay ($side * (1.05 + 0.28 * $t)) 3 ($W * 0.08) 16 $shellLo $chitin ($side * 0.42))
    }
}

$spine = @()
for ($i = 0; $i -le 10; $i++) {
    $t = $i / 10.0
    $x = $W * (0.86 - 0.72 * $t)
    $half = $H * (0.10 + 0.36 * [math]::Sin([math]::PI * [math]::Pow($t, 0.66)))
    $spine += , @($x, $cy, $half)
}
Body $g $spine $shell $shellLo $shellHi 0.45
Plates $g $spine (C 108 116 74 200) (C 158 168 112 190) 6

# A head shield: the front plate, wider than the ones behind it and lipped,
# which is what it drives through everything with.
Blob $g @( (P ($W * 0.93) $cy), (P ($W * 0.86) ($cy - $H * 0.26)),
           (P ($W * 0.70) ($cy - $H * 0.30)), (P ($W * 0.66) $cy),
           (P ($W * 0.70) ($cy + $H * 0.30)), (P ($W * 0.86) ($cy + $H * 0.26)) ) `
    (C 116 124 80) 0.45
$pen = New-Object System.Drawing.Pen -ArgumentList (C 166 176 118 200), 7.0
$g.DrawArc($pen, (RectF ($W * 0.66) ($cy - $H * 0.30) ($W * 0.28) ($H * 0.60)), -74, 148)
$pen.Dispose()

# Horns for pushing, not for stabbing: short, blunt, and set wide.
Blob $g @( (P ($W * 0.99) ($cy - 34)), (P ($W * 0.86) ($cy - $H * 0.25)),
           (P ($W * 0.82) ($cy - 40)) ) $shellHi 0.3
Blob $g @( (P ($W * 0.99) ($cy + 34)), (P ($W * 0.86) ($cy + $H * 0.25)),
           (P ($W * 0.82) ($cy + 40)) ) $shellHi 0.3

Eye $g ($W * 0.90) ($cy - 20) 9 (C 226 176 86)
Eye $g ($W * 0.90) ($cy + 20) 9 (C 226 176 86)
Lamp $g ($W * 0.78) $cy 13 (C 255 150 60)
Finish $bmp $g $outW $outH "enemy_hulk.png"

# =====================================================================
#  SPITTER  -  most of it is the gland. A bloated abdomen, a siphon for a
#  face, and fins too small to close a gap with, because it never means to.
# =====================================================================
$outW = 78; $outH = 78
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$flesh   = C 78 56 112
$fleshLo = C 40 26 64
$sacGlow = C 156 214 96

# The body first: a pear, all the mass at the back where the gland is.
$spine = @()
for ($i = 0; $i -le 9; $i++) {
    $t = $i / 9.0
    $x = $W * (0.90 - 0.62 * $t)
    $half = $H * (0.06 + 0.34 * [math]::Sin([math]::PI * [math]::Pow($t, 1.9)))
    $spine += , @($x, $cy, $half)
}
Body $g $spine $flesh $fleshLo (C 138 112 178) 0.5
Mottle $g $spine (C 52 34 84 160) 18 3

# The gland showing through the skin at the back, not stuck to the outside of
# it. Clipped to the body, so it reads as an organ rather than as a bubble.
$outline = BodyOutline $spine
$clip = New-Object System.Drawing.Drawing2D.GraphicsPath
$clip.AddClosedCurve([System.Drawing.PointF[]]$outline, 0.5)
$saved = $g.Save()
$g.SetClip($clip, [System.Drawing.Drawing2D.CombineMode]::Intersect)
Disc $g (C 112 156 78 170) ($W * 0.32) ($cy + 4) ($H * 0.23)
Disc $g (C 168 222 104 140) ($W * 0.30) ($cy + 2) ($H * 0.14)
Disc $g (C 214 248 160 120) ($W * 0.29) $cy ($H * 0.07)
$g.Restore($saved)
$clip.Dispose()

# The siphon: a hard nozzle at the front, ringed like a muscle.
Blob $g @( (P ($W * 0.99) $cy), (P ($W * 0.86) ($cy - 26)), (P ($W * 0.74) ($cy - 16)),
           (P ($W * 0.74) ($cy + 16)), (P ($W * 0.86) ($cy + 26)) ) $fleshLo 0.35
Ring $g (C 178 148 220 170) ($W * 0.84) $cy 22 5
Disc $g (C 18 10 28) ($W * 0.96) $cy 13
Lamp $g ($W * 0.955) $cy 8 $sacGlow

# Stubby stabilisers, and a pair of eyes that only have to aim.
Fin $g ($W * 0.56) ($cy - 22) ($W * 0.44) ($cy - 50) 11 (C 60 40 92 200) (C 122 96 164) 3
Fin $g ($W * 0.56) ($cy + 22) ($W * 0.44) ($cy + 50) 11 (C 60 40 92 200) (C 122 96 164) 3
Eye $g ($W * 0.76) ($cy - 24) 12 (C 198 240 128) $true
Eye $g ($W * 0.76) ($cy + 24) 12 (C 198 240 128) $true
Finish $bmp $g $outW $outH "enemy_spitter.png"

# =====================================================================
#  BLOOM  -  a medusa. No front, no back, no eyes: a bell that pulses and
#  a curtain of threads that do the work. Translucent, so the canals and
#  the gonads show through it.
# =====================================================================
$outW = 96; $outH = 96
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $c = $W / 2.0

# Threads first: long, limp, and trailing every way.
for ($i = 0; $i -lt 16; $i++) {
    $a = 2 * [math]::PI * $i / 16.0
    $x = $c + [math]::Cos($a) * ($W * 0.26)
    $y = $c + [math]::Sin($a) * ($W * 0.26)
    [void](Limb $g $x $y $a 4 ($W * 0.10) 7 (C 168 120 226 170) (C 210 170 252 140) `
        (([math]::Sin($i * 2.4)) * 0.34))
}

Disc $g (C 104 70 176 60)  $c $c ($W * 0.44)
Disc $g (C 120 84 196 120) $c $c ($W * 0.36)
Disc $g (C 146 108 220 150) $c $c ($W * 0.28)

# Radial canals, the plumbing of the animal, visible through the bell.
for ($i = 0; $i -lt 8; $i++) {
    $a = 2 * [math]::PI * $i / 8.0
    Stroke $g (C 202 168 250 140) ($c + [math]::Cos($a) * ($W * 0.06)) ($c + [math]::Sin($a) * ($W * 0.06)) `
        ($c + [math]::Cos($a) * ($W * 0.34)) ($c + [math]::Sin($a) * ($W * 0.34)) 7.0
}

# The scalloped margin of the bell.
for ($i = 0; $i -lt 12; $i++) {
    $a = 2 * [math]::PI * $i / 12.0
    Disc $g (C 158 120 228 120) ($c + [math]::Cos($a) * ($W * 0.335)) `
        ($c + [math]::Sin($a) * ($W * 0.335)) ($W * 0.05)
}

# The gonads: four lobes round the middle, the only solid thing in it.
for ($i = 0; $i -lt 4; $i++) {
    $a = 2 * [math]::PI * $i / 4.0 + 0.78
    Disc $g (C 214 158 255 190) ($c + [math]::Cos($a) * ($W * 0.13)) `
        ($c + [math]::Sin($a) * ($W * 0.13)) ($W * 0.062)
}
Lamp $g $c $c ($W * 0.075) (C 236 206 255)
Finish $bmp $g $outW $outH "enemy_bloom.png"

# =====================================================================
#  SPAWNLING  -  a larva. Head, a stub of a body, and a tail that is most
#  of its length. Not finished, and it does not need to be.
# =====================================================================
$outW = 34; $outH = 22
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$spine = @()
for ($i = 0; $i -le 8; $i++) {
    $t = $i / 8.0
    $x = $W * (0.88 - 0.80 * $t)
    $half = $H * (0.34 * [math]::Pow(1.0 - $t, 1.5) + 0.025)
    $spine += , @($x, ($cy + [math]::Sin($t * 3.4) * 7), $half)
}
Body $g $spine (C 176 132 108) (C 104 74 62) (C 226 196 172) 0.55
Fin $g ($W * 0.16) $cy ($W * 0.03) ($cy - 16) 8 (C 150 112 92 180) (C 206 170 148) 3
Fin $g ($W * 0.16) $cy ($W * 0.03) ($cy + 16) 8 (C 150 112 92 180) (C 206 170 148) 3
Disc $g (C 214 176 150) ($W * 0.84) $cy ($H * 0.30)
Eye $g ($W * 0.88) ($cy - 5) 7 (C 40 30 26)
Lamp $g ($W * 0.80) ($cy + 4) 5 (C 255 172 96)
Finish $bmp $g $outW $outH "enemy_spawnling.png"

# =====================================================================
#  BROODER  -  a walking womb. The sac is the animal; the rest is the
#  minimum needed to carry it. You can see the young turning inside.
# =====================================================================
$outW = 102; $outH = 92
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$sac   = C 132 148 84
$sacLo = C 74 86 44
$skinB = C 96 78 58

for ($i = 0; $i -lt 4; $i++) {
    $t = $i / 3.0
    foreach ($side in @(-1, 1)) {
        $ax = $W * (0.26 + 0.30 * $t)
        $ay = $cy + $side * ($H * 0.26)
        [void](Limb $g $ax $ay ($side * (1.15 + 0.2 * $t)) 3 ($W * 0.09) 14 $skinB (C 34 28 20) ($side * 0.46))
    }
}

# The sac, translucent, with embryos in it.
Disc $g (C 96 108 60 180) ($W * 0.44) $cy ($H * 0.44)
Disc $g $sac ($W * 0.44) $cy ($H * 0.39)
$rand = New-Object System.Random -ArgumentList 91
for ($i = 0; $i -lt 9; $i++) {
    $a = $rand.NextDouble() * 6.283
    $d = $rand.NextDouble() * $H * 0.26
    $ex = $W * 0.44 + [math]::Cos($a) * $d
    $ey = $cy + [math]::Sin($a) * $d
    Disc $g (C 62 70 36 200) $ex $ey 15
    Disc $g (C 206 216 150 160) ($ex - 3) ($ey - 3) 7
    Stroke $g (C 62 70 36 170) $ex $ey ($ex - 20) ($ey + 12) 5
}
Ring $g (C 178 192 118 120) ($W * 0.44) $cy ($H * 0.39) 7

# A small, hard head bolted to the front of all that: it barely matters.
$spine = @()
for ($i = 0; $i -le 6; $i++) {
    $t = $i / 6.0
    $spine += , @(($W * (0.95 - 0.25 * $t)), $cy, ($H * (0.10 + 0.10 * $t)))
}
Body $g $spine $skinB (C 56 44 32) (C 152 128 100) 0.5
Eye $g ($W * 0.90) ($cy - 13) 10 (C 240 200 96)
Eye $g ($W * 0.90) ($cy + 13) 10 (C 240 200 96)
Lamp $g ($W * 0.965) $cy 11 (C 214 236 130)
Finish $bmp $g $outW $outH "enemy_brooder.png"

# =====================================================================
#  LANCER  -  a swordfish that gave up on turning. Rigid, ribbed spine, a
#  bony rostrum, and fins set far back purely to hold a line.
# =====================================================================
$outW = 88; $outH = 32
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$steel   = C 74 92 108
$steelLo = C 36 48 60
$steelHi = C 152 176 190

Fin $g ($W * 0.22) $cy ($W * 0.02) ($cy - 40) 20 (C 48 62 78 210) $steelHi 4
Fin $g ($W * 0.22) $cy ($W * 0.02) ($cy + 40) 20 (C 48 62 78 210) $steelHi 4

$spine = @()
for ($i = 0; $i -le 10; $i++) {
    $t = $i / 10.0
    $x = $W * (0.72 - 0.52 * $t)
    $half = $H * (0.40 * [math]::Sin([math]::PI * [math]::Pow($t, 0.5)) + 0.03)
    $spine += , @($x, $cy, $half)
}
Body $g $spine $steel $steelLo (C 150 172 186) 0.45

# The rostrum: a spike that is part of the skull, not stuck onto it.
Poly $g @( (P ($W * 0.995) $cy), (P ($W * 0.70) ($cy - 13)), (P ($W * 0.70) ($cy + 13)) ) $steelHi
Poly $g @( (P ($W * 0.96) $cy), (P ($W * 0.72) ($cy - 6)), (P ($W * 0.72) ($cy + 6)) ) (C 216 232 238)

# Ribs down the flank: this thing is a strut with a skin on it.
for ($i = 0; $i -lt 6; $i++) {
    $x = $W * (0.28 + 0.066 * $i)
    Stroke $g (C 40 54 66 150) $x ($cy - 14) $x ($cy + 14) 4
}
Fin $g ($W * 0.46) ($cy - 10) ($W * 0.34) ($cy - 32) 9 (C 52 66 82 200) $steelHi 3
Fin $g ($W * 0.46) ($cy + 10) ($W * 0.34) ($cy + 32) 9 (C 52 66 82 200) $steelHi 3

Eye $g ($W * 0.66) ($cy - 8) 10 (C 120 232 255) $true
Lamp $g ($W * 0.60) ($cy + 7) 7 (C 150 240 255)
Finish $bmp $g $outW $outH "enemy_lancer.png"

# =====================================================================
#  WEAVER  -  a sea spider. A small hard body slung under eight long legs,
#  and a spinner underneath it that the filaments come out of.
# =====================================================================
$outW = 82; $outH = 82
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $c = $W / 2.0

$carapace = C 58 74 86
$carapLo  = C 28 38 48
$carapHi  = C 128 158 172

for ($i = 0; $i -lt 8; $i++) {
    $a = 2 * [math]::PI * $i / 8.0 + 0.39
    $sx = $c + [math]::Cos($a) * ($W * 0.11)
    $sy = $c + [math]::Sin($a) * ($W * 0.11)
    $end = Limb $g $sx $sy $a 4 ($W * 0.105) 13 $carapLo $carapHi `
        (([math]::Sin($i * 1.7)) * 0.30 + 0.12)
    # a hooked tip, which is what it holds on with
    Stroke $g $carapHi $end[0] $end[1] ($end[0] + [math]::Cos($a + 1.3) * 18) `
        ($end[1] + [math]::Sin($a + 1.3) * 18) 4
}

Disc $g $carapLo $c $c ($W * 0.20)
Disc $g $carapace $c $c ($W * 0.165)
Disc $g (C 82 104 118) ($c - $W * 0.02) ($c - $W * 0.02) ($W * 0.11)

# Four eyes in a row, the way a spider has them.
for ($i = 0; $i -lt 4; $i++) {
    $x = $c + ($i - 1.5) * ($W * 0.045)
    Eye $g $x ($c - $W * 0.085) ($W * 0.021) (C 230 116 236)
}

# The spinner, and a filament still hanging off it.
Lamp $g $c ($c + $W * 0.07) ($W * 0.045) (C 168 246 236)
Stroke $g (C 168 246 236 140) $c ($c + $W * 0.09) ($c - $W * 0.24) ($c + $W * 0.30) 4
Finish $bmp $g $outW $outH "enemy_weaver.png"

# =====================================================================
#  HUSK  -  already dead and still moving. The seams have split and what
#  is behind them is under pressure. Killing it finishes what started.
# =====================================================================
$outW = 70; $outH = 62
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$dead   = C 96 86 74
$deadLo = C 48 42 36
$raw    = C 232 116 58

# The pressure inside, drawn first so the split seams sit over it.
Disc $g (C 180 78 40 210) ($W * 0.46) $cy ($H * 0.34)
Disc $g (C 244 150 70 190) ($W * 0.46) $cy ($H * 0.22)

$spine = @()
for ($i = 0; $i -le 9; $i++) {
    $t = $i / 9.0
    $x = $W * (0.88 - 0.70 * $t)
    $half = $H * (0.09 + 0.30 * [math]::Sin([math]::PI * [math]::Pow($t, 0.7)))
    $spine += , @($x, ($cy + [math]::Sin($t * 2.8) * 5), $half)
}
Body $g $spine $dead $deadLo (C 146 134 118) 0.5
Mottle $g $spine (C 40 34 28 170) 22 11

# The splits. Ragged rather than straight: each one walks in short segments so
# it reads as a tear in something rather than as a drawn line.
$rand = New-Object System.Random -ArgumentList 53
foreach ($seam in @(@(0.30, -0.24, 0.60, 0.04), @(0.36, 0.26, 0.66, 0.06), @(0.26, 0.02, 0.42, 0.28))) {
    $x0 = $W * $seam[0]; $y0 = $cy + $H * $seam[1]
    $x1 = $W * $seam[2]; $y1 = $cy + $H * $seam[3]

    $steps = 5
    for ($k = 0; $k -lt $steps; $k++) {
        $ta = $k / [double]$steps
        $tb = ($k + 1) / [double]$steps
        $jx = ($rand.NextDouble() - 0.5) * 16
        $jy = ($rand.NextDouble() - 0.5) * 16

        $ax = $x0 + ($x1 - $x0) * $ta; $ay = $y0 + ($y1 - $y0) * $ta
        $bx = $x0 + ($x1 - $x0) * $tb + $jx; $by = $y0 + ($y1 - $y0) * $tb + $jy

        Stroke $g (C 255 168 88 190) $ax $ay $bx $by 10
        Stroke $g (C 255 230 186 210) $ax $ay $bx $by 3.5
    }
}

# It still has legs, but they drag.
for ($i = 0; $i -lt 3; $i++) {
    foreach ($side in @(-1, 1)) {
        $ax = $W * (0.34 + 0.16 * $i)
        [void](Limb $g $ax ($cy + $side * ($H * 0.24)) ($side * 1.4) 3 ($W * 0.09) 12 $deadLo (C 26 22 18) ($side * 0.5))
    }
}
Disc $g $deadLo ($W * 0.86) $cy ($H * 0.17)
Eye $g ($W * 0.88) ($cy - 9) 9 (C 40 34 28)
Eye $g ($W * 0.88) ($cy + 9) 9 (C 40 34 28)
Finish $bmp $g $outW $outH "enemy_husk.png"

# =====================================================================
#  ANGLER  -  a profile, because the lure has to arc over the top of it
#  where you will see it and the body will not. Upper and lower jaw
#  hinged at the back, the lower one jutting.
# =====================================================================
$outW = 82; $outH = 50
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H * 0.60

$dark   = C 30 34 44
$darkLo = C 12 14 20
$gut    = C 62 68 82

# The tail and the caudal fin.
Fin $g ($W * 0.18) $cy ($W * 0.03) ($cy - 34) 18 (C 20 24 32 220) (C 70 80 96) 4
Fin $g ($W * 0.18) $cy ($W * 0.03) ($cy + 34) 18 (C 20 24 32 220) (C 70 80 96) 4

$spine = @()
for ($i = 0; $i -le 10; $i++) {
    $t = $i / 10.0
    $x = $W * (0.66 - 0.52 * $t)
    $half = $H * (0.05 + 0.34 * [math]::Sin([math]::PI * [math]::Pow($t, 0.72)))
    $spine += , @($x, $cy, $half)
}
Body $g $spine $dark $darkLo $gut 0.5
Mottle $g $spine (C 10 12 18 180) 18 5

# The jaws, hinged at the back of the skull and gaping. The gullet behind them
# is drawn first, so the teeth stand in front of a hole rather than on the body.
$hinge = $W * 0.54
Blob $g @( (P $hinge ($cy - 26)), (P ($W * 0.88) ($cy - 20)), (P ($W * 0.92) ($cy + 26)),
           (P $hinge ($cy + 30)) ) (C 8 6 10) 0.3

# Upper jaw: a bony arc with the teeth hanging off its underside.
Blob $g @( (P $hinge ($cy - 30)), (P ($W * 0.78) ($cy - 40)), (P ($W * 0.95) ($cy - 14)),
           (P ($W * 0.86) ($cy - 10)), (P $hinge ($cy - 14)) ) $darkLo 0.35
# Lower jaw, jutting further forward the way an angler's does.
Blob $g @( (P $hinge ($cy + 14)), (P ($W * 0.88) ($cy + 18)), (P ($W * 0.99) ($cy + 30)),
           (P ($W * 0.80) ($cy + 40)), (P $hinge ($cy + 34)) ) (C 24 26 34) 0.35

for ($i = 0; $i -lt 5; $i++) {
    $t = $i / 4.0
    # Longest at the front, and each one leans back towards the throat.
    $len = 30.0 - 11.0 * $t
    $ux = $hinge + ($W * 0.93 - $hinge) * $t
    $uy = $cy - 16 + 6.0 * $t
    Poly $g @( (P ($ux - 11) $uy), (P ($ux + 11) $uy), (P ($ux - 5) ($uy + $len)) ) (C 240 244 232)

    $lx = $hinge + ($W * 0.96 - $hinge) * $t
    $ly = $cy + 20 + 8.0 * $t
    Poly $g @( (P ($lx - 11) $ly), (P ($lx + 11) $ly), (P ($lx - 5) ($ly - $len)) ) (C 224 230 216)
}

# The illicium: a bare rod arcing up out of the skull with the lamp on it.
$pen = New-Object System.Drawing.Pen -ArgumentList (C 42 48 58), 8.0
$pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
$g.DrawBezier($pen, (P ($W * 0.52) ($cy - 26)), (P ($W * 0.62) ($cy - 96)),
              (P ($W * 0.86) ($cy - 108)), (P ($W * 0.93) ($cy - 62)))
$pen.Dispose()
Lamp $g ($W * 0.93) ($cy - 62) 16 (C 168 246 255)

Eye $g ($W * 0.60) ($cy - 16) 11 (C 250 236 180)
Finish $bmp $g $outW $outH "enemy_angler.png"

# =====================================================================
#  FISH  -  the harmless one that drifts past. Small, pale, and calm, so
#  the arena is not made only of things that want to kill you.
# =====================================================================
$outW = 50; $outH = 24
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

Fin $g ($W * 0.24) $cy ($W * 0.04) ($cy - 26) 14 (C 96 128 128 190) (C 150 190 186) 3
Fin $g ($W * 0.24) $cy ($W * 0.04) ($cy + 26) 14 (C 96 128 128 190) (C 150 190 186) 3

$spine = @()
for ($i = 0; $i -le 8; $i++) {
    $t = $i / 8.0
    $x = $W * (0.92 - 0.70 * $t)
    $half = $H * (0.40 * [math]::Sin([math]::PI * [math]::Pow($t, 0.6)) + 0.03)
    $spine += , @($x, $cy, $half)
}
Body $g $spine (C 118 152 150) (C 62 86 90) (C 196 220 214) 0.5
Fin $g ($W * 0.58) ($cy + 6) ($W * 0.46) ($cy + 24) 8 (C 100 132 132 180) (C 164 200 196) 3
Stroke $g (C 184 216 212 120) ($W * 0.26) $cy ($W * 0.86) $cy 3
Eye $g ($W * 0.86) ($cy - 3) 8 (C 40 52 56)
Finish $bmp $g $outW $outH "fish.png"
