// osc[AV]lab LTC — ventana principal
#pragma once
#include <JuceHeader.h>
#include "Engine.h"
#include "Look.h"

namespace ltcapp
{
struct MidiOutSink : MidiSink
{
    explicit MidiOutSink (MidiOut& o) : out (o) {}
    void push (const uint8_t* d, int len, uint64_t ns) override { out.push (d, len, ns); }
    void newGeneration() override { out.newGeneration(); }
    MidiOut& out;
};

class Meter : public juce::Component
{
public:
    void setLevel (float l) { if (std::abs (l - level) > 0.001f) { level = l; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    float level = 0;
};

class MtcPage : public juce::Component, private juce::ChangeListener
{
public:
    MtcPage (Engine&, MidiOut&, juce::AudioDeviceManager&, juce::PropertiesFile&);
    ~MtcPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                      // llamado por el timer de la ventana
    void refreshChannels();
    void refreshDestinations();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshChannels(); }
    void setMode (int m);
    void applyGenStart();
    void save();

    Engine& engine; MidiOut& midi; juce::AudioDeviceManager& dm; juce::PropertiesFile& props;

    juce::TextButton modeLtc { "LTC POR AUDIO" }, modeGen { "GENERADOR" }, audioBtn { "INTERFAZ DE AUDIO..." };
    juce::ComboBox inCh, outCh, gRate, dest, mtcRate, fw;
    Meter inMeter, outMeter;
    juce::TextEditor gH, gM, gS, gF;
    juce::TextButton play { "PLAY" }, pause { "PAUSA" }, stop { "STOP" }, ff { "ENVIAR FULL FRAME" }, refreshBtn { "ACTUALIZAR" };
    juce::ToggleButton ltcOut { "Sacar LTC por audio" };
    juce::Slider offset;
    juce::Label stats, guide;

    juce::Rectangle<int> tcArea, colSrc, colOut, colGuide;
    juce::String tcText = "00:00:00:00", stateText = "PARADO", rateText, infoText;
    bool stateOk = false, ready = false;
};

class FilesPage : public juce::Component, private juce::Thread
{
public:
    explicit FilesPage (juce::PropertiesFile&);
    ~FilesPage() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void run() override;
    void start();
    juce::PropertiesFile& props;
    juce::TextEditor offH, offM, offS, offF, durH, durM, durS, numFiles, intH, intM;
    juce::ComboBox fps, sync, bits, srate;
    juce::TextButton gen { "GENERAR ARCHIVOS" };
    juce::Label status;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File outDir;
    juce::Rectangle<int> c1, c2, c3;
};

class MainComponent : public juce::Component, private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { mtc.refresh(); }
    void showPage (int p);
    juce::PropertiesFile& initProps();

    Look look;
    juce::ApplicationProperties appProps;
    juce::AudioDeviceManager dm;
    MidiOut midiOut;
    MidiOutSink sink { midiOut };
    Engine engine { sink };
    MtcPage mtc;
    FilesPage files;
    juce::TextButton tabMtc { juce::String::fromUTF8 ("LTC → MTC") }, tabFiles { "GENERAR ARCHIVOS" };
    juce::Image logo;
};
} // namespace ltcapp
