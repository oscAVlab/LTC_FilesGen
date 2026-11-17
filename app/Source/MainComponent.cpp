// osc[AV]lab LTC — interfaz
#include "MainComponent.h"
#include "BinaryData.h"

namespace ltcapp
{
using namespace colours;
static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
static juce::Font mono (float size) { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain)); }
static juce::Font sans (float size, bool bold = false) { return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)); }

static void heading (juce::Graphics& g, juce::Rectangle<int> col, const juce::String& t)
{
    g.setColour (accent);
    g.setFont (mono (12.0f));
    g.drawText (t.toUpperCase(), col.removeFromTop (22), juce::Justification::centredLeft);
    g.setColour (line);
    g.fillRect (col.getX(), col.getY() + 2, col.getWidth(), 1);
}
static void caption (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t)
{
    g.setColour (dim);
    g.setFont (mono (10.5f));
    g.drawText (t.toUpperCase(), r, juce::Justification::bottomLeft);
}
static void styleEditor (juce::TextEditor& e, const juce::String& v)
{
    e.setFont (mono (18.0f));
    e.setInputRestrictions (4, "0123456789");
    e.setJustification (juce::Justification::centred);
    e.setText (v, false);
    e.applyFontToAllText (mono (18.0f));
    e.setColour (juce::TextEditor::backgroundColourId, bg);
    e.setColour (juce::TextEditor::outlineColourId, line);
    e.setColour (juce::TextEditor::focusedOutlineColourId, accent);
    e.setColour (juce::TextEditor::textColourId, text);
}
static int intOf (const juce::TextEditor& e) { return e.getText().getIntValue(); }

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (bg); g.fillRect (r);
    g.setColour (line); g.drawRect (r, 1.0f);
    const float db = level > 0 ? 20.0f * std::log10 (level) : -99.0f;
    const float w = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    g.setColour (accent); g.fillRect (r.reduced (2).withWidth ((r.getWidth() - 4) * w));
}

