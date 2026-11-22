#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

// ==================== InstrumentSlot ====================
InstrumentSlot::InstrumentSlot()
{
    for (int i = 0; i < 128; ++i) lastRRIndexPerNote[i] = -1;
}

void InstrumentSlot::initDefault (int slotIdx)
{
    loaded = true;
    instrumentName = "Instrument " + juce::String (slotIdx + 1);
    midiChannel.store (slotIdx + 1);    // Slot 1 -> Ch 1, Slot 2 -> Ch 2 ...
    outputBusIndex.store (slotIdx);     // Slot 1 -> Out 1, Slot 2 -> Out 2 ...
    slotVolumeDb.store (0.0f);
    slotPan.store (0.0f);
    slotMute.store (false);
    slotSolo.store (false);

    groups.clear();
    zones.clear();
    SampleGroup g1;
    g1.name = "Group 1";
    g1.keyswitchNote = 24;
    g1.keyswitchColorIdx = 0;
    groups.push_back (g1);

    macroConfigs[0] = { "Dynamics",   true,  true,  (int)MacroTarget::Dynamics,     0.85f, 0.0f, 1.0f,  18, 12,  82, 185 };
    macroConfigs[1] = { "Expression", true,  true,  (int)MacroTarget::Expression,   0.85f, 0.0f, 1.0f, 112, 12,  82, 185 };
    macroConfigs[2] = { "Cutoff",     false, true,  (int)MacroTarget::FilterCutoff, 1.00f, 0.0f, 1.0f, 215, 25, 115, 140 };
    macroConfigs[3] = { "Reverb",     false, true,  (int)MacroTarget::ReverbMix,    0.15f, 0.0f, 1.0f, 345, 25, 115, 140 };
    macroConfigs[4] = { "Release",    false, true,  (int)MacroTarget::Release,      0.25f, 0.0f, 1.0f, 475, 25, 115, 140 };
    macroConfigs[5] = { "Delay",      false, false, (int)MacroTarget::DelayMix,     0.00f, 0.0f, 1.0f, 605, 25, 115, 140 };
    macroConfigs[6] = { "Chorus",     false, false, (int)MacroTarget::ChorusMix,    0.00f, 0.0f, 1.0f, 215, 25, 115, 140 };
    macroConfigs[7] = { "Drive",      false, false, (int)MacroTarget::Drive,        0.00f, 0.0f, 1.0f, 345, 25, 115, 140 };
}

void InstrumentSlot::clearSlot (int slotIdx)
{
    loaded = false;
    instrumentName = "Empty Slot " + juce::String (slotIdx + 1);
    zones.clear();
    groups.clear();
    for (auto& v : voices) v.active = false;
}

