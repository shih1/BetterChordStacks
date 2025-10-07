/*
  ==============================================================================

    PluginEditor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class BetterChordStacksAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    BetterChordStacksAudioProcessorEditor(BetterChordStacksAudioProcessor &);
    ~BetterChordStacksAudioProcessorEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;

private:
    BetterChordStacksAudioProcessor &audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BetterChordStacksAudioProcessorEditor)
};