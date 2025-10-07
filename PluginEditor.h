/*
  ==============================================================================
    PluginEditor.h - Modern UI for BetterChordStacks
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/** Custom rotary slider look and feel */
class ModernRotarySliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
  ModernRotarySliderLookAndFeel();

  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                        juce::Slider &slider) override;

private:
  juce::Colour primaryColour_{0xff4a90e2};
  juce::Colour trackColour_{0xff2a2a2a};
};

//==============================================================================
/** Custom labeled rotary slider component */
class LabeledRotarySlider : public juce::Component
{
public:
  LabeledRotarySlider(const juce::String &labelText, const juce::String &suffix = "");

  void resized() override;
  juce::Slider &getSlider() { return slider_; }

private:
  juce::Slider slider_;
  juce::Label label_;
  juce::Label valueLabel_;
  juce::String suffix_;

  void updateValueLabel();
};

//==============================================================================
/** Main plugin editor */
class BetterChordStacksAudioProcessorEditor : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
  explicit BetterChordStacksAudioProcessorEditor(BetterChordStacksAudioProcessor &p);
  ~BetterChordStacksAudioProcessorEditor() override;

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  void timerCallback() override;
  void drawInfoPanel(juce::Graphics &g, juce::Rectangle<int> area);

  BetterChordStacksAudioProcessor &audioProcessor_;

  ModernRotarySliderLookAndFeel modernLookAndFeel_;

  LabeledRotarySlider glideTimeSlider_;
  LabeledRotarySlider pitchBendRangeSlider_;

  juce::ComboBox strategyComboBox_;
  juce::Label strategyLabel_;

  juce::TextButton infoButton_;
  bool showingInfo_ = false;

  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> glideTimeAttachment_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchBendAttachment_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> strategyAttachment_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BetterChordStacksAudioProcessorEditor)
};