/*
  ==============================================================================

    Real-time spectrum analyser showing the signal leaving the selected
    module (i.e. after it and every module before it in the chain).

  ==============================================================================
*/

#include "SpectrumAnalyser.h"
#include "LookAndFeel.h"

SpectrumAnalyser::SpectrumAnalyser (VST_1AudioProcessor& p)
    : processor (p)
{
    setOpaque (false);
    startTimerHz (30);
}

SpectrumAnalyser::~SpectrumAnalyser()
{
    stopTimer();
}

void SpectrumAnalyser::setSource (const juce::String& moduleName, juce::Colour accentColour)
{
    title = moduleName;
    accent = accentColour;

    // Start fresh so the previous tap point's spectrum doesn't linger
    processor.getAnalyserTap().discardPending();
    std::fill (history.begin(), history.end(), 0.0f);
    std::fill (levels.begin(), levels.end(), minDb);
    newSamplesSinceFft = 0;
    peakDb = minDb;

    rebuildPath();
    repaint();
}

void SpectrumAnalyser::setPathDescription (const juce::String& description)
{
    if (pathDescription != description)
    {
        pathDescription = description;
        repaint();
    }
}

//==============================================================================
void SpectrumAnalyser::timerCallback()
{
    auto& tap = processor.getAnalyserTap();
    float blockPeak = 0.0f;
    int totalPulled = 0;

    for (;;)
    {
        auto numPulled = tap.pull (incoming.data(), (int) incoming.size());
        if (numPulled == 0)
            break;

        for (int i = 0; i < numPulled; ++i)
        {
            auto sample = incoming[(size_t) i];
            blockPeak = juce::jmax (blockPeak, std::abs (sample));

            history[(size_t) historyWritePos] = sample;
            historyWritePos = (historyWritePos + 1) % fftSize;
        }

        newSamplesSinceFft += numPulled;
        totalPulled += numPulled;
    }

    // Peak meter: instant attack, steady fall-off
    peakDb = juce::jmax (juce::Decibels::gainToDecibels (blockPeak, minDb), peakDb - 1.5f);

    if (newSamplesSinceFft >= fftSize / 4)
    {
        runFft();
        newSamplesSinceFft = 0;
    }
    else if (totalPulled == 0)
    {
        // No audio arriving (transport stopped): let the display fall away
        for (auto& level : levels)
            level = juce::jmax (minDb, level - 2.0f);
    }

    rebuildPath();
    repaint();
}

void SpectrumAnalyser::runFft()
{
    // Unroll the ring buffer so the oldest sample comes first
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = history[(size_t) ((historyWritePos + i) % fftSize)];

    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // With a normalised window, a full-scale sine reads as 0 dB
    constexpr float scale = 2.0f / (float) fftSize;

    for (int bin = 0; bin < numBins; ++bin)
    {
        auto db = juce::Decibels::gainToDecibels (fftData[(size_t) bin] * scale, minDb);
        auto& level = levels[(size_t) bin];

        // Fast attack, slower release keeps the display readable
        level = db > level ? db : level + (db - level) * 0.25f;
    }
}

//==============================================================================
float SpectrumAnalyser::frequencyToX (float frequency) const
{
    auto proportion = std::log (frequency / minFreq) / std::log (maxFreq / minFreq);
    return plotArea.getX() + proportion * plotArea.getWidth();
}

float SpectrumAnalyser::decibelsToY (float decibels) const
{
    return juce::jmap (juce::jlimit (minDb, maxDb, decibels), minDb, maxDb, plotArea.getBottom(), plotArea.getY());
}

void SpectrumAnalyser::rebuildPath()
{
    spectrumPath.clear();

    if (plotArea.isEmpty())
        return;

    auto sampleRate = processor.getSampleRate() > 0.0 ? (float) processor.getSampleRate() : 44100.0f;
    auto binForFrequency = [sampleRate] (float frequency) { return frequency * (float) fftSize / sampleRate; };

    constexpr float step = 2.0f;
    auto width = plotArea.getWidth();
    auto ratio = maxFreq / minFreq;

    for (float x = 0.0f; x <= width; x += step)
    {
        auto f0 = minFreq * std::pow (ratio, x / width);
        auto f1 = minFreq * std::pow (ratio, juce::jmin (width, x + step) / width);
        auto b0 = binForFrequency (f0);
        auto b1 = binForFrequency (f1);

        float level = minDb;

        if (b1 - b0 >= 1.0f)
        {
            // Several bins fall in this pixel column: show the loudest
            for (int bin = (int) b0; bin <= (int) b1 && bin < numBins; ++bin)
                level = juce::jmax (level, levels[(size_t) bin]);
        }
        else
        {
            // Fewer bins than pixels: interpolate between neighbours
            auto index = juce::jlimit (0, numBins - 2, (int) b0);
            auto frac = juce::jlimit (0.0f, 1.0f, b0 - (float) index);
            level = juce::jmap (frac, levels[(size_t) index], levels[(size_t) index + 1]);
        }

        auto point = juce::Point<float> (plotArea.getX() + x, decibelsToY (level));

        // Note: Path::isEmpty() stays true until a line is added, so it can't be used to detect the first point
        if (x == 0.0f)
            spectrumPath.startNewSubPath (point);
        else
            spectrumPath.lineTo (point);
    }
}

