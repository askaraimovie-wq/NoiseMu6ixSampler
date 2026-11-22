#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

// ==================== GroupCardsComponent ====================
GroupCardsComponent::GroupCardsComponent (NoiseMu6ixSamplerAudioProcessor& p) : processor (p) {}

juce::Rectangle<int> GroupCardsComponent::getCardRect (int idx) const
{
    int cols = std::max (1, getWidth() / 145);
    int cardW = (getWidth() - 8) / cols;
    int cardH = 38;
    int r = idx / cols;
    int c = idx % cols;
    return { 4 + c * cardW, 4 + r * (cardH + 4), cardW - 6, cardH };
}

void GroupCardsComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1e24));
    g.setColour (juce::Colour (0xff3e424c));
    g.drawRect (getLocalBounds(), 1);

    const juce::ScopedLock sl (processor.dataLock);
    for (int i = 0; i < (int)processor.groups.size(); ++i)
    {
        auto r = getCardRect (i);
        if (r.getY() >= getHeight()) break;

        const auto& grp = processor.groups[(size_t)i];
        bool isSel = (i == selectedGroupIdx);

        int zCount = 0;
        for (auto& z : processor.zones)
            if (z->groupIndex == i) ++zCount;

        g.setColour (isSel ? juce::Colour (0xffd9a05b) : juce::Colour (0xff2e323c));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);

        g.setColour (isSel ? juce::Colours::white : juce::Colour (0xff555a66));
        g.drawRoundedRectangle (r.toFloat(), 4.0f, isSel ? 1.6f : 1.0f);

        g.setColour (isSel ? juce::Colour (0xff111317) : juce::Colours::white);
        g.setFont (juce::Font (12.5f, isSel ? juce::Font::bold : juce::Font::plain));
        g.drawText (grp.name, r.reduced (8, 2).removeFromTop (18), juce::Justification::centredLeft, true);

        juce::String sub = juce::String (zCount) + " zones";
        if (grp.solo) sub += " [S]";
        if (grp.mute) sub += " [M]";
        g.setColour (isSel ? juce::Colour (0xff2c2419) : juce::Colour (0xffa8adb8));
        g.setFont (10.5f);
        g.drawText (sub, r.reduced (8, 2).removeFromBottom (14), juce::Justification::centredLeft, true);
    }
}

void GroupCardsComponent::mouseDown (const juce::MouseEvent& e)
{
    const juce::ScopedLock sl (processor.dataLock);
    for (int i = 0; i < (int)processor.groups.size(); ++i)
    {
        if (getCardRect (i).contains (e.getPosition()))
        {
            selectedGroupIdx = i;
            repaint();
            if (onGroupSelected) onGroupSelected (i);
            return;
        }
    }
}

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
    return { x1, yTop, std::max (10.0f, x2 - x1), std::max (12.0f, yBot - yTop) };
}

MappingGridComponent::DragMode MappingGridComponent::hitTestDragMode (const juce::Rectangle<float>& r, juce::Point<float> pt) const
{
    float edgeX = std::min (8.0f, r.getWidth() * 0.25f);
    float edgeY = std::min (8.0f, r.getHeight() * 0.25f);

    if (std::abs (pt.x - r.getX()) <= edgeX)           return DragMode::LeftEdge;
    if (std::abs (pt.x - r.getRight()) <= edgeX)       return DragMode::RightEdge;
    if (std::abs (pt.y - r.getY()) <= edgeY)           return DragMode::TopEdge;
    if (std::abs (pt.y - r.getBottom()) <= edgeY)      return DragMode::BottomEdge;
    return DragMode::Move;
}