// =====================================================================  LTC → MTC
MtcPage::MtcPage (Engine& e, MidiOut& m, juce::AudioDeviceManager& d, juce::PropertiesFile& p)
    : engine (e), midi (m), dm (d), props (p)
{
    for (auto* b : { &modeLtc, &modeGen }) { b->setClickingTogglesState (false); addAndMakeVisible (b); }
    modeLtc.onClick = [this] { setMode (0); };
    modeGen.onClick = [this] { setMode (1); };

    addAndMakeVisible (audioBtn);
    audioBtn.onClick = [this]
    {
        auto* sel = new juce::AudioDeviceSelectorComponent (dm, 1, 8, 0, 8, false, false, false, false);
        sel->setSize (520, 420);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (sel);
        o.dialogTitle = "Interfaz de audio";
        o.dialogBackgroundColour = panel;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    };
    dm.addChangeListener (this);

    for (auto* c : { &inCh, &outCh, &gRate, &dest, &mtcRate, &fw }) addAndMakeVisible (c);
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);

    inCh.onChange = [this] { engine.inChannel = juce::jmax (0, inCh.getSelectedItemIndex()); save(); };
    outCh.onChange = [this] { engine.outChannel = juce::jmax (0, outCh.getSelectedItemIndex()); save(); };

    for (int i = 0; i < tc::NumRates; ++i) gRate.addItem (tc::rate (i).label, i + 1);
    gRate.setSelectedId (props.getIntValue ("genRate", tc::R25) + 1, juce::dontSendNotification);
    if (gRate.getSelectedId() == 0) gRate.setSelectedId (tc::R25 + 1, juce::dontSendNotification);
    gRate.onChange = [this] { applyGenStart(); };

    styleEditor (gH, props.getValue ("gH", "0")); styleEditor (gM, props.getValue ("gM", "0"));
    styleEditor (gS, props.getValue ("gS", "0")); styleEditor (gF, props.getValue ("gF", "0"));
    for (auto* t : { &gH, &gM, &gS, &gF }) { addAndMakeVisible (t); t->onTextChange = [this] { applyGenStart(); }; }

    for (auto* b : { &play, &pause, &stop, &ff, &refreshBtn }) addAndMakeVisible (b);
    play.onClick = [this] { engine.play(); };
    pause.onClick = [this] { engine.pause(); };
    stop.onClick = [this] { engine.stop(); };
    ff.onClick = [this] { engine.sendFullFrameNow(); };
    refreshBtn.onClick = [this] { refreshDestinations(); };

    addAndMakeVisible (ltcOut);
    ltcOut.setColour (juce::ToggleButton::tickColourId, accent);
    ltcOut.setToggleState (props.getBoolValue ("ltcOut", false), juce::dontSendNotification);
    engine.ltcOut = ltcOut.getToggleState();
    ltcOut.onClick = [this] { engine.ltcOut = ltcOut.getToggleState(); save(); };

    dest.onChange = [this] { midi.setDestination (dest.getText().upToFirstOccurrenceOf (" (virtual)", false, false)); save(); };

    mtcRate.addItem ("Auto (como el LTC)", 1);
    mtcRate.addItem ("24", 2); mtcRate.addItem ("25", 3); mtcRate.addItem ("29.97 Drop", 4); mtcRate.addItem ("30 (y 29.97 ND)", 5);
    mtcRate.setSelectedId (props.getIntValue ("mtcRate", 1), juce::dontSendNotification);
    if (mtcRate.getSelectedId() == 0) mtcRate.setSelectedId (1, juce::dontSendNotification);
    mtcRate.onChange = [this]
    {
        static const int map[] = { -1, tc::R24, tc::R25, tc::R2997DF, tc::R30 };
        engine.mtcRate = map[juce::jlimit (0, 4, mtcRate.getSelectedId() - 1)];
        save();
    };
    mtcRate.onChange();

    fw.addItem ("4 frames", 4); fw.addItem ("10 frames", 10); fw.addItem ("1 segundo", 25); fw.addItem ("3 segundos", 75);
    fw.setSelectedId (props.getIntValue ("fw", 10), juce::dontSendNotification);
    if (fw.getSelectedId() == 0) fw.setSelectedId (10, juce::dontSendNotification);
    fw.onChange = [this] { engine.freewheelFrames = fw.getSelectedId(); save(); };
    fw.onChange();

    addAndMakeVisible (offset);
    offset.setSliderStyle (juce::Slider::IncDecButtons);
    offset.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 80, 30);
    offset.setRange (-200.0, 200.0, 0.5);
    offset.setTextValueSuffix (" ms");
    offset.setValue (props.getDoubleValue ("offset", 0.0), juce::dontSendNotification);
    offset.onValueChange = [this] { engine.userOffsetMs = (float) offset.getValue(); save(); };
    engine.userOffsetMs = (float) offset.getValue();

    stats.setFont (mono (11.0f)); stats.setColour (juce::Label::textColourId, dim);
    addAndMakeVisible (stats);

    guide.setFont (sans (14.0f));
    guide.setColour (juce::Label::textColourId, paperSoft);
    guide.setJustificationType (juce::Justification::topLeft);
    guide.setText (U ("1. Salida: «osc[AV]lab MTC (virtual)» si Live está en este Mac, o tu interfaz MIDI.\n\n"
                      "2. En Live: Settings › Link, Tempo & MIDI, elige ese puerto y pon MIDI Sync Type = MIDI Timecode.\n\n"
                      "3. MIDI Timecode Rate: el que muestra arriba (o SMPTE All).\n\n"
                      "4. MIDI Timecode Offset: la hora del LTC que va en el compás 1 (ej. 01:00:00:00).\n\n"
                      "5. Activa EXT en la barra de control. Live sigue la posición; el tempo lo pones tú."), juce::dontSendNotification);
    addAndMakeVisible (guide);

    refreshChannels();
    refreshDestinations();
    ready = true;
    setMode (props.getIntValue ("mode", 0));
}

