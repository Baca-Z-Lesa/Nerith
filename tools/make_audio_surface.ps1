# Procedural sound for everything that happens above the water, at the ends of a
# dive, and over a cutscene. It is a second file rather than more of make_audio.ps1
# for one blunt reason: Add-Type registers a type name into the session for good,
# so a second Add-Type of a class called Synth in the same PowerShell fails, and
# the alternative - one enormous file that regenerates the whole game's audio
# every time you want to retune a footstep - costs forty seconds a go. The class
# here is called SurfaceSynth, the helpers it shares with Synth are copied rather
# than imported, and the two scripts never write the same filename.
$OutDir = "$PSScriptRoot\..\assets\audio"
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force -Path $OutDir | Out-Null }

Add-Type @"
using System;
using System.IO;

public static class SurfaceSynth
{
    const int Rate = 44100;

    // Fixed seed, same as the abyss script and for the same reason: audio that
    // is regenerated on every build has to produce identical bytes, or the repo
    // grows a megabyte of noise diff every time anyone touches a gain.
    static Random rng = new Random(20260915);

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

    // The complement of the low pass, and the only way anything in this game
    // gets to be bright. Rain on a steel deck is the first sound here that is
    // not underwater, and underwater is all a low pass can make.
    static void HighPass(float[] b, float cutoff)
    {
        float a = 1f - (float)Math.Exp(-2.0 * Math.PI * cutoff / Rate);
        float y = 0f;
        for (int i = 0; i < b.Length; i++)
        {
            y += (b[i] - y) * a;
            b[i] -= y;
        }
    }

