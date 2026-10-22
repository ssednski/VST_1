/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Overdrive.h"
#include "AudioTap.h"

//==============================================================================
/**
*/
class VST_1AudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    VST_1AudioProcessor();
    ~VST_1AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    enum class DSP_Option
    {
        Phaser,
        Chorus,
        Overdrive,
        LadderFilter,
        GeneralFilter,
        END_OF_LIST
    };

    static constexpr int numDspOptions = static_cast<int> (DSP_Option::END_OF_LIST);
    using DSP_Order = std::array<DSP_Option, numDspOptions>;

    static juce::String getDspOptionName (DSP_Option option);
    static juce::String getBypassParamID (DSP_Option option);
    static DSP_Order getDefaultDspOrder();
    static bool isValidDspOrder (const DSP_Order& order);

    /** Thread safe: may be called from any thread. */
    DSP_Order getDspOrder() const;

    /** Thread safe and lock-free. Invalid orders (not a permutation) are ignored.
        The audio thread picks up the new order at the start of its next block. */
    void setDspOrder (const DSP_Order& newOrder);

    //==============================================================================
    /** Chooses which module's *output* is sent to the visualiser, i.e. the signal
        after that module and every module before it in the chain. Thread safe. */
    void setAnalyserSource (DSP_Option option)  { analyserSource.store (static_cast<int> (option)); }

    /** Turns sample capture on/off (the editor enables it while it is open). */
    void setAnalyserEnabled (bool shouldBeEnabled) { analyserEnabled.store (shouldBeEnabled); }

    /** Read from the message thread only. */
    AudioTap& getAnalyserTap() noexcept { return analyserTap; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters", createParameterLayout() };

private:
    static juce::uint32 packDspOrder (const DSP_Order& order);
    static DSP_Order unpackDspOrder (juce::uint32 packed);

    void updateDspOrderFromShared();
    void updateParameters();
    void updateGeneralFilterCoefficients();

    // Written by the message thread / host, read by the audio thread.
    // The entire order is packed into one word so it is swapped atomically.
    std::atomic<juce::uint32> sharedDspOrder { packDspOrder (getDefaultDspOrder()) };
    static_assert (std::atomic<juce::uint32>::is_always_lock_free);

    // Audio-thread-only copy of the order
    DSP_Order dspOrder = getDefaultDspOrder();
    juce::uint32 lastPackedOrder = 0xffffffff;

    AudioTap analyserTap;
    std::atomic<int> analyserSource { static_cast<int> (DSP_Option::Phaser) };
    std::atomic<bool> analyserEnabled { false };

    //==============================================================================
    juce::dsp::Gain<float> inputGain, outputGain;
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Chorus<float> chorus;
    Overdrive overdrive;
    juce::dsp::LadderFilter<float> ladderFilter;

    using MonoFilter = juce::dsp::IIR::Filter<float>;
    using FilterCoefficients = juce::dsp::IIR::Coefficients<float>;
    juce::dsp::ProcessorDuplicator<MonoFilter, FilterCoefficients> generalFilter;

    double currentSampleRate = 44100.0;
    int lastFilterType = -1;
    float lastFilterFreq = -1.0f, lastFilterQ = -1.0f, lastFilterGain = -1000.0f;

    //==============================================================================
    // Cached raw parameter pointers (atomics owned by the APVTS)
    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;

    std::atomic<float>* phaserRate = nullptr;
    std::atomic<float>* phaserDepth = nullptr;
    std::atomic<float>* phaserCentreFreq = nullptr;
    std::atomic<float>* phaserFeedback = nullptr;
    std::atomic<float>* phaserMix = nullptr;
    std::atomic<float>* phaserBypass = nullptr;

    std::atomic<float>* chorusRate = nullptr;
    std::atomic<float>* chorusDepth = nullptr;
    std::atomic<float>* chorusCentreDelay = nullptr;
    std::atomic<float>* chorusFeedback = nullptr;
    std::atomic<float>* chorusMix = nullptr;
    std::atomic<float>* chorusBypass = nullptr;

    std::atomic<float>* overdriveDrive = nullptr;
    std::atomic<float>* overdriveLevel = nullptr;
    std::atomic<float>* overdriveMix = nullptr;
    std::atomic<float>* overdriveBypass = nullptr;

    std::atomic<float>* ladderMode = nullptr;
    std::atomic<float>* ladderCutoff = nullptr;
    std::atomic<float>* ladderResonance = nullptr;
    std::atomic<float>* ladderDrive = nullptr;
    std::atomic<float>* ladderBypass = nullptr;

    std::atomic<float>* filterType = nullptr;
    std::atomic<float>* filterFreq = nullptr;
    std::atomic<float>* filterQ = nullptr;
    std::atomic<float>* filterGain = nullptr;
    std::atomic<float>* filterBypass = nullptr;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VST_1AudioProcessor)
};
