# Turns a flat sprite into one that reads as a solid object lit from the upper
# left, without touching the drawing that produced it.
#
# The trick is that a sprite's own alpha channel is a height map in disguise:
# the further a pixel is from the edge of the silhouette, the more it bulges
# towards the viewer. Take the distance to the nearest transparent pixel, dome
# it, and the gradient of that dome is a surface normal you can light. Every
# creature in the game gets the same treatment from the same code, so nothing
# drifts out of style with anything else, and no sprite changes size - which
# matters, because the game reads texture dimensions for its origins.
#
# Dot-sourced by make_3d.ps1. Nothing here draws; it only shades.

Add-Type -AssemblyName System.Drawing

Add-Type -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;

public class ReliefParams
{
    public double Softness = 0.18;   // dome radius, as a fraction of the sprite's short side
    public double SoftnessMin = 2.5; // ...but never thinner than this many pixels
    public double Relief = 2.2;      // how hard the normal bends; the sense of roundness
    public double Ambient = 0.46;    // how much light reaches the shadow side
    public double Specular = 0.3;
    public double Shininess = 16.0;
    public double Rim = 0.55;        // bright edge on the lit side
    public double RimWidth = 1.6;    // in pixels
    public double Grain = 0.07;      // fine surface noise
    public double Mottle = 0.0;      // broad blotches, for skin
    public double Streak = 0.0;      // brushed lines along the sprite's long axis
    public double Speckle = 0.0;     // hard bright/dark grit, for stone
    public double Occlusion = 0.35;  // darkening in the crevices between lobes
    public int Smoothing = 2;        // blur passes over the height field
    public double Gain = 1.06;       // lit art must not end up dimmer than flat art

    // Structural detail, cut into the surface after it has been smoothed so it
    // stays crisp. All of it is geometry rather than paint, so it catches the
    // same light as the body it sits on.
    public double Plates = 0.0;      // grooves across the long axis: hull panels, body segments
    public double PlateSpacing = 9.0;
    public double Pores = 0.0;       // a lattice of dents: skin, hide, sponge
    public double PoreSpacing = 5.0;
    public double Veins = 0.0;       // ridges radiating from the middle: jelly, membrane
    public int VeinCount = 14;
    public double Lip = 0.0;         // a second ridge inset from the outline: a shell edge
    public double LipInset = 3.2;

    // The shadow side is tinted rather than merely darkened: flesh goes red
    // under its own surface, metal goes blue because the water is what lights it.
    public int ShadowR = 26, ShadowG = 10, ShadowB = 22;
    public int RimR = 190, RimG = 240, RimB = 255;
    public int SpecR = 235, SpecG = 252, SpecB = 255;

    public double GlowKeep = 196.0;  // pixels brighter than this are lights, not surface
    public uint Seed = 1u;
}

public static class Relief
{
    // --- value noise, so the grain is stable across runs and across sprites ---
    static double Hash(int x, int y, uint seed)
    {
        unchecked
        {
            uint h = (uint)(x * 374761393) + (uint)(y * 668265263) + seed * 2246822519u;
            h = (h ^ (h >> 13)) * 1274126177u;
            h = h ^ (h >> 16);
            return (h & 0xFFFFFF) / 16777215.0;
        }
    }

    static double Smooth(double t) { return t * t * (3.0 - 2.0 * t); }

    static double Noise(double x, double y, uint seed)
    {
        int xi = (int)Math.Floor(x), yi = (int)Math.Floor(y);
        double xf = Smooth(x - xi), yf = Smooth(y - yi);

        double a = Hash(xi, yi, seed), b = Hash(xi + 1, yi, seed);
        double c = Hash(xi, yi + 1, seed), d = Hash(xi + 1, yi + 1, seed);

        return (a * (1 - xf) + b * xf) * (1 - yf) + (c * (1 - xf) + d * xf) * yf;
    }