void SpectrumAnalyser::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (Palette::panel.darker (0.25f));
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds, 12.0f, 1.0f);

    // Header: which module's input we're looking at, and what came before it
    auto header = getLocalBounds().reduced (20, 10).removeFromTop (24);

    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.setColour (Palette::textDim);
    auto prefix = juce::String ("OUTPUT OF ");
    auto prefixWidth = g.getCurrentFont().getStringWidth (prefix);
    g.drawText (prefix, header.removeFromLeft (prefixWidth), juce::Justification::centredLeft);

    g.setColour (accent);
    auto name = title.toUpperCase();
    g.drawText (name, header.removeFromLeft (g.getCurrentFont().getStringWidth (name) + 16), juce::Justification::centredLeft);

    // Peak meter on the right
    auto meterArea = header.removeFromRight (190);
    auto meterText = meterArea.removeFromRight (64);
    auto meterBar = meterArea.withSizeKeepingCentre (meterArea.getWidth() - 8, 6).toFloat();

    g.setColour (Palette::outline);
    g.fillRoundedRectangle (meterBar, 3.0f);
    auto peakProportion = juce::jmap (juce::jlimit (-60.0f, 0.0f, peakDb), -60.0f, 0.0f, 0.0f, 1.0f);
    g.setColour (peakDb > -1.0f ? juce::Colour (0xffff5a5a) : accent);
    g.fillRoundedRectangle (meterBar.withWidth (meterBar.getWidth() * peakProportion), 3.0f);

    g.setColour (Palette::textDim);
    g.setFont (juce::Font (11.0f));
    g.drawText (peakDb <= minDb + 0.5f ? "-inf dB" : juce::String (peakDb, 1) + " dB",
                meterText, juce::Justification::centredRight);

    g.setFont (juce::Font (12.0f));
    g.drawText (pathDescription, header, juce::Justification::centredLeft, true);

    if (plotArea.isEmpty())
        return;

    // Grid
    g.setFont (juce::Font (10.0f));

    for (auto db = minDb + 12.0f; db < maxDb; db += 12.0f)
    {
        auto y = decibelsToY (db);
        g.setColour (Palette::outline.withAlpha (0.5f));
        g.drawHorizontalLine (juce::roundToInt (y), plotArea.getX(), plotArea.getRight());
        g.setColour (Palette::textDim.withAlpha (0.7f));
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (plotArea.getRight() + 4.0f, y - 7.0f, 30.0f, 14.0f),
                    juce::Justification::centredLeft);
    }

    for (auto freq : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        auto x = frequencyToX (freq);
        g.setColour (Palette::outline.withAlpha (0.5f));
        g.drawVerticalLine (juce::roundToInt (x), plotArea.getY(), plotArea.getBottom());
        g.setColour (Palette::textDim.withAlpha (0.7f));
        g.drawText (freq >= 1000.0f ? juce::String ((int) (freq / 1000.0f)) + "k" : juce::String ((int) freq),
                    juce::Rectangle<float> (x - 20.0f, plotArea.getBottom() + 2.0f, 40.0f, 14.0f),
                    juce::Justification::centred);
    }

    // Spectrum
    if (! spectrumPath.isEmpty())
    {
        juce::Path fill (spectrumPath);
        fill.lineTo (plotArea.getRight(), plotArea.getBottom());
        fill.lineTo (plotArea.getX(), plotArea.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.35f), 0.0f, plotArea.getY(),
                                                 accent.withAlpha (0.02f), 0.0f, plotArea.getBottom(), false));
        g.fillPath (fill);

        g.setColour (accent);
        g.strokePath (spectrumPath, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (peakDb <= -90.0f)
    {
        g.setColour (Palette::textDim.withAlpha (0.8f));
        g.setFont (juce::Font (13.0f));
        g.drawText ("No signal leaving this module", plotArea, juce::Justification::centred);
    }
}

void SpectrumAnalyser::resized()
{
    plotArea = getLocalBounds().toFloat().reduced (20.0f, 0.0f)
                   .withTrimmedTop (42.0f).withTrimmedBottom (22.0f).withTrimmedRight (30.0f);
    rebuildPath();
}
