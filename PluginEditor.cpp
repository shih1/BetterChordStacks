/*
  ==============================================================================

    PluginEditor.cpp

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BetterChordStacksAudioProcessorEditor::BetterChordStacksAudioProcessorEditor(BetterChordStacksAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(400, 300);
}

BetterChordStacksAudioProcessorEditor::~BetterChordStacksAudioProcessorEditor()
{
}

void BetterChordStacksAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("Better Chord Stacks - Use Generic Editor for now", getLocalBounds(), juce::Justification::centred, 1);
}

void BetterChordStacksAudioProcessorEditor::resized()
{
}