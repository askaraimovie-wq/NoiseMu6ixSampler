#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

// ==================== MappingGridComponent ====================
MappingGridComponent::MappingGridComponent (NoiseMu6ixSamplerAudioProcessor& p) : processor (p) {}

int MappingGridComponent::xToNote (float x) const
{
    float w = (float)std::max (1, getWidth());
    float frac = juce::jlimit (0.0f, 1.0f, x / w);
    return juce::jlimit (minNote, maxNote, minNote + (int)std::round (frac * (maxNote - minNote)));
}

int MappingGridComponent::yToVel (float y) const
{
    float h = (float)std::max (1, getHeight());
    float frac = juce::jlimit (0.0f, 1.0f, 1.0f - (y / h));
    return juce::jlimit (0, 127, (int)std::round (frac * 127.0f));
}

juce::Rectangle<float> MappingGridComponent::getZoneRect (const SampleZone& z) const
{
    float w = (float)std::max (1, getWidth());
    float h = (float)std::max (1, getHeight());
    float noteSpan = (float)(maxNote - minNote + 1);

    float x1 = ((float)(juce::jlimit (minNote, maxNote, z.lowKey) - minNote) / noteSpan) * w;
    float x2 = ((float)(juce::jlimit (minNote, maxNote, z.highKey) - minNote + 1) / noteSpan) * w;
    float yTop = (1.0f - (float)z.highVel / 127.0f) * h;
    float yBot = (1.0f - (float)z.lowVel / 127.0f) * h;
    return { x1, yTop, std::max (6.0f, x2 - x1), std::max (8.0f, yBot - yTop) };
}

void MappingGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2b2b2b));
    float w = (float)getWidth();
    float h = (float)getHeight();
    int totalNotes = maxNote - minNote + 1;
    float keyW = w / (float)totalNotes;

    for (int n = minNote; n <= maxNote; ++n)
    {
        float x = (float)(n - minNote) * keyW;
        int pc = n % 12;
        if (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10)
        {
            g.setColour (juce::Colour (0xff232323));
            g.fillRect (x, 0.0f, keyW, h);
        }
        g.setColour (pc == 0 ? juce::Colour (0xff555555) : juce::Colour (0xff383838));
        g.drawVerticalLine ((int)x, 0.0f, h);
    }

    g.setColour (juce::Colour (0xff404040));
    for (int v : { 32, 64, 96 })
        g.drawHorizontalLine ((int)((1.0f - (float)v / 127.0f) * h), 0.0f, w);

    const juce::ScopedLock sl (processor.dataLock);
    if (processor.zones.empty())
    {
        g.setColour (juce::Colour (0xffaaaaaa));
        g.setFont (14.0f);
        g.drawFittedText ("Drag & Drop WAV Samples Here (or Load .nm6i Monolith Instrument)",
                          getLocalBounds(), juce::Justification::centred, 2);
        return;
    }

    static const juce::uint32 palette[6] = { 0xffd9a05b, 0xff5ba8d9, 0xff7cc47f, 0xffc97575, 0xffa77fd4, 0xffd4c25b };
    for (size_t i = 0; i < processor.zones.size(); ++i)
    {
        const auto& z = processor.zones[i];
        if (activeGroupFilter >= 0 && z->groupIndex != activeGroupFilter) continue;

        auto rect = getZoneRect (*z);
        bool isSel = ((int)i == selectedZoneIdx);
        auto col = juce::Colour (palette[(size_t)(std::max (0, z->groupIndex)) % 6]);

        g.setColour (col.withAlpha (isSel ? 0.85f : 0.48f));
        g.fillRect (rect);
        g.setColour (isSel ? juce::Colours::white : col.brighter (0.3f));
        g.drawRect (rect, isSel ? 2.0f : 1.0f);

        float rootX = ((float)(juce::jlimit (minNote, maxNote, z->rootNote) - minNote) + 0.5f) * keyW;
        g.setColour (juce::Colours::yellow);
        g.fillEllipse (rootX - 3.0f, rect.getBottom() - 8.0f, 6.0f, 6.0f);

        if (rect.getWidth() > 36 && rect.getHeight() > 14)
        {
            g.setColour (juce::Colours::black);
            g.setFont (11.0f);
            g.drawFittedText (z->name, rect.toNearestInt().reduced (3, 1), juce::Justification::topLeft, 1);
        }
    }
}

void MappingGridComponent::mouseDown (const juce::MouseEvent& e)
{
    const juce::ScopedLock sl (processor.dataLock);
    int hitIdx = -1;
    juce::Rectangle<float> hitRect;
    for (int i = (int)processor.zones.size() - 1; i >= 0; --i)
    {
        const auto& z = processor.zones[(size_t)i];
        if (activeGroupFilter >= 0 && z->groupIndex != activeGroupFilter) continue;
        auto r = getZoneRect (*z);
        if (r.contains (e.position)) { hitIdx = i; hitRect = r; break; }
    }

    selectedZoneIdx = hitIdx;
    dragMode = DragMode::None;
    if (hitIdx >= 0)
    {
        auto& z = processor.zones[(size_t)hitIdx];
        origLowK = z->lowKey; origHighK = z->highKey;
        origLowV = z->lowVel; origHighV = z->highVel; origRoot = z->rootNote;
        dragStartNote = xToNote (e.position.x);
        dragStartVel  = yToVel (e.position.y);

        float edge = 8.0f;
        if (std::abs (e.position.x - hitRect.getX()) < edge)           dragMode = DragMode::LeftEdge;
        else if (std::abs (e.position.x - hitRect.getRight()) < edge)  dragMode = DragMode::RightEdge;
        else if (std::abs (e.position.y - hitRect.getY()) < edge)      dragMode = DragMode::TopEdge;
        else if (std::abs (e.position.y - hitRect.getBottom()) < edge) dragMode = DragMode::BottomEdge;
        else                                                           dragMode = DragMode::Move;
    }
    repaint();
    if (onZoneChanged) onZoneChanged();
}

void MappingGridComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (selectedZoneIdx < 0 || dragMode == DragMode::None) return;
    const juce::ScopedLock sl (processor.dataLock);
    if (selectedZoneIdx >= (int)processor.zones.size()) return;

    auto& z = processor.zones[(size_t)selectedZoneIdx];
    int curNote = xToNote (e.position.x);
    int curVel  = yToVel (e.position.y);
    int dNote   = curNote - dragStartNote;
    int dVel    = curVel - dragStartVel;

    switch (dragMode)
    {
        case DragMode::LeftEdge:   z->lowKey  = juce::jlimit (0, z->highKey, curNote); break;
        case DragMode::RightEdge:  z->highKey = juce::jlimit (z->lowKey, 127, curNote); break;
        case DragMode::TopEdge:    z->highVel = juce::jlimit (z->lowVel + 1, 127, curVel); break;
        case DragMode::BottomEdge: z->lowVel  = juce::jlimit (0, z->highVel - 1, curVel); break;
        case DragMode::Move:
        {
            int spanK = origHighK - origLowK;
            int newLowK = juce::jlimit (0, 127 - spanK, origLowK + dNote);
            z->lowKey = newLowK; z->highKey = newLowK + spanK;
            z->rootNote = juce::jlimit (0, 127, origRoot + (newLowK - origLowK));

            int spanV = origHighV - origLowV;
            int newLowV = juce::jlimit (0, 127 - spanV, origLowV + dVel);
            z->lowVel = newLowV; z->highVel = newLowV + spanV;
            break;
        }
        default: break;
    }
    repaint();
    if (onZoneChanged) onZoneChanged();
}

