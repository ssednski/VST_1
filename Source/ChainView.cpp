/*
  ==============================================================================

    The signal chain strip: one draggable tile per effect, laid out in
    processing order. Dropping a tile onto another moves it to that position.

  ==============================================================================
*/

#include "ChainView.h"
#include "LookAndFeel.h"

PowerButton::PowerButton (juce::Colour accentColour)
    : accent (accentColour)
{
    setTooltip ("Enable / bypass this module");
}

void PowerButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    auto bounds = getLocalBounds().toFloat();
    auto size = juce::jmin (bounds.getWidth(), bounds.getHeight()) - 2.0f;
    auto circle = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());

    auto active = ! getToggleState();
    auto colour = active ? accent : Palette::textDim.withAlpha (0.6f);

    if (isHighlighted || isDown)
        colour = colour.brighter (0.25f);

    g.setColour (active ? accent.withAlpha (0.16f) : Palette::background.withAlpha (0.5f));
    g.fillEllipse (circle);

    auto icon = circle.reduced (size * 0.27f);
    auto centre = icon.getCentre();
    auto radius = icon.getWidth() * 0.5f;

    juce::Path path;
    path.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                        0.7f, juce::MathConstants<float>::twoPi - 0.7f, true);
    path.startNewSubPath (centre.x, icon.getY() - 1.0f);
    path.lineTo (centre.x, centre.y);

    g.setColour (colour);
    g.strokePath (path, { 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
}

//==============================================================================
ChainSlot::ChainSlot (DSP_Option optionToUse, juce::AudioProcessorValueTreeState& apvts, const juce::String& bypassParamID)
    : option (optionToUse),
      accent (Palette::forOption (optionToUse)),
      powerButton (accent),
      bypassAttachment (apvts, bypassParamID, powerButton)
{
    powerButton.onStateChange = [this] { repaint(); };
    addAndMakeVisible (powerButton);

    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Click to edit, drag to reorder");
}

void ChainSlot::setSelected (bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void ChainSlot::setPosition (int newPosition)
{
    if (position != newPosition)
    {
        position = newPosition;
        repaint();
    }
}

void ChainSlot::setBeingDragged (bool isBeingDragged)
{
    beingDragged = isBeingDragged;
    setAlpha (beingDragged ? 0.35f : 1.0f);
}

void ChainSlot::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    constexpr float corner = 10.0f;
    auto bypassed = isBypassed();

    auto background = selected ? Palette::panelLight : Palette::panel;
    if (dragHover)
        background = background.interpolatedWith (accent, 0.15f);

    g.setColour (background);
    g.fillRoundedRectangle (bounds, corner);

    // Accent strip along the top
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, corner);
        g.reduceClipRegion (clip);
        g.setColour (bypassed ? Palette::outline : accent);
        g.fillRect (bounds.withHeight (4.0f));
    }

    auto outlineColour = dragHover ? accent : (selected ? accent.withAlpha (0.8f) : Palette::outline);
    g.setColour (outlineColour);
    g.drawRoundedRectangle (bounds, corner, selected || dragHover ? 2.0f : 1.0f);

    auto content = getLocalBounds().reduced (10, 8);
    content.removeFromTop (4);

    // Position badge
    auto badge = content.removeFromTop (22).removeFromLeft (22).toFloat();
    g.setColour (Palette::background.withAlpha (0.6f));
    g.fillEllipse (badge);
    g.setColour (Palette::textDim);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText (juce::String (position + 1), badge, juce::Justification::centred);

    // Name and status
    g.setColour (bypassed ? Palette::textDim : Palette::text);
    g.setFont (juce::Font (16.0f, juce::Font::bold));
    g.drawFittedText (VST_1AudioProcessor::getDspOptionName (option),
                      content.removeFromTop (24), juce::Justification::centred, 1);

    g.setColour (bypassed ? Palette::textDim.withAlpha (0.7f) : accent);
    g.setFont (juce::Font (11.0f));
    g.drawText (bypassed ? "BYPASSED" : "ACTIVE", content.removeFromTop (16), juce::Justification::centred);

    // Grip dots hint that the tile can be dragged
    auto grip = content.withSizeKeepingCentre (18, 8).toFloat();
    g.setColour (Palette::textDim.withAlpha (0.5f));
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 3; ++col)
            g.fillEllipse (grip.getX() + (float) col * 7.0f, grip.getY() + (float) row * 5.0f, 3.0f, 3.0f);
}

void ChainSlot::resized()
{
    powerButton.setBounds (getWidth() - 34, 10, 24, 24);
}

void ChainSlot::mouseDown (const juce::MouseEvent&)
{
    if (onClick != nullptr)
        onClick (option);
}

void ChainSlot::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 6)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        if (! container->isDragAndDropActive())
        {
            container->startDragging (static_cast<int> (option), this);
            setBeingDragged (true);
        }
    }
}

bool ChainSlot::isInterestedInDragSource (const SourceDetails& details)
{
    auto* source = dynamic_cast<ChainSlot*> (details.sourceComponent.get());
    return source != nullptr && source != this;
}

void ChainSlot::itemDragEnter (const SourceDetails&)
{
    dragHover = true;
    repaint();
}

