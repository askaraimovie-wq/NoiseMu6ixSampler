#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

NoiseMu6ixSamplerAudioProcessor::NoiseMu6ixSamplerAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    formatManager.registerBasicFormats();

    SampleGroup g1;
    g1.name = "Group 1 (RR1)";
    g1.rrGroup = 1;
    groups.push_back (g1);

    macroConfigs[0] = { "Dynamics",   true,  true,  (int)MacroTarget::Dynamics };
    macroConfigs[1] = { "Expression", true,  true,  (int)MacroTarget::Expression };
    macroConfigs[2] = { "Cutoff",     false, true,  (int)MacroTarget::FilterCutoff };
    macroConfigs[3] = { "Reverb",     false, true,  (int)MacroTarget::ReverbMix };
    macroConfigs[4] = { "Delay",      false, true,  (int)MacroTarget::DelayMix };
    macroConfigs[5] = { "Release",    false, true,  (int)MacroTarget::Release };
    macroConfigs[6] = { "Chorus",     false, false, (int)MacroTarget::ChorusMix };
    macroConfigs[7] = { "Drive",      false, false, (int)MacroTarget::Drive };
}

juce::AudioProcessorValueTreeState::ParameterLayout NoiseMu6ixSamplerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    float defaults[numMacros] = { 0.85f, 0.85f, 1.0f, 0.15f, 0.0f, 0.25f, 0.0f, 0.0f };

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
    filter.reset();

    chorus.prepare (spec);
    chorus.reset();

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    eqLowL.prepare (monoSpec);  eqLowR.prepare (monoSpec);
    eqMidL.prepare (monoSpec);  eqMidR.prepare (monoSpec);
    eqHighL.prepare (monoSpec); eqHighR.prepare (monoSpec);

    int maxDelaySamples = (int)(sampleRate * 2.0) + 16;
    delayBuffer.setSize (2, maxDelaySamples);
    delayBuffer.clear();
    delayWritePos = 0;

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
        if (macroConfigs[i].visible && macroConfigs[i].target == (int)target)
        {
            if (auto* p = apvts.getRawParameterValue ("macro" + juce::String (i)))
                return p->load();
        }
    }
    return defaultValue;
}

int NoiseMu6ixSamplerAudioProcessor::addGroup (const juce::String& name)
{
    const juce::ScopedLock sl (dataLock);
    SampleGroup g;
    int idx = (int)groups.size() + 1;
    g.name = name.isEmpty() ? ("Group " + juce::String (idx)) : name;
    g.rrGroup = juce::jlimit (1, 10, ((idx - 1) % 10) + 1);
    groups.push_back (g);
    return (int)groups.size() - 1;
}

void NoiseMu6ixSamplerAudioProcessor::removeGroup (int groupIdx)
{
    const juce::ScopedLock sl (dataLock);
    if (groups.size() <= 1 || groupIdx < 0 || groupIdx >= (int)groups.size())
        return;

    zones.erase (std::remove_if (zones.begin(), zones.end(),
                 [groupIdx](const std::shared_ptr<SampleZone>& z) { return z->groupIndex == groupIdx; }),
                 zones.end());

    for (auto& z : zones)
        if (z->groupIndex > groupIdx)
            z->groupIndex -= 1;

    groups.erase (groups.begin() + groupIdx);
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
    int sStart = zone->sampleStart.load();
    int sEnd   = zone->sampleEnd.load();
    if (sEnd <= sStart + 256) { sStart = 0; sEnd = zone->buffer.getNumSamples() - 1; }

    int len = sEnd - sStart;
    int start = findNearestZeroCrossing (zone->buffer, sStart + (int)(len * 0.30));
    int end   = findNearestZeroCrossing (zone->buffer, sStart + (int)(len * 0.80));
    if (end <= start + 256) { start = sStart; end = sEnd; }
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
    return -1;
}