MtcPage::~MtcPage() { dm.removeChangeListener (this); }

void MtcPage::save()
{
    if (! ready) return;
    props.setValue ("mode", engine.mode.load());
    props.setValue ("inCh", inCh.getSelectedItemIndex());
    props.setValue ("outCh", outCh.getSelectedItemIndex());
    props.setValue ("ltcOut", ltcOut.getToggleState());
    props.setValue ("dest", dest.getText());
    props.setValue ("mtcRate", mtcRate.getSelectedId());
    props.setValue ("fw", fw.getSelectedId());
    props.setValue ("offset", offset.getValue());
    props.setValue ("genRate", gRate.getSelectedId() - 1);
    props.setValue ("gH", gH.getText()); props.setValue ("gM", gM.getText());
    props.setValue ("gS", gS.getText()); props.setValue ("gF", gF.getText());
}

void MtcPage::setMode (int m)
{
    engine.setMode (m);
    modeLtc.setToggleState (m == 0, juce::dontSendNotification);
    modeGen.setToggleState (m == 1, juce::dontSendNotification);
    for (juce::Component* c : { (juce::Component*) &inCh, (juce::Component*) &inMeter, (juce::Component*) &audioBtn })
        c->setVisible (true);
    for (juce::Component* c : { (juce::Component*) &gH, (juce::Component*) &gM, (juce::Component*) &gS, (juce::Component*) &gF,
                                (juce::Component*) &gRate, (juce::Component*) &play, (juce::Component*) &pause, (juce::Component*) &stop,
                                (juce::Component*) &ltcOut, (juce::Component*) &outCh, (juce::Component*) &outMeter })
        c->setVisible (m == 1);
    inCh.setVisible (m == 0); inMeter.setVisible (m == 0);
    applyGenStart();
    save();
    resized(); repaint();
}

void MtcPage::applyGenStart()
{
    const int rid = juce::jmax (0, gRate.getSelectedId() - 1);
    const auto& r = tc::rate (rid);
    tc::Label l { juce::jlimit (0, 23, intOf (gH)), juce::jlimit (0, 59, intOf (gM)), juce::jlimit (0, 59, intOf (gS)), juce::jlimit (0, r.n - 1, intOf (gF)) };
    engine.genRate = rid;
    engine.genStart = tc::labelToAbs (l, r);
    if (engine.mode.load() == 1 && ! engine.running.load()) engine.stop();
    save();
}

void MtcPage::refreshChannels()
{
    auto* dev = dm.getCurrentAudioDevice();
    auto fill = [] (juce::ComboBox& c, const juce::StringArray& names, const juce::BigInteger& active, int sel)
    {
        c.clear (juce::dontSendNotification);
        int id = 1;
        for (int i = 0; i < names.size(); ++i)
            if (active[i]) c.addItem (juce::String (i + 1) + "  " + names[i], id++);
        if (c.getNumItems() == 0) c.addItem ("(sin canales)", 1);
        c.setSelectedItemIndex (juce::jlimit (0, c.getNumItems() - 1, sel), juce::dontSendNotification);
    };
    if (dev != nullptr)
    {
        fill (inCh, dev->getInputChannelNames(), dev->getActiveInputChannels(), props.getIntValue ("inCh", 0));
        fill (outCh, dev->getOutputChannelNames(), dev->getActiveOutputChannels(), props.getIntValue ("outCh", 0));
    }
    else { fill (inCh, {}, {}, 0); fill (outCh, {}, {}, 0); }
    engine.inChannel = juce::jmax (0, inCh.getSelectedItemIndex());
    engine.outChannel = juce::jmax (0, outCh.getSelectedItemIndex());
    if (auto* xml = dm.createStateXml().release()) { props.setValue ("audio", xml); delete xml; }
}

