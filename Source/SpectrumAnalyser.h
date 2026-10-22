/*
  ==============================================================================

    Real-time spectrum analyser showing the signal arriving at the selected
    module (i.e. after every module before it in the chain).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SpectrumAnalyser : public juce::Component,
                         private juce::Timer
{
public:
    explicit SpectrumAnalyser (VST_1AudioProcessor&);
    ~SpectrumAnalyser() override;

    /** Switches to a new module; clears the current display. */
    void setSource (const juce::String& moduleName, juce::Colour accentColour);

    /** Text describing what the signal has passed through so far. */
    void setPathDescription (const juce::String& description);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void runFft();
    void rebuildPath();

    float frequencyToX (float frequency) const;
    float decibelsToY (float decibels) const;

    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int numBins = fftSize / 2;
    static constexpr float minDb = -96.0f, maxDb = 0.0f;
    static constexpr float minFreq = 20.0f, maxFreq = 20000.0f;

    VST_1AudioProcessor& processor;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, true };

    std::vector<float> history = std::vector<float> ((size_t) fftSize, 0.0f); // ring buffer of the latest samples
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> levels  = std::vector<float> ((size_t) numBins, minDb); // smoothed, in dB
    std::vector<float> incoming = std::vector<float> (8192, 0.0f);

    int historyWritePos = 0;
    int newSamplesSinceFft = 0;
    float peakDb = minDb;

    juce::String title, pathDescription;
    juce::Colour accent { juce::Colours::white };

    juce::Rectangle<float> plotArea;
    juce::Path spectrumPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyser)
};