void NoiseMu6ixSamplerAudioProcessor::loadSampleFile (const juce::File& file, int targetGroupIdx, bool autoMapName, int dropNote)
{
    if (!file.existsAsFile()) return;
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (!reader) return;

    auto zone = std::make_shared<SampleZone>();
    zone->name = file.getFileNameWithoutExtension();
    zone->file = file;
    zone->sampleRate = reader->sampleRate;
    int totalLen = (int)reader->lengthInSamples;
    zone->buffer.setSize ((int)reader->numChannels, totalLen);
    reader->read (&zone->buffer, 0, totalLen, 0, true, true);

    zone->sampleStart.store (0);
    zone->sampleEnd.store (std::max (0, totalLen - 1));

    const juce::ScopedLock sl (dataLock);
    int gIdx = juce::jlimit (0, std::max (0, (int)groups.size() - 1), targetGroupIdx);

    if (dropNote >= 0)
    {
        zone->rootNote = juce::jlimit (0, 127, dropNote);
        zone->lowKey   = juce::jlimit (0, 127, dropNote - 2);
        zone->highKey  = juce::jlimit (0, 127, dropNote + 2);
    }
    else if (autoMapName)
    {
        int parsedNote = parseNoteFromFilename (zone->name);
        zone->rootNote = parsedNote;
        zone->lowKey   = juce::jlimit (0, 127, parsedNote - 6);
        zone->highKey  = juce::jlimit (0, 127, parsedNote + 6);

        int parsedRR = parseRRFromFilename (zone->name);
        if (parsedRR >= 1)
        {
            while ((int)groups.size() < parsedRR)
            {
                SampleGroup ng;
                ng.name = "Group " + juce::String ((int)groups.size() + 1) + " (RR" + juce::String ((int)groups.size() + 1) + ")";
                ng.rrGroup = (int)groups.size() + 1;
                groups.push_back (ng);
            }
            gIdx = parsedRR - 1;
        }

        auto lower = zone->name.toLowerCase();
        if (lower.contains ("long") || lower.contains ("sus"))
            zone->isLooping.store (true);
    }

    zone->groupIndex = gIdx;
    autoDetectLoop (zone);
    zones.push_back (zone);
}

void NoiseMu6ixSamplerAudioProcessor::loadSampleFolder (const juce::File& folder, int targetGroupIdx)
{
    if (!folder.isDirectory()) return;
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.ogg");
    files.sort();
    for (auto& f : files)
        loadSampleFile (f, targetGroupIdx, true, -1);
    autoSpreadKeyRanges();
}

void NoiseMu6ixSamplerAudioProcessor::autoSpreadKeyRanges()
{
    const juce::ScopedLock sl (dataLock);
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
    const juce::ScopedLock sl (dataLock);
    if (index >= 0 && index < (int)zones.size())
        zones.erase (zones.begin() + index);
}

void NoiseMu6ixSamplerAudioProcessor::clearAllZones()
{
    const juce::ScopedLock sl (dataLock);
    zones.clear();
}