void MtcPage::refreshDestinations()
{
    const auto saved = props.getValue ("dest");
    dest.clear (juce::dontSendNotification);
    auto names = midi.getDestinations();
    for (int i = 0; i < names.size(); ++i) dest.addItem (names[i], i + 1);
    int idx = names.indexOf (saved);
    dest.setSelectedItemIndex (juce::jmax (0, idx), juce::dontSendNotification);
    midi.setDestination (dest.getText().upToFirstOccurrenceOf (" (virtual)", false, false));
}

void MtcPage::refresh()
{
    const int rid = engine.displayRate.load();
    const auto& r = tc::rate (rid);
    const int m = engine.mode.load();
    const bool any = engine.running.load() || m == 1 || engine.haveLtc.load();
    tcText = any ? juce::String (tc::format (tc::absToLabel ((int64_t) std::floor (engine.displayPos.load()), r), r.df)) : "00:00:00:00";

    static const char* names[] = { "PARADO", "BUSCANDO LTC", "LOCK", "FREEWHEEL", "", "PLAY", "PAUSA", "RELOCATE" };
    const auto st = (State) engine.state.load();
    stateText = st == State::SinSenal ? U ("SIN SEÑAL") : juce::String (names[(int) st]);
    stateOk = st == State::Lock || st == State::Play || st == State::Relocate;

    if (engine.running.load())
    {
        const int sel = engine.mtcRate.load();
        static const char* codes[] = { "24", "25", "29.97 DF", "30" };
        rateText = juce::String (r.label) + U (" fps  →  MTC ") + codes[tc::rate (sel >= 0 ? sel : rid).mtcCode];
        infoText = m == 0 ? (U ("jitter LTC ") + juce::String (engine.jitterMs.load(), 2) + U (" ms   ·   velocidad ")
                             + juce::String (engine.speed.load() * 100.0, 2) + "%   ·   latencia compensada "
                             + juce::String (engine.compMs.load(), 1) + " ms")
                          : juce::String();
    }
    else { rateText = m == 1 ? juce::String (r.label) + " fps" : juce::String(); infoText = {}; }

    stats.setText ("QF enviados: " + juce::String (engine.qfCount.load()) + U ("   ·   Full Frames: ") + juce::String (engine.ffCount.load()),
                   juce::dontSendNotification);
    inMeter.setLevel (engine.inLevel.load());
    outMeter.setLevel (engine.outLevel.load());
    repaint (tcArea);
}

void MtcPage::paint (juce::Graphics& g)
{
    // display de timecode
    auto r = tcArea;
    g.setColour (text);
    g.setFont (mono ((float) juce::jmin (84, r.getHeight() - 20)));
    g.drawText (tcText, r.removeFromLeft (r.getWidth() * 2 / 3), juce::Justification::centredLeft);
    auto s = r.reduced (0, 10);
    auto pill = s.removeFromTop (34).removeFromRight (190);
    g.setColour (stateOk ? accent : line);
    g.drawRect (pill, 1);
    g.setColour (stateOk ? accent : dim);
    g.setFont (mono (13.0f));
    g.drawText (stateText, pill, juce::Justification::centred);
    g.setColour (dim);
    g.setFont (mono (12.0f));
    g.drawText (rateText, s.removeFromTop (28), juce::Justification::centredRight);
    g.drawFittedText (infoText, s.removeFromTop (34), juce::Justification::centredRight, 2);
    g.setColour (line);
    g.fillRect (tcArea.getX(), tcArea.getBottom(), tcArea.getWidth(), 1);

    heading (g, colSrc, "Fuente");
    heading (g, colOut, "Salida MTC");
    heading (g, colGuide, "Ableton Live");

    // rótulos
    auto cap = [&] (juce::Component& c, const juce::String& t) { if (c.isVisible()) caption (g, c.getBounds().translated (0, -22).withHeight (20).withWidth (juce::jmax (c.getWidth(), 340)), t); };
    cap (audioBtn, "Entrada");
    cap (inCh, "Canal LTC");
    cap (gH, "Inicio  (hh  mm  ss  ff)");
    cap (gRate, "Frame rate");
    cap (outCh, "Canal de salida LTC");
    cap (dest, "Puerto MIDI");
    cap (mtcRate, "Rate MTC");
    cap (fw, "Freewheel");
    cap (offset, U ("COMPENSACIÓN EXTRA"));
}

