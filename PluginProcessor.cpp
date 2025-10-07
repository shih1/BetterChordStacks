/*
  ==============================================================================
    BetterChordStacks - Implementation
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <numeric>

//==============================================================================
// Chord Implementation
//==============================================================================
Chord::Chord(std::vector<Note> notes, int64_t ts)
    : notes_(std::move(notes)), timestamp_(ts)
{
    std::sort(notes_.begin(), notes_.end());
}

void Chord::addNote(const Note &note)
{
    auto it = std::find_if(notes_.begin(), notes_.end(),
                           [&note](const Note &n)
                           { return n.pitch == note.pitch; });

    if (it == notes_.end())
    {
        notes_.push_back(note);
        std::sort(notes_.begin(), notes_.end());
    }
}

void Chord::removeNote(int pitch)
{
    notes_.erase(
        std::remove_if(notes_.begin(), notes_.end(),
                       [pitch](const Note &n)
                       { return n.pitch == pitch; }),
        notes_.end());
}

bool Chord::containsNote(int pitch) const
{
    return std::any_of(notes_.begin(), notes_.end(),
                       [pitch](const Note &n)
                       { return n.pitch == pitch; });
}

std::vector<int> Chord::getPitches() const
{
    std::vector<int> pitches;
    pitches.reserve(notes_.size());
    for (const auto &note : notes_)
        pitches.push_back(note.pitch);
    return pitches;
}

//==============================================================================
// Nearest Note Mapping Strategy
//==============================================================================
std::vector<std::pair<int, std::vector<int>>>
NearestNoteMapping::map(const std::vector<int> &source, const std::vector<int> &target) const
{
    if (source.empty() || target.empty())
        return {};

    std::vector<std::pair<int, std::vector<int>>> mapping;
    std::vector<bool> targetUsed(target.size(), false);

    // Initialize mapping for each source note
    for (int srcNote : source)
        mapping.push_back({srcNote, {}});

    // First pass: assign each source to its nearest target
    for (size_t i = 0; i < source.size(); ++i)
    {
        int srcNote = source[i];
        size_t bestIdx = 0;
        int minDist = std::abs(srcNote - target[0]);

        for (size_t j = 1; j < target.size(); ++j)
        {
            int dist = std::abs(srcNote - target[j]);
            if (dist < minDist)
            {
                minDist = dist;
                bestIdx = j;
            }
        }

        mapping[i].second.push_back(target[bestIdx]);
        targetUsed[bestIdx] = true;
    }

    // Second pass: distribute unassigned targets to nearest sources
    for (size_t i = 0; i < target.size(); ++i)
    {
        if (!targetUsed[i])
        {
            size_t nearestSrcIdx = 0;
            int minDist = std::abs(target[i] - source[0]);

            for (size_t j = 1; j < source.size(); ++j)
            {
                int dist = std::abs(target[i] - source[j]);
                if (dist < minDist)
                {
                    minDist = dist;
                    nearestSrcIdx = j;
                }
            }

            mapping[nearestSrcIdx].second.push_back(target[i]);
        }
    }

    return mapping;
}

//==============================================================================
// Random Mapping Strategy
//==============================================================================
std::vector<std::pair<int, std::vector<int>>>
RandomMapping::map(const std::vector<int> &source, const std::vector<int> &target) const
{
    if (source.empty() || target.empty())
        return {};

    std::vector<std::pair<int, std::vector<int>>> mapping;
    std::vector<int> remainingTargets = target;

    // Initial 1-to-1 mapping
    for (int srcNote : source)
    {
        std::vector<int> targets;

        if (!remainingTargets.empty())
        {
            std::uniform_int_distribution<size_t> dist(0, remainingTargets.size() - 1);
            size_t idx = dist(rng_);
            targets.push_back(remainingTargets[idx]);
            remainingTargets.erase(remainingTargets.begin() + idx);
        }

        mapping.push_back({srcNote, targets});
    }

    // Distribute remaining targets randomly
    while (!remainingTargets.empty())
    {
        std::uniform_int_distribution<size_t> dist(0, mapping.size() - 1);
        size_t srcIdx = dist(rng_);
        mapping[srcIdx].second.push_back(remainingTargets.back());
        remainingTargets.pop_back();
    }

    return mapping;
}

//==============================================================================
// MPE Voice Allocator
//==============================================================================
MPEVoiceAllocator::MPEVoiceAllocator()
{
    reset();
}

std::optional<int> MPEVoiceAllocator::allocate()
{
    for (int i = 0; i < MAX_VOICES; ++i)
    {
        if (!channelUsed_[i])
        {
            channelUsed_[i] = true;
            return FIRST_VOICE_CHANNEL + i;
        }
    }
    return std::nullopt;
}

void MPEVoiceAllocator::release(int channel)
{
    if (channel >= FIRST_VOICE_CHANNEL && channel <= LAST_VOICE_CHANNEL)
    {
        int idx = channel - FIRST_VOICE_CHANNEL;
        channelUsed_[idx] = false;
    }
}

void MPEVoiceAllocator::reset()
{
    channelUsed_.fill(false);
}

int MPEVoiceAllocator::getAvailableCount() const
{
    return static_cast<int>(std::count(channelUsed_.begin(), channelUsed_.end(), false));
}

//==============================================================================
// Gliding Voice
//==============================================================================
GlidingVoice::GlidingVoice(int channel, int startPitch, int targetPitch,
                           int64_t startTime, int durationSamples)
    : channel_(channel), startPitch_(startPitch), currentPitch_(startPitch), targetPitch_(targetPitch), glideStartTime_(startTime), glideDurationSamples_(durationSamples)
{
}

void GlidingVoice::updateGlide(int64_t currentTime)
{
    if (!isGliding_)
        return;

    int64_t elapsed = currentTime - glideStartTime_;

    if (elapsed >= glideDurationSamples_)
    {
        currentPitchBend_ = 0.0f;
        currentPitch_ = targetPitch_;
        isGliding_ = false;
    }
    else
    {
        float progress = glideDurationSamples_ > 0
                             ? static_cast<float>(elapsed) / static_cast<float>(glideDurationSamples_)
                             : 1.0f;

        float pitchDiff = static_cast<float>(targetPitch_ - startPitch_);
        currentPitchBend_ = pitchDiff * progress;
    }
}

bool GlidingVoice::hasReachedTarget() const
{
    return !isGliding_ && currentPitch_ == targetPitch_;
}

bool GlidingVoice::shouldSendPitchBend(int64_t currentTime) const
{
    if (!isGliding_)
        return false;

    int64_t timeSinceLastBend = currentTime - lastPitchBendTime_;
    return timeSinceLastBend >= PITCH_BEND_UPDATE_INTERVAL;
}

//==============================================================================
// Chord Transition Engine
//==============================================================================
ChordTransitionEngine::ChordTransitionEngine() = default;

void ChordTransitionEngine::prepare(double sampleRate, int lookaheadSamples)
{
    sampleRate_ = sampleRate;
    lookaheadSamples_ = lookaheadSamples;
    reset();
}

void ChordTransitionEngine::reset()
{
    allocator_.reset();
    activeVoices_.clear();
    midiBuffer_.clear();
    currentChord_.reset();
    pendingChord_.reset();
    currentGlobalTime_ = 0;
}

void ChordTransitionEngine::processBlock(juce::MidiBuffer &midiMessages,
                                         int numSamples,
                                         float glideTimeMs,
                                         float pitchBendRange,
                                         const VoiceMappingStrategy &mappingStrategy)
{
    pitchBendRange_ = pitchBendRange;

    juce::MidiBuffer output;
    int blockStartTime = static_cast<int>(currentGlobalTime_);

    bufferIncomingMidi(midiMessages, blockStartTime);
    processBufferedEvents(output, blockStartTime, numSamples, mappingStrategy);

    midiMessages.swapWith(output);
    currentGlobalTime_ += numSamples;
}

void ChordTransitionEngine::bufferIncomingMidi(const juce::MidiBuffer &input, int blockStartTime)
{
    for (const auto metadata : input)
    {
        auto msg = metadata.getMessage();

        if (msg.isNoteOn() || msg.isNoteOff())
        {
            int64_t globalTime = blockStartTime + metadata.samplePosition;
            midiBuffer_.emplace_back(msg, globalTime);
        }
    }
}

void ChordTransitionEngine::processBufferedEvents(juce::MidiBuffer &output,
                                                  int blockStartTime,
                                                  int blockSize,
                                                  const VoiceMappingStrategy &mappingStrategy)
{
    int blockEndTime = blockStartTime + blockSize;
    int lookaheadEndTime = blockEndTime + lookaheadSamples_;

    // Collect simultaneous note-ons and note-offs
    std::map<int64_t, std::vector<Note>> noteOnsByTime;
    std::vector<BufferedMidiEvent> noteOffs;

    for (const auto &event : midiBuffer_)
    {
        const auto &msg = event.message;

        if (msg.isNoteOn() && event.globalTimestamp < lookaheadEndTime)
        {
            noteOnsByTime[event.globalTimestamp].emplace_back(
                msg.getNoteNumber(), msg.getVelocity(), event.globalTimestamp);
        }
        else if (msg.isNoteOff() &&
                 event.globalTimestamp >= blockStartTime &&
                 event.globalTimestamp < blockEndTime)
        {
            noteOffs.push_back(event);
        }
    }

    // Process note-offs
    for (const auto &noteOff : noteOffs)
    {
        int localSample = static_cast<int>(noteOff.globalTimestamp - blockStartTime);
        localSample = juce::jlimit(0, blockSize - 1, localSample);
        handleNoteOff(noteOff.message, output, localSample);
    }

    // Detect chord changes from simultaneous note-ons
    for (const auto &[timestamp, notes] : noteOnsByTime)
    {
        if (notes.size() >= 2)
            detectChordChange(notes);
    }

    // Start transition if conditions are met
    if (currentChord_.has_value() && pendingChord_.has_value())
    {
        int64_t transitionStartTime = pendingChord_->getTimestamp() - lookaheadSamples_;

        if (currentGlobalTime_ >= transitionStartTime && activeVoices_.empty())
        {
            startTransition(mappingStrategy);
        }
    }

    // Process active voice glides
    processVoiceGlides(output, blockStartTime, blockSize);

    // Clean up processed events
    midiBuffer_.erase(
        std::remove_if(midiBuffer_.begin(), midiBuffer_.end(),
                       [blockEndTime](const BufferedMidiEvent &e)
                       {
                           return e.globalTimestamp < blockEndTime;
                       }),
        midiBuffer_.end());
}

void ChordTransitionEngine::detectChordChange(const std::vector<Note> &simultaneousNotes)
{
    if (simultaneousNotes.empty())
        return;

    Chord newChord(simultaneousNotes, simultaneousNotes[0].timestamp);

    if (!currentChord_.has_value())
    {
        // First chord - start immediately without glide
        currentChord_ = newChord;

        for (const auto &note : newChord.getNotes())
        {
            if (auto channel = allocator_.allocate())
            {
                GlidingVoice voice(*channel, note.pitch, note.pitch,
                                   note.timestamp, 0);
                activeVoices_.push_back(voice);
            }
        }
    }
    else
    {
        // Subsequent chord - queue as pending
        pendingChord_ = newChord;
    }
}

void ChordTransitionEngine::startTransition(const VoiceMappingStrategy &mappingStrategy)
{
    if (!currentChord_.has_value() || !pendingChord_.has_value())
        return;

    auto sourcePitches = currentChord_->getPitches();
    auto targetPitches = pendingChord_->getPitches();
    auto mapping = mappingStrategy.map(sourcePitches, targetPitches);

    // Release old voices and clear
    for (auto &voice : activeVoices_)
        allocator_.release(voice.getChannel());

    activeVoices_.clear();

    int64_t transitionStart = pendingChord_->getTimestamp() - lookaheadSamples_;

    // Create new voices based on mapping
    for (const auto &[srcPitch, targetPitches] : mapping)
    {
        for (int targetPitch : targetPitches)
        {
            if (auto channel = allocator_.allocate())
            {
                GlidingVoice voice(*channel, srcPitch, targetPitch,
                                   transitionStart, lookaheadSamples_);
                activeVoices_.push_back(voice);
            }
        }
    }

    currentChord_ = pendingChord_;
    pendingChord_.reset();
}

void ChordTransitionEngine::processVoiceGlides(juce::MidiBuffer &output,
                                               int blockStartTime,
                                               int blockSize)
{
    for (auto &voice : activeVoices_)
    {
        for (int sample = 0; sample < blockSize; ++sample)
        {
            int64_t globalTime = blockStartTime + sample;
            voice.updateGlide(globalTime);

            // Send initial note-on at glide start
            if (globalTime == voice.getStartPitch() && sample == 0)
            {
                sendMidiNoteOn(output, voice.getChannel(), voice.getStartPitch(), 100, sample);
            }

            // Send pitch bend updates during glide
            if (voice.isGliding() && (sample % GlidingVoice::PITCH_BEND_UPDATE_INTERVAL == 0))
            {
                sendMidiPitchBend(output, voice.getChannel(),
                                  voice.getCurrentPitchBend(), sample);
            }

            // Handle glide completion
            if (voice.hasReachedTarget() && voice.getStartPitch() != voice.getTargetPitch())
            {
                sendMidiNoteOff(output, voice.getChannel(), voice.getStartPitch(), sample);
                sendMidiNoteOn(output, voice.getChannel(), voice.getTargetPitch(), 100, sample);
                sendMidiPitchBend(output, voice.getChannel(), 0.0f, sample);
            }
        }
    }
}

void ChordTransitionEngine::handleNoteOff(const juce::MidiMessage &msg,
                                          juce::MidiBuffer &output,
                                          int localSample)
{
    int pitch = msg.getNoteNumber();

    if (currentChord_.has_value())
    {
        currentChord_->removeNote(pitch);

        // Stop voices playing this pitch
        for (auto &voice : activeVoices_)
        {
            if (voice.getStartPitch() == pitch || voice.getCurrentPitch() == pitch)
            {
                sendMidiNoteOff(output, voice.getChannel(), voice.getCurrentPitch(), localSample);
                allocator_.release(voice.getChannel());
            }
        }

        activeVoices_.erase(
            std::remove_if(activeVoices_.begin(), activeVoices_.end(),
                           [pitch](const GlidingVoice &v)
                           {
                               return v.getStartPitch() == pitch || v.getCurrentPitch() == pitch;
                           }),
            activeVoices_.end());

        if (currentChord_->isEmpty())
        {
            currentChord_.reset();
            pendingChord_.reset();
            activeVoices_.clear();
        }
    }
}

void ChordTransitionEngine::sendMidiNoteOn(juce::MidiBuffer &output, int channel,
                                           int pitch, int velocity, int sample)
{
    output.addEvent(juce::MidiMessage::noteOn(channel, pitch,
                                              static_cast<juce::uint8>(velocity)),
                    sample);
}

void ChordTransitionEngine::sendMidiNoteOff(juce::MidiBuffer &output, int channel,
                                            int pitch, int sample)
{
    output.addEvent(juce::MidiMessage::noteOff(channel, pitch), sample);
}

void ChordTransitionEngine::sendMidiPitchBend(juce::MidiBuffer &output, int channel,
                                              float semitones, int sample)
{
    float normalized = juce::jlimit(-1.0f, 1.0f, semitones / pitchBendRange_);
    int bendValue = static_cast<int>((normalized * 8192.0f) + 8192.0f);
    bendValue = juce::jlimit(0, 16383, bendValue);

    output.addEvent(juce::MidiMessage::pitchWheel(channel, bendValue), sample);
}

//==============================================================================
// Audio Processor
//==============================================================================
BetterChordStacksAudioProcessor::BetterChordStacksAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts_(*this, nullptr, "Parameters", createParameterLayout()), nearestNoteStrategy_(std::make_unique<NearestNoteMapping>()), randomStrategy_(std::make_unique<RandomMapping>())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
BetterChordStacksAudioProcessor::createParameterLayout()
{
    using namespace Parameters;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        GLIDE_TIME_ID, GLIDE_TIME_NAME,
        juce::NormalisableRange<float>(GLIDE_TIME_MIN, GLIDE_TIME_MAX, 1.0f),
        GLIDE_TIME_DEFAULT, "ms"));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        STRATEGY_ID, STRATEGY_NAME,
        STRATEGY_CHOICES, STRATEGY_DEFAULT));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        PITCH_BEND_RANGE_ID, PITCH_BEND_RANGE_NAME,
        juce::NormalisableRange<float>(PITCH_BEND_RANGE_MIN, PITCH_BEND_RANGE_MAX, 1.0f),
        PITCH_BEND_RANGE_DEFAULT, "semitones"));

    return layout;
}

const juce::String BetterChordStacksAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BetterChordStacksAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() &&
           layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void BetterChordStacksAudioProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    float glideTimeMs = *apvts_.getRawParameterValue(Parameters::GLIDE_TIME_ID);
    int lookaheadSamples = static_cast<int>((glideTimeMs / 1000.0f) * newSampleRate);

    engine_.prepare(newSampleRate, lookaheadSamples);
    setLatencySamples(lookaheadSamples);
}

void BetterChordStacksAudioProcessor::releaseResources()
{
    engine_.reset();
}

void BetterChordStacksAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                                   juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    float glideTimeMs = *apvts_.getRawParameterValue(Parameters::GLIDE_TIME_ID);
    float pitchBendRange = *apvts_.getRawParameterValue(Parameters::PITCH_BEND_RANGE_ID);

    // Update latency if glide time changed
    int newLookahead = static_cast<int>((glideTimeMs / 1000.0f) * getSampleRate());
    if (newLookahead != getLatencySamples())
        setLatencySamples(newLookahead);

    const auto &strategy = getCurrentMappingStrategy();
    engine_.processBlock(midiMessages, buffer.getNumSamples(),
                         glideTimeMs, pitchBendRange, strategy);
}

const VoiceMappingStrategy &BetterChordStacksAudioProcessor::getCurrentMappingStrategy() const
{
    int strategyIndex = static_cast<int>(*apvts_.getRawParameterValue(Parameters::STRATEGY_ID));
    return strategyIndex == 0 ? static_cast<const VoiceMappingStrategy &>(*nearestNoteStrategy_) : static_cast<const VoiceMappingStrategy &>(*randomStrategy_);
}

juce::AudioProcessorEditor *BetterChordStacksAudioProcessor::createEditor()
{
    return new BetterChordStacksAudioProcessorEditor(*this);
}

void BetterChordStacksAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BetterChordStacksAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType()))
        apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new BetterChordStacksAudioProcessor();
}