    static double Clamp(double v, double lo, double hi)
    {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    public static void Apply(string inPath, string outPath, ReliefParams p)
    {
        using (Bitmap src = new Bitmap(inPath))
        {
            int w = src.Width, h = src.Height;

            using (Bitmap img = new Bitmap(w, h, PixelFormat.Format32bppArgb))
            {
                using (Graphics g = Graphics.FromImage(img))
                {
                    g.CompositingMode = System.Drawing.Drawing2D.CompositingMode.SourceCopy;
                    g.DrawImage(src, 0, 0, w, h);
                }

                BitmapData data = img.LockBits(new Rectangle(0, 0, w, h),
                                               ImageLockMode.ReadWrite, PixelFormat.Format32bppArgb);
                int stride = data.Stride;
                byte[] buf = new byte[stride * h];
                System.Runtime.InteropServices.Marshal.Copy(data.Scan0, buf, 0, buf.Length);

                // ---- coverage, then the distance to the nearest empty pixel ----
                double[] cover = new double[w * h];
                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                        cover[y * w + x] = buf[y * stride + x * 4 + 3] / 255.0;
                }

                double far = w + h;
                double[] dist = new double[w * h];

                for (int i = 0; i < dist.Length; i++)
                {
                    // A half covered pixel is already half way out of the shape,
                    // which is what keeps the dome smooth on an antialiased edge.
                    dist[i] = cover[i] <= 0.02 ? 0.0 : (cover[i] < 0.98 ? cover[i] : far);
                }

                // Two chamfer passes: forward, then backward. Cheap and close
                // enough to a euclidean distance for shading.
                const double D1 = 1.0, D2 = 1.41421356;
                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        int i = y * w + x;
                        if (dist[i] <= 0.0) continue;

                        double best = dist[i];
                        if (x > 0) best = Math.Min(best, dist[i - 1] + D1);
                        if (y > 0) best = Math.Min(best, dist[i - w] + D1);
                        if (x > 0 && y > 0) best = Math.Min(best, dist[i - w - 1] + D2);
                        if (x < w - 1 && y > 0) best = Math.Min(best, dist[i - w + 1] + D2);
                        dist[i] = best;
                    }
                }
                for (int y = h - 1; y >= 0; y--)
                {
                    for (int x = w - 1; x >= 0; x--)
                    {
                        int i = y * w + x;
                        if (dist[i] <= 0.0) continue;

                        double best = dist[i];
                        if (x < w - 1) best = Math.Min(best, dist[i + 1] + D1);
                        if (y < h - 1) best = Math.Min(best, dist[i + w] + D1);
                        if (x < w - 1 && y < h - 1) best = Math.Min(best, dist[i + w + 1] + D2);
                        if (x > 0 && y < h - 1) best = Math.Min(best, dist[i + w - 1] + D2);
                        dist[i] = best;
                    }
                }

                double soft = Math.Max(p.SoftnessMin, Math.Min(w, h) * p.Softness);

                // ---- the dome, plus whatever surface detail the material asks for ----
                double[] height = new double[w * h];
                double longAxis = w >= h ? 1.0 : 0.0;

                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        int i = y * w + x;
                        if (cover[i] <= 0.02) { height[i] = 0.0; continue; }

                        // A quarter sine rises steeply at the edge and flattens
                        // on top, which is what a rounded body actually does.
                        double t = Clamp(dist[i] / soft, 0.0, 1.0);
                        double dome = Math.Sin(t * Math.PI * 0.5);

                        double detail = 0.0;
                        if (p.Grain > 0.0)
                            detail += (Noise(x * 0.62, y * 0.62, p.Seed) - 0.5) * p.Grain;
                        if (p.Mottle > 0.0)
                            detail += (Noise(x * 0.17, y * 0.17, p.Seed + 7u) - 0.5) * p.Mottle;
                        if (p.Streak > 0.0)
                        {
                            double sx = longAxis > 0.5 ? x * 0.05 : x * 1.30;
                            double sy = longAxis > 0.5 ? y * 1.30 : y * 0.05;
                            detail += (Noise(sx, sy, p.Seed + 13u) - 0.5) * p.Streak;
                        }
                        if (p.Speckle > 0.0)
                        {
                            double s = Hash(x, y, p.Seed + 29u);
                            if (s > 0.93) detail += p.Speckle;
                            else if (s < 0.07) detail -= p.Speckle;
                        }

                        height[i] = dome + detail;
                    }
                }

                // A thin spike or a soft glow has almost no room between its two
                // edges, so its raw dome is a spike too and differencing it
                // gives normals that point sideways and catch the highlight as
                // white noise. Blurring the height field first leaves the broad
                // form alone and takes that noise out.
                if (p.Smoothing > 0)
                {
                    double[] tmp = new double[w * h];
                    for (int pass = 0; pass < p.Smoothing; pass++)
                    {
                        for (int y = 0; y < h; y++)
                        {
                            for (int x = 0; x < w; x++)
                            {
                                int i = y * w + x;
                                double sum = 0.0, weight = 0.0;

                                for (int dy = -1; dy <= 1; dy++)
                                {
                                    int yy = y + dy;
                                    if (yy < 0 || yy >= h) continue;

                                    for (int dx = -1; dx <= 1; dx++)
                                    {
                                        int xx = x + dx;
                                        if (xx < 0 || xx >= w) continue;

                                        double k = (dx == 0 && dy == 0) ? 4.0
                                                   : ((dx == 0 || dy == 0) ? 2.0 : 1.0);
                                        sum += height[yy * w + xx] * k;
                                        weight += k;
                                    }
                                }

                                tmp[i] = sum / weight;
                            }
                        }
                        Array.Copy(tmp, height, height.Length);
                    }
                }

                // ---- structural detail, cut in after the smoothing ----
                // It goes in last on purpose: a groove one pixel wide would be
                // blurred out of existence if it went in before.
                if (p.Plates > 0.0 || p.Pores > 0.0 || p.Veins > 0.0 || p.Lip > 0.0)
                {
                    double cx = w * 0.5, cy = h * 0.5;

                    for (int y = 0; y < h; y++)
                    {
                        for (int x = 0; x < w; x++)
                        {
                            int i = y * w + x;
                            if (cover[i] <= 0.02) continue;

                            // Detail fades out near the outline, so it never
                            // eats into the silhouette or fights the rim.
                            double inward = Clamp(dist[i] / (soft * 0.9), 0.0, 1.0);
                            double cut = 0.0;

                            if (p.Plates > 0.0)
                            {
                                // Grooves run across whichever way the sprite is
                                // longer, which is the way panels and segments
                                // actually run on a hull or a body.
                                double along = longAxis > 0.5 ? x : y;
                                double phase = along / p.PlateSpacing;
                                double frac = phase - Math.Floor(phase);

                                // A groove with a raised lip on its lit side.
                                if (frac < 0.16) cut -= p.Plates * (1.0 - frac / 0.16);
                                else if (frac < 0.30) cut += p.Plates * 0.5 * ((frac - 0.16) / 0.14);
                            }

                            if (p.Pores > 0.0)
                            {
                                // Offset every other row so the lattice reads as
                                // hide rather than as a grid.
                                double row = Math.Floor(y / p.PoreSpacing);
                                double ox = (((int)row & 1) == 0) ? 0.0 : p.PoreSpacing * 0.5;
                                double px = ((x + ox) / p.PoreSpacing);
                                double py = (y / p.PoreSpacing);
                                double fx = px - Math.Floor(px) - 0.5;
                                double fy = py - Math.Floor(py) - 0.5;
                                double r = Math.Sqrt(fx * fx + fy * fy) * 2.0;

                                if (r < 0.7) cut -= p.Pores * (0.7 - r) / 0.7;
                            }

                            if (p.Veins > 0.0)
                            {
                                double angle = Math.Atan2(y - cy, x - cx);
                                double ridge = Math.Cos(angle * p.VeinCount);
                                double reach = Clamp(Math.Sqrt((x - cx) * (x - cx)
                                                               + (y - cy) * (y - cy))
                                                     / (Math.Min(w, h) * 0.5), 0.0, 1.0);
                                cut += p.Veins * ridge * reach;
                            }

                            if (p.Lip > 0.0)
                            {
                                // A bump sitting a fixed distance in from the
                                // edge, which is what the rim of a shell is.
                                double d = Math.Abs(dist[i] - p.LipInset);
                                if (d < 1.6) cut += p.Lip * (1.0 - d / 1.6);
                            }

                            height[i] += cut * inward;
                        }
                    }
                }

                // ---- light it ----
                double lx = -0.48, ly = -0.62, lz = 0.62;
                double ll = Math.Sqrt(lx * lx + ly * ly + lz * lz);
                lx /= ll; ly /= ll; lz /= ll;

                // Half vector against a viewer straight overhead.
                double hx = lx, hy = ly, hz = lz + 1.0;
                double hl = Math.Sqrt(hx * hx + hy * hy + hz * hz);
                hx /= hl; hy /= hl; hz /= hl;

                for (int y = 0; y < h; y++)
                {
                    for (int x = 0; x < w; x++)
                    {
                        int i = y * w + x;
                        int o = y * stride + x * 4;

                        if (cover[i] <= 0.02) continue;

                        double hxm = height[i - (x > 0 ? 1 : 0)];
                        double hxp = height[i + (x < w - 1 ? 1 : 0)];
                        double hym = height[i - (y > 0 ? w : 0)];
                        double hyp = height[i + (y < h - 1 ? w : 0)];

                        double nx = -(hxp - hxm) * 0.5 * p.Relief;
                        double ny = -(hyp - hym) * 0.5 * p.Relief;
                        double nz = 1.0;
                        double nl = Math.Sqrt(nx * nx + ny * ny + nz * nz);
                        nx /= nl; ny /= nl; nz /= nl;

                        double diffuse = Math.Max(0.0, nx * lx + ny * ly + nz * lz);
                        double lit = (p.Ambient + (1.0 - p.Ambient) * diffuse) * p.Gain;

                        // Crevices: where the dome is low but the pixel is not
                        // near the outline either, something else is overlapping it.
                        double ao = 1.0 - p.Occlusion * (1.0 - Clamp(dist[i] / (soft * 0.6), 0.0, 1.0));
                        lit *= ao;

                        // Both the highlight and the rim are weighted by how
                        // solid the pixel is, so the feathered edge of a glow
                        // never picks up a hard white speck.
                        double solid = cover[i] * cover[i];

                        double spec = Math.Pow(Math.Max(0.0, nx * hx + ny * hy + nz * hz),
                                               p.Shininess) * p.Specular * solid;

                        // The rim rides the outline, but only where the surface
                        // is already turned towards the light.
                        double edge = 1.0 - Clamp(dist[i] / p.RimWidth, 0.0, 1.0);
                        double rim = edge * edge * p.Rim * diffuse * diffuse * solid;

                        int b0 = buf[o + 0], g0 = buf[o + 1], r0 = buf[o + 2];

                        // Anything already glowing is a light source in its own
                        // right, so the surface shading is faded out over it.
                        double lum = 0.299 * r0 + 0.587 * g0 + 0.114 * b0;
                        double keep = Clamp((lum - p.GlowKeep) / (255.0 - p.GlowKeep), 0.0, 1.0);
                        double shade = lit + (1.0 - lit) * keep;
                        double shadow = (1.0 - Clamp(shade, 0.0, 1.0)) * (1.0 - keep);

                        double r = r0 * shade + p.ShadowR * shadow
                                   + p.SpecR * spec + p.RimR * rim;
                        double gg = g0 * shade + p.ShadowG * shadow
                                    + p.SpecG * spec + p.RimG * rim;
                        double bb = b0 * shade + p.ShadowB * shadow
                                    + p.SpecB * spec + p.RimB * rim;

                        buf[o + 2] = (byte)Clamp(r, 0, 255);
                        buf[o + 1] = (byte)Clamp(gg, 0, 255);
                        buf[o + 0] = (byte)Clamp(bb, 0, 255);
                    }
                }

                System.Runtime.InteropServices.Marshal.Copy(buf, 0, data.Scan0, buf.Length);
                img.UnlockBits(data);
                img.Save(outPath, ImageFormat.Png);
            }
        }
    }
}
"@ -ReferencedAssemblies System.Drawing