bool MappingGridComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".nm6i"))
            return true;
    return false;
}

void MappingGridComponent::filesDropped (const juce::StringArray& files, int x, int)
{
    int dropNote = xToNote ((float)x);
    int grp = (activeGroupFilter >= 0) ? activeGroupFilter : 0;
    for (auto& f : files)
    {
        if (f.endsWithIgnoreCase (".nm6i"))
            processor.loadMonolithInstrument (juce::File (f));
        else
            processor.loadSampleFile (juce::File (f), grp, files.size() > 1, files.size() == 1 ? dropNote : -1);
    }
    const juce::ScopedLock sl (processor.dataLock);
    if (!processor.zones.empty()) selectedZoneIdx = (int)processor.zones.size() - 1;
    repaint();
    if (onZoneChanged) onZoneChanged();
}

// ==================== WaveEditorComponent ====================
void WaveEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1c20));
    g.setColour (juce::Colour (0xff4a4e57));
    g.drawRect (getLocalBounds(), 1);

    if (!zone || zone->buffer.getNumSamples() == 0)
    {
        g.setColour (juce::Colours::lightgrey);
        g.setFont (14.0f);
        g.drawFittedText ("Select a Sample Zone in Mapping Editor to edit Waveform & Auto-Loop",
                          getLocalBounds(), juce::Justification::centred, 1);
        return;
    }

    auto b = getLocalBounds().reduced (4);
    int w = b.getWidth(), h = b.getHeight();
    int numSamples = zone->buffer.getNumSamples();
    int visibleSamples = std::max (256, (int)((float)numSamples / zoomFactor));
    const float* data = zone->buffer.getReadPointer (0);

    auto sampleToX = [&](int s) -> float {
        return (float)b.getX() + ((float)s / (float)visibleSamples) * (float)w;
    };

    float xSStart = sampleToX (zone->sampleStart.load());
    float xSEnd   = sampleToX (zone->sampleEnd.load());
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    if (xSStart > b.getX())   g.fillRect ((float)b.getX(), (float)b.getY(), xSStart - b.getX(), (float)h);
    if (xSEnd < b.getRight()) g.fillRect (xSEnd, (float)b.getY(), b.getRight() - xSEnd, (float)h);

    if (zone->isLooping.load())
    {
        float xLStart = sampleToX (zone->loopStart.load());
        float xLEnd   = sampleToX (zone->loopEnd.load());
        g.setColour (juce::Colour (0x3300e5ff));
        g.fillRect (xLStart, (float)b.getY(), std::max (2.0f, xLEnd - xLStart), (float)h);
        g.setColour (juce::Colour (0xff00e5ff)); g.drawVerticalLine ((int)xLStart, (float)b.getY(), (float)b.getBottom());
        g.setColour (juce::Colour (0xffff5555)); g.drawVerticalLine ((int)xLEnd, (float)b.getY(), (float)b.getBottom());
    }

    g.setColour (juce::Colour (0xffd9a05b));
    float midY = (float)b.getCentreY();
    int step = std::max (1, visibleSamples / std::max (1, w));
    for (int x = 0; x < w; ++x)
    {
        int idx = x * step;
        if (idx >= numSamples) break;
        float peak = 0.0f;
        for (int k = 0; k < step && idx + k < numSamples; ++k)
            peak = std::max (peak, std::abs (data[idx + k]));
        float barH = peak * (h * 0.45f);
        g.drawVerticalLine (b.getX() + x, midY - barH, midY + barH);
    }

    g.setColour (juce::Colours::limegreen); g.drawVerticalLine ((int)xSStart, (float)b.getY(), (float)b.getBottom());
    g.setColour (juce::Colours::orange);    g.drawVerticalLine ((int)xSEnd,   (float)b.getY(), (float)b.getBottom());
}

void WaveEditorComponent::mouseDown (const juce::MouseEvent& e)
{
    if (!zone || zone->buffer.getNumSamples() == 0) return;
    int numSamples = zone->buffer.getNumSamples();
    int visibleSamples = std::max (256, (int)((float)numSamples / zoomFactor));
    int clicked = (int)(((float)e.x / (float)std::max (1, getWidth())) * visibleSamples);

    if (editingTrim)
        draggingFirst = (std::abs (clicked - zone->sampleStart.load()) <= std::abs (clicked - zone->sampleEnd.load()));
    else
    {
        draggingFirst = (std::abs (clicked - zone->loopStart.load()) <= std::abs (clicked - zone->loopEnd.load()));
        zone->isLooping.store (true);
    }
    mouseDrag (e);
}

void WaveEditorComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (!zone || zone->buffer.getNumSamples() == 0) return;
    int numSamples = zone->buffer.getNumSamples();
    int visibleSamples = std::max (256, (int)((float)numSamples / zoomFactor));
    int raw = juce::jlimit (0, numSamples - 1, (int)(((float)e.x / (float)std::max (1, getWidth())) * visibleSamples));
    int snapped = NoiseMu6ixSamplerAudioProcessor::findNearestZeroCrossing (zone->buffer, raw);

    if (editingTrim)
    {
        if (draggingFirst) { if (snapped < zone->sampleEnd.load() - 64) zone->sampleStart.store (snapped); }
        else               { if (snapped > zone->sampleStart.load() + 64) zone->sampleEnd.store (snapped); }
    }
    else
    {
        if (draggingFirst) { if (snapped < zone->loopEnd.load() - 64) zone->loopStart.store (snapped); }
        else               { if (snapped > zone->loopStart.load() + 64) zone->loopEnd.store (snapped); }
    }
    repaint();
    if (onWaveChanged) onWaveChanged();
}

