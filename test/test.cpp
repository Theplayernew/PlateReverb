// Standalone check of the engine: stability + measured RT60 vs. the Reverb Time setting.
#include "../Source/PlateReverb.h"
#include <cstdio>
int main()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (double rt : { 0.5, 1.0, 2.5, 6.0 })
        {
            plate::PlateReverb r; r.prepare (sr);
            plate::Params p; p.mix = 1.0; p.reverbTime = rt; p.dampHz = 20000; p.bandwidthHz = 20000;
            p.lowCutHz = 20; p.highCutHz = 20000; p.modDepth = 0.0; r.setParams (p);
            const int N = (int) (sr * (rt * 2.0 + 2.0));
            std::vector<double> e ((size_t) N); bool bad = false;
            for (int i = 0; i < N; ++i)
            {
                double l, rr; const double in = i == 0 ? 1.0 : 0.0;
                r.process (in, in, l, rr);
                if (! std::isfinite (l) || ! std::isfinite (rr)) bad = true;
                e[(size_t) i] = l * l + rr * rr;
            }
            // Schroeder backward integration, fit -5..-35 dB, extrapolate to 60 dB
            std::vector<double> edc ((size_t) N); double acc = 0;
            for (int i = N - 1; i >= 0; --i) { acc += e[(size_t) i]; edc[(size_t) i] = acc; }
            double t5 = -1, t35 = -1;
            for (int i = 0; i < N; ++i)
            {
                const double db = 10 * std::log10 (edc[(size_t) i] / edc[0] + 1e-30);
                if (t5 < 0 && db <= -5) t5 = i / sr;
                if (t35 < 0 && db <= -35) { t35 = i / sr; break; }
            }
            printf ("sr=%6.0f  set RT60=%.1fs  measured=%.2fs  nan=%d\n", sr, rt, (t35 - t5) * 2.0, bad);
        }
    // max-setting stability
    plate::PlateReverb r; r.prepare (48000); plate::Params p; p.mix = 1; p.reverbTime = 20; p.modDepth = 1; r.setParams (p);
    double mx = 0; for (int i = 0; i < 48000 * 40; ++i) { double l, rr; double in = i < 48000 ? ((i * 7919 % 1000) / 500.0 - 1.0) : 0; r.process (in, in, l, rr); mx = std::max (mx, std::fabs (l)); }
    printf ("20 s / full-mod noise burst: peak out = %.3f\n", mx);
}
