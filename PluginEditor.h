#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <functional>

// ==================== Logo Button (Click to open UI Size % Menu) ====================
class LogoSizeButton : public juce::Component
{
public:
    void paint (juce::Graphics& g) override;
    void mouseEnter (const juce::MouseEvent&) override { hovered = true; repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }
    void mouseDown  (const juce::MouseEvent&) override { if (onClick) onClick(); }

    std::function<void()> onClick;
private:
    bool hovered = false;
};

// ==================== Kontakt-Style Colored Piano Keyboard ====================
class ColoredMidiKeyboardComponent : public juce::MidiKeyboardComponent
{
public:
    ColoredMidiKeyboardComponent (NoiseMu6ixSamplerAudioProcessor& p, juce::MidiKeyboardState& state);
    int activeFilterGroupIdx = 0;

protected:
    void drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour noteFillColour) override;

private:
    bool getKeyColourInfo (int note, juce::Colour& outCol, bool& isKeyswitch) const;
    NoiseMu6ixSamplerAudioProcessor& processor;
};

// ==================== Kontakt-Style Individual Group Cards ====================
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

// ==================== Mapping Editor Grid ====================
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
    int activeGroupFilter = 0;
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

// ==================== Clean Performance Knob/Slider (Front Panel Only) ====================
class PerformanceMacroWidget : public juce::Component
{
public:
    PerformanceMacroWidget (NoiseMu6ixSamplerAudioProcessor& p, int slotIndex);
    void resized() override;
    void paint (juce::Graphics& g) override;
    void syncFromConfig();

private:
    NoiseMu6ixSamplerAudioProcessor& processor;
    int index;
    juce::Label nameLabel;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ==================== Draggable & Resizable Panel Editor Canvas ====================
class PanelDesignCanvas : public juce::Component
{
public:
    PanelDesignCanvas (NoiseMu6ixSamplerAudioProcessor& p);
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent&) override { dragOp = DragOp::None; }

    int selectedMacroIdx = 0;
    std::function<void (int)> onSelectionChanged;
    std::function<void()> onLayoutChanged;

private:
    enum class DragOp { None, Move, Resize };
    DragOp dragOp = DragOp::None;
    juce::Point<int> dragStartMouse;
    int origX = 0, origY = 0, origW = 100, origH = 120;

    NoiseMu6ixSamplerAudioProcessor& processor;
};

// ==================== Left Sidebar List ====================
class FileBrowserListModel : public juce::ListBoxModel
{
public:
    juce::Array<juce::File> files;
    bool isLibraryStyle = false;
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
    int groupHeaderY = -1;
    int mapHeaderY   = -1;
    int waveHeaderY  = -1;
    int fxHeaderY    = -1;
    int panelHeaderY = -1;
};

// ==================== Scalable Root Canvas ====================
class RootUICanvas : public juce::Component
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

    void resized() override;
    void timerCallback() override;

