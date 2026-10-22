/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
    constexpr int headerHeight = 84;
    constexpr int chainHeight = 112;
    constexpr int panelHeight = 220;
    constexpr int footerHeight = 30;
    constexpr int margin = 16;

    juce::String getModuleDescription (VST_1AudioProcessor::DSP_Option option)
    {
        using Option = VST_1AudioProcessor::DSP_Option;

        switch (option)
        {
            case Option::Phaser:        return "Sweeping all-pass stages create moving notches in the spectrum";
            case Option::Chorus:        return "Modulated delay that thickens and widens the sound";
            case Option::Overdrive:     return "Warm tanh soft clipping with level compensation";
            case Option::LadderFilter:  return "Moog-style resonant ladder filter with built-in drive";
            case Option::GeneralFilter: return "Biquad filter: peak, notch, band pass or all pass";
            case Option::END_OF_LIST:   break;
        }

        return {};
    }

    juce::StringArray getParamIDs (VST_1AudioProcessor::DSP_Option option)
    {
        using Option = VST_1AudioProcessor::DSP_Option;
        using namespace ParamIDs;

        switch (option)
        {
            case Option::Phaser:        return { phaserRate, phaserDepth, phaserCentreFreq, phaserFeedback, phaserMix };
            case Option::Chorus:        return { chorusRate, chorusDepth, chorusCentreDelay, chorusFeedback, chorusMix };
            case Option::Overdrive:     return { overdriveDrive, overdriveLevel, overdriveMix };
            case Option::LadderFilter:  return { ladderMode, ladderCutoff, ladderResonance, ladderDrive };
            case Option::GeneralFilter: return { filterType, filterFreq, filterQ, filterGain };
            case Option::END_OF_LIST:   break;
        }

        return {};
    }
}

//==============================================================================
VST_1AudioProcessorEditor::GainKnob::GainKnob (juce::AudioProcessorValueTreeState& apvts,
                                               const juce::String& paramID, const juce::String& name)
    : slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxRight),
      attachment (apvts, paramID, slider)
{
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 20);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.setTooltip ("Double-click to reset");

    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::Font (11.0f, juce::Font::bold));
    label.setJustificationType (juce::Justification::centredLeft);
}

//==============================================================================
VST_1AudioProcessorEditor::VST_1AudioProcessorEditor (VST_1AudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      inputKnob (p.apvts, ParamIDs::inputGain, "INPUT"),
      outputKnob (p.apvts, ParamIDs::outputGain, "OUTPUT"),
      chainView (p.apvts),
      analyser (p)
{
    setLookAndFeel (&lookAndFeel);
    audioProcessor.setAnalyserEnabled (true);
    addAndMakeVisible (analyser);

    for (auto* knob : { &inputKnob, &outputKnob })
    {
        addAndMakeVisible (knob->slider);
        addAndMakeVisible (knob->label);
    }

    chainView.setOrder (audioProcessor.getDspOrder(), false);
    chainView.onSelect = [this] (DSP_Option option) { selectModule (option); };
    chainView.onOrderChanged = [this] (const VST_1AudioProcessor::DSP_Order& order)
    {
        audioProcessor.setDspOrder (order);
        updateAnalyserPathDescription();
    };
    addAndMakeVisible (chainView);

    resetOrderButton.setTooltip ("Restore the default signal chain order");
    resetOrderButton.onClick = [this]
    {
        auto order = VST_1AudioProcessor::getDefaultDspOrder();
        audioProcessor.setDspOrder (order);
        chainView.setOrder (order, true);
        updateAnalyserPathDescription();
    };
    addAndMakeVisible (resetOrderButton);

    for (int i = 0; i < VST_1AudioProcessor::numDspOptions; ++i)
    {
        auto option = static_cast<DSP_Option> (i);
        auto& panel = panels[(size_t) i];

        panel = std::make_unique<ParamPanel> (audioProcessor.apvts, VST_1AudioProcessor::getDspOptionName (option),
                                              getModuleDescription (option), Palette::forOption (option), getParamIDs (option));
        addChildComponent (*panel);
    }

    selectModule (audioProcessor.getDspOrder()[0]);

    setSize (920, 760);
    timerCallback();
    startTimerHz (20);
}

VST_1AudioProcessorEditor::~VST_1AudioProcessorEditor()
{
    stopTimer();
    audioProcessor.setAnalyserEnabled (false);
    setLookAndFeel (nullptr);
}

//==============================================================================
void VST_1AudioProcessorEditor::selectModule (DSP_Option option)
{
    selectedModule = option;
    chainView.setSelected (option);

    for (int i = 0; i < VST_1AudioProcessor::numDspOptions; ++i)
        panels[(size_t) i]->setVisible (static_cast<DSP_Option> (i) == option);

    audioProcessor.setAnalyserSource (option);
    analyser.setSource (VST_1AudioProcessor::getDspOptionName (option), Palette::forOption (option));
    updateAnalyserPathDescription();
}

void VST_1AudioProcessorEditor::updateAnalyserPathDescription()
{
    // Describe what the signal has passed through, up to and including the selected module.
    // Bypassed modules are skipped because they don't change the signal.
    const juce::String arrow ("  >  ");
    juce::StringArray stages { "IN" };
    bool selectedIsBypassed = false;

    for (auto option : chainView.getOrder())
    {
        auto bypassID = VST_1AudioProcessor::getBypassParamID (option);
        auto bypassed = audioProcessor.apvts.getRawParameterValue (bypassID)->load() >= 0.5f;

        if (! bypassed)
            stages.add (VST_1AudioProcessor::getDspOptionName (option));

        if (option == selectedModule)
        {
            selectedIsBypassed = bypassed;
            break;
        }
    }

    auto description = stages.joinIntoString (arrow);

    if (stages.size() == 1)
        description << "  (dry input)";

    if (selectedIsBypassed)
        description << "  (" << VST_1AudioProcessor::getDspOptionName (selectedModule) << " bypassed)";

    analyser.setPathDescription (description);
}

void VST_1AudioProcessorEditor::timerCallback()
{
    // Pick up order changes that didn't come from this editor (e.g. the host restoring state)
    chainView.setOrder (audioProcessor.getDspOrder(), true);

    auto& apvts = audioProcessor.apvts;

    for (int i = 0; i < VST_1AudioProcessor::numDspOptions; ++i)
    {
        auto bypassID = VST_1AudioProcessor::getBypassParamID (static_cast<DSP_Option> (i));
        panels[(size_t) i]->setBypassed (apvts.getRawParameterValue (bypassID)->load() > 0.5f);
    }

    // Only the peak filter uses the gain control
    auto filterType = static_cast<GeneralFilterType> (juce::roundToInt (apvts.getRawParameterValue (ParamIDs::filterType)->load()));
    panels[(size_t) DSP_Option::GeneralFilter]->setControlEnabled (ParamIDs::filterGain, filterType == GeneralFilterType::Peak);

    // Bypass changes and host-driven reorders alter what feeds the analyser
    updateAnalyserPathDescription();
}

void VST_1AudioProcessorEditor::dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&)
{
    chainView.clearDragState();
}

