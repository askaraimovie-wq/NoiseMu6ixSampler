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

    bool keyswitchEnabled = false;
    int keyswitchNote = 24;
    int keyswitchColorIdx = 0;
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
    juce::String name = "Macro";
    bool isSlider = false;
    bool visible = false;
    int target = (int)MacroTarget::None;

    float value  = 0.5f;   // 0.0 to 1.0 live control value
    float minVal = 0.0f;   // From %
    float maxVal = 1.0f;   // To %

    int posX = 20;
    int posY = 20;
    int width = 100;
    int height = 135;
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

// ==================== Single Instrument Slot (Up to 16 in Multi-Rack) ====================
struct InstrumentSlot
{
    static constexpr int numMacros = 8;
    static constexpr int maxVoices = 24;

    bool loaded = false;
    juce::String instrumentName = "Empty Slot";

    // Routing (Kontakt-style)
    std::atomic<int> midiChannel { 1 };     // 0 = Omni, 1..16 = MIDI Ch 1..16
    std::atomic<int> outputBusIndex { 0 };  // 0..15 = Stereo Out 1..16
    std::atomic<float> slotVolumeDb { 0.0f };
    std::atomic<float> slotPan { 0.0f };    // -1..+1
    std::atomic<bool> slotMute { false };
    std::atomic<bool> slotSolo { false };

    // Sampler data
    std::vector<SampleGroup> groups;
    std::vector<std::shared_ptr<SampleZone>> zones;
    std::atomic<bool> rrEnabled { true };
    std::atomic<int> rrMode { 0 };
    std::atomic<bool> keyswitchMasterOn { false };
    std::atomic<int> activeKeyswitchNote { 24 };

    FXSettings fx;
    MacroConfig macroConfigs[numMacros];

    // Voices & DSP
    NM6Voice voices[maxVoices];
    int lastRRIndexPerNote[128] = { -1 };
    juce::dsp::IIR::Filter<float> eqLowL, eqLowR, eqMidL, eqMidR, eqHighL, eqHighR;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Chorus<float> chorus;
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    juce::Reverb reverb;

    InstrumentSlot();
    void initDefault (int slotIdx);
    void clearSlot (int slotIdx);
    void prepare (double sampleRate, int samplesPerBlock);
    float getMacroValue (MacroTarget target, float defaultValue) const;
    void triggerZone (std::shared_ptr<SampleZone> selectedZone, const SampleGroup& grp,
                      int note, int vel, const juce::ADSR::Parameters& adsrParams, double sr);
    int processSlot (juce::AudioBuffer<float>& outBuf, const juce::MidiBuffer& midiMessages,
                     double sampleRate, std::mt19937& rng, bool mutedByOtherSolo);
};

// ==================== Main Multi-Instrument Processor ====================
class NoiseMu6ixSamplerAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int maxSlots = 16;

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

    // Slot Management
    int findFirstEmptySlot() const;
    int activateNewEmptySlot();
    void removeSlot (int slotIdx);
    InstrumentSlot& getSlot (int slotIdx);

    // Save / Load Compressed Monolith Instrument (.nm6i) for a specific Slot
    bool saveSlotMonolith (int slotIdx, const juce::File& targetFile);
    bool loadSlotMonolith (int slotIdx, const juce::File& sourceFile);

    // Group & Zone helpers for a specific Slot
    int addGroup (int slotIdx, const juce::String& name = {});
    void removeGroup (int slotIdx, int groupIdx);
    void loadSampleFile (int slotIdx, const juce::File& file, int targetGroupIdx = 0, bool autoMapName = true, int dropNote = -1);
    void autoSpreadKeyRanges (int slotIdx, int onlyGroupIdx = -1);
    void removeZone (int slotIdx, int zoneIdx);

    static int findNearestZeroCrossing (const juce::AudioBuffer<float>& buf, int targetSample);
    static void autoDetectLoop (std::shared_ptr<SampleZone> zone);
    static juce::Colour getKeyswitchPaletteColour (int idx);

    juce::CriticalSection dataLock;
    std::unique_ptr<InstrumentSlot> slots[maxSlots];
    std::atomic<int> activeVoicesCount { 0 };
    juce::MidiKeyboardState keyboardState;

private:
    static BusesProperties createMultiOutBuses();
    int parseNoteFromFilename (const juce::String& name);
    std::unique_ptr<juce::XmlElement> buildSlotXml (const InstrumentSlot& slot, bool forMonolith);
    void applySlotXml (InstrumentSlot& slot, const juce::XmlElement& root, juce::ZipFile* monolithZip);

    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> slotScratchBuffer;
    std::mt19937 rng { std::random_device{}() };
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixSamplerAudioProcessor)
};
