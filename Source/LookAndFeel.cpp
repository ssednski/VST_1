/*
  ==============================================================================

    Colour palette and custom LookAndFeel for the multi-effect GUI.

  ==============================================================================
*/

#include "LookAndFeel.h"

juce::Colour Palette::forOption (VST_1AudioProcessor::DSP_Option option)
{
    using Option = VST_1AudioProcessor::DSP_Option;

    switch (option)
    {
        case Option::Phaser:        return juce::Colour (0xffa47cf3);
        case Option::Chorus:        return juce::Colour (0xff3ec5f0);
        case Option::Overdrive:     return juce::Colour (0xffff7a45);
        case Option::LadderFilter:  return juce::Colour (0xff3ee0a8);
        case Option::GeneralFilter: return juce::Colour (0xfff5c84c);
        case Option::END_OF_LIST:   break;
    }

    return text;
}

//==============================================================================
MultiFxLookAndFeel::MultiFxLookAndFeel()
{
    using namespace Palette;

    setColour (juce::ResizableWindow::backgroundColourId, background);

    setColour (juce::Slider::rotarySliderFillColourId, text);
    setColour (juce::Slider::rotarySliderOutlineColourId, outline);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, outline);

    setColour (juce::Label::textColourId, textDim);
    setColour (juce::TextEditor::backgroundColourId, panelLight);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, outline);

    setColour (juce::ComboBox::backgroundColourId, panelLight);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::ComboBox::focusedOutlineColourId, outline);

    setColour (juce::PopupMenu::backgroundColourId, panelLight);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, outline);
    setColour (juce::PopupMenu::highlightedTextColourId, text);

    setColour (juce::TextButton::buttonColourId, panelLight);
    setColour (juce::TextButton::buttonOnColourId, outline);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, text);

    setColour (juce::BubbleComponent::backgroundColourId, panelLight);
    setColour (juce::BubbleComponent::outlineColourId, outline);
    setColour (juce::TooltipWindow::backgroundColourId, panelLight);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, outline);
}

void MultiFxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float startAngle, float endAngle,
                                           juce::Slider& slider)
{
    using namespace Palette;

    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();

    auto enabled = slider.isEnabled();
    auto accent = enabled ? slider.findColour (juce::Slider::rotarySliderFillColourId)
                          : textDim.withAlpha (0.35f);

    auto lineWidth = juce::jmax (2.5f, radius * 0.13f);
    auto arcRadius = radius - lineWidth * 0.5f;
    auto knobRadius = arcRadius - lineWidth * 1.5f;

    // Knob body
    auto knobBounds = juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (panelLight.brighter (0.15f), centre.x, knobBounds.getY(),
                                             panel.darker (0.4f), centre.x, knobBounds.getBottom(), false));
    g.fillEllipse (knobBounds);
    g.setColour (outline);
    g.drawEllipse (knobBounds, 1.0f);

    // Background track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (outline);
    g.strokePath (track, { lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    // Value arc: bipolar parameters grow outwards from zero instead of from the minimum
    auto valueAngle = startAngle + sliderPos * (endAngle - startAngle);
    auto originAngle = startAngle;

    if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        originAngle = startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle);

    if (std::abs (valueAngle - originAngle) > 0.001f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                juce::jmin (originAngle, valueAngle), juce::jmax (originAngle, valueAngle), true);
        g.setColour (accent);
        g.strokePath (valueArc, { lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }

    // Pointer
    auto pointerStart = centre.getPointOnCircumference (knobRadius * 0.35f, valueAngle);
    auto pointerEnd   = centre.getPointOnCircumference (knobRadius * 0.85f, valueAngle);
    g.setColour (enabled ? text : textDim.withAlpha (0.5f));
    g.drawLine ({ pointerStart, pointerEnd }, juce::jmax (2.0f, lineWidth * 0.6f));
}

void MultiFxLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                               bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = backgroundColour;

    if (isDown)
        colour = colour.brighter (0.2f);
    else if (isHighlighted)
        colour = colour.brighter (0.1f);

    g.setColour (colour);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
}
