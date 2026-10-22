/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

namespace
{
    const juce::Identifier dspOrderPropertyId { "dspOrder" };

    juce::String hzToText (float hz, int)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz"
                             : juce::String (hz, hz < 10.0f ? 2 : 0) + " Hz";
    }

    juce::String percentToText (float value, int)     { return juce::String (juce::roundToInt (value * 100.0f)) + " %"; }
    juce::String decibelsToText (float value, int)    { return juce::String (value, 1) + " dB"; }
    juce::String msToText (float value, int)          { return juce::String (value, 1) + " ms"; }
    juce::String plainToText (float value, int)       { return juce::String (value, 2); }

    float textToNumber (const juce::String& text)     { return text.getFloatValue(); }

    float textToHz (const juce::String& text)
    {
        auto value = text.getFloatValue();
        return text.containsIgnoreCase ("k") ? value * 1000.0f : value;
    }

    float textToProportion (const juce::String& text) { return text.getFloatValue() / 100.0f; }

    juce::NormalisableRange<float> frequencyRange (float min, float max)
    {
        juce::NormalisableRange<float> range { min, max, 0.01f };
        range.setSkewForCentre (std::sqrt (min * max)); // geometric centre feels linear on a log scale
        return range;
    }

    template <typename Attributes>
    std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const juce::String& name,
                                                          juce::NormalisableRange<float> range, float defaultValue,
                                                          Attributes attributes)
    {
        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, defaultValue, attributes);
    }

    std::unique_ptr<juce::AudioParameterBool> makeBypass (const char* id, const juce::String& name)
    {
        return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, false);
    }

    using Attributes = juce::AudioParameterFloatAttributes;

    const auto hzAttributes      = Attributes().withStringFromValueFunction (hzToText).withValueFromStringFunction (textToHz);
    const auto percentAttributes = Attributes().withStringFromValueFunction (percentToText).withValueFromStringFunction (textToProportion);
    const auto dbAttributes      = Attributes().withStringFromValueFunction (decibelsToText).withValueFromStringFunction (textToNumber);
    const auto msAttributes      = Attributes().withStringFromValueFunction (msToText).withValueFromStringFunction (textToNumber);
    const auto plainAttributes   = Attributes().withStringFromValueFunction (plainToText).withValueFromStringFunction (textToNumber);
}

//==============================================================================
VST_1AudioProcessor::VST_1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    auto param = [this] (const char* id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    inputGainParam    = param (ParamIDs::inputGain);
    outputGainParam   = param (ParamIDs::outputGain);

    phaserRate        = param (ParamIDs::phaserRate);
    phaserDepth       = param (ParamIDs::phaserDepth);
    phaserCentreFreq  = param (ParamIDs::phaserCentreFreq);
    phaserFeedback    = param (ParamIDs::phaserFeedback);
    phaserMix         = param (ParamIDs::phaserMix);
    phaserBypass      = param (ParamIDs::phaserBypass);

    chorusRate        = param (ParamIDs::chorusRate);
    chorusDepth       = param (ParamIDs::chorusDepth);
    chorusCentreDelay = param (ParamIDs::chorusCentreDelay);
    chorusFeedback    = param (ParamIDs::chorusFeedback);
    chorusMix         = param (ParamIDs::chorusMix);
    chorusBypass      = param (ParamIDs::chorusBypass);

    overdriveDrive    = param (ParamIDs::overdriveDrive);
    overdriveLevel    = param (ParamIDs::overdriveLevel);
    overdriveMix      = param (ParamIDs::overdriveMix);
    overdriveBypass   = param (ParamIDs::overdriveBypass);

    ladderMode        = param (ParamIDs::ladderMode);
    ladderCutoff      = param (ParamIDs::ladderCutoff);
    ladderResonance   = param (ParamIDs::ladderResonance);
    ladderDrive       = param (ParamIDs::ladderDrive);
    ladderBypass      = param (ParamIDs::ladderBypass);

    filterType        = param (ParamIDs::filterType);
    filterFreq        = param (ParamIDs::filterFreq);
    filterQ           = param (ParamIDs::filterQ);
    filterGain        = param (ParamIDs::filterGain);
    filterBypass      = param (ParamIDs::filterBypass);

    // Give the shared filter state valid coefficients before it is ever prepared
    *generalFilter.state = juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass (currentSampleRate, 1000.0f);
}

