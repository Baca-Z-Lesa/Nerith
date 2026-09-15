# Procedural sound effects. The project ships no recorded audio: every sound is
# synthesised here and written straight to 16 bit mono WAV, so the whole game
# stays reproducible from source.
$OutDir = "$PSScriptRoot\..\assets\audio"
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force -Path $OutDir | Out-Null }

Add-Type @"
using System;
using System.IO;

public static class Synth
{
    const int Rate = 44100;
    static Random rng = new Random(20260818);

    public static void WriteWav(string path, float[] samples)
    {
        short[] pcm = new short[samples.Length];
        for (int i = 0; i < samples.Length; i++)
        {
            float v = samples[i];
            if (v > 1f) v = 1f;
            if (v < -1f) v = -1f;
            pcm[i] = (short)(v * 31000f);
        }
        byte[] data = new byte[pcm.Length * 2];
        Buffer.BlockCopy(pcm, 0, data, 0, data.Length);

        using (var fs = new FileStream(path, FileMode.Create))
        using (var bw = new BinaryWriter(fs))
        {
            bw.Write(System.Text.Encoding.ASCII.GetBytes("RIFF"));
            bw.Write(36 + data.Length);
            bw.Write(System.Text.Encoding.ASCII.GetBytes("WAVE"));
            bw.Write(System.Text.Encoding.ASCII.GetBytes("fmt "));
            bw.Write(16);
            bw.Write((short)1);
            bw.Write((short)1);
            bw.Write(Rate);
            bw.Write(Rate * 2);
            bw.Write((short)2);
            bw.Write((short)16);
            bw.Write(System.Text.Encoding.ASCII.GetBytes("data"));
            bw.Write(data.Length);
            bw.Write(data);
        }
    }

    static int Count(float seconds) { return (int)(seconds * Rate); }
    static float T(int i) { return (float)i / Rate; }
    static float Noise() { return (float)(rng.NextDouble() * 2.0 - 1.0); }

    // One pole low pass, the cheapest way to turn white noise into water.
    static void LowPass(float[] buffer, float cutoff)
    {
        float a = 1f - (float)Math.Exp(-2.0 * Math.PI * cutoff / Rate);
        float y = 0f;
        for (int i = 0; i < buffer.Length; i++)
        {
            y += (buffer[i] - y) * a;
            buffer[i] = y;
        }
    }

    static float Decay(float t, float length, float power)
    {
        float f = 1f - t / length;
        if (f < 0f) f = 0f;
        return (float)Math.Pow(f, power);
    }

    // Short fade so a sample never starts or ends on a click.
    static void Edges(float[] b, float seconds)
    {
        int n = Count(seconds);
        for (int i = 0; i < n && i < b.Length; i++)
        {
            float f = (float)i / n;
            b[i] *= f;
            b[b.Length - 1 - i] *= f;
        }
    }

    public static float[] Ambient(float seconds)
    {
        // Generated a little long, then the tail is crossfaded into the head so
        // the loop point is inaudible.
        float fade = 0.75f;
        int n = Count(seconds + fade);
        float[] b = new float[n];
        float[] noise = new float[n];

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float lfo = 0.62f + 0.38f * (float)Math.Sin(2 * Math.PI * 0.043 * t);
            float groan = 0.5f + 0.5f * (float)Math.Sin(2 * Math.PI * 0.017 * t + 1.3);

            b[i] = 0.34f * lfo * (float)Math.Sin(2 * Math.PI * 41.0 * t)
                 + 0.26f * lfo * (float)Math.Sin(2 * Math.PI * 43.4 * t)
                 + 0.13f * groan * (float)Math.Sin(2 * Math.PI * 88.0 * t)
                 + 0.05f * groan * (float)Math.Sin(2 * Math.PI * 131.0 * t + 0.6);

            noise[i] = Noise();
        }

        LowPass(noise, 320f);
        for (int i = 0; i < n; i++)
        {
            float swell = 0.5f + 0.5f * (float)Math.Sin(2 * Math.PI * 0.031 * T(i) + 2.1);
            b[i] += noise[i] * 0.55f * swell;
        }

