#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

// ==================== MappingGridComponent ====================
MappingGridComponent::MappingGridComponent (NoiseMu6ixSamplerAudioProcessor& p)
    : processor (p)
{
}

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

    // Draw Kontakt-style vertical key grid
    for (int n = minNote; n <= maxNote; ++n)
    {
        float x = (float)(n - minNote) * keyW;
        int pitchClass = n % 12;
        bool isBlack = (pitchClass == 1 || pitchClass == 3 || pitchClass == 6 || pitchClass == 8 || pitchClass == 10);
        if (isBlack)
        {
            g.setColour (juce::Colour (0xff232323));
            g.fillRect (x, 0.0f, keyW, h);
        }
        g.setColour (pitchClass == 0 ? juce::Colour (0xff555555) : juce::Colour (0xff383838));
        g.drawVerticalLine ((int)x, 0.0f, h);
    }

    // Horizontal velocity lines (32, 64, 96)
    g.setColour (juce::Colour (0xff404040));
    for (int v : { 32, 64, 96 })
    {
        float y = (1.0f - (float)v / 127.0f) * h;
        g.drawHorizontalLine ((int)y, 0.0f, w);
    }

    // Draw Sample Zones
    const juce::ScopedLock sl (processor.dataLock);
    if (processor.zones.empty())
    {
        g.setColour (juce::Colour (0xffaaaaaa));
        g.setFont (14.0f);
        g.drawFittedText ("Drag & Drop WAV Samples onto this Mapping Grid (or use Left Browser / Load WAVs)",
                          getLocalBounds(), juce::Justification::centred, 2);
        return;
    }

    static const juce::uint32 palette[6] = {
        0xffd9a05b, 0xff5ba8d9, 0xff7cc47f, 0xffc97575, 0xffa77fd4, 0xffd4c25b
    };

    for (size_t i = 0; i < processor.zones.size(); ++i)
    {
        const auto& z = processor.zones[i];
        if (activeGroupFilter >= 0 && z->groupIndex != activeGroupFilter)
            continue;

        auto rect = getZoneRect (*z);
        bool isSel = ((int)i == selectedZoneIdx);
        auto col = juce::Colour (palette[(size_t)(std::max (0, z->groupIndex)) % 6]);

        g.setColour (col.withAlpha (isSel ? 0.85f : 0.48f));
        g.fillRect (rect);

        g.setColour (isSel ? juce::Colours::white : col.brighter (0.3f));
        g.drawRect (rect, isSel ? 2.0f : 1.0f);

        // Root note yellow marker
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
        if (r.contains (e.position))
        {
            hitIdx = i;
            hitRect = r;
            break;
        }
    }

    selectedZoneIdx = hitIdx;
    dragMode = DragMode::None;

    if (hitIdx >= 0)
    {
        auto& z = processor.zones[(size_t)hitIdx];
        origLowK  = z->lowKey;
        origHighK = z->highKey;
        origLowV  = z->lowVel;
        origHighV = z->highVel;
        origRoot  = z->rootNote;
        dragStartNote = xToNote (e.position.x);
        dragStartVel  = yToVel (e.position.y);

        float edge = 8.0f;
        if (std::abs (e.position.x - hitRect.getX()) < edge)          dragMode = DragMode::LeftEdge;
        else if (std::abs (e.position.x - hitRect.getRight()) < edge) dragMode = DragMode::RightEdge;
        else if (std::abs (e.position.y - hitRect.getY()) < edge)     dragMode = DragMode::TopEdge;
        else if (std::abs (e.position.y - hitRect.getBottom()) < edge)dragMode = DragMode::BottomEdge;
        else                                                          dragMode = DragMode::Move;
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
        case DragMode::LeftEdge:
            z->lowKey = juce::jlimit (0, z->highKey, curNote);
            break;
        case DragMode::RightEdge:
            z->highKey = juce::jlimit (z->lowKey, 127, curNote);
            break;
        case DragMode::TopEdge:
            z->highVel = juce::jlimit (z->lowVel + 1, 127, curVel);
            break;
        case DragMode::BottomEdge:
            z->lowVel = juce::jlimit (0, z->highVel - 1, curVel);
            break;
        case DragMode::Move:
        {
            int spanK = origHighK - origLowK;
            int newLowK = juce::jlimit (0, 127 - spanK, origLowK + dNote);
            z->lowKey   = newLowK;
            z->highKey  = newLowK + spanK;
            z->rootNote = juce::jlimit (0, 127, origRoot + (newLowK - origLowK));

            int spanV = origHighV - origLowV;
            int newLowV = juce::jlimit (0, 127 - spanV, origLowV + dVel);
            z->lowVel  = newLowV;
            z->highVel = newLowV + spanV;
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
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".aif"))
            return true;
    return false;
}

