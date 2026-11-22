#pragma once
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <atomic>
#include <random>

struct SampleZone
{
    juce::String name;
    juce::File file;
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;

    int rootNote = 60;
    int lowKey = 0;
    int highKey = 127;
    int lowVel = 0;
    int highVel = 127;
    int rrGroup = 1; // 1 to 10

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
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Sample & Zone Management
    void loadSampleFile (const juce::File& file, bool autoMapName = true);
    void loadSampleFolder (const juce::File& folder);
    void autoSpreadKeyRanges();
    void removeZone (int index);
    void clearAllZones();
    static int findNearestZeroCrossing (const juce::AudioBuffer<float>& buf, int targetSample);
    void autoDetectLoop (std::shared_ptr<SampleZone> zone);

    juce::CriticalSection zoneLock;
    std::vector<std::shared_ptr<SampleZone>> zones;

    // 0 = Cycle, 1 = No-Repeat Random, 2 = True Random
    std::atomic<int> rrMode { 1 };

    static constexpr int numMacros = 6;
    MacroConfig macroConfigs[numMacros];

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    int parseNoteFromFilename (const juce::String& name);
    int parseRRFromFilename (const juce::String& name);
    float getMacroValueForTarget (MacroTarget target, float defaultValue) const;

    juce::AudioFormatManager formatManager;
    static constexpr int maxVoices = 32;
    NM6Voice voices[maxVoices];
    int lastRRIndexPerNote[128] = { 0 };
    std::mt19937 rng { std::random_device{}() };

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::Reverb reverb;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixSamplerAudioProcessor)
};