        int keep = Count(seconds);
        int cross = n - keep;
        float[] outBuf = new float[keep];
        Array.Copy(b, outBuf, keep);
        for (int i = 0; i < cross; i++)
        {
            float f = (float)i / cross;
            outBuf[i] = outBuf[i] * f + b[keep + i] * (1f - f);
        }
        return outBuf;
    }

    public static float[] JetFire()
    {
        float len = 0.17f;
        int n = Count(len);
        float[] b = new float[n];
        for (int i = 0; i < n; i++) b[i] = Noise();
        LowPass(b, 2600f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float env = Decay(t, len, 2.6f) * Math.Min(1f, t / 0.004f);
            float body = (float)Math.Sin(2 * Math.PI * (340.0 - 190.0 * t / len) * t);
            b[i] = (b[i] * 0.75f + body * 0.35f) * env * 0.55f;
        }
        Edges(b, 0.003f);
        return b;
    }

    public static float[] Impact()
    {
        float len = 0.13f;
        int n = Count(len);
        float[] b = new float[n];
        for (int i = 0; i < n; i++) b[i] = Noise();
        LowPass(b, 6500f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float env = Decay(t, len, 4.0f);
            float ping = (float)Math.Sin(2 * Math.PI * 900.0 * t);
            b[i] = (b[i] * 0.8f + ping * 0.3f) * env * 0.6f;
        }
        Edges(b, 0.002f);
        return b;
    }

    public static float[] EnemyDie()
    {
        float len = 0.45f;
        int n = Count(len);
        float[] b = new float[n];
        float[] noise = new float[n];
        for (int i = 0; i < n; i++) noise[i] = Noise();
        LowPass(noise, 1100f);

        double phase = 0.0;
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            double freq = 230.0 - 175.0 * (t / len);
            phase += 2 * Math.PI * freq / Rate;
            float env = Decay(t, len, 2.2f);
            b[i] = ((float)Math.Sin(phase) * 0.55f + noise[i] * 0.55f) * env * 0.65f;
        }
        Edges(b, 0.004f);
        return b;
    }

    // Every creature used to die with the same noise, which made a basin full
    // of different animals sound like one animal being killed repeatedly. One
    // shape of death sound, parameterised, and a small table decides which
    // creature gets which. The parameters are the animal: what its body is
    // made of, how much air is in it, and how far it falls as it goes.
    public static float[] DeathVoice(float len, double startHz, double endHz,
                                     float tone, float noiseMix, float cutoff,
                                     float rasp, float crackle)
    {
        int n = Count(len);
        float[] b = new float[n];
        float[] noise = new float[n];
        for (int i = 0; i < n; i++) noise[i] = Noise();
        LowPass(noise, cutoff);

        double phase = 0.0;
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float k = t / len;

            // The pitch slide is the fall. A body with air in it slides a long
            // way; a shell barely slides at all, it just stops.
            double freq = startHz + (endHz - startHz) * k;
            phase += 2 * Math.PI * freq / Rate;

            // Rasp is the body arguing on the way down: a slow tremolo that a
            // smooth-skinned thing does not have and a chitinous one does.
            float wobble = 1f + rasp * (float)Math.Sin(2 * Math.PI * 38.0 * t);

            float env = Decay(t, len, 2.2f);
            float body = (float)Math.Sin(phase) * tone * wobble + noise[i] * noiseMix;

            // Crackle is the shell going, a handful of hard ticks rather than
            // a texture, so it reads as breaking instead of as static.
            if (crackle > 0f && rng.NextDouble() < crackle * (1.0 - k))
                body += (float)(rng.NextDouble() * 2.0 - 1.0) * 0.9f;

            b[i] = body * env * 0.62f;
        }
        Edges(b, 0.004f);
        return b;
    }

    // What a shot landing on it sounds like, which is the same question asked
    // of a much shorter sound: is this meat, shell, or something that should
    // not be here.
    public static float[] HitVoice(float len, double hz, float tone,
                                   float noiseMix, float cutoff, float ring)
    {
        int n = Count(len);
        float[] b = new float[n];
        float[] noise = new float[n];
        for (int i = 0; i < n; i++) noise[i] = Noise();
        LowPass(noise, cutoff);

        double phase = 0.0;
        double phase2 = 0.0;
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            phase += 2 * Math.PI * hz / Rate;
            phase2 += 2 * Math.PI * hz * 2.71 / Rate;

            float env = Decay(t, len, 3.4f) * Math.Min(1f, t / 0.0015f);
            float body = (float)Math.Sin(phase) * tone
                         + (float)Math.Sin(phase2) * ring
                         + noise[i] * noiseMix;

            b[i] = body * env * 0.58f;
        }
        Edges(b, 0.002f);
        return b;
    }

    public static float[] PlayerHurt()
    {
        float len = 0.36f;
        int n = Count(len);
        float[] b = new float[n];
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float env = Decay(t, len, 1.8f) * Math.Min(1f, t / 0.003f);
            // Two close tones so it beats, which reads as wrong rather than musical.
            float a = (float)Math.Sin(2 * Math.PI * 104.0 * t);
            float c = (float)Math.Sin(2 * Math.PI * 111.0 * t);
            float grit = (float)Math.Sin(2 * Math.PI * 208.0 * t) * 0.3f;
            b[i] = (a + c + grit) * env * 0.32f;
        }
        Edges(b, 0.004f);
        return b;
    }

    public static float[] PartBreak()
    {
        float len = 0.7f;
        int n = Count(len);
        float[] b = new float[n];
        float[] noise = new float[n];
        for (int i = 0; i < n; i++) noise[i] = Noise();
        LowPass(noise, 3800f);

        // Inharmonic partials are what make a clang sound like metal.
        double[] partials = { 286.0, 517.0, 733.0, 1129.0, 1583.0 };
        double[] decays = { 1.6, 2.2, 2.8, 3.6, 4.6 };

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float sum = 0f;
            for (int p = 0; p < partials.Length; p++)
                sum += (float)Math.Sin(2 * Math.PI * partials[p] * t) * Decay(t, len, (float)decays[p]);

            float transient = noise[i] * Decay(t, 0.09f, 3.0f);
            b[i] = (sum * 0.16f + transient * 0.7f) * 0.7f;
        }
        Edges(b, 0.003f);
        return b;
    }

    public static float[] LevelClear()
    {
        float len = 1.05f;
        int n = Count(len);
        float[] b = new float[n];
        double[] notes = { 392.0, 523.25, 659.25, 784.0 };

        for (int k = 0; k < notes.Length; k++)
        {
            int start = Count(0.09f * k);
            for (int i = start; i < n; i++)
            {
                float t = T(i - start);
                float env = Decay(t, len - 0.09f * k, 2.4f) * Math.Min(1f, t / 0.02f);
                b[i] += (float)Math.Sin(2 * Math.PI * notes[k] * t) * env * 0.17f;
                b[i] += (float)Math.Sin(2 * Math.PI * notes[k] * 2 * t) * env * 0.05f;
            }
        }
        Edges(b, 0.006f);
        return b;
    }

    public static float[] BossRoar()
    {
        float len = 1.9f;
        int n = Count(len);
        float[] b = new float[n];
        float[] noise = new float[n];
        for (int i = 0; i < n; i++) noise[i] = Noise();
        LowPass(noise, 420f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float env = Math.Min(1f, t / 0.25f) * Decay(t, len, 1.4f);
            float wobble = 1f + 0.22f * (float)Math.Sin(2 * Math.PI * 6.5 * t);
            float low = (float)Math.Sin(2 * Math.PI * 44.0 * t * wobble);
            float mid = (float)Math.Sin(2 * Math.PI * 67.0 * t * wobble) * 0.5f;
            b[i] = (low + mid + noise[i] * 0.8f) * env * 0.42f;
        }
        Edges(b, 0.01f);
        return b;
    }

    // Something big, a long way off, that has noticed you. It never resolves
    // into anything: it is two slow tones sliding past each other, filtered
    // down until the water has eaten most of it. Played at random during a
    // fight, which is what stops a quiet stretch from feeling safe.
    public static float[] CreatureCall(float baseHz)
    {
        float len = 3.4f;
        int n = Count(len);
        float[] b = new float[n];
        float[] noise = new float[n];
        for (int i = 0; i < n; i++) noise[i] = Noise();
        LowPass(noise, 190f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);

            // Slow in, slower out. A call with a hard attack reads as an attack.
            float env = Math.Min(1f, t / 0.9f) * Decay(t, len, 1.9f);

            // The pitch sags as it runs out of air.
            float glide = 1f - 0.18f * (t / len);
            float vib = 1f + 0.035f * (float)Math.Sin(2 * Math.PI * 2.3 * t);

            float a = (float)Math.Sin(2 * Math.PI * baseHz * t * glide * vib);
            float c = (float)Math.Sin(2 * Math.PI * (baseHz * 1.48) * t * glide) * 0.42f;
            float d = (float)Math.Sin(2 * Math.PI * (baseHz * 0.51) * t * glide) * 0.55f;

            b[i] = (a + c + d + noise[i] * 0.30f) * env * 0.27f;
        }
        Edges(b, 0.05f);
        return b;
    }

    // A brooder opening. Wet, short, and low enough to be felt under the
    // shooting, so a swarm arriving is audible before it is visible.
    public static float[] Hatch()
    {
        float len = 0.42f;
        int n = Count(len);
        float[] b = new float[n];
        float[] noise = new float[n];
        for (int i = 0; i < n; i++) noise[i] = Noise();
        LowPass(noise, 1500f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float env = Decay(t, len, 2.6f);

            // A falling pitch is what makes a pop sound like something giving
            // way rather than like a click.
            float sweep = 340f - 250f * (t / len);
            float body = (float)Math.Sin(2 * Math.PI * sweep * t);

            b[i] = (body * 0.6f + noise[i] * 0.7f) * env * 0.5f;
        }
        Edges(b, 0.005f);
        return b;
    }

    // ------------------------------------------------------------------
    //  MUSIC
    //  Three loops of the same length over the same root, so they can be
    //  played together and faded independently: the bed is always on, the
    //  pulse comes up when something is hunting you, and the dread comes up
    //  for a boss. Nothing here is a tune - a melody would wear out in four
    //  minutes of a level - it is harmony and rhythm that can sit under a
    //  fight indefinitely.
    // ------------------------------------------------------------------
    const double Root = 55.0; // A1, low enough to feel rather than hear

    static void Loop(float[] b, float seconds)
    {
        // Crossfades the tail into the head, and this is NOT enough on its
        // own: it moves the first sample and leaves the last one exactly where
        // it was, so the one join a looping player actually makes is the one
        // join it does not touch. Measured on the bed, the wrap stepped 0.045
        // against a sample to sample slope of 0.0008 - sixty times the signal's
        // own rate of change, which is a faint tick every sixteen seconds for
        // as long as the game is running.
        //
        // So it also walks the tail back down to meet the head. Over a tenth
        // of a second, which is far longer than any transient in a drone and
        // far shorter than anything the ear hears as a fade.
        int fade = Count(0.6f);
        int n = b.Length;
        for (int i = 0; i < fade && i < n / 2; i++)
        {
            float f = (float)i / fade;
            b[i] = b[i] * f + b[n - fade + i] * (1f - f);
        }

        int meet = Count(0.10f);
        if (meet > n / 4) meet = n / 4;

        for (int i = 0; i < meet; i++)
        {
            float f = (float)i / meet;
            b[n - meet + i] = b[n - meet + i] * (1f - f) + b[0] * f;
        }
    }

    // Always on: a slow chord that breathes, with the fifth drifting in and
    // out of tune with itself so it never settles.
    public static float[] MusicBed(float seconds)
    {
        int n = Count(seconds);
        float[] b = new float[n];

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float swell = 0.55f + 0.45f * (float)Math.Sin(2 * Math.PI * t / seconds);
            float beat = 1f + 0.004f * (float)Math.Sin(2 * Math.PI * 0.21 * t);

            b[i] = (float)(Math.Sin(2 * Math.PI * Root * t) * 0.42
                         + Math.Sin(2 * Math.PI * Root * 1.5 * t * beat) * 0.26
                         + Math.Sin(2 * Math.PI * Root * 2.0 * t) * 0.14
                         + Math.Sin(2 * Math.PI * Root * 3.0 * t) * 0.06) * swell * 0.5f;
        }

        Loop(b, seconds);
        return b;
    }

    // Up when something is hunting you: a heartbeat on the root, and a minor
    // third above it that is the only thing in the score with any tension.
    public static float[] MusicPulse(float seconds)
    {
        int n = Count(seconds);
        float[] b = new float[n];
        float period = 0.92f; // just faster than a resting pulse

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float phase = t % period;

            // Two thumps, the second softer: a heart, not a metronome.
            float beat = Decay(phase, 0.30f, 3.0f)
                         + (phase > 0.26f ? Decay(phase - 0.26f, 0.24f, 3.4f) * 0.6f : 0f);

            float body = (float)Math.Sin(2 * Math.PI * Root * 0.5 * t);
            float third = (float)Math.Sin(2 * Math.PI * Root * 1.19 * t) * 0.30f;

            b[i] = (body * beat + third * beat * 0.7f) * 0.42f;
        }

        Loop(b, seconds);
        return b;
    }

    // Up for a boss: a tritone against the root, which is the interval the ear
    // refuses to accept as resolved, plus a low grind under it.
    public static float[] MusicDread(float seconds)
    {
        int n = Count(seconds);
        float[] b = new float[n];
        float[] noise = new float[n];
        for (int i = 0; i < n; i++) noise[i] = Noise();
        LowPass(noise, 180f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float swell = 0.4f + 0.6f * (float)Math.Sin(2 * Math.PI * t / seconds * 2.0);
            float shiver = 1f + 0.02f * (float)Math.Sin(2 * Math.PI * 7.3 * t);

            b[i] = (float)(Math.Sin(2 * Math.PI * Root * t) * 0.34
                         + Math.Sin(2 * Math.PI * Root * 1.414 * t * shiver) * 0.30
                         + Math.Sin(2 * Math.PI * Root * 0.5 * t) * 0.22
                         + noise[i] * 0.35) * swell * 0.5f;
        }

        Loop(b, seconds);
        return b;
    }

    // Up when something is out there that you have not seen. Nothing with a
    // pitch you could name: a breath in and a breath out, far too slow to be
    // yours, and a single high partial that wanders a little every cycle so
    // the ear keeps checking whether it is still there. It has to be the one
    // layer you notice without being able to say what changed.
    public static float[] MusicStalk(float seconds)
    {
        int n = Count(seconds);
        float[] b = new float[n];

        float[] breath = new float[n];
        for (int i = 0; i < n; i++) breath[i] = Noise();
        LowPass(breath, 240f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);

            // One slow cycle over the whole loop, so it never lines up with
            // the bed's own swell and the two drift against each other.
            float lungs = 0.5f - 0.5f * (float)Math.Cos(2 * Math.PI * t / seconds);
            float draw = lungs * lungs; // heavier on the way in than on the way out

            // The partial that wanders. High enough to sit above everything
            // else in the score and quiet enough to be deniable.
            float wander = 1f + 0.06f * (float)Math.Sin(2 * Math.PI * 0.037 * t);
            float thread = (float)Math.Sin(2 * Math.PI * Root * 12.0 * t * wander);

            b[i] = (breath[i] * 0.55f * draw
                    + thread * 0.045f * (0.3f + 0.7f * draw)
                    + (float)Math.Sin(2 * Math.PI * Root * 0.75 * t) * 0.20f * draw) * 0.5f;
        }

        Loop(b, seconds);
        return b;
    }

    // A creature changing its mind. Not a roar: a rising tone that stops dead,
    // because what has to land is the moment the fight turns, and a rise that
    // resolves reads as an ending rather than a beginning.
    public static float[] BossPhase()
    {
        float len = 2.6f;
        int n = Count(len);
        float[] b = new float[n];
        float[] noise = new float[n];
        for (int i = 0; i < n; i++) noise[i] = Noise();
        LowPass(noise, 700f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float u = t / len;

            // Rises through most of it, then the last fifth is all fall.
            float env = u < 0.78f ? (float)Math.Pow(u / 0.78f, 1.5) : (1f - (u - 0.78f) / 0.22f);
            if (env < 0f) env = 0f;

            float sweep = 58f + 150f * (float)Math.Pow(u, 2.2);
            float a = (float)Math.Sin(2 * Math.PI * sweep * t);
            float c = (float)Math.Sin(2 * Math.PI * sweep * 1.5 * t) * 0.4f;
            float d = (float)Math.Sin(2 * Math.PI * sweep * 0.5 * t) * 0.7f;

            b[i] = (a + c + d + noise[i] * 0.55f) * env * 0.40f;
        }
        Edges(b, 0.02f);
        return b;
    }

    // Something enormous coming apart. Four seconds, because the end of a boss
    // has to be allowed to take its time: a hit, then the collapse, then the
    // long low note of the water closing over it.
    public static float[] BossDeath()
    {
        float len = 4.2f;
        int n = Count(len);
        float[] b = new float[n];
        float[] noise = new float[n];
        for (int i = 0; i < n; i++) noise[i] = Noise();
        LowPass(noise, 340f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);

            // The initial hit.
            float crack = Decay(t, 0.5f, 2.2f);

            // The collapse: irregular, so it reads as structure failing rather
            // than as one explosion.
            float rubble = 0f;
            for (int k = 0; k < 5; k++)
            {
                float at = 0.25f + k * 0.42f;
                if (t > at) rubble += Decay(t - at, 1.1f, 2.6f) * (0.7f - k * 0.09f);
            }

            // And the water closing over it.
            float tail = Math.Min(1f, t / 1.4f) * Decay(t, len, 1.3f);
            float low = (float)Math.Sin(2 * Math.PI * 31.0 * t)
                        + (float)Math.Sin(2 * Math.PI * 46.5 * t) * 0.55f;

            b[i] = (noise[i] * (crack * 1.1f + rubble * 0.8f) + low * tail * 0.5f) * 0.52f;
        }
        Edges(b, 0.01f);
        return b;
    }

    // A jet wound up to full: a tightening whine that cuts off when it goes.
    public static float[] Charge()
    {
        float len = 0.9f;
        int n = Count(len);
        float[] b = new float[n];

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float u = t / len;
            float env = (float)Math.Pow(u, 0.8) * (1f - (float)Math.Pow(u, 12));
            float sweep = 180f + 620f * u * u;

            b[i] = (float)Math.Sin(2 * Math.PI * sweep * t) * env * 0.26f;
        }
        Edges(b, 0.01f);
        return b;
    }

    // The alarm the sub makes when the hull is nearly gone. Two tones a
    // semitone apart, which is the interval that will not let you settle.
    public static float[] Siren()
    {
        float len = 1.1f;
        int n = Count(len);
        float[] b = new float[n];

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float env = Math.Min(1f, t / 0.07f) * Decay(t, len, 1.6f);
            float wow = 1f + 0.05f * (float)Math.Sin(2 * Math.PI * 5.5 * t);

            b[i] = ((float)Math.Sin(2 * Math.PI * 494.0 * t * wow)
                    + (float)Math.Sin(2 * Math.PI * 466.0 * t)) * env * 0.16f;
        }
        Edges(b, 0.01f);
        return b;
    }

    // A vent letting go: a hiss with a rumble under it, rising then falling,
    // because superheated water arrives before the noise of the rock does.
    public static float[] Vent()
    {
        float len = 1.5f;
        int n = Count(len);
        float[] b = new float[n];
        float[] hiss = new float[n];
        for (int i = 0; i < n; i++) hiss[i] = Noise();
        LowPass(hiss, 2600f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float env = Math.Min(1f, t / 0.12f) * Decay(t, len, 1.7f);
            float rumble = (float)Math.Sin(2 * Math.PI * 52.0 * t)
                           + (float)Math.Sin(2 * Math.PI * 78.0 * t) * 0.5f;

            b[i] = (hiss[i] * 0.85f + rumble * 0.35f) * env * 0.40f;
        }
        Edges(b, 0.01f);
        return b;
    }
}
"@