//==============================================================================
void VST_1AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto bounds = getLocalBounds().reduced (margin, 0);

    // Header
    auto header = bounds.removeFromTop (headerHeight);
    g.setColour (Palette::text);
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("MULTI-FX", header.withTrimmedTop (16).removeFromTop (30), juce::Justification::centredLeft);

    g.setColour (Palette::textDim);
    g.setFont (juce::Font (13.0f));
    g.drawText ("VST_1  |  Phaser  -  Chorus  -  Overdrive  -  Ladder Filter  -  Filter",
                header.withTrimmedTop (48).removeFromTop (20), juce::Justification::centredLeft);

    g.setColour (Palette::outline);
    g.drawHorizontalLine (headerHeight - 1, (float) margin, (float) (getWidth() - margin));

    // Chain section caption
    g.setColour (Palette::textDim);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText ("SIGNAL CHAIN", bounds.withTrimmedTop (10).removeFromTop (24), juce::Justification::centredLeft);

    // Footer hint
    g.setFont (juce::Font (12.0f));
    g.drawText ("Drag modules to reorder the chain   |   Click a module to edit it   |   Power button bypasses   |   Double-click a knob to reset",
                getLocalBounds().removeFromBottom (footerHeight), juce::Justification::centred);
}

void VST_1AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (margin, 0);

    auto header = bounds.removeFromTop (headerHeight).reduced (0, 14);
    for (auto* knob : { &outputKnob, &inputKnob })
    {
        auto area = header.removeFromRight (150);
        header.removeFromRight (12);

        // The rotary takes everything left of its 70px text box; the label sits above the text box
        knob->slider.setBounds (area);
        knob->label.setBounds (area.removeFromTop (16).withTrimmedLeft (area.getWidth() - 70));
    }

    bounds.removeFromTop (10);
    auto caption = bounds.removeFromTop (24);
    resetOrderButton.setBounds (caption.removeFromRight (110));

    bounds.removeFromTop (6);
    chainView.setBounds (bounds.removeFromTop (chainHeight).expanded (margin, 0));

    bounds.removeFromTop (12);
    bounds.removeFromBottom (footerHeight);

    auto panelArea = bounds.removeFromBottom (panelHeight);
    for (auto& panel : panels)
        panel->setBounds (panelArea);

    bounds.removeFromBottom (12);
    analyser.setBounds (bounds);
}
