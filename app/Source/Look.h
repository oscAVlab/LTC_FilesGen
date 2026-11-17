// osc[AV]lab LTC — estética (manual de marca V_2026)
#pragma once
#include <JuceHeader.h>

namespace ltcapp::colours
{
// Paleta osc[AV]lab V_2026 (manual de marca): ink / paper / turquesa
const juce::Colour bg      { 0xff0d0d0c };   // ink
const juce::Colour panel   { 0xff171614 };
const juce::Colour panel2  { 0xff26241f };   // ink-soft
const juce::Colour line    { 0xff35322b };
const juce::Colour text    { 0xfff4f1ea };   // paper
const juce::Colour dim     { 0xff7a766d };   // mute (metadatos / labels)
const juce::Colour accent  { 0xff00b3a4 };   // turquesa ★
const juce::Colour deep    { 0xff007a6f };   // turquesa profunda
const juce::Colour bus     { 0xffa8e6df };   // turquesa suave (buses)
const juce::Colour paperSoft { 0xffece8df };
// colores funcionales de directo (sólo estados)
const juce::Colour mute    { 0xffd9534f };
const juce::Colour solo    { 0xffe8c547 };
const juce::Colour meterLo { 0xff00b3a4 };
const juce::Colour meterHi { 0xffd9534f };
}

namespace ltcapp
{
class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::DocumentWindow::backgroundColourId, bg);
        setColour (juce::TextButton::buttonColourId, panel2);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, bg);
        setColour (juce::ComboBox::backgroundColourId, panel2);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, dim);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
        setColour (juce::PopupMenu::highlightedTextColourId, bg);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::trackColourId, accent);
        setColour (juce::Slider::thumbColourId, text);
        setColour (juce::Slider::backgroundColourId, panel2);
        setColour (juce::ListBox::backgroundColourId, panel);
        setColour (juce::TooltipWindow::backgroundColourId, panel2);
        setColour (juce::TooltipWindow::textColourId, text);
        auto scheme = getDarkColourScheme();
        scheme.setUIColour (ColourScheme::windowBackground, bg);
        scheme.setUIColour (ColourScheme::widgetBackground, panel2);
        scheme.setUIColour (ColourScheme::highlightedFill, accent);
        setColourScheme (scheme);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, (float) h * 0.62f)).withStyle ("Bold"));
    }
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) / 2.0f;
        const auto c = r.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        juce::Path bgArc, valArc;
        bgArc.addCentredArc (c.x, c.y, rad - 2, rad - 2, 0, a0, a1, true);
        valArc.addCentredArc (c.x, c.y, rad - 2, rad - 2, 0, a0, ang, true);
        g.setColour (colours::panel2);
        g.strokePath (bgArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (s.findColour (juce::Slider::trackColourId).withAlpha (s.isEnabled() ? 1.0f : 0.4f));
        g.strokePath (valArc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colours::panel);
        g.fillEllipse (juce::Rectangle<float> (rad * 1.3f, rad * 1.3f).withCentre (c));
        g.setColour (colours::text);
        g.drawLine (juce::Line<float> (c, c.getPointOnCircumference (rad * 0.62f, ang)), 2.0f);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (11.5f)); }
    juce::Font getPopupMenuFont() override               { return juce::Font (juce::FontOptions (14.0f)); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = bgc;
        if (down) c = c.brighter (0.15f); else if (over) c = c.brighter (0.07f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (colours::line);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        using namespace colours;
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const auto tint = s.findColour (juce::Slider::trackColourId);

        if (style == juce::Slider::LinearVertical)
        {
            auto groove = r.withSizeKeepingCentre (4.0f, r.getHeight() - 8.0f);
            g.setColour (panel2);
            g.fillRoundedRectangle (groove, 2.0f);
            g.setColour (tint.withAlpha (0.85f));
            g.fillRoundedRectangle (groove.withTop (pos), 2.0f);
            // marca 0 dB
            const float zero = (float) s.getPositionOfValue (0.0);
            g.setColour (dim);
            g.drawHorizontalLine ((int) zero, r.getX() + 2, r.getRight() - 2);
            auto cap = juce::Rectangle<float> (r.getWidth() - 6.0f, 26.0f).withCentre ({ r.getCentreX(), pos });
            g.setColour (juce::Colour (0xff3a3a44));
            g.fillRoundedRectangle (cap, 3.0f);
            g.setColour (tint);
            g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 6.0f, 2.0f));
            return;
        }

        // barras horizontales (envíos / pan)
        g.setColour (panel2);
        g.fillRoundedRectangle (r, 2.0f);
        const bool centred = s.getProperties().getWithDefault ("centred", false);
        g.setColour (tint.withAlpha (s.isEnabled() ? 0.9f : 0.35f));
        if (centred)
        {
            const float c = r.getCentreX();
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (c, pos), r.getY() + 1, juce::jmax (c, pos), r.getBottom() - 1));
            g.setColour (dim);
            g.drawVerticalLine ((int) c, r.getY(), r.getBottom());
        }
        else if (s.getValue() > s.getMinimum())
        {
            g.fillRoundedRectangle (r.withRight (pos), 2.0f);
        }
    }
};
} // namespace ltcapp