void MtcPage::resized()
{
    auto r = getLocalBounds();
    tcArea = r.removeFromTop (120);
    r.removeFromTop (22);
    const int gap = 36;
    const int w = (r.getWidth() - 2 * gap) / 3;
    colSrc = r.removeFromLeft (w); r.removeFromLeft (gap);
    colOut = r.removeFromLeft (w); r.removeFromLeft (gap);
    colGuide = r;

    const int H = 34;
    auto c = colSrc.withTrimmedTop (36);
    auto seg = c.removeFromTop (H);
    modeLtc.setBounds (seg.removeFromLeft (seg.getWidth() / 2));
    modeGen.setBounds (seg);
    c.removeFromTop (26);
    audioBtn.setBounds (c.removeFromTop (H));
    c.removeFromTop (26);
    if (engine.mode.load() == 0)
    {
        auto row = c.removeFromTop (H);
        inCh.setBounds (row.removeFromLeft (row.getWidth() / 2 - 6));
        row.removeFromLeft (12);
        inMeter.setBounds (row.withSizeKeepingCentre (row.getWidth(), 12));
    }
    else
    {
        auto row = c.removeFromTop (44);
        const int fw4 = (row.getWidth() - 3 * 8) / 4;
        for (auto* t : { &gH, &gM, &gS, &gF }) { t->setBounds (row.removeFromLeft (fw4)); row.removeFromLeft (8); }
        c.removeFromTop (26);
        gRate.setBounds (c.removeFromTop (H));
        c.removeFromTop (8);
        auto b = c.removeFromTop (H);
        const int bw = (b.getWidth() - 16) / 3;
        play.setBounds (b.removeFromLeft (bw)); b.removeFromLeft (8);
        pause.setBounds (b.removeFromLeft (bw)); b.removeFromLeft (8);
        stop.setBounds (b);
        c.removeFromTop (8);
        ltcOut.setBounds (c.removeFromTop (28));
        c.removeFromTop (24);
        auto row2 = c.removeFromTop (H);
        outCh.setBounds (row2.removeFromLeft (row2.getWidth() / 2 - 6));
        row2.removeFromLeft (12);
        outMeter.setBounds (row2.withSizeKeepingCentre (row2.getWidth(), 12));
    }

    auto o = colOut.withTrimmedTop (36 + 30);
    auto drow = o.removeFromTop (H);
    refreshBtn.setBounds (drow.removeFromRight (110)); drow.removeFromRight (8);
    dest.setBounds (drow);
    o.removeFromTop (30);
    auto rr = o.removeFromTop (H);
    mtcRate.setBounds (rr.removeFromLeft (rr.getWidth() / 2 - 6)); rr.removeFromLeft (12);
    fw.setBounds (rr);
    o.removeFromTop (30);
    offset.setBounds (o.removeFromTop (H).removeFromLeft (200));
    o.removeFromTop (16);
    ff.setBounds (o.removeFromTop (H));
    o.removeFromTop (12);
    stats.setBounds (o.removeFromTop (24));

    guide.setBounds (colGuide.withTrimmedTop (34));
}