void MappingGridComponent::filesDropped (const juce::StringArray& files, int x, int)
{
    int dropNote = xToNote ((float)x);
    int grp = (activeGroupFilter >= 0) ? activeGroupFilter : 0;
    for (auto& f : files)
    {
        processor.loadSampleFile (juce::File (f), grp, files.size() > 1, files.size() == 1 ? dropNote : -1);
    }
    const juce::ScopedLock sl (processor.dataLock);
    if (!processor.zones.empty())
        selectedZoneIdx = (int)processor.zones.size() - 1;
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
        g.drawFittedText ("Select a Sample Zone in Mapping Editor to edit its Waveform & Auto-Loop",
                          getLocalBounds(), juce::Justification::centred, 1);
        return;
    }

    auto b = getLocalBounds().reduced (4);
    int w = b.getWidth();
    int h = b.getHeight();
    int numSamples = zone->buffer.getNumSamples();
    int visibleSamples = std::max (256, (int)((float)numSamples / zoomFactor));
    const float* data = zone->buffer.getReadPointer (0);

    auto sampleToX = [&](int s) -> float {
        return (float)b.getX() + ((float)s / (float)visibleSamples) * (float)w;
    };

    // Draw trimmed out areas
    float xSStart = sampleToX (zone->sampleStart.load());
    float xSEnd   = sampleToX (zone->sampleEnd.load());
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    if (xSStart > b.getX()) g.fillRect ((float)b.getX(), (float)b.getY(), xSStart - b.getX(), (float)h);
    if (xSEnd < b.getRight()) g.fillRect (xSEnd, (float)b.getY(), b.getRight() - xSEnd, (float)h);

    // Draw Loop region
    if (zone->isLooping.load())
    {
        float xLStart = sampleToX (zone->loopStart.load());
        float xLEnd   = sampleToX (zone->loopEnd.load());
        g.setColour (juce::Colour (0x3300e5ff));
        g.fillRect (xLStart, (float)b.getY(), std::max (2.0f, xLEnd - xLStart), (float)h);

        g.setColour (juce::Colour (0xff00e5ff));
        g.drawVerticalLine ((int)xLStart, (float)b.getY(), (float)b.getBottom());
        g.setColour (juce::Colour (0xffff5555));
        g.drawVerticalLine ((int)xLEnd, (float)b.getY(), (float)b.getBottom());
    }

    // Draw Waveform
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

    // Draw Start / End markers
    g.setColour (juce::Colours::limegreen);
    g.drawVerticalLine ((int)xSStart, (float)b.getY(), (float)b.getBottom());
    g.setColour (juce::Colours::orange);
    g.drawVerticalLine ((int)xSEnd, (float)b.getY(), (float)b.getBottom());
}