    // A resonant peak. Noise through a low pass is water; noise through one of
    // these is metal, which is what a rig, a winch, a grate and a bell all are
    // and what inharmonic sine partials can only fake for a single clang.
    // bandwidth is in Hz: T60 is about 6.91 / (PI * bandwidth), so 2 Hz rings
    // for a second, 40 Hz for fifty milliseconds, 400 Hz is only a colour.
    //
    // Feed this noise. Its gain at the centre frequency is roughly
    // 1 / |1 - r.exp(-2jw)|, which for a narrow band low down is forty or fifty
    // times, and noise only ever has a sliver of its energy inside the band so
    // the result comes out sane. Put a sustained sine through the middle of one
    // and you get all forty times of it: shaft_hold's 87 Hz resonator was
    // written straight onto a buffer that already held a 132 to 66 Hz sweep,
    // the sweep crossed 87 Hz on the way down, and the file peaked at 7.9 -
    // ten times over full scale, and every other layer in it inaudible
    // underneath. The fix is not a smaller gain, it is resonating the noise
    // layer on its own and adding it afterwards, which is also what actually
    // happens: a hole makes its note because water is moving through it, not
    // because a winch is playing it a tone.
    static void Ring(float[] b, float hz, float bandwidth, float gain)
    {
        double w = 2 * Math.PI * hz / Rate;
        double r = Math.Exp(-Math.PI * bandwidth / Rate);
        double a1 = -2.0 * r * Math.Cos(w);
        double a2 = r * r;
        float y1 = 0f, y2 = 0f;

        for (int i = 0; i < b.Length; i++)
        {
            float y = (float)(b[i] * (1.0 - r) - a1 * y1 - a2 * y2);
            y2 = y1;
            y1 = y;
            b[i] += y * gain;
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

    // Several of the new sounds are not one gesture but three: a brake coming
    // off, then a motor, then a load landing. Every recipe in the abyss script
    // is a single envelope over a single buffer, which cannot say "and then,
    // four hundred milliseconds later".
    static void Mix(float[] into, float[] what, float atSeconds, float gain)
    {
        int at = Count(atSeconds);
        for (int i = 0; i < what.Length && at + i < into.Length; i++)
            into[at + i] += what[i] * gain;
    }

    // A sine that slides. Charge, BossPhase and Hatch each grew their own copy
    // of this loop; nine of the sounds below want one, so it stops being three
    // copies and starts being a thing. power = 0 is deliberate and load
    // bearing: Decay then returns 1 and the tone does not fade at all, which is
    // how a held lift sounds like a process rather than like an event.
    static float[] Tone(float len, double startHz, double endHz, float curve,
                        float attack, float power)
    {
        int n = Count(len);
        float[] b = new float[n];
        double phase = 0.0;

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float u = (float)Math.Pow(t / len, curve);
            phase += 2 * Math.PI * (startHz + (endHz - startHz) * u) / Rate;
            b[i] = (float)Math.Sin(phase)
                   * Math.Min(1f, t / Math.Max(0.0005f, attack))
                   * Decay(t, len, power);
        }
        return b;
    }

    // A burst of filtered noise, which is the atom every mechanical sound here
    // is built out of: a tick, a slip, a bite, a splash.
    static float[] Burst(float len, float cutoff, float power)
    {
        int n = Count(len);
        float[] b = new float[n];
        for (int i = 0; i < n; i++) b[i] = Noise();
        LowPass(b, cutoff);
        for (int i = 0; i < n; i++) b[i] *= Decay(T(i), len, power);
        return b;
    }

    // Soft clip. A harmonic series of pure sines is a church organ, not a
    // winch motor: the thing that makes a motor a motor is that its load keeps
    // pushing it into the part of its range where it stops being linear.
    static void Drive(float[] b, float amount)
    {
        float norm = (float)Math.Tanh(amount);
        for (int i = 0; i < b.Length; i++) b[i] = (float)Math.Tanh(b[i] * amount) / norm;
    }

    // The level, set at the end, on the recipes whose peak cannot be read off
    // the page. Measured over the whole set, a recipe written as a sum of
    // gains lands within about thirty per cent of where you meant it to, and
    // Ring is where the other seventy per cent comes from - see the warning on
    // Ring about what it does when it is fed a sine instead of noise. Aim is a
    // single scale factor over the finished buffer, so it moves the level and
    // never the balance: the ratios the recipe was written with survive it.
    // Recipes that already land in the 0.5 to 0.8 window do not call it, which
    // is deliberate - the quiet sounds have to stay quiet relative to the loud
    // ones, and normalising everything to the same peak is how a mix ends up
    // with a footstep as loud as a hull failing.
    static void Aim(float[] b, float peak)
    {
        float top = 0f;
        for (int i = 0; i < b.Length; i++)
        {
            float v = Math.Abs(b[i]);
            if (v > top) top = v;
        }
        if (top < 1e-6f) return;

        float k = peak / top;
        for (int i = 0; i < b.Length; i++) b[i] *= k;
    }

    // Generated long, then the tail is crossfaded into the head. Kept identical
    // to the abyss script's Ambient so the two ambiences behave the same way
    // when the crossfade in AudioSystem runs them against each other.
    static float[] Wrap(float[] b, float seconds)
    {
        int keep = Count(seconds);
        int cross = b.Length - keep;
        float[] outBuf = new float[keep];
        Array.Copy(b, outBuf, keep);
        for (int i = 0; i < cross; i++)
        {
            float f = (float)i / cross;
            outBuf[i] = outBuf[i] * f + b[keep + i] * (1f - f);
        }
        return outBuf;
    }

    // Copied from the abyss script because BroodBurn needs it and a hatchery
    // burning out has to be recognisably the same animal as the things that
    // come out of it. Two copies of one function is the price of not having a
    // shared assembly, and it is cheaper than a second-best noise for a death.
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

            double freq = startHz + (endHz - startHz) * k;
            phase += 2 * Math.PI * freq / Rate;

            float wobble = 1f + rasp * (float)Math.Sin(2 * Math.PI * 38.0 * t);

            float env = Decay(t, len, 2.2f);
            float body = (float)Math.Sin(phase) * tone * wobble + noise[i] * noiseMix;

            if (crackle > 0f && rng.NextDouble() < crackle * (1.0 - k))
                body += (float)(rng.NextDouble() * 2.0 - 1.0) * 0.9f;

            b[i] = body * env * 0.62f;
        }
        Edges(b, 0.004f);
        return b;
    }

    // ------------------------------------------------------------------
    //  THE DECK
    //  Everything here is above the waterline, which means it is allowed to
    //  have treble. That is the whole difference between this file and the
    //  other one: down there a low pass eats everything over about two
    //  kilohertz, and up here rain on steel is nothing but the part a low
    //  pass would have eaten.
    // ------------------------------------------------------------------

    public static float[] DeckAmbient(float seconds)
    {
        // How the ends meet, which is the only thing that matters in a twelve
        // second loop a player will hear forty times a session. Two separate
        // mechanisms, because the layers fail differently:
        //
        // The tonal and modulated layers are made exactly periodic at the loop
        // length. Every modulator runs at k/12 Hz - the rain gusts at 3/12, the
        // swell at 2/12, the structure's slow breath at 1/12 - and the two
        // structural partials, 62 Hz and 41.5 Hz, complete 744 and 498 whole
        // cycles in twelve seconds. So at t = 12.000 every one of them is back
        // at the value and the slope it had at t = 0, and there is nothing to
        // hide. Getting this wrong is what makes a loop breathe once per lap:
        // pick 0.26 Hz instead of 0.25 and the swell arrives at the seam a
        // quarter of a cycle out, forever.
        //
        // The noise layers have no phase to get right and no way to be made
        // periodic, so they get the crossfade: the buffer is generated 0.75 s
        // long, and that overhang is faded back over the head. Because the
        // tonal layers are already identical across those 0.75 s, the crossfade
        // is a no-op for them and only ever touches the rain and the sea.
        float fade = 0.75f;
        int n = Count(seconds + fade);

        float[] rain = new float[n];
        float[] sea = new float[n];
        for (int i = 0; i < n; i++) { rain[i] = Noise(); sea[i] = Noise(); }
        HighPass(rain, 1400f);
        LowPass(sea, 260f);

        // The rig is steel standing in water, and steel standing in water has
        // two notes it makes whenever anything at all pushes on it. Taking them
        // out of the sea noise rather than adding sines means they come and go
        // with the swell for free, which is what a structure actually does.
        float[] steel = new float[n];
        Array.Copy(sea, steel, n);
        Ring(steel, 196f, 6f, 0.5f);
        Ring(steel, 293f, 9f, 0.35f);

        float[] b = new float[n];
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float gust = 0.82f + 0.18f * (float)Math.Sin(2 * Math.PI * (3.0 / 12.0) * t);
            float rise = 0.5f - 0.5f * (float)Math.Cos(2 * Math.PI * (2.0 / 12.0) * t);
            float swell = 0.35f + 0.65f * (float)Math.Pow(rise, 1.6);
            float work = 0.7f + 0.3f * (float)Math.Sin(2 * Math.PI * (1.0 / 12.0) * t);

            b[i] = rain[i] * 0.16f * gust
                 + sea[i] * 0.42f * swell
                 + steel[i] * 0.25f
                 + (0.10f * (float)Math.Sin(2 * Math.PI * 62.0 * t)
                  + 0.07f * (float)Math.Sin(2 * Math.PI * 41.5 * t)) * work;
        }

        // Nothing discrete lives in here. Anything a player could name after
        // the third lap - "that creak again" - is in deck_rig.wav, which is
        // fired on a random timer precisely so it never becomes part of the
        // loop's furniture.
        float[] outBuf = Wrap(b, seconds);
        Aim(outBuf, 0.72f);
        return outBuf;
    }

    // A boot on a wet steel plate: the boot, the plate it rings, the weight
    // behind it, and the film of water the sole pushes out of the way. Two
    // variants a semitone or so apart, alternated by the caller, because a man
    // walking with one footstep sound is a man hopping.
    public static float[] DeckStep(float len, double plate, double top)
    {
        float[] b = Burst(len, 5200f, 9.0f);
        HighPass(b, 700f);
        for (int i = 0; i < b.Length; i++) b[i] *= 0.55f;

        Ring(b, (float)plate, 40f, 0.55f);
        Ring(b, (float)top, 90f, 0.30f);

        // Eighty kilos of brass diver. Without this the step is a tap on a
        // sheet of tin, and the man on the deck weighs nothing.
        Mix(b, Tone(len, 96.0, 58.0, 1.0f, 0.001f, 6.0f), 0.0f, 0.35f);
        Mix(b, Burst(0.09f, 900f, 3.0f), 0.0f, 0.25f);

        Edges(b, 0.002f);
        return b;
    }

    public static float[] DeckRig()
    {
        float len = 2.6f;
        int n = Count(len);
        float[] b = new float[n];
        for (int i = 0; i < n; i++) b[i] = Noise();
        LowPass(b, 420f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            b[i] *= Math.Min(1f, t / 0.35f) * Decay(t, len, 1.6f) * 0.45f;
        }

        // Three members, deliberately not a chord. A rig that rang a triad
        // would be a church, and the point of this sound is that the thing you
        // are standing on was welded by a contractor to a budget.
        Ring(b, 73f, 2.5f, 0.9f);
        Ring(b, 109f, 3.5f, 0.7f);
        Ring(b, 247f, 8.0f, 0.35f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            b[i] *= 1f + 0.35f * (float)Math.Sin(2 * Math.PI * 0.9 * t + 0.7);
        }

        // Creaking is not a pitch, it is a hundred small slips. Five of them,
        // unevenly spaced, because evenly spaced is a machine and this is a
        // joint opening and closing as the swell works it.
        float[] at = { 0.31f, 0.77f, 1.05f, 1.62f, 2.10f };
        for (int k = 0; k < at.Length; k++)
            Mix(b, Burst(0.10f, 2200f, 4.0f), at[k], 0.5f);

        Edges(b, 0.02f);
        Aim(b, 0.70f);
        return b;
    }

    public static float[] DeckWinch()
    {
        float len = 2.4f;
        int n = Count(len);
        float[] b = new float[n];

        // The brake coming off. It has to come first and it has to be brighter
        // than everything after it, or the motor sounds like it started itself.
        float[] brake = Burst(0.14f, 3000f, 5.0f);
        HighPass(brake, 400f);
        Ring(brake, 420f, 60f, 0.4f);
        Mix(b, brake, 0.0f, 0.6f);

        float[] motor = new float[n];
        double phase = 0.0;
        for (int i = 0; i < n; i++)
        {
            float t = T(i);

            // Takes half a second to find its speed, holds, and drops away as
            // the load comes onto the deck. A motor at a constant pitch is a
            // fridge; the ramp is the whole reason this reads as lifting.
            double f;
            if (t < 0.5f) f = 47.0 + 14.0 * (t / 0.5);
            else if (t < 1.70f) f = 61.0;
            else if (t < 1.90f) f = 61.0 - 17.0 * ((t - 1.70) / 0.20);
            else f = 44.0;
            phase += 2 * Math.PI * f / Rate;

            float env;
            if (t < 0.10f) env = 0f;
            else if (t < 1.90f) env = Math.Min(1f, (t - 0.10f) / 0.18f);
            else env = Math.Max(0f, 1f - (t - 1.90f) / 0.15f);

            motor[i] = (float)(Math.Sin(phase) * 0.50
                             + Math.Sin(2 * phase) * 0.25
                             + Math.Sin(3 * phase) * 0.16
                             + Math.Sin(4 * phase) * 0.10) * env;
        }
        Drive(motor, 2.2f);

        // Half rather than all of it: Drive normalises its output back to unity
        // peak, so mixing the motor in at 1.0 would leave no headroom at all
        // for the brake and the load, and those two are the ones that say what
        // the machine is doing.
        Mix(b, motor, 0.0f, 0.55f);

        // Chain over the drum. Twelve ticks at 0.135 s, which is a hair under
        // eight a second: fast enough to be a mechanism, slow enough that you
        // can still hear the individual links. Without them this is a motor and
        // not a winch, and a motor lifts nothing.
        for (int k = 0; k < 12; k++)
        {
            float[] tick = Burst(0.02f, 6000f, 2.0f);
            Ring(tick, 1650f, 120f, 0.5f);
            Mix(b, tick, 0.25f + 0.135f * k, 0.28f);
        }

        float[] land = new float[Count(0.5f)];
        for (int i = 0; i < land.Length; i++)
        {
            float t = T(i);
            land[i] = (float)Math.Sin(2 * Math.PI * 286.0 * t) * Decay(t, 0.5f, 2.6f) * 0.20f
                    + (float)Math.Sin(2 * Math.PI * 517.0 * t) * Decay(t, 0.4f, 3.2f) * 0.12f;
        }
        Mix(land, Burst(0.12f, 3400f, 3.0f), 0.0f, 0.5f);
        Mix(b, land, 2.00f, 1.0f);

        Edges(b, 0.006f);
        Aim(b, 0.78f);
        return b;
    }

    public static float[] DeckPaid()
    {
        float len = 1.3f;
        int n = Count(len);
        float[] b = new float[n];

        // A fifth and an octave and no third at all. A major chord here would
        // be the game congratulating you for selling salvage stripped off men
        // who did not come back up, and a minor one would be the game telling
        // you how to feel about that. The open fifth says the number is correct
        // and nothing else, which is the only honest thing a payout in this
        // game can say.
        double[] notes = { 440.00, 659.25, 880.00 };
        float[] starts = { 0.00f, 0.16f, 0.30f };
        float[] gains = { 0.16f, 0.12f, 0.09f };

        for (int k = 0; k < notes.Length; k++)
        {
            int at = Count(starts[k]);
            for (int i = at; i < n; i++)
            {
                float t = T(i - at);
                float env = Decay(t, 0.9f, 3.0f) * Math.Min(1f, t / 0.004f);
                b[i] += (float)Math.Sin(2 * Math.PI * notes[k] * t) * env * gains[k];
            }
        }

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            b[i] += (float)Math.Sin(2 * Math.PI * 110.0 * t) * Decay(t, len, 1.6f) * 0.22f;
        }

        // The stamp on the docket, which is what actually tells the player the
        // transaction happened. The chord is only how they are meant to take it.
        Mix(b, Burst(0.05f, 2400f, 4.0f), 0.0f, 0.5f);

        Edges(b, 0.004f);
        Aim(b, 0.62f);
        return b;
    }

    public static float[] DeckBoard()
    {
        float len = 0.55f;
        int n = Count(len);
        float[] b = new float[n];

        float[] latch = Burst(0.06f, 4000f, 6.0f);
        HighPass(latch, 900f);
        Ring(latch, 780f, 70f, 0.45f);
        Mix(b, latch, 0.0f, 0.55f);

        Mix(b, Tone(0.42f, 180.0, 120.0, 1.0f, 0.002f, 2.2f), 0.02f, 0.25f);

        // The filament over the board coming up. It is the quietest thing in
        // the sound and it is the reason the screen is suddenly lit: a menu
        // that appears out of a latch alone reads as a UI, and a menu that
        // appears out of a lamp reads as a man switching a lamp on.
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            b[i] += (float)Math.Sin(2 * Math.PI * 2400.0 * t)
                    * Math.Min(1f, t / 0.22f) * Decay(t, len, 1.2f) * 0.05f;
        }

        Edges(b, 0.004f);

        // At its written gains this came out at 0.24 peak, which against a
        // footstep at 0.6 and the trim the board gets in Config would have put
        // opening the upgrade screen quieter than walking to it.
        Aim(b, 0.60f);
        return b;
    }

    public static float[] BellIn()
    {
        float len = 0.9f;
        int n = Count(len);
        float[] b = new float[n];

        // Two steps, and then the whole buffer is put inside a steel sphere.
        // A step in a diving bell is not a boot noise with reverb on it, it is
        // a room: the bell has two strong modes and everything that happens in
        // there excites both of them, including the man's own boots.
        Mix(b, DeckStep(0.20f, 320.0, 1180.0), 0.00f, 0.8f);
        Mix(b, DeckStep(0.22f, 286.0, 1046.0), 0.19f, 0.6f);
        Ring(b, 165f, 12f, 0.8f);
        Ring(b, 262f, 18f, 0.5f);

        Mix(b, Burst(0.05f, 5000f, 5.0f), 0.46f, 0.6f);

        // The seal taking. This is the one place in the file where Mix is not a
        // convenience: the sub-buffer is low passed before it goes in, so the
        // first half of the sound is outside the filter and the second half is
        // behind it. The world going dull is not an effect applied to the
        // sound, it is the fact that two thirds of a second in, everything
        // arriving is arriving through eight millimetres of steel.
        float[] seal = Tone(0.30f, 140.0, 70.0, 1.0f, 0.004f, 2.0f);
        LowPass(seal, 600f);
        Mix(b, seal, 0.60f, 0.45f);

        Edges(b, 0.006f);

        // Two footsteps, a room, a hatch and a seal all land inside 0.9 s, and
        // written at their own gains they summed past full scale and clipped a
        // hundred and forty eight samples at the second step. Nothing about the
        // recipe is wrong; there is simply more happening here per second than
        // anywhere else in the file.
        Aim(b, 0.80f);
        return b;
    }

    public static float[] BellGo()
    {
        float len = 2.2f;
        int n = Count(len);
        float[] b = new float[n];

        // Laid out so it can be fired on the same frame as BellIn without the
        // two fighting: the first half second is empty, which is exactly where
        // BellIn's hatch lands, and the water does not arrive until 0.85, by
        // which time BellIn has finished. Two sounds that overlap by design
        // beat one long sound that has to cover both, because the caller gets
        // to shorten the gap later without a re-record.
        float[] take = Burst(0.14f, 3000f, 5.0f);
        Ring(take, 420f, 60f, 0.4f);
        Mix(b, take, 0.0f, 0.5f);

        float[] rope = new float[n];
        for (int i = 0; i < n; i++) rope[i] = Noise();
        LowPass(rope, 1800f);
        HighPass(rope, 300f);
        Ring(rope, 880f, 40f, 0.5f);
        for (int i = 0; i < n; i++)
        {
            float t = T(i) - 0.15f;
            if (t < 0f) { rope[i] = 0f; continue; }
            rope[i] *= Math.Min(1f, t / 0.20f) * Decay(t, 0.95f, 1.4f) * 0.35f;
        }
        for (int i = 0; i < n; i++) b[i] += rope[i];

        Mix(b, Burst(0.50f, 4200f, 2.6f), 0.85f, 0.55f);

        float[] under = new float[n];
        for (int i = 0; i < n; i++) under[i] = Noise();
        LowPass(under, 380f);
        for (int i = 0; i < n; i++)
        {
            float t = T(i) - 0.90f;
            if (t < 0f) { under[i] = 0f; continue; }
            under[i] *= Math.Min(1f, t / 0.50f) * Decay(t, 1.30f, 1.2f) * 0.60f;
        }

        // The surface closing is not a sound of its own. It is the handover
        // between the bright splash and this low passed layer: the moment one
        // is louder than the other is the moment the water is over the top of
        // him, and moving the crossover is how you move the waterline.
        for (int i = 0; i < n; i++) b[i] += under[i];

        for (int i = 0; i < n; i++)
        {
            float t = T(i) - 1.0f;
            if (t < 0f) continue;

            // The same 41 and 62 Hz the deck ambience is built on, so what goes
            // down with him is the deck itself rather than a farewell chord.
            float env = Math.Min(1f, t / 0.6f) * Decay(t, 1.2f, 1.5f) * 0.30f;
            b[i] += ((float)Math.Sin(2 * Math.PI * 62.0 * T(i))
                   + 0.6f * (float)Math.Sin(2 * Math.PI * 41.0 * T(i))) * env;
        }

        Edges(b, 0.01f);
        Aim(b, 0.80f);
        return b;
    }

    // ------------------------------------------------------------------
    //  LIFTS AND SHAFTS
    //  Two pairs of sounds with the same job: say that the machine has you,
    //  and then say whether you agreed. The held half of each pair must not
    //  fade out. A sustained sound that dies while you are still holding the
    //  key reads as the machine giving up on you, which is the opposite of
    //  what is happening.
    // ------------------------------------------------------------------

    public static float[] LiftHold()
    {
        float len = 1.15f;
        int n = Count(len);
        float[] b = new float[n];

        Mix(b, Tone(len, 88.0, 176.0, 1.4f, 0.06f, 0.0f), 0.0f, 0.30f);
        Mix(b, Tone(len, 132.0, 264.0, 1.4f, 0.06f, 0.0f), 0.0f, 0.16f);
        Ring(b, 528f, 40f, 0.35f);

        float[] squeeze = new float[n];
        for (int i = 0; i < n; i++) squeeze[i] = Noise();
        LowPass(squeeze, 900f);
        for (int i = 0; i < n; i++) b[i] += squeeze[i] * Math.Min(1f, T(i) / 0.25f) * 0.28f;

        // Cut, not faded. The file has to end mid-gesture so that a player who
        // lets go early and a player who holds to the end hear the same thing
        // happening to them, and neither hears a resolution.
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            if (t > 1.05f) b[i] *= Math.Max(0f, 1f - (t - 1.05f) / 0.10f);
        }

        Edges(b, 0.002f);
        Aim(b, 0.72f);
        return b;
    }

    public static float[] LiftGo()
    {
        float len = 1.4f;
        int n = Count(len);
        float[] b = new float[n];

        float[] clamp = Burst(0.10f, 3400f, 5.0f);
        Ring(clamp, 640f, 80f, 0.5f);
        Mix(b, clamp, 0.0f, 0.7f);

        // The octave from LiftHold, carried on rather than restarted, so the
        // confirmation is audibly the same thing one step further along and not
        // a different machine answering.
        Mix(b, Tone(0.45f, 176.0, 352.0, 1.0f, 0.01f, 0.0f), 0.0f, 0.26f);
        for (int i = Count(0.45f); i < n; i++)
        {
            float t = T(i) - 0.45f;
            b[i] += (float)Math.Sin(2 * Math.PI * 352.0 * T(i)) * 0.14f * Decay(t, 0.95f, 1.1f);
        }

        float[] past = new float[n];
        for (int i = 0; i < n; i++) past[i] = Noise();
        LowPass(past, 1500f);
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float run = t < 0.20f ? 0f : Decay(t - 0.20f, 1.20f, 1.1f);
            b[i] += past[i] * Math.Min(1f, t / 0.40f) * run * 0.45f;
        }

        // A rising fifth, and the only one in the game. Everything else that
        // moves in pitch here falls, because everything else that happens to
        // you here is worse than what came before it. This is the one moment
        // that is not, so it gets the interval nothing else is allowed.
        Mix(b, Tone(0.90f, 110.0, 165.0, 1.0f, 0.25f, 1.3f), 0.50f, 0.22f);

        Edges(b, 0.008f);
        Aim(b, 0.76f);
        return b;
    }

    public static float[] ShaftHold()
    {
        float len = 1.15f;
        int n = Count(len);
        float[] b = new float[n];

        Mix(b, Tone(len, 132.0, 66.0, 1.2f, 0.06f, 0.0f), 0.0f, 0.34f);
        Mix(b, Tone(len, 99.0, 49.5, 1.2f, 0.06f, 0.0f), 0.0f, 0.18f);

        // 420 Hz where the lift has 900. The lift is a machine that was built
        // and maintained; the shaft is a hole, and a hole has no treble.
        float[] draught = new float[n];
        for (int i = 0; i < n; i++) draught[i] = Noise();
        LowPass(draught, 420f);

        // The shaft's own note, which sits against the falling octave rather
        // than under it. Resonated on the draught alone rather than on the
        // finished buffer, for the reason written on Ring: the sweep passes
        // through 87 Hz on its way down, and a 5 Hz band handed a sine at its
        // centre frequency returns it multiplied by forty.
        Ring(draught, 87f, 5f, 0.6f);
        for (int i = 0; i < n; i++) b[i] += draught[i] * Math.Min(1f, T(i) / 0.25f) * 0.34f;

        for (int k = 0; k < 13; k++)
            Mix(b, Burst(0.03f, 3000f, 3.0f), 0.09f * k, 0.18f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            if (t > 1.05f) b[i] *= Math.Max(0f, 1f - (t - 1.05f) / 0.10f);
        }

        Edges(b, 0.002f);
        Aim(b, 0.74f);
        return b;
    }

    public static float[] ShaftGo()
    {
        float len = 1.6f;
        int n = Count(len);
        float[] b = new float[n];

        float[] grate = Burst(0.16f, 2600f, 4.0f);
        HighPass(grate, 200f);
        Mix(b, grate, 0.0f, 0.75f);

        Mix(b, Tone(len, 66.0, 28.0, 0.7f, 0.01f, 0.0f), 0.0f, 0.42f);
        Mix(b, Tone(len, 99.0, 42.0, 0.7f, 0.01f, 0.0f), 0.0f, 0.20f);

        float[] press = new float[n];
        for (int i = 0; i < n; i++) press[i] = Noise();
        LowPass(press, 260f);
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float end = t > len - 0.3f ? (len - t) / 0.3f : 1f;
            b[i] += press[i] * Math.Min(1f, t / 0.5f) * end * 0.50f;
        }

        // One thin high thread, which is the last of the surface. It is four
        // per cent of the mix and it is the only thing in the sound that is
        // above you.
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            b[i] += (float)Math.Sin(2 * Math.PI * 660.0 * t) * 0.04f * Decay(t, len, 0.8f);
        }

        // It does not resolve, because the next thing that happens is a new
        // level and the level is the resolution. The 30 ms fade is not a
        // resolution either, it is insurance: the sub-bass here is still at
        // three quarters of full level on the last sample, and cutting a 28 Hz
        // tone off in the eight milliseconds the rest of the file uses puts a
        // broadband thump on the end that reads as a speaker being unplugged.
        Edges(b, 0.03f);
        Aim(b, 0.80f);
        return b;
    }

    public static float[] ShaftOpen()
    {
        float len = 1.8f;
        int n = Count(len);
        float[] b = new float[n];

        float[] bolt = Burst(0.09f, 3800f, 6.0f);
        Ring(bolt, 240f, 50f, 0.6f);
        Ring(bolt, 393f, 70f, 0.4f);
        Mix(b, bolt, 0.0f, 0.8f);

        // The grate winding back. Nine teeth at 0.095 s, all of them through
        // one resonator, so they are nine passes of the same piece of metal
        // rather than nine unrelated ticks.
        float[] teeth = new float[n];
        for (int k = 0; k < 9; k++)
            Mix(teeth, Burst(0.035f, 4000f, 2.6f), 0.10f + 0.095f * k, 0.35f);
        Ring(teeth, 1320f, 140f, 0.30f);
        for (int i = 0; i < n; i++) b[i] += teeth[i];

        float[] below = new float[n];
        for (int i = 0; i < n; i++) below[i] = Noise();
        LowPass(below, 300f);
        for (int i = 0; i < n; i++)
        {
            float t = T(i) - 0.80f;
            if (t < 0f) continue;

            float env = Math.Min(1f, t / 0.50f) * Decay(t, 1.00f, 1.1f);
            b[i] += below[i] * env * 0.40f;

            // G1, not the A the rest of the score lives on. The way down is
            // the one direction in this game that is not home, and a shaft
            // that opened on the root would be an invitation.
            b[i] += ((float)Math.Sin(2 * Math.PI * 49.0 * T(i)) * 0.28f
                   + (float)Math.Sin(2 * Math.PI * 73.5 * T(i)) * 0.18f) * env;
        }

        Edges(b, 0.01f);
        Aim(b, 0.78f);
        return b;
    }

    // ------------------------------------------------------------------
    //  SALVAGE
    //  Three sounds for one action, picked by value. The player never sees a
    //  number until they surface, so the sound is the receipt: a fourteen and
    //  a three hundred and seventy have to be tellable apart across a dark
    //  room with a fight going on.
    // ------------------------------------------------------------------

    // The same grab in all three, so it is audibly one hand doing one thing and
    // only what it closed on changed.
    static float[] Grab()
    {
        return Burst(0.04f, 3200f, 4.0f);
    }

    public static float[] FindSmall()
    {
        float len = 0.34f;
        int n = Count(len);
        float[] b = new float[n];
        Mix(b, Grab(), 0.0f, 0.35f);

        // 520 against 1180 is not quite a twelfth and is meant to sit wrong.
        // It is scrap. Picking it up should feel like picking up a brick.
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            b[i] += (float)Math.Sin(2 * Math.PI * 520.0 * t) * Decay(t, len, 4.0f) * 0.30f
                  + (float)Math.Sin(2 * Math.PI * 1180.0 * t) * Decay(t, 0.28f, 5.0f) * 0.12f;
        }

        Edges(b, 0.003f);

        // The three finds are aimed at 0.46, 0.55 and 0.68 rather than left at
        // whatever their recipes came to, because they are the only sounds in
        // the game that have to carry a number. They share one volume trim in
        // Config, so the file is the only place the difference between a
        // fourteen and a three hundred and seventy can live, and ringing twice
        // and lasting three times as long turned out not to be enough on its
        // own with a fight going on over the top of it.
        Aim(b, 0.46f);
        return b;
    }

    public static float[] FindGood()
    {
        float len = 0.55f;
        int n = Count(len);
        float[] b = new float[n];
        Mix(b, Grab(), 0.0f, 0.35f);

        // It rings twice. That is the entire difference from find_small, and
        // it is enough: one strike is an object, two strikes is an object that
        // somebody would pay for.
        float[] starts = { 0.00f, 0.12f };
        float[] scale = { 1.0f, 0.45f };
        for (int k = 0; k < starts.Length; k++)
        {
            int at = Count(starts[k]);
            for (int i = at; i < n; i++)
            {
                float t = T(i - at);
                float env = Decay(t, len, 3.0f) * Math.Min(1f, t / 0.003f);
                b[i] += ((float)Math.Sin(2 * Math.PI * 523.25 * t) * 0.22f
                       + (float)Math.Sin(2 * Math.PI * 784.00 * t) * 0.14f) * env * scale[k];
            }
        }

        Ring(b, 1568f, 110f, 0.25f);
        Edges(b, 0.004f);
        Aim(b, 0.55f);
        return b;
    }

    public static float[] FindRich()
    {
        float len = 1.10f;
        int n = Count(len);
        float[] b = new float[n];
        Mix(b, Grab(), 0.0f, 0.35f);

        double[] notes = { 440.0, 660.0, 880.0, 1320.0 };
        float[] gains = { 0.16f, 0.12f, 0.08f, 0.05f };

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float env = Decay(t, len, 2.0f) * Math.Min(1f, t / 0.010f);

            for (int k = 0; k < notes.Length; k++)
            {
                // The fifth trembles by four thousandths. It is the only
                // glitter in the game and a perfectly steady one would sound
                // like a coin machine rather than like something alive being
                // worth money.
                double hz = k == 1 ? notes[k] * (1.0 + 0.004 * Math.Sin(2 * Math.PI * 1.7 * t)) : notes[k];
                b[i] += (float)Math.Sin(2 * Math.PI * hz * t) * env * gains[k];
            }

            // The root of the whole score underneath, so that even the best
            // thing that can happen to you down here still belongs to this game.
            b[i] += (float)Math.Sin(2 * Math.PI * 55.0 * t) * Decay(t, len, 1.4f) * 0.18f;
        }

        Edges(b, 0.006f);
        Aim(b, 0.68f);
        return b;
    }

    // ------------------------------------------------------------------
    //  CUTTING, AND WHAT COMES OUT
    // ------------------------------------------------------------------

    public static float[] CutHull()
    {
        float len = 0.30f;
        int n = Count(len);
        float[] b = new float[n];
        for (int i = 0; i < n; i++) b[i] = Noise();
        LowPass(b, 5600f);
        HighPass(b, 1100f);

        Ring(b, 1960f, 200f, 0.35f);
        Ring(b, 2940f, 260f, 0.20f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);

            // Retriggered every 0.22 s against a 0.30 s file, so it overlaps
            // itself by eighty milliseconds and never stops. The envelope is a
            // plateau with soft shoulders for exactly that reason, and Edges is
            // deliberately not called: a two millisecond fade at each end would
            // punch a hole through the middle of the overlap five times a
            // second, which is audible as a flutter and impossible to find
            // later if you do not know it is there.
            float env = Math.Min(1f, t / 0.04f) * Math.Min(1f, (len - t) / 0.05f);

            // Gas boiling at the tip. 23 Hz is under the pitch floor, so it is
            // heard as the flame being unsteady rather than as a note.
            float gas = (float)Math.Sin(2 * Math.PI * 74.0 * t) * 0.16f
                        * (1f + 0.5f * (float)Math.Sin(2 * Math.PI * 23.0 * t));

            b[i] = b[i] * 0.42f * env + gas * env;
        }
        return b;
    }

    public static float[] HullGive()
    {
        float len = 1.9f;
        int n = Count(len);
        float[] b = new float[n];

        // Plate coming apart is a resonance being destroyed, not a bang: the
        // two rings are what the sheet was ringing at right up until it stopped
        // being one sheet.
        float[] tear = Burst(0.30f, 4200f, 2.4f);
        Ring(tear, 196f, 14f, 0.7f);
        Ring(tear, 311f, 22f, 0.5f);
        Mix(b, tear, 0.0f, 0.85f);

        // The seam running. Seven, each quieter than the last, all through one
        // resonator so the crack is travelling along the same piece of steel.
        float[] seam = new float[n];
        for (int k = 0; k < 7; k++)
            Mix(seam, Burst(0.06f, 3000f, 3.0f), 0.12f + 0.11f * k, 0.55f - 0.05f * k);
        Ring(seam, 880f, 90f, 0.4f);
        for (int i = 0; i < n; i++) b[i] += seam[i];

        float[] flood = new float[n];
        for (int i = 0; i < n; i++) flood[i] = Noise();
        LowPass(flood, 700f);
        for (int i = 0; i < n; i++)
        {
            float t = T(i) - 0.55f;
            if (t < 0f) continue;
            b[i] += flood[i] * Math.Min(1f, t / 0.35f) * Decay(t, 1.35f, 1.3f) * 0.55f;
        }

        // And the wreck settling onto whatever is under it, which is the part
        // that says the hole is permanent.
        for (int i = 0; i < n; i++)
        {
            float t = T(i) - 0.70f;
            if (t < 0f) continue;

            float env = Math.Min(1f, t / 0.40f) * Decay(t, 1.20f, 1.5f) * 0.35f;
            b[i] += ((float)Math.Sin(2 * Math.PI * 58.0 * T(i))
                   + 0.5f * (float)Math.Sin(2 * Math.PI * 87.0 * T(i))) * env;
        }

        Edges(b, 0.008f);
        Aim(b, 0.82f);
        return b;
    }

    public static float[] BroodBurn()
    {
        float len = 1.6f;
        int n = Count(len);
        float[] b = new float[n];

        Mix(b, Tone(0.55f, 300.0, 46.0, 1.0f, 0.002f, 2.0f), 0.0f, 0.50f);

        // Wet, and boiling rather than burning. The 17 Hz on the hiss is the
        // difference: a steady hiss is a gas jet and a hiss that gulps is
        // something with fluid in it going off.
        float[] hiss = new float[n];
        for (int i = 0; i < n; i++) hiss[i] = Noise();
        LowPass(hiss, 2200f);
        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            b[i] += hiss[i] * Math.Min(1f, t / 0.05f) * Decay(t, len, 1.8f) * 0.60f
                    * (1f + 0.45f * (float)Math.Sin(2 * Math.PI * 17.0 * t));
        }

        float[] at = { 0.25f, 0.48f, 0.73f, 1.05f };
        for (int k = 0; k < at.Length; k++)
            Mix(b, Burst(0.22f, 1400f, 2.8f), at[k], 0.45f - 0.07f * k);

        // The hatchery is an animal, so it gets the animals' death sound. Two
        // recordings of one creature is how a soundtrack stops being coherent.
        Mix(b, DeathVoice(1.20f, 120.0, 38.0, 0.45f, 0.50f, 900.0f, 0.35f, 0.0f), 0.40f, 0.60f);

        Edges(b, 0.01f);
        Aim(b, 0.80f);
        return b;
    }

    public static float[] RigLog()
    {
        float len = 1.10f;
        int n = Count(len);
        float[] b = new float[n];

        // The only machine in the game that still works, which is why it is the
        // smallest and driest sound in it. A click is a tiny resonance and not
        // a crack: 3100 Hz at 400 Hz of bandwidth rings for about five
        // milliseconds, which is a contact closing.
        for (int k = 0; k < 2; k++)
        {
            float[] click = Burst(0.012f, 8000f, 3.0f);
            HighPass(click, 2000f);
            Ring(click, 3100f, 400f, 0.4f);

            // Two contacts, 60 ms apart. One click on its own is a finger tap
            // on a desk; two is a switch with a mechanism behind it.
            Mix(b, click, 0.06f * k, k == 0 ? 0.7f : 0.35f);
        }

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            if (t < 0.10f) continue;

            // A capstan that has been in salt air for years: 6.2 Hz of wow,
            // which is a bearing that is not quite round any more.
            float sick = 1f + 0.08f * (float)Math.Sin(2 * Math.PI * 6.2 * t);
            b[i] += ((float)Math.Sin(2 * Math.PI * 38.0 * t * sick) * 0.14f
                   + (float)Math.Sin(2 * Math.PI * 76.0 * t * sick) * 0.07f);
        }

        float[] tape = new float[n];
        for (int i = 0; i < n; i++) tape[i] = Noise();
        HighPass(tape, 2600f);
        for (int i = 0; i < n; i++)
        {
            float t = T(i) - 0.12f;
            if (t < 0f) continue;

            // No decay. The tape is still running when the file ends, because
            // the player is reading a line of a dead man's log over the top of
            // it and a recorder that stopped would say the line was over.
            b[i] += tape[i] * Math.Min(1f, t / 0.10f) * 0.10f;
        }

        Edges(b, 0.006f);

        // Dry and small is the intent, inaudible is not: two clicks and a 38 Hz
        // capstan came to 0.27 peak, and this one plays under a wall of log
        // text that the player is supposed to notice has started.
        Aim(b, 0.55f);
        return b;
    }

    // ------------------------------------------------------------------
    //  THE REELS
    //  Diegetic. Every one of these is a picture the player is looking at,
    //  which is why none of them is scored: the music for the reels is one
    //  separate loop underneath, and these are what is in the frame.
    // ------------------------------------------------------------------

    public static float[] ReelHorn()
    {
        float len = 2.6f;
        int n = Count(len);
        float[] b = new float[n];

        float[] rain = new float[n];
        for (int i = 0; i < n; i++) rain[i] = Noise();
        HighPass(rain, 1500f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float env = Math.Min(1f, t / 0.35f) * Decay(t, len, 1.2f);

            // G2 and D3, deliberately off the A everything else in this game is
            // tuned to. The horn belongs to the company that owns the boat, and
            // the company is not part of what is down there.
            float sag = 1f - 0.03f * (t / len);

            b[i] += ((float)Math.Sin(2 * Math.PI * 98.0 * t * sag) * 0.26f
                   + (float)Math.Sin(2 * Math.PI * 147.0 * t * sag) * 0.18f
                   + (float)Math.Sin(2 * Math.PI * 196.0 * t * sag) * 0.08f) * env;

            // Flat, not enveloped. Rain does not know a shot is happening.
            b[i] += rain[i] * 0.10f;
        }

        Edges(b, 0.02f);
        Aim(b, 0.62f);
        return b;
    }

    public static float[] ReelFall()
    {
        float len = 2.3f;
        int n = Count(len);
        float[] b = new float[n];

        float[] press = new float[n];
        for (int i = 0; i < n; i++) press[i] = Noise();

        // Twice, because one pole is 6 dB per octave and that is not dark. A
        // single pass at 520 Hz leaves noise only 12 dB down at 2 kHz, which is
        // audibly hiss rather than pressure - and worse, it fills the band that
        // ReelClimb's air layer has to arrive into for that shot to work at
        // all. The pair of them is 12 dB per octave, which is enough.
        LowPass(press, 520f);
        LowPass(press, 520f);
        for (int i = 0; i < n; i++)
        {
            float t = T(i);

            // Rises the whole way and never decays. This is the shot where the
            // number on the depth gauge is going up, and a pressure layer that
            // faded would be the descent ending.
            // The 1.4 puts back what the second pole took out, so the balance
            // against the rock and the falling octave is still the one the
            // recipe was written with.
            b[i] += press[i] * Math.Min(1f, t / 0.50f) * (0.35f + 0.35f * t / len) * 1.4f;
        }

        // Rock going past, nine times. 0.23 s apart is just under the spacing
        // at which the ear starts counting events instead of feeling a rate,
        // which is the difference between falling and being shown nine rocks.
        for (int k = 0; k < 9; k++)
            Mix(b, Burst(0.16f, 1800f, 2.6f), 0.18f + 0.23f * k, 0.22f + 0.03f * k);

        Mix(b, Tone(len, 82.0, 44.0, 1.0f, 0.05f, 0.0f), 0.0f, 0.30f);
        Mix(b, Tone(len, 123.0, 66.0, 1.0f, 0.05f, 0.0f), 0.0f, 0.16f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            b[i] += (float)Math.Sin(2 * Math.PI * 990.0 * t) * 0.05f * Decay(t, 1.40f, 1.6f);
        }

        Edges(b, 0.03f);
        Aim(b, 0.78f);
        return b;
    }

    public static float[] ReelClimb()
    {
        float len = 2.3f;
        int n = Count(len);
        float[] b = new float[n];

        float[] press = new float[n];
        for (int i = 0; i < n; i++) press[i] = Noise();

        // Two poles, as in ReelFall, and here it is not a nicety. The whole
        // shot is one layer above 2 kHz arriving over a layer below 520 Hz, and
        // with a single pole the dark layer's own leakage above 2 kHz was most
        // of what was up there already.
        LowPass(press, 520f);
        LowPass(press, 520f);

        float[] air = new float[n];
        for (int i = 0; i < n; i++) air[i] = Noise();
        HighPass(air, 2000f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            b[i] += press[i] * Decay(t, len, 1.4f) * 0.55f * 1.4f;

            // The top of the buffer getting brighter is the shot. Nothing else
            // in this file says "surface": the water thinning out is a low pass
            // handing over to a high pass, and that handover is the picture.
            if (t > 1.60f) b[i] += air[i] * Math.Min(1f, (t - 1.60f) / 0.70f) * 0.12f;
        }

        Mix(b, Tone(len, 44.0, 88.0, 1.0f, 0.05f, 0.0f), 0.0f, 0.28f);
        Mix(b, Tone(len, 66.0, 132.0, 1.0f, 0.05f, 0.0f), 0.0f, 0.15f);

        // Six things going past, and the gaps get wider every time. Widening
        // gaps are things leaving; narrowing gaps are things arriving. It is
        // the same six bursts either way round and it is the only reason this
        // reads as an escape rather than as a descent played backwards.
        float[] at = { 0.25f, 0.60f, 0.98f, 1.39f, 1.83f, 2.28f };
        for (int k = 0; k < at.Length; k++)
            Mix(b, Burst(0.28f, 900f, 2.2f), at[k], 0.30f);

        Edges(b, 0.03f);
        Aim(b, 0.76f);
        return b;
    }

    public static float[] ReelDawn()
    {
        float len = 3.0f;
        int n = Count(len);
        float[] b = new float[n];

        float[] dark = new float[n];
        for (int i = 0; i < n; i++) dark[i] = Noise();
        LowPass(dark, 380f);
        LowPass(dark, 380f);

        float[] light = new float[n];
        for (int i = 0; i < n; i++) light[i] = Noise();
        HighPass(light, 1200f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);

            // The light comes and goes twice before it stays. Surfacing under a
            // swell is not a fade up, it is two near misses and then air.
            float lift = 0.5f - 0.5f * (float)Math.Cos(2 * Math.PI * 0.45 * t);
            b[i] += dark[i] * Decay(t, len, 1.2f) * 0.45f * 1.4f * lift;
            b[i] += light[i] * Math.Min(1f, t / 2.20f) * 0.16f;

            // A2 and E3. The first consonance in the game, three hours in, and
            // the only place the score is allowed to agree with itself.
            float env = Math.Min(1f, t / 1.20f) * Decay(t, len, 0.9f);
            b[i] += ((float)Math.Sin(2 * Math.PI * 110.0 * t) * 0.20f
                   + (float)Math.Sin(2 * Math.PI * 165.0 * t) * 0.14f
                   + (float)Math.Sin(2 * Math.PI * 220.0 * t) * 0.07f) * env;
        }

        Edges(b, 0.02f);

        // Calm, not absent. It shares a volume trim with ReelFall and
        // ReelClimb, and at its written gains it came out 7 dB under them,
        // which would have made the one shot in the game where the player
        // reaches air the quietest thing in the reel.
        Aim(b, 0.58f);
        return b;
    }

    public static float[] ReelMark()
    {
        float len = 2.8f;
        int n = Count(len);
        float[] b = new float[n];

        float[] breath = new float[n];
        for (int i = 0; i < n; i++) breath[i] = Noise();
        LowPass(breath, 240f);

        for (int i = 0; i < n; i++)
        {
            float t = T(i);
            float env = Math.Min(1f, t / 0.02f) * Decay(t, len, 1.1f);

            // Root, octave, fifth, double octave. No third, which is the whole
            // trick: this card ends the reel that says the trench is not a
            // trench and it also ends the reel that says not for long, and a
            // third would commit it to one of them.
            b[i] += ((float)Math.Sin(2 * Math.PI * 55.0 * t) * 0.40f
                   + (float)Math.Sin(2 * Math.PI * 110.0 * t) * 0.22f
                   + (float)Math.Sin(2 * Math.PI * 165.0 * t) * 0.14f) * env;

            // The top leaves first, so the chord sinks as it goes rather than
            // holding still and getting quieter.
            b[i] += (float)Math.Sin(2 * Math.PI * 220.0 * t) * 0.08f * Decay(t, len, 2.4f);

            b[i] += breath[i] * Decay(t, len, 0.8f) * 0.22f;
        }

        // The metal the title is punched into.
        float[] stamp = Burst(0.70f, 6000f, 3.0f);
        Ring(stamp, 1650f, 60f, 0.5f);
        Mix(b, stamp, 0.0f, 0.35f);

        Edges(b, 0.02f);
        Aim(b, 0.80f);
        return b;
    }

    // ------------------------------------------------------------------
    //  MUSIC FOR THE REELS
    //  A fifth layer of the existing score rather than a separate player, so
    //  it inherits the fade, the mute and the volume trim that updateMusic
    //  already does for the other four and needs no code of its own.
    // ------------------------------------------------------------------

    // There is no Loop helper in this file, and its absence is the point. The
    // abyss script folds a finished buffer's last 0.6 s back over its first
    // 0.6 s, which works only because every partial in those pieces completes a
    // whole number of cycles in the loop: on a buffer that is already exactly
    // periodic the fold is not a no-op, it is damage. It replaces sample 0 with
    // the sample from 0.6 s before the end, so playback wraps from the last
    // sample to a sample 26460 places earlier and the seam acquires a step it
    // did not have. Measured on the first cut of music_reel that step was 0.148
    // against a largest internal step of 0.0196 - seven and a half times the
    // roughest thing the piece does anywhere else, which is a click. Written
    // periodic and left alone, the same file joins at 0.0002.

    // One bar's worth of raised cosine window, wrapped. Returned as a plain
    // static because PowerShell 5.1 compiles this against a C# 5 compiler and
    // local functions do not exist there.
    static float BarWindow(float t, int bar, float barLen, float loopLen, float cross)
    {
        float centre = barLen * bar + barLen * 0.5f;
        float d = t - centre;
        while (d < -loopLen * 0.5f) d += loopLen;
        while (d >= loopLen * 0.5f) d -= loopLen;
        d = Math.Abs(d);

        float flat = (barLen - cross) * 0.5f;
        float edge = (barLen + cross) * 0.5f;
        if (d <= flat) return 1f;
        if (d >= edge) return 0f;
        return 0.5f + 0.5f * (float)Math.Cos(Math.PI * (d - flat) / cross);
    }

    public static float[] MusicReel(float seconds)
    {
        int n = Count(seconds);
        float[] b = new float[n];

        float barLen = seconds / 4f;
        float cross = 1.6f;

        // A minor, F major, D minor, E minor, on a pedal A that never moves.
        // E minor and not E major is the entire composition: with no G sharp
        // there is no leading note, so there is no cadence, so arriving back at
        // bar one sounds like continuing rather than like arriving. That is
        // what lets the same twenty seconds sit under the reel where everything
        // goes wrong and the reel where you get out, because neither a
        // resolution nor a collapse is ever offered. i - bVI - iv - v is a
        // rotation and not a progression: it has no origin and no destination,
        // so it does not wear out on the fourth lap.
        double[,] chords = {
            { 220.00, 261.60, 329.60 },
            { 174.60, 220.00, 261.60 },
            { 146.80, 174.60, 220.00 },
            { 164.80, 196.00, 246.95 }
        };
        float[] gains = { 0.13f, 0.11f, 0.09f };

        // Every frequency in this piece completes a whole number of cycles in
        // twenty seconds, which is why they are 261.60 and 164.80 and 246.95
        // rather than the 261.63 and 164.81 and 246.94 an equal tempered table
        // would give. That is at worst two tenths of a cent out, which nobody
        // has ever heard, and it buys a loop that is exactly periodic: the last
        // sample runs into the first with no phase step at all, so the join
        // needs no crossfade and cannot pump. Every modulator obeys the same
        // rule - the bell's 0.05 Hz breath is one cycle per loop and the salt's
        // window is one raised cosine per loop.
        for (int i = 0; i < n; i++)
        {
            float t = T(i);

            b[i] += (float)Math.Sin(2 * Math.PI * 55.00 * t) * 0.30f
                  + (float)Math.Sin(2 * Math.PI * 110.00 * t) * 0.18f
                  + (float)Math.Sin(2 * Math.PI * 164.80 * t) * 0.12f;

            for (int bar = 0; bar < 4; bar++)
            {
                float win = BarWindow(t, bar, barLen, seconds, cross);
                if (win <= 0f) continue;

                // Adjacent windows are complementary halves of one raised
                // cosine and sum to exactly one, so no chord is ever struck -
                // they appear and are replaced. Nothing in the piece has an
                // attack, because an attack is an event and a cutscene already
                // has all the events it needs on screen.
                for (int k = 0; k < 3; k++)
                    b[i] += (float)Math.Sin(2 * Math.PI * chords[bar, k] * t) * gains[k] * win;
            }

            // One high thing, breathing once across the loop. The player's
            // score has no treble at all by design; the reels are the one place
            // the game is allowed some air.
            b[i] += (float)Math.Sin(2 * Math.PI * 880.0 * t) * 0.035f
                    * (0.5f + 0.5f * (float)Math.Sin(2 * Math.PI * 0.05 * t));
        }

        // Salt. Glues the piece to the ambient drone that is still running
        // underneath it, and windowed to nothing at both ends so the one layer
        // that cannot be made periodic contributes nothing at the seam.
        float[] salt = new float[n];
        for (int i = 0; i < n; i++) salt[i] = Noise();
        LowPass(salt, 160f);
        for (int i = 0; i < n; i++)
            b[i] += salt[i] * 0.10f * (0.5f - 0.5f * (float)Math.Cos(2 * Math.PI * T(i) / seconds));

        Aim(b, 0.75f);
        return b;
    }
}
"@