// =====================================================================  Generar archivos
FilesPage::FilesPage (juce::PropertiesFile& p) : juce::Thread ("LTC files"), props (p)
{
    styleEditor (offH, "0"); styleEditor (offM, "0"); styleEditor (offS, "0"); styleEditor (offF, "0");
    styleEditor (durH, "0"); styleEditor (durM, "5"); styleEditor (durS, "0");
    styleEditor (numFiles, "1"); styleEditor (intH, "1"); styleEditor (intM, "0");
    for (auto* e : { &offH, &offM, &offS, &offF, &durH, &durM, &durS, &numFiles, &intH, &intM }) addAndMakeVisible (e);
    fps.addItemList ({ "23.976", "24", "25", "29.97", "30" }, 1); fps.setSelectedId (4);
    sync.addItemList ({ "Non-Drop (NDF)", "Drop Frame (DF)" }, 1); sync.setSelectedId (1);
    bits.addItemList ({ "16-bit", "24-bit" }, 1); bits.setSelectedId (2);
    srate.addItemList ({ "44.1 kHz", "48 kHz", "96 kHz" }, 1); srate.setSelectedId (2);
    fps.onChange = [this] { sync.setEnabled (fps.getSelectedId() == 4); if (! sync.isEnabled()) sync.setSelectedId (1); };
    for (auto* c : { &fps, &sync, &bits, &srate }) addAndMakeVisible (c);
    addAndMakeVisible (gen);
    gen.setColour (juce::TextButton::buttonColourId, accent);
    gen.setColour (juce::TextButton::textColourOffId, bg);
    gen.onClick = [this] { start(); };
    status.setFont (mono (12.0f)); status.setColour (juce::Label::textColourId, dim);
    addAndMakeVisible (status);
}

FilesPage::~FilesPage() { stopThread (3000); }

void FilesPage::start()
{
    if (isThreadRunning()) return;
    chooser = std::make_unique<juce::FileChooser> ("Carpeta de destino",
        juce::File (props.getValue ("filesDir", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getFullPathName())));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult() == juce::File()) return;
        outDir = fc.getResult();
        props.setValue ("filesDir", outDir.getFullPathName());
        gen.setEnabled (false);
        startThread();
    });
}

static juce::String cleanName (int h, int m, int s, int f = 0)
{
    juce::StringArray p;
    if (h > 0) p.add (juce::String (h) + "h");
    if (m > 0) p.add (juce::String (m) + "m");
    if (s > 0) p.add (juce::String (s) + "s");
    if (f > 0) p.add (juce::String (f) + "f");
    return p.isEmpty() ? juce::String ("0s") : p.joinIntoString (" ");
}

void FilesPage::run()
{
    const juce::String fpsStr = fps.getText();
    const bool df = sync.getSelectedId() == 2 && fpsStr == "29.97";
    const int rid = fpsStr == "23.976" ? tc::R2398 : fpsStr == "24" ? tc::R24 : fpsStr == "25" ? tc::R25 : fpsStr == "30" ? tc::R30 : (df ? tc::R2997DF : tc::R2997ND);
    const auto& r = tc::rate (rid);
    const double sr = srate.getSelectedId() == 1 ? 44100.0 : srate.getSelectedId() == 3 ? 96000.0 : 48000.0;
    const int bd = bits.getSelectedId() == 1 ? 16 : 24;
    const int n = juce::jmax (1, intOf (numFiles));
    const int oH = intOf (offH), oM = intOf (offM), oS = intOf (offS), oF = intOf (offF);
    const int dH = intOf (durH), dM = intOf (durM), dS = intOf (durS);
    const int iH = intOf (intH), iM = intOf (intM);
    const double secs = dH * 3600.0 + dM * 60.0 + dS;
    const juce::String syncStr = df ? "DF" : "NDF";
    const juce::String fpsSuffix = " _ " + fpsStr + " " + syncStr + "fps";
    const juce::String dur = cleanName (dH, dM, dS);

    juce::File dir = outDir;
    if (n > 1) { dir = outDir.getChildFile ("LTC_Bundle(" + juce::String (n) + "files at " + fpsStr + syncStr + ") | oscLAB"); dir.createDirectory(); }

    juce::WavAudioFormat wav;
    juce::String err;
    for (int i = 0; i < n && ! threadShouldExit(); ++i)
    {
        int h = oH + (n > 1 ? i * iH : 0), m = oM + (n > 1 ? i * iM : 0);
        while (m >= 60) { ++h; m -= 60; }
        h %= 24;
        const juce::String name = (n > 1 ? juce::String (i + 1) + "_" : juce::String()) + "LTC_Off " + cleanName (h, m, oS, oF)
                                  + " _ Dur " + dur + fpsSuffix + " | oscLAB.wav";
        juce::MessageManager::callAsync ([this, i, n] { status.setText ("Generando " + juce::String (i + 1) + "/" + juce::String (n) + "...", juce::dontSendNotification); });
        auto file = dir.getChildFile (name);
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> os (file.createOutputStream());
        if (os == nullptr) { err = "No se pudo escribir " + file.getFullPathName(); break; }
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 1, bd, {}, 0));
        if (w == nullptr) { err = "Error de formato"; break; }
        os.release();
        tc::LtcEncoder enc;
        const double start = (double) tc::labelToAbs ({ h, m, oS, juce::jmin (oF, r.n - 1) }, r);
        const double step = r.real / sr;
        const int64_t total = (int64_t) (secs * sr);
        juce::AudioBuffer<float> buf (1, 8192);
        for (int64_t pos = 0; pos < total && ! threadShouldExit(); pos += 8192)
        {
            const int len = (int) juce::jmin ((int64_t) 8192, total - pos);
            enc.render (buf.getWritePointer (0), len, start + step * (double) pos, step, r, 0.5f);
            w->writeFromAudioSampleBuffer (buf, 0, len);
        }
    }
    juce::MessageManager::callAsync ([this, err, dir]
    {
        gen.setEnabled (true);
        status.setText (err.isNotEmpty() ? err : "Listo: " + dir.getFullPathName(), juce::dontSendNotification);
        if (err.isEmpty()) dir.revealToUser();
    });
}

