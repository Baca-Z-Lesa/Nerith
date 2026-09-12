Add-Type -AssemblyName System.Drawing
. "$PSScriptRoot\organic.ps1"

# The deep bestiary: what lives below the campaign's own floor. Every one of
# these is built around a rule the shallow creatures do not have, and the rule
# is written on the body the same way it is up there:
#
#   Warden    armoured only in front   a fused head shield, soft unplated back
#   Siphon    latches on and drains    a rasping mouth disc and hooks, no jaw
#   Revenant  buried until it is not   flat, shovel headed, silted over
#   Herald    lights you up for others lamps on stalks, and nothing to bite with
#   Rift      arrives rather than swims a folded membrane and a body half there
#   Coilworm  the body is the weapon   a boring head and a train of ribbed rings
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

"generating the deep bestiary..."

# =====================================================================
#  WARDEN  -  a wall that walks. Everything it has went into the front:
#  one fused shield of plate over the head, small eyes tucked behind the
#  rim, and short braced legs. The back half is bare flesh, and it is
#  meant to be obviously bare - that is the whole fight.
# =====================================================================
$outW = 104; $outH = 88
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$plate   = C 104 112 96
$plateHi = C 152 160 138
$plateLo = C 54 60 50
$flesh   = C 150 78 84
$fleshLo = C 92 44 52
$ink     = C 18 14 12

# Braced legs, short and thick, angled forward against whatever it is pushing.
for ($i = 0; $i -lt 4; $i++) {
    $t = $i / 3.0
    foreach ($side in @(-1, 1)) {
        $ax = $W * (0.34 + 0.30 * $t)
        $ay = $cy + $side * ($H * 0.20)
        [void](Limb $g $ax $ay ($side * (0.9 - 0.25 * $t)) 3 ($W * 0.075) 17 $plateLo $ink ($side * 0.22))
    }
}

# The body: wide at the shield, tapering to a soft unarmoured tail.
$spine = @()
for ($i = 0; $i -le 12; $i++) {
    $t = $i / 12.0
    $x = $W * (0.14 + 0.74 * $t)
    $half = $H * (0.30 - 0.19 * $t * $t)
    $spine += , @($x, $cy, $half)
}
Body $g $spine $flesh $fleshLo (C 196 128 128) 0.5
Mottle $g $spine $fleshLo 16 771

# The shield itself, covering only the front third and standing proud of it.
$shieldSpine = @()
for ($i = 0; $i -le 6; $i++) {
    $t = $i / 6.0
    $shieldSpine += , @(($W * (0.10 + 0.36 * $t)), $cy, ($H * (0.33 - 0.10 * $t * $t)))
}
Body $g $shieldSpine $plate $plateLo $plateHi 0.42

# Growth ridges across the shield, the way a shell actually thickens, plus a
# keel down the middle and the hard rim it stops at. Overlapping plates were
# tried here and simply swallowed the whole animal.
for ($i = 1; $i -le 4; $i++) {
    $t = $i / 5.0
    $rx = $W * (0.10 + 0.36 * $t)
    $ry = $H * (0.33 - 0.10 * $t * $t)
    Stroke $g $plateLo $rx ($cy - $ry * 0.86) $rx ($cy + $ry * 0.86) ($H * 0.018)
    Stroke $g $plateHi ($rx + $W * 0.012) ($cy - $ry * 0.80) ($rx + $W * 0.012) ($cy + $ry * 0.80) ($H * 0.010)
}
Stroke $g $plateHi ($W * 0.12) $cy ($W * 0.44) $cy ($H * 0.05)
Stroke $g $plateLo ($W * 0.455) ($cy - $H * 0.24) ($W * 0.455) ($cy + $H * 0.24) ($H * 0.045)

# Eyes tucked in behind the rim, where a shot from the front cannot reach.
Eye $g ($W * 0.47) ($cy - $H * 0.15) ($H * 0.036) (C 232 176 72) $true
Eye $g ($W * 0.47) ($cy + $H * 0.15) ($H * 0.036) (C 232 176 72) $true

Gills $g ($W * 0.60) $cy 4 ($W * 0.045) ($H * 0.20) $fleshLo
Finish $bmp $g $outW $outH "enemy_warden.png"

# =====================================================================
#  SIPHON  -  all mouth. A rasping disc of hooked teeth on the front of a
#  thin muscular body, with recurved barbs down the flanks so that once it
#  is on you it stays on. No jaw, no eyes worth the name.
# =====================================================================
$outW = 88; $outH = 56
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$skin   = C 122 96 120
$skinLo = C 66 46 70
$skinHi = C 176 150 168
$tooth  = C 236 228 206
$gum    = C 168 66 82