void ChainSlot::itemDragExit (const SourceDetails&)
{
    dragHover = false;
    repaint();
}

void ChainSlot::itemDropped (const SourceDetails& details)
{
    dragHover = false;
    repaint();

    if (auto* source = dynamic_cast<ChainSlot*> (details.sourceComponent.get()))
        if (onMove != nullptr)
            onMove (source->getOption(), option);
}

//==============================================================================
ChainView::ChainView (juce::AudioProcessorValueTreeState& apvts)
{
    for (int i = 0; i < VST_1AudioProcessor::numDspOptions; ++i)
    {
        auto option = static_cast<DSP_Option> (i);
        auto& slot = slots[(size_t) i];

        slot = std::make_unique<ChainSlot> (option, apvts, VST_1AudioProcessor::getBypassParamID (option));
        slot->onClick = [this] (DSP_Option o) { setSelected (o); if (onSelect != nullptr) onSelect (o); };
        slot->onMove  = [this] (DSP_Option dragged, DSP_Option target) { moveOption (dragged, target); };
        addAndMakeVisible (*slot);
    }
}

void ChainView::setOrder (const DSP_Order& newOrder, bool animate)
{
    if (newOrder == order || ! VST_1AudioProcessor::isValidDspOrder (newOrder))
        return;

    order = newOrder;
    layoutSlots (animate);
}

void ChainView::setSelected (DSP_Option option)
{
    for (auto& slot : slots)
        slot->setSelected (slot->getOption() == option);
}

void ChainView::clearDragState()
{
    for (auto& slot : slots)
        slot->setBeingDragged (false);
}

void ChainView::moveOption (DSP_Option dragged, DSP_Option target)
{
    std::vector<DSP_Option> list (order.begin(), order.end());

    auto from = std::find (list.begin(), list.end(), dragged) - list.begin();
    auto to   = std::find (list.begin(), list.end(), target)  - list.begin();

    if (from == to)
        return;

    list.erase (list.begin() + from);
    list.insert (list.begin() + to, dragged);

    std::copy (list.begin(), list.end(), order.begin());
    clearDragState();
    layoutSlots (true);

    if (onOrderChanged != nullptr)
        onOrderChanged (order);
}

juce::Rectangle<int> ChainView::getSlotArea() const
{
    return getLocalBounds().reduced (56, 4);
}

juce::Rectangle<int> ChainView::getSlotBounds (int position) const
{
    constexpr int gap = 30;
    constexpr int numSlots = VST_1AudioProcessor::numDspOptions;

    auto area = getSlotArea();
    auto slotWidth = (area.getWidth() - gap * (numSlots - 1)) / numSlots;

    return { area.getX() + position * (slotWidth + gap), area.getY(), slotWidth, area.getHeight() };
}

void ChainView::layoutSlots (bool animate)
{
    auto& animator = juce::Desktop::getInstance().getAnimator();

    for (int position = 0; position < VST_1AudioProcessor::numDspOptions; ++position)
    {
        auto& slot = *slots[(size_t) order[(size_t) position]];
        slot.setPosition (position);

        auto target = getSlotBounds (position);

        if (animate && isShowing() && ! slot.getBounds().isEmpty())
        {
            animator.animateComponent (&slot, target, 1.0f, 200, false, 1.0, 0.2);
        }
        else
        {
            animator.cancelAnimation (&slot, false);
            slot.setBounds (target);
        }
    }

    repaint();
}

void ChainView::paint (juce::Graphics& g)
{
    auto area = getSlotArea();
    auto centreY = (float) area.getCentreY();

    auto drawTerminal = [&g, centreY] (juce::Rectangle<float> r, const juce::String& text)
    {
        r = r.withSizeKeepingCentre (r.getWidth(), 26.0f).withCentre ({ r.getCentreX(), centreY });
        g.setColour (Palette::panelLight);
        g.fillRoundedRectangle (r, 13.0f);
        g.setColour (Palette::outline);
        g.drawRoundedRectangle (r, 13.0f, 1.0f);
        g.setColour (Palette::textDim);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (text, r, juce::Justification::centred);
    };

    auto bounds = getLocalBounds().toFloat();
    auto inTerminal  = bounds.withWidth (40.0f);
    auto outTerminal = bounds.withLeft (bounds.getRight() - 40.0f);

    auto drawArrow = [&g, centreY] (float x1, float x2)
    {
        g.setColour (Palette::textDim.withAlpha (0.6f));
        g.drawArrow ({ x1 + 3.0f, centreY, x2 - 3.0f, centreY }, 2.0f, 9.0f, 8.0f);
    };

    // Arrows follow the target layout (not the animating tiles) so they stay put
    drawArrow (inTerminal.getRight(), (float) getSlotBounds (0).getX());

    for (int i = 0; i < VST_1AudioProcessor::numDspOptions - 1; ++i)
        drawArrow ((float) getSlotBounds (i).getRight(), (float) getSlotBounds (i + 1).getX());

    drawArrow ((float) getSlotBounds (VST_1AudioProcessor::numDspOptions - 1).getRight(), outTerminal.getX());

    drawTerminal (inTerminal, "IN");
    drawTerminal (outTerminal, "OUT");
}

void ChainView::resized()
{
    layoutSlots (false);
}