VST_1AudioProcessor::~VST_1AudioProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VST_1AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto global = std::make_unique<juce::AudioProcessorParameterGroup> ("global", "Global", "|");
    global->addChild (makeFloat (ParamIDs::inputGain,  "Input",  { -24.0f, 24.0f, 0.1f }, 0.0f, dbAttributes),
                      makeFloat (ParamIDs::outputGain, "Output", { -24.0f, 24.0f, 0.1f }, 0.0f, dbAttributes));

    auto phaser = std::make_unique<juce::AudioProcessorParameterGroup> ("phaser", "Phaser", "|");
    phaser->addChild (makeFloat (ParamIDs::phaserRate,       "Rate",     { 0.01f, 2.0f, 0.01f, 0.5f }, 0.2f, hzAttributes),
                      makeFloat (ParamIDs::phaserDepth,      "Depth",    { 0.0f, 1.0f, 0.01f }, 0.5f, percentAttributes),
                      makeFloat (ParamIDs::phaserCentreFreq, "Centre",   frequencyRange (20.0f, 20000.0f), 1000.0f, hzAttributes),
                      makeFloat (ParamIDs::phaserFeedback,   "Feedback", { -0.95f, 0.95f, 0.01f }, 0.0f, percentAttributes),
                      makeFloat (ParamIDs::phaserMix,        "Mix",      { 0.0f, 1.0f, 0.01f }, 0.5f, percentAttributes),
                      makeBypass (ParamIDs::phaserBypass,    "Phaser Bypass"));

    auto chorus = std::make_unique<juce::AudioProcessorParameterGroup> ("chorus", "Chorus", "|");
    chorus->addChild (makeFloat (ParamIDs::chorusRate,        "Rate",     { 0.01f, 20.0f, 0.01f, 0.4f }, 1.0f, hzAttributes),
                      makeFloat (ParamIDs::chorusDepth,       "Depth",    { 0.0f, 1.0f, 0.01f }, 0.25f, percentAttributes),
                      makeFloat (ParamIDs::chorusCentreDelay, "Delay",    { 1.0f, 100.0f, 0.1f, 0.5f }, 7.0f, msAttributes),
                      makeFloat (ParamIDs::chorusFeedback,    "Feedback", { -0.95f, 0.95f, 0.01f }, 0.0f, percentAttributes),
                      makeFloat (ParamIDs::chorusMix,         "Mix",      { 0.0f, 1.0f, 0.01f }, 0.5f, percentAttributes),
                      makeBypass (ParamIDs::chorusBypass,     "Chorus Bypass"));

    auto drive = std::make_unique<juce::AudioProcessorParameterGroup> ("overdrive", "Overdrive", "|");
    drive->addChild (makeFloat (ParamIDs::overdriveDrive, "Drive", { 0.0f, 40.0f, 0.1f }, 12.0f, dbAttributes),
                     makeFloat (ParamIDs::overdriveLevel, "Level", { -24.0f, 12.0f, 0.1f }, 0.0f, dbAttributes),
                     makeFloat (ParamIDs::overdriveMix,   "Mix",   { 0.0f, 1.0f, 0.01f }, 1.0f, percentAttributes),
                     makeBypass (ParamIDs::overdriveBypass, "Overdrive Bypass"));

    auto ladder = std::make_unique<juce::AudioProcessorParameterGroup> ("ladder", "Ladder Filter", "|");
    ladder->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::ladderMode, 1 }, "Mode",
                                                                    juce::StringArray { "LP 12", "HP 12", "BP 12", "LP 24", "HP 24", "BP 24" }, 3),
                      makeFloat (ParamIDs::ladderCutoff,    "Cutoff",    frequencyRange (20.0f, 20000.0f), 2000.0f, hzAttributes),
                      makeFloat (ParamIDs::ladderResonance, "Resonance", { 0.0f, 1.0f, 0.01f }, 0.2f, percentAttributes),
                      makeFloat (ParamIDs::ladderDrive,     "Drive",     { 1.0f, 10.0f, 0.01f, 0.5f }, 1.0f, plainAttributes),
                      makeBypass (ParamIDs::ladderBypass,   "Ladder Bypass"));

    auto filter = std::make_unique<juce::AudioProcessorParameterGroup> ("filter", "Filter", "|");
    filter->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::filterType, 1 }, "Type",
                                                                    juce::StringArray { "Peak", "Notch", "Band Pass", "All Pass" }, 0),
                      makeFloat (ParamIDs::filterFreq, "Frequency", frequencyRange (20.0f, 20000.0f), 1000.0f, hzAttributes),
                      makeFloat (ParamIDs::filterQ,    "Q",         { 0.1f, 18.0f, 0.01f, 0.35f }, 0.707f, plainAttributes),
                      makeFloat (ParamIDs::filterGain, "Gain",      { -24.0f, 24.0f, 0.1f }, 0.0f, dbAttributes),
                      makeBypass (ParamIDs::filterBypass, "Filter Bypass"));

    layout.add (std::move (global), std::move (phaser), std::move (chorus),
                std::move (drive), std::move (ladder), std::move (filter));
    return layout;
}