void NoiseMu6ixSamplerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    float dynVal    = getMacroValueForTarget (MacroTarget::Dynamics, 0.85f);
    float exprVal   = getMacroValueForTarget (MacroTarget::Expression, 0.85f);
    float cutNorm   = getMacroValueForTarget (MacroTarget::FilterCutoff, fx.filterCutoff.load());
    float resNorm   = getMacroValueForTarget (MacroTarget::FilterRes, fx.filterRes.load());
    float revMix    = getMacroValueForTarget (MacroTarget::ReverbMix, fx.reverbMix.load());
    float revSize   = getMacroValueForTarget (MacroTarget::ReverbSize, fx.reverbSize.load());
    float dlyMix    = getMacroValueForTarget (MacroTarget::DelayMix, fx.delayMix.load());
    float choMix    = getMacroValueForTarget (MacroTarget::ChorusMix, fx.chorusMix.load());
    float drvVal    = getMacroValueForTarget (MacroTarget::Drive, fx.driveAmount.load());
    float atkNorm   = getMacroValueForTarget (MacroTarget::Attack, fx.attack.load());
    float decNorm   = getMacroValueForTarget (MacroTarget::Decay, fx.decay.load());
    float susNorm   = getMacroValueForTarget (MacroTarget::Sustain, fx.sustain.load());
    float relNorm   = getMacroValueForTarget (MacroTarget::Release, fx.release.load());
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
    float masterPanL = std::cos (panNorm * juce::MathConstants<float>::halfPi);
    float masterPanR = std::sin (panNorm * juce::MathConstants<float>::halfPi);

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            int vel  = msg.getVelocity();

            std::vector<std::shared_ptr<SampleZone>> matches;
            std::vector<SampleGroup> groupSnapshot;
            {
                const juce::ScopedTryLock sl (dataLock);
                if (sl.isLocked())
                {
                    groupSnapshot = groups;
                    bool anySolo = false;
                    for (auto& g : groups) if (g.solo) { anySolo = true; break; }

                    for (auto& z : zones)
                    {
                        if (z->groupIndex < 0 || z->groupIndex >= (int)groups.size()) continue;
                        const auto& g = groups[(size_t)z->groupIndex];
                        if (g.mute) continue;
                        if (anySolo && !g.solo) continue;

                        if (note >= z->lowKey && note <= z->highKey &&
                            vel  >= z->lowVel && vel  <= z->highVel &&
                            z->buffer.getNumSamples() > 0)
                            matches.push_back (z);
                    }
                }
            }

            if (!matches.empty())
            {
                std::sort (matches.begin(), matches.end(), [&](const auto& a, const auto& b) {
                    int rrA = (a->groupIndex < (int)groupSnapshot.size()) ? groupSnapshot[(size_t)a->groupIndex].rrGroup : 1;
                    int rrB = (b->groupIndex < (int)groupSnapshot.size()) ? groupSnapshot[(size_t)b->groupIndex].rrGroup : 1;
                    return rrA < rrB;
                });

                int chosenIdx = 0;
                int count = (int)matches.size();
                if (count > 1)
                {
                    int mode = rrMode.load();
                    if (mode == 0) chosenIdx = (lastRRIndexPerNote[note] + 1) % count;
                    else if (mode == 1)
                    {
                        std::uniform_int_distribution<int> dist (0, count - 2);
                        int r = dist (rng);
                        chosenIdx = (r >= lastRRIndexPerNote[note]) ? r + 1 : r;
                    }
                    else
                    {
                        std::uniform_int_distribution<int> dist (0, count - 1);
                        chosenIdx = dist (rng);
                    }
                }
                lastRRIndexPerNote[note] = chosenIdx;
                auto selectedZone = matches[(size_t)chosenIdx];
                SampleGroup grp;
                if (selectedZone->groupIndex >= 0 && selectedZone->groupIndex < (int)groupSnapshot.size())
                    grp = groupSnapshot[(size_t)selectedZone->groupIndex];

                NM6Voice* targetVoice = &voices[0];
                for (auto& v : voices)
                    if (!v.active) { targetVoice = &v; break; }

                float totalDb = selectedZone->volumeDb + grp.volumeDb;
                float totalPan = juce::jlimit (-1.0f, 1.0f, selectedZone->pan + grp.pan);
                float panAngle = (totalPan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
                float totalTune = selectedZone->tuneSemi + grp.tuneSemi;

                targetVoice->active = true;
                targetVoice->midiNote = note;
                targetVoice->velocity = vel / 127.0f;
                targetVoice->zone = selectedZone;
                targetVoice->playPos = (double)selectedZone->sampleStart.load();
                targetVoice->voiceGain = juce::Decibels::decibelsToGain (totalDb);
                targetVoice->voicePanL = std::cos (panAngle);
                targetVoice->voicePanR = std::sin (panAngle);
                targetVoice->basePitchRatio = std::pow (2.0, ((note - selectedZone->rootNote) + totalTune) / 12.0)
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
            for (auto& v : voices) v.adsr.noteOff();
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

    int activeCnt = 0;
    for (auto& v : voices)
    {
        if (!v.active || !v.zone) continue;
        ++activeCnt;
        v.adsr.setParameters (adsrParams);

        auto& buf = v.zone->buffer;
        int totalLen = buf.getNumSamples();
        int sEnd     = juce::jlimit (1, std::max (1, totalLen - 1), v.zone->sampleEnd.load());
        bool looping = v.zone->isLooping.load();
        int lStart   = juce::jlimit (0, std::max (0, sEnd - 2), v.zone->loopStart.load());
        int lEnd     = juce::jlimit (lStart + 1, sEnd, v.zone->loopEnd.load());
        int loopLen  = lEnd - lStart;
        int cf       = looping ? juce::jlimit (0, std::max (0, loopLen / 3), v.zone->crossfadeSamples.load()) : 0;

        double step = v.basePitchRatio * globalPitchMult;

        for (int s = 0; s < numSamples; ++s)
        {
            if (!v.adsr.isActive()) { v.active = false; break; }
            if (!looping && v.playPos >= sEnd) { v.active = false; break; }

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

            float env = v.adsr.getNextSample() * v.velocity * v.voiceGain * masterGain;
            outL[s] += sampleL * env * v.voicePanL * masterPanL;
            outR[s] += sampleR * env * v.voicePanR * masterPanR;

            v.playPos += step;
            if (looping && loopLen > 16 && v.playPos >= lEnd)
                v.playPos = (double)(lStart + cf) + (v.playPos - (double)lEnd);
        }
    }
    activeVoicesCount.store (activeCnt);

    // 1. 3-Band EQ
    if (fx.eqOn.load())
    {
        auto lowC  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (currentSampleRate, 120.0f,  0.707f, juce::Decibels::decibelsToGain (fx.eqLowDb.load()));
        auto midC  = juce::dsp::IIR::Coefficients<float>::makePeakFilter(currentSampleRate, 1000.0f, 0.9f,   juce::Decibels::decibelsToGain (fx.eqMidDb.load()));
        auto highC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (currentSampleRate, 5500.0f, 0.707f, juce::Decibels::decibelsToGain (fx.eqHighDb.load()));
        eqLowL.coefficients = lowC;   eqLowR.coefficients = lowC;
        eqMidL.coefficients = midC;   eqMidR.coefficients = midC;
        eqHighL.coefficients = highC; eqHighR.coefficients = highC;

        for (int s = 0; s < numSamples; ++s)
        {
            outL[s] = eqHighL.processSample (eqMidL.processSample (eqLowL.processSample (outL[s])));
            outR[s] = eqHighR.processSample (eqMidR.processSample (eqLowR.processSample (outR[s])));
        }
    }

    // 2. Drive / Saturation
    if (fx.driveOn.load() && drvVal > 0.001f)
    {
        float gain = 1.0f + drvVal * 8.0f;
        float norm = 1.0f / std::tanh (gain);
        for (int s = 0; s < numSamples; ++s)
        {
            outL[s] = std::tanh (outL[s] * gain) * norm;
            outR[s] = std::tanh (outR[s] * gain) * norm;
        }
    }

    // 3. Multi-Mode Filter
    if (fx.filterOn.load())
    {
        int ft = fx.filterType.load();
        if (ft == 0)      filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        else if (ft == 1) filter.setType (juce::dsp::StateVariableTPTFilterType::highpass);
        else              filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);

        float cutoffHz = 80.0f * std::pow (250.0f, cutNorm);
        filter.setCutoffFrequency (juce::jlimit (80.0f, 19900.0f, cutoffHz));
        filter.setResonance (0.707f + resNorm * 4.5f);
        for (int s = 0; s < numSamples; ++s)
        {
            outL[s] = filter.processSample (0, outL[s]);
            outR[s] = filter.processSample (1, outR[s]);
        }
    }

    // 4. Chorus
    if (fx.chorusOn.load() && choMix > 0.001f)
    {
        chorus.setRate (fx.chorusRate.load());
        chorus.setDepth (fx.chorusDepth.load());
        chorus.setMix (choMix);
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chorus.process (ctx);
    }

    // 5. Stereo Delay
    if (fx.delayOn.load() && dlyMix > 0.001f && delayBuffer.getNumSamples() > 16)
    {
        int bufLen = delayBuffer.getNumSamples();
        int delaySampsL = juce::jlimit (1, bufLen - 2, (int)(fx.delayTimeMs.load() * 0.001 * currentSampleRate));
        int delaySampsR = juce::jlimit (1, bufLen - 2, (int)(fx.delayTimeMs.load() * 0.00125 * currentSampleRate));
        float fb = juce::jlimit (0.0f, 0.88f, fx.delayFeedback.load());
        float* dL = delayBuffer.getWritePointer (0);
        float* dR = delayBuffer.getWritePointer (1);

        for (int s = 0; s < numSamples; ++s)
        {
            int rL = (delayWritePos - delaySampsL + bufLen) % bufLen;
            int rR = (delayWritePos - delaySampsR + bufLen) % bufLen;
            float tapL = dL[rL];
            float tapR = dR[rR];

            dL[delayWritePos] = outL[s] + tapR * fb;
            dR[delayWritePos] = outR[s] + tapL * fb;

            outL[s] = outL[s] * (1.0f - dlyMix * 0.4f) + tapL * dlyMix;
            outR[s] = outR[s] * (1.0f - dlyMix * 0.4f) + tapR * dlyMix;

            delayWritePos = (delayWritePos + 1) % bufLen;
        }
    }

    // 6. Reverb
    if (fx.reverbOn.load() && revMix > 0.001f)
    {
        juce::Reverb::Parameters revParams;
        revParams.roomSize = revSize;
        revParams.damping  = fx.reverbDamp.load();
        revParams.wetLevel = revMix * 0.65f;
        revParams.dryLevel = 1.0f - (revMix * 0.25f);
        revParams.width    = 1.0f;
        reverb.setParameters (revParams);
        reverb.processStereo (outL, outR, numSamples);
    }
}