// ==================== CustomMacroSlot ====================
CustomMacroSlot::CustomMacroSlot (NoiseMu6ixSamplerAudioProcessor& p, int slotIndex)
    : processor (p), index (slotIndex)
{
    auto& cfg = processor.macroConfigs[index];
    nameLabel.setText (cfg.name, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setEditable (true);
    nameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff2b2f38));
    nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    nameLabel.onTextChange = [this] { processor.macroConfigs[index].name = nameLabel.getText(); };
    addAndMakeVisible (nameLabel);

    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 48, 16);
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffd9a05b));
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffd9a05b));
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "macro" + juce::String (index), slider);

    styleToggle.onClick = [this] {
        processor.macroConfigs[index].isSlider = !processor.macroConfigs[index].isSlider;
        refreshStyle();
    };
    addAndMakeVisible (styleToggle);

    hideBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5c2b2b));
    hideBtn.onClick = [this] {
        processor.macroConfigs[index].visible = false;
        if (onVisibilityChanged) onVisibilityChanged();
    };
    addAndMakeVisible (hideBtn);

    targetBox.addItem ("None",       (int)MacroTarget::None);
    targetBox.addItem ("Dynamics",   (int)MacroTarget::Dynamics);
    targetBox.addItem ("Expression", (int)MacroTarget::Expression);
    targetBox.addItem ("Cutoff",     (int)MacroTarget::FilterCutoff);
    targetBox.addItem ("Resonance",  (int)MacroTarget::FilterRes);
    targetBox.addItem ("Reverb Mix", (int)MacroTarget::ReverbMix);
    targetBox.addItem ("Reverb Size",(int)MacroTarget::ReverbSize);
    targetBox.addItem ("Delay Mix",  (int)MacroTarget::DelayMix);
    targetBox.addItem ("Chorus Mix", (int)MacroTarget::ChorusMix);
    targetBox.addItem ("Drive",      (int)MacroTarget::Drive);
    targetBox.addItem ("Attack",     (int)MacroTarget::Attack);
    targetBox.addItem ("Decay",      (int)MacroTarget::Decay);
    targetBox.addItem ("Sustain",    (int)MacroTarget::Sustain);
    targetBox.addItem ("Release",    (int)MacroTarget::Release);
    targetBox.addItem ("Pitch",      (int)MacroTarget::Pitch);
    targetBox.addItem ("Pan",        (int)MacroTarget::Pan);
    targetBox.setSelectedId (cfg.target, juce::dontSendNotification);
    targetBox.onChange = [this] { processor.macroConfigs[index].target = targetBox.getSelectedId(); };
    addAndMakeVisible (targetBox);
    refreshStyle();
}

void CustomMacroSlot::setCustomizeMode (bool on)
{
    customizeMode = on;
    styleToggle.setVisible (on);
    targetBox.setVisible (on);
    hideBtn.setVisible (on);
    resized();
}

void CustomMacroSlot::refreshStyle()
{
    bool isSl = processor.macroConfigs[index].isSlider;
    slider.setSliderStyle (isSl ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    styleToggle.setButtonText (isSl ? "Slider" : "Knob");
    repaint();
}

void CustomMacroSlot::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff23262c));
    g.setColour (juce::Colour (0xff484d57));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.2f);
}

void CustomMacroSlot::resized()
{
    auto r = getLocalBounds().reduced (6);
    auto topRow = r.removeFromTop (22);
    if (customizeMode) { hideBtn.setBounds (topRow.removeFromRight (22)); topRow.removeFromRight (3); }
    nameLabel.setBounds (topRow);
    r.removeFromTop (4);
    if (customizeMode)
    {
        targetBox.setBounds (r.removeFromBottom (22)); r.removeFromBottom (4);
        styleToggle.setBounds (r.removeFromBottom (20)); r.removeFromBottom (4);
    }
    slider.setBounds (r);
}

// ==================== FileBrowserListModel ====================
void FileBrowserListModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= files.size()) return;
    g.fillAll (selected ? juce::Colour (0xffd9a05b).withAlpha (0.35f) : juce::Colour (0xff24262b));
    g.setColour (selected ? juce::Colours::white : juce::Colour (0xffcccccc));
    g.setFont (12.0f);
    g.drawText (files[row].getFileName(), 6, 0, w - 12, h, juce::Justification::centredLeft, true);
}

void FileBrowserListModel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < files.size() && onFileDoubleClicked)
        onFileDoubleClicked (files[row]);
}

