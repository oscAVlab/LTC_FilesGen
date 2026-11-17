// osc[AV]lab LTC — pruebas del motor (sin interfaz de audio)
#include <JuceHeader.h>
#include "../Source/Engine.h"
#include <cstdio>
#include <vector>

using namespace ltcapp;

struct Capture : MidiSink
{
    struct M { std::vector<uint8_t> d; uint64_t ns; uint32_t gen; };
    std::vector<M> msgs; uint32_t gen = 0;
    void push (const uint8_t* d, int len, uint64_t ns) override { msgs.push_back ({ std::vector<uint8_t> (d, d + len), ns, gen }); }
    void newGeneration() override { ++gen; }
};

static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; std::printf ("  FALLO: " __VA_ARGS__); std::printf ("\n"); } } while (0)

static std::vector<float> encode (int rid, tc::Label start, double secs, double sr, double speed = 1.0)
{
    const auto& r = tc::rate (rid);
    std::vector<float> v ((size_t) (secs * sr));
    tc::LtcEncoder e;
    e.render (v.data(), (int) v.size(), (double) tc::labelToAbs (start, r), r.real / sr * speed, r, 0.4f);
    return v;
}

static void testRoundTrip (int rid, tc::Label start)
{
    const double sr = 48000; const auto& r = tc::rate (rid);
    auto v = encode (rid, start, 3.0, sr);
    tc::LtcDecoder d; d.prepare (sr);
    std::vector<tc::LtcFrame> fr;
    d.process (v.data(), (int) v.size(), 0, [&] (const tc::LtcFrame& f) { fr.push_back (f); });
    int bad = 0; int64_t prev = INT64_MIN;
    for (auto& f : fr) { const int64_t a = tc::labelToAbs (f.label, r); if (prev != INT64_MIN && tc::wrapDiff ((double) (a - prev), r) != 1) ++bad; prev = a; }
    const int expected = (int) (3.0 * r.real) - 1;
    std::printf ("[ida/vuelta %-8s] frames %d (esperado ~%d), saltos %d, primero %s\n", r.label, (int) fr.size(), expected,
                 bad, fr.empty() ? "-" : tc::format (fr[0].label, fr[0].df).c_str());
    CHECK (std::abs ((int) fr.size() - expected) <= 2, "frames decodificados");
    CHECK (bad == 0, "secuencia");
    CHECK (! fr.empty() && fr[0].df == r.df, "flag DF");
}

struct RunResult { double meanIv, sdIv, phaseMin, phaseMax, speed; int nQF, nFF, nSets; bool running; int state; };

static RunResult runEngine (int rid, tc::Label start, double secs, double speed, int gapStartMs = -1, int gapMs = 0, int fixedRate = -1)
{
    const double sr = 48000; const int B = 256;
    auto v = encode (rid, start, secs, sr, speed);
    if (gapStartMs >= 0)
        for (size_t i = (size_t) (gapStartMs * sr / 1000); i < (size_t) ((gapStartMs + gapMs) * sr / 1000) && i < v.size(); ++i) v[i] = 0;
    Capture cap; Engine e (cap);
    e.prepare (sr, B, 0);              // comp = 256 muestras
    e.mtcRate = fixedRate;
    e.setMode (0);
    const uint64_t base = 1000000000ull;
    for (size_t pos = 0; pos + B <= v.size(); pos += B)
    {
        const float* in[1] = { v.data() + pos };
        float o[B]; float* out[1] = { o };
        e.process (in, 1, out, 1, B, base + (uint64_t) ((double) pos / sr * 1e9));
    }
    RunResult rr {};
    std::vector<Capture::M> qf; int ff = 0;
    for (auto& m : cap.msgs) { if (m.d[0] == 0xF1) qf.push_back (m); else if (m.d[0] == 0xF0) ++ff; }
    rr.nQF = (int) qf.size(); rr.nFF = ff;
    // intervalos (sólo en la última generación, zona estable)
    std::vector<double> iv;
    for (size_t i = 1; i < qf.size(); ++i) if (qf[i].gen == qf[i - 1].gen) iv.push_back (((double) qf[i].ns - (double) qf[i - 1].ns) / 1e6);
    if (iv.size() > 40) iv = std::vector<double> (iv.begin() + 20, iv.end() - 4);
    double mean = 0; for (auto x : iv) mean += x; mean /= juce::jmax ((size_t) 1, iv.size());
    double sd = 0; for (auto x : iv) sd += (x - mean) * (x - mean); sd = std::sqrt (sd / juce::jmax ((size_t) 1, iv.size()));
    rr.meanIv = mean; rr.sdIv = sd;
    // fase: la pieza 0 del frame base F debe salir cuando empieza F (entrada) menos la compensación
    const auto& r = tc::rate (rid);
    const double startAbs = (double) tc::labelToAbs (start, r);
    rr.phaseMin = 1e9; rr.phaseMax = -1e9; rr.nSets = 0;
    int nib[8]; bool have[8] = {};
    for (size_t i = 0; i < qf.size(); ++i)
    {
        const int piece = qf[i].d[1] >> 4; nib[piece] = qf[i].d[1] & 15; have[piece] = true;
        if (piece == 0) for (int k = 1; k < 8; ++k) have[k] = false;
        if (piece == 7 && have[0])
        {
            tc::Label l { nib[6] | ((nib[7] & 1) << 4), nib[4] | (nib[5] << 4), nib[2] | (nib[3] << 4), nib[0] | (nib[1] << 4) };
            const double F = tc::wrapDiff ((double) tc::labelToAbs (l, r) - startAbs, r);
            const double inSample = F / (r.real * speed) * sr;          // inicio del frame en la entrada
            const double expectNs = (double) base + (inSample - B) / sr * 1e9;
            const double ph = ((double) qf[i - 7].ns - expectNs) / 1e6;
            if (++rr.nSets > 5 && i + 40 < qf.size()) { rr.phaseMin = juce::jmin (rr.phaseMin, ph); rr.phaseMax = juce::jmax (rr.phaseMax, ph); }
        }
    }
    rr.speed = e.speed.load(); rr.running = e.running.load(); rr.state = e.state.load();
    return rr;
}