$spine = @()
for ($i = 0; $i -le 14; $i++) {
    $t = $i / 14.0
    $x = $W * (0.16 + 0.76 * $t)
    $half = $H * (0.26 * [math]::Sin(0.35 + $t * 2.5) + 0.05)
    $spine += , @($x, ($cy + [math]::Sin($t * 3.1) * $H * 0.05), [math]::Max($H * 0.03, $half))
}
Body $g $spine $skin $skinLo $skinHi 0.55

# Barbs down the flanks, all pointing backwards: it is built to not come off.
for ($i = 2; $i -lt 12; $i += 2) {
    $s = $spine[$i]
    foreach ($side in @(-1, 1)) {
        Poly $g @( (P $s[0] ($s[1] + $side * $s[2] * 0.9)),
                   (P ($s[0] + $W * 0.055) ($s[1] + $side * $s[2] * 1.5)),
                   (P ($s[0] + $W * 0.02) ($s[1] + $side * $s[2] * 0.85)) ) $skinLo
    }
}

# The mouth disc, face on: a ring of gum with rings of hooked teeth in it.
Disc $g $skinLo ($W * 0.17) $cy ($H * 0.34)
Disc $g $gum ($W * 0.17) $cy ($H * 0.28)
for ($ring = 0; $ring -lt 3; $ring++) {
    $rad = $H * (0.24 - 0.07 * $ring)
    $n = 12 - $ring * 2
    for ($i = 0; $i -lt $n; $i++) {
        $a = 2 * [math]::PI * $i / $n + $ring * 0.3
        Poly $g @( (P ($W * 0.17 + [math]::Cos($a) * $rad) ($cy + [math]::Sin($a) * $rad)),
                   (P ($W * 0.17 + [math]::Cos($a + 0.22) * ($rad * 0.72)) ($cy + [math]::Sin($a + 0.22) * ($rad * 0.72))),
                   (P ($W * 0.17 + [math]::Cos($a - 0.22) * ($rad * 0.72)) ($cy + [math]::Sin($a - 0.22) * ($rad * 0.72))) ) $tooth
    }
}
Disc $g (C 22 10 18) ($W * 0.17) $cy ($H * 0.055)
Finish $bmp $g $outW $outH "enemy_siphon.png"

# =====================================================================
#  REVENANT  -  spends most of its life under the floor. Flat, wide and
#  shovel headed for digging, the whole top surface silted over, and a row
#  of eyes along the leading edge because that is the only part of it that
#  is ever above the silt.
# =====================================================================
$outW = 96; $outH = 80
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$pale   = C 162 156 134
$paleLo = C 96 92 78
$paleHi = C 208 202 180
$silt   = C 118 112 92
$ink    = C 26 24 18

# A flat body, widest at the head, trailing into a whip.
$spine = @()
for ($i = 0; $i -le 14; $i++) {
    $t = $i / 14.0
    $x = $W * (0.10 + 0.82 * $t)
    $half = $H * (0.34 * [math]::Exp(-3.4 * $t * $t) + 0.02)
    $spine += , @($x, $cy, $half)
}
Body $g $spine $pale $paleLo $paleHi 0.5

# The shovel: a hard leading edge it digs with, wider than the body behind it.
Poly $g @( (P ($W * 0.06) $cy),
           (P ($W * 0.22) ($cy - $H * 0.36)),
           (P ($W * 0.40) ($cy - $H * 0.24)),
           (P ($W * 0.40) ($cy + $H * 0.24)),
           (P ($W * 0.22) ($cy + $H * 0.36)) ) $paleHi
Stroke $g $paleLo ($W * 0.07) $cy ($W * 0.38) $cy ($H * 0.03)

# Silt lying on top of it, which is what it looks like when it is buried.
Mottle $g $spine $silt 26 5150

# Eyes in a row along the leading edge, tiny and all facing forward.
for ($i = -2; $i -le 2; $i++) {
    Eye $g ($W * 0.26) ($cy + $i * $H * 0.10) ($H * 0.028) (C 250 236 190) $false
}
Finish $bmp $g $outW $outH "enemy_revenant.png"

