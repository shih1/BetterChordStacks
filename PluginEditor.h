#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class PitchBendEditor : public juce::AudioProcessorEditor
{
public:
  PitchBendEditor(PitchBendProcessor &);
  ~PitchBendEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  PitchBendProcessor &audioProcessor;

  juce::Slider bendAmountSlider;
  juce::Label bendAmountLabel;

  juce::Slider bendTimeSlider;
  juce::Label bendTimeLabel;

  juce::Slider bendCurveSlider;
  juce::Label bendCurveLabel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchBendEditor)
};