void MappingGridComponent::mouseMove (const juce::MouseEvent& e)
{
    const juce::ScopedLock sl (processor.dataLock);
    for (int i = (int)processor.zones.size() - 1; i >= 0; --i)
    {
        const auto& z = processor.zones[(size_t)i];
        if (activeGroupFilter >= 0 && z->groupIndex != activeGroupFilter) continue;
        auto r = getZoneRect (*z);
        if (r.contains (e.position))
        {
            auto mode = hitTestDragMode (r, e.position);
            if (mode == DragMode::LeftEdge || mode == DragMode::RightEdge)
                setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            else if (mode == DragMode::TopEdge || mode == DragMode::BottomEdge)
                setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            else
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            return;
        }
    }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void MappingGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff26282e));
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
            g.setColour (juce::Colour (0xff1e2026));
            g.fillRect (x, 0.0f, keyW, h);
        }
        g.setColour (pc == 0 ? juce::Colour (0xff585d68) : juce::Colour (0xff353842));
        g.drawVerticalLine ((int)x, 0.0f, h);

        if (pc == 0)
        {
            g.setColour (juce::Colour (0xff8a909e));
            g.setFont (9.5f);
            g.drawText (juce::MidiMessage::getMidiNoteName (n, true, true, 3), (int)x + 2, (int)h - 14, 24, 12, juce::Justification::centredLeft);
        }
    }

    g.setColour (juce::Colour (0xff3f434e));
    for (int v : { 32, 64, 96 })
        g.drawHorizontalLine ((int)((1.0f - (float)v / 127.0f) * h), 0.0f, w);

    const juce::ScopedLock sl (processor.dataLock);
    if (processor.zones.empty())
    {
        g.setColour (juce::Colour (0xffb0b5c0));
        g.setFont (14.0f);
        g.drawFittedText ("Drag & Drop WAV Samples Here (Edges = Resize Key/Velocity | Center = Move)",
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
        g.fillRoundedRectangle (rect, 2.0f);
        g.setColour (isSel ? juce::Colours::white : col.brighter (0.3f));
        g.drawRoundedRectangle (rect, 2.0f, isSel ? 2.0f : 1.0f);

        // Visual grab handles on selected zone for super easy dragging!
        if (isSel)
        {
            g.setColour (juce::Colours::white);
            g.fillRect (rect.getX() - 2.0f, rect.getCentreY() - 6.0f, 4.0f, 12.0f);
            g.fillRect (rect.getRight() - 2.0f, rect.getCentreY() - 6.0f, 4.0f, 12.0f);
            g.fillRect (rect.getCentreX() - 6.0f, rect.getY() - 2.0f, 12.0f, 4.0f);
            g.fillRect (rect.getCentreX() - 6.0f, rect.getBottom() - 2.0f, 12.0f, 4.0f);
        }

        float rootX = ((float)(juce::jlimit (minNote, maxNote, z->rootNote) - minNote) + 0.5f) * keyW;
        g.setColour (juce::Colours::yellow);
        g.fillEllipse (rootX - 3.5f, rect.getBottom() - 9.0f, 7.0f, 7.0f);

        if (rect.getWidth() > 36 && rect.getHeight() > 14)
        {
            g.setColour (juce::Colours::black);
            g.setFont (juce::Font (11.0f, isSel ? juce::Font::bold : juce::Font::plain));
            g.drawFittedText (z->name, rect.toNearestInt().reduced (4, 2), juce::Justification::topLeft, 1);
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
        dragMode = hitTestDragMode (hitRect, e.position);
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

            if (std::abs (dVel) > 3) // small deadzone so horizontal moves stay clean
            {
                int spanV = origHighV - origLowV;
                int newLowV = juce::jlimit (0, 127 - spanV, origLowV + dVel);
                z->lowVel = newLowV; z->highVel = newLowV + spanV;
            }
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
    int grp = (activeGroupFilter >= 0) ? activeGroupFilter : targetDropGroup;
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
    g.fillAll (juce::Colour (0xff191b20));
    g.setColour (juce::Colour (0xff4a4e57));
    g.drawRect (getLocalBounds(), 1);

    if (!zone || zone->buffer.getNumSamples() == 0)
    {
        g.setColour (juce::Colours::lightgrey);
        g.setFont (14.0f);
        g.drawFittedText ("Click any Sample Zone in Mapping Editor above to edit its Waveform & Auto-Loop",
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

// ==================== EditRackContent ====================
void EditRackContent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff25282f));
    int w = getWidth();

    auto drawSectionHeader = [&](int y, const juce::String& title) {
        g.setColour (juce::Colour (0xff1a1d23));
        g.fillRoundedRectangle (4.0f, (float)y, (float)(w - 8), 24.0f, 4.0f);
        g.setColour (juce::Colour (0xffd9a05b));
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (title, 12, y, w - 24, 24, juce::Justification::centredLeft);
    };

    drawSectionHeader (4,   "1. GROUP EDITOR  (Click + to add individual Groups | Unlimited Round Robin)");
    drawSectionHeader (228, "2. MAPPING EDITOR  (Drag edges to resize Key/Velocity | Click zone to edit Wave below)");
    drawSectionHeader (588, "3. WAVE EDITOR  (Zero-Cross Auto-Loop & Sample Start/End)");
    drawSectionHeader (900, "4. FX & ADSR RACK  (EQ, Filter, Drive, Chorus, Stereo Delay, Reverb, ADSR)");
}

// ==================== Main Editor ====================
NoiseMu6ixSamplerAudioProcessorEditor::NoiseMu6ixSamplerAudioProcessorEditor (NoiseMu6ixSamplerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), groupCards (p), mappingGrid (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize (1160, 720);

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
                    refreshGroupUI(); refreshSelectedZoneUI(); refreshFXUI(); mappingGrid.repaint();
                }
            });
    };
    addAndMakeVisible (loadInstBtn);

    loadFilesBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Select WAV Samples", juce::File{}, "*.wav;*.flac;*.aif");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems,
            [this](const juce::FileChooser& fc) {
                for (auto& f : fc.getResults()) processor.loadSampleFile (f, selectedGroupIdx, true, -1);
                refreshGroupUI(); refreshSelectedZoneUI(); mappingGrid.repaint();
            });
    };
    addAndMakeVisible (loadFilesBtn);

    loadFolderBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Select Sample Folder to Auto-Map");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc) {
                processor.loadSampleFolder (fc.getResult(), selectedGroupIdx);
                refreshGroupUI(); refreshSelectedZoneUI(); mappingGrid.repaint();
            });
    };
    addAndMakeVisible (loadFolderBtn);

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
            if (f.hasFileExtension (".nm6i")) processor.loadMonolithInstrument (f);
            else processor.loadSampleFile (f, selectedGroupIdx, true, -1);
            const juce::ScopedLock sl (processor.dataLock);
            mappingGrid.selectedZoneIdx = (int)processor.zones.size() - 1;
            refreshGroupUI(); refreshSelectedZoneUI(); refreshFXUI(); mappingGrid.repaint();
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
        refreshGroupUI(); refreshSelectedZoneUI(); refreshFXUI(); mappingGrid.repaint();
    };
    fileListBox.setRowHeight (22);
    fileListBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1f2126));
    addAndMakeVisible (fileListBox);

    // Scrollable Edit Viewport
    editViewport.setViewedComponent (&editRack, false);
    editViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (editViewport);

    // --- Section 1: Group Editor inside editRack ---
    addGroupBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2f6642));
    addGroupBtn.onClick = [this] {
        selectedGroupIdx = processor.addGroup();
        groupCards.selectedGroupIdx = selectedGroupIdx;
        mappingGrid.targetDropGroup = selectedGroupIdx;
        refreshGroupUI();
    };
    editRack.addAndMakeVisible (addGroupBtn);

    deleteGroupBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff662f2f));
    deleteGroupBtn.onClick = [this] {
        processor.removeGroup (selectedGroupIdx);
        selectedGroupIdx = std::max (0, selectedGroupIdx - 1);
        groupCards.selectedGroupIdx = selectedGroupIdx;
        mappingGrid.targetDropGroup = selectedGroupIdx;
        refreshGroupUI();
        mappingGrid.repaint();
    };
    editRack.addAndMakeVisible (deleteGroupBtn);

    rrToggleBtn.onClick = [this] {
        bool next = !processor.rrEnabled.load();
        processor.rrEnabled.store (next);
        refreshGroupUI();
    };
    editRack.addAndMakeVisible (rrToggleBtn);

    rrModeBox.addItem ("RR Mode: Cycle", 1);
    rrModeBox.addItem ("RR Mode: No-Repeat Random", 2);
    rrModeBox.setSelectedId (processor.rrMode.load() + 1, juce::dontSendNotification);
    rrModeBox.onChange = [this] { processor.rrMode.store (rrModeBox.getSelectedId() - 1); };
    editRack.addAndMakeVisible (rrModeBox);

    filterByGroupBtn.onClick = [this] {
        mappingGrid.activeGroupFilter = filterByGroupBtn.getToggleState() ? selectedGroupIdx : -1;
        mappingGrid.repaint();
    };
    editRack.addAndMakeVisible (filterByGroupBtn);

    groupCards.onGroupSelected = [this](int idx) {
        selectedGroupIdx = idx;
        mappingGrid.targetDropGroup = idx;
        if (filterByGroupBtn.getToggleState())
            mappingGrid.activeGroupFilter = idx;
        refreshGroupUI();
        mappingGrid.repaint();
    };
    editRack.addAndMakeVisible (groupCards);

    grpNameLabel.setEditable (true);
    grpNameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1b1d22));
    grpNameLabel.setColour (juce::Label::outlineColourId, juce::Colour (0xff4a4e57));
    grpNameLabel.onTextChange = [this] {
        const juce::ScopedLock sl (processor.dataLock);
        if (selectedGroupIdx >= 0 && selectedGroupIdx < (int)processor.groups.size())
            processor.groups[(size_t)selectedGroupIdx].name = grpNameLabel.getText();
        groupCards.repaint();
    };
    editRack.addAndMakeVisible (grpNameLabel);

    groupSoloBtn.onClick = [this] { applyGroupInspector(); };
    groupMuteBtn.onClick = [this] { applyGroupInspector(); };
    editRack.addAndMakeVisible (groupSoloBtn);
    editRack.addAndMakeVisible (groupMuteBtn);

    auto setupRackHSlider = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV, std::function<void()> cb) {
        s.setRange (minV, maxV, step);
        s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
        s.onValueChange = std::move (cb);
        editRack.addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centredRight);
        l.setFont (11.5f);
        editRack.addAndMakeVisible (l);
    };
    setupRackHSlider (grpVolSlider,  grpVolLbl,  -24, 12, 0.1, 0.0, [this]{ applyGroupInspector(); });
    setupRackHSlider (grpPanSlider,  grpPanLbl,  -1, 1, 0.01, 0.0,  [this]{ applyGroupInspector(); });
    setupRackHSlider (grpTuneSlider, grpTuneLbl, -24, 24, 0.1, 0.0, [this]{ applyGroupInspector(); });

    // --- Section 2: Mapping Editor inside editRack ---
    mappingGrid.onZoneChanged = [this] { refreshSelectedZoneUI(); groupCards.repaint(); };
    editRack.addAndMakeVisible (mappingGrid);

    autoSpreadBtn.onClick = [this] { processor.autoSpreadKeyRanges(); mappingGrid.repaint(); refreshSelectedZoneUI(); };
    delZoneBtn.onClick = [this] {
        if (mappingGrid.selectedZoneIdx >= 0)
        {
            processor.removeZone (mappingGrid.selectedZoneIdx);
            mappingGrid.selectedZoneIdx = -1;
            mappingGrid.repaint();
            refreshSelectedZoneUI();
            groupCards.repaint();
        }
    };
    editRack.addAndMakeVisible (autoSpreadBtn);
    editRack.addAndMakeVisible (delZoneBtn);
    editRack.addAndMakeVisible (selZoneNameLbl);

    auto setupZoneNum = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV) {
        s.setRange (minV, maxV, step); s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 42, 20);
        s.onValueChange = [this] { applyZoneBarToSelected(); };
        editRack.addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centred); l.setFont (11.0f);
        editRack.addAndMakeVisible (l);
    };
    setupZoneNum (zRootSlider,  zRootLbl,  0, 127, 1, 60);
    setupZoneNum (zLowKSlider,  zLowKLbl,  0, 127, 1, 48);
    setupZoneNum (zHighKSlider, zHighKLbl, 0, 127, 1, 72);
    setupZoneNum (zLowVSlider,  zLowVLbl,  0, 127, 1, 0);
    setupZoneNum (zHighVSlider, zHighVLbl, 0, 127, 1, 127);
    setupZoneNum (zVolSlider,   zVolLbl,   -24, 12, 0.5, 0);
    setupZoneNum (zPanSlider,   zPanLbl,   -1, 1, 0.05, 0);
    setupZoneNum (zTuneSlider,  zTuneLbl,  -24, 24, 0.5, 0);

    // --- Section 3: Wave Editor inside editRack ---
    editRack.addAndMakeVisible (waveEditor);
    editRack.addAndMakeVisible (loopToggle);
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
    editRack.addAndMakeVisible (autoLoopBtn);

    waveDragModeBox.addItem ("Drag: Loop Start/End", 1);
    waveDragModeBox.addItem ("Drag: Sample Start/End", 2);
    waveDragModeBox.setSelectedId (1, juce::dontSendNotification);
    waveDragModeBox.onChange = [this] { waveEditor.setEditMode (waveDragModeBox.getSelectedId() == 2); };
    editRack.addAndMakeVisible (waveDragModeBox);

    setupRackHSlider (crossfadeSlider, cfLbl, 64, 15000, 1, 2048, [this] {
        const juce::ScopedLock sl (processor.dataLock);
        int idx = mappingGrid.selectedZoneIdx;
        if (idx >= 0 && idx < (int)processor.zones.size())
            processor.zones[(size_t)idx]->crossfadeSamples.store ((int)crossfadeSlider.getValue());
    });
    setupRackHSlider (zoomSlider, zoomLbl, 1.0, 16.0, 0.1, 1.0, [this] { waveEditor.setZoom ((float)zoomSlider.getValue()); });

    // --- Section 4: FX & ADSR Rack inside editRack ---
    auto setupFXToggle = [this](juce::ToggleButton& b) {
        b.onClick = [this] { applyFXToProcessor(); };
        editRack.addAndMakeVisible (b);
    };
    setupFXToggle (fxEqOn); setupFXToggle (fxFltOn); setupFXToggle (fxDrvOn);
    setupFXToggle (fxChoOn); setupFXToggle (fxDlyOn); setupFXToggle (fxRevOn);

    fxFltTypeBox.addItem ("Lowpass", 1);
    fxFltTypeBox.addItem ("Highpass", 2);
    fxFltTypeBox.addItem ("Bandpass", 3);
    fxFltTypeBox.setSelectedId (1, juce::dontSendNotification);
    fxFltTypeBox.onChange = [this] { applyFXToProcessor(); };
    editRack.addAndMakeVisible (fxFltTypeBox);

    auto setupFXKnob = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV) {
        s.setRange (minV, maxV, step);
        s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 15);
        s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffd9a05b));
        s.setColour (juce::Slider::thumbColourId, juce::Colour (0xffd9a05b));
        s.onValueChange = [this] { applyFXToProcessor(); };
        editRack.addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (11.0f);
        editRack.addAndMakeVisible (l);
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

    refreshGroupUI();
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

