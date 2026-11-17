// osc[AV]lab LTC — lector/generador LTC → MTC regenerado
#include "MainComponent.h"

class LtcApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override { window = std::make_unique<Window>(); }
    void shutdown() override                        { window.reset(); }
    void systemRequestedQuit() override             { quit(); }

private:
    struct Window : juce::DocumentWindow
    {
        Window() : DocumentWindow ("osc[AV]lab LTC", ltcapp::colours::bg, allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new ltcapp::MainComponent(), true);
            setResizable (true, false);
            setResizeLimits (1100, 700, 3000, 2000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
};

START_JUCE_APPLICATION (LtcApp)
