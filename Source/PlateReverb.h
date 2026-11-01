#pragma once
// Dattorro-style plate reverb engine (Dattorro, "Effect Design Part 1", JAES 1997).
// Pure C++, no JUCE dependency. Stereo in / stereo out.
#include <vector>
#include <cmath>
#include <algorithm>

namespace plate
{
constexpr double kPi = 3.14159265358979323846;

class Delay
{
public:
    void init (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0);
        mask = n - 1;
        pos = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0); }
    inline void write (double v) { buf[(size_t) pos] = v; pos = (pos + 1) & mask; }
    // d = 1 is the most recently written sample
    inline double read (int d) const { return buf[(size_t) ((pos - d) & mask)]; }
    inline double readFrac (double d) const
    {
        const int i = (int) d;
        const double f = d - (double) i;
        const double a = buf[(size_t) ((pos - i) & mask)];
        const double b = buf[(size_t) ((pos - i - 1) & mask)];
        return a + f * (b - a);
    }
private:
    std::vector<double> buf;
    int mask = 0, pos = 0;
};

struct Allpass
{
    Delay d; int n = 1;
    void init (int len, int maxExtra = 0) { n = std::max (len, 2); d.init (n + maxExtra + 4); }
    inline double process (double x, double g)
    {
        const double z = d.read (n);
        const double w = x - g * z;
        d.write (w);
        return z + g * w;
    }
    inline double processMod (double x, double g, double mod)
    {
        const double z = d.readFrac ((double) n + mod);
        const double w = x - g * z;
        d.write (w);
        return z + g * w;
    }
};

struct OnePoleLP
{
    double z = 0.0;
    inline double process (double x, double a) { z = (1.0 - a) * x + a * z; return z; }   // a = pole
    static double coeff (double hz, double sr) { return std::exp (-2.0 * kPi * std::min (hz, 0.49 * sr) / sr); }
};

struct Params
{
    double predelayMs = 10.0;
    double reverbTime = 2.5;     // seconds (RT60, low/mid frequencies)
    double dampHz     = 6000.0;  // tank high-frequency damping
    double bandwidthHz= 14000.0; // input lowpass
    double diffusion  = 1.0;     // 0..1
    double modDepth   = 0.3;     // 0..1
    double lowCutHz   = 80.0;    // wet highpass
    double highCutHz  = 12000.0; // wet lowpass
    double width      = 1.0;     // 0..1
    double mix        = 0.3;     // 0..1
};

class PlateReverb
{
public:
    void prepare (double sampleRate)
    {
        sr = (double) sampleRate;
        const double k = sr / 29761.0;      // Dattorro's reference rate
        auto S = [k] (int v) { return std::max (2, (int) std::lround ((double) v * k)); };
        scale = k;

        pre.init ((int) (0.3 * sr) + 4);
        const int in[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i) inAp[i].init (S (in[i]));

        const int modRoom = (int) std::ceil (14.0 * k) + 2;
        apf1L.init (S (672), modRoom);  apf1R.init (S (908), modRoom);
        dl1L.init (S (4453));           dl1R.init (S (4217));
        apf2L.init (S (1800));          apf2R.init (S (2656));
        dl2L.init (S (3720));           dl2R.init (S (3163));
        nDl1L = S (4453); nDl1R = S (4217); nDl2L = S (3720); nDl2R = S (3163);

        auto T = [k] (int v) { return std::max (1, (int) std::lround ((double) v * k)); };
        tL[0] = T (266);  tL[1] = T (2974); tL[2] = T (1913); tL[3] = T (1996); tL[4] = T (1990); tL[5] = T (187);  tL[6] = T (1066);
        tR[0] = T (353);  tR[1] = T (3627); tR[2] = T (1228); tR[3] = T (2673); tR[4] = T (2111); tR[5] = T (335);  tR[6] = T (121);

        loopSec = (double) (apf1L.n + nDl1L + apf2L.n + nDl2L + apf1R.n + nDl1R + apf2R.n + nDl2R) / sr;
        smoothA = std::exp (-1.0 / (0.03 * sr));
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
        lastL = lastR = 0.0; lfoPhase = 0.0;
    }

    void setParams (const Params& p) { target = p; }