void InstrumentSlot::prepare (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 2;

    filter.prepare (spec); filter.reset();
    chorus.prepare (spec); chorus.reset();

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    eqLowL.prepare (monoSpec);  eqLowR.prepare (monoSpec);
    eqMidL.prepare (monoSpec);  eqMidR.prepare (monoSpec);
    eqHighL.prepare (monoSpec); eqHighR.prepare (monoSpec);

    int maxDelaySamples = (int)(sampleRate * 1.5) + 16;
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

float InstrumentSlot::getMacroValue (MacroTarget target, float defaultValue) const
{
    for (int i = 0; i < numMacros; ++i)
    {
        if (macroConfigs[i].visible && macroConfigs[i].target == (int)target)
        {
            float raw = macroConfigs[i].value;
            float mn = juce::jlimit (0.0f, 1.0f, macroConfigs[i].minVal);
            float mx = juce::jlimit (0.0f, 1.0f, macroConfigs[i].maxVal);
            return juce::jlimit (0.0f, 1.0f, mn + raw * (mx - mn));
        }
    }
    return defaultValue;
}

void InstrumentSlot::triggerZone (std::shared_ptr<SampleZone> selectedZone, const SampleGroup& grp,
                                  int note, int vel, const juce::ADSR::Parameters& adsrParams, double sr)
{
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
                                  * (selectedZone->sampleRate / sr);
    targetVoice->adsr.setSampleRate (sr);
    targetVoice->adsr.setParameters (adsrParams);
    targetVoice->adsr.noteOn();
}

int InstrumentSlot::processSlot (juce::AudioBuffer<float>& outBuf, const juce::MidiBuffer& midiMessages,
                                 double sampleRate, std::mt19937& rng, bool mutedByOtherSolo)
{
    if (!loaded) return 0;

    float dynVal    = getMacroValue (MacroTarget::Dynamics, 0.85f);
    float exprVal   = getMacroValue (MacroTarget::Expression, 0.85f);
    float cutNorm   = getMacroValue (MacroTarget::FilterCutoff, fx.filterCutoff.load());
    float resNorm   = getMacroValue (MacroTarget::FilterRes, fx.filterRes.load());
    float revMix    = getMacroValue (MacroTarget::ReverbMix, fx.reverbMix.load());
    float revSize   = getMacroValue (MacroTarget::ReverbSize, fx.reverbSize.load());
    float dlyMix    = getMacroValue (MacroTarget::DelayMix, fx.delayMix.load());
    float choMix    = getMacroValue (MacroTarget::ChorusMix, fx.chorusMix.load());
    float drvVal    = getMacroValue (MacroTarget::Drive, fx.driveAmount.load());
    float atkNorm   = getMacroValue (MacroTarget::Attack, fx.attack.load());
    float decNorm   = getMacroValue (MacroTarget::Decay, fx.decay.load());
    float susNorm   = getMacroValue (MacroTarget::Sustain, fx.sustain.load());
    float relNorm   = getMacroValue (MacroTarget::Release, fx.release.load());
    float pitchNorm = getMacroValue (MacroTarget::Pitch, 0.5f);
    float panNorm   = getMacroValue (MacroTarget::Pan, 0.5f);

    juce::ADSR::Parameters adsrParams;
    adsrParams.attack  = 0.002f + atkNorm * atkNorm * 2.5f;
    adsrParams.decay   = 0.01f  + decNorm * decNorm * 2.5f;
    adsrParams.sustain = susNorm;
    adsrParams.release = 0.01f  + relNorm * relNorm * 4.0f;

    int chFilter = midiChannel.load(); // 0 = Omni, 1..16 = specific

    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (chFilter > 0 && msg.getChannel() != chFilter)
            continue;

        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            int vel  = msg.getVelocity();

            bool consumedByKS = false;
            if (keyswitchMasterOn.load())
            {
                for (const auto& g : groups)
                {
                    if (g.keyswitchEnabled && g.keyswitchNote == note)
                    {
                        activeKeyswitchNote.store (note);
                        consumedByKS = true;
                        break;
                    }
                }
            }

            if (!consumedByKS)
            {
                std::vector<std::shared_ptr<SampleZone>> matches;
                bool anySolo = false;
                for (auto& g : groups) if (g.solo) { anySolo = true; break; }
                bool ksMaster = keyswitchMasterOn.load();
                int curKS = activeKeyswitchNote.load();

                for (auto& z : zones)
                {
                    if (z->groupIndex < 0 || z->groupIndex >= (int)groups.size()) continue;
                    const auto& g = groups[(size_t)z->groupIndex];
                    if (g.mute) continue;
                    if (anySolo && !g.solo) continue;
                    if (ksMaster && g.keyswitchEnabled && g.keyswitchNote != curKS) continue;

                    if (note >= z->lowKey && note <= z->highKey &&
                        vel  >= z->lowVel && vel  <= z->highVel &&
                        z->buffer.getNumSamples() > 0)
                        matches.push_back (z);
                }

                if (!matches.empty())
                {
                    if (rrEnabled.load() && matches.size() > 1)
                    {
                        int count = (int)matches.size();
                        int chosenIdx = 0;
                        if (rrMode.load() == 0)
                            chosenIdx = (lastRRIndexPerNote[note] + 1) % count;
                        else
                        {
                            std::uniform_int_distribution<int> dist (0, count - 2);
                            int r = dist (rng);
                            chosenIdx = (r >= lastRRIndexPerNote[note]) ? r + 1 : r;
                        }
                        lastRRIndexPerNote[note] = chosenIdx;
                        auto selZ = matches[(size_t)chosenIdx];
                        SampleGroup grp = (selZ->groupIndex < (int)groups.size()) ? groups[(size_t)selZ->groupIndex] : SampleGroup{};
                        triggerZone (selZ, grp, note, vel, adsrParams, sampleRate);
                    }
                    else
                    {
                        for (auto& selZ : matches)
                        {
                            SampleGroup grp = (selZ->groupIndex < (int)groups.size()) ? groups[(size_t)selZ->groupIndex] : SampleGroup{};
                            triggerZone (selZ, grp, note, vel, adsrParams, sampleRate);
                        }
                    }
                }
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

    if (slotMute.load() || mutedByOtherSolo)
        return 0;

    int numSamples = outBuf.getNumSamples();
    outBuf.clear();
    float* outL = outBuf.getWritePointer (0);
    float* outR = outBuf.getWritePointer (1);

    float slotGain = dynVal * exprVal * 1.4f * juce::Decibels::decibelsToGain (slotVolumeDb.load());
    float combinedPan = juce::jlimit (0.0f, 1.0f, panNorm + slotPan.load() * 0.5f);
    float panL = std::cos (combinedPan * juce::MathConstants<float>::halfPi);
    float panR = std::sin (combinedPan * juce::MathConstants<float>::halfPi);
    double globalPitchMult = std::pow (2.0, ((pitchNorm - 0.5f) * 24.0f) / 12.0);

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
        double step  = v.basePitchRatio * globalPitchMult;

        for (int s = 0; s < numSamples; ++s)
        {
            if (!v.adsr.isActive()) { v.active = false; break; }
            if (!looping && v.playPos >= sEnd) { v.active = false; break; }

            float sL = 0.0f, sR = 0.0f;
            if (looping && cf > 4 && v.playPos >= (lEnd - cf) && v.playPos < lEnd)
            {
                double intoFade = v.playPos - (lEnd - cf);
                float t = (float)(intoFade / (double)cf);
                float gOut = std::cos (t * juce::MathConstants<float>::halfPi);
                float gIn  = std::sin (t * juce::MathConstants<float>::halfPi);
                double fadeInPos = lStart + intoFade;
                sL = readInterpolated (buf, 0, v.playPos) * gOut + readInterpolated (buf, 0, fadeInPos) * gIn;
                sR = readInterpolated (buf, 1, v.playPos) * gOut + readInterpolated (buf, 1, fadeInPos) * gIn;
            }
            else
            {
                sL = readInterpolated (buf, 0, v.playPos);
                sR = readInterpolated (buf, 1, v.playPos);
            }

            float env = v.adsr.getNextSample() * v.velocity * v.voiceGain * slotGain;
            outL[s] += sL * env * v.voicePanL * panL;
            outR[s] += sR * env * v.voicePanR * panR;

            v.playPos += step;
            if (looping && loopLen > 16 && v.playPos >= lEnd)
                v.playPos = (double)(lStart + cf) + (v.playPos - (double)lEnd);
        }
    }

    if (activeCnt == 0 && !fx.delayOn.load() && !fx.reverbOn.load())
        return 0;

    // 1. 3-Band EQ
    if (fx.eqOn.load())
    {
        auto lowC  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, 120.0f,  0.707f, juce::Decibels::decibelsToGain (fx.eqLowDb.load()));
        auto midC  = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 1000.0f, 0.9f,   juce::Decibels::decibelsToGain (fx.eqMidDb.load()));
        auto highC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 5500.0f, 0.707f, juce::Decibels::decibelsToGain (fx.eqHighDb.load()));
        eqLowL.coefficients = lowC;   eqLowR.coefficients = lowC;
        eqMidL.coefficients = midC;   eqMidR.coefficients = midC;
        eqHighL.coefficients = highC; eqHighR.coefficients = highC;
        for (int s = 0; s < numSamples; ++s)
        {
            outL[s] = eqHighL.processSample (eqMidL.processSample (eqLowL.processSample (outL[s])));
            outR[s] = eqHighR.processSample (eqMidR.processSample (eqLowR.processSample (outR[s])));
        }
    }

    // 2. Drive
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

    // 3. Filter
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
        juce::dsp::AudioBlock<float> block (outBuf);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chorus.process (ctx);
    }

    // 5. Stereo Delay
    if (fx.delayOn.load() && dlyMix > 0.001f && delayBuffer.getNumSamples() > 16)
    {
        int bufLen = delayBuffer.getNumSamples();
        int delaySampsL = juce::jlimit (1, bufLen - 2, (int)(fx.delayTimeMs.load() * 0.001 * sampleRate));
        int delaySampsR = juce::jlimit (1, bufLen - 2, (int)(fx.delayTimeMs.load() * 0.00125 * sampleRate));
        float fb = juce::jlimit (0.0f, 0.88f, fx.delayFeedback.load());
        float* dL = delayBuffer.getWritePointer (0);
        float* dR = delayBuffer.getWritePointer (1);

        for (int s = 0; s < numSamples; ++s)
        {
            int rL = (delayWritePos - delaySampsL + bufLen) % bufLen;
            int rR = (delayWritePos - delaySampsR + bufLen) % bufLen;
            float tapL = dL[rL], tapR = dR[rR];
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

    return activeCnt;
}

// ==================== NoiseMu6ixSamplerAudioProcessor ====================
juce::AudioProcessor::BusesProperties NoiseMu6ixSamplerAudioProcessor::createMultiOutBuses()
{
    BusesProperties buses;
    buses.addBus (false, "Out 1", juce::AudioChannelSet::stereo(), true);
    for (int i = 2; i <= maxSlots; ++i)
        buses.addBus (false, "Out " + juce::String (i), juce::AudioChannelSet::stereo(), true);
    return buses;
}

juce::Colour NoiseMu6ixSamplerAudioProcessor::getKeyswitchPaletteColour (int idx)
{
    switch (idx)
    {
        case 0:  return juce::Colour (0xffe04848);
        case 1:  return juce::Colour (0xff38c172);
        case 2:  return juce::Colour (0xfff2c94c);
        case 3:  return juce::Colour (0xff9b51e0);
        case 4:  return juce::Colour (0xfff2994a);
        case 5:  return juce::Colour (0xff2dd4bf);
        default: return juce::Colour (0xffe04848);
    }
}

NoiseMu6ixSamplerAudioProcessor::NoiseMu6ixSamplerAudioProcessor()
    : AudioProcessor (createMultiOutBuses())
{
    formatManager.registerBasicFormats();
    for (int i = 0; i < maxSlots; ++i)
    {
        slots[i] = std::make_unique<InstrumentSlot>();
        slots[i]->clearSlot (i);
    }
    // Start with Slot 1 active and ready
    slots[0]->initDefault (0);
}

bool NoiseMu6ixSamplerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.outputBuses.isEmpty()) return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (const auto& bus : layouts.outputBuses)
        if (!bus.isDisabled() && bus != juce::AudioChannelSet::stereo())
            return false;
    return true;
}

