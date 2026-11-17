// osc[AV]lab LTC — motor
#include "Engine.h"

namespace ltcapp
{
void Engine::prepare (double sampleRate, int blockSize, int latencySamples)
{
    sr = sampleRate;
    decoder.prepare (sr);
    encoder.reset();
    compSamples = (double) (latencySamples + blockSize);
    compMs = compSamples / sr * 1000.0;
    clockRunning = false; running = false;
    good = 0; lastAbs = INT64_MIN; maxF = 0; sawWrap = false; haveLabel = false; nPeriods = 0; nErrs = 0;
}

void Engine::audioDeviceAboutToStart (juce::AudioIODevice* d)
{
    const int inLat = d->getActiveInputChannels().countNumberOfSetBits() > 0 ? d->getInputLatencyInSamples() : 0;
    prepare (d->getCurrentSampleRate(), d->getCurrentBufferSizeSamples(), inLat + d->getOutputLatencyInSamples());
}

void Engine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                               int n, const juce::AudioIODeviceCallbackContext& ctx)
{
    process (in, numIn, out, numOut, n, ctx.hostTimeNs != nullptr ? *ctx.hostTimeNs : 0);
}

int Engine::mtcCode() const
{
    const int sel = mtcRate.load();
    return tc::rate (sel >= 0 ? sel : clockRate).mtcCode;
}

void Engine::sendFullFrame (int64_t absFrame, uint64_t ns)
{
    uint8_t m[10];
    tc::fullFrame (tc::absToLabel (absFrame, tc::rate (clockRate)), mtcCode(), m);
    midi.push (m, 10, ns);
    ++ffCount;
}

void Engine::hardLocate (double pos, double atSample, int rateId, State st)
{
    clockRate = rateId;
    nominal = tc::rate (rateId).real / sr;
    rate = nominal;
    if (mode.load() == 0 && nPeriods >= 8)      // arranca con la velocidad medida del LTC (varispeed)
    {
        double sum = 0; for (int i = 0; i < nPeriods; ++i) sum += periods[i];
        rate = juce::jlimit (nominal * 0.9, nominal * 1.1, (double) nPeriods / sum);
    }
    pos0 = pos; s0 = atSample;
    clockRunning = true; running = true;
    midi.newGeneration();
    // Full Frame con la posición que tendrá el MTC al salir ahora
    const double comp = mode.load() == 0 ? compSamples : 0.0;
    sendFullFrame ((int64_t) std::floor (posAt ((double) S + comp)), blockNs);
    int64_t q = (int64_t) std::ceil (posAt ((double) S + comp) * 4.0);
    while (((q % 8) + 8) % 8 != 0) ++q;     // pieza 0 siempre en frame par
    nextQ = q;
    state = (int) st;
}

void Engine::stopClock (State st)
{
    if (clockRunning && mode.load() == 1) genPos = posAt ((double) S);
    clockRunning = false; running = false;
    state = (int) st;
}

int Engine::detectRate() const
{
    if (nPeriods == 0) return -1;
    double sum = 0; for (int i = 0; i < nPeriods; ++i) sum += periods[i];
    const double fps = sr * nPeriods / sum;
    if (maxF >= 29 || (! sawWrap && fps > 27.5))
        return dfFlag ? tc::R2997DF : (fps < 29.985 ? tc::R2997ND : tc::R30);
    if (maxF == 24 || (! sawWrap && fps > 24.5)) return tc::R25;
    return fps < 23.988 ? tc::R2398 : tc::R24;
}

void Engine::onLtcFrame (const tc::LtcFrame& d)
{
    const double ps = d.periodSamples / sr;
    if (ps > 0.02 && ps < 0.06) { periods[periodIdx] = d.periodSamples; periodIdx = (periodIdx + 1) % 50; nPeriods = juce::jmin (nPeriods + 1, 50); }
    // estadísticas de rate sólo con frames que siguen al anterior (descarta basura tras un corte)
    if (haveLabel)
    {
        const int secPrev = lastLabel.h * 3600 + lastLabel.m * 60 + lastLabel.s;
        const int secCur  = d.label.h * 3600 + d.label.m * 60 + d.label.s;
        const bool sameSec = secCur == secPrev && d.label.f == lastLabel.f + 1;
        const bool nextSec = (secCur == (secPrev + 1) % 86400) && (d.label.f == 0 || (d.df && d.label.f == 2));
        if (sameSec) maxF = juce::jmax (maxF, d.label.f);
        if (nextSec) { sawWrap = true; maxF = juce::jmax (maxF, lastLabel.f); }
        if (sameSec || nextSec) dfFlag = d.df;
    }
    lastLabel = d.label; haveLabel = true; haveLtc = true;

    const int rid = detectRate();
    if (rid < 0) return;
    const auto& r = tc::rate (rid);
    const int64_t abs = tc::labelToAbs (d.label, r) + 1;           // posición al final del frame
    good = (lastAbs != INT64_MIN && tc::wrapDiff ((double) (abs - lastAbs), r) == 1.0) ? good + 1 : 0;
    lastAbs = abs;
    lastLtcSample = d.endSample;
    const double t = d.endSample;

    if (! clockRunning)
    {
        if (good >= 3 && sawWrap) hardLocate ((double) abs, t, rid, State::Lock);
        return;
    }
    const double err = tc::wrapDiff ((double) abs - posAt (t), r);
    if (rid != clockRate || std::abs (err) > 1.5)
    {
        if (good >= 2) hardLocate ((double) abs, t, rid, State::Relocate);
        return;
    }
    // PLL de 2º orden: fase suave + seguimiento de velocidad (varispeed)
    const double frameSamples = sr / r.real;
    pos0 = posAt (t) + 0.12 * err; s0 = t;
    rate += 0.01 * err / frameSamples;
    rate = juce::jlimit (nominal * 0.9, nominal * 1.1, rate);
    errs[errIdx] = err * frameSamples / sr * 1000.0; errIdx = (errIdx + 1) % 60; nErrs = juce::jmin (nErrs + 1, 60);
    state = (int) State::Lock;
}

