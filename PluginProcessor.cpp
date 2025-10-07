#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// MPE Channel Manager Implementation
//==============================================================================
MPEChannelManager::MPEChannelManager()
{
    reset();
}

int MPEChannelManager::assignChannel()
{
    if (availableChannels.empty())
        return -1;

    int channel = availableChannels.front();
    availableChannels.erase(availableChannels.begin());
    usedChannels.insert(channel);
    return channel;
}

void MPEChannelManager::releaseChannel(int channel)
{
    if (usedChannels.count(channel))
    {
        usedChannels.erase(channel);
        availableChannels.push_back(channel);
    }
}

void MPEChannelManager::reset()
{
    availableChannels.clear();
    usedChannels.clear();
    // MPE Lower Zone: channels 2-16 (channel 1 is master)
    for (int i = 2; i <= 16; ++i)
        availableChannels.push_back(i);
}

int MPEChannelManager::getAvailableChannelCount() const
{
    return (int)availableChannels.size();
}

//==============================================================================
// Voice Mapper Implementation
//==============================================================================
std::vector<std::pair<int, std::vector<int>>> VoiceMapper::mapNotes(
    const std::vector<int> &currentNotes,
    const std::vector<int> &nextNotes,
    Strategy strategy)
{
    std::vector<std::pair<int, std::vector<int>>> mapping;

    if (currentNotes.empty() || nextNotes.empty())
        return mapping;

    if (strategy == NearestNote)
    {
        // Build nearest-note mapping with expansion support
        std::vector<bool> targetUsed(nextNotes.size(), false);

        // Create initial mapping
        for (size_t i = 0; i < currentNotes.size(); ++i)
        {
            mapping.push_back({currentNotes[i], {}});
        }

        // First pass: assign each source to nearest target
        for (size_t i = 0; i < currentNotes.size(); ++i)
        {
            int current = currentNotes[i];
            int bestTargetIdx = 0;
            int minDist = std::abs(current - nextNotes[0]);

            for (size_t j = 0; j < nextNotes.size(); ++j)
            {
                int dist = std::abs(current - nextNotes[j]);
                if (dist < minDist)
                {
                    minDist = dist;
                    bestTargetIdx = (int)j;
                }
            }

            mapping[i].second.push_back(nextNotes[bestTargetIdx]);
            targetUsed[bestTargetIdx] = true;
        }

        // Handle unassigned targets - distribute to nearest sources
        for (size_t i = 0; i < nextNotes.size(); ++i)
        {
            if (!targetUsed[i])
            {
                // Find nearest source note
                int nearestSourceIdx = 0;
                int minDist = std::abs(nextNotes[i] - currentNotes[0]);

                for (size_t j = 0; j < currentNotes.size(); ++j)
                {
                    int dist = std::abs(nextNotes[i] - currentNotes[j]);
                    if (dist < minDist)
                    {
                        minDist = dist;
                        nearestSourceIdx = (int)j;
                    }
                }

                mapping[nearestSourceIdx].second.push_back(nextNotes[i]);
            }
        }
    }
    else if (strategy == Random)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        std::vector<int> remainingTargets = nextNotes;

        for (int current : currentNotes)
        {
            std::vector<int> targets;

            if (!remainingTargets.empty())
            {
                std::uniform_int_distribution<> dist(0, (int)remainingTargets.size() - 1);
                int idx = dist(gen);
                targets.push_back(remainingTargets[idx]);
                remainingTargets.erase(remainingTargets.begin() + idx);
            }

            mapping.push_back({current, targets});
        }

        // Distribute remaining targets randomly
        while (!remainingTargets.empty())
        {
            std::uniform_int_distribution<> dist(0, (int)mapping.size() - 1);
            int sourceIdx = dist(gen);
            mapping[sourceIdx].second.push_back(remainingTargets.back());
            remainingTargets.pop_back();
        }
    }

    return mapping;
}

//==============================================================================
// Gliding Voice Implementation
//==============================================================================
GlidingVoice::GlidingVoice()
    : channel(-1), startNote(-1), targetNote(-1),
      currentPitch(0.0f), samplePosition(0),
      totalSamples(0), isActive(false), noteOnSent(false),
      glideStartSample(0)
{
}

//==============================================================================
// Audio Processor Implementation
//==============================================================================
BetterChordStacksAudioProcessor::BetterChordStacksAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

BetterChordStacksAudioProcessor::~BetterChordStacksAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BetterChordStacksAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "glideTime",
        "Glide Time",
        juce::NormalisableRange<float>(10.0f, 2000.0f, 1.0f),
        200.0f,
        "ms"));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "strategy",
        "Mapping Strategy",
        juce::StringArray{"Nearest Note", "Random"},
        0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "pitchBendRange",
        "Pitch Bend Range",
        juce::NormalisableRange<float>(1.0f, 24.0f, 1.0f),
        12.0f,
        "semitones"));

    return layout;
}