void NoiseMu6ixSamplerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;
    slotScratchBuffer.setSize (2, samplesPerBlock);
    for (int i = 0; i < maxSlots; ++i)
        slots[i]->prepare (sampleRate, samplesPerBlock);
}

InstrumentSlot& NoiseMu6ixSamplerAudioProcessor::getSlot (int slotIdx)
{
    return *slots[juce::jlimit (0, maxSlots - 1, slotIdx)];
}

int NoiseMu6ixSamplerAudioProcessor::findFirstEmptySlot() const
{
    for (int i = 0; i < maxSlots; ++i)
        if (!slots[i]->loaded) return i;
    return -1;
}

int NoiseMu6ixSamplerAudioProcessor::activateNewEmptySlot()
{
    const juce::ScopedLock sl (dataLock);
    int idx = findFirstEmptySlot();
    if (idx >= 0)
    {
        slots[idx]->initDefault (idx);
        slots[idx]->prepare (currentSampleRate, currentBlockSize);
    }
    return idx;
}

void NoiseMu6ixSamplerAudioProcessor::removeSlot (int slotIdx)
{
    const juce::ScopedLock sl (dataLock);
    if (slotIdx >= 0 && slotIdx < maxSlots)
        slots[slotIdx]->clearSlot (slotIdx);
}