// ==================== Main Editor ====================
NoiseMu6ixSamplerAudioProcessorEditor::NoiseMu6ixSamplerAudioProcessorEditor (NoiseMu6ixSamplerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), mappingGrid (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize (1160, 710);

    // Header
    modeToggleBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffb86b28));
    modeToggleBtn.onClick = [this] {
        inEditMode = !inEditMode;
        modeToggleBtn.setButtonText (inEditMode ? "Front Panel" : "Edit Mode");
        updateModeVisibility();
    };
    addAndMakeVisible (modeToggleBtn);

    saveInstBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b5c3f));
    saveInstBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Save Compressed Instrument (.nm6i)", juce::File{}, "*.nm6i");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f != juce::File{})
                {
                    if (!f.hasFileExtension (".nm6i")) f = f.withFileExtension (".nm6i");
                    processor.saveMonolithInstrument (f);
                }
            });
    };
    addAndMakeVisible (saveInstBtn);

    loadInstBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b475c));
    loadInstBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Load Compressed Instrument (.nm6i)", juce::File{}, "*.nm6i");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                if (processor.loadMonolithInstrument (fc.getResult()))
                {
                    refreshGroupList();
                    refreshSelectedZoneUI();
                    refreshFXUI();
                    mappingGrid.repaint();
                }
            });
    };
    addAndMakeVisible (loadInstBtn);

    loadFilesBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Select WAV Samples", juce::File{}, "*.wav;*.flac;*.aif");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems,
            [this](const juce::FileChooser& fc) {
                for (auto& f : fc.getResults()) processor.loadSampleFile (f, selectedGroupIdx, true, -1);
                refreshGroupList(); refreshSelectedZoneUI(); mappingGrid.repaint();
            });
    };
    addAndMakeVisible (loadFilesBtn);

    loadFolderBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Select Sample Folder to Auto-Map");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc) {
                processor.loadSampleFolder (fc.getResult(), selectedGroupIdx);
                refreshGroupList(); refreshSelectedZoneUI(); mappingGrid.repaint();
            });
    };
    addAndMakeVisible (loadFolderBtn);

    rrModeBox.addItem ("RR: Cycle", 1);
    rrModeBox.addItem ("RR: No-Repeat Random", 2);
    rrModeBox.addItem ("RR: True Random", 3);
    rrModeBox.setSelectedId (processor.rrMode.load() + 1, juce::dontSendNotification);
    rrModeBox.onChange = [this] { processor.rrMode.store (rrModeBox.getSelectedId() - 1); };
    addAndMakeVisible (rrModeBox);

    voicesLabel.setJustificationType (juce::Justification::centredRight);
    voicesLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd9a05b));
    addAndMakeVisible (voicesLabel);

    // Left Sidebar
    browseFolderBtn.onClick = [this] { browseLocalFolder(); };
    addAndMakeVisible (browseFolderBtn);

    addSelectedFileBtn.onClick = [this] {
        int row = fileListBox.getSelectedRow();
        if (row >= 0 && row < fileListModel.files.size())
        {
            auto f = fileListModel.files[row];
            if (f.hasFileExtension (".nm6i"))
                processor.loadMonolithInstrument (f);
            else
                processor.loadSampleFile (f, selectedGroupIdx, true, -1);
            const juce::ScopedLock sl (processor.dataLock);
            mappingGrid.selectedZoneIdx = (int)processor.zones.size() - 1;
            refreshGroupList(); refreshSelectedZoneUI(); refreshFXUI(); mappingGrid.repaint();
        }
    };
    addAndMakeVisible (addSelectedFileBtn);
    folderPathLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    folderPathLabel.setFont (11.5f);
    addAndMakeVisible (folderPathLabel);

    fileListModel.onFileDoubleClicked = [this](const juce::File& f) {
        if (f.hasFileExtension (".nm6i")) processor.loadMonolithInstrument (f);
        else processor.loadSampleFile (f, selectedGroupIdx, true, -1);
        const juce::ScopedLock sl (processor.dataLock);
        mappingGrid.selectedZoneIdx = (int)processor.zones.size() - 1;
        refreshGroupList(); refreshSelectedZoneUI(); refreshFXUI(); mappingGrid.repaint();
    };
    fileListBox.setRowHeight (22);
    fileListBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1f2126));
    addAndMakeVisible (fileListBox);

    // Sub-Tabs
    tabGroupBtn.onClick   = [this] { currentTab = EditTab::Group;   updateModeVisibility(); };
    tabMappingBtn.onClick = [this] { currentTab = EditTab::Mapping; updateModeVisibility(); };
    tabWaveBtn.onClick    = [this] { currentTab = EditTab::Wave;    updateModeVisibility(); };
    tabFXBtn.onClick      = [this] { currentTab = EditTab::FX;      updateModeVisibility(); };
    addAndMakeVisible (tabGroupBtn);
    addAndMakeVisible (tabMappingBtn);
    addAndMakeVisible (tabWaveBtn);
    addAndMakeVisible (tabFXBtn);

    // Group Editor
    groupSelector.onChange = [this] {
        selectedGroupIdx = std::max (0, groupSelector.getSelectedItemIndex());
        if (filterByGroupBtn.getToggleState()) mappingGrid.activeGroupFilter = selectedGroupIdx;
        refreshGroupList(); mappingGrid.repaint();
    };
    addAndMakeVisible (groupSelector);
    createGroupBtn.onClick = [this] { selectedGroupIdx = processor.addGroup(); refreshGroupList(); };
    deleteGroupBtn.onClick = [this] { processor.removeGroup (selectedGroupIdx); selectedGroupIdx = std::max (0, selectedGroupIdx - 1); refreshGroupList(); mappingGrid.repaint(); };
    groupSoloBtn.onClick = [this] { applyGroupInspector(); };
    groupMuteBtn.onClick = [this] { applyGroupInspector(); };
    filterByGroupBtn.onClick = [this] { mappingGrid.activeGroupFilter = filterByGroupBtn.getToggleState() ? selectedGroupIdx : -1; mappingGrid.repaint(); };
    addAndMakeVisible (createGroupBtn); addAndMakeVisible (deleteGroupBtn);
    addAndMakeVisible (groupSoloBtn); addAndMakeVisible (groupMuteBtn); addAndMakeVisible (filterByGroupBtn);

    auto setupHSlider = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV, std::function<void()> cb) {
        s.setRange (minV, maxV, step);
        s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
        s.onValueChange = std::move (cb);
        addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (l);
    };
    setupHSlider (grpRRSlider,   grpRRLbl,   1, 10, 1, 1,       [this]{ applyGroupInspector(); });
    setupHSlider (grpVolSlider,  grpVolLbl,  -24, 12, 0.1, 0.0, [this]{ applyGroupInspector(); });
    setupHSlider (grpPanSlider,  grpPanLbl,  -1, 1, 0.01, 0.0,  [this]{ applyGroupInspector(); });
    setupHSlider (grpTuneSlider, grpTuneLbl, -24, 24, 0.1, 0.0, [this]{ applyGroupInspector(); });

    // Mapping Editor
    mappingGrid.onZoneChanged = [this] { refreshSelectedZoneUI(); };
    addAndMakeVisible (mappingGrid);
    autoSpreadBtn.onClick = [this] { processor.autoSpreadKeyRanges(); mappingGrid.repaint(); refreshSelectedZoneUI(); };
    delZoneBtn.onClick = [this] {
        if (mappingGrid.selectedZoneIdx >= 0) { processor.removeZone (mappingGrid.selectedZoneIdx); mappingGrid.selectedZoneIdx = -1; mappingGrid.repaint(); refreshSelectedZoneUI(); }
    };
    addAndMakeVisible (autoSpreadBtn); addAndMakeVisible (delZoneBtn); addAndMakeVisible (selZoneNameLbl);

    auto setupZoneNum = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV) {
        s.setRange (minV, maxV, step); s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 20);
        s.onValueChange = [this] { applyZoneBarToSelected(); };
        addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centred); l.setFont (11.0f);
        addAndMakeVisible (l);
    };
    setupZoneNum (zRootSlider,  zRootLbl,  0, 127, 1, 60);
    setupZoneNum (zLowKSlider,  zLowKLbl,  0, 127, 1, 48);
    setupZoneNum (zHighKSlider, zHighKLbl, 0, 127, 1, 72);
    setupZoneNum (zLowVSlider,  zLowVLbl,  0, 127, 1, 0);
    setupZoneNum (zHighVSlider, zHighVLbl, 0, 127, 1, 127);
    setupZoneNum (zVolSlider,   zVolLbl,   -24, 12, 0.5, 0);
    setupZoneNum (zPanSlider,   zPanLbl,   -1, 1, 0.05, 0);
    setupZoneNum (zTuneSlider,  zTuneLbl,  -24, 24, 0.5, 0);

    // Wave Editor
    addAndMakeVisible (waveEditor);
    addAndMakeVisible (loopToggle);
    loopToggle.onClick = [this] {
        const juce::ScopedLock sl (processor.dataLock);
        int idx = mappingGrid.selectedZoneIdx;
        if (idx >= 0 && idx < (int)processor.zones.size()) { processor.zones[(size_t)idx]->isLooping.store (loopToggle.getToggleState()); waveEditor.repaint(); }
    };
    autoLoopBtn.onClick = [this] {
        const juce::ScopedLock sl (processor.dataLock);
        int idx = mappingGrid.selectedZoneIdx;
        if (idx >= 0 && idx < (int)processor.zones.size())
        {
            auto z = processor.zones[(size_t)idx];
            z->isLooping.store (true);
            processor.autoDetectLoop (z);
            loopToggle.setToggleState (true, juce::dontSendNotification);
            crossfadeSlider.setValue (z->crossfadeSamples.load(), juce::dontSendNotification);
            waveEditor.repaint();
        }
    };
    addAndMakeVisible (autoLoopBtn);
    waveDragModeBox.addItem ("Drag: Loop Start/End", 1);
    waveDragModeBox.addItem ("Drag: Sample Start/End", 2);
    waveDragModeBox.setSelectedId (1, juce::dontSendNotification);
    waveDragModeBox.onChange = [this] { waveEditor.setEditMode (waveDragModeBox.getSelectedId() == 2); };
    addAndMakeVisible (waveDragModeBox);

    setupHSlider (crossfadeSlider, cfLbl, 64, 15000, 1, 2048, [this] {
        const juce::ScopedLock sl (processor.dataLock);
        int idx = mappingGrid.selectedZoneIdx;
        if (idx >= 0 && idx < (int)processor.zones.size())
            processor.zones[(size_t)idx]->crossfadeSamples.store ((int)crossfadeSlider.getValue());
    });
    setupHSlider (zoomSlider, zoomLbl, 1.0, 16.0, 0.1, 1.0, [this] { waveEditor.setZoom ((float)zoomSlider.getValue()); });

    // FX & ADSR Rack
    auto setupFXToggle = [this](juce::ToggleButton& b) {
        b.onClick = [this] { applyFXToProcessor(); };
        addAndMakeVisible (b);
    };
    setupFXToggle (fxEqOn); setupFXToggle (fxFltOn); setupFXToggle (fxDrvOn);
    setupFXToggle (fxChoOn); setupFXToggle (fxDlyOn); setupFXToggle (fxRevOn);

    fxFltTypeBox.addItem ("Lowpass", 1);
    fxFltTypeBox.addItem ("Highpass", 2);
    fxFltTypeBox.addItem ("Bandpass", 3);
    fxFltTypeBox.setSelectedId (1, juce::dontSendNotification);
    fxFltTypeBox.onChange = [this] { applyFXToProcessor(); };
    addAndMakeVisible (fxFltTypeBox);

    auto setupFXKnob = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV) {
        s.setRange (minV, maxV, step);
        s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 46, 15);
        s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffd9a05b));
        s.setColour (juce::Slider::thumbColourId, juce::Colour (0xffd9a05b));
        s.onValueChange = [this] { applyFXToProcessor(); };
        addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (11.0f);
        addAndMakeVisible (l);
    };

    setupFXKnob (fxEqLow,   lEqLow,   -12, 12, 0.1, 0.0);
    setupFXKnob (fxEqMid,   lEqMid,   -12, 12, 0.1, 0.0);
    setupFXKnob (fxEqHigh,  lEqHigh,  -12, 12, 0.1, 0.0);
    setupFXKnob (fxFltCut,  lFltCut,  0.0, 1.0, 0.01, 1.0);
    setupFXKnob (fxFltRes,  lFltRes,  0.0, 1.0, 0.01, 0.1);
    setupFXKnob (fxDrvAmt,  lDrvAmt,  0.0, 1.0, 0.01, 0.0);
    setupFXKnob (fxChoRate, lChoRate, 0.1, 6.0, 0.05, 1.2);
    setupFXKnob (fxChoDepth,lChoDepth,0.0, 1.0, 0.01, 0.25);
    setupFXKnob (fxChoMix,  lChoMix,  0.0, 1.0, 0.01, 0.35);
    setupFXKnob (fxDlyTime, lDlyTime, 30, 1200, 1, 320);
    setupFXKnob (fxDlyFb,   lDlyFb,   0.0, 0.85, 0.01, 0.35);
    setupFXKnob (fxDlyMix,  lDlyMix,  0.0, 1.0, 0.01, 0.25);
    setupFXKnob (fxRevSize, lRevSize, 0.0, 1.0, 0.01, 0.65);
    setupFXKnob (fxRevDamp, lRevDamp, 0.0, 1.0, 0.01, 0.45);
    setupFXKnob (fxRevMix,  lRevMix,  0.0, 1.0, 0.01, 0.15);
    setupFXKnob (fxAtk,     lAtk,     0.0, 1.0, 0.01, 0.02);
    setupFXKnob (fxDec,     lDec,     0.0, 1.0, 0.01, 0.25);
    setupFXKnob (fxSus,     lSus,     0.0, 1.0, 0.01, 1.0);
    setupFXKnob (fxRel,     lRel,     0.0, 1.0, 0.01, 0.25);

    // Front Panel
    addMacroBtn.onClick = [this] {
        for (int i = 0; i < NoiseMu6ixSamplerAudioProcessor::numMacros; ++i)
            if (!processor.macroConfigs[i].visible) { processor.macroConfigs[i].visible = true; updateModeVisibility(); break; }
    };
    addAndMakeVisible (addMacroBtn);

    toggleCustomizeBtn.onClick = [this] {
        customizeFrontUI = !customizeFrontUI;
        toggleCustomizeBtn.setButtonText (customizeFrontUI ? "Done Customizing" : "Customize Controls");
        for (auto& slot : macroSlots) slot->setCustomizeMode (customizeFrontUI);
        resized();
    };
    addAndMakeVisible (toggleCustomizeBtn);

    for (int i = 0; i < NoiseMu6ixSamplerAudioProcessor::numMacros; ++i)
    {
        auto slot = std::make_unique<CustomMacroSlot> (processor, i);
        slot->setCustomizeMode (customizeFrontUI);
        slot->onVisibilityChanged = [this] { updateModeVisibility(); };
        addAndMakeVisible (*slot);
        macroSlots.push_back (std::move (slot));
    }

    keyboard.setAvailableRange (24, 96);
    addAndMakeVisible (keyboard);

    refreshGroupList();
    refreshFXUI();
    updateModeVisibility();
    startTimerHz (10);
}