"generating surface audio..."

$jobs = @(
    # Deck.
    @("ambient_deck.wav",  [SurfaceSynth]::DeckAmbient(12.0)),
    @("deck_step_a.wav",   [SurfaceSynth]::DeckStep(0.20, 320.0, 1180.0)),
    @("deck_step_b.wav",   [SurfaceSynth]::DeckStep(0.22, 286.0, 1046.0)),
    @("deck_rig.wav",      [SurfaceSynth]::DeckRig()),
    @("deck_winch.wav",    [SurfaceSynth]::DeckWinch()),
    @("deck_paid.wav",     [SurfaceSynth]::DeckPaid()),
    @("deck_board.wav",    [SurfaceSynth]::DeckBoard()),
    @("deck_bell_in.wav",  [SurfaceSynth]::BellIn()),
    @("deck_bell_go.wav",  [SurfaceSynth]::BellGo()),

    # Lifts, shafts, salvage, cutting.
    @("lift_hold.wav",     [SurfaceSynth]::LiftHold()),
    @("lift_go.wav",       [SurfaceSynth]::LiftGo()),
    @("shaft_hold.wav",    [SurfaceSynth]::ShaftHold()),
    @("shaft_go.wav",      [SurfaceSynth]::ShaftGo()),
    @("shaft_open.wav",    [SurfaceSynth]::ShaftOpen()),
    @("find_small.wav",    [SurfaceSynth]::FindSmall()),
    @("find_good.wav",     [SurfaceSynth]::FindGood()),
    @("find_rich.wav",     [SurfaceSynth]::FindRich()),
    @("cut_hull.wav",      [SurfaceSynth]::CutHull()),
    @("hull_give.wav",     [SurfaceSynth]::HullGive()),
    @("brood_burn.wav",    [SurfaceSynth]::BroodBurn()),
    @("rig_log.wav",       [SurfaceSynth]::RigLog()),

    # Cutscenes.
    @("reel_horn.wav",     [SurfaceSynth]::ReelHorn()),
    @("reel_fall.wav",     [SurfaceSynth]::ReelFall()),
    @("reel_climb.wav",    [SurfaceSynth]::ReelClimb()),
    @("reel_dawn.wav",     [SurfaceSynth]::ReelDawn()),
    @("reel_mark.wav",     [SurfaceSynth]::ReelMark()),
    @("music_reel.wav",    [SurfaceSynth]::MusicReel(20.0))
)

foreach ($job in $jobs) {
    $path = Join-Path $OutDir $job[0]
    [SurfaceSynth]::WriteWav($path, $job[1])
    $size = [math]::Round((Get-Item $path).Length / 1KB, 1)
    "  $($job[0])  ${size} KB"
}
"done"
