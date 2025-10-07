/*
  ==============================================================================
    PluginEditor.cpp - Modern UI Implementation
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Modern Rotary Slider Look and Feel
//==============================================================================
ModernRotarySliderLookAndFeel::ModernRotarySliderLookAndFeel()
{
  setColour(juce::Slider::rotarySliderFillColourId, primaryColour_);
  setColour(juce::Slider::rotarySliderOutlineColourId, trackColour_);
  setColour(juce::Slider::thumbColourId, juce::Colours::white);
}

void ModernRotarySliderLookAndFeel::drawRotarySlider(juce::Graphics &g,
                                                     int x, int y, int width, int height,
                                                     float sliderPos,
                                                     float rotaryStartAngle, float rotaryEndAngle,
                                                     juce::Slider &slider)
{
  auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(10.0f);
  auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
  auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
  auto lineW = juce::jmin(8.0f, radius * 0.5f);
  auto arcRadius = radius - lineW * 0.5f;

  // Background arc
  juce::Path backgroundArc;
  backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                              arcRadius, arcRadius,
                              0.0f,
                              rotaryStartAngle, rotaryEndAngle,
                              true);

  g.setColour(trackColour_);
  g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

  // Value arc
  if (slider.isEnabled())
  {
    juce::Path valueArc;
    valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(),
                           arcRadius, arcRadius,
                           0.0f,
                           rotaryStartAngle, toAngle,
                           true);

    auto gradient = juce::ColourGradient::horizontal(primaryColour_.darker(0.3f),
                                                     x, primaryColour_,
                                                     x + width);
    g.setGradientFill(gradient);
    g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
  }

  // Pointer
  juce::Path pointer;
  auto pointerLength = radius * 0.33f;
  auto pointerThickness = lineW * 0.75f;

  pointer.addRectangle(-pointerThickness * 0.5f, -radius + lineW,
                       pointerThickness, pointerLength);

  pointer.applyTransform(juce::AffineTransform::rotation(toAngle)
                             .translated(bounds.getCentreX(), bounds.getCentreY()));

  g.setColour(slider.findColour(juce::Slider::thumbColourId));
  g.fillPath(pointer);

  // Center circle
  g.setColour(trackColour_.brighter(0.1f));
  g.fillEllipse(bounds.getCentreX() - 6, bounds.getCentreY() - 6, 12, 12);
}

//==============================================================================
// Labeled Rotary Slider
//==============================================================================
LabeledRotarySlider::LabeledRotarySlider(const juce::String &labelText, const juce::String &suffix)
    : suffix_(suffix)
{
  slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  slider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  addAndMakeVisible(slider_);

  label_.setText(labelText, juce::dontSendNotification);
  label_.setJustificationType(juce::Justification::centred);
  label_.setFont(juce::Font(14.0f, juce::Font::bold));
  addAndMakeVisible(label_);

  valueLabel_.setJustificationType(juce::Justification::centred);
  valueLabel_.setFont(juce::Font(13.0f));
  valueLabel_.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  addAndMakeVisible(valueLabel_);

  slider_.onValueChange = [this]
  { updateValueLabel(); };
  updateValueLabel();
}

void LabeledRotarySlider::resized()
{
  auto bounds = getLocalBounds();

  label_.setBounds(bounds.removeFromTop(20));
  valueLabel_.setBounds(bounds.removeFromBottom(20));
  slider_.setBounds(bounds);
}

void LabeledRotarySlider::updateValueLabel()
{
  auto value = slider_.getValue();
  juce::String text;

  if (value < 100.0)
    text = juce::String(value, 1);
  else
    text = juce::String(static_cast<int>(value));

  text += " " + suffix_;
  valueLabel_.setText(text, juce::dontSendNotification);
}

//==============================================================================
// Plugin Editor
//==============================================================================
BetterChordStacksAudioProcessorEditor::BetterChordStacksAudioProcessorEditor(
    BetterChordStacksAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor_(p), glideTimeSlider_("Glide Time", "ms"), pitchBendRangeSlider_("Bend Range", "st")
{
  setSize(500, 350);
  setResizable(false, false);

  // Apply custom look and feel to sliders
  glideTimeSlider_.getSlider().setLookAndFeel(&modernLookAndFeel_);
  pitchBendRangeSlider_.getSlider().setLookAndFeel(&modernLookAndFeel_);

  // Glide Time Slider
  addAndMakeVisible(glideTimeSlider_);
  glideTimeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor_.getAPVTS(), Parameters::GLIDE_TIME_ID, glideTimeSlider_.getSlider());

  // Pitch Bend Range Slider
  addAndMakeVisible(pitchBendRangeSlider_);
  pitchBendAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      audioProcessor_.getAPVTS(), Parameters::PITCH_BEND_RANGE_ID,
      pitchBendRangeSlider_.getSlider());

  // Strategy ComboBox
  strategyLabel_.setText("Mapping Strategy", juce::dontSendNotification);
  strategyLabel_.setJustificationType(juce::Justification::centred);
  strategyLabel_.setFont(juce::Font(14.0f, juce::Font::bold));
  addAndMakeVisible(strategyLabel_);

  strategyComboBox_.addItemList(Parameters::STRATEGY_CHOICES, 1);
  strategyComboBox_.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(strategyComboBox_);

  strategyAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
      audioProcessor_.getAPVTS(), Parameters::STRATEGY_ID, strategyComboBox_);

  // Info Button
  infoButton_.setButtonText("?");
  infoButton_.setTooltip("Show plugin information");
  infoButton_.onClick = [this]
  { showingInfo_ = !showingInfo_; repaint(); };
  addAndMakeVisible(infoButton_);

  startTimerHz(30);
}

BetterChordStacksAudioProcessorEditor::~BetterChordStacksAudioProcessorEditor()
{
  glideTimeSlider_.getSlider().setLookAndFeel(nullptr);
  pitchBendRangeSlider_.getSlider().setLookAndFeel(nullptr);
}

void BetterChordStacksAudioProcessorEditor::paint(juce::Graphics &g)
{
  // Background gradient
  auto bounds = getLocalBounds().toFloat();
  juce::ColourGradient gradient(juce::Colour(0xff1a1a1a), 0, 0,
                                juce::Colour(0xff0f0f0f), 0, bounds.getBottom(),
                                false);
  g.setGradientFill(gradient);
  g.fillAll();

  // Header
  auto headerArea = getLocalBounds().removeFromTop(60);
  g.setColour(juce::Colour(0xff2a2a2a));
  g.fillRect(headerArea);

  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(28.0f, juce::Font::bold));
  g.drawText("Better Chord Stacks", headerArea, juce::Justification::centred);

  g.setFont(juce::Font(12.0f));
  g.setColour(juce::Colours::lightgrey);
  auto subtitleArea = headerArea.removeFromBottom(18);
  g.drawText("MPE Chord Glide Engine", subtitleArea, juce::Justification::centred);

  // Info panel
  if (showingInfo_)
  {
    auto infoArea = getLocalBounds().reduced(40, 80);
    drawInfoPanel(g, infoArea);
  }

  // Bottom decoration line
  g.setColour(juce::Colour(0xff4a90e2).withAlpha(0.3f));
  g.fillRect(0, getHeight() - 2, getWidth(), 2);
}

void BetterChordStacksAudioProcessorEditor::resized()
{
  auto bounds = getLocalBounds();
  bounds.removeFromTop(60); // Header
  bounds.reduce(40, 30);

  // Info button in top right
  infoButton_.setBounds(getWidth() - 40, 10, 30, 30);

  // Sliders in top row
  auto sliderArea = bounds.removeFromTop(150);
  auto sliderWidth = sliderArea.getWidth() / 2;

  glideTimeSlider_.setBounds(sliderArea.removeFromLeft(sliderWidth).reduced(10));
  pitchBendRangeSlider_.setBounds(sliderArea.reduced(10));

  // Strategy selector below
  bounds.removeFromTop(20);
  auto strategyArea = bounds.removeFromTop(70);
  strategyArea = strategyArea.withSizeKeepingCentre(250, 70);

  strategyLabel_.setBounds(strategyArea.removeFromTop(25));
  strategyArea.removeFromTop(5);
  strategyComboBox_.setBounds(strategyArea.removeFromTop(30));
}

void BetterChordStacksAudioProcessorEditor::timerCallback()
{
  // Future: Add visualization or real-time feedback here
}

void BetterChordStacksAudioProcessorEditor::drawInfoPanel(juce::Graphics &g,
                                                          juce::Rectangle<int> area)
{
  // Semi-transparent background
  g.setColour(juce::Colour(0xff1a1a1a).withAlpha(0.95f));
  g.fillRoundedRectangle(area.toFloat(), 10.0f);

  g.setColour(juce::Colour(0xff4a90e2));
  g.drawRoundedRectangle(area.toFloat().reduced(1), 10.0f, 2.0f);

  area.reduce(20, 20);

  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(16.0f, juce::Font::bold));
  g.drawText("How It Works", area.removeFromTop(25), juce::Justification::centredLeft);

  area.removeFromTop(10);

  juce::String infoText =
      "Better Chord Stacks creates smooth transitions between chords using MPE:\n\n"
      "1. Play a chord (2+ simultaneous notes)\n"
      "2. Play another chord - voices will glide smoothly\n"
      "3. Each voice uses a separate MIDI channel for independent pitch bend\n\n"
      "Glide Time: Duration of the transition\n"
      "Bend Range: Must match your synth's pitch bend range\n"
      "Mapping: How voices are assigned between chords\n\n"
      "Requires: MPE-compatible synthesizer";

  g.setFont(juce::Font(13.0f));
  g.setColour(juce::Colours::lightgrey);
  g.drawMultiLineText(infoText, area.getX(), area.getY() + 15, area.getWidth());
}