const juce::String BetterChordStacksAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BetterChordStacksAudioProcessor::acceptsMidi() const
{
    return true;
}

bool BetterChordStacksAudioProcessor::producesMidi() const
{
    return true;
}

bool BetterChordStacksAudioProcessor::isMidiEffect() const
{
    return true;
}

double BetterChordStacksAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int BetterChordStacksAudioProcessor::getNumPrograms()
{
    return 1;
}

int BetterChordStacksAudioProcessor::getCurrentProgram()
{
    return 0;
}

void BetterChordStacksAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String BetterChordStacksAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void BetterChordStacksAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

void BetterChordStacksAudioProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    sampleRate = newSampleRate;

    float glideTimeMs = *apvts.getRawParameterValue("glideTime");
    lookaheadSamples = static_cast<int>((glideTimeMs / 1000.0f) * sampleRate);

    channelManager.reset();
    glidingVoices.clear();
    midiBuffer.clear();
    currentChord.clear();
    pendingChord.clear();
    hasCurrentChord = false;
    hasPendingChord = false;
    currentSampleOffset = 0;

    setLatencySamples(lookaheadSamples);
}

void BetterChordStacksAudioProcessor::releaseResources()
{
}

bool BetterChordStacksAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo() && layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void BetterChordStacksAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    juce::MidiBuffer outputMidi;
    int numSamples = buffer.getNumSamples();

    // Update lookahead if glide time changed
    float glideTimeMs = *apvts.getRawParameterValue("glideTime");
    int newLookahead = static_cast<int>((glideTimeMs / 1000.0f) * sampleRate);
    if (newLookahead != lookaheadSamples)
    {
        lookaheadSamples = newLookahead;
        setLatencySamples(lookaheadSamples);
    }

    bufferMidiMessages(midiMessages, numSamples);
    processBufferedMidi(outputMidi, numSamples);

    midiMessages.swapWith(outputMidi);
    currentSampleOffset += numSamples;
}

void BetterChordStacksAudioProcessor::bufferMidiMessages(const juce::MidiBuffer &input, int numSamples)
{
    juce::ignoreUnused(numSamples);

    for (const auto metadata : input)
    {
        auto msg = metadata.getMessage();
        int globalSamplePos = currentSampleOffset + metadata.samplePosition;

        if (msg.isNoteOn() || msg.isNoteOff())
        {
            midiBuffer.push_back(BufferedMidiMessage(msg, globalSamplePos));
        }
    }
}

void BetterChordStacksAudioProcessor::processBufferedMidi(juce::MidiBuffer &output, int numSamples)
{
    int outputStartSample = currentSampleOffset;
    int outputEndSample = currentSampleOffset + numSamples;

    std::map<int, std::vector<BufferedMidiMessage>> notesByTimestamp;

    for (const auto &buffered : midiBuffer)
    {
        if (buffered.message.isNoteOn() &&
            buffered.samplePosition < outputEndSample + lookaheadSamples)
        {
            notesByTimestamp[buffered.samplePosition].push_back(buffered);
        }
    }

    for (auto &[timestamp, notes] : notesByTimestamp)
    {
        if (notes.size() >= 2)
        {
            detectChord(notes);
        }
    }

    if (hasCurrentChord && hasPendingChord)
    {
        int glideStartTime = pendingChordTimestamp - lookaheadSamples;

        if (currentSampleOffset >= glideStartTime && glidingVoices.empty())
        {
            startGlideTransition();
        }
    }

    processGlides(output, outputStartSample, outputEndSample);

    midiBuffer.erase(
        std::remove_if(midiBuffer.begin(), midiBuffer.end(),
                       [outputEndSample](const BufferedMidiMessage &msg)
                       {
                           return msg.samplePosition < outputEndSample;
                       }),
        midiBuffer.end());
}

void BetterChordStacksAudioProcessor::detectChord(const std::vector<BufferedMidiMessage> &simultaneousNotes)
{
    std::vector<int> notes;
    int timestamp = simultaneousNotes[0].samplePosition;

    for (const auto &buffered : simultaneousNotes)
    {
        notes.push_back(buffered.message.getNoteNumber());
    }

    std::sort(notes.begin(), notes.end());

    if (!hasCurrentChord)
    {
        currentChord = notes;
        hasCurrentChord = true;

        for (int note : currentChord)
        {
            int channel = channelManager.assignChannel();
            if (channel != -1)
            {
                GlidingVoice voice;
                voice.channel = channel;
                voice.startNote = note;
                voice.targetNote = note;
                voice.currentPitch = 0.0f;
                voice.isActive = true;
                voice.noteOnSent = false;
                voice.glideStartSample = timestamp;
                voice.samplePosition = 0;
                voice.totalSamples = 0;
                glidingVoices.push_back(voice);
            }
        }
    }
    else if (!hasPendingChord)
    {
        pendingChord = notes;
        hasPendingChord = true;
        pendingChordTimestamp = timestamp;
    }
    else
    {
        pendingChord = notes;
        pendingChordTimestamp = timestamp;
    }
}

