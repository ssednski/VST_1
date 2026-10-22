/*
  ==============================================================================

    Editing panel for one effect module. Controls are generated from a list
    of parameter IDs: choice parameters become combo boxes, everything else
    becomes a rotary knob.

  ==============================================================================
*/

#include "ParamPanel.h"
#include "LookAndFeel.h"

ParamPanel::ParamPanel (juce::AudioProcessorValueTreeState& apvts, const juce::String& titleText,
                        const juce::String& descriptionText, juce::Colour accentColour,
                        const juce::StringArray& paramIDs)
    : title (titleText), description (descriptionText), accent (accentColour)
{
    for (auto& id : paramIDs)
    {
        auto* parameter = apvts.getParameter (id);
        jassert (parameter != nullptr);

        auto control = std::make_unique<Control>();
        control->paramID = id;

        control->label.setText (parameter->getName (32).toUpperCase(), juce::dontSendNotification);
        control->label.setJustificationType (juce::Justification::centred);
        control->label.setFont (juce::Font (12.0f, juce::Font::bold));
        addAndMakeVisible (control->label);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
        {
            control->comboBox = std::make_unique<juce::ComboBox>();
            control->comboBox->addItemList (choice->choices, 1);
            control->comboBox->setJustificationType (juce::Justification::centred);
            control->comboBoxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, *control->comboBox);
            addAndMakeVisible (*control->comboBox);
        }
        else
        {
            control->slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            control->slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
            control->slider->setColour (juce::Slider::rotarySliderFillColourId, accent);
            control->slider->setTooltip ("Double-click to reset");
            control->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, *control->slider);

            // Must come after the attachment, which sets the slider's range
            control->slider->setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
            addAndMakeVisible (*control->slider);
        }

        controls.push_back (std::move (control));
    }
}

void ParamPanel::setBypassed (bool isBypassed)
{
    if (bypassed == isBypassed)
        return;

    bypassed = isBypassed;

    for (auto& control : controls)
        control->getEditor().setAlpha (bypassed ? 0.45f : 1.0f);

    repaint();
}

void ParamPanel::setControlEnabled (const juce::String& paramID, bool shouldBeEnabled)
{
    for (auto& control : controls)
    {
        if (control->paramID == paramID)
        {
            control->getEditor().setEnabled (shouldBeEnabled);
            control->label.setAlpha (shouldBeEnabled ? 1.0f : 0.4f);
        }
    }
}

void ParamPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds, 12.0f, 1.0f);

    auto header = getLocalBounds().reduced (20, 14).removeFromTop (44);

    // Accent marker + title
    auto marker = header.removeFromLeft (6).withSizeKeepingCentre (6, 36).toFloat();
    g.setColour (bypassed ? Palette::outline : accent);
    g.fillRoundedRectangle (marker, 3.0f);
    header.removeFromLeft (12);

    // Bypass badge on the right
    if (bypassed)
    {
        auto badge = header.removeFromRight (96).withSizeKeepingCentre (96, 24).toFloat();
        g.setColour (Palette::background.withAlpha (0.7f));
        g.fillRoundedRectangle (badge, 12.0f);
        g.setColour (Palette::textDim);
        g.drawRoundedRectangle (badge, 12.0f, 1.0f);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText ("BYPASSED", badge, juce::Justification::centred);
    }

    g.setColour (Palette::text);
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText (title, header.removeFromTop (24), juce::Justification::centredLeft);

    g.setColour (Palette::textDim);
    g.setFont (juce::Font (13.0f));
    g.drawText (description, header, juce::Justification::centredLeft);
}

void ParamPanel::resized()
{
    auto area = getLocalBounds().reduced (20, 14);
    area.removeFromTop (56);

    if (controls.empty())
        return;

    constexpr int maxColumnWidth = 130;
    constexpr int labelHeight = 18;

    auto columnWidth = juce::jmin (maxColumnWidth, area.getWidth() / (int) controls.size());
    auto row = area.withSizeKeepingCentre (columnWidth * (int) controls.size(), juce::jmin (area.getHeight(), 170));

    for (auto& control : controls)
    {
        auto column = row.removeFromLeft (columnWidth).reduced (6, 0);
        control->label.setBounds (column.removeFromTop (labelHeight));
        column.removeFromTop (4);

        if (control->slider != nullptr)
            control->slider->setBounds (column);
        else
            control->comboBox->setBounds (column.withTrimmedBottom (20).withSizeKeepingCentre (column.getWidth(), 30)); // line up with the knob centres (above their text boxes)
    }
}