void NoiseMu6ixSamplerAudioProcessorEditor::refreshGroupUI()
{
    bool rrOn = processor.rrEnabled.load();
    rrToggleBtn.setButtonText (rrOn ? "Round Robin: ON (Unlimited)" : "Round Robin: OFF (Layer)");
    rrToggleBtn.setColour (juce::TextButton::buttonColourId, rrOn ? juce::Colour (0xff2a7a4b) : juce::Colour (0xff4a4e57));
    rrModeBox.setEnabled (rrOn);

    const juce::ScopedLock sl (processor.dataLock);
    selectedGroupIdx = juce::jlimit (0, std::max (0, (int)processor.groups.size() - 1), selectedGroupIdx);
    groupCards.selectedGroupIdx = selectedGroupIdx;
    groupCards.repaint();

    if (selectedGroupIdx < (int)processor.groups.size())
    {
        const auto& g = processor.groups[(size_t)selectedGroupIdx];
        grpNameLabel.setText (g.name, juce::dontSendNotification);
        groupSoloBtn.setToggleState (g.solo, juce::dontSendNotification);
        groupMuteBtn.setToggleState (g.mute, juce::dontSendNotification);
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
    g.volumeDb = (float)grpVolSlider.getValue();
    g.pan      = (float)grpPanSlider.getValue();
    g.tuneSemi = (float)grpTuneSlider.getValue();
    groupCards.repaint();
}

void NoiseMu6ixSamplerAudioProcessorEditor::refreshSelectedZoneUI()
{
    const juce::ScopedLock sl (processor.dataLock);
    int idx = mappingGrid.selectedZoneIdx;
    if (idx < 0 || idx >= (int)processor.zones.size())
    {
        selZoneNameLbl.setText ("Sample: (click a zone on grid)", juce::dontSendNotification);
        waveEditor.setZone (nullptr);
        return;
    }
    auto z = processor.zones[(size_t)idx];
    juce::String rootName = juce::MidiMessage::getMidiNoteName (z->rootNote, true, true, 3);
    selZoneNameLbl.setText ("Sample: " + z->name + " [" + rootName + "]", juce::dontSendNotification);
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
    editViewport.setVisible (inEditMode);

    bool showFront = !inEditMode;
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
    g.setFont (juce::Font (17.5f, juce::Font::bold));
    g.drawText ("NoiseMu6ixSampler_v1.0.4", 12, 10, 260, 26, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff23252b));
    g.fillRect (0, 46, 220, getHeight() - 46);
    g.setColour (juce::Colour (0xff3c4049));
    g.drawVerticalLine (220, 46.0f, (float)getHeight());

    g.setColour (juce::Colour (0xff25282f));
    g.fillRect (226, 52, getWidth() - 232, getHeight() - 148);
    g.setColour (juce::Colour (0xff555963));
    g.drawRect (226, 52, getWidth() - 232, getHeight() - 148, 1);
}