# =====================================================================
#  HERALD  -  it does not fight. It hangs in the water making light, and
#  everything else uses it. Lamps on long stalks all over a soft bell,
#  trailing threads, and nothing anywhere on it that could bite.
# =====================================================================
$outW = 92; $outH = 92
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cx = $W / 2.0; $cy = $H / 2.0

$bell   = C 70 104 128 216
$bellLo = C 38 62 84 224
$glow   = C 160 236 255

# Threads first, so the bell sits over their roots.
for ($i = 0; $i -lt 9; $i++) {
    $a = [math]::PI * (0.15 + 0.7 * $i / 8.0)
    $x = $cx + [math]::Cos($a) * $W * 0.30
    $y = $cy + [math]::Sin($a) * $H * 0.30
    [void](Limb $g $x $y ($a + 0.2) 5 ($W * 0.075) 7 (C 96 128 150 190) (C 130 170 190 170) 0.18)
}

# The bell: soft, and a little flattened, drawn as a body so it countershades.
$spine = @()
for ($i = 0; $i -le 10; $i++) {
    $t = $i / 10.0
    $spine += , @(($cx - $W * 0.30 + $W * 0.60 * $t), $cy, ($H * 0.32 * [math]::Sin(0.25 + $t * 2.65)))
}
Body $g $spine $bell $bellLo (C 120 170 200 200) 0.6

# Lamps on stalks, which is the entire reason the thing exists.
$rand = New-Object System.Random -ArgumentList 9090
for ($i = 0; $i -lt 11; $i++) {
    $a = $rand.NextDouble() * 6.283
    $d = (0.20 + $rand.NextDouble() * 0.26)
    $lx = $cx + [math]::Cos($a) * $W * $d
    $ly = $cy + [math]::Sin($a) * $H * $d * 0.8
    Stroke $g (C 110 150 174 200) $cx $cy $lx $ly 6
    Lamp $g $lx $ly ($H * (0.026 + $rand.NextDouble() * 0.022)) $glow
}
Lamp $g $cx $cy ($H * 0.08) (C 210 250 255)
Finish $bmp $g $outW $outH "enemy_herald.png"

# =====================================================================
#  RIFT  -  it does not swim to you, it is suddenly nearer. A hunched
#  body under a folded membrane it wraps around itself, lamps clustered
#  where a face would be, and the back half drawn thin enough to read as
#  not entirely present.
# =====================================================================
$outW = 88; $outH = 88
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$cloak   = C 62 48 86
$cloakLo = C 32 22 50
$cloakHi = C 116 96 150
$lamp    = C 192 124 255

