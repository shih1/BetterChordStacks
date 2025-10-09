#include "PluginProcessor.h"
#include "PluginEditor.h"

PitchBendProcessor::PitchBendProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter(bendAmount = new juce::AudioParameterFloat("bendAmount", "Bend Amount",
                                                            juce::NormalisableRange<float>(0.0f, 2.0f, 0.01f),
                                                            1.0f));
    addParameter(bendTime = new juce::AudioParameterFloat("bendTime", "Bend Time",
                                                          juce::NormalisableRange<float>(0.01f, 2.0f, 0.01f),
                                                          0.5f));
    addParameter(bendCurve = new juce::AudioParameterFloat("bendCurve", "Bend Curve",
                                                           juce::NormalisableRange<float>(-2.0f, 2.0f, 0.01f),
                                                           0.0f));

    channelInUse.fill(false);
    channelInUse[0] = true; // Channel 1 is master channel in MPE lower zone
}

PitchBendProcessor::~PitchBendProcessor()
{
}

const juce::String PitchBendProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PitchBendProcessor::acceptsMidi() const
{
    return true;
}

bool PitchBendProcessor::producesMidi() const
{
    return true;
}

bool PitchBendProcessor::isMidiEffect() const
{
    return true;
}

double PitchBendProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PitchBendProcessor::getNumPrograms()
{
    return 1;
}

int PitchBendProcessor::getCurrentProgram()
{
    return 0;
}

void PitchBendProcessor::setCurrentProgram(int index)
{
}

const juce::String PitchBendProcessor::getProgramName(int index)
{
    return {};
}

void PitchBendProcessor::changeProgramName(int index, const juce::String &newName)
{
}

void PitchBendProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentTime = 0.0;

    // Send MPE Configuration Message to set up lower zone with 14 member channels
    // This tells the DAW we're in MPE mode
    // RPN MSB (101) = 0, RPN LSB (100) = 6 for MPE Configuration
    // Data Entry MSB (6) = number of member channels (14)
}

void PitchBendProcessor::releaseResources()
{
}

bool PitchBendProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return true;
}

int PitchBendProcessor::findAvailableMPEChannel()
{
    // MPE lower zone uses channels 2-15 (channel 1 is master)
    for (int ch = 2; ch <= 15; ++ch)
    {
        if (!channelInUse[ch - 1]) // Array is 0-indexed
        {
            return ch;
        }
    }
    return 2; // Fallback to channel 2 if all busy
}

int PitchBendProcessor::calculatePitchBend(double elapsedTime, float amount, float duration, float curve)
{
    if (elapsedTime >= duration)
        return static_cast<int>(amount * 8192.0f);

    float progress = static_cast<float>(elapsedTime / duration);

    // Apply curve
    if (curve != 0.0f)
    {
        if (curve > 0.0f)
            progress = std::pow(progress, 1.0f + curve);
        else
            progress = 1.0f - std::pow(1.0f - progress, 1.0f - curve);
    }

    // Pitch bend range: -8192 to +8191
    return static_cast<int>(progress * amount * 8192.0f);
}

void PitchBendProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    buffer.clear();

    juce::MidiBuffer processedMidi;

    // Send MPE zone configuration at the start (only once per block for efficiency)
    static bool mpeConfigSent = false;
    if (!mpeConfigSent)
    {
        // Send MPE Configuration Message for lower zone with 14 member channels
        auto mpeConfig = juce::MPEMessages::setLowerZone(14, 48, 2);
        for (const auto &msg : mpeConfig)
            processedMidi.addEvent(msg, 0);
        mpeConfigSent = true;
    }

    float amount = bendAmount->get();
    float duration = bendTime->get();
    float curve = bendCurve->get();

    // Process incoming MIDI messages
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        int samplePos = metadata.samplePosition;

        if (message.isNoteOn())
        {
            double noteStartTime = currentTime + (samplePos / currentSampleRate);

            // Find an available MPE channel for this note
            int mpeChannel = findAvailableMPEChannel();
            channelInUse[mpeChannel - 1] = true;

            // Add note to active notes
            NoteInfo note;
            note.noteNumber = message.getNoteNumber();
            note.mpeChannel = mpeChannel;
            note.startTime = noteStartTime;
            note.lastBendValue = 0;
            note.isActive = true;
            activeNotes.push_back(note);

            // Send note on the MPE member channel (not the original channel)
            juce::MidiMessage mpeNoteOn = juce::MidiMessage::noteOn(
                mpeChannel,
                note.noteNumber,
                message.getVelocity());
            processedMidi.addEvent(mpeNoteOn, samplePos);

            // Initialize pitch bend to center for this channel
            juce::MidiMessage initialBend = juce::MidiMessage::pitchWheel(mpeChannel, 8192);
            processedMidi.addEvent(initialBend, samplePos);
        }
        else if (message.isNoteOff())
        {
            // Find the corresponding active note and send note off on its MPE channel
            for (auto &note : activeNotes)
            {
                if (note.noteNumber == message.getNoteNumber() && note.isActive)
                {
                    // Send note off on the MPE channel
                    juce::MidiMessage mpeNoteOff = juce::MidiMessage::noteOff(
                        note.mpeChannel,
                        note.noteNumber,
                        message.getVelocity());
                    processedMidi.addEvent(mpeNoteOff, samplePos);

                    // Mark channel as free
                    channelInUse[note.mpeChannel - 1] = false;
                    note.isActive = false;
                    break;
                }
            }
        }
        else
        {
            // Pass through other messages (but might need channel remapping for CC, aftertouch, etc.)
            processedMidi.addEvent(message, samplePos);
        }
    }

    // Generate pitch bend updates for active notes at reduced rate
    for (int i = 0; i < buffer.getNumSamples(); i += updateRateInSamples)
    {
        double sampleTime = currentTime + (i / currentSampleRate);

        for (auto &note : activeNotes)
        {
            if (!note.isActive)
                continue;

            double elapsedTime = sampleTime - note.startTime;

            if (elapsedTime >= 0.0 && elapsedTime <= duration)
            {
                int bendValue = calculatePitchBend(elapsedTime, amount, duration, curve);

                // Only send if value changed significantly
                if (std::abs(bendValue - note.lastBendValue) > 10)
                {
                    note.lastBendValue = bendValue;

                    // Send pitch bend on this note's MPE member channel
                    juce::MidiMessage bendMsg = juce::MidiMessage::pitchWheel(
                        note.mpeChannel,
                        bendValue + 8192);
                    processedMidi.addEvent(bendMsg, i);
                }
            }
        }
    }

    // Clean up inactive notes
    activeNotes.erase(
        std::remove_if(activeNotes.begin(), activeNotes.end(),
                       [](const NoteInfo &note)
                       { return !note.isActive; }),
        activeNotes.end());

    midiMessages.swapWith(processedMidi);
    currentTime += buffer.getNumSamples() / currentSampleRate;
}

bool PitchBendProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor *PitchBendProcessor::createEditor()
{
    return new PitchBendEditor(*this);
}

void PitchBendProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    juce::MemoryOutputStream stream(destData, true);

    stream.writeFloat(*bendAmount);
    stream.writeFloat(*bendTime);
    stream.writeFloat(*bendCurve);
}

void PitchBendProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);

    *bendAmount = stream.readFloat();
    *bendTime = stream.readFloat();
    *bendCurve = stream.readFloat();
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new PitchBendProcessor();
}