int NoiseMu6ixSamplerAudioProcessor::addGroup (int slotIdx, const juce::String& name)
{
    const juce::ScopedLock sl (dataLock);
    auto& slot = getSlot (slotIdx);
    SampleGroup g;
    int idx = (int)slot.groups.size();
    g.name = name.isEmpty() ? ("Group " + juce::String (idx + 1)) : name;
    g.keyswitchNote = juce::jlimit (24, 96, 24 + idx);
    g.keyswitchColorIdx = idx % 6;
    slot.groups.push_back (g);
    return (int)slot.groups.size() - 1;
}

void NoiseMu6ixSamplerAudioProcessor::removeGroup (int slotIdx, int groupIdx)
{
    const juce::ScopedLock sl (dataLock);
    auto& slot = getSlot (slotIdx);
    if (slot.groups.size() <= 1 || groupIdx < 0 || groupIdx >= (int)slot.groups.size())
        return;

    slot.zones.erase (std::remove_if (slot.zones.begin(), slot.zones.end(),
                      [groupIdx](const std::shared_ptr<SampleZone>& z) { return z->groupIndex == groupIdx; }),
                      slot.zones.end());

    for (auto& z : slot.zones)
        if (z->groupIndex > groupIdx) z->groupIndex -= 1;

    slot.groups.erase (slot.groups.begin() + groupIdx);
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
        if (idx1 > 0 && idx1 < numSamples && data[idx1 - 1] <= 0.0f && data[idx1] > 0.0f) return idx1;
        int idx2 = targetSample - offset;
        if (idx2 > 0 && idx2 < numSamples && data[idx2 - 1] <= 0.0f && data[idx2] > 0.0f) return idx2;
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
        for (int n = 11; n >= 0; --n)
            if (upper.contains (juce::String (noteNames[n]) + juce::String (oct)) ||
                upper.contains (juce::String (flatNames[n]) + juce::String (oct)))
                return juce::jlimit (0, 127, (oct + 1) * 12 + n);
    return 60;
}

void NoiseMu6ixSamplerAudioProcessor::loadSampleFile (int slotIdx, const juce::File& file,
                                                      int targetGroupIdx, bool autoMapName, int dropNote)
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
    auto& slot = getSlot (slotIdx);
    if (!slot.loaded) slot.initDefault (slotIdx);
    if (slot.groups.empty()) slot.groups.push_back (SampleGroup{});

    int gIdx = juce::jlimit (0, (int)slot.groups.size() - 1, targetGroupIdx);
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
        auto lower = zone->name.toLowerCase();
        if (lower.contains ("long") || lower.contains ("sus"))
            zone->isLooping.store (true);
    }

    zone->groupIndex = gIdx;
    autoDetectLoop (zone);
    slot.zones.push_back (zone);
}

void NoiseMu6ixSamplerAudioProcessor::autoSpreadKeyRanges (int slotIdx, int onlyGroupIdx)
{
    const juce::ScopedLock sl (dataLock);
    auto& slot = getSlot (slotIdx);
    std::vector<int> uniqueRoots;
    for (auto& z : slot.zones)
    {
        if (onlyGroupIdx >= 0 && z->groupIndex != onlyGroupIdx) continue;
        if (std::find (uniqueRoots.begin(), uniqueRoots.end(), z->rootNote) == uniqueRoots.end())
            uniqueRoots.push_back (z->rootNote);
    }
    if (uniqueRoots.empty()) return;
    std::sort (uniqueRoots.begin(), uniqueRoots.end());

    for (auto& z : slot.zones)
    {
        if (onlyGroupIdx >= 0 && z->groupIndex != onlyGroupIdx) continue;
        auto it = std::find (uniqueRoots.begin(), uniqueRoots.end(), z->rootNote);
        if (it == uniqueRoots.end()) continue;
        size_t idx = (size_t)std::distance (uniqueRoots.begin(), it);
        z->lowKey  = (idx == 0) ? 24 : (uniqueRoots[idx - 1] + uniqueRoots[idx]) / 2 + 1;
        z->highKey = (idx + 1 == uniqueRoots.size()) ? 96 : (uniqueRoots[idx] + uniqueRoots[idx + 1]) / 2;
    }
}

