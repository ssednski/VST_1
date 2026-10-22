/*
  ==============================================================================

    Overdrive: pre-gain -> tanh soft clipper -> make-up gain, with a dry/wet mix.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class Overdrive
{
public:
    Overdrive()
    {
        auto& shaper = chain.get<shaperIndex>();
        shaper.functionToUse = [] (float x) { return std::tanh (x); };
    }

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        chain.prepare (spec);
        mixer.prepare (spec);

        chain.get<preGainIndex>().setRampDurationSeconds (0.05);
        chain.get<postGainIndex>().setRampDurationSeconds (0.05);
    }

    void reset()
    {
        chain.reset();
        mixer.reset();
    }

    /** Safe to call from the audio thread: only updates smoothed targets. */
    void setParameters (float driveDecibels, float levelDecibels, float mixProportion)
    {
        chain.get<preGainIndex>().setGainDecibels (driveDecibels);
        // Roughly compensate for the loudness added by the drive stage
        chain.get<postGainIndex>().setGainDecibels (levelDecibels - driveDecibels * 0.5f);
        mixer.setWetMixProportion (mixProportion);
    }

    template <typename ProcessContext>
    void process (const ProcessContext& context)
    {
        mixer.pushDrySamples (context.getInputBlock());
        chain.process (context);
        mixer.mixWetSamples (context.getOutputBlock());
    }

private:
    enum
    {
        preGainIndex,
        shaperIndex,
        postGainIndex
    };

    juce::dsp::ProcessorChain<juce::dsp::Gain<float>,
                              juce::dsp::WaveShaper<float>,
                              juce::dsp::Gain<float>> chain;
    juce::dsp::DryWetMixer<float> mixer;
};