std::unique_ptr<juce::XmlElement> NoiseMu6ixSamplerAudioProcessor::buildStateXml (bool forMonolith)
{
    auto root = std::make_unique<juce::XmlElement> ("NoiseMu6ixStateV3");
    root->setAttribute ("rrMode", rrMode.load());

    if (auto apvtsXml = apvts.copyState().createXml())
        root->addChildElement (apvtsXml.release());

    // FX Settings
    auto* fxXml = root->createNewChildElement ("FX");
    fxXml->setAttribute ("eqOn", fx.eqOn.load());
    fxXml->setAttribute ("eqLow", fx.eqLowDb.load());
    fxXml->setAttribute ("eqMid", fx.eqMidDb.load());
    fxXml->setAttribute ("eqHigh", fx.eqHighDb.load());
    fxXml->setAttribute ("fltOn", fx.filterOn.load());
    fxXml->setAttribute ("fltType", fx.filterType.load());
    fxXml->setAttribute ("fltCut", fx.filterCutoff.load());
    fxXml->setAttribute ("fltRes", fx.filterRes.load());
    fxXml->setAttribute ("drvOn", fx.driveOn.load());
    fxXml->setAttribute ("drvAmt", fx.driveAmount.load());
    fxXml->setAttribute ("choOn", fx.chorusOn.load());
    fxXml->setAttribute ("choRate", fx.chorusRate.load());
    fxXml->setAttribute ("choDepth", fx.chorusDepth.load());
    fxXml->setAttribute ("choMix", fx.chorusMix.load());
    fxXml->setAttribute ("dlyOn", fx.delayOn.load());
    fxXml->setAttribute ("dlyTime", fx.delayTimeMs.load());
    fxXml->setAttribute ("dlyFb", fx.delayFeedback.load());
    fxXml->setAttribute ("dlyMix", fx.delayMix.load());
    fxXml->setAttribute ("revOn", fx.reverbOn.load());
    fxXml->setAttribute ("revSize", fx.reverbSize.load());
    fxXml->setAttribute ("revDamp", fx.reverbDamp.load());
    fxXml->setAttribute ("revMix", fx.reverbMix.load());
    fxXml->setAttribute ("atk", fx.attack.load());
    fxXml->setAttribute ("dec", fx.decay.load());
    fxXml->setAttribute ("sus", fx.sustain.load());
    fxXml->setAttribute ("rel", fx.release.load());

    auto* macrosXml = root->createNewChildElement ("Macros");
    for (int i = 0; i < numMacros; ++i)
    {
        auto* m = macrosXml->createNewChildElement ("Macro");
        m->setAttribute ("name", macroConfigs[i].name);
        m->setAttribute ("isSlider", macroConfigs[i].isSlider);
        m->setAttribute ("visible", macroConfigs[i].visible);
        m->setAttribute ("target", macroConfigs[i].target);
    }

    const juce::ScopedLock sl (dataLock);
    auto* groupsXml = root->createNewChildElement ("Groups");
    for (auto& g : groups)
    {
        auto* gx = groupsXml->createNewChildElement ("Group");
        gx->setAttribute ("name", g.name);
        gx->setAttribute ("rr", g.rrGroup);
        gx->setAttribute ("vol", g.volumeDb);
        gx->setAttribute ("pan", g.pan);
        gx->setAttribute ("tune", g.tuneSemi);
        gx->setAttribute ("solo", g.solo);
        gx->setAttribute ("mute", g.mute);
    }

    auto* zonesXml = root->createNewChildElement ("Zones");
    for (size_t i = 0; i < zones.size(); ++i)
    {
        auto& z = zones[i];
        auto* zx = zonesXml->createNewChildElement ("Zone");
        zx->setAttribute ("name", z->name);
        if (forMonolith)
            zx->setAttribute ("entry", "samples/s_" + juce::String ((int)i) + ".flac");
        else
            zx->setAttribute ("path", z->file.getFullPathName());

        zx->setAttribute ("grp", z->groupIndex);
        zx->setAttribute ("root", z->rootNote);
        zx->setAttribute ("lowK", z->lowKey);
        zx->setAttribute ("highK", z->highKey);
        zx->setAttribute ("lowV", z->lowVel);
        zx->setAttribute ("highV", z->highVel);
        zx->setAttribute ("vol", z->volumeDb);
        zx->setAttribute ("pan", z->pan);
        zx->setAttribute ("tune", z->tuneSemi);
        zx->setAttribute ("sStart", z->sampleStart.load());
        zx->setAttribute ("sEnd", z->sampleEnd.load());
        zx->setAttribute ("loop", z->isLooping.load());
        zx->setAttribute ("lStart", z->loopStart.load());
        zx->setAttribute ("lEnd", z->loopEnd.load());
        zx->setAttribute ("cf", z->crossfadeSamples.load());
    }
    return root;
}

