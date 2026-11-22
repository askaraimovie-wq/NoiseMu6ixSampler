#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <functional>

class MappingGridComponent : public juce::Component,
                             public juce::FileDragAndDropTarget
{
public:
    MappingGridComponent (NoiseMu6ixSamplerAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { dragMode = DragMode::None; }

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    int selectedZoneIdx = -1;
    int activeGroupFilter = -1;
    std::function<void()> onZoneChanged;

private:
    static constexpr int minNote = 24;
    static constexpr int maxNote = 96;
    enum class DragMode { None, Move, LeftEdge, RightEdge, TopEdge, BottomEdge };
    DragMode dragMode = DragMode::None;
    int dragStartNote = 60, dragStartVel = 64;
    int origLowK = 0, origHighK = 127, origLowV = 0, origHighV = 127, origRoot = 60;

    juce::Rectangle<float> getZoneRect (const SampleZone& z) const;
    int xToNote (float x) const;
    int yToVel (float y) const;

    NoiseMu6ixSamplerAudioProcessor& processor;
};

class WaveEditorComponent : public juce::Component
{
public:
    WaveEditorComponent() = default;
    void setZone (std::shared_ptr<SampleZone> z) { zone = z; repaint(); }
    void setEditMode (bool trimMode) { editingTrim = trimMode; repaint(); }
    void setZoom (float z) { zoomFactor = juce::jlimit (1.0f, 16.0f, z); repaint(); }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    std::function<void()> onWaveChanged;

private:
    std::shared_ptr<SampleZone> zone;
    bool editingTrim = false;
    bool draggingFirst = true;
    float zoomFactor = 1.0f;
};

class CustomMacroSlot : public juce::Component
{
public:
    CustomMacroSlot (NoiseMu6ixSamplerAudioProcessor& p, int slotIndex);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void setCustomizeMode (bool on);
    void refreshStyle();

    std::function<void()> onVisibilityChanged;

private:
    NoiseMu6ixSamplerAudioProcessor& processor;
    int index;
    bool customizeMode = false;

    juce::Label nameLabel;
    juce::Slider slider;
    juce::TextButton styleToggle;
    juce::TextButton hideBtn { "X" };
    juce::ComboBox targetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class FileBrowserListModel : public juce::ListBoxModel
{
public:
    juce::Array<juce::File> files;
    std::function<void (const juce::File&)> onFileDoubleClicked;

    int getNumRows() override { return files.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
};

class NoiseMu6ixSamplerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                              public juce::Timer
{
public:
    NoiseMu6ixSamplerAudioProcessorEditor (NoiseMu6ixSamplerAudioProcessor&);
    ~NoiseMu6ixSamplerAudioProcessorEditor() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    enum class EditTab { Group = 0, Mapping = 1, Wave = 2, FX = 3 };

    void updateModeVisibility();
    void refreshGroupList();
    void refreshSelectedZoneUI();
    void refreshFXUI();
    void applyZoneBarToSelected();
    void applyGroupInspector();
    void applyFXToProcessor();
    void browseLocalFolder();

    NoiseMu6ixSamplerAudioProcessor& processor;
    std::unique_ptr<juce::FileChooser> fileChooser;

    bool inEditMode = true;
    EditTab currentTab = EditTab::Mapping;
    bool customizeFrontUI = true;
    int selectedGroupIdx = 0;

    // Top Header Bar
    juce::TextButton modeToggleBtn { "Front Panel" };
    juce::TextButton saveInstBtn   { "Save .nm6i" };
    juce::TextButton loadInstBtn   { "Load .nm6i" };
    juce::TextButton loadFilesBtn  { "Load WAVs" };
    juce::TextButton loadFolderBtn { "Auto-Map" };
    juce::ComboBox rrModeBox;
    juce::Label voicesLabel { {}, "Voices: 0" };

    // Left Sidebar: File Browser
    juce::TextButton browseFolderBtn { "Open Folder..." };
    juce::TextButton addSelectedFileBtn { "+ Add to Grid" };
    juce::Label folderPathLabel { {}, "Select sample folder" };
    FileBrowserListModel fileListModel;
    juce::ListBox fileListBox { "FileList", &fileListModel };

    // Edit Mode Sub-Tabs
    juce::TextButton tabGroupBtn   { "Group Editor" };
    juce::TextButton tabMappingBtn { "Mapping Editor" };
    juce::TextButton tabWaveBtn    { "Wave Editor" };
    juce::TextButton tabFXBtn      { "FX & ADSR Rack" };

    // Group Editor Controls
    juce::ComboBox groupSelector;
    juce::TextButton createGroupBtn { "Create Group" };
    juce::TextButton deleteGroupBtn { "Delete Group" };
    juce::ToggleButton groupSoloBtn { "Solo" };
    juce::ToggleButton groupMuteBtn { "Mute" };
    juce::ToggleButton filterByGroupBtn { "Selected Group Only" };
    juce::Slider grpRRSlider, grpVolSlider, grpPanSlider, grpTuneSlider;
    juce::Label grpRRLbl { {}, "RR (1-10)" }, grpVolLbl { {}, "Group Vol (dB)" },
                grpPanLbl { {}, "Group Pan" }, grpTuneLbl { {}, "Group Tune" };

    // Mapping Editor Controls
    MappingGridComponent mappingGrid;
    juce::TextButton autoSpreadBtn { "Auto-Spread Keys" };
    juce::TextButton delZoneBtn    { "Del Zone" };
    juce::Label selZoneNameLbl     { {}, "Sample: (none selected)" };
    juce::Slider zRootSlider, zLowKSlider, zHighKSlider, zLowVSlider, zHighVSlider;
    juce::Slider zVolSlider, zPanSlider, zTuneSlider;
    juce::Label zRootLbl { {}, "Root" }, zLowKLbl { {}, "Low K" }, zHighKLbl { {}, "High K" },
                zLowVLbl { {}, "Low V" }, zHighVLbl { {}, "High V" },
                zVolLbl { {}, "Vol dB" }, zPanLbl { {}, "Pan" }, zTuneLbl { {}, "Tune" };

    // Wave Editor Controls
    WaveEditorComponent waveEditor;
    juce::ToggleButton loopToggle { "Long / Loop" };
    juce::TextButton autoLoopBtn  { "Auto-Loop (Zero-Cross)" };
    juce::ComboBox waveDragModeBox;
    juce::Slider crossfadeSlider, zoomSlider;
    juce::Label cfLbl { {}, "Crossfade" }, zoomLbl { {}, "Zoom" };

    // FX & ADSR Rack Controls (Inside Edit Mode Only!)
    juce::ToggleButton fxEqOn { "3-Band EQ" }, fxFltOn { "Filter" }, fxDrvOn { "Drive" },
                       fxChoOn { "Chorus" }, fxDlyOn { "Stereo Delay" }, fxRevOn { "Reverb" };
    juce::ComboBox fxFltTypeBox;
    juce::Slider fxEqLow, fxEqMid, fxEqHigh;
    juce::Slider fxFltCut, fxFltRes, fxDrvAmt;
    juce::Slider fxChoRate, fxChoDepth, fxChoMix;
    juce::Slider fxDlyTime, fxDlyFb, fxDlyMix;
    juce::Slider fxRevSize, fxRevDamp, fxRevMix;
    juce::Slider fxAtk, fxDec, fxSus, fxRel;

    juce::Label lEqLow{{},"Low"}, lEqMid{{},"Mid"}, lEqHigh{{},"High"},
                lFltCut{{},"Cut"}, lFltRes{{},"Res"}, lDrvAmt{{},"Drive"},
                lChoRate{{},"Rate"}, lChoDepth{{},"Depth"}, lChoMix{{},"Mix"},
                lDlyTime{{},"ms"}, lDlyFb{{},"Fdbk"}, lDlyMix{{},"Mix"},
                lRevSize{{},"Size"}, lRevDamp{{},"Damp"}, lRevMix{{},"Mix"},
                lAtk{{},"Atk"}, lDec{{},"Dec"}, lSus{{},"Sus"}, lRel{{},"Rel"};

    // Front Panel (Clean Custom UI)
    juce::TextButton addMacroBtn        { "+ Add Knob/Slider" };
    juce::TextButton toggleCustomizeBtn { "Done Customizing" };
    std::vector<std::unique_ptr<CustomMacroSlot>> macroSlots;

    // Bottom Piano Keyboard
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixSamplerAudioProcessorEditor)
};