# Named materials, so a texture is tagged with what it is made of rather than
# with eleven numbers.
function New-Material {
    param([string]$Kind, [uint32]$Seed = 1)

    $m = New-Object ReliefParams
    $m.Seed = $Seed

    switch ($Kind) {
        'flesh' {
            # Deep sea animals: soft, wet, and lit from inside their own shadow.
            $m.Softness = 0.22; $m.Relief = 2.4; $m.Ambient = 0.44
            $m.Specular = 0.42; $m.Shininess = 13; $m.Rim = 0.60; $m.RimWidth = 1.8
            $m.Grain = 0.05; $m.Mottle = 0.16; $m.Occlusion = 0.34
            $m.Pores = 0.16; $m.PoreSpacing = 4.5
            $m.ShadowR = 34; $m.ShadowG = 8; $m.ShadowB = 26
            $m.RimR = 150; $m.RimG = 226; $m.RimB = 240
        }
        'shell' {
            # Carapace and plating on a creature: harder, drier, more specular.
            $m.Softness = 0.17; $m.Relief = 2.8; $m.Ambient = 0.40
            $m.Specular = 0.62; $m.Shininess = 26; $m.Rim = 0.66; $m.RimWidth = 1.6
            $m.Grain = 0.07; $m.Mottle = 0.10; $m.Occlusion = 0.40
            $m.Plates = 0.22; $m.PlateSpacing = 11.0; $m.Lip = 0.18; $m.LipInset = 3.0
            $m.ShadowR = 18; $m.ShadowG = 10; $m.ShadowB = 30
            $m.RimR = 176; $m.RimG = 232; $m.RimB = 248
        }
        'metal' {
            $m.Softness = 0.20; $m.Relief = 2.6; $m.Ambient = 0.42
            $m.Specular = 0.80; $m.Shininess = 40; $m.Rim = 0.70; $m.RimWidth = 1.5
            $m.Grain = 0.02; $m.Streak = 0.05; $m.Occlusion = 0.40
            $m.Smoothing = 3; $m.Gain = 1.10
            $m.Plates = 0.30; $m.PlateSpacing = 13.0; $m.Lip = 0.20; $m.LipInset = 2.6
            $m.ShadowR = 10; $m.ShadowG = 18; $m.ShadowB = 34
            $m.RimR = 205; $m.RimG = 245; $m.RimB = 255
        }
        'hardware' {
            # Small, spiky, bolted-together things: a mine, a drone, a tube.
            # Too little room between their edges for a full metal treatment.
            $m.Softness = 0.30; $m.Relief = 1.8; $m.Ambient = 0.50
            $m.Specular = 0.45; $m.Shininess = 30; $m.Rim = 0.45; $m.RimWidth = 1.3
            $m.Grain = 0.02; $m.Occlusion = 0.22; $m.Smoothing = 4; $m.Gain = 1.08
            $m.ShadowR = 10; $m.ShadowG = 18; $m.ShadowB = 32
            $m.RimR = 198; $m.RimG = 238; $m.RimB = 252
        }
        'glow' {
            # Lit from inside and radially symmetric. Anything that breaks the
            # symmetry reads as a smudge, so this only deepens the falloff.
            $m.Softness = 0.50; $m.Relief = 0.8; $m.Ambient = 0.86
            $m.Specular = 0.12; $m.Shininess = 40; $m.Rim = 0.18; $m.RimWidth = 1.2
            $m.Grain = 0.0; $m.Occlusion = 0.0; $m.Smoothing = 5
            $m.GlowKeep = 150; $m.Gain = 1.04
            $m.ShadowR = 24; $m.ShadowG = 16; $m.ShadowB = 48
            $m.RimR = 210; $m.RimG = 240; $m.RimB = 255
        }
        'rock' {
            $m.Softness = 0.26; $m.Relief = 2.0; $m.Ambient = 0.48
            $m.Specular = 0.10; $m.Shininess = 7; $m.Rim = 0.30; $m.RimWidth = 2.0
            $m.Grain = 0.16; $m.Mottle = 0.22; $m.Speckle = 0.10; $m.Occlusion = 0.30
            $m.Pores = 0.10; $m.PoreSpacing = 7.0
            $m.ShadowR = 12; $m.ShadowG = 16; $m.ShadowB = 22
            $m.RimR = 150; $m.RimG = 186; $m.RimB = 196
        }
        'plant' {
            $m.Softness = 0.12; $m.Relief = 1.7; $m.Ambient = 0.54
            $m.Specular = 0.22; $m.Shininess = 17; $m.Rim = 0.42; $m.RimWidth = 1.4
            $m.Grain = 0.04; $m.Streak = 0.20; $m.Occlusion = 0.22
            $m.ShadowR = 8; $m.ShadowG = 22; $m.ShadowB = 20
            $m.RimR = 168; $m.RimG = 236; $m.RimB = 200
        }
        'glass' {
            # Orbs and anything lit from within: almost all highlight and rim.
            $m.Softness = 0.40; $m.Relief = 3.4; $m.Ambient = 0.56
            $m.Specular = 1.10; $m.Shininess = 64; $m.Rim = 0.95; $m.RimWidth = 1.5
            $m.Grain = 0.0; $m.Occlusion = 0.10; $m.GlowKeep = 226
            $m.ShadowR = 20; $m.ShadowG = 14; $m.ShadowB = 44
            $m.RimR = 220; $m.RimG = 250; $m.RimB = 255
        }
        'jelly' {
            # Translucent: barely any core shadow, a lot of rim.
            $m.Softness = 0.34; $m.Relief = 2.2; $m.Ambient = 0.62
            $m.Specular = 0.50; $m.Shininess = 22; $m.Rim = 0.85; $m.RimWidth = 2.2
            $m.Grain = 0.03; $m.Mottle = 0.12; $m.Occlusion = 0.14
            $m.Veins = 0.10; $m.VeinCount = 16
            $m.ShadowR = 30; $m.ShadowG = 18; $m.ShadowB = 52
            $m.RimR = 196; $m.RimG = 230; $m.RimB = 255
        }
        'soft' {
            # Bubbles, jets, motes: a hint of form and nothing else.
            $m.Softness = 0.45; $m.Relief = 1.1; $m.Ambient = 0.80
            $m.Specular = 0.30; $m.Shininess = 30; $m.Rim = 0.35; $m.RimWidth = 1.2
            $m.Grain = 0.0; $m.Occlusion = 0.0; $m.GlowKeep = 170
            $m.ShadowR = 20; $m.ShadowG = 40; $m.ShadowB = 60
        }
        default { throw "unknown material: $Kind" }
    }

    return $m
}
