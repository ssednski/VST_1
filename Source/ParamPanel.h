/*
  ==============================================================================

    Editing panel for one effect module. Controls are generated from a list
    of parameter IDs: choice parameters become combo boxes, everything else
    becomes a rotary knob.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class ParamPanel : public juce::Component
{
public:
    ParamPanel (juce::AudioProcessorValueTreeState&, const juce::String& title, const juce::String& description,
                juce::Colour accent, const juce::StringArray& paramIDs);

    void setBypassed (bool isBypassed);
    void setControlEnabled (const juce::String& paramID, bool shouldBeEnabled);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::String paramID;
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> comboBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboBoxAttachment;

        juce::Component& getEditor() { return slider != nullptr ? static_cast<juce::Component&> (*slider) : *comboBox; }
    };

    const juce::String title, description;
    const juce::Colour accent;
    bool bypassed = false;

    std::vector<std::unique_ptr<Control>> controls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamPanel)
};