void FilesPage::paint (juce::Graphics& g)
{
    heading (g, c1, "Tiempo");
    heading (g, c2, "Formato");
    heading (g, c3, "Multi-Export");
    auto cap = [&] (juce::Component& c, const juce::String& t) { caption (g, c.getBounds().translated (0, -22).withHeight (20).withWidth (juce::jmax (c.getWidth(), 340)), t); };
    cap (offH, "Offset inicial  (hh  mm  ss  ff)");
    cap (durH, U ("DURACIÓN POR ARCHIVO  (HH  MM  SS)"));
    cap (fps, "FPS"); cap (sync, "Modo sync"); cap (bits, "Bit depth"); cap (srate, "Sample rate");
    cap (numFiles, U ("Nº ARCHIVOS")); cap (intH, "Intervalo  (hh  mm)");
}

void FilesPage::resized()
{
    auto r = getLocalBounds().withTrimmedTop (20);
    auto bottom = r.removeFromBottom (60);
    const int gap = 36, w = (r.getWidth() - 2 * gap) / 3;
    c1 = r.removeFromLeft (w); r.removeFromLeft (gap);
    c2 = r.removeFromLeft (w); r.removeFromLeft (gap);
    c3 = r;
    auto row4 = [] (juce::Rectangle<int> row, std::initializer_list<juce::Component*> cs)
    {
        const int n = (int) cs.size(), cw = (row.getWidth() - (n - 1) * 8) / n;
        for (auto* c : cs) { c->setBounds (row.removeFromLeft (cw)); row.removeFromLeft (8); }
    };
    auto a = c1.withTrimmedTop (36 + 30);
    row4 (a.removeFromTop (44), { &offH, &offM, &offS, &offF });
    a.removeFromTop (34);
    row4 (a.removeFromTop (44), { &durH, &durM, &durS });
    auto b = c2.withTrimmedTop (36 + 30);
    row4 (b.removeFromTop (34), { &fps, &sync });
    b.removeFromTop (34);
    row4 (b.removeFromTop (34), { &bits, &srate });
    auto c = c3.withTrimmedTop (36 + 30);
    auto rr = c.removeFromTop (44);
    numFiles.setBounds (rr.removeFromLeft (rr.getWidth() / 2 - 6)); rr.removeFromLeft (12);
    row4 (rr, {});
    intH.setBounds (rr.removeFromLeft (rr.getWidth() / 2 - 4)); rr.removeFromLeft (8);
    intM.setBounds (rr);
    gen.setBounds (bottom.removeFromRight (320).withSizeKeepingCentre (320, 50));
    status.setBounds (bottom);
}