void NoiseMu6ixSamplerAudioProcessorEditor::layoutScrollableRack (int w)
{
    const int totalH = 1380;
    editRack.setBounds (0, 0, w, totalH);
    int pad = 10;
    int innerW = w - pad * 2;

    // ===== 1. GROUP EDITOR (y = 32..220) =====
    int y = 34;
    addGroupBtn.setBounds      (pad, y, 110, 26);
    deleteGroupBtn.setBounds   (pad + 116, y, 80, 26);
    rrToggleBtn.setBounds      (pad + 206, y, 210, 26);
    rrModeBox.setBounds        (pad + 422, y, 185, 26);
    filterByGroupBtn.setBounds (pad + 616, y, 155, 26);

    y += 32;
    groupCards.setBounds (pad, y, innerW, 112);

    y += 118;
    grpNameLabel.setBounds  (pad, y, 150, 24);
    groupSoloBtn.setBounds  (pad + 158, y, 60, 24);
    groupMuteBtn.setBounds  (pad + 220, y, 60, 24);
    grpVolLbl.setBounds     (pad + 286, y, 48, 24);
    grpVolSlider.setBounds  (pad + 336, y, 140, 24);
    grpPanLbl.setBounds     (pad + 482, y, 36, 24);
    grpPanSlider.setBounds  (pad + 520, y, 130, 24);
    grpTuneLbl.setBounds    (pad + 656, y, 40, 24);
    grpTuneSlider.setBounds (pad + 698, y, 140, 24);

    // ===== 2. MAPPING EDITOR (y = 258..580) =====
    y = 258;
    selZoneNameLbl.setBounds (pad, y, 380, 24);
    autoSpreadBtn.setBounds  (w - pad - 230, y, 135, 24);
    delZoneBtn.setBounds     (w - pad - 88,  y, 88,  24);

    y += 28;
    int colW = innerW / 8;
    auto placeCol = [&](int idx, juce::Label& l, juce::Slider& s) {
        int cx = pad + idx * colW + 2;
        l.setBounds (cx, y, colW - 4, 15);
        s.setBounds (cx, y + 16, colW - 4, 22);
    };
    placeCol (0, zRootLbl,  zRootSlider);
    placeCol (1, zLowKLbl,  zLowKSlider);
    placeCol (2, zHighKLbl, zHighKSlider);
    placeCol (3, zLowVLbl,  zLowVSlider);
    placeCol (4, zHighVLbl, zHighVSlider);
    placeCol (5, zVolLbl,   zVolSlider);
    placeCol (6, zPanLbl,   zPanSlider);
    placeCol (7, zTuneLbl,  zTuneSlider);

    y += 44;
    mappingGrid.setBounds (pad, y, innerW, 246);

    // ===== 3. WAVE EDITOR (y = 618..892) =====
    y = 618;
    loopToggle.setBounds      (pad, y, 105, 26);
    autoLoopBtn.setBounds     (pad + 110, y, 170, 26);
    waveDragModeBox.setBounds (pad + 288, y, 175, 26);
    cfLbl.setBounds           (pad + 470, y, 65, 26);
    crossfadeSlider.setBounds (pad + 538, y, 150, 26);
    zoomLbl.setBounds         (pad + 694, y, 42, 26);
    zoomSlider.setBounds      (pad + 738, y, 130, 26);

    y += 32;
    waveEditor.setBounds (pad, y, innerW, 236);

    // ===== 4. FX & ADSR RACK (y = 932..1370) =====
    auto layoutModule = [](juce::Rectangle<int> box, juce::ToggleButton* tog, juce::ComboBox* combo,
                           std::vector<std::pair<juce::Label*, juce::Slider*>> knobs) {
        auto top = box.removeFromTop (24);
        if (tog) tog->setBounds (top.removeFromLeft (115));
        if (combo) combo->setBounds (top.removeFromRight (95));
        int kw = box.getWidth() / std::max (1, (int)knobs.size());
        for (auto& kv : knobs)
        {
            auto c = box.removeFromLeft (kw).reduced (2, 2);
            kv.first->setBounds (c.removeFromTop (15));
            kv.second->setBounds (c);
        }
    };

    int w3 = innerW / 3;
    juce::Rectangle<int> r1 (pad, 932, innerW, 145);
    layoutModule (r1.removeFromLeft (w3).reduced (4), &fxEqOn, nullptr,
                  { {&lEqLow, &fxEqLow}, {&lEqMid, &fxEqMid}, {&lEqHigh, &fxEqHigh} });
    layoutModule (r1.removeFromLeft (w3).reduced (4), &fxFltOn, &fxFltTypeBox,
                  { {&lFltCut, &fxFltCut}, {&lFltRes, &fxFltRes} });
    layoutModule (r1.reduced (4), &fxDrvOn, nullptr,
                  { {&lDrvAmt, &fxDrvAmt} });

    juce::Rectangle<int> r2 (pad, 1082, innerW, 145);
    layoutModule (r2.removeFromLeft (w3).reduced (4), &fxChoOn, nullptr,
                  { {&lChoRate, &fxChoRate}, {&lChoDepth, &fxChoDepth}, {&lChoMix, &fxChoMix} });
    layoutModule (r2.removeFromLeft (w3).reduced (4), &fxDlyOn, nullptr,
                  { {&lDlyTime, &fxDlyTime}, {&lDlyFb, &fxDlyFb}, {&lDlyMix, &fxDlyMix} });
    layoutModule (r2.reduced (4), &fxRevOn, nullptr,
                  { {&lRevSize, &fxRevSize}, {&lRevDamp, &fxRevDamp}, {&lRevMix, &fxRevMix} });

    juce::Rectangle<int> r3 (pad, 1232, innerW, 135);
    layoutModule (r3.reduced (4), nullptr, nullptr,
                  { {&lAtk, &fxAtk}, {&lDec, &fxDec}, {&lSus, &fxSus}, {&lRel, &fxRel} });
}