//==============================================================================
juce::String VST_1AudioProcessor::getDspOptionName (DSP_Option option)
{
    switch (option)
    {
        case DSP_Option::Phaser:        return "Phaser";
        case DSP_Option::Chorus:        return "Chorus";
        case DSP_Option::Overdrive:     return "Overdrive";
        case DSP_Option::LadderFilter:  return "Ladder Filter";
        case DSP_Option::GeneralFilter: return "Filter";
        case DSP_Option::END_OF_LIST:   break;
    }

    jassertfalse;
    return {};
}

juce::String VST_1AudioProcessor::getBypassParamID (DSP_Option option)
{
    switch (option)
    {
        case DSP_Option::Phaser:        return ParamIDs::phaserBypass;
        case DSP_Option::Chorus:        return ParamIDs::chorusBypass;
        case DSP_Option::Overdrive:     return ParamIDs::overdriveBypass;
        case DSP_Option::LadderFilter:  return ParamIDs::ladderBypass;
        case DSP_Option::GeneralFilter: return ParamIDs::filterBypass;
        case DSP_Option::END_OF_LIST:   break;
    }

    jassertfalse;
    return {};
}

VST_1AudioProcessor::DSP_Order VST_1AudioProcessor::getDefaultDspOrder()
{
    DSP_Order order {};
    for (int i = 0; i < numDspOptions; ++i)
        order[(size_t) i] = static_cast<DSP_Option> (i);
    return order;
}

bool VST_1AudioProcessor::isValidDspOrder (const DSP_Order& order)
{
    std::array<bool, numDspOptions> seen {};

    for (auto option : order)
    {
        auto index = static_cast<int> (option);
        if (! juce::isPositiveAndBelow (index, numDspOptions) || seen[(size_t) index])
            return false;

        seen[(size_t) index] = true;
    }

    return true;
}

// Each slot needs 3 bits (up to 8 options), so the whole order fits in one 32-bit word.
static constexpr int bitsPerSlot = 3;
static_assert (VST_1AudioProcessor::numDspOptions <= (1 << bitsPerSlot));
static_assert (VST_1AudioProcessor::numDspOptions * bitsPerSlot <= 32);

juce::uint32 VST_1AudioProcessor::packDspOrder (const DSP_Order& order)
{
    juce::uint32 packed = 0;
    for (size_t i = 0; i < order.size(); ++i)
        packed |= (juce::uint32) static_cast<int> (order[i]) << (i * bitsPerSlot);
    return packed;
}

VST_1AudioProcessor::DSP_Order VST_1AudioProcessor::unpackDspOrder (juce::uint32 packed)
{
    constexpr juce::uint32 mask = (1u << bitsPerSlot) - 1u;

    DSP_Order order {};
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = static_cast<DSP_Option> ((packed >> (i * bitsPerSlot)) & mask);
    return order;
}

VST_1AudioProcessor::DSP_Order VST_1AudioProcessor::getDspOrder() const
{
    return unpackDspOrder (sharedDspOrder.load (std::memory_order_acquire));
}

void VST_1AudioProcessor::setDspOrder (const DSP_Order& newOrder)
{
    if (! isValidDspOrder (newOrder))
    {
        jassertfalse;
        return;
    }

    sharedDspOrder.store (packDspOrder (newOrder), std::memory_order_release);
}

void VST_1AudioProcessor::updateDspOrderFromShared()
{
    // Lock-free and allocation-free: a single atomic load per block
    auto packed = sharedDspOrder.load (std::memory_order_acquire);

    if (packed != lastPackedOrder)
    {
        dspOrder = unpackDspOrder (packed);
        lastPackedOrder = packed;
    }
}

//==============================================================================
const juce::String VST_1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VST_1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool VST_1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool VST_1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double VST_1AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int VST_1AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int VST_1AudioProcessor::getCurrentProgram()
{
    return 0;
}

void VST_1AudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String VST_1AudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void VST_1AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void VST_1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    inputGain.prepare (spec);
    outputGain.prepare (spec);
    inputGain.setRampDurationSeconds (0.05);
    outputGain.setRampDurationSeconds (0.05);

    phaser.prepare (spec);
    chorus.prepare (spec);
    overdrive.prepare (spec);
    ladderFilter.prepare (spec);
    generalFilter.prepare (spec);

    // Force coefficient recalculation for the new sample rate
    lastFilterType = -1;
    updateParameters();

    phaser.reset();
    chorus.reset();
    overdrive.reset();
    ladderFilter.reset();
    generalFilter.reset();
}