void WaveEditorComponent::mouseDown (const juce::MouseEvent& e)
{
    if (!zone || zone->buffer.getNumSamples() == 0) return;
    int numSamples = zone->buffer.getNumSamples();
    int visibleSamples = std::max (256, (int)((float)numSamples / zoomFactor));
    int clicked = (int)(((float)e.x / (float)std::max (1, getWidth())) * visibleSamples);

    if (editingTrim)
    {
        int d1 = std::abs (clicked - zone->sampleStart.load());
        int d2 = std::abs (clicked - zone->sampleEnd.load());
        draggingFirst = (d1 <= d2);
    }
    else
    {
        int d1 = std::abs (clicked - zone->loopStart.load());
        int d2 = std::abs (clicked - zone->loopEnd.load());
        draggingFirst = (d1 <= d2);
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
        if (draggingFirst)
        {
            if (snapped < zone->sampleEnd.load() - 64) zone->sampleStart.store (snapped);
        }
        else
        {
            if (snapped > zone->sampleStart.load() + 64) zone->sampleEnd.store (snapped);
        }
    }
    else
    {
        if (draggingFirst)
        {
            if (snapped < zone->loopEnd.load() - 64) zone->loopStart.store (snapped);
        }
        else
        {
            if (snapped > zone->loopStart.load() + 64) zone->loopEnd.store (snapped);
        }
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
    if (customizeMode)
    {
        hideBtn.setBounds (topRow.removeFromRight (22));
        topRow.removeFromRight (3);
    }
    nameLabel.setBounds (topRow);
    r.removeFromTop (4);

    if (customizeMode)
    {
        targetBox.setBounds (r.removeFromBottom (22));
        r.removeFromBottom (4);
        styleToggle.setBounds (r.removeFromBottom (20));
        r.removeFromBottom (4);
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
    setSize (1140, 700);

    // Top Header Controls
    modeToggleBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffb86b28));
    modeToggleBtn.onClick = [this] {
        inEditMode = !inEditMode;
        modeToggleBtn.setButtonText (inEditMode ? "Front Panel (Performance)" : "Edit Mode (Kontakt Rack)");
        updateModeVisibility();
    };
    addAndMakeVisible (modeToggleBtn);

    loadFilesBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Select WAV Samples", juce::File{}, "*.wav;*.flac;*.aif");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems,
            [this](const juce::FileChooser& fc) {
                for (auto& f : fc.getResults()) processor.loadSampleFile (f, selectedGroupIdx, true, -1);
                refreshGroupList();
                refreshSelectedZoneUI();
                mappingGrid.repaint();
            });
    };
    addAndMakeVisible (loadFilesBtn);

    loadFolderBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Select Sample Folder to Auto-Map");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc) {
                processor.loadSampleFolder (fc.getResult(), selectedGroupIdx);
                refreshGroupList();
                refreshSelectedZoneUI();
                mappingGrid.repaint();
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

    // Left File Browser
    browseFolderBtn.onClick = [this] { browseLocalFolder(); };
    addAndMakeVisible (browseFolderBtn);

    addSelectedFileBtn.onClick = [this] {
        int row = fileListBox.getSelectedRow();
        if (row >= 0 && row < fileListModel.files.size())
        {
            processor.loadSampleFile (fileListModel.files[row], selectedGroupIdx, true, -1);
            const juce::ScopedLock sl (processor.dataLock);
            mappingGrid.selectedZoneIdx = (int)processor.zones.size() - 1;
            refreshGroupList();
            refreshSelectedZoneUI();
            mappingGrid.repaint();
        }
    };
    addAndMakeVisible (addSelectedFileBtn);

    folderPathLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    folderPathLabel.setFont (11.5f);
    addAndMakeVisible (folderPathLabel);

    fileListModel.onFileDoubleClicked = [this](const juce::File& f) {
        processor.loadSampleFile (f, selectedGroupIdx, true, -1);
        const juce::ScopedLock sl (processor.dataLock);
        mappingGrid.selectedZoneIdx = (int)processor.zones.size() - 1;
        refreshGroupList();
        refreshSelectedZoneUI();
        mappingGrid.repaint();
    };
    fileListBox.setRowHeight (22);
    fileListBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1f2126));
    addAndMakeVisible (fileListBox);

    // Edit Mode Sub-Tabs
    tabGroupBtn.onClick   = [this] { currentTab = EditTab::Group;   updateModeVisibility(); };
    tabMappingBtn.onClick = [this] { currentTab = EditTab::Mapping; updateModeVisibility(); };
    tabWaveBtn.onClick    = [this] { currentTab = EditTab::Wave;    updateModeVisibility(); };
    addAndMakeVisible (tabGroupBtn);
    addAndMakeVisible (tabMappingBtn);
    addAndMakeVisible (tabWaveBtn);

    // Group Editor Controls
    groupSelector.onChange = [this] {
        selectedGroupIdx = std::max (0, groupSelector.getSelectedItemIndex());
        if (filterByGroupBtn.getToggleState())
            mappingGrid.activeGroupFilter = selectedGroupIdx;
        refreshGroupList();
        mappingGrid.repaint();
    };
    addAndMakeVisible (groupSelector);

    createGroupBtn.onClick = [this] {
        selectedGroupIdx = processor.addGroup();
        refreshGroupList();
    };
    addAndMakeVisible (createGroupBtn);

    deleteGroupBtn.onClick = [this] {
        processor.removeGroup (selectedGroupIdx);
        selectedGroupIdx = std::max (0, selectedGroupIdx - 1);
        refreshGroupList();
        mappingGrid.repaint();
    };
    addAndMakeVisible (deleteGroupBtn);

    groupSoloBtn.onClick = [this] { applyGroupInspector(); };
    groupMuteBtn.onClick = [this] { applyGroupInspector(); };
    filterByGroupBtn.onClick = [this] {
        mappingGrid.activeGroupFilter = filterByGroupBtn.getToggleState() ? selectedGroupIdx : -1;
        mappingGrid.repaint();
    };
    addAndMakeVisible (groupSoloBtn);
    addAndMakeVisible (groupMuteBtn);
    addAndMakeVisible (filterByGroupBtn);

    auto setupSlider = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV, std::function<void()> cb) {
        s.setRange (minV, maxV, step);
        s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
        s.onValueChange = std::move (cb);
        addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (l);
    };

    setupSlider (grpRRSlider,   grpRRLbl,   1, 10, 1, 1,         [this]{ applyGroupInspector(); });
    setupSlider (grpVolSlider,  grpVolLbl,  -24, 12, 0.1, 0.0,   [this]{ applyGroupInspector(); });
    setupSlider (grpPanSlider,  grpPanLbl,  -1, 1, 0.01, 0.0,    [this]{ applyGroupInspector(); });
    setupSlider (grpTuneSlider, grpTuneLbl, -24, 24, 0.1, 0.0,   [this]{ applyGroupInspector(); });

    // Mapping Editor Controls
    mappingGrid.onZoneChanged = [this] { refreshSelectedZoneUI(); };
    addAndMakeVisible (mappingGrid);

    autoSpreadBtn.onClick = [this] { processor.autoSpreadKeyRanges(); mappingGrid.repaint(); refreshSelectedZoneUI(); };
    delZoneBtn.onClick = [this] {
        if (mappingGrid.selectedZoneIdx >= 0)
        {
            processor.removeZone (mappingGrid.selectedZoneIdx);
            mappingGrid.selectedZoneIdx = -1;
            mappingGrid.repaint();
            refreshSelectedZoneUI();
        }
    };
    addAndMakeVisible (autoSpreadBtn);
    addAndMakeVisible (delZoneBtn);
    addAndMakeVisible (selZoneNameLbl);

    auto setupZoneNum = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV) {
        s.setRange (minV, maxV, step);
        s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 20);
        s.onValueChange = [this] { applyZoneBarToSelected(); };
        addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (11.0f);
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

    // Wave Editor Controls
    addAndMakeVisible (waveEditor);
    addAndMakeVisible (loopToggle);
    loopToggle.onClick = [this] {
        const juce::ScopedLock sl (processor.dataLock);
        int idx = mappingGrid.selectedZoneIdx;
        if (idx >= 0 && idx < (int)processor.zones.size())
        {
            processor.zones[(size_t)idx]->isLooping.store (loopToggle.getToggleState());
            waveEditor.repaint();
        }
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

    setupSlider (crossfadeSlider, cfLbl, 64, 15000, 1, 2048, [this] {
        const juce::ScopedLock sl (processor.dataLock);
        int idx = mappingGrid.selectedZoneIdx;
        if (idx >= 0 && idx < (int)processor.zones.size())
            processor.zones[(size_t)idx]->crossfadeSamples.store ((int)crossfadeSlider.getValue());
    });
    setupSlider (zoomSlider, zoomLbl, 1.0, 16.0, 0.1, 1.0, [this] {
        waveEditor.setZoom ((float)zoomSlider.getValue());
    });

    // Front Panel Custom Controls
    addMacroBtn.onClick = [this] {
        for (int i = 0; i < NoiseMu6ixSamplerAudioProcessor::numMacros; ++i)
        {
            if (!processor.macroConfigs[i].visible)
            {
                processor.macroConfigs[i].visible = true;
                updateModeVisibility();
                resized();
                break;
            }
        }
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
        slot->onVisibilityChanged = [this] { updateModeVisibility(); resized(); };
        addAndMakeVisible (*slot);
        macroSlots.push_back (std::move (slot));
    }

    // Bottom Piano Keyboard
    keyboard.setAvailableRange (24, 96);
    addAndMakeVisible (keyboard);

    refreshGroupList();
    updateModeVisibility();
    startTimerHz (10);
}

