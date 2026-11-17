// osc[AV]lab LTC — salida MIDI con marcas de tiempo
#if defined(__APPLE__)
 #include <CoreMIDI/CoreMIDI.h>
 #include <mach/mach_time.h>
#endif
#include "MidiOut.h"

namespace ltcapp
{
static const char* kVirtualName = "osc[AV]lab MTC";

#if JUCE_MAC
static mach_timebase_info_data_t timebase()
{
    static mach_timebase_info_data_t tb = [] { mach_timebase_info_data_t t; mach_timebase_info (&t); return t; }();
    return tb;
}
uint64_t hostNowNs()
{
    const auto tb = timebase();
    return (uint64_t) ((__uint128_t) mach_absolute_time() * tb.numer / tb.denom);
}
static uint64_t nsToHost (uint64_t ns)
{
    const auto tb = timebase();
    return (uint64_t) ((__uint128_t) ns * tb.denom / tb.numer);
}

struct MidiOut::Impl
{
    MIDIClientRef client = 0;
    MIDIPortRef port = 0;
    MIDIEndpointRef source = 0;        // puerto virtual
    MIDIEndpointRef dest = 0;          // destino elegido (0 = virtual)

    Impl()
    {
        MIDIClientCreate (CFSTR ("osc[AV]lab LTC"), nullptr, nullptr, &client);
        MIDIOutputPortCreate (client, CFSTR ("MTC out"), &port);
        MIDISourceCreate (client, CFSTR ("osc[AV]lab MTC"), &source);
    }
    ~Impl()
    {
        if (source) MIDIEndpointDispose (source);
        if (port) MIDIPortDispose (port);
        if (client) MIDIClientDispose (client);
    }
    static juce::String nameOf (MIDIEndpointRef e)
    {
        CFStringRef s = nullptr;
        if (MIDIObjectGetStringProperty (e, kMIDIPropertyDisplayName, &s) == noErr && s != nullptr)
        {
            auto r = juce::String::fromCFString (s);
            CFRelease (s);
            return r;
        }
        return "MIDI";
    }
    void send (const uint8_t* d, int len, uint64_t ns)
    {
        uint8_t buffer[256];
        auto* pl = (MIDIPacketList*) buffer;
        auto* pkt = MIDIPacketListInit (pl);
        const MIDITimeStamp ts = ns ? nsToHost (ns) : 0;
        pkt = MIDIPacketListAdd (pl, sizeof (buffer), pkt, ts, (ByteCount) len, d);
        if (pkt == nullptr) return;
        if (dest == 0) MIDIReceived (source, pl);
        else MIDISend (port, dest, pl);
    }
    void flush() { if (dest != 0) MIDIFlushOutput (port); }
};
#else
uint64_t hostNowNs()
{
    return (uint64_t) ((double) juce::Time::getHighResolutionTicks() * 1.0e9 / (double) juce::Time::getHighResolutionTicksPerSecond());
}
#endif

MidiOut::MidiOut() : juce::Thread ("osc[AV]lab MTC")
{
   #if JUCE_MAC
    impl = std::make_unique<Impl>();
   #else
    virtualOut = juce::MidiOutput::createNewDevice (kVirtualName);
    current = virtualOut.get();
   #endif
    destName = kVirtualName;
    startThread (juce::Thread::Priority::highest);
}

MidiOut::~MidiOut()
{
    stopThread (1000);
}

juce::StringArray MidiOut::getDestinations()
{
    juce::StringArray s;
    s.add (juce::String (kVirtualName) + juce::String::fromUTF8 (" (virtual)"));
   #if JUCE_MAC
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i)
        s.add (Impl::nameOf (MIDIGetDestination (i)));
   #else
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (d.name != kVirtualName) s.add (d.name);
   #endif
    return s;
}

void MidiOut::setDestination (const juce::String& name)
{
    const juce::ScopedLock sl (lock);
    destName = name;
   #if JUCE_MAC
    impl->dest = 0;
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); ++i)
        if (Impl::nameOf (MIDIGetDestination (i)) == name) impl->dest = MIDIGetDestination (i);
    if (impl->dest == 0) destName = kVirtualName;
   #else
    hwOut.reset();
    current = virtualOut.get();
    for (auto& d : juce::MidiOutput::getAvailableDevices())
        if (d.name == name) { hwOut = juce::MidiOutput::openDevice (d.identifier); current = hwOut.get(); }
    if (hwOut == nullptr) destName = kVirtualName;
   #endif
}

void MidiOut::push (const uint8_t* data, int len, uint64_t ns)
{
    const auto w = fifo.write (1);
    if (w.blockSize1 + w.blockSize2 == 0) return;
    auto& m = buf[(size_t) (w.blockSize1 ? w.startIndex1 : w.startIndex2)];
    std::memcpy (m.d, data, (size_t) juce::jmin (len, 10));
    m.len = (uint8_t) juce::jmin (len, 10);
    m.ns = ns;
    m.gen = generation.load();
}

void MidiOut::sendNow (const Msg& m)
{
    const juce::ScopedLock sl (lock);
   #if JUCE_MAC
    impl->send (m.d, m.len, m.ns);
   #else
    if (current != nullptr) current->sendMessageNow (juce::MidiMessage (m.d, m.len));
   #endif
}

void MidiOut::run()
{
    while (! threadShouldExit())
    {
        if (fifo.getNumReady() == 0) { wait (1); continue; }
        int s1, b1, s2, b2;
        fifo.prepareToRead (1, s1, b1, s2, b2);
        const Msg m = buf[(size_t) (b1 ? s1 : s2)];
        const uint32_t g = generation.load();
        if (g != lastGen)
        {
            lastGen = g;
           #if JUCE_MAC
            const juce::ScopedLock sl (lock);
            impl->flush();
           #endif
        }
        if (m.gen != g) { fifo.finishedRead (1); continue; }   // de antes de un salto: se descarta

        // espera hasta ~0.5 ms antes de su hora; CoreMIDI programa el resto con la marca de tiempo
        bool stale = false;
        while (m.ns != 0 && ! threadShouldExit())
        {
            if (generation.load() != g) { stale = true; break; }
            const int64_t left = (int64_t) m.ns - (int64_t) hostNowNs();
           #if JUCE_MAC
            if (left <= 500000) break;
           #else
            if (left <= 0) break;
           #endif
            if (left > 2000000) juce::Thread::sleep (1);
            else juce::Thread::yield();
        }
        if (! stale) sendNow (m);
        fifo.finishedRead (1);
    }
}
} // namespace ltcapp