void NoiseMu6ixSamplerAudioProcessor::applyStateXml (const juce::XmlElement& root, juce::ZipFile* monolithZip)
{
    rrMode.store (root.getIntAttribute ("rrMode", 1));

    if (auto* apvtsXml = root.getChildByName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*apvtsXml));

    if (auto* fxXml = root.getChildByName ("FX"))
    {
        fx.eqOn.store         (fxXml->getBoolAttribute ("eqOn", false));
        fx.eqLowDb.store      ((float)fxXml->getDoubleAttribute ("eqLow", 0.0));
        fx.eqMidDb.store      ((float)fxXml->getDoubleAttribute ("eqMid", 0.0));
        fx.eqHighDb.store     ((float)fxXml->getDoubleAttribute ("eqHigh", 0.0));
        fx.filterOn.store     (fxXml->getBoolAttribute ("fltOn", true));
        fx.filterType.store   (fxXml->getIntAttribute ("fltType", 0));
        fx.filterCutoff.store ((float)fxXml->getDoubleAttribute ("fltCut", 1.0));
        fx.filterRes.store    ((float)fxXml->getDoubleAttribute ("fltRes", 0.1));
        fx.driveOn.store      (fxXml->getBoolAttribute ("drvOn", false));
        fx.driveAmount.store  ((float)fxXml->getDoubleAttribute ("drvAmt", 0.0));
        fx.chorusOn.store     (fxXml->getBoolAttribute ("choOn", false));
        fx.chorusRate.store   ((float)fxXml->getDoubleAttribute ("choRate", 1.2));
        fx.chorusDepth.store  ((float)fxXml->getDoubleAttribute ("choDepth", 0.25));
        fx.chorusMix.store    ((float)fxXml->getDoubleAttribute ("choMix", 0.35));
        fx.delayOn.store      (fxXml->getBoolAttribute ("dlyOn", false));
        fx.delayTimeMs.store  ((float)fxXml->getDoubleAttribute ("dlyTime", 320.0));
        fx.delayFeedback.store((float)fxXml->getDoubleAttribute ("dlyFb", 0.35));
        fx.delayMix.store     ((float)fxXml->getDoubleAttribute ("dlyMix", 0.25));
        fx.reverbOn.store     (fxXml->getBoolAttribute ("revOn", true));
        fx.reverbSize.store   ((float)fxXml->getDoubleAttribute ("revSize", 0.65));
        fx.reverbDamp.store   ((float)fxXml->getDoubleAttribute ("revDamp", 0.45));
        fx.reverbMix.store    ((float)fxXml->getDoubleAttribute ("revMix", 0.15));
        fx.attack.store       ((float)fxXml->getDoubleAttribute ("atk", 0.02));
        fx.decay.store        ((float)fxXml->getDoubleAttribute ("dec", 0.25));
        fx.sustain.store      ((float)fxXml->getDoubleAttribute ("sus", 1.0));
        fx.release.store      ((float)fxXml->getDoubleAttribute ("rel", 0.25));
    }

    if (auto* macrosXml = root.getChildByName ("Macros"))
    {
        int i = 0;
        for (auto* m : macrosXml->getChildIterator())
        {
            if (i >= numMacros) break;
            macroConfigs[i].name     = m->getStringAttribute ("name", "Macro");
            macroConfigs[i].isSlider = m->getBoolAttribute ("isSlider", false);
            macroConfigs[i].visible  = m->getBoolAttribute ("visible", true);
            macroConfigs[i].target   = m->getIntAttribute ("target", 1);
            ++i;
        }
    }

    if (auto* groupsXml = root.getChildByName ("Groups"))
    {
        const juce::ScopedLock sl (dataLock);
        groups.clear();
        for (auto* gx : groupsXml->getChildIterator())
        {
            SampleGroup g;
            g.name     = gx->getStringAttribute ("name", "Group");
            g.rrGroup  = gx->getIntAttribute ("rr", 1);
            g.volumeDb = (float)gx->getDoubleAttribute ("vol", 0.0);
            g.pan      = (float)gx->getDoubleAttribute ("pan", 0.0);
            g.tuneSemi = (float)gx->getDoubleAttribute ("tune", 0.0);
            g.solo     = gx->getBoolAttribute ("solo", false);
            g.mute     = gx->getBoolAttribute ("mute", false);
            groups.push_back (g);
        }
        if (groups.empty()) groups.push_back (SampleGroup{});
    }

    if (auto* zonesXml = root.getChildByName ("Zones"))
    {
        clearAllZones();
        for (auto* zx : zonesXml->getChildIterator())
        {
            std::shared_ptr<SampleZone> loadedZone;

            if (monolithZip != nullptr && zx->hasAttribute ("entry"))
            {
                auto entryName = zx->getStringAttribute ("entry");
                int idx = monolithZip->getIndexOfFileName (entryName);
                if (idx >= 0)
                {
                    std::unique_ptr<juce::InputStream> zipStream (monolithZip->createStreamForEntry (idx));
                    if (zipStream)
                    {
                        juce::MemoryBlock mb;
                        zipStream->readIntoMemoryBlock (mb);
                        std::unique_ptr<juce::AudioFormatReader> reader (
                            formatManager.createReaderFor (std::make_unique<juce::MemoryInputStream> (mb, true)));
                        if (reader)
                        {
                            loadedZone = std::make_shared<SampleZone>();
                            loadedZone->name = zx->getStringAttribute ("name", "Sample");
                            loadedZone->sampleRate = reader->sampleRate;
                            loadedZone->buffer.setSize ((int)reader->numChannels, (int)reader->lengthInSamples);
                            reader->read (&loadedZone->buffer, 0, (int)reader->lengthInSamples, 0, true, true);
                        }
                    }
                }
            }
            else if (zx->hasAttribute ("path"))
            {
                juce::File f (zx->getStringAttribute ("path"));
                if (f.existsAsFile())
                {
                    loadSampleFile (f, zx->getIntAttribute ("grp", 0), false, -1);
                    const juce::ScopedLock sl (dataLock);
                    if (!zones.empty()) loadedZone = zones.back();
                }
            }

            if (loadedZone)
            {
                loadedZone->groupIndex = zx->getIntAttribute ("grp", 0);
                loadedZone->rootNote   = zx->getIntAttribute ("root", 60);
                loadedZone->lowKey     = zx->getIntAttribute ("lowK", 0);
                loadedZone->highKey    = zx->getIntAttribute ("highK", 127);
                loadedZone->lowVel     = zx->getIntAttribute ("lowV", 0);
                loadedZone->highVel    = zx->getIntAttribute ("highV", 127);
                loadedZone->volumeDb   = (float)zx->getDoubleAttribute ("vol", 0.0);
                loadedZone->pan        = (float)zx->getDoubleAttribute ("pan", 0.0);
                loadedZone->tuneSemi   = (float)zx->getDoubleAttribute ("tune", 0.0);
                loadedZone->sampleStart.store (zx->getIntAttribute ("sStart", 0));
                loadedZone->sampleEnd.store (zx->getIntAttribute ("sEnd", std::max (0, loadedZone->buffer.getNumSamples() - 1)));
                loadedZone->isLooping.store (zx->getBoolAttribute ("loop", false));
                loadedZone->loopStart.store (zx->getIntAttribute ("lStart", 0));
                loadedZone->loopEnd.store (zx->getIntAttribute ("lEnd", 0));
                loadedZone->crossfadeSamples.store (zx->getIntAttribute ("cf", 2048));

                if (monolithZip != nullptr)
                {
                    const juce::ScopedLock sl (dataLock);
                    zones.push_back (loadedZone);
                }
            }
        }
    }
}