# The membrane, folded around it: three overlapping sheets, back to front.
# Kept behind the body and thin enough to see through: a membrane that fills
# the frame is a purple blob, not a creature wearing something.
for ($i = 0; $i -lt 3; $i++) {
    $t = $i / 2.0
    $alpha = [int](70 + 46 * $t)
    Blob $g @( (P ($W * 0.44) ($cy - $H * (0.08 + 0.17 * $t))),
               (P ($W * (0.66 + 0.12 * $t)) ($cy - $H * (0.15 + 0.13 * $t))),
               (P ($W * (0.90 - 0.04 * $t)) $cy),
               (P ($W * (0.66 + 0.12 * $t)) ($cy + $H * (0.15 + 0.13 * $t))),
               (P ($W * 0.44) ($cy + $H * (0.08 + 0.17 * $t))) ) `
        (C $cloakHi.R $cloakHi.G $cloakHi.B $alpha) 0.6
}

$spine = @()
for ($i = 0; $i -le 10; $i++) {
    $t = $i / 10.0
    $spine += , @(($W * (0.16 + 0.52 * $t)), ($cy + [math]::Sin($t * 2.0) * $H * 0.04),
                  ($H * (0.24 - 0.16 * $t)))
}
Body $g $spine $cloak $cloakLo $cloakHi 0.5

# The lamps, clustered where a face would be if it had one.
for ($i = 0; $i -lt 5; $i++) {
    $a = -0.9 + $i * 0.45
    Lamp $g ($W * 0.20 + [math]::Cos($a) * $W * 0.05) ($cy + [math]::Sin($a) * $H * 0.13) ($H * 0.032) $lamp
}
Lamp $g ($W * 0.17) $cy ($H * 0.055) (C 226 180 255)
Finish $bmp $g $outW $outH "enemy_rift.png"

# =====================================================================
#  COILWORM  -  the head is only how it steers. What hurts you is the
#  fifteen metres of ribbed body behind it, which is drawn separately and
#  laid along wherever the head has been.
# =====================================================================
$outW = 72; $outH = 64
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cy = $H / 2.0

$chitin   = C 118 86 58
$chitinLo = C 62 42 28
$chitinHi = C 170 134 96
$maw      = C 172 54 58

$spine = @()
for ($i = 0; $i -le 8; $i++) {
    $t = $i / 8.0
    $spine += , @(($W * (0.18 + 0.66 * $t)), $cy, ($H * (0.36 - 0.09 * $t)))
}
Body $g $spine $chitin $chitinLo $chitinHi 0.4
Plates $g $spine $chitin $chitinLo 3

# The boring mouth: a round hole ringed with hooks that rasp rather than bite.
Disc $g $chitinLo ($W * 0.20) $cy ($H * 0.34)
Disc $g $maw ($W * 0.20) $cy ($H * 0.24)
Disc $g (C 26 10 12) ($W * 0.20) $cy ($H * 0.13)
for ($i = 0; $i -lt 10; $i++) {
    $a = 2 * [math]::PI * $i / 10.0
    Poly $g @( (P ($W * 0.20 + [math]::Cos($a) * $H * 0.32) ($cy + [math]::Sin($a) * $H * 0.32)),
               (P ($W * 0.20 + [math]::Cos($a + 0.26) * $H * 0.17) ($cy + [math]::Sin($a + 0.26) * $H * 0.17)),
               (P ($W * 0.20 + [math]::Cos($a - 0.26) * $H * 0.17) ($cy + [math]::Sin($a - 0.26) * $H * 0.17)) ) $chitinHi
}
Eye $g ($W * 0.56) ($cy - $H * 0.22) ($H * 0.05) (C 240 198 96) $true
Eye $g ($W * 0.56) ($cy + $H * 0.22) ($H * 0.05) (C 240 198 96) $true
Finish $bmp $g $outW $outH "enemy_coilworm.png"

# One body ring, laid down the trail the head has already swum.
$outW = 40; $outH = 40
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cx = $W / 2.0; $cy = $H / 2.0

Disc $g $chitinLo $cx $cy ($H * 0.46)
Disc $g $chitin $cx $cy ($H * 0.40)
Disc $g $chitinHi ($cx - $W * 0.05) ($cy - $H * 0.06) ($H * 0.24)
for ($i = 0; $i -lt 8; $i++) {
    $a = 2 * [math]::PI * $i / 8.0
    Stroke $g $chitinLo ($cx + [math]::Cos($a) * $H * 0.22) ($cy + [math]::Sin($a) * $H * 0.22) `
        ($cx + [math]::Cos($a) * $H * 0.46) ($cy + [math]::Sin($a) * $H * 0.46) 6
}
Finish $bmp $g $outW $outH "enemy_coil_seg.png"

# The rift's shot: a shard of the same light its lamps make.
$outW = 22; $outH = 22
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cx = $W / 2.0; $cy = $H / 2.0

Poly $g @( (P ($cx + $W * 0.44) $cy),
           (P $cx ($cy - $H * 0.26)),
           (P ($cx - $W * 0.40) $cy),
           (P $cx ($cy + $H * 0.26)) ) (C 150 86 220 230)
Poly $g @( (P ($cx + $W * 0.30) $cy),
           (P $cx ($cy - $H * 0.14)),
           (P ($cx - $W * 0.24) $cy),
           (P $cx ($cy + $H * 0.14)) ) (C 226 196 255)
Finish $bmp $g $outW $outH "shot_rift.png"

# The siphon leaves a mark on the hull while it feeds. Reused as the drain
# ring, so the thing hanging off you is visible from the HUD side too.
$outW = 26; $outH = 26
$r = NewCanvas ($outW * $SS) ($outH * $SS); $bmp = $r[0]; $g = $r[1]
$W = $outW * $SS; $H = $outH * $SS; $cx = $W / 2.0; $cy = $H / 2.0

Ring $g (C 208 90 110 220) $cx $cy ($H * 0.36) 9
for ($i = 0; $i -lt 7; $i++) {
    $a = 2 * [math]::PI * $i / 7.0
    Stroke $g (C 236 150 160 200) ($cx + [math]::Cos($a) * $H * 0.22) ($cy + [math]::Sin($a) * $H * 0.22) `
        ($cx + [math]::Cos($a) * $H * 0.42) ($cy + [math]::Sin($a) * $H * 0.42) 6
}
Finish $bmp $g $outW $outH "shot_siphon.png"
