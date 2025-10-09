#pragma once

#include <JuceHeader.h>

class PitchBendProcessor : public juce::AudioProcessor
{
public:
    PitchBendProcessor();
    ~PitchBendProcessor() override;

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

    // Parameters
    juce::AudioParameterFloat *bendAmount;
    juce::AudioParameterFloat *bendTime;
    juce::AudioParameterFloat *bendCurve;

private:
    struct NoteInfo
    {
        int noteNumber;
        int mpeChannel; // The MPE member channel this note is on
        double startTime;
        int lastBendValue;
        bool isActive;
    };

    std::vector<NoteInfo> activeNotes;
    std::array<bool, 16> channelInUse; // Track which MPE channels are in use
    double currentSampleRate = 44100.0;
    double currentTime = 0.0;
    int updateRateInSamples = 64; // Update pitch bend every N samples

    int calculatePitchBend(double elapsedTime, float amount, float duration, float curve);
    int findAvailableMPEChannel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PitchBendProcessor)
};