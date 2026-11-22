#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay() = default;
    void setZone (std::shared_ptr<SampleZone> z) { zone = z; repaint(); }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    std::shared_ptr<SampleZone> zone;
    bool draggingStart = true;
};

class CustomMacroSlot : public juce::Component
{
public:
    CustomMacroSlot (NoiseMu6ixSamplerAudioProcessor& p, int slotIndex);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void refreshStyle();

private:
    NoiseMu6ixSamplerAudioProcessor& processor;
    int index;
    juce::Label nameLabel;
    juce::Slider slider;
    juce::TextButton styleToggle;
    juce::ComboBox targetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class NoiseMu6ixSamplerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                              public juce::FileDragAndDropTarget
{
public:
    NoiseMu6ixSamplerAudioProcessorEditor (NoiseMu6ixSamplerAudioProcessor&);
    ~NoiseMu6ixSamplerAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void refreshZoneList();
    void loadSelectedZoneToInspector();
    void applyInspectorToZone();

    NoiseMu6ixSamplerAudioProcessor& processor;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Top Bar
    juce::TextButton loadFilesBtn { "Load WAVs" };
    juce::TextButton loadFolderBtn { "Auto-Map Folder" };
    juce::TextButton clearBtn { "Clear All" };
    juce::ComboBox rrModeBox;
    juce::ComboBox zoneSelector;
    juce::TextButton deleteZoneBtn { "Del Zone" };

    // Waveform & Loop Controls
    WaveformDisplay waveform;
    juce::ToggleButton loopToggle { "Long / Loop" };
    juce::TextButton autoLoopBtn { "Auto-Loop (Zero-Cross)" };
    juce::Slider crossfadeSlider;
    juce::Label cfLabel { {}, "Crossfade" };

    // Zone Mapping & Velocity Controls
    juce::Slider rootNoteSlider, lowKeySlider, highKeySlider;
    juce::Slider lowVelSlider, highVelSlider, rrGroupSlider;
    juce::Label rootLbl { {}, "Root" }, lowKLbl { {}, "Low Key" }, highKLbl { {}, "High Key" };
    juce::Label lowVLbl { {}, "Min Vel" }, highVLbl { {}, "Max Vel" }, rrLbl { {}, "RR (1-10)" };

    // 6 Custom Macro Slots
    std::vector<std::unique_ptr<CustomMacroSlot>> macroSlots;

    // Piano Keyboard
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixSamplerAudioProcessorEditor)
};