void NoiseMu6ixSamplerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header
    auto header = area.removeFromTop (46).reduced (8, 8);
    header.removeFromLeft (260);
    modeToggleBtn.setBounds (header.removeFromLeft (110)); header.removeFromLeft (8);
    saveInstBtn.setBounds   (header.removeFromLeft (95));  header.removeFromLeft (6);
    loadInstBtn.setBounds   (header.removeFromLeft (95));  header.removeFromLeft (10);
    loadFilesBtn.setBounds  (header.removeFromLeft (90));  header.removeFromLeft (6);
    loadFolderBtn.setBounds (header.removeFromLeft (90));
    voicesLabel.setBounds   (header.removeFromRight (95));

    // Left Sidebar
    auto left = area.removeFromLeft (220).reduced (8, 8);
    browseFolderBtn.setBounds (left.removeFromTop (28)); left.removeFromTop (6);
    folderPathLabel.setBounds (left.removeFromTop (20)); left.removeFromTop (4);
    addSelectedFileBtn.setBounds (left.removeFromBottom (30)); left.removeFromBottom (6);
    fileListBox.setBounds (left);

    keyboard.setBounds (area.removeFromBottom (88).reduced (8, 4));
    auto rack = area.reduced (8, 8);

    if (inEditMode)
    {
        editViewport.setBounds (rack);
        layoutScrollableRack (rack.getWidth() - editViewport.getScrollBarThickness() - 2);
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
