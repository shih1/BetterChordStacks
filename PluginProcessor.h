/*
  ==============================================================================
    BetterChordStacks - Modern JUCE MIDI Plugin
    Smooth MPE-based chord transitions with intelligent voice mapping
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <memory>
#include <optional>
#include <array>
#include <random>

//==============================================================================
// Forward declarations
class ChordTransitionEngine;
class MPEVoiceAllocator;

//==============================================================================
/** Parameters for the plugin */
namespace Parameters
{
    inline constexpr const char *GLIDE_TIME_ID = "glideTime";
    inline constexpr const char *GLIDE_TIME_NAME = "Glide Time";
    inline constexpr float GLIDE_TIME_MIN = 10.0f;
    inline constexpr float GLIDE_TIME_MAX = 2000.0f;
    inline constexpr float GLIDE_TIME_DEFAULT = 200.0f;

    inline constexpr const char *STRATEGY_ID = "strategy";
    inline constexpr const char *STRATEGY_NAME = "Mapping Strategy";
    inline const juce::StringArray STRATEGY_CHOICES = {"Nearest Note", "Random"};
    inline constexpr int STRATEGY_DEFAULT = 0;

    inline constexpr const char *PITCH_BEND_RANGE_ID = "pitchBendRange";
    inline constexpr const char *PITCH_BEND_RANGE_NAME = "Pitch Bend Range";
    inline constexpr float PITCH_BEND_RANGE_MIN = 1.0f;
    inline constexpr float PITCH_BEND_RANGE_MAX = 24.0f;
    inline constexpr float PITCH_BEND_RANGE_DEFAULT = 12.0f;
}

//==============================================================================
/** Represents a musical note with timing information */
struct Note
{
    int pitch;
    int velocity;
    int64_t timestamp;

    Note(int p, int v, int64_t t) : pitch(p), velocity(v), timestamp(t) {}

    bool operator<(const Note &other) const { return pitch < other.pitch; }
    bool operator==(const Note &other) const { return pitch == other.pitch; }
};

//==============================================================================
/** A collection of simultaneously played notes */
class Chord
{
public:
    Chord() = default;
    explicit Chord(std::vector<Note> notes, int64_t ts = 0);

    void addNote(const Note &note);
    void removeNote(int pitch);
    bool containsNote(int pitch) const;
    bool isEmpty() const { return notes_.empty(); }
    size_t size() const { return notes_.size(); }
    int64_t getTimestamp() const { return timestamp_; }

    const std::vector<Note> &getNotes() const { return notes_; }
    std::vector<int> getPitches() const;

private:
    std::vector<Note> notes_;
    int64_t timestamp_ = 0;
};

//==============================================================================
/** Strategy pattern for voice mapping algorithms */
class VoiceMappingStrategy
{
public:
    virtual ~VoiceMappingStrategy() = default;

    /** Maps source pitches to target pitches, handling expansion/contraction */
    virtual std::vector<std::pair<int, std::vector<int>>>
    map(const std::vector<int> &source, const std::vector<int> &target) const = 0;
};

class NearestNoteMapping : public VoiceMappingStrategy
{
public:
    std::vector<std::pair<int, std::vector<int>>>
    map(const std::vector<int> &source, const std::vector<int> &target) const override;
};

class RandomMapping : public VoiceMappingStrategy
{
public:
    std::vector<std::pair<int, std::vector<int>>>
    map(const std::vector<int> &source, const std::vector<int> &target) const override;

private:
    mutable std::mt19937 rng_{std::random_device{}()};
};

//==============================================================================
/** Manages MPE channel allocation (channels 2-16, channel 1 is master) */
class MPEVoiceAllocator
{
public:
    static constexpr int MASTER_CHANNEL = 1;
    static constexpr int FIRST_VOICE_CHANNEL = 2;
    static constexpr int LAST_VOICE_CHANNEL = 16;
    static constexpr int MAX_VOICES = LAST_VOICE_CHANNEL - FIRST_VOICE_CHANNEL + 1;

    MPEVoiceAllocator();

    std::optional<int> allocate();
    void release(int channel);
    void reset();
    int getAvailableCount() const;

private:
    std::array<bool, MAX_VOICES> channelUsed_{};
};