void NoiseMu6ixSamplerAudioProcessorEditor::timerCallback()
{
    voicesLabel.setText ("Voices: " + juce::String (processor.activeVoicesCount.load()), juce::dontSendNotification);
}

void NoiseMu6ixSamplerAudioProcessorEditor::browseLocalFolder()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Browse Sample / .nm6i Folder");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc) {
            auto dir = fc.getResult();
            if (dir.isDirectory())
            {
                folderPathLabel.setText (dir.getFileName(), juce::dontSendNotification);
                fileListModel.files = dir.findChildFiles (juce::File::findFiles, false, "*.wav;*.flac;*.aif;*.nm6i");
                fileListModel.files.sort();
                fileListBox.updateContent();
                fileListBox.repaint();
            }
        });
}

void NoiseMu6ixSamplerAudioProcessorEditor::refreshGroupList()
{
    const juce::ScopedLock sl (processor.dataLock);
    groupSelector.clear (juce::dontSendNotification);
    for (size_t i = 0; i < processor.groups.size(); ++i)
        groupSelector.addItem (processor.groups[i].name + " [RR" + juce::String (processor.groups[i].rrGroup) + "]", (int)i + 1);
    selectedGroupIdx = juce::jlimit (0, std::max (0, (int)processor.groups.size() - 1), selectedGroupIdx);
    groupSelector.setSelectedItemIndex (selectedGroupIdx, juce::dontSendNotification);

    if (selectedGroupIdx < (int)processor.groups.size())
    {
        const auto& g = processor.groups[(size_t)selectedGroupIdx];
        groupSoloBtn.setToggleState (g.solo, juce::dontSendNotification);
        groupMuteBtn.setToggleState (g.mute, juce::dontSendNotification);
        grpRRSlider.setValue (g.rrGroup, juce::dontSendNotification);
        grpVolSlider.setValue (g.volumeDb, juce::dontSendNotification);
        grpPanSlider.setValue (g.pan, juce::dontSendNotification);
        grpTuneSlider.setValue (g.tuneSemi, juce::dontSendNotification);
    }
}

