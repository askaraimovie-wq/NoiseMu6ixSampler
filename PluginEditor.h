#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <functional>

// ==================== Kontakt-Style Individual Group Boxes Grid ====================
class GroupCardsComponent : public juce::Component
{
public:
    GroupCardsComponent (NoiseMu6ixSamplerAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

    int selectedGroupIdx = 0;
    std::function<void (int)> onGroupSelected;

private:
    juce::Rectangle<int> getCardRect (int idx) const;
    NoiseMu6ixSamplerAudioProcessor& processor;
};

// ==================== Simplified & Smooth Mapping Editor Grid ====================
class MappingGridComponent : public juce::Component,
                             public juce::FileDragAndDropTarget
{
public:
    MappingGridComponent (NoiseMu6ixSamplerAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { dragMode = DragMode::None; }

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    int selectedZoneIdx = -1;
    int activeGroupFilter = -1;
    int targetDropGroup = 0;
    std::function<void()> onZoneChanged;

private:
    static constexpr int minNote = 24;
    static constexpr int maxNote = 96;
    enum class DragMode { None, Move, LeftEdge, RightEdge, TopEdge, BottomEdge };

    DragMode hitTestDragMode (const juce::Rectangle<float>& r, juce::Point<float> pt) const;
    juce::Rectangle<float> getZoneRect (const SampleZone& z) const;
    int xToNote (float x) const;
    int yToVel (float y) const;

    DragMode dragMode = DragMode::None;
    int dragStartNote = 60, dragStartVel = 64;
    int origLowK = 0, origHighK = 127, origLowV = 0, origHighV = 127, origRoot = 60;

    NoiseMu6ixSamplerAudioProcessor& processor;
};

// ==================== Wave Editor Component ====================
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

// ==================== Front-Panel Custom Knob/Slider Slot ====================
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

// ==================== Left File Browser List ====================
class FileBrowserListModel : public juce::ListBoxModel
{
public:
    juce::Array<juce::File> files;
    std::function<void (const juce::File&)> onFileDoubleClicked;

    int getNumRows() override { return files.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
};

// ==================== Scrollable Edit Rack Container ====================
class EditRackContent : public juce::Component
{
public:
    void paint (juce::Graphics& g) override;
};

// ==================== Main Editor ====================
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
    void updateModeVisibility();
    void refreshGroupUI();
    void refreshSelectedZoneUI();
    void refreshFXUI();
    void applyZoneBarToSelected();
    void applyGroupInspector();
    void applyFXToProcessor();
    void browseLocalFolder();
    void layoutScrollableRack (int width);

    NoiseMu6ixSamplerAudioProcessor& processor;
    std::unique_ptr<juce::FileChooser> fileChooser;

    bool inEditMode = true;
    bool customizeFrontUI = true;
    int selectedGroupIdx = 0;

    // Top Header Bar
    juce::TextButton modeToggleBtn { "Front Panel" };
    juce::TextButton saveInstBtn   { "Save .nm6i" };
    juce::TextButton loadInstBtn   { "Load .nm6i" };
    juce::TextButton loadFilesBtn  { "Load WAVs" };
    juce::TextButton loadFolderBtn { "Auto-Map" };
    juce::Label voicesLabel { {}, "Voices: 0" };

    // Left Sidebar: File Browser
    juce::TextButton browseFolderBtn { "Open Folder..." };
    juce::TextButton addSelectedFileBtn { "+ Add to Grid" };
    juce::Label folderPathLabel { {}, "Select sample folder" };
    FileBrowserListModel fileListModel;
    juce::ListBox fileListBox { "FileList", &fileListModel };

    // Scrollable Edit Mode Viewport (One-by-one vertical Kontakt Rack!)
    juce::Viewport editViewport;
    EditRackContent editRack;

    // --- Section 1: Group Editor (Individual Kontakt Cards + Unlimited RR ON/OFF) ---
    juce::TextButton addGroupBtn    { "+ Add Group" };
    juce::TextButton deleteGroupBtn { "- Delete" };
    juce::TextButton rrToggleBtn    { "Round Robin: ON (Unlimited)" };
    juce::ComboBox   rrModeBox;
    juce::ToggleButton filterByGroupBtn { "Selected Group Only" };
    GroupCardsComponent groupCards;

    juce::Label grpNameLabel;
    juce::ToggleButton groupSoloBtn { "Solo" };
    juce::ToggleButton groupMuteBtn { "Mute" };
    juce::Slider grpVolSlider, grpPanSlider, grpTuneSlider;
    juce::Label grpVolLbl { {}, "Vol dB" }, grpPanLbl { {}, "Pan" }, grpTuneLbl { {}, "Tune" };

    // --- Section 2: Mapping Editor (Easy Drag Grid) ---
    MappingGridComponent mappingGrid;
    juce::TextButton autoSpreadBtn { "Auto-Spread Keys" };
    juce::TextButton delZoneBtn    { "Del Zone" };
    juce::Label selZoneNameLbl     { {}, "Sample: (click a zone on grid)" };
    juce::Slider zRootSlider, zLowKSlider, zHighKSlider, zLowVSlider, zHighVSlider;
    juce::Slider zVolSlider, zPanSlider, zTuneSlider;
    juce::Label zRootLbl { {}, "Root" }, zLowKLbl { {}, "Low K" }, zHighKLbl { {}, "High K" },
                zLowVLbl { {}, "Low V" }, zHighVLbl { {}, "High V" },
                zVolLbl { {}, "Vol dB" }, zPanLbl { {}, "Pan" }, zTuneLbl { {}, "Tune" };

    // --- Section 3: Wave Editor (Syncs directly with Mapping Editor!) ---
    WaveEditorComponent waveEditor;
    juce::ToggleButton loopToggle { "Long / Loop" };
    juce::TextButton autoLoopBtn  { "Auto-Loop (Zero-Cross)" };
    juce::ComboBox waveDragModeBox;
    juce::Slider crossfadeSlider, zoomSlider;
    juce::Label cfLbl { {}, "Crossfade" }, zoomLbl { {}, "Zoom" };

    // --- Section 4: FX & ADSR Rack ---
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