void VST_1AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool VST_1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void VST_1AudioProcessor::updateGeneralFilterCoefficients()
{
    auto type = juce::roundToInt (filterType->load());
    auto freq = juce::jmin (filterFreq->load(), (float) currentSampleRate * 0.49f);
    auto q    = filterQ->load();
    auto gain = filterGain->load();

    if (type == lastFilterType && freq == lastFilterFreq && q == lastFilterQ && gain == lastFilterGain)
        return;

    lastFilterType = type;
    lastFilterFreq = freq;
    lastFilterQ = q;
    lastFilterGain = gain;

    using Coeffs = juce::dsp::IIR::ArrayCoefficients<float>;
    std::array<float, 6> coefficients;

    switch (static_cast<GeneralFilterType> (type))
    {
        case GeneralFilterType::Peak:
            coefficients = Coeffs::makePeakFilter (currentSampleRate, freq, q, juce::Decibels::decibelsToGain (gain));
            break;
        case GeneralFilterType::Notch:
            coefficients = Coeffs::makeNotch (currentSampleRate, freq, q);
            break;
        case GeneralFilterType::BandPass:
            coefficients = Coeffs::makeBandPass (currentSampleRate, freq, q);
            break;
        case GeneralFilterType::AllPass:
        default:
            coefficients = Coeffs::makeAllPass (currentSampleRate, freq, q);
            break;
    }

    // Assigning an array into the existing (shared) coefficients object doesn't allocate,
    // unlike creating a new Coefficients::Ptr, so this is safe on the audio thread.
    *generalFilter.state = coefficients;
}

void VST_1AudioProcessor::updateParameters()
{
    inputGain.setGainDecibels (inputGainParam->load());
    outputGain.setGainDecibels (outputGainParam->load());

    phaser.setRate (phaserRate->load());
    phaser.setDepth (phaserDepth->load());
    phaser.setCentreFrequency (juce::jmin (phaserCentreFreq->load(), (float) currentSampleRate * 0.45f));
    phaser.setFeedback (phaserFeedback->load());
    phaser.setMix (phaserMix->load());

    chorus.setRate (chorusRate->load());
    chorus.setDepth (chorusDepth->load());
    chorus.setCentreDelay (chorusCentreDelay->load());
    chorus.setFeedback (chorusFeedback->load());
    chorus.setMix (chorusMix->load());

    overdrive.setParameters (overdriveDrive->load(), overdriveLevel->load(), overdriveMix->load());

    ladderFilter.setMode (static_cast<juce::dsp::LadderFilterMode> (juce::roundToInt (ladderMode->load())));
    ladderFilter.setCutoffFrequencyHz (juce::jmin (ladderCutoff->load(), (float) currentSampleRate * 0.45f));
    ladderFilter.setResonance (ladderResonance->load());
    ladderFilter.setDrive (ladderDrive->load());

    updateGeneralFilterCoefficients();
}

void VST_1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    updateDspOrderFromShared();
    updateParameters();

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    auto isBypassed = [] (std::atomic<float>* bypassParam) { return bypassParam->load() > 0.5f; };

    inputGain.process (context);

    auto tapOption = analyserEnabled.load() ? static_cast<DSP_Option> (analyserSource.load())
                                            : DSP_Option::END_OF_LIST;

    for (auto option : dspOrder)
    {
        // Capture the signal arriving at the selected module, before it is processed
        if (option == tapOption)
            analyserTap.push (block);

        switch (option)
        {
            case DSP_Option::Phaser:
                if (! isBypassed (phaserBypass))     phaser.process (context);
                break;
            case DSP_Option::Chorus:
                if (! isBypassed (chorusBypass))     chorus.process (context);
                break;
            case DSP_Option::Overdrive:
                if (! isBypassed (overdriveBypass))  overdrive.process (context);
                break;
            case DSP_Option::LadderFilter:
                if (! isBypassed (ladderBypass))     ladderFilter.process (context);
                break;
            case DSP_Option::GeneralFilter:
                if (! isBypassed (filterBypass))     generalFilter.process (context);
                break;
            case DSP_Option::END_OF_LIST:
                jassertfalse;
                break;
        }
    }

    outputGain.process (context);
}

//==============================================================================
bool VST_1AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* VST_1AudioProcessor::createEditor()
{
    return new VST_1AudioProcessorEditor (*this);
}

//==============================================================================
void VST_1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    juce::StringArray order;
    for (auto option : getDspOrder())
        order.add (juce::String (static_cast<int> (option)));

    state.setProperty (dspOrderPropertyId, order.joinIntoString (","), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VST_1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);

    auto tokens = juce::StringArray::fromTokens (state.getProperty (dspOrderPropertyId).toString(), ",", {});
    if (tokens.size() == numDspOptions)
    {
        DSP_Order order {};
        for (int i = 0; i < numDspOptions; ++i)
            order[(size_t) i] = static_cast<DSP_Option> (tokens[i].getIntValue());

        if (isValidDspOrder (order))
            setDspOrder (order);
    }

    state.removeProperty (dspOrderPropertyId, nullptr);
    apvts.replaceState (state);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VST_1AudioProcessor();
}
