#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

NoiseMu6ixSamplerAudioProcessor::NoiseMu6ixSamplerAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    formatManager.registerBasicFormats();

    macroConfigs[0] = { "Dynamics",   true,  (int)MacroTarget::Dynamics };
    macroConfigs[1] = { "Expression", true,  (int)MacroTarget::Expression };
    macroConfigs[2] = { "Cutoff",     false, (int)MacroTarget::FilterCutoff };
    macroConfigs[3] = { "Reverb",     false, (int)MacroTarget::ReverbMix };
    macroConfigs[4] = { "Attack",     false, (int)MacroTarget::Attack };
    macroConfigs[5] = { "Release",    false, (int)MacroTarget::Release };
}

juce::AudioProcessorValueTreeState::ParameterLayout NoiseMu6ixSamplerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    float defaults[numMacros] = { 0.85f, 0.85f, 1.0f, 0.15f, 0.02f, 0.25f };

    for (int i = 0; i < numMacros; ++i)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            "macro" + juce::String (i),
            "Macro " + juce::String (i + 1),
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
            defaults[i]));
    }
    return { params.begin(), params.end() };
}

bool NoiseMu6ixSamplerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NoiseMu6ixSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 2;
    filter.prepare (spec);
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.reset();

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    for (auto& v : voices)
    {
        v.active = false;
        v.adsr.setSampleRate (sampleRate);
    }
}

float NoiseMu6ixSamplerAudioProcessor::getMacroValueForTarget (MacroTarget target, float defaultValue) const
{
    for (int i = 0; i < numMacros; ++i)
    {
        if (macroConfigs[i].target == (int)target)
        {
            if (auto* p = apvts.getRawParameterValue ("macro" + juce::String (i)))
                return p->load();
        }
    }
    return defaultValue;
}

int NoiseMu6ixSamplerAudioProcessor::findNearestZeroCrossing (const juce::AudioBuffer<float>& buf, int targetSample)
{
    int numSamples = buf.getNumSamples();
    if (numSamples < 4) return 0;

    targetSample = juce::jlimit (1, numSamples - 2, targetSample);
    const float* data = buf.getReadPointer (0);

    int searchRadius = std::min (1500, numSamples / 4);
    for (int offset = 0; offset < searchRadius; ++offset)
    {
        int idx1 = targetSample + offset;
        if (idx1 > 0 && idx1 < numSamples && data[idx1 - 1] <= 0.0f && data[idx1] > 0.0f)
            return idx1;

        int idx2 = targetSample - offset;
        if (idx2 > 0 && idx2 < numSamples && data[idx2 - 1] <= 0.0f && data[idx2] > 0.0f)
            return idx2;
    }
    return targetSample;
}

void NoiseMu6ixSamplerAudioProcessor::autoDetectLoop (std::shared_ptr<SampleZone> zone)
{
    if (!zone || zone->buffer.getNumSamples() < 512) return;
    int len = zone->buffer.getNumSamples();
    int start = findNearestZeroCrossing (zone->buffer, (int)(len * 0.30));
    int end   = findNearestZeroCrossing (zone->buffer, (int)(len * 0.80));
    if (end <= start + 256)
    {
        start = 0;
        end = len - 1;
    }
    zone->loopStart.store (start);
    zone->loopEnd.store (end);
    int maxCf = std::max (64, (end - start) / 4);
    zone->crossfadeSamples.store (std::min (3000, maxCf));
}

int NoiseMu6ixSamplerAudioProcessor::parseNoteFromFilename (const juce::String& name)
{
    static const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flatNames[12] = { "C", "DB", "D", "EB", "E", "F", "GB", "G", "AB", "A", "BB", "B" };

    auto upper = name.toUpperCase();
    for (int oct = -1; oct <= 8; ++oct)
    {
        for (int n = 11; n >= 0; --n)
        {
            juce::String token1 = juce::String (noteNames[n]) + juce::String (oct);
            juce::String token2 = juce::String (flatNames[n]) + juce::String (oct);
            if (upper.contains (token1) || upper.contains (token2))
                return juce::jlimit (0, 127, (oct + 1) * 12 + n);
        }
    }
    return 60;
}