// =====================================================================  Ventana
juce::PropertiesFile& MainComponent::initProps()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "osc[AV]lab LTC";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.folderName = "osc[AV]lab LTC";
    appProps.setStorageParameters (o);
    auto* p = appProps.getUserSettings();
    std::unique_ptr<juce::XmlElement> xml (p->getXmlValue ("audio"));
    dm.initialise (8, 8, xml.get(), true);
    return *p;
}

MainComponent::MainComponent() : mtc (engine, midiOut, dm, initProps()), files (*appProps.getUserSettings())
{
    juce::LookAndFeel::setDefaultLookAndFeel (&look);
    logo = juce::ImageCache::getFromMemory (BinaryData::logooscavlab_png, BinaryData::logooscavlab_pngSize);
    for (auto* t : { &tabMtc, &tabFiles }) { addAndMakeVisible (t); t->setClickingTogglesState (false); }
    tabMtc.onClick = [this] { showPage (0); };
    tabFiles.onClick = [this] { showPage (1); };
    addChildComponent (mtc);
    addChildComponent (files);
    dm.addAudioCallback (&engine);
    mtc.refreshChannels();
    showPage (juce::SystemStats::getEnvironmentVariable ("LTC_PAGE", "0").getIntValue());
    setSize (1240, 760);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    dm.removeAudioCallback (&engine);
    if (auto xml = dm.createStateXml()) appProps.getUserSettings()->setValue ("audio", xml.get());
    appProps.saveIfNeeded();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void MainComponent::showPage (int p)
{
    mtc.setVisible (p == 0); files.setVisible (p == 1);
    tabMtc.setToggleState (p == 0, juce::dontSendNotification);
    tabFiles.setToggleState (p == 1, juce::dontSendNotification);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    auto card = getLocalBounds().reduced (24);
    g.setColour (panel);
    g.fillRect (card);
    g.setColour (accent);
    g.fillRect (card.removeFromTop (2));
    auto head = getLocalBounds().reduced (24).reduced (40, 0).withTrimmedTop (22).removeFromTop (60);
    if (logo.isValid())
    {
        const float h = 46.0f, w = h * (float) logo.getWidth() / (float) logo.getHeight();
        g.setOpacity (0.9f);
        g.drawImage (logo, juce::Rectangle<float> ((float) head.getX(), (float) head.getCentreY() - h / 2, w, h), juce::RectanglePlacement::stretchToFit);
        g.setOpacity (1.0f);
        head.removeFromLeft ((int) w + 24);
    }
    g.setColour (accent);
    g.setFont (mono (14.0f));
    g.drawText (U ("LTC → MTC      v") + OSCAVLAB_VERSION, head, juce::Justification::centredLeft);
    g.setColour (line);
    auto hl = getLocalBounds().reduced (24).reduced (40, 0);
    g.fillRect (hl.getX(), hl.getY() + 96, hl.getWidth(), 1);
    // firma
    g.setColour (dim.withAlpha (0.6f));
    g.setFont (sans (13.0f));
    g.drawText (U ("ž"), getLocalBounds().removeFromTop (24), juce::Justification::centred);
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (24).reduced (40, 0);
    auto head = r.withTrimmedTop (22).removeFromTop (60);
    auto tabs = head.removeFromRight (360).withSizeKeepingCentre (360, 34);
    tabMtc.setBounds (tabs.removeFromLeft (180));
    tabFiles.setBounds (tabs);
    auto body = r.withTrimmedTop (96 + 24).withTrimmedBottom (24);
    mtc.setBounds (body);
    files.setBounds (body);
}
} // namespace ltcapp
