#pragma once
// Dattorro-style plate reverb engine (Dattorro, "Effect Design Part 1", JAES 1997).
// Pure C++, no JUCE dependency. Stereo in / stereo out.
#include <vector>
#include <cmath>
#include <algorithm>

namespace plate
{
constexpr float kPi = 3.14159265358979f;

class Delay
{
public:
    void init (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        pos = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void write (float v) { buf[(size_t) pos] = v; pos = (pos + 1) & mask; }
    // d = 1 is the most recently written sample
    inline float read (int d) const { return buf[(size_t) ((pos - d) & mask)]; }
    inline float readFrac (float d) const
    {
        const int i = (int) d;
        const float f = d - (float) i;
        const float a = buf[(size_t) ((pos - i) & mask)];
        const float b = buf[(size_t) ((pos - i - 1) & mask)];
        return a + f * (b - a);
    }
private:
    std::vector<float> buf;
    int mask = 0, pos = 0;
};

struct Allpass
{
    Delay d; int n = 1;
    void init (int len, int maxExtra = 0) { n = std::max (len, 2); d.init (n + maxExtra + 4); }
    inline float process (float x, float g)
    {
        const float z = d.read (n);
        const float w = x - g * z;
        d.write (w);
        return z + g * w;
    }
    inline float processMod (float x, float g, float mod)
    {
        const float z = d.readFrac ((float) n + mod);
        const float w = x - g * z;
        d.write (w);
        return z + g * w;
    }
};

struct OnePoleLP
{
    float z = 0.0f;
    inline float process (float x, float a) { z = (1.0f - a) * x + a * z; return z; }   // a = pole
    static float coeff (float hz, float sr) { return std::exp (-2.0f * kPi * std::min (hz, 0.49f * sr) / sr); }
};

struct Params
{
    float predelayMs = 10.0f;
    float decay      = 0.6f;     // 0..1 (tank feedback gain, mapped internally)
    float dampHz     = 6000.0f;  // tank high-frequency damping
    float bandwidthHz= 14000.0f; // input lowpass
    float diffusion  = 1.0f;     // 0..1
    float modDepth   = 0.3f;     // 0..1
    float lowCutHz   = 80.0f;    // wet highpass
    float highCutHz  = 12000.0f; // wet lowpass
    float width      = 1.0f;     // 0..1
    float mix        = 0.3f;     // 0..1
};

class PlateReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const float k = sr / 29761.0f;      // Dattorro's reference rate
        auto S = [k] (int v) { return std::max (2, (int) std::lround ((float) v * k)); };
        scale = k;

        pre.init ((int) (0.3f * sr) + 4);
        const int in[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i) inAp[i].init (S (in[i]));

        const int modRoom = (int) std::ceil (14.0f * k) + 2;
        apf1L.init (S (672), modRoom);  apf1R.init (S (908), modRoom);
        dl1L.init (S (4453));           dl1R.init (S (4217));
        apf2L.init (S (1800));          apf2R.init (S (2656));
        dl2L.init (S (3720));           dl2R.init (S (3163));
        nDl1L = S (4453); nDl1R = S (4217); nDl2L = S (3720); nDl2R = S (3163);

        auto T = [k] (int v) { return std::max (1, (int) std::lround ((float) v * k)); };
        tL[0] = T (266);  tL[1] = T (2974); tL[2] = T (1913); tL[3] = T (1996); tL[4] = T (1990); tL[5] = T (187);  tL[6] = T (1066);
        tR[0] = T (353);  tR[1] = T (3627); tR[2] = T (1228); tR[3] = T (2673); tR[4] = T (2111); tR[5] = T (335);  tR[6] = T (121);

        smoothA = std::exp (-1.0f / (0.03f * sr));
        reset();
        cur = target;
    }

    void reset()
    {
        pre.clear();
        for (auto& a : inAp) a.d.clear();
        for (auto* a : { &apf1L, &apf1R, &apf2L, &apf2R }) a->d.clear();
        for (auto* d : { &dl1L, &dl1R, &dl2L, &dl2R }) d->clear();
        bw = dampL = dampR = hpL = hpR = lpL = lpR = OnePoleLP{};
        lastL = lastR = 0.0f; lfoPhase = 0.0f;
    }

    void setParams (const Params& p) { target = p; }