int NoiseMu6ixSamplerAudioProcessor::parseRRFromFilename (const juce::String& name)
{
    auto upper = name.toUpperCase();
    int idx = upper.indexOf ("RR");
    if (idx >= 0 && idx + 2 < upper.length())
    {
        auto numStr = upper.substring (idx + 2, idx + 4).retainCharacters ("0123456789");
        int rr = numStr.getIntValue();
        if (rr >= 1 && rr <= 10) return rr;
    }
    return 1;
}

void NoiseMu6ixSamplerAudioProcessor::loadSampleFile (const juce::File& file, bool autoMapName)
{
    if (!file.existsAsFile()) return;
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (!reader) return;

    auto zone = std::make_shared<SampleZone>();
    zone->name = file.getFileNameWithoutExtension();
    zone->file = file;
    zone->sampleRate = reader->sampleRate;
    zone->buffer.setSize ((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read (&zone->buffer, 0, (int)reader->lengthInSamples, 0, true, true);

    if (autoMapName)
    {
        zone->rootNote = parseNoteFromFilename (zone->name);
        zone->rrGroup  = parseRRFromFilename (zone->name);
        auto lower = zone->name.toLowerCase();
        if (lower.contains ("long") || lower.contains ("sus"))
            zone->isLooping.store (true);
    }

    autoDetectLoop (zone);

    const juce::ScopedLock sl (zoneLock);
    zones.push_back (zone);
}

void NoiseMu6ixSamplerAudioProcessor::loadSampleFolder (const juce::File& folder)
{
    if (!folder.isDirectory()) return;
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.ogg");
    files.sort();
    for (auto& f : files)
        loadSampleFile (f, true);

    autoSpreadKeyRanges();
}

void NoiseMu6ixSamplerAudioProcessor::autoSpreadKeyRanges()
{
    const juce::ScopedLock sl (zoneLock);
    if (zones.empty()) return;

    std::vector<int> uniqueRoots;
    for (auto& z : zones)
        if (std::find (uniqueRoots.begin(), uniqueRoots.end(), z->rootNote) == uniqueRoots.end())
            uniqueRoots.push_back (z->rootNote);

    std::sort (uniqueRoots.begin(), uniqueRoots.end());

    for (auto& z : zones)
    {
        auto it = std::find (uniqueRoots.begin(), uniqueRoots.end(), z->rootNote);
        size_t idx = (size_t)std::distance (uniqueRoots.begin(), it);

        z->lowKey  = (idx == 0) ? 0 : (uniqueRoots[idx - 1] + uniqueRoots[idx]) / 2 + 1;
        z->highKey = (idx + 1 == uniqueRoots.size()) ? 127 : (uniqueRoots[idx] + uniqueRoots[idx + 1]) / 2;
    }
}

void NoiseMu6ixSamplerAudioProcessor::removeZone (int index)
{
    const juce::ScopedLock sl (zoneLock);
    if (index >= 0 && index < (int)zones.size())
        zones.erase (zones.begin() + index);
}

void NoiseMu6ixSamplerAudioProcessor::clearAllZones()
{
    const juce::ScopedLock sl (zoneLock);
    zones.clear();
}

void NoiseMu6ixSamplerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    // Read Macro Parameters
    float dynVal    = getMacroValueForTarget (MacroTarget::Dynamics, 0.85f);
    float exprVal   = getMacroValueForTarget (MacroTarget::Expression, 0.85f);
    float cutNorm   = getMacroValueForTarget (MacroTarget::FilterCutoff, 1.0f);
    float resNorm   = getMacroValueForTarget (MacroTarget::FilterRes, 0.1f);
    float revMix    = getMacroValueForTarget (MacroTarget::ReverbMix, 0.15f);
    float revSize   = getMacroValueForTarget (MacroTarget::ReverbSize, 0.65f);
    float atkNorm   = getMacroValueForTarget (MacroTarget::Attack, 0.02f);
    float decNorm   = getMacroValueForTarget (MacroTarget::Decay, 0.25f);
    float susNorm   = getMacroValueForTarget (MacroTarget::Sustain, 1.0f);
    float relNorm   = getMacroValueForTarget (MacroTarget::Release, 0.25f);
    float pitchNorm = getMacroValueForTarget (MacroTarget::Pitch, 0.5f);
    float panNorm   = getMacroValueForTarget (MacroTarget::Pan, 0.5f);

    juce::ADSR::Parameters adsrParams;
    adsrParams.attack  = 0.002f + atkNorm * atkNorm * 2.5f;
    adsrParams.decay   = 0.01f  + decNorm * decNorm * 2.5f;
    adsrParams.sustain = susNorm;
    adsrParams.release = 0.01f  + relNorm * relNorm * 4.0f;

    float masterGain = dynVal * exprVal * 1.4f;
    float pitchSemitones = (pitchNorm - 0.5f) * 24.0f;
    double globalPitchMult = std::pow (2.0, pitchSemitones / 12.0);
    float panL = std::cos (panNorm * juce::MathConstants<float>::halfPi);
    float panR = std::sin (panNorm * juce::MathConstants<float>::halfPi);

    // Handle MIDI Events
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            int vel  = msg.getVelocity();

            std::vector<std::shared_ptr<SampleZone>> matches;
            {
                const juce::ScopedTryLock sl (zoneLock);
                if (sl.isLocked())
                {
                    for (auto& z : zones)
                    {
                        if (note >= z->lowKey && note <= z->highKey &&
                            vel  >= z->lowVel && vel  <= z->highVel &&
                            z->buffer.getNumSamples() > 0)
                        {
                            matches.push_back (z);
                        }
                    }
                }
            }

            if (!matches.empty())
            {
                // Sort matches by RR Group so Cycle mode is predictable
                std::sort (matches.begin(), matches.end(), [](const auto& a, const auto& b) {
                    return a->rrGroup < b->rrGroup;
                });

                int chosenIdx = 0;
                int count = (int)matches.size();
                if (count > 1)
                {
                    int mode = rrMode.load();
                    if (mode == 0) // Cycle
                    {
                        chosenIdx = (lastRRIndexPerNote[note] + 1) % count;
                    }
                    else if (mode == 1) // No-Repeat Random
                    {
                        std::uniform_int_distribution<int> dist (0, count - 2);
                        int r = dist (rng);
                        chosenIdx = (r >= lastRRIndexPerNote[note]) ? r + 1 : r;
                    }
                    else // True Random
                    {
                        std::uniform_int_distribution<int> dist (0, count - 1);
                        chosenIdx = dist (rng);
                    }
                }
                lastRRIndexPerNote[note] = chosenIdx;
                auto selectedZone = matches[(size_t)chosenIdx];

                // Find free voice or steal oldest
                NM6Voice* targetVoice = &voices[0];
                for (auto& v : voices)
                {
                    if (!v.active) { targetVoice = &v; break; }
                }

                targetVoice->active = true;
                targetVoice->midiNote = note;
                targetVoice->velocity = vel / 127.0f;
                targetVoice->zone = selectedZone;
                targetVoice->playPos = 0.0;
                targetVoice->basePitchRatio = std::pow (2.0, (note - selectedZone->rootNote) / 12.0)
                                              * (selectedZone->sampleRate / currentSampleRate);
                targetVoice->adsr.setSampleRate (currentSampleRate);
                targetVoice->adsr.setParameters (adsrParams);
                targetVoice->adsr.noteOn();
            }
        }
        else if (msg.isNoteOff())
        {
            int note = msg.getNoteNumber();
            for (auto& v : voices)
                if (v.active && v.midiNote == note)
                    v.adsr.noteOff();
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (auto& v : voices)
                v.adsr.noteOff();
        }
    }

    int numSamples = buffer.getNumSamples();
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    auto readInterpolated = [](const juce::AudioBuffer<float>& buf, int ch, double pos) -> float
    {
        int len = buf.getNumSamples();
        if (pos < 0.0 || pos >= len - 1) return 0.0f;
        int i0 = (int)pos;
        float frac = (float)(pos - i0);
        const float* data = buf.getReadPointer (std::min (ch, buf.getNumChannels() - 1));
        return data[i0] + frac * (data[i0 + 1] - data[i0]);
    };

    // Render Voices with Zero-Click Equal-Power Crossfade Looping
    for (auto& v : voices)
    {
        if (!v.active || !v.zone) continue;
        v.adsr.setParameters (adsrParams);

        auto& buf = v.zone->buffer;
        int totalLen = buf.getNumSamples();
        bool looping = v.zone->isLooping.load();
        int lStart   = juce::jlimit (0, std::max (0, totalLen - 2), v.zone->loopStart.load());
        int lEnd     = juce::jlimit (lStart + 1, std::max (1, totalLen - 1), v.zone->loopEnd.load());
        int loopLen  = lEnd - lStart;
        int cf       = looping ? juce::jlimit (0, std::max (0, loopLen / 3), v.zone->crossfadeSamples.load()) : 0;

        double step = v.basePitchRatio * globalPitchMult;

        for (int s = 0; s < numSamples; ++s)
        {
            if (!v.adsr.isActive())
            {
                v.active = false;
                break;
            }

            if (!looping && v.playPos >= totalLen - 1)
            {
                v.active = false;
                break;
            }

            float sampleL = 0.0f, sampleR = 0.0f;

            if (looping && cf > 4 && v.playPos >= (lEnd - cf) && v.playPos < lEnd)
            {
                double intoFade = v.playPos - (lEnd - cf);
                float t = (float)(intoFade / (double)cf);
                float gOut = std::cos (t * juce::MathConstants<float>::halfPi);
                float gIn  = std::sin (t * juce::MathConstants<float>::halfPi);

                double fadeInPos = lStart + intoFade;
                sampleL = readInterpolated (buf, 0, v.playPos) * gOut + readInterpolated (buf, 0, fadeInPos) * gIn;
                sampleR = readInterpolated (buf, 1, v.playPos) * gOut + readInterpolated (buf, 1, fadeInPos) * gIn;
            }
            else
            {
                sampleL = readInterpolated (buf, 0, v.playPos);
                sampleR = readInterpolated (buf, 1, v.playPos);
            }

            float env = v.adsr.getNextSample() * v.velocity * masterGain;
            outL[s] += sampleL * env * panL;
            outR[s] += sampleR * env * panR;

            v.playPos += step;

            if (looping && loopLen > 16 && v.playPos >= lEnd)
            {
                v.playPos = (double)(lStart + cf) + (v.playPos - (double)lEnd);
            }
        }
    }

    // Master Filter
    float cutoffHz = 80.0f * std::pow (250.0f, cutNorm); // 80 Hz to 20 kHz
    filter.setCutoffFrequency (juce::jlimit (80.0f, 19900.0f, cutoffHz));
    filter.setResonance (0.707f + resNorm * 4.0f);
    for (int s = 0; s < numSamples; ++s)
    {
        outL[s] = filter.processSample (0, outL[s]);
        outR[s] = filter.processSample (1, outR[s]);
    }

    // Master Reverb
    juce::Reverb::Parameters revParams;
    revParams.roomSize = revSize;
    revParams.damping  = 0.45f;
    revParams.wetLevel = revMix * 0.6f;
    revParams.dryLevel = 1.0f - (revMix * 0.25f);
    revParams.width    = 1.0f;
    reverb.setParameters (revParams);
    reverb.processStereo (outL, outR, numSamples);
}