void Engine::process (const float* const* in, int numIn, float* const* out, int numOut, int n, uint64_t hostNs)
{
    for (int c = 0; c < numOut; ++c) if (out[c] != nullptr) juce::FloatVectorOperations::clear (out[c], n);
    blockNs = hostNs != 0 ? hostNs : hostNowNs();
    const int m = mode.load();

    if (cmdReset.exchange (false))
    {
        clockRunning = false; running = false; good = 0; lastAbs = INT64_MIN; haveLtc = false;
        state = (int) (m == 0 ? State::Buscando : State::Parado);
        genPos = (double) genStart.load();
        encoder.reset();
    }

    // ---- comandos del generador ----
    if (m == 1)
    {
        if (cmdStop.exchange (false))
        {
            stopClock (State::Parado);
            genPos = (double) genStart.load();
            clockRate = genRate.load();
            midi.newGeneration();
            sendFullFrame ((int64_t) genPos, blockNs);
        }
        if (cmdPause.exchange (false) && clockRunning) stopClock (State::Pausa);
        if (cmdPlay.exchange (false) && ! clockRunning) hardLocate (genPos, (double) S, genRate.load(), State::Play);
        if (! clockRunning) genPos = juce::jmax (genPos, 0.0);
    }
    else { cmdPlay = cmdPause = cmdStop = false; }

    if (cmdFF.exchange (false))
    {
        const double comp = m == 0 ? compSamples : 0.0;
        const double p = clockRunning ? posAt ((double) S + comp) : (m == 1 ? genPos : (double) (lastAbs == INT64_MIN ? 0 : lastAbs));
        if (! clockRunning && m == 1) clockRate = genRate.load();
        sendFullFrame ((int64_t) std::floor (p), blockNs);
    }

    // ---- entrada LTC ----
    if (m == 0)
    {
        const int ch = inChannel.load();
        if (ch < numIn && in[ch] != nullptr)
        {
            decoder.process (in[ch], n, S, [this] (const tc::LtcFrame& f) { onLtcFrame (f); });
            const float l = decoder.takeLevel();
            inLevel = juce::jmax (l, inLevel.load() * 0.85f);
        }
        if (clockRunning)
        {
            const double fwSamples = freewheelFrames.load() * sr / tc::rate (clockRate).real;
            const double since = (double) (S + n) - lastLtcSample;
            if (since > fwSamples + 0.06 * sr) { stopClock (State::SinSenal); good = 0; }
            else if (since > 0.08 * sr) state = (int) State::Freewheel;
        }
        else
        {
            const double since = (double) S - lastLtcSample;
            state = (int) ((lastLtcSample > 0 && since > 0.5 * sr) ? State::SinSenal : State::Buscando);
        }
    }

    // ---- emisión de quarter frames con hora exacta ----
    if (clockRunning)
    {
        const double comp = m == 0 ? compSamples : 0.0;
        const double offS = userOffsetMs.load() * 0.001 * sr;
        const double blockEnd = (double) (S + n);
        const auto& r = tc::rate (clockRate);
        const int code = mtcCode();
        for (int guard = 0; guard < 64; ++guard)
        {
            const double t = s0 + ((double) nextQ / 4.0 - pos0) / rate;      // muestra (dominio del reloj)
            const double tOut = t - comp - offS;                              // muestra de salida equivalente
            if (tOut >= blockEnd) break;
            const double rel = juce::jmax (0.0, tOut - (double) S);
            const uint64_t ns = blockNs + (uint64_t) (rel / sr * 1.0e9);
            uint8_t msg[2];
            tc::quarterFrame (nextQ, r, code, msg);
            midi.push (msg, 2, ns);
            ++qfCount;
            ++nextQ;
        }
    }

    // ---- salida LTC del generador ----
    if (m == 1 && ltcOut.load())
    {
        const int oc = outChannel.load();
        if (oc < numOut && out[oc] != nullptr)
        {
            const int rid = clockRunning ? clockRate : genRate.load();
            if (clockRunning) encoder.render (out[oc], n, posAt ((double) S), rate, tc::rate (rid));
            float pk = 0; for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (out[oc][i]));
            outLevel = juce::jmax (pk, outLevel.load() * 0.85f);
        }
    }

    // ---- estado para la interfaz ----
    if (clockRunning) { displayPos = posAt ((double) S); displayRate = clockRate; speed = rate / nominal; }
    else if (m == 1) { displayPos = genPos; displayRate = genRate.load(); speed = 1.0; }
    else if (lastAbs != INT64_MIN) { displayPos = (double) (lastAbs - 1); }
    if (nErrs > 5)
    {
        double s2 = 0; for (int i = 0; i < nErrs; ++i) s2 += errs[i] * errs[i];
        jitterMs = (float) std::sqrt (s2 / nErrs);
    }
    S += n;
}
} // namespace ltcapp