    void process (double inL, double inR, double& outL, double& outR)
    {
        // parameter smoothing
        auto sm = [this] (double& c, double t) { c = t + smoothA * (c - t); };
        sm (cur.predelayMs, target.predelayMs); sm (cur.reverbTime, target.reverbTime);
        sm (cur.dampHz, target.dampHz);         sm (cur.bandwidthHz, target.bandwidthHz);
        sm (cur.diffusion, target.diffusion);   sm (cur.modDepth, target.modDepth);
        sm (cur.lowCutHz, target.lowCutHz);     sm (cur.highCutHz, target.highCutHz);
        sm (cur.width, target.width);           sm (cur.mix, target.mix);

        // Feedback gain from RT60: one trip round the figure-eight takes loopSec and passes
        // through 4 feedback gains (2 per branch), so  fb^4 = 10^(-3 * loopSec / RT60).
        const double rt = std::max (0.05, cur.reverbTime);
        const double fb = std::min (0.997, std::pow (10.0, -3.0 * loopSec / (4.0 * rt)));
        const double dampA = OnePoleLP::coeff (cur.dampHz, sr);
        const double bwA   = OnePoleLP::coeff (cur.bandwidthHz, sr);
        const double g1 = 0.75 * cur.diffusion,  g2 = 0.625 * cur.diffusion;
        const double gd1 = 0.70 * cur.diffusion, gd2 = std::clamp (fb + 0.15, 0.25, 0.5) * cur.diffusion;

        // input
        const double mono = 0.5 * (inL + inR);
        const int pd = std::max (1, (int) (cur.predelayMs * 0.001 * sr));
        double x = pre.read (pd);
        pre.write (mono);
        x = bw.process (x, bwA);
        x = inAp[0].process (x, g1);
        x = inAp[1].process (x, g1);
        x = inAp[2].process (x, g2);
        x = inAp[3].process (x, g2);

        // LFOs (quadrature), ~0.8 Hz
        lfoPhase += 0.8 / sr; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
        const double depth = cur.modDepth * 12.0 * scale;
        const double modL = depth * std::sin (2.0 * kPi * lfoPhase);
        const double modR = depth * std::cos (2.0 * kPi * lfoPhase);

        // tank: left branch
        double a = apf1L.processMod (x + lastR, -gd1, modL);
        double d1 = dl1L.read (nDl1L); dl1L.write (a);
        double b = dampL.process (d1, dampA) * fb;
        double c = apf2L.process (b, gd2);
        double d2 = dl2L.read (nDl2L); dl2L.write (c);
        const double newL = d2 * fb;

        // tank: right branch
        double a2 = apf1R.processMod (x + lastL, -gd1, modR);
        double e1 = dl1R.read (nDl1R); dl1R.write (a2);
        double b2 = dampR.process (e1, dampA) * fb;
        double c2 = apf2R.process (b2, gd2);
        double e2 = dl2R.read (nDl2R); dl2R.write (c2);
        const double newR = e2 * fb;

        lastL = newL; lastR = newR;

        // output taps
        double wl = dl1R.read (tL[0]) + dl1R.read (tL[1]) - apf2R.d.read (tL[2]) + dl2R.read (tL[3])
                 - dl1L.read (tL[4]) - apf2L.d.read (tL[5]) - dl2L.read (tL[6]);
        double wr = dl1L.read (tR[0]) + dl1L.read (tR[1]) - apf2L.d.read (tR[2]) + dl2L.read (tR[3])
                 - dl1R.read (tR[4]) - apf2R.d.read (tR[5]) - dl2R.read (tR[6]);
        wl *= 0.6; wr *= 0.6;

        // wet filtering: highpass (LP-subtract) then lowpass
        const double hpA = OnePoleLP::coeff (cur.lowCutHz, sr);
        const double lpA = OnePoleLP::coeff (cur.highCutHz, sr);
        wl -= hpL.process (wl, hpA);  wr -= hpR.process (wr, hpA);
        wl = lpL.process (wl, lpA);   wr = lpR.process (wr, lpA);

        // width
        const double m = 0.5 * (wl + wr), s = 0.5 * (wl - wr) * cur.width;
        wl = m + s; wr = m - s;

        // equal-power mix
        const double wetG = std::sin (cur.mix * 0.5 * kPi), dryG = std::cos (cur.mix * 0.5 * kPi);
        outL = inL * dryG + wl * wetG;
        outR = inR * dryG + wr * wetG;
    }

private:
    double sr = 44100.0, scale = 1.0, smoothA = 0.99, lfoPhase = 0.0, loopSec = 0.72;
    Params cur, target;
    Delay pre; Allpass inAp[4];
    Allpass apf1L, apf1R, apf2L, apf2R;
    Delay dl1L, dl1R, dl2L, dl2R;
    int nDl1L = 1, nDl1R = 1, nDl2L = 1, nDl2R = 1;
    int tL[7] {}, tR[7] {};
    OnePoleLP bw, dampL, dampR, hpL, hpR, lpL, lpR;
    double lastL = 0.0, lastR = 0.0;
};
} // namespace plate
