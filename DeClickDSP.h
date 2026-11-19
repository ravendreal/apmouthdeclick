// DeClickDSP.h — mouth-click detection & repair core (no JUCE dependency).
//
// How it works
//  1. Detection: the signal is high-passed (Frequency param) and its energy is
//     followed by a fast (~0.2 ms) and a slow (~20 ms) envelope. A mouth click
//     is a short burst where fast energy jumps far above the slow background.
//     Sensitivity sets how big that jump must be. Events longer than a few ms
//     (consonants, sibilants, plosives) are rejected as "not a click".
//  2. Repair: the detected region (+ Widen Repair padding) is replaced by
//     autoregressive (Burg LPC) interpolation: the audio before the gap is
//     predicted forward, the audio after it predicted backward, and the two are
//     cross-faded. The voice underneath continues naturally; the click is gone.
//  3. Everything runs with a fixed lookahead (latency is reported to the DAW).
#pragma once
#if defined(__GNUC__) || defined(__clang__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wconversion"
#elif defined(_MSC_VER)
 #pragma warning(push)
 #pragma warning(disable: 4244 4267)
#endif
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace declick {

constexpr double kPi = 3.14159265358979323846;

struct Params {
    float sensitivity = 4.0f;  // 0..10 (0 = off)
    float freqHz      = 2000.f; // detection high-pass cutoff
    float widenMs     = 0.0f;   // extra repair padding on each side
};

class Biquad {
public:
    void setHighPass(double sr, double fc, double q = 0.70710678) {
        const double w0 = 2.0 * kPi * fc / sr;
        const double c = std::cos(w0), s = std::sin(w0), alpha = s / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 + c) / 2.0 / a0; b1 = -(1.0 + c) / a0; b2 = b0;
        a1 = -2.0 * c / a0; a2 = (1.0 - alpha) / a0;
    }
    double process(double x) {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setLowPass(double sr, double fc, double q = 0.70710678) {
        const double w0 = 2.0 * kPi * fc / sr;
        const double c = std::cos(w0), s = std::sin(w0), alpha = s / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (1.0 - c) / 2.0 / a0; b1 = (1.0 - c) / a0; b2 = b0;
        a1 = -2.0 * c / a0; a2 = (1.0 - alpha) / a0;
    }
    void reset() { z1 = z2 = 0.0; }
private:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};

class Channel {
public:
    static constexpr double kMaxWidenMs   = 5.0;
    static constexpr double kMaxCoreMs    = 4.0;   // strong part longer than this = speech, not a click
    static constexpr double kMaxClickMs   = 9.0;   // strong part + decaying tail
    static constexpr double kPrePadMs     = 0.4;
    static constexpr double kPostPadMs    = 0.8;
    static constexpr double kHoldMs       = 0.6;   // merge bursts closer than this
    static constexpr double kContextMs    = 6.0;   // audio used to model the voice
    static constexpr double kFallMs       = 5.0;   // how fast the background estimate drops after speech

    // Fixed lookahead so latency never changes with parameters.
    static int lookaheadSamples(double sr) {
        const double ms = kPrePadMs + kMaxClickMs + kHoldMs + kPostPadMs
                        + 2.0 * kMaxWidenMs + kContextMs + 2.0;
        return (int) std::ceil(ms * 0.001 * sr);
    }

    void prepare(double sampleRate) {
        sr = sampleRate;
        L = lookaheadSamples(sr);
        int need = 4 * L + 16;
        size = 1; while (size < need) size <<= 1;
        mask = size - 1;
        wet.assign(size, 0.f); dry.assign(size, 0.f); flag.assign(size, 0);
        ctx = std::max(96, (int) std::lround(kContextMs * 0.001 * sr));
        order = std::min(32, ctx / 3);
        const int maxGap = (int) std::ceil((kPrePadMs + kMaxClickMs + kHoldMs + kPostPadMs + 2 * kMaxWidenMs) * 0.001 * sr) + 8;
        scratchA.assign(order + 1, 0.0); scratchF.assign(ctx, 0.0); scratchB.assign(ctx, 0.0);
        ctxBuf.assign(ctx, 0.0); fwd.assign(maxGap, 0.0); bwd.assign(maxGap, 0.0);
        seg.assign(maxGap + 2 * ctx, 0.0); low.assign(maxGap + 2 * ctx, 0.0);
        fastCoef = 1.0 - std::exp(-1.0 / (0.0002 * sr));
        slowCoef = 1.0 - std::exp(-1.0 / (0.020 * sr));
        midCoef  = 1.0 - std::exp(-1.0 / (0.003 * sr));
        fallCoef = 1.0 - std::exp(-1.0 / (kFallMs * 0.001 * sr));
        reset();
        lastFreq = -1.0;
    }

    void reset() {
        std::fill(wet.begin(), wet.end(), 0.f);
        std::fill(dry.begin(), dry.end(), 0.f);
        std::fill(flag.begin(), flag.end(), 0);
        hp1.reset(); hp2.reset();
        fast = slow = mid = 1e-10; n = 0; state = Idle; nPending = 0; repairs = 0;
    }

    void setParams(const Params& p) {
        enabled = p.sensitivity > 0.01f;
        // 0 -> 27 dB jump needed (very picky), 10 -> 10 dB (very aggressive)
        thrDb = 27.0 - 1.7 * (double) p.sensitivity;
        if (std::abs(p.freqHz - lastFreq) > 0.5) {
            const double fc = std::min((double) p.freqHz, 0.45 * sr);
            hp1.setHighPass(sr, fc); hp2.setHighPass(sr, fc);
            lastFreq = p.freqHz; splitHz = fc;
        }
        widen = (int) std::lround(std::clamp((double) p.widenMs, 0.0, kMaxWidenMs) * 0.001 * sr);
    }

    int latency() const { return L; }

    // Push one input sample; returns processed sample delayed by latency().
    // dryOut receives the equally delayed unprocessed sample, repaired the flag.
    float process(float x, float& dryOut, bool& repaired) {
        const int64_t i = n;
        wet[i & mask] = x; dry[i & mask] = x; flag[i & mask] = 0;
        if (enabled) detect(x, i);

        // run due repairs
        while (nPending > 0 && pending[0].readyAt <= i) {
            repair(pending[0].r0, pending[0].r1, i);
            for (int k = 1; k < nPending; ++k) pending[k - 1] = pending[k];
            --nPending;
        }

        const int64_t o = i - L;
        float y = 0.f; dryOut = 0.f; repaired = false;
        if (o >= 0) { y = wet[o & mask]; dryOut = dry[o & mask]; repaired = flag[o & mask] != 0; }
        ++n;
        return y;
    }

    int takeRepairCount() { int r = repairs; repairs = 0; return r; }

private:
    enum State { Idle, Active, Blocked };
    struct Region { int64_t r0, r1, readyAt; };

    void detect(float x, int64_t i) {
        const double h = hp2.process(hp1.process((double) x));
        const double e = h * h;
        fast += fastCoef * (e - fast);
        // Background: grows at most ~10 dB/sample-step so a 1 ms click barely moves it,
        // falls freely; frozen while a click is being tracked.
        mid += midCoef * (e - mid);
        if (state != Active) {
            if (mid < slow) slow += fallCoef * (mid - slow);                       // falls quickly after speech stops
            else            slow += slowCoef * (std::min(e, slow * 10.0 + 1e-12) - slow);
        }
        const double ratioDb = 10.0 * std::log10((fast + 1e-12) / (slow + 1e-9));
        const bool loudEnough = fast > 1e-7;                   // ignore anything below ~-70 dBFS
        const bool above = ratioDb > thrDb && loudEnough;
        const bool below = ratioDb < std::min(thrDb - 4.0, 5.0);   // back near background = click over

        const int hold = (int) (kHoldMs * 0.001 * sr);
        const int maxLen = (int) (kMaxClickMs * 0.001 * sr);
        const int maxCore = (int) (kMaxCoreMs * 0.001 * sr);

        switch (state) {
            case Idle:
                if (above) { state = Active; start = i; lastAbove = lastCore = i; }
                break;
            case Active:
                if (!below) lastAbove = i;
                if (above) lastCore = i;
                if (lastCore - start > maxCore) { state = Blocked; break; }   // too long: speech, not a click
                if (i - start > maxLen) { finish(start, i); state = Blocked; break; }  // long tail: repair what we have
                if (i - lastAbove > hold) { finish(start, lastAbove); state = Idle; }
                break;
            case Blocked:
                if (below) state = Idle;
                break;
        }
    }

    void finish(int64_t s, int64_t e) {
        const int64_t r0 = s - (int64_t) (kPrePadMs * 0.001 * sr) - widen;
        const int64_t r1 = e + (int64_t) (kPostPadMs * 0.001 * sr) + widen + 1;
        if (nPending > 0 && r0 <= pending[nPending - 1].r1 + ctx / 4) {   // merge with previous
            Region& p = pending[nPending - 1];
            p.r1 = std::max(p.r1, r1); p.readyAt = p.r1 + ctx;
            if (p.r1 - p.r0 > (int64_t) fwd.size()) p.r1 = p.r0 + (int64_t) fwd.size();
            return;
        }
        if (nPending == kMaxPending) return;
        pending[nPending++] = { r0, r1, r1 + ctx };
    }

    // Burg's method: a[0]=1, prediction x[n] = -sum a[k] x[n-k]
    void burg(const double* x, int N, int m, double* a) {
        double* f = scratchF.data(); double* b = scratchB.data();
        for (int k = 0; k < N; ++k) { f[k] = b[k] = x[k]; }
        for (int k = 0; k <= m; ++k) a[k] = 0.0;
        a[0] = 1.0;
        double Dk = 0.0;
        for (int k = 0; k < N; ++k) Dk += 2.0 * x[k] * x[k];
        Dk -= x[0] * x[0] + x[N - 1] * x[N - 1];
        for (int k = 0; k < m; ++k) {
            if (Dk <= 1e-20) break;
            double mu = 0.0;
            for (int t = 0; t <= N - k - 2; ++t) mu += f[t + k + 1] * b[t];
            mu *= -2.0 / Dk;
            for (int t = 0; t <= (k + 1) / 2; ++t) {
                const double t1 = a[t] + mu * a[k + 1 - t];
                const double t2 = a[k + 1 - t] + mu * a[t];
                a[t] = t1; a[k + 1 - t] = t2;
            }
            for (int t = 0; t <= N - k - 2; ++t) {
                const double t1 = f[t + k + 1] + mu * b[t];
                const double t2 = b[t] + mu * f[t + k + 1];
                f[t + k + 1] = t1; b[t] = t2;
            }
            Dk = (1.0 - mu * mu) * Dk - f[k + 1] * f[k + 1] - b[N - k - 2] * b[N - k - 2];
        }
    }

    // ctxBuf holds context in time order ending right before the gap; extrapolate G samples.
    void extrapolate(double* out, int G) {
        double* a = scratchA.data();
        burg(ctxBuf.data(), ctx, order, a);
        // history: last `order` samples of context
        for (int g = 0; g < G; ++g) {
            double acc = 0.0;
            for (int k = 1; k <= order; ++k) {
                const int idx = g - k;
                const double v = idx >= 0 ? out[idx] : ctxBuf[ctx + idx];
                acc -= a[k] * v;
            }
            out[g] = acc;
        }
    }

    void repair(int64_t r0, int64_t r1, int64_t now) {
        const int64_t firstUnsent = now - L + 1;
        r0 = std::max(r0, firstUnsent);
        if (r1 > now - ctx) r1 = now - ctx;           // must have right context available
        int G = (int) (r1 - r0);
        if (G <= 0) return;
        G = std::min(G, (int) fwd.size());
        r1 = r0 + G;
        if (r0 - ctx < 0) return;

        // Local segment: [left context | gap | right context]
        const int S = G + 2 * ctx;
        for (int k = 0; k < S; ++k) seg[k] = wet[(r0 - ctx + k) & mask];

        // Zero-phase low-pass split at the Frequency setting: low band (voice body) is kept,
        // only the band above it (where the click lives) is rebuilt.
        Biquad lp; lp.setLowPass(sr, splitHz);
        for (int k = 0; k < S; ++k) low[k] = lp.process(seg[k]);
        lp.reset();
        for (int k = S - 1; k >= 0; --k) low[k] = lp.process(low[k]);
        double lowE = 0.0;
        for (int k = 0; k < ctx; ++k) lowE += low[k] * low[k] + low[ctx + G + k] * low[ctx + G + k];
        const bool quiet = std::sqrt(lowE / (2.0 * ctx)) < 0.002;   // ~-54 dBFS: nothing to protect
        if (quiet) for (int k = 0; k < S; ++k) low[k] = 0.0;       // rebuild full band (also removes the "thump")
        for (int k = 0; k < S; ++k) seg[k] -= low[k];                // seg is now the band to rebuild

        for (int k = 0; k < ctx; ++k) ctxBuf[k] = seg[k];
        extrapolate(fwd.data(), G);
        for (int k = 0; k < ctx; ++k) ctxBuf[k] = seg[S - 1 - k];   // reversed right side
        extrapolate(bwd.data(), G);

        for (int g = 0; g < G; ++g) {
            const double t = (g + 1.0) / (G + 1.0);
            const double w = std::sin(0.5 * kPi * t); const double w2 = w * w;
            const double v = (1.0 - w2) * fwd[g] + w2 * bwd[G - 1 - g];
            wet[(r0 + g) & mask] = (float) (low[ctx + g] + v);
            flag[(r0 + g) & mask] = 1;
        }
        ++repairs;
    }

    double sr = 48000.0;
    int L = 0, size = 0, mask = 0, ctx = 0, order = 0, widen = 0;
    std::vector<float> wet, dry; std::vector<uint8_t> flag;
    std::vector<double> scratchA, scratchF, scratchB, ctxBuf, fwd, bwd, seg, low;
    double splitHz = 2000.0;
    Biquad hp1, hp2; double lastFreq = -1.0;
    double fast = 0, slow = 0, mid = 0, fastCoef = 0, slowCoef = 0, midCoef = 0, fallCoef = 0, thrDb = 20.0;
    bool enabled = true;
    int64_t n = 0, start = 0, lastAbove = 0, lastCore = 0;
    State state = Idle;
    static constexpr int kMaxPending = 16;
    Region pending[kMaxPending]; int nPending = 0;
    int repairs = 0;
};

} // namespace declick

#if defined(__GNUC__) || defined(__clang__)
 #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
 #pragma warning(pop)
#endif
