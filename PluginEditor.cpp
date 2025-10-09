#include "PluginProcessor.h"
#include "PluginEditor.h"

PitchBendEditor::PitchBendEditor(PitchBendProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
  setSize(400, 300);

  // Bend Amount Slider
  bendAmountSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  bendAmountSlider.setRange(0.0, 2.0, 0.01);
  bendAmountSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 20);
  bendAmountSlider.setValue(*audioProcessor.bendAmount);
  bendAmountSlider.onValueChange = [this]
  {
    *audioProcessor.bendAmount = static_cast<float>(bendAmountSlider.getValue());
  };
  addAndMakeVisible(bendAmountSlider);

  bendAmountLabel.setText("Bend Amount", juce::dontSendNotification);
  bendAmountLabel.setJustificationType(juce::Justification::centred);
  bendAmountLabel.attachToComponent(&bendAmountSlider, false);
  addAndMakeVisible(bendAmountLabel);

  // Bend Time Slider
  bendTimeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  bendTimeSlider.setRange(0.01, 2.0, 0.01);
  bendTimeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 20);
  bendTimeSlider.setValue(*audioProcessor.bendTime);
  bendTimeSlider.onValueChange = [this]
  {
    *audioProcessor.bendTime = static_cast<float>(bendTimeSlider.getValue());
  };
  addAndMakeVisible(bendTimeSlider);

  bendTimeLabel.setText("Bend Time (s)", juce::dontSendNotification);
  bendTimeLabel.setJustificationType(juce::Justification::centred);
  bendTimeLabel.attachToComponent(&bendTimeSlider, false);
  addAndMakeVisible(bendTimeLabel);

  // Bend Curve Slider
  bendCurveSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  bendCurveSlider.setRange(-2.0, 2.0, 0.01);
  bendCurveSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 90, 20);
  bendCurveSlider.setValue(*audioProcessor.bendCurve);
  bendCurveSlider.onValueChange = [this]
  {
    *audioProcessor.bendCurve = static_cast<float>(bendCurveSlider.getValue());
  };
  addAndMakeVisible(bendCurveSlider);

  bendCurveLabel.setText("Bend Curve", juce::dontSendNotification);
  bendCurveLabel.setJustificationType(juce::Justification::centred);
  bendCurveLabel.attachToComponent(&bendCurveSlider, false);
  addAndMakeVisible(bendCurveLabel);
}

PitchBendEditor::~PitchBendEditor()
{
}

void PitchBendEditor::paint(juce::Graphics &g)
{
  g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

  g.setColour(juce::Colours::white);
  g.setFont(20.0f);
  g.drawFittedText("Pitch Bend FX", getLocalBounds().removeFromTop(40),
                   juce::Justification::centred, 1);
}

void PitchBendEditor::resized()
{
  auto area = getLocalBounds().reduced(20);
  area.removeFromTop(40); // Space for title

  auto sliderHeight = area.getHeight() / 3;

  auto bendAmountArea = area.removeFromTop(sliderHeight);
  bendAmountArea.removeFromTop(25); // Space for label
  bendAmountSlider.setBounds(bendAmountArea.withSizeKeepingCentre(120, 120));

  auto bendTimeArea = area.removeFromTop(sliderHeight);
  bendTimeArea.removeFromTop(25);
  bendTimeSlider.setBounds(bendTimeArea.withSizeKeepingCentre(120, 120));

  auto bendCurveArea = area;
  bendCurveArea.removeFromTop(25);
  bendCurveSlider.setBounds(bendCurveArea.withSizeKeepingCentre(120, 120));
}