void NoiseMu6ixSamplerAudioProcessorEditor::applyGroupInspector()
{
    const juce::ScopedLock sl (processor.dataLock);
    if (selectedGroupIdx < 0 || selectedGroupIdx >= (int)processor.groups.size()) return;
    auto& g = processor.groups[(size_t)selectedGroupIdx];
    g.solo     = groupSoloBtn.getToggleState();
    g.mute     = groupMuteBtn.getToggleState();
    g.rrGroup  = (int)grpRRSlider.getValue();
    g.volumeDb = (float)grpVolSlider.getValue();
    g.pan      = (float)grpPanSlider.getValue();
    g.tuneSemi = (float)grpTuneSlider.getValue();
}

void NoiseMu6ixSamplerAudioProcessorEditor::refreshSelectedZoneUI()
{
    const juce::ScopedLock sl (processor.dataLock);
    int idx = mappingGrid.selectedZoneIdx;
    if (idx < 0 || idx >= (int)processor.zones.size())
    {
        selZoneNameLbl.setText ("Sample: (none selected)", juce::dontSendNotification);
        waveEditor.setZone (nullptr);
        return;
    }
    auto z = processor.zones[(size_t)idx];
    selZoneNameLbl.setText ("Sample: " + z->name, juce::dontSendNotification);
    zRootSlider.setValue (z->rootNote, juce::dontSendNotification);
    zLowKSlider.setValue (z->lowKey, juce::dontSendNotification);
    zHighKSlider.setValue (z->highKey, juce::dontSendNotification);
    zLowVSlider.setValue (z->lowVel, juce::dontSendNotification);
    zHighVSlider.setValue (z->highVel, juce::dontSendNotification);
    zVolSlider.setValue (z->volumeDb, juce::dontSendNotification);
    zPanSlider.setValue (z->pan, juce::dontSendNotification);
    zTuneSlider.setValue (z->tuneSemi, juce::dontSendNotification);
    waveEditor.setZone (z);
    loopToggle.setToggleState (z->isLooping.load(), juce::dontSendNotification);
    crossfadeSlider.setValue (z->crossfadeSamples.load(), juce::dontSendNotification);
}

void NoiseMu6ixSamplerAudioProcessorEditor::applyZoneBarToSelected()
{
    const juce::ScopedLock sl (processor.dataLock);
    int idx = mappingGrid.selectedZoneIdx;
    if (idx < 0 || idx >= (int)processor.zones.size()) return;
    auto z = processor.zones[(size_t)idx];
    z->rootNote = (int)zRootSlider.getValue();
    z->lowKey   = (int)zLowKSlider.getValue();
    z->highKey  = (int)zHighKSlider.getValue();
    z->lowVel   = (int)zLowVSlider.getValue();
    z->highVel  = (int)zHighVSlider.getValue();
    z->volumeDb = (float)zVolSlider.getValue();
    z->pan      = (float)zPanSlider.getValue();
    z->tuneSemi = (float)zTuneSlider.getValue();
    mappingGrid.repaint();
}

void NoiseMu6ixSamplerAudioProcessorEditor::refreshFXUI()
{
    auto& f = processor.fx;
    fxEqOn.setToggleState  (f.eqOn.load(), juce::dontSendNotification);
    fxEqLow.setValue       (f.eqLowDb.load(), juce::dontSendNotification);
    fxEqMid.setValue       (f.eqMidDb.load(), juce::dontSendNotification);
    fxEqHigh.setValue      (f.eqHighDb.load(), juce::dontSendNotification);
    fxFltOn.setToggleState (f.filterOn.load(), juce::dontSendNotification);
    fxFltTypeBox.setSelectedId (f.filterType.load() + 1, juce::dontSendNotification);
    fxFltCut.setValue      (f.filterCutoff.load(), juce::dontSendNotification);
    fxFltRes.setValue      (f.filterRes.load(), juce::dontSendNotification);
    fxDrvOn.setToggleState (f.driveOn.load(), juce::dontSendNotification);
    fxDrvAmt.setValue      (f.driveAmount.load(), juce::dontSendNotification);
    fxChoOn.setToggleState (f.chorusOn.load(), juce::dontSendNotification);
    fxChoRate.setValue     (f.chorusRate.load(), juce::dontSendNotification);
    fxChoDepth.setValue    (f.chorusDepth.load(), juce::dontSendNotification);
    fxChoMix.setValue      (f.chorusMix.load(), juce::dontSendNotification);
    fxDlyOn.setToggleState (f.delayOn.load(), juce::dontSendNotification);
    fxDlyTime.setValue     (f.delayTimeMs.load(), juce::dontSendNotification);
    fxDlyFb.setValue       (f.delayFeedback.load(), juce::dontSendNotification);
    fxDlyMix.setValue      (f.delayMix.load(), juce::dontSendNotification);
    fxRevOn.setToggleState (f.reverbOn.load(), juce::dontSendNotification);
    fxRevSize.setValue     (f.reverbSize.load(), juce::dontSendNotification);
    fxRevDamp.setValue     (f.reverbDamp.load(), juce::dontSendNotification);
    fxRevMix.setValue      (f.reverbMix.load(), juce::dontSendNotification);
    fxAtk.setValue         (f.attack.load(), juce::dontSendNotification);
    fxDec.setValue         (f.decay.load(), juce::dontSendNotification);
    fxSus.setValue         (f.sustain.load(), juce::dontSendNotification);
    fxRel.setValue         (f.release.load(), juce::dontSendNotification);
}

void NoiseMu6ixSamplerAudioProcessorEditor::applyFXToProcessor()
{
    auto& f = processor.fx;
    f.eqOn.store          (fxEqOn.getToggleState());
    f.eqLowDb.store       ((float)fxEqLow.getValue());
    f.eqMidDb.store       ((float)fxEqMid.getValue());
    f.eqHighDb.store      ((float)fxEqHigh.getValue());
    f.filterOn.store      (fxFltOn.getToggleState());
    f.filterType.store    (fxFltTypeBox.getSelectedId() - 1);
    f.filterCutoff.store  ((float)fxFltCut.getValue());
    f.filterRes.store     ((float)fxFltRes.getValue());
    f.driveOn.store       (fxDrvOn.getToggleState());
    f.driveAmount.store   ((float)fxDrvAmt.getValue());
    f.chorusOn.store      (fxChoOn.getToggleState());
    f.chorusRate.store    ((float)fxChoRate.getValue());
    f.chorusDepth.store   ((float)fxChoDepth.getValue());
    f.chorusMix.store     ((float)fxChoMix.getValue());
    f.delayOn.store       (fxDlyOn.getToggleState());
    f.delayTimeMs.store   ((float)fxDlyTime.getValue());
    f.delayFeedback.store ((float)fxDlyFb.getValue());
    f.delayMix.store      ((float)fxDlyMix.getValue());
    f.reverbOn.store      (fxRevOn.getToggleState());
    f.reverbSize.store    ((float)fxRevSize.getValue());
    f.reverbDamp.store    ((float)fxRevDamp.getValue());
    f.reverbMix.store     ((float)fxRevMix.getValue());
    f.attack.store        ((float)fxAtk.getValue());
    f.decay.store         ((float)fxDec.getValue());
    f.sustain.store       ((float)fxSus.getValue());
    f.release.store       ((float)fxRel.getValue());
}