//==============================================================================
/** Represents a single gliding voice with MPE channel */
class GlidingVoice
{
public:
    GlidingVoice(int channel, int startPitch, int targetPitch,
                 int64_t startTime, int durationSamples);

    void updateGlide(int64_t currentTime);
    float getCurrentPitchBend() const { return currentPitchBend_; }
    int getChannel() const { return channel_; }
    int getStartPitch() const { return startPitch_; }
    int getCurrentPitch() const { return currentPitch_; }
    int getTargetPitch() const { return targetPitch_; }

    bool isGliding() const { return isGliding_; }
    bool hasReachedTarget() const;
    bool shouldSendPitchBend(int64_t currentTime) const;

    static constexpr int PITCH_BEND_UPDATE_INTERVAL = 8; // samples

private:
    int channel_;
    int startPitch_;
    int currentPitch_;
    int targetPitch_;
    float currentPitchBend_ = 0.0f;

    int64_t glideStartTime_;
    int64_t lastPitchBendTime_ = 0;
    int glideDurationSamples_;
    bool isGliding_ = true;
};

//==============================================================================
/** Main engine for chord transition logic */
class ChordTransitionEngine
{
public:
    ChordTransitionEngine();

    void prepare(double sampleRate, int lookaheadSamples);
    void reset();

    void processBlock(juce::MidiBuffer &midiMessages,
                      int numSamples,
                      float glideTimeMs,
                      float pitchBendRange,
                      const VoiceMappingStrategy &mappingStrategy);

    void processBlock_WithDebug(
        juce::MidiBuffer &midiMessages,
        int numSamples,
        float glideTimeMs,
        float pitchBendRange,
        const VoiceMappingStrategy &mappingStrategy,
        juce::FileLogger *logger);

private:
    struct BufferedMidiEvent
    {
        juce::MidiMessage message;
        int64_t globalTimestamp;

        BufferedMidiEvent(juce::MidiMessage msg, int64_t ts)
            : message(std::move(msg)), globalTimestamp(ts) {}
    };

    void bufferIncomingMidi(const juce::MidiBuffer &input, int blockStartTime);
    void processBufferedEvents(juce::MidiBuffer &output,
                               int blockStartTime,
                               int blockSize,
                               const VoiceMappingStrategy &mappingStrategy);

    void detectChordChange(const std::vector<Note> &simultaneousNotes);
    void startTransition(const VoiceMappingStrategy &mappingStrategy);
    void processVoiceGlides(juce::MidiBuffer &output, int blockStartTime, int blockSize);
    void handleNoteOff(const juce::MidiMessage &msg, juce::MidiBuffer &output, int localSample);

    void sendMidiNoteOn(juce::MidiBuffer &output, int channel, int pitch, int velocity, int sample);
    void sendMidiNoteOff(juce::MidiBuffer &output, int channel, int pitch, int sample);
    void sendMidiPitchBend(juce::MidiBuffer &output, int channel, float semitones, int sample);

    MPEVoiceAllocator allocator_;
    std::vector<GlidingVoice> activeVoices_;
    std::deque<BufferedMidiEvent> midiBuffer_;

    std::optional<Chord> currentChord_;
    std::optional<Chord> pendingChord_;

    double sampleRate_ = 44100.0;
    int lookaheadSamples_ = 0;
    int64_t currentGlobalTime_ = 0;
    float pitchBendRange_ = 12.0f;
};

//==============================================================================
/** Main Audio Processor */
class BetterChordStacksAudioProcessor : public juce::AudioProcessor
{
public:
    BetterChordStacksAudioProcessor();
    ~BetterChordStacksAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String &) override {}

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    juce::AudioProcessorValueTreeState &getAPVTS() { return apvts_; }

private:
    std::unique_ptr<juce::FileLogger> debugLogger_;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    const VoiceMappingStrategy &getCurrentMappingStrategy() const;

    juce::AudioProcessorValueTreeState apvts_;
    ChordTransitionEngine engine_;

    std::unique_ptr<NearestNoteMapping> nearestNoteStrategy_;
    std::unique_ptr<RandomMapping> randomStrategy_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BetterChordStacksAudioProcessor)
};