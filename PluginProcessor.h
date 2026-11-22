#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <atomic>
#include <random>

struct SampleGroup
{
    juce::String name = "Group 1";
    float volumeDb = 0.0f;
    float pan = 0.0f;
    float tuneSemi = 0.0f;
    bool solo = false;
    bool mute = false;

    // Keyswitch support per group
    bool keyswitchEnabled = false;
    int keyswitchNote = 24;        // Default C1
    int keyswitchColorIdx = 0;     // 0=Red, 1=Green, 2=Yellow, 3=Purple, 4=Orange, 5=Cyan
};

struct SampleZone
{
    juce::String name;
    juce::File file;
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;

    int groupIndex = 0;
    int rootNote = 60;
    int lowKey = 48;
    int highKey = 72;
    int lowVel = 0;
    int highVel = 127;

    float volumeDb = 0.0f;
    float pan = 0.0f;
    float tuneSemi = 0.0f;

    std::atomic<int> sampleStart { 0 };
    std::atomic<int> sampleEnd { 0 };

    std::atomic<bool> isLooping { false };
    std::atomic<int> loopStart { 0 };
    std::atomic<int> loopEnd { 0 };
    std::atomic<int> crossfadeSamples { 2048 };
};

enum class MacroTarget
{
    None = 1,
    Dynamics,
    Expression,
    FilterCutoff,
    FilterRes,
    ReverbMix,
    ReverbSize,
    DelayMix,
    ChorusMix,
    Drive,
    Attack,
    Decay,
    Sustain,
    Release,
    Pitch,
    Pan
};

struct MacroConfig
{
    juce::String name;
    bool isSlider = false;
    bool visible = true;
    int target = (int)MacroTarget::None;
};

struct FXSettings
{
    std::atomic<bool> eqOn { false };
    std::atomic<float> eqLowDb  { 0.0f };
    std::atomic<float> eqMidDb  { 0.0f };
    std::atomic<float> eqHighDb { 0.0f };

    std::atomic<bool> filterOn { true };
    std::atomic<int> filterType { 0 };
    std::atomic<float> filterCutoff { 1.0f };
    std::atomic<float> filterRes { 0.1f };

    std::atomic<bool> driveOn { false };
    std::atomic<float> driveAmount { 0.0f };

    std::atomic<bool> chorusOn { false };
    std::atomic<float> chorusRate { 1.2f };
    std::atomic<float> chorusDepth { 0.25f };
    std::atomic<float> chorusMix { 0.35f };

    std::atomic<bool> delayOn { false };
    std::atomic<float> delayTimeMs { 320.0f };
    std::atomic<float> delayFeedback { 0.35f };
    std::atomic<float> delayMix { 0.25f };

    std::atomic<bool> reverbOn { true };
    std::atomic<float> reverbSize { 0.65f };
    std::atomic<float> reverbDamp { 0.45f };
    std::atomic<float> reverbMix { 0.15f };

    std::atomic<float> attack  { 0.02f };
    std::atomic<float> decay   { 0.25f };
    std::atomic<float> sustain { 1.0f };
    std::atomic<float> release { 0.25f };
};

struct NM6Voice
{
    bool active = false;
    int midiNote = -1;
    float velocity = 1.0f;
    std::shared_ptr<SampleZone> zone;
    double playPos = 0.0;
    double basePitchRatio = 1.0;
    float voiceGain = 1.0f;
    float voicePanL = 0.707f;
    float voicePanR = 0.707f;
    juce::ADSR adsr;
};

class NoiseMu6ixSamplerAudioProcessor : public juce::AudioProcessor
{
public:
    NoiseMu6ixSamplerAudioProcessor();
    ~NoiseMu6ixSamplerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool saveMonolithInstrument (const juce::File& targetFile);
    bool loadMonolithInstrument (const juce::File& sourceFile);

    int addGroup (const juce::String& name = {});
    void removeGroup (int groupIdx);
    void loadSampleFile (const juce::File& file, int targetGroupIdx = 0, bool autoMapName = true, int dropNote = -1);
    void loadSampleFolder (const juce::File& folder, int targetGroupIdx = 0);
    void autoSpreadKeyRanges (int onlyGroupIdx = -1);
    void removeZone (int index);
    void clearAllZones();

    static int findNearestZeroCrossing (const juce::AudioBuffer<float>& buf, int targetSample);
    void autoDetectLoop (std::shared_ptr<SampleZone> zone);

    static juce::Colour getKeyswitchPaletteColour (int idx);

    juce::CriticalSection dataLock;
    std::vector<SampleGroup> groups;
    std::vector<std::shared_ptr<SampleZone>> zones;

    // Unlimited Round Robin ON/OFF + Mode (0 = Cycle, 1 = No-Repeat Random)
    std::atomic<bool> rrEnabled { true };
    std::atomic<int> rrMode { 0 };

    // Keyswitch Master Switch & Active Keyswitch Note
    std::atomic<bool> keyswitchMasterOn { false };
    std::atomic<int> activeKeyswitchNote { 24 };

    std::atomic<int> activeVoicesCount { 0 };
    FXSettings fx;

    static constexpr int numMacros = 8;
    MacroConfig macroConfigs[numMacros];

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    int parseNoteFromFilename (const juce::String& name);
    int parseRRFromFilename (const juce::String& name);
    float getMacroValueForTarget (MacroTarget target, float defaultValue) const;
    std::unique_ptr<juce::XmlElement> buildStateXml (bool forMonolith);
    void applyStateXml (const juce::XmlElement& root, juce::ZipFile* monolithZip);
    void triggerZone (std::shared_ptr<SampleZone> selectedZone, const SampleGroup& grp,
                      int note, int vel, const juce::ADSR::Parameters& adsrParams);

    juce::AudioFormatManager formatManager;
    static constexpr int maxVoices = 64;
    NM6Voice voices[maxVoices];
    int lastRRIndexPerNote[128] = { -1 };
    std::mt19937 rng { std::random_device{}() };

    juce::dsp::IIR::Filter<float> eqLowL, eqLowR, eqMidL, eqMidR, eqHighL, eqHighR;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Chorus<float> chorus;
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    juce::Reverb reverb;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixSamplerAudioProcessor)
};
