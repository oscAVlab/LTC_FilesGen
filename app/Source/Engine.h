// osc[AV]lab LTC — motor: LTC (entrada o generador) → reloj con PLL → MTC regenerado
#pragma once
#include <JuceHeader.h>
#include "Ltc.h"
#include "MidiOut.h"
#include <atomic>

namespace ltcapp
{
// destino de mensajes MIDI (MidiOut en la app, un capturador en las pruebas)
struct MidiSink
{
    virtual ~MidiSink() = default;
    virtual void push (const uint8_t* d, int len, uint64_t ns) = 0;
    virtual void newGeneration() = 0;
};

enum class State { Parado, Buscando, Lock, Freewheel, SinSenal, Play, Pausa, Relocate };

class Engine : public juce::AudioIODeviceCallback
{
public:
    explicit Engine (MidiSink& sink) : midi (sink) {}

    // ---- parámetros (desde la interfaz) ----
    std::atomic<int> mode { 0 };              // 0 = LTC por audio, 1 = generador
    std::atomic<int> inChannel { 0 };
    std::atomic<int> outChannel { 0 };
    std::atomic<bool> ltcOut { false };       // generador: sacar LTC por audio
    std::atomic<int> mtcRate { -1 };          // -1 auto; si no, tc::RateId
    std::atomic<int> freewheelFrames { 10 };
    std::atomic<float> userOffsetMs { 0 };    // + = el MTC sale antes
    std::atomic<int> genRate { tc::R25 };
    std::atomic<int64_t> genStart { 0 };

    void play()  { cmdPlay = true; }
    void pause() { cmdPause = true; }
    void stop()  { cmdStop = true; }
    void sendFullFrameNow() { cmdFF = true; }
    void setMode (int m) { mode = m; cmdReset = true; }

    // ---- estado (para la interfaz) ----
    std::atomic<int> state { (int) State::Parado };
    std::atomic<double> displayPos { 0 };
    std::atomic<int> displayRate { tc::R25 };
    std::atomic<double> speed { 1.0 };
    std::atomic<float> jitterMs { 0 }, inLevel { 0 }, outLevel { 0 };
    std::atomic<int64_t> qfCount { 0 }, ffCount { 0 };
    std::atomic<bool> running { false }, haveLtc { false };
    std::atomic<double> compMs { 0 };

    // ---- núcleo (también usado por las pruebas) ----
    void prepare (double sampleRate, int blockSize, int latencySamples);
    // hostNs = hora de host de la primera muestra de salida del bloque (0 = desconocida)
    void process (const float* const* in, int numIn, float* const* out, int numOut, int n, uint64_t hostNs);

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                           int n, const juce::AudioIODeviceCallbackContext& ctx) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}

private:
    void onLtcFrame (const tc::LtcFrame& fr);
    int detectRate() const;
    void hardLocate (double pos, double atSample, int rateId, State st);
    double posAt (double s) const { return pos0 + (s - s0) * rate; }
    int mtcCode() const;
    void sendFullFrame (int64_t absFrame, uint64_t ns);
    void stopClock (State st);

    MidiSink& midi;
    tc::LtcDecoder decoder;
    tc::LtcEncoder encoder;
    double sr = 48000;
    int64_t S = 0;                    // contador de muestras
    uint64_t blockNs = 0;
    double compSamples = 0;           // latencia entrada→salida (modo LTC)

    // reloj
    bool clockRunning = false;
    double pos0 = 0, s0 = 0, rate = 0, nominal = 0;
    int clockRate = tc::R25;
    int64_t nextQ = 0;

    // LTC
    int64_t lastAbs = INT64_MIN; int good = 0; double lastLtcSample = -1e18;
    int maxF = 0; bool sawWrap = false, dfFlag = false; tc::Label lastLabel; bool haveLabel = false;
    double periods[50] {}; int nPeriods = 0, periodIdx = 0;
    double errs[60] {}; int nErrs = 0, errIdx = 0;

    // generador
    double genPos = 0;

    std::atomic<bool> cmdPlay { false }, cmdPause { false }, cmdStop { false }, cmdFF { false }, cmdReset { false };
};
} // namespace ltcapp