bool NoiseMu6ixSamplerAudioProcessor::saveMonolithInstrument (const juce::File& targetFile)
{
    auto xml = buildStateXml (true);
    if (!xml) return false;

    juce::ZipFile::Builder builder;
    auto xmlStr = xml->toString();
    builder.addEntry (new juce::MemoryInputStream (xmlStr.toRawUTF8(), xmlStr.getNumBytesAsUTF8(), true),
                      9, "preset.xml", juce::Time::getCurrentTime());

    juce::FlacAudioFormat flac;
    const juce::ScopedLock sl (dataLock);
    for (size_t i = 0; i < zones.size(); ++i)
    {
        auto& z = zones[i];
        if (z->buffer.getNumSamples() == 0) continue;

        auto* memOut = new juce::MemoryOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer (
            flac.createWriterFor (memOut, z->sampleRate, (unsigned int)z->buffer.getNumChannels(), 16, {}, 5));

        if (writer)
        {
            writer->writeFromAudioSampleBuffer (z->buffer, 0, z->buffer.getNumSamples());
            writer->flush();
            juce::MemoryBlock flacData (memOut->getData(), memOut->getDataSize());
            writer.reset(); // deletes memOut safely
            builder.addEntry (new juce::MemoryInputStream (flacData, true),
                              0, "samples/s_" + juce::String ((int)i) + ".flac", juce::Time::getCurrentTime());
        }
        else
        {
            delete memOut;
        }
    }

    juce::FileOutputStream outStream (targetFile);
    if (!outStream.openedOk()) return false;
    outStream.setPosition (0);
    outStream.truncate();
    return builder.writeToStream (outStream, nullptr);
}

bool NoiseMu6ixSamplerAudioProcessor::loadMonolithInstrument (const juce::File& sourceFile)
{
    if (!sourceFile.existsAsFile()) return false;
    juce::ZipFile zip (sourceFile);
    int xmlIdx = zip.getIndexOfFileName ("preset.xml");
    if (xmlIdx < 0) return false;

    std::unique_ptr<juce::InputStream> xmlStream (zip.createStreamForEntry (xmlIdx));
    if (!xmlStream) return false;

    auto xml = juce::parseXML (xmlStream->readEntireStreamAsString());
    if (!xml || !xml->hasTagName ("NoiseMu6ixStateV3")) return false;

    applyStateXml (*xml, &zip);
    return true;
}

void NoiseMu6ixSamplerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto root = buildStateXml (false))
        copyXmlToBinary (*root, destData);
}

void NoiseMu6ixSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto root = getXmlFromBinary (data, sizeInBytes);
    if (root && root->hasTagName ("NoiseMu6ixStateV3"))
        applyStateXml (*root, nullptr);
}

juce::AudioProcessorEditor* NoiseMu6ixSamplerAudioProcessor::createEditor()
{
    return new NoiseMu6ixSamplerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoiseMu6ixSamplerAudioProcessor();
}