void NoiseMu6ixSamplerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto root = std::make_unique<juce::XmlElement> ("NoiseMu6ixState");
    root->setAttribute ("rrMode", rrMode.load());

    if (auto apvtsXml = apvts.copyState().createXml())
        root->addChildElement (apvtsXml.release());

    auto* macrosXml = root->createNewChildElement ("Macros");
    for (int i = 0; i < numMacros; ++i)
    {
        auto* m = macrosXml->createNewChildElement ("Macro");
        m->setAttribute ("name", macroConfigs[i].name);
        m->setAttribute ("isSlider", macroConfigs[i].isSlider);
        m->setAttribute ("target", macroConfigs[i].target);
    }

    auto* zonesXml = root->createNewChildElement ("Zones");
    const juce::ScopedLock sl (zoneLock);
    for (auto& z : zones)
    {
        auto* zx = zonesXml->createNewChildElement ("Zone");
        zx->setAttribute ("path", z->file.getFullPathName());
        zx->setAttribute ("root", z->rootNote);
        zx->setAttribute ("lowK", z->lowKey);
        zx->setAttribute ("highK", z->highKey);
        zx->setAttribute ("lowV", z->lowVel);
        zx->setAttribute ("highV", z->highVel);
        zx->setAttribute ("rr", z->rrGroup);
        zx->setAttribute ("loop", z->isLooping.load());
        zx->setAttribute ("lStart", z->loopStart.load());
        zx->setAttribute ("lEnd", z->loopEnd.load());
        zx->setAttribute ("cf", z->crossfadeSamples.load());
    }

    copyXmlToBinary (*root, destData);
}