void NoiseMu6ixSamplerAudioProcessorEditor::updateModeVisibility()
{
    bool showGroup   = inEditMode && (currentTab == EditTab::Group);
    bool showMapping = inEditMode && (currentTab == EditTab::Mapping);
    bool showWave    = inEditMode && (currentTab == EditTab::Wave);
    bool showFX      = inEditMode && (currentTab == EditTab::FX);
    bool showFront   = !inEditMode;

    tabGroupBtn.setVisible (inEditMode); tabMappingBtn.setVisible (inEditMode);
    tabWaveBtn.setVisible (inEditMode);  tabFXBtn.setVisible (inEditMode);

    auto activeCol = juce::Colour (0xffd9a05b);
    auto idleCol   = juce::Colour (0xff3a3e47);
    tabGroupBtn.setColour   (juce::TextButton::buttonColourId, showGroup   ? activeCol : idleCol);
    tabMappingBtn.setColour (juce::TextButton::buttonColourId, showMapping ? activeCol : idleCol);
    tabWaveBtn.setColour    (juce::TextButton::buttonColourId, showWave    ? activeCol : idleCol);
    tabFXBtn.setColour      (juce::TextButton::buttonColourId, showFX      ? activeCol : idleCol);

    for (auto* c : std::initializer_list<juce::Component*>{
            &groupSelector, &createGroupBtn, &deleteGroupBtn, &groupSoloBtn, &groupMuteBtn, &filterByGroupBtn,
            &grpRRSlider, &grpVolSlider, &grpPanSlider, &grpTuneSlider, &grpRRLbl, &grpVolLbl, &grpPanLbl, &grpTuneLbl })
        c->setVisible (showGroup);

    for (auto* c : std::initializer_list<juce::Component*>{
            &mappingGrid, &autoSpreadBtn, &delZoneBtn, &selZoneNameLbl,
            &zRootSlider, &zLowKSlider, &zHighKSlider, &zLowVSlider, &zHighVSlider, &zVolSlider, &zPanSlider, &zTuneSlider,
            &zRootLbl, &zLowKLbl, &zHighKLbl, &zLowVLbl, &zHighVLbl, &zVolLbl, &zPanLbl, &zTuneLbl })
        c->setVisible (showMapping);

    for (auto* c : std::initializer_list<juce::Component*>{
            &waveEditor, &loopToggle, &autoLoopBtn, &waveDragModeBox, &crossfadeSlider, &zoomSlider, &cfLbl, &zoomLbl })
        c->setVisible (showWave);

    for (auto* c : std::initializer_list<juce::Component*>{
            &fxEqOn, &fxFltOn, &fxDrvOn, &fxChoOn, &fxDlyOn, &fxRevOn, &fxFltTypeBox,
            &fxEqLow, &fxEqMid, &fxEqHigh, &fxFltCut, &fxFltRes, &fxDrvAmt,
            &fxChoRate, &fxChoDepth, &fxChoMix, &fxDlyTime, &fxDlyFb, &fxDlyMix,
            &fxRevSize, &fxRevDamp, &fxRevMix, &fxAtk, &fxDec, &fxSus, &fxRel,
            &lEqLow, &lEqMid, &lEqHigh, &lFltCut, &lFltRes, &lDrvAmt,
            &lChoRate, &lChoDepth, &lChoMix, &lDlyTime, &lDlyFb, &lDlyMix,
            &lRevSize, &lRevDamp, &lRevMix, &lAtk, &lDec, &lSus, &lRel })
        c->setVisible (showFX);

    addMacroBtn.setVisible (showFront);
    toggleCustomizeBtn.setVisible (showFront);
    for (int i = 0; i < (int)macroSlots.size(); ++i)
        macroSlots[(size_t)i]->setVisible (showFront && processor.macroConfigs[i].visible);

    resized();
    repaint();
}

void NoiseMu6ixSamplerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d22));

    g.setColour (juce::Colour (0xff111317));
    g.fillRect (0, 0, getWidth(), 46);
    g.setColour (juce::Colour (0xffd9a05b));
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.drawText ("NOISE MU6IX SAMPLER v3", 12, 10, 240, 26, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff23252b));
    g.fillRect (0, 46, 220, getHeight() - 46);
    g.setColour (juce::Colour (0xff3c4049));
    g.drawVerticalLine (220, 46.0f, (float)getHeight());

    g.setColour (juce::Colour (0xff2c2f36));
    g.fillRect (226, 52, getWidth() - 232, getHeight() - 148);
    g.setColour (juce::Colour (0xff555963));
    g.drawRect (226, 52, getWidth() - 232, getHeight() - 148, 1);
}