    void process (float inL, float inR, float& outL, float& outR)
    {
        // parameter smoothing
        auto sm = [this] (float& c, float t) { c = t + smoothA * (c - t); };
        sm (cur.predelayMs, target.predelayMs); sm (cur.decay, target.decay);
        sm (cur.dampHz, target.dampHz);         sm (cur.bandwidthHz, target.bandwidthHz);
        sm (cur.diffusion, target.diffusion);   sm (cur.modDepth, target.modDepth);
        sm (cur.lowCutHz, target.lowCutHz);     sm (cur.highCutHz, target.highCutHz);
        sm (cur.width, target.width);           sm (cur.mix, target.mix);

        const float fb = 0.05f + 0.945f * cur.decay;            // tank feedback gain
        const float dampA = OnePoleLP::coeff (cur.dampHz, sr);
        const float bwA   = OnePoleLP::coeff (cur.bandwidthHz, sr);
        const float g1 = 0.75f * cur.diffusion,  g2 = 0.625f * cur.diffusion;
        const float gd1 = 0.70f * cur.diffusion, gd2 = std::clamp (fb + 0.15f, 0.25f, 0.5f) * cur.diffusion;

        // input
        const float mono = 0.5f * (inL + inR);
        const int pd = std::max (1, (int) (cur.predelayMs * 0.001f * sr));
        float x = pre.read (pd);
        pre.write (mono);
        x = bw.process (x, bwA);
        x = inAp[0].process (x, g1);
        x = inAp[1].process (x, g1);
        x = inAp[2].process (x, g2);
        x = inAp[3].process (x, g2);

        // LFOs (quadrature), ~0.8 Hz
        lfoPhase += 0.8f / sr; if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
        const float depth = cur.modDepth * 12.0f * scale;
        const float modL = depth * std::sin (2.0f * kPi * lfoPhase);
        const float modR = depth * std::cos (2.0f * kPi * lfoPhase);

        // tank: left branch
        float a = apf1L.processMod (x + lastR, -gd1, modL);
        float d1 = dl1L.read (nDl1L); dl1L.write (a);
        float b = dampL.process (d1, dampA) * fb;
        float c = apf2L.process (b, gd2);
        float d2 = dl2L.read (nDl2L); dl2L.write (c);
        const float newL = d2 * fb;

        // tank: right branch
        float a2 = apf1R.processMod (x + lastL, -gd1, modR);
        float e1 = dl1R.read (nDl1R); dl1R.write (a2);
        float b2 = dampR.process (e1, dampA) * fb;
        float c2 = apf2R.process (b2, gd2);
        float e2 = dl2R.read (nDl2R); dl2R.write (c2);
        const float newR = e2 * fb;

        lastL = newL; lastR = newR;

        // output taps
        float wl = dl1R.read (tL[0]) + dl1R.read (tL[1]) - apf2R.d.read (tL[2]) + dl2R.read (tL[3])
                 - dl1L.read (tL[4]) - apf2L.d.read (tL[5]) - dl2L.read (tL[6]);
        float wr = dl1L.read (tR[0]) + dl1L.read (tR[1]) - apf2L.d.read (tR[2]) + dl2L.read (tR[3])
                 - dl1R.read (tR[4]) - apf2R.d.read (tR[5]) - dl2R.read (tR[6]);
        wl *= 0.6f; wr *= 0.6f;

        // wet filtering: highpass (LP-subtract) then lowpass
        const float hpA = OnePoleLP::coeff (cur.lowCutHz, sr);
        const float lpA = OnePoleLP::coeff (cur.highCutHz, sr);
        wl -= hpL.process (wl, hpA);  wr -= hpR.process (wr, hpA);
        wl = lpL.process (wl, lpA);   wr = lpR.process (wr, lpA);

        // width
        const float m = 0.5f * (wl + wr), s = 0.5f * (wl - wr) * cur.width;
        wl = m + s; wr = m - s;

        // equal-power mix
        const float wetG = std::sin (cur.mix * 0.5f * kPi), dryG = std::cos (cur.mix * 0.5f * kPi);
        outL = inL * dryG + wl * wetG;
        outR = inR * dryG + wr * wetG;
    }

private:
    float sr = 44100.0f, scale = 1.0f, smoothA = 0.99f, lfoPhase = 0.0f;
    Params cur, target;
    Delay pre; Allpass inAp[4];
    Allpass apf1L, apf1R, apf2L, apf2R;
    Delay dl1L, dl1R, dl2L, dl2R;
    int nDl1L = 1, nDl1R = 1, nDl2L = 1, nDl2R = 1;
    int tL[7] {}, tR[7] {};
    OnePoleLP bw, dampL, dampR, hpL, hpR, lpL, lpR;
    float lastL = 0.0f, lastR = 0.0f;
};
} // namespace plate