void NoiseMu6ixSamplerAudioProcessorEditor::timerCallback()
{
    voicesLabel.setText ("Voices: " + juce::String (processor.activeVoicesCount.load()), juce::dontSendNotification);
}

void NoiseMu6ixSamplerAudioProcessorEditor::browseLocalFolder()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Browse Sample Folder");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc) {
            auto dir = fc.getResult();
            if (dir.isDirectory())
            {
                folderPathLabel.setText (dir.getFileName(), juce::dontSendNotification);
                fileListModel.files = dir.findChildFiles (juce::File::findFiles, false, "*.wav;*.flac;*.aif;*.aiff;*.ogg");
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
    {
        auto& g = processor.groups[i];
        groupSelector.addItem (g.name + " [RR" + juce::String (g.rrGroup) + "]", (int)i + 1);
    }
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

void NoiseMu6ixSamplerAudioProcessorEditor::updateModeVisibility()
{
    bool showGroup   = inEditMode && (currentTab == EditTab::Group);
    bool showMapping = inEditMode && (currentTab == EditTab::Mapping);
    bool showWave    = inEditMode && (currentTab == EditTab::Wave);
    bool showFront   = !inEditMode;

    tabGroupBtn.setVisible (inEditMode);
    tabMappingBtn.setVisible (inEditMode);
    tabWaveBtn.setVisible (inEditMode);

    auto activeCol = juce::Colour (0xffd9a05b);
    auto idleCol   = juce::Colour (0xff3a3e47);
    tabGroupBtn.setColour   (juce::TextButton::buttonColourId, showGroup   ? activeCol : idleCol);
    tabMappingBtn.setColour (juce::TextButton::buttonColourId, showMapping ? activeCol : idleCol);
    tabWaveBtn.setColour    (juce::TextButton::buttonColourId, showWave    ? activeCol : idleCol);

    // Group Editor
    for (auto* c : std::initializer_list<juce::Component*>{
            &groupSelector, &createGroupBtn, &deleteGroupBtn, &groupSoloBtn, &groupMuteBtn, &filterByGroupBtn,
            &grpRRSlider, &grpVolSlider, &grpPanSlider, &grpTuneSlider,
            &grpRRLbl, &grpVolLbl, &grpPanLbl, &grpTuneLbl })
        c->setVisible (showGroup);

    // Mapping Editor
    for (auto* c : std::initializer_list<juce::Component*>{
            &mappingGrid, &autoSpreadBtn, &delZoneBtn, &selZoneNameLbl,
            &zRootSlider, &zLowKSlider, &zHighKSlider, &zLowVSlider, &zHighVSlider, &zVolSlider, &zPanSlider, &zTuneSlider,
            &zRootLbl, &zLowKLbl, &zHighKLbl, &zLowVLbl, &zHighVLbl, &zVolLbl, &zPanLbl, &zTuneLbl })
        c->setVisible (showMapping);

    // Wave Editor
    for (auto* c : std::initializer_list<juce::Component*>{
            &waveEditor, &loopToggle, &autoLoopBtn, &waveDragModeBox,
            &crossfadeSlider, &zoomSlider, &cfLbl, &zoomLbl })
        c->setVisible (showWave);

    // Front Panel
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

    // Top Kontakt-style header bar
    g.setColour (juce::Colour (0xff111317));
    g.fillRect (0, 0, getWidth(), 46);
    g.setColour (juce::Colour (0xffd9a05b));
    g.setFont (juce::Font (19.0f, juce::Font::bold));
    g.drawText ("NOISE MU6IX SAMPLER", 14, 10, 260, 26, juce::Justification::centredLeft);

    // Left Sidebar panel background
    g.setColour (juce::Colour (0xff23252b));
    g.fillRect (0, 46, 220, getHeight() - 46);
    g.setColour (juce::Colour (0xff3c4049));
    g.drawVerticalLine (220, 46.0f, (float)getHeight());

    // Main Rack Area warm-grey Kontakt border
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
    header.removeFromLeft (240);
    modeToggleBtn.setBounds (header.removeFromLeft (185));
    header.removeFromLeft (10);
    rrModeBox.setBounds (header.removeFromLeft (165));
    header.removeFromLeft (10);
    loadFilesBtn.setBounds (header.removeFromLeft (90));
    header.removeFromLeft (6);
    loadFolderBtn.setBounds (header.removeFromLeft (120));
    voicesLabel.setBounds (header.removeFromRight (100));

    // Left Sidebar (File Browser)
    auto left = area.removeFromLeft (220).reduced (8, 8);
    browseFolderBtn.setBounds (left.removeFromTop (28));
    left.removeFromTop (6);
    folderPathLabel.setBounds (left.removeFromTop (20));
    left.removeFromTop (4);
    addSelectedFileBtn.setBounds (left.removeFromBottom (30));
    left.removeFromBottom (6);
    fileListBox.setBounds (left);

    // Bottom Piano Keyboard (Right side)
    keyboard.setBounds (area.removeFromBottom (88).reduced (8, 4));

    // Main Right Rack Area
    auto rack = area.reduced (10, 10);

    if (inEditMode)
    {
        // Sub-Tabs
        auto tabBar = rack.removeFromTop (30);
        tabGroupBtn.setBounds   (tabBar.removeFromLeft (130));
        tabBar.removeFromLeft (4);
        tabMappingBtn.setBounds (tabBar.removeFromLeft (130));
        tabBar.removeFromLeft (4);
        tabWaveBtn.setBounds    (tabBar.removeFromLeft (130));
        rack.removeFromTop (8);

        if (currentTab == EditTab::Group)
        {
            auto topG = rack.removeFromTop (30);
            groupSelector.setBounds (topG.removeFromLeft (240));
            topG.removeFromLeft (8);
            createGroupBtn.setBounds (topG.removeFromLeft (150));
            topG.removeFromLeft (6);
            deleteGroupBtn.setBounds (topG.removeFromLeft (110));
            topG.removeFromLeft (12);
            groupSoloBtn.setBounds (topG.removeFromLeft (100));
            groupMuteBtn.setBounds (topG.removeFromLeft (75));
            filterByGroupBtn.setBounds (topG.removeFromLeft (155));

            rack.removeFromTop (18);
            auto placeRow = [&](juce::Label& l, juce::Slider& s) {
                auto row = rack.removeFromTop (32);
                l.setBounds (row.removeFromLeft (130));
                s.setBounds (row.removeFromLeft (320));
                rack.removeFromTop (8);
            };
            placeRow (grpRRLbl,   grpRRSlider);
            placeRow (grpVolLbl,  grpVolSlider);
            placeRow (grpPanLbl,  grpPanSlider);
            placeRow (grpTuneLbl, grpTuneSlider);
        }
        else if (currentTab == EditTab::Mapping)
        {
            auto topM = rack.removeFromTop (28);
            selZoneNameLbl.setBounds (topM.removeFromLeft (340));
            delZoneBtn.setBounds (topM.removeFromRight (90));
            topM.removeFromRight (6);
            autoSpreadBtn.setBounds (topM.removeFromRight (135));

            rack.removeFromTop (6);
            auto paramBar = rack.removeFromTop (44);
            int colW = paramBar.getWidth() / 8;
            auto placeCol = [&](juce::Label& l, juce::Slider& s) {
                auto c = paramBar.removeFromLeft (colW).reduced (2, 0);
                l.setBounds (c.removeFromTop (16));
                s.setBounds (c);
            };
            placeCol (zRootLbl,  zRootSlider);
            placeCol (zLowKLbl,  zLowKSlider);
            placeCol (zHighKLbl, zHighKSlider);
            placeCol (zLowVLbl,  zLowVSlider);
            placeCol (zHighVLbl, zHighVSlider);
            placeCol (zVolLbl,   zVolSlider);
            placeCol (zPanLbl,   zPanSlider);
            placeCol (zTuneLbl,  zTuneSlider);

            rack.removeFromTop (6);
            mappingGrid.setBounds (rack);
        }
        else if (currentTab == EditTab::Wave)
        {
            auto topW = rack.removeFromTop (30);
            loopToggle.setBounds (topW.removeFromLeft (110));
            autoLoopBtn.setBounds (topW.removeFromLeft (175));
            topW.removeFromLeft (10);
            waveDragModeBox.setBounds (topW.removeFromLeft (185));

            rack.removeFromTop (8);
            auto sliderW = rack.removeFromTop (28);
            cfLbl.setBounds (sliderW.removeFromLeft (75));
            crossfadeSlider.setBounds (sliderW.removeFromLeft (230));
            sliderW.removeFromLeft (20);
            zoomLbl.setBounds (sliderW.removeFromLeft (45));
            zoomSlider.setBounds (sliderW.removeFromLeft (180));

            rack.removeFromTop (8);
            waveEditor.setBounds (rack);
        }
    }
    else
    {
        // Front Panel (Performance Custom UI)
        auto topF = rack.removeFromTop (32);
        addMacroBtn.setBounds (topF.removeFromLeft (150));
        topF.removeFromLeft (8);
        toggleCustomizeBtn.setBounds (topF.removeFromLeft (150));

        rack.removeFromTop (12);
        std::vector<CustomMacroSlot*> visibleSlots;
        for (int i = 0; i < (int)macroSlots.size(); ++i)
            if (processor.macroConfigs[i].visible)
                visibleSlots.push_back (macroSlots[(size_t)i].get());

        if (!visibleSlots.empty())
        {
            int cols = std::min (4, (int)visibleSlots.size());
            int rows = ((int)visibleSlots.size() + cols - 1) / cols;
            int cellW = rack.getWidth() / cols;
            int cellH = std::min (240, rack.getHeight() / std::max (1, rows));

            for (size_t i = 0; i < visibleSlots.size(); ++i)
            {
                int rIdx = (int)i / cols;
                int cIdx = (int)i % cols;
                visibleSlots[i]->setBounds (rack.getX() + cIdx * cellW + 6,
                                            rack.getY() + rIdx * cellH + 6,
                                            cellW - 12, cellH - 12);
            }
        }
    }
}