void NoiseMu6ixSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto root = getXmlFromBinary (data, sizeInBytes);
    if (!root || !root->hasTagName ("NoiseMu6ixState")) return;

    rrMode.store (root->getIntAttribute ("rrMode", 1));

    if (auto* apvtsXml = root->getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*apvtsXml));

    if (auto* macrosXml = root->getChildByName ("Macros"))
    {
        int i = 0;
        for (auto* m : macrosXml->getChildIterator())
        {
            if (i >= numMacros) break;
            macroConfigs[i].name     = m->getStringAttribute ("name", "Macro");
            macroConfigs[i].isSlider = m->getBoolAttribute ("isSlider", false);
            macroConfigs[i].target   = m->getIntAttribute ("target", 1);
            ++i;
        }
    }

    if (auto* zonesXml = root->getChildByName ("Zones"))
    {
        clearAllZones();
        for (auto* zx : zonesXml->getChildIterator())
        {
            juce::File f (zx->getStringAttribute ("path"));
            if (f.existsAsFile())
            {
                loadSampleFile (f, false);
                const juce::ScopedLock sl (zoneLock);
                if (!zones.empty())
                {
                    auto& z = zones.back();
                    z->rootNote = zx->getIntAttribute ("root", 60);
                    z->lowKey   = zx->getIntAttribute ("lowK", 0);
                    z->highKey  = zx->getIntAttribute ("highK", 127);
                    z->lowVel   = zx->getIntAttribute ("lowV", 0);
                    z->highVel  = zx->getIntAttribute ("highV", 127);
                    z->rrGroup  = zx->getIntAttribute ("rr", 1);
                    z->isLooping.store (zx->getBoolAttribute ("loop", false));
                    z->loopStart.store (zx->getIntAttribute ("lStart", 0));
                    z->loopEnd.store (zx->getIntAttribute ("lEnd", 0));
                    z->crossfadeSamples.store (zx->getIntAttribute ("cf", 2048));
                }
            }
        }
    }
}

juce::AudioProcessorEditor* NoiseMu6ixSamplerAudioProcessor::createEditor()
{
    return new NoiseMu6ixSamplerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoiseMu6ixSamplerAudioProcessor();
}
