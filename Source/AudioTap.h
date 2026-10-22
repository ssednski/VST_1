/*
  ==============================================================================

    Lock-free single-producer / single-consumer sample FIFO used to send audio
    from the audio thread to the GUI for visualisation.

    The audio thread pushes (mixing all channels down to mono), the message
    thread pulls. Neither side locks, and push() never allocates. If the GUI
    falls behind, new samples are simply dropped.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AudioTap
{
public:
    explicit AudioTap (int capacity = 1 << 15)
        : fifo (capacity), buffer ((size_t) capacity, 0.0f)
    {
    }

    /** Audio thread only. */
    void push (const juce::dsp::AudioBlock<const float>& block)
    {
        auto numChannels = (int) block.getNumChannels();
        auto numSamples  = (int) block.getNumSamples();

        if (numChannels == 0 || numSamples == 0)
            return;

        auto scale = 1.0f / (float) numChannels;
        auto scope = fifo.write (juce::jmin (numSamples, fifo.getFreeSpace()));

        auto writeRange = [&] (int start, int size, int sourceOffset)
        {
            for (int i = 0; i < size; ++i)
            {
                float sum = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    sum += block.getSample (ch, sourceOffset + i);

                buffer[(size_t) (start + i)] = sum * scale;
            }
        };

        writeRange (scope.startIndex1, scope.blockSize1, 0);
        writeRange (scope.startIndex2, scope.blockSize2, scope.blockSize1);
    }

    /** Message thread only. Returns the number of samples copied into dest. */
    int pull (float* dest, int maxSamples)
    {
        auto scope = fifo.read (juce::jmin (maxSamples, fifo.getNumReady()));

        std::copy_n (buffer.data() + scope.startIndex1, scope.blockSize1, dest);
        std::copy_n (buffer.data() + scope.startIndex2, scope.blockSize2, dest + scope.blockSize1);

        return scope.blockSize1 + scope.blockSize2;
    }

    /** Message thread only: throws away anything waiting to be read. */
    void discardPending()
    {
        fifo.read (fifo.getNumReady());
    }

private:
    juce::AbstractFifo fifo;
    std::vector<float> buffer;

    JUCE_DECLARE_NON_COPYABLE (AudioTap)
};