void BetterChordStacksAudioProcessor::startGlideTransition()
{
    if (!hasCurrentChord || !hasPendingChord)
        return;

    int strategyIndex = static_cast<int>(*apvts.getRawParameterValue("strategy"));
    VoiceMapper::Strategy strategy = strategyIndex == 0 ? VoiceMapper::NearestNote : VoiceMapper::Random;

    auto mapping = VoiceMapper::mapNotes(currentChord, pendingChord, strategy);

    for (auto &voice : glidingVoices)
    {
        if (voice.channel != -1)
            channelManager.releaseChannel(voice.channel);
    }
    glidingVoices.clear();

    int glideStartSample = pendingChordTimestamp - lookaheadSamples;

    for (auto &[sourceNote, targetNotes] : mapping)
    {
        for (size_t i = 0; i < targetNotes.size(); ++i)
        {
            int channel = channelManager.assignChannel();
            if (channel == -1)
                continue;

            GlidingVoice voice;
            voice.channel = channel;
            voice.startNote = sourceNote;
            voice.targetNote = targetNotes[i];
            voice.currentPitch = 0.0f;
            voice.samplePosition = 0;
            voice.totalSamples = lookaheadSamples;
            voice.isActive = true;
            voice.noteOnSent = false;
            voice.glideStartSample = glideStartSample;

            glidingVoices.push_back(voice);
        }
    }

    currentChord = pendingChord;
    pendingChord.clear();
    hasPendingChord = false;
}

void BetterChordStacksAudioProcessor::processGlides(juce::MidiBuffer &output, int startSample, int endSample)
{
    for (auto &voice : glidingVoices)
    {
        if (!voice.isActive)
            continue;

        if (!voice.noteOnSent && voice.glideStartSample >= startSample &&
            voice.glideStartSample < endSample)
        {
            int localSample = voice.glideStartSample - startSample;
            output.addEvent(juce::MidiMessage::noteOn(voice.channel, voice.startNote, (juce::uint8)100),
                            localSample);
            voice.noteOnSent = true;
            sendPitchBend(output, voice.channel, 0.0f, localSample);
        }

        if (!voice.noteOnSent)
            continue;

        for (int sample = startSample; sample < endSample; ++sample)
        {
            if (sample < voice.glideStartSample)
                continue;

            int glideElapsed = sample - voice.glideStartSample;

            if (glideElapsed <= voice.totalSamples)
            {
                float progress = voice.totalSamples > 0 ? (float)glideElapsed / (float)voice.totalSamples : 1.0f;

                float targetPitchOffset = (float)(voice.targetNote - voice.startNote);
                voice.currentPitch = targetPitchOffset * progress;

                if ((glideElapsed % 8) == 0 || glideElapsed == voice.totalSamples)
                {
                    sendPitchBend(output, voice.channel, voice.currentPitch, sample - startSample);
                }

                voice.samplePosition = glideElapsed;
            }

            if (glideElapsed == voice.totalSamples)
            {
                int localSample = sample - startSample;

                output.addEvent(juce::MidiMessage::noteOff(voice.channel, voice.startNote, (juce::uint8)0),
                                localSample);

                if (voice.targetNote != voice.startNote)
                {
                    output.addEvent(juce::MidiMessage::noteOn(voice.channel, voice.targetNote, (juce::uint8)100),
                                    localSample);
                }

                sendPitchBend(output, voice.channel, 0.0f, localSample);

                voice.startNote = voice.targetNote;
                voice.totalSamples = 0;
            }
        }
    }
}

void BetterChordStacksAudioProcessor::sendPitchBend(juce::MidiBuffer &buffer, int channel,
                                                    float pitchInSemitones, int sampleOffset)
{
    float pitchBendRange = *apvts.getRawParameterValue("pitchBendRange");
    float normalizedPitch = pitchInSemitones / pitchBendRange;
    normalizedPitch = juce::jlimit(-1.0f, 1.0f, normalizedPitch);

    int pitchBendValue = static_cast<int>((normalizedPitch * 8192.0f) + 8192.0f);
    pitchBendValue = juce::jlimit(0, 16383, pitchBendValue);

    buffer.addEvent(juce::MidiMessage::pitchWheel(channel, pitchBendValue), sampleOffset);
}

bool BetterChordStacksAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *BetterChordStacksAudioProcessor::createEditor()
{
    return new BetterChordStacksAudioProcessorEditor(*this);
}

void BetterChordStacksAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BetterChordStacksAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new BetterChordStacksAudioProcessor();
}