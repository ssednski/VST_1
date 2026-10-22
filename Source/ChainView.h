/*
  ==============================================================================

    The signal chain strip: one draggable tile per effect, laid out in
    processing order. Dropping a tile onto another moves it to that position.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/** Toggle drawn as a power icon. Its toggle state is the *bypass* state,
    so the icon is lit while the toggle is off (effect active). */
class PowerButton : public juce::ToggleButton
{
public:
    explicit PowerButton (juce::Colour accentColour);

    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    juce::Colour accent;
};

//==============================================================================
class ChainSlot : public juce::Component,
                  public juce::DragAndDropTarget,
                  public juce::SettableTooltipClient
{
public:
    using DSP_Option = VST_1AudioProcessor::DSP_Option;

    ChainSlot (DSP_Option, juce::AudioProcessorValueTreeState&, const juce::String& bypassParamID);

    DSP_Option getOption() const noexcept { return option; }
    bool isBypassed() const               { return powerButton.getToggleState(); }

    void setSelected (bool shouldBeSelected);
    void setPosition (int newPosition);
    void setBeingDragged (bool isBeingDragged);

    std::function<void (DSP_Option)> onClick;
    std::function<void (DSP_Option dragged, DSP_Option target)> onMove;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    const DSP_Option option;
    const juce::Colour accent;

    PowerButton powerButton;
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment;

    int position = 0;
    bool selected = false, dragHover = false, beingDragged = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainSlot)
};

//==============================================================================
class ChainView : public juce::Component
{
public:
    using DSP_Option = VST_1AudioProcessor::DSP_Option;
    using DSP_Order  = VST_1AudioProcessor::DSP_Order;

    explicit ChainView (juce::AudioProcessorValueTreeState&);

    void setOrder (const DSP_Order& newOrder, bool animate);
    const DSP_Order& getOrder() const noexcept { return order; }

    void setSelected (DSP_Option option);
    void clearDragState();

    /** Called when the user selects a module. */
    std::function<void (DSP_Option)> onSelect;

    /** Called when the user drags a module to a new place in the chain. */
    std::function<void (const DSP_Order&)> onOrderChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<int> getSlotBounds (int position) const;
    juce::Rectangle<int> getSlotArea() const;
    void layoutSlots (bool animate);
    void moveOption (DSP_Option dragged, DSP_Option target);

    std::array<std::unique_ptr<ChainSlot>, VST_1AudioProcessor::numDspOptions> slots; // indexed by DSP_Option
    DSP_Order order = VST_1AudioProcessor::getDefaultDspOrder();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChainView)
};
