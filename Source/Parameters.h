/*
  ==============================================================================

    Parameter IDs shared between the processor and the editor.

  ==============================================================================
*/

#pragma once

namespace ParamIDs
{
    // Global
    inline constexpr const char* inputGain          = "inputGain";
    inline constexpr const char* outputGain         = "outputGain";

    // Phaser
    inline constexpr const char* phaserRate         = "phaserRate";
    inline constexpr const char* phaserDepth        = "phaserDepth";
    inline constexpr const char* phaserCentreFreq   = "phaserCentreFreq";
    inline constexpr const char* phaserFeedback     = "phaserFeedback";
    inline constexpr const char* phaserMix          = "phaserMix";
    inline constexpr const char* phaserBypass       = "phaserBypass";

    // Chorus
    inline constexpr const char* chorusRate         = "chorusRate";
    inline constexpr const char* chorusDepth        = "chorusDepth";
    inline constexpr const char* chorusCentreDelay  = "chorusCentreDelay";
    inline constexpr const char* chorusFeedback     = "chorusFeedback";
    inline constexpr const char* chorusMix          = "chorusMix";
    inline constexpr const char* chorusBypass       = "chorusBypass";

    // Overdrive
    inline constexpr const char* overdriveDrive     = "overdriveDrive";
    inline constexpr const char* overdriveLevel     = "overdriveLevel";
    inline constexpr const char* overdriveMix       = "overdriveMix";
    inline constexpr const char* overdriveBypass    = "overdriveBypass";

    // Ladder filter
    inline constexpr const char* ladderMode         = "ladderMode";
    inline constexpr const char* ladderCutoff       = "ladderCutoff";
    inline constexpr const char* ladderResonance    = "ladderResonance";
    inline constexpr const char* ladderDrive        = "ladderDrive";
    inline constexpr const char* ladderBypass       = "ladderBypass";

    // General filter
    inline constexpr const char* filterType         = "filterType";
    inline constexpr const char* filterFreq         = "filterFreq";
    inline constexpr const char* filterQ            = "filterQ";
    inline constexpr const char* filterGain         = "filterGain";
    inline constexpr const char* filterBypass       = "filterBypass";
}

/** Indices of the "filterType" choice parameter. */
enum class GeneralFilterType
{
    Peak,
    Notch,
    BandPass,
    AllPass
};