void NoiseMu6ixSamplerAudioProcessor::removeZone (int slotIdx, int zoneIdx)
{
    const juce::ScopedLock sl (dataLock);
    auto& slot = getSlot (slotIdx);
    if (zoneIdx >= 0 && zoneIdx < (int)slot.zones.size())
        slot.zones.erase (slot.zones.begin() + zoneIdx);
}

void NoiseMu6ixSamplerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    keyboardState.processNextMidiBuffer (midiMessages, 0, buffer.getNumSamples(), true);

    const juce::ScopedTryLock sl (dataLock);
    if (!sl.isLocked()) return;

    int numSamples = buffer.getNumSamples();
    if (slotScratchBuffer.getNumSamples() < numSamples)
        slotScratchBuffer.setSize (2, numSamples, false, false, true);

    bool anySlotSolo = false;
    for (int i = 0; i < maxSlots; ++i)
        if (slots[i]->loaded && slots[i]->slotSolo.load()) { anySlotSolo = true; break; }

    int totalVoices = 0;
    int numOutBuses = getBusCount (false);

    for (int i = 0; i < maxSlots; ++i)
    {
        auto& slot = *slots[i];
        if (!slot.loaded) continue;

        bool mutedBySolo = anySlotSolo && !slot.slotSolo.load();
        juce::AudioBuffer<float> scratchSlice (slotScratchBuffer.getArrayOfWritePointers(), 2, numSamples);
        totalVoices += slot.processSlot (scratchSlice, midiMessages, currentSampleRate, rng, mutedBySolo);

        int busIdx = juce::jlimit (0, std::max (0, numOutBuses - 1), slot.outputBusIndex.load());
        auto destBus = getBusBuffer (buffer, false, busIdx);
        if (destBus.getNumChannels() >= 2)
        {
            destBus.addFrom (0, 0, scratchSlice, 0, 0, numSamples);
            destBus.addFrom (1, 0, scratchSlice, 1, 0, numSamples);
        }
        else if (numOutBuses > 0)
        {
            auto mainBus = getBusBuffer (buffer, false, 0);
            if (mainBus.getNumChannels() >= 2)
            {
                mainBus.addFrom (0, 0, scratchSlice, 0, 0, numSamples);
                mainBus.addFrom (1, 0, scratchSlice, 1, 0, numSamples);
            }
        }
    }
    activeVoicesCount.store (totalVoices);
}

