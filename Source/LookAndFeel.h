/*
  ==============================================================================

    Colour palette and custom LookAndFeel for the multi-effect GUI.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

namespace Palette
{
    const juce::Colour background  { 0xff14161b };
    const juce::Colour panel       { 0xff1d2128 };
    const juce::Colour panelLight  { 0xff272c35 };
    const juce::Colour outline     { 0xff363c48 };
    const juce::Colour text        { 0xffe8eaf0 };
    const juce::Colour textDim     { 0xff8a91a0 };

    juce::Colour forOption (VST_1AudioProcessor::DSP_Option option);
}

class MultiFxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MultiFxLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
