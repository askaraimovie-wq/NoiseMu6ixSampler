#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <atomic>
#include <random>

struct SampleGroup
{
    juce::String name = "Group 1";
    int rrGroup = 1; // 1 to 10
    float volumeDb = 0.0f;
    float pan = 0.0f;      // -1.0 to 1.0
    float tuneSemi = 0.0f; // -24 to +24
    bool solo = false;
    bool mute = false;
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
    float pan = 0.0f;      // -1.0 to 1.0
    float tuneSemi = 0.0f; // -24 to +24

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
    bool isSlider = false; // false = Knob, true = Slider
    bool visible = true;
    int target = (int)MacroTarget::None;
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
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Group & Zone Management
    int addGroup (const juce::String& name = {});
    void removeGroup (int groupIdx);
    void loadSampleFile (const juce::File& file, int targetGroupIdx = 0, bool autoMapName = true, int dropNote = -1);
    void loadSampleFolder (const juce::File& folder, int targetGroupIdx = 0);
    void autoSpreadKeyRanges();
    void removeZone (int index);
    void clearAllZones();

    static int findNearestZeroCrossing (const juce::AudioBuffer<float>& buf, int targetSample);
    void autoDetectLoop (std::shared_ptr<SampleZone> zone);

    juce::CriticalSection dataLock;
    std::vector<SampleGroup> groups;
    std::vector<std::shared_ptr<SampleZone>> zones;

    // 0 = Cycle, 1 = No-Repeat Random, 2 = True Random
    std::atomic<int> rrMode { 1 };
    std::atomic<int> activeVoicesCount { 0 };

    static constexpr int numMacros = 8;
    MacroConfig macroConfigs[numMacros];

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    int parseNoteFromFilename (const juce::String& name);
    int parseRRFromFilename (const juce::String& name);
    float getMacroValueForTarget (MacroTarget target, float defaultValue) const;

    juce::AudioFormatManager formatManager;
    static constexpr int maxVoices = 48;
    NM6Voice voices[maxVoices];
    int lastRRIndexPerNote[128] = { 0 };
    std::mt19937 rng { std::random_device{}() };

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::Reverb reverb;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixSamplerAudioProcessor)
};
