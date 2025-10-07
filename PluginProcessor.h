/*
  ==============================================================================

    PluginProcessor.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>
#include <deque>
#include <random>

//==============================================================================
struct BufferedMidiMessage
{
    juce::MidiMessage message;
    int samplePosition;

    BufferedMidiMessage(const juce::MidiMessage &msg, int pos)
        : message(msg), samplePosition(pos) {}
};

//==============================================================================
class MPEChannelManager
{
public:
    MPEChannelManager();
    int assignChannel();
    void releaseChannel(int channel);
    void reset();
    int getAvailableChannelCount() const;

private:
    std::vector<int> availableChannels;
    std::set<int> usedChannels;
};

//==============================================================================
class VoiceMapper
{
public:
    enum Strategy
    {
        NearestNote,
        Random
    };

    static std::vector<std::pair<int, std::vector<int>>> mapNotes(
        const std::vector<int> &currentNotes,
        const std::vector<int> &nextNotes,
        Strategy strategy);
};

//==============================================================================
struct GlidingVoice
{
    int channel;
    int startNote;
    int targetNote;
    float currentPitch;
    int samplePosition;
    int totalSamples;
    bool isActive;
    bool noteOnSent;
    int glideStartSample;

    GlidingVoice();
};

//==============================================================================
class BetterChordStacksAudioProcessor : public juce::AudioProcessor
{
public:
    BetterChordStacksAudioProcessor();
    ~BetterChordStacksAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    MPEChannelManager channelManager;
    std::vector<GlidingVoice> glidingVoices;
    std::deque<BufferedMidiMessage> midiBuffer;

    int lookaheadSamples = 0;
    int currentSampleOffset = 0;

    std::vector<int> currentChord;
    std::vector<int> pendingChord;
    bool hasCurrentChord = false;
    bool hasPendingChord = false;
    int pendingChordTimestamp = 0;

    double sampleRate = 44100.0;

    void bufferMidiMessages(const juce::MidiBuffer &input, int numSamples);
    void processBufferedMidi(juce::MidiBuffer &output, int numSamples);
    void detectChord(const std::vector<BufferedMidiMessage> &simultaneousNotes);
    void startGlideTransition();
    void processGlides(juce::MidiBuffer &output, int startSample, int endSample);
    void sendPitchBend(juce::MidiBuffer &buffer, int channel, float pitchInSemitones, int sampleOffset);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BetterChordStacksAudioProcessor)
};