"generating audio..."

$jobs = @(
    @("ambient_abyss.wav",   [Synth]::Ambient(14.0)),
    @("jet_fire.wav",        [Synth]::JetFire()),
    @("impact.wav",          [Synth]::Impact()),
    @("enemy_die.wav",       [Synth]::EnemyDie()),

    # One death per kind of body. The numbers are the animal: how much air is
    # in it, what its shell is made of, and how far it falls on the way out.
    @("die_soft.wav",        [Synth]::DeathVoice(0.44, 240.0, 52.0, 0.55, 0.55, 1100.0, 0.0, 0.0)),
    @("die_shell.wav",       [Synth]::DeathVoice(0.30, 170.0, 120.0, 0.22, 0.60, 4200.0, 0.10, 0.06)),
    @("die_bladder.wav",     [Synth]::DeathVoice(0.72, 300.0, 38.0, 0.30, 0.85, 2000.0, 0.0, 0.0)),
    @("die_wet.wav",         [Synth]::DeathVoice(0.38, 620.0, 150.0, 0.62, 0.42, 2600.0, 0.55, 0.0)),
    @("die_void.wav",        [Synth]::DeathVoice(0.66, 90.0, 760.0, 0.72, 0.18, 5200.0, 0.22, 0.0)),

    @("hit_soft.wav",        [Synth]::HitVoice(0.10, 190.0, 0.50, 0.55, 1400.0, 0.0)),
    @("hit_shell.wav",       [Synth]::HitVoice(0.08, 420.0, 0.30, 0.45, 5200.0, 0.32)),
    @("hit_void.wav",        [Synth]::HitVoice(0.16, 880.0, 0.55, 0.10, 6000.0, 0.40)),
    @("player_hurt.wav",     [Synth]::PlayerHurt()),
    @("boss_part_break.wav", [Synth]::PartBreak()),
    @("level_clear.wav",     [Synth]::LevelClear()),
    @("boss_roar.wav",       [Synth]::BossRoar()),
    @("creature_call_a.wav", [Synth]::CreatureCall(58.0)),
    @("creature_call_b.wav", [Synth]::CreatureCall(41.0)),
    @("hatch.wav",           [Synth]::Hatch()),
    @("vent.wav",            [Synth]::Vent()),
    @("boss_phase.wav",      [Synth]::BossPhase()),
    @("boss_death.wav",      [Synth]::BossDeath()),
    @("charge.wav",          [Synth]::Charge()),
    @("siren.wav",           [Synth]::Siren()),
    @("music_bed.wav",       [Synth]::MusicBed(16.0)),
    @("music_pulse.wav",     [Synth]::MusicPulse(16.0)),
    @("music_dread.wav",     [Synth]::MusicDread(16.0)),
    @("music_stalk.wav",     [Synth]::MusicStalk(16.0))
)

foreach ($job in $jobs) {
    $path = Join-Path $OutDir $job[0]
    [Synth]::WriteWav($path, $job[1])
    $size = [math]::Round((Get-Item $path).Length / 1KB, 1)
    "  $($job[0])  ${size} KB"
}
"done"