void NoiseMu6ixSamplerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header
    auto header = area.removeFromTop (46).reduced (8, 8);
    header.removeFromLeft (232);
    modeToggleBtn.setBounds (header.removeFromLeft (105)); header.removeFromLeft (6);
    saveInstBtn.setBounds   (header.removeFromLeft (90));  header.removeFromLeft (5);
    loadInstBtn.setBounds   (header.removeFromLeft (90));  header.removeFromLeft (8);
    rrModeBox.setBounds     (header.removeFromLeft (155)); header.removeFromLeft (8);
    loadFilesBtn.setBounds  (header.removeFromLeft (85));  header.removeFromLeft (5);
    loadFolderBtn.setBounds (header.removeFromLeft (85));
    voicesLabel.setBounds   (header.removeFromRight (90));

    // Left Sidebar
    auto left = area.removeFromLeft (220).reduced (8, 8);
    browseFolderBtn.setBounds (left.removeFromTop (28)); left.removeFromTop (6);
    folderPathLabel.setBounds (left.removeFromTop (20)); left.removeFromTop (4);
    addSelectedFileBtn.setBounds (left.removeFromBottom (30)); left.removeFromBottom (6);
    fileListBox.setBounds (left);

    keyboard.setBounds (area.removeFromBottom (88).reduced (8, 4));
    auto rack = area.reduced (10, 10);

    if (inEditMode)
    {
        auto tabBar = rack.removeFromTop (30);
        tabGroupBtn.setBounds   (tabBar.removeFromLeft (125)); tabBar.removeFromLeft (4);
        tabMappingBtn.setBounds (tabBar.removeFromLeft (125)); tabBar.removeFromLeft (4);
        tabWaveBtn.setBounds    (tabBar.removeFromLeft (125)); tabBar.removeFromLeft (4);
        tabFXBtn.setBounds      (tabBar.removeFromLeft (135));
        rack.removeFromTop (8);

        if (currentTab == EditTab::Group)
        {
            auto topG = rack.removeFromTop (30);
            groupSelector.setBounds (topG.removeFromLeft (230)); topG.removeFromLeft (8);
            createGroupBtn.setBounds (topG.removeFromLeft (120)); topG.removeFromLeft (6);
            deleteGroupBtn.setBounds (topG.removeFromLeft (110)); topG.removeFromLeft (10);
            groupSoloBtn.setBounds (topG.removeFromLeft (70));
            groupMuteBtn.setBounds (topG.removeFromLeft (70));
            filterByGroupBtn.setBounds (topG.removeFromLeft (150));

            rack.removeFromTop (16);
            auto placeRow = [&](juce::Label& l, juce::Slider& s) {
                auto row = rack.removeFromTop (32);
                l.setBounds (row.removeFromLeft (130));
                s.setBounds (row.removeFromLeft (320));
                rack.removeFromTop (8);
            };
            placeRow (grpRRLbl, grpRRSlider); placeRow (grpVolLbl, grpVolSlider);
            placeRow (grpPanLbl, grpPanSlider); placeRow (grpTuneLbl, grpTuneSlider);
        }
        else if (currentTab == EditTab::Mapping)
        {
            auto topM = rack.removeFromTop (28);
            selZoneNameLbl.setBounds (topM.removeFromLeft (340));
            delZoneBtn.setBounds (topM.removeFromRight (90)); topM.removeFromRight (6);
            autoSpreadBtn.setBounds (topM.removeFromRight (135));

            rack.removeFromTop (6);
            auto paramBar = rack.removeFromTop (44);
            int colW = paramBar.getWidth() / 8;
            auto placeCol = [&](juce::Label& l, juce::Slider& s) {
                auto c = paramBar.removeFromLeft (colW).reduced (2, 0);
                l.setBounds (c.removeFromTop (16)); s.setBounds (c);
            };
            placeCol (zRootLbl, zRootSlider); placeCol (zLowKLbl, zLowKSlider);
            placeCol (zHighKLbl, zHighKSlider); placeCol (zLowVLbl, zLowVSlider);
            placeCol (zHighVLbl, zHighVSlider); placeCol (zVolLbl, zVolSlider);
            placeCol (zPanLbl, zPanSlider); placeCol (zTuneLbl, zTuneSlider);
            rack.removeFromTop (6);
            mappingGrid.setBounds (rack);
        }
        else if (currentTab == EditTab::Wave)
        {
            auto topW = rack.removeFromTop (30);
            loopToggle.setBounds (topW.removeFromLeft (110));
            autoLoopBtn.setBounds (topW.removeFromLeft (175)); topW.removeFromLeft (10);
            waveDragModeBox.setBounds (topW.removeFromLeft (185));

            rack.removeFromTop (8);
            auto sliderW = rack.removeFromTop (28);
            cfLbl.setBounds (sliderW.removeFromLeft (75));
            crossfadeSlider.setBounds (sliderW.removeFromLeft (230)); sliderW.removeFromLeft (20);
            zoomLbl.setBounds (sliderW.removeFromLeft (45));
            zoomSlider.setBounds (sliderW.removeFromLeft (180));
            rack.removeFromTop (8);
            waveEditor.setBounds (rack);
        }
        else if (currentTab == EditTab::FX)
        {
            auto layoutModule = [](juce::Rectangle<int> box, juce::ToggleButton* tog, juce::ComboBox* combo,
                                   std::vector<std::pair<juce::Label*, juce::Slider*>> knobs) {
                auto top = box.removeFromTop (26);
                if (tog) tog->setBounds (top.removeFromLeft (115));
                if (combo) combo->setBounds (top.removeFromRight (95));
                int kw = box.getWidth() / std::max (1, (int)knobs.size());
                for (auto& kv : knobs)
                {
                    auto c = box.removeFromLeft (kw).reduced (2, 2);
                    kv.first->setBounds (c.removeFromTop (16));
                    kv.second->setBounds (c);
                }
            };

            auto row1 = rack.removeFromTop (160).reduced (0, 4);
            int w3 = row1.getWidth() / 3;
            layoutModule (row1.removeFromLeft (w3).reduced (6), &fxEqOn, nullptr,
                          { {&lEqLow, &fxEqLow}, {&lEqMid, &fxEqMid}, {&lEqHigh, &fxEqHigh} });
            layoutModule (row1.removeFromLeft (w3).reduced (6), &fxFltOn, &fxFltTypeBox,
                          { {&lFltCut, &fxFltCut}, {&lFltRes, &fxFltRes} });
            layoutModule (row1.reduced (6), &fxDrvOn, nullptr,
                          { {&lDrvAmt, &fxDrvAmt} });

            auto row2 = rack.removeFromTop (160).reduced (0, 4);
            layoutModule (row2.removeFromLeft (w3).reduced (6), &fxChoOn, nullptr,
                          { {&lChoRate, &fxChoRate}, {&lChoDepth, &fxChoDepth}, {&lChoMix, &fxChoMix} });
            layoutModule (row2.removeFromLeft (w3).reduced (6), &fxDlyOn, nullptr,
                          { {&lDlyTime, &fxDlyTime}, {&lDlyFb, &fxDlyFb}, {&lDlyMix, &fxDlyMix} });
            layoutModule (row2.reduced (6), &fxRevOn, nullptr,
                          { {&lRevSize, &fxRevSize}, {&lRevDamp, &fxRevDamp}, {&lRevMix, &fxRevMix} });

            auto row3 = rack.removeFromTop (145).reduced (6, 4);
            layoutModule (row3, nullptr, nullptr,
                          { {&lAtk, &fxAtk}, {&lDec, &fxDec}, {&lSus, &fxSus}, {&lRel, &fxRel} });
        }
    }
    else
    {
        auto topF = rack.removeFromTop (32);
        addMacroBtn.setBounds (topF.removeFromLeft (150)); topF.removeFromLeft (8);
        toggleCustomizeBtn.setBounds (topF.removeFromLeft (150));

        rack.removeFromTop (12);
        std::vector<CustomMacroSlot*> visibleSlots;
        for (int i = 0; i < (int)macroSlots.size(); ++i)
            if (processor.macroConfigs[i].visible) visibleSlots.push_back (macroSlots[(size_t)i].get());

        if (!visibleSlots.empty())
        {
            int cols = std::min (4, (int)visibleSlots.size());
            int rows = ((int)visibleSlots.size() + cols - 1) / cols;
            int cellW = rack.getWidth() / cols;
            int cellH = std::min (240, rack.getHeight() / std::max (1, rows));
            for (size_t i = 0; i < visibleSlots.size(); ++i)
                visibleSlots[i]->setBounds (rack.getX() + ((int)i % cols) * cellW + 6,
                                            rack.getY() + ((int)i / cols) * cellH + 6,
                                            cellW - 12, cellH - 12);
        }
    }
}