private:
    static constexpr int baseWidth  = 980;
    static constexpr int baseHeight = 620;

    void setUIScale (float newScale);
    void showScaleMenu();
    void updateModeVisibility();
    void updateSidebarVisibility();
    void refreshGroupUI();
    void refreshSelectedZoneUI();
    void refreshFXUI();
    void refreshPanelEditorInspector();
    void applyPanelEditorInspector();
    void syncFrontPanelWidgets();
    void applyZoneBarToSelected();
    void applyGroupInspector();
    void applyFXToProcessor();
    void browseLocalFolder();
    void addLibraryDialog();
    void loadSavedLibrariesList();
    void saveLibrariesList();
    juce::File getLibraryConfigFile() const;
    void layoutBaseUI();
    void layoutScrollableRack (int width);

    NoiseMu6ixSamplerAudioProcessor& processor;
    std::unique_ptr<juce::FileChooser> fileChooser;

    float uiScale = 1.0f;
    RootUICanvas rootCanvas;

    bool inEditMode = true;
    bool sidebarShowLibraries = true;
    int selectedGroupIdx = 0;

    // 5 Module Toggle States (ON/OFF in Edit Mode)
    bool showGroupSec = true;
    bool showMapSec   = true;
    bool showWaveSec  = false;
    bool showFXSec    = false;
    bool showPanelSec = false;

    // Top Header Bar
    LogoSizeButton logoBtn;
    juce::TextButton modeToggleBtn { "Front Panel" };
    juce::TextButton saveInstBtn   { "Save .nm6i" };
    juce::TextButton loadInstBtn   { "Load .nm6i" };
    juce::Label versionLabel       { {}, "v1.0.6" };
    juce::Label voicesLabel        { {}, "Voices: 0" };

    // Left Sidebar
    juce::TextButton sideLibTabBtn   { "Libraries" };
    juce::TextButton sideFilesTabBtn { "Files" };
    juce::TextButton addLibraryBtn    { "+ Add Library" };
    juce::TextButton removeLibraryBtn { "- Remove" };
    FileBrowserListModel libraryListModel;
    juce::ListBox libraryListBox { "LibraryList", &libraryListModel };
    juce::TextButton browseFolderBtn    { "Open Folder..." };
    juce::TextButton addSelectedFileBtn { "+ Add to Grid" };
    juce::Label folderPathLabel         { {}, "Select sample folder" };
    FileBrowserListModel fileListModel;
    juce::ListBox fileListBox { "FileList", &fileListModel };

    // 5 Module ON/OFF Toggle Buttons above Edit Rack
    juce::TextButton togGroupBtn { "Group Editor" };
    juce::TextButton togMapBtn   { "Map Editor" };
    juce::TextButton togWaveBtn  { "Wave Editor" };
    juce::TextButton togFXBtn    { "FX Editor" };
    juce::TextButton togPanelBtn { "Panel Editor" };

    // Scrollable Edit Mode Viewport
    juce::Viewport editViewport;
    EditRackContent editRack;

    // --- 1. Group Editor ---
    juce::TextButton addGroupBtn     { "+ Add Group" };
    juce::TextButton deleteGroupBtn  { "- Delete" };
    juce::TextButton rrToggleBtn     { "RR: ON (Unlimited)" };
    juce::ComboBox   rrModeBox;
    juce::TextButton ksMasterBtn     { "Keyswitch: OFF" };
    juce::ToggleButton showAllGroupsBtn { "Show All Groups" };
    GroupCardsComponent groupCards;
    juce::Label grpNameLabel;
    juce::ToggleButton groupSoloBtn { "Solo" };
    juce::ToggleButton groupMuteBtn { "Mute" };
    juce::Slider grpVolSlider, grpPanSlider, grpTuneSlider;
    juce::Label grpVolLbl { {}, "Vol" }, grpPanLbl { {}, "Pan" }, grpTuneLbl { {}, "Tune" };
    juce::ToggleButton grpKSEnabledBtn { "Group KS" };
    juce::Slider grpKSNoteSlider;
    juce::Label grpKSNoteLbl { {}, "KS Key" };
    juce::ComboBox grpKSColorBox;

    // --- 2. Mapping Editor ---
    MappingGridComponent mappingGrid;
    juce::TextButton autoSpreadBtn { "Auto-Spread" };
    juce::TextButton delZoneBtn    { "Del Zone" };
    juce::Label selZoneNameLbl     { {}, "Sample: (click a zone on grid)" };
    juce::Slider zRootSlider, zLowKSlider, zHighKSlider, zLowVSlider, zHighVSlider;
    juce::Slider zVolSlider, zPanSlider, zTuneSlider;
    juce::Label zRootLbl { {}, "Root" }, zLowKLbl { {}, "Low K" }, zHighKLbl { {}, "High K" },
                zLowVLbl { {}, "Low V" }, zHighVLbl { {}, "High V" },
                zVolLbl { {}, "Vol dB" }, zPanLbl { {}, "Pan" }, zTuneLbl { {}, "Tune" };

    // --- 3. Wave Editor ---
    WaveEditorComponent waveEditor;
    juce::ToggleButton loopToggle { "Long / Loop" };
    juce::TextButton autoLoopBtn  { "Auto-Loop (Zero-Cross)" };
    juce::ComboBox waveDragModeBox;
    juce::Slider crossfadeSlider, zoomSlider;
    juce::Label cfLbl { {}, "Crossfade" }, zoomLbl { {}, "Zoom" };

    // --- 4. FX & ADSR Rack ---
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

    // --- 5. Panel Editor (Inside Edit Mode Only!) ---
    juce::TextButton peAddKnobBtn   { "+ Knob" };
    juce::TextButton peAddSliderBtn { "+ Slider" };
    juce::TextButton peDelCtrlBtn   { "Delete" };
    juce::Label      peNameLbl      { {}, "Name:" };
    juce::TextEditor peNameEdit;
    juce::TextButton peStyleBtn     { "Knob" };
    juce::ComboBox   peTargetBox;
    juce::Label      peRangeLbl     { {}, "Range (%):" };
    juce::TextEditor peMinEdit;
    juce::Label      peToLbl        { {}, "to" };
    juce::TextEditor peMaxEdit;
    PanelDesignCanvas panelDesignCanvas;

    // --- Clean Front Panel (Performance Mode - NO Add/Customize buttons!) ---
    juce::Component frontPanelContainer;
    std::vector<std::unique_ptr<PerformanceMacroWidget>> perfMacros;

    // Bottom Piano Keyboard
    ColoredMidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoiseMu6ixSamplerAudioProcessorEditor)
};
