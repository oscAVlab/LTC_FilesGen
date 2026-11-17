// osc[AV]lab LTC — salida MIDI con marcas de tiempo (CoreMIDI en Mac)
#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace ltcapp
{
// Reloj de host en nanosegundos (mismo dominio que AudioIODeviceCallbackContext::hostTimeNs)
uint64_t hostNowNs();

class MidiOut : private juce::Thread
{
public:
    MidiOut();
    ~MidiOut() override;

    // 0 = puerto virtual "osc[AV]lab MTC"; 1.. = destinos del sistema
    juce::StringArray getDestinations();
    void setDestination (const juce::String& name);
    juce::String getDestinationName() const { return destName; }

    // Llamable desde el hilo de audio (sin bloqueos). ns = 0 → enviar ya.
    void push (const uint8_t* data, int len, uint64_t ns);
    // Descarta lo que haya en cola de una generación anterior (tras un salto de posición)
    void newGeneration() { ++generation; }

private:
    struct Msg { uint8_t d[10]; uint8_t len; uint64_t ns; uint32_t gen; };
    void run() override;
    void sendNow (const Msg& m);

    juce::AbstractFifo fifo { 4096 };
    std::vector<Msg> buf = std::vector<Msg> (4096);
    std::atomic<uint32_t> generation { 0 };
    uint32_t lastGen = 0;
    juce::String destName;
    juce::CriticalSection lock;

   #if JUCE_MAC
    struct Impl;
    std::unique_ptr<Impl> impl;
   #else
    std::unique_ptr<juce::MidiOutput> virtualOut, hwOut;
    juce::MidiOutput* current = nullptr;
   #endif
};
} // namespace ltcapp
