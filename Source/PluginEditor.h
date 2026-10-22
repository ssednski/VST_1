/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"
#include "ChainView.h"
#include "ParamPanel.h"
#include "SpectrumAnalyser.h"

//==============================================================================
/**
*/
class VST_1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                   public juce::DragAndDropContainer,
                                   private juce::Timer
{
public:
    VST_1AudioProcessorEditor (VST_1AudioProcessor&);
    ~VST_1AudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using DSP_Option = VST_1AudioProcessor::DSP_Option;

    void timerCallback() override;
    void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&) override;
    void selectModule (DSP_Option option);
    void updateAnalyserPathDescription();

    struct GainKnob
    {
        GainKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& name);

        juce::Slider slider;
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    // Declared first so it outlives every component that uses it
    MultiFxLookAndFeel lookAndFeel;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    VST_1AudioProcessor& audioProcessor;

    juce::TooltipWindow tooltipWindow { this, 600 };

    GainKnob inputKnob, outputKnob;
    ChainView chainView;
    SpectrumAnalyser analyser;
    juce::TextButton resetOrderButton { "Reset Order" };
    std::array<std::unique_ptr<ParamPanel>, VST_1AudioProcessor::numDspOptions> panels; // indexed by DSP_Option

    DSP_Option selectedModule = DSP_Option::Phaser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VST_1AudioProcessorEditor)
};