std::unique_ptr<juce::XmlElement> NoiseMu6ixSamplerAudioProcessor::buildSlotXml (const InstrumentSlot& slot, bool forMonolith)
{
    auto root = std::make_unique<juce::XmlElement> ("NoiseMu6ixStateV107");
    root->setAttribute ("instName", slot.instrumentName);
    root->setAttribute ("loaded", slot.loaded);
    root->setAttribute ("midiCh", slot.midiChannel.load());
    root->setAttribute ("outBus", slot.outputBusIndex.load());
    root->setAttribute ("slotVol", slot.slotVolumeDb.load());
    root->setAttribute ("slotPan", slot.slotPan.load());
    root->setAttribute ("slotMute", slot.slotMute.load());
    root->setAttribute ("slotSolo", slot.slotSolo.load());

    root->setAttribute ("rrEnabled", slot.rrEnabled.load());
    root->setAttribute ("rrMode", slot.rrMode.load());
    root->setAttribute ("ksMaster", slot.keyswitchMasterOn.load());
    root->setAttribute ("activeKS", slot.activeKeyswitchNote.load());

    auto* fxXml = root->createNewChildElement ("FX");
    fxXml->setAttribute ("eqOn", slot.fx.eqOn.load());
    fxXml->setAttribute ("eqLow", slot.fx.eqLowDb.load());
    fxXml->setAttribute ("eqMid", slot.fx.eqMidDb.load());
    fxXml->setAttribute ("eqHigh", slot.fx.eqHighDb.load());
    fxXml->setAttribute ("fltOn", slot.fx.filterOn.load());
    fxXml->setAttribute ("fltType", slot.fx.filterType.load());
    fxXml->setAttribute ("fltCut", slot.fx.filterCutoff.load());
    fxXml->setAttribute ("fltRes", slot.fx.filterRes.load());
    fxXml->setAttribute ("drvOn", slot.fx.driveOn.load());
    fxXml->setAttribute ("drvAmt", slot.fx.driveAmount.load());
    fxXml->setAttribute ("choOn", slot.fx.chorusOn.load());
    fxXml->setAttribute ("choRate", slot.fx.chorusRate.load());
    fxXml->setAttribute ("choDepth", slot.fx.chorusDepth.load());
    fxXml->setAttribute ("choMix", slot.fx.chorusMix.load());
    fxXml->setAttribute ("dlyOn", slot.fx.delayOn.load());
    fxXml->setAttribute ("dlyTime", slot.fx.delayTimeMs.load());
    fxXml->setAttribute ("dlyFb", slot.fx.delayFeedback.load());
    fxXml->setAttribute ("dlyMix", slot.fx.delayMix.load());
    fxXml->setAttribute ("revOn", slot.fx.reverbOn.load());
    fxXml->setAttribute ("revSize", slot.fx.reverbSize.load());
    fxXml->setAttribute ("revDamp", slot.fx.reverbDamp.load());
    fxXml->setAttribute ("revMix", slot.fx.reverbMix.load());
    fxXml->setAttribute ("atk", slot.fx.attack.load());
    fxXml->setAttribute ("dec", slot.fx.decay.load());
    fxXml->setAttribute ("sus", slot.fx.sustain.load());
    fxXml->setAttribute ("rel", slot.fx.release.load());

    auto* macrosXml = root->createNewChildElement ("Macros");
    for (int i = 0; i < InstrumentSlot::numMacros; ++i)
    {
        auto* m = macrosXml->createNewChildElement ("Macro");
        m->setAttribute ("name", slot.macroConfigs[i].name);
        m->setAttribute ("isSlider", slot.macroConfigs[i].isSlider);
        m->setAttribute ("visible", slot.macroConfigs[i].visible);
        m->setAttribute ("target", slot.macroConfigs[i].target);
        m->setAttribute ("val", slot.macroConfigs[i].value);
        m->setAttribute ("minVal", slot.macroConfigs[i].minVal);
        m->setAttribute ("maxVal", slot.macroConfigs[i].maxVal);
        m->setAttribute ("x", slot.macroConfigs[i].posX);
        m->setAttribute ("y", slot.macroConfigs[i].posY);
        m->setAttribute ("w", slot.macroConfigs[i].width);
        m->setAttribute ("h", slot.macroConfigs[i].height);
    }

    auto* groupsXml = root->createNewChildElement ("Groups");
    for (auto& g : slot.groups)
    {
        auto* gx = groupsXml->createNewChildElement ("Group");
        gx->setAttribute ("name", g.name);
        gx->setAttribute ("vol", g.volumeDb);
        gx->setAttribute ("pan", g.pan);
        gx->setAttribute ("tune", g.tuneSemi);
        gx->setAttribute ("solo", g.solo);
        gx->setAttribute ("mute", g.mute);
        gx->setAttribute ("ksOn", g.keyswitchEnabled);
        gx->setAttribute ("ksNote", g.keyswitchNote);
        gx->setAttribute ("ksCol", g.keyswitchColorIdx);
    }

    auto* zonesXml = root->createNewChildElement ("Zones");
    for (size_t i = 0; i < slot.zones.size(); ++i)
    {
        auto& z = slot.zones[i];
        auto* zx = zonesXml->createNewChildElement ("Zone");
        zx->setAttribute ("name", z->name);
        if (forMonolith) zx->setAttribute ("entry", "samples/s_" + juce::String ((int)i) + ".flac");
        else             zx->setAttribute ("path", z->file.getFullPathName());

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

void NoiseMu6ixSamplerAudioProcessor::applySlotXml (InstrumentSlot& slot, const juce::XmlElement& root, juce::ZipFile* monolithZip)
{
    slot.loaded = root.getBoolAttribute ("loaded", true);
    if (root.hasAttribute ("instName"))
        slot.instrumentName = root.getStringAttribute ("instName", slot.instrumentName);
    if (monolithZip == nullptr)
    {
        slot.midiChannel.store    (root.getIntAttribute ("midiCh", slot.midiChannel.load()));
        slot.outputBusIndex.store (root.getIntAttribute ("outBus", slot.outputBusIndex.load()));
        slot.slotVolumeDb.store   ((float)root.getDoubleAttribute ("slotVol", 0.0));
        slot.slotPan.store        ((float)root.getDoubleAttribute ("slotPan", 0.0));
        slot.slotMute.store       (root.getBoolAttribute ("slotMute", false));
        slot.slotSolo.store       (root.getBoolAttribute ("slotSolo", false));
    }

    slot.rrEnabled.store (root.getBoolAttribute ("rrEnabled", true));
    slot.rrMode.store (root.getIntAttribute ("rrMode", 0));
    slot.keyswitchMasterOn.store (root.getBoolAttribute ("ksMaster", false));
    slot.activeKeyswitchNote.store (root.getIntAttribute ("activeKS", 24));

    if (auto* fxXml = root.getChildByName ("FX"))
    {
        slot.fx.eqOn.store         (fxXml->getBoolAttribute ("eqOn", false));
        slot.fx.eqLowDb.store      ((float)fxXml->getDoubleAttribute ("eqLow", 0.0));
        slot.fx.eqMidDb.store      ((float)fxXml->getDoubleAttribute ("eqMid", 0.0));
        slot.fx.eqHighDb.store     ((float)fxXml->getDoubleAttribute ("eqHigh", 0.0));
        slot.fx.filterOn.store     (fxXml->getBoolAttribute ("fltOn", true));
        slot.fx.filterType.store   (fxXml->getIntAttribute ("fltType", 0));
        slot.fx.filterCutoff.store ((float)fxXml->getDoubleAttribute ("fltCut", 1.0));
        slot.fx.filterRes.store    ((float)fxXml->getDoubleAttribute ("fltRes", 0.1));
        slot.fx.driveOn.store      (fxXml->getBoolAttribute ("drvOn", false));
        slot.fx.driveAmount.store  ((float)fxXml->getDoubleAttribute ("drvAmt", 0.0));
        slot.fx.chorusOn.store     (fxXml->getBoolAttribute ("choOn", false));
        slot.fx.chorusRate.store   ((float)fxXml->getDoubleAttribute ("choRate", 1.2));
        slot.fx.chorusDepth.store  ((float)fxXml->getDoubleAttribute ("choDepth", 0.25));
        slot.fx.chorusMix.store    ((float)fxXml->getDoubleAttribute ("choMix", 0.35));
        slot.fx.delayOn.store      (fxXml->getBoolAttribute ("dlyOn", false));
        slot.fx.delayTimeMs.store  ((float)fxXml->getDoubleAttribute ("dlyTime", 320.0));
        slot.fx.delayFeedback.store((float)fxXml->getDoubleAttribute ("dlyFb", 0.35));
        slot.fx.delayMix.store     ((float)fxXml->getDoubleAttribute ("dlyMix", 0.25));
        slot.fx.reverbOn.store     (fxXml->getBoolAttribute ("revOn", true));
        slot.fx.reverbSize.store   ((float)fxXml->getDoubleAttribute ("revSize", 0.65));
        slot.fx.reverbDamp.store   ((float)fxXml->getDoubleAttribute ("revDamp", 0.45));
        slot.fx.reverbMix.store    ((float)fxXml->getDoubleAttribute ("revMix", 0.15));
        slot.fx.attack.store       ((float)fxXml->getDoubleAttribute ("atk", 0.02));
        slot.fx.decay.store        ((float)fxXml->getDoubleAttribute ("dec", 0.25));
        slot.fx.sustain.store      ((float)fxXml->getDoubleAttribute ("sus", 1.0));
        slot.fx.release.store      ((float)fxXml->getDoubleAttribute ("rel", 0.25));
    }

    if (auto* macrosXml = root.getChildByName ("Macros"))
    {
        int i = 0;
        for (auto* m : macrosXml->getChildIterator())
        {
            if (i >= InstrumentSlot::numMacros) break;
            slot.macroConfigs[i].name     = m->getStringAttribute ("name", "Macro");
            slot.macroConfigs[i].isSlider = m->getBoolAttribute ("isSlider", false);
            slot.macroConfigs[i].visible  = m->getBoolAttribute ("visible", true);
            slot.macroConfigs[i].target   = m->getIntAttribute ("target", 1);
            slot.macroConfigs[i].value    = (float)m->getDoubleAttribute ("val", slot.macroConfigs[i].value);
            slot.macroConfigs[i].minVal   = (float)m->getDoubleAttribute ("minVal", 0.0);
            slot.macroConfigs[i].maxVal   = (float)m->getDoubleAttribute ("maxVal", 1.0);
            slot.macroConfigs[i].posX     = m->getIntAttribute ("x", slot.macroConfigs[i].posX);
            slot.macroConfigs[i].posY     = m->getIntAttribute ("y", slot.macroConfigs[i].posY);
            slot.macroConfigs[i].width    = m->getIntAttribute ("w", slot.macroConfigs[i].width);
            slot.macroConfigs[i].height   = m->getIntAttribute ("h", slot.macroConfigs[i].height);
            ++i;
        }
    }

    if (auto* groupsXml = root.getChildByName ("Groups"))
    {
        slot.groups.clear();
        for (auto* gx : groupsXml->getChildIterator())
        {
            SampleGroup g;
            g.name              = gx->getStringAttribute ("name", "Group");
            g.volumeDb          = (float)gx->getDoubleAttribute ("vol", 0.0);
            g.pan               = (float)gx->getDoubleAttribute ("pan", 0.0);
            g.tuneSemi          = (float)gx->getDoubleAttribute ("tune", 0.0);
            g.solo              = gx->getBoolAttribute ("solo", false);
            g.mute              = gx->getBoolAttribute ("mute", false);
            g.keyswitchEnabled  = gx->getBoolAttribute ("ksOn", false);
            g.keyswitchNote     = gx->getIntAttribute ("ksNote", 24);
            g.keyswitchColorIdx = gx->getIntAttribute ("ksCol", 0);
            slot.groups.push_back (g);
        }
        if (slot.groups.empty()) slot.groups.push_back (SampleGroup{});
    }

    if (auto* zonesXml = root.getChildByName ("Zones"))
    {
        slot.zones.clear();
        for (auto* zx : zonesXml->getChildIterator())
        {
            std::shared_ptr<SampleZone> loadedZone;
            if (monolithZip != nullptr && zx->hasAttribute ("entry"))
            {
                int idx = monolithZip->getIndexOfFileName (zx->getStringAttribute ("entry"));
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
                    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (f));
                    if (reader)
                    {
                        loadedZone = std::make_shared<SampleZone>();
                        loadedZone->name = f.getFileNameWithoutExtension();
                        loadedZone->file = f;
                        loadedZone->sampleRate = reader->sampleRate;
                        loadedZone->buffer.setSize ((int)reader->numChannels, (int)reader->lengthInSamples);
                        reader->read (&loadedZone->buffer, 0, (int)reader->lengthInSamples, 0, true, true);
                    }
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
                slot.zones.push_back (loadedZone);
            }
        }
    }
}

bool NoiseMu6ixSamplerAudioProcessor::saveSlotMonolith (int slotIdx, const juce::File& targetFile)
{
    const juce::ScopedLock sl (dataLock);
    auto& slot = getSlot (slotIdx);
    slot.instrumentName = targetFile.getFileNameWithoutExtension();

    auto xml = buildSlotXml (slot, true);
    if (!xml) return false;

    juce::ZipFile::Builder builder;
    auto xmlStr = xml->toString();
    builder.addEntry (new juce::MemoryInputStream (xmlStr.toRawUTF8(), xmlStr.getNumBytesAsUTF8(), true),
                      9, "preset.xml", juce::Time::getCurrentTime());

    juce::FlacAudioFormat flac;
    for (size_t i = 0; i < slot.zones.size(); ++i)
    {
        auto& z = slot.zones[i];
        if (z->buffer.getNumSamples() == 0) continue;
        auto* memOut = new juce::MemoryOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer (
            flac.createWriterFor (memOut, z->sampleRate, (unsigned int)z->buffer.getNumChannels(), 16, {}, 5));
        if (writer)
        {
            writer->writeFromAudioSampleBuffer (z->buffer, 0, z->buffer.getNumSamples());
            writer->flush();
            juce::MemoryBlock flacData (memOut->getData(), memOut->getDataSize());
            writer.reset();
            builder.addEntry (new juce::MemoryInputStream (flacData, true),
                              0, "samples/s_" + juce::String ((int)i) + ".flac", juce::Time::getCurrentTime());
        }
        else delete memOut;
    }

    juce::FileOutputStream outStream (targetFile);
    if (!outStream.openedOk()) return false;
    outStream.setPosition (0);
    outStream.truncate();
    return builder.writeToStream (outStream, nullptr);
}

bool NoiseMu6ixSamplerAudioProcessor::loadSlotMonolith (int slotIdx, const juce::File& sourceFile)
{
    if (!sourceFile.existsAsFile()) return false;
    juce::ZipFile zip (sourceFile);
    int xmlIdx = zip.getIndexOfFileName ("preset.xml");
    if (xmlIdx < 0) return false;
    std::unique_ptr<juce::InputStream> xmlStream (zip.createStreamForEntry (xmlIdx));
    if (!xmlStream) return false;
    auto xml = juce::parseXML (xmlStream->readEntireStreamAsString());
    if (!xml) return false;

    const juce::ScopedLock sl (dataLock);
    auto& slot = getSlot (slotIdx);
    if (!slot.loaded) slot.initDefault (slotIdx);
    applySlotXml (slot, *xml, &zip);
    slot.instrumentName = sourceFile.getFileNameWithoutExtension();
    slot.loaded = true;
    slot.prepare (currentSampleRate, currentBlockSize);
    return true;
}

void NoiseMu6ixSamplerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock sl (dataLock);
    auto multiRoot = std::make_unique<juce::XmlElement> ("NoiseMu6ixMultiV107");
    for (int i = 0; i < maxSlots; ++i)
    {
        auto slotXml = buildSlotXml (*slots[i], false);
        slotXml->setAttribute ("slotIdx", i);
        multiRoot->addChildElement (slotXml.release());
    }
    copyXmlToBinary (*multiRoot, destData);
}

void NoiseMu6ixSamplerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto multiRoot = getXmlFromBinary (data, sizeInBytes);
    if (!multiRoot || !multiRoot->hasTagName ("NoiseMu6ixMultiV107")) return;

    const juce::ScopedLock sl (dataLock);
    for (auto* child : multiRoot->getChildIterator())
    {
        int idx = child->getIntAttribute ("slotIdx", -1);
        if (idx >= 0 && idx < maxSlots)
        {
            applySlotXml (*slots[idx], *child, nullptr);
            slots[idx]->prepare (currentSampleRate, currentBlockSize);
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