static void testEngine (int rid, tc::Label start, double speed = 1.0)
{
    const auto& r = tc::rate (rid);
    auto rr = runEngine (rid, start, 4.0, speed);
    const double expIv = 1000.0 / (r.real * speed) / 4.0;
    std::printf ("[LTC->MTC %-8s x%.2f] QF %d, FF %d, intervalo %.4f ms (esperado %.4f) sd %.4f, fase [%.3f, %.3f] ms, velocidad %.4f\n",
                 r.label, speed, rr.nQF, rr.nFF, rr.meanIv, expIv, rr.sdIv, rr.phaseMin, rr.phaseMax, rr.speed);
    CHECK (rr.nFF == 1, "un solo Full Frame (sin relocates)");
    CHECK (std::abs (rr.meanIv - expIv) < 0.01, "intervalo QF");
    CHECK (rr.sdIv < 0.05, "jitter QF");
    CHECK (rr.phaseMin > -0.5 && rr.phaseMax < 0.5, "fase");
    CHECK (rr.nSets > 20, "sets completos de 8 QF");
}

static void testDropouts()
{
    auto a = runEngine (tc::R25, { 1, 0, 0, 0 }, 4.0, 1.0, 2000, 200);   // corte de 200 ms, freewheel 10 frames = 400 ms
    std::printf ("[corte 200 ms] FF %d, sigue %d\n", a.nFF, (int) a.running);
    CHECK (a.nFF == 1 && a.running, "freewheel tapa el corte corto");
    auto b = runEngine (tc::R25, { 1, 0, 0, 0 }, 4.0, 1.0, 2500, 1500);  // corte hasta el final
    std::printf ("[corte largo] parado %d, estado %d\n", (int) ! b.running, b.state);
    CHECK (! b.running && b.state == (int) State::SinSenal, "para tras el freewheel");
}

static void testGenerator()
{
    const double sr = 48000; const int B = 512;
    Capture cap; Engine e (cap);
    e.prepare (sr, B, 0);
    e.genRate = tc::R2997DF;
    e.genStart = tc::labelToAbs ({ 0, 9, 59, 0 }, tc::rate (tc::R2997DF));
    e.ltcOut = true; e.outChannel = 0;
    e.setMode (1);
    std::vector<float> outAll;
    for (int k = 0; k < 400; ++k)
    {
        if (k == 1) e.play();
        float o[B]; float* out[1] = { o };
        e.process (nullptr, 0, out, 1, B, 1000000000ull + (uint64_t) (k * B / sr * 1e9));
        outAll.insert (outAll.end(), o, o + B);
    }
    tc::LtcDecoder d; d.prepare (sr);
    std::vector<tc::LtcFrame> fr;
    d.process (outAll.data(), (int) outAll.size(), 0, [&] (const tc::LtcFrame& f) { fr.push_back (f); });
    int qf = 0; for (auto& m : cap.msgs) qf += m.d[0] == 0xF1;
    std::printf ("[generador 29.97 DF] QF %d, LTC por audio: %d frames, %s → %s\n", qf, (int) fr.size(),
                 fr.empty() ? "-" : tc::format (fr.front().label, true).c_str(), fr.empty() ? "-" : tc::format (fr.back().label, true).c_str());
    CHECK (fr.size() > 100, "LTC del generador decodificable");
    CHECK (! fr.empty() && fr.front().label.m == 9 && fr.back().label.m == 10, "cruza el minuto 10 en DF");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testRoundTrip (tc::R2398, { 0, 0, 0, 0 });
    testRoundTrip (tc::R24, { 23, 59, 58, 0 });
    testRoundTrip (tc::R25, { 1, 0, 0, 0 });
    testRoundTrip (tc::R2997DF, { 0, 9, 59, 0 });
    testRoundTrip (tc::R2997ND, { 10, 0, 0, 0 });
    testRoundTrip (tc::R30, { 0, 59, 58, 0 });
    testEngine (tc::R2398, { 0, 0, 0, 0 });
    testEngine (tc::R24, { 23, 59, 58, 0 });
    testEngine (tc::R25, { 1, 0, 0, 0 });
    testEngine (tc::R2997DF, { 0, 9, 59, 0 });
    testEngine (tc::R2997ND, { 10, 0, 0, 0 });
    testEngine (tc::R30, { 2, 0, 0, 0 });
    testEngine (tc::R25, { 1, 0, 0, 0 }, 1.03);   // varispeed
    testDropouts();
    testGenerator();
    std::printf (failures ? "\n%d FALLOS\n" : "\nTODO OK\n", failures);
    return failures ? 1 : 0;
}
