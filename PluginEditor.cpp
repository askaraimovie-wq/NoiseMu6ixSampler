#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

// ==================== LogoSizeButton ====================
void LogoSizeButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (hovered)
    {
        g.setColour (juce::Colour (0xff252933));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colour (0xffd9a05b).withAlpha (0.6f));
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }

    g.setColour (juce::Colour (0xffd9a05b));
    g.fillRoundedRectangle (4.0f, 5.0f, 24.0f, 24.0f, 5.0f);
    g.setColour (juce::Colour (0xff111317));
    g.setFont (juce::Font (13.0f, juce::Font::bold));
    g.drawText ("N6", 4, 5, 24, 24, juce::Justification::centred);

    g.setColour (juce::Colour (0xfff0c078));
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.drawText ("NOISE MU6IX", 34, 2, 145, 20, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff9aa0ae));
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("STUDIO SAMPLER  \xE2\x96\xBE", 35, 20, 145, 12, juce::Justification::centredLeft);
}

// ==================== LibraryBrowserComponent ====================
void LibraryBrowserComponent::refreshHeight()
{
    int y = 4;
    for (const auto& lib : libraries)
    {
        y += bannerH + 4;
        if (lib.expanded)
            y += std::max (1, lib.instruments.size()) * itemH + 6;
    }
    setSize (getWidth(), std::max (y + 10, 200));
    repaint();
}

void LibraryBrowserComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff181a1f));
    if (libraries.empty())
    {
        g.setColour (juce::Colour (0xff8a909e));
        g.setFont (11.5f);
        g.drawFittedText ("Click '+ Add Lib' to add a folder of .nm6i instruments & set a cover photo",
                          getLocalBounds().reduced (10), juce::Justification::centred, 4);
        return;
    }

    int y = 4;
    int w = getWidth() - 8;
    for (int i = 0; i < (int)libraries.size(); ++i)
    {
        const auto& lib = libraries[(size_t)i];
        juce::Rectangle<int> bRect (4, y, w, bannerH);
        bool isSel = (i == selectedLibraryIdx);

        // Draw Banner Photo or stylish gradient
        if (lib.cachedImage.isValid())
        {
            g.drawImage (lib.cachedImage, bRect.toFloat(), juce::RectanglePlacement::fillDestination);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (bRect.toFloat(), 4.0f);
        }
        else
        {
            juce::ColourGradient grad (juce::Colour (0xff2e3442), 4.0f, (float)y,
                                       juce::Colour (0xff1c2028), 4.0f, (float)(y + bannerH), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bRect.toFloat(), 4.0f);
        }

        g.setColour (isSel ? juce::Colour (0xffd9a05b) : juce::Colour (0xff4a4f5c));
        g.drawRoundedRectangle (bRect.toFloat(), 4.0f, isSel ? 1.8f : 1.0f);

        // Title & expand arrow
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (lib.title, bRect.reduced (8, 4).removeFromTop (22), juce::Justification::centredLeft, true);

        g.setColour (juce::Colour (0xffe0c088));
        g.setFont (10.0f);
        juce::String sub = juce::String (lib.instruments.size()) + " instruments  " + (lib.expanded ? "[-]" : "[+]");
        g.drawText (sub, bRect.reduced (8, 4).removeFromBottom (16), juce::Justification::centredLeft, true);

        y += bannerH + 4;

        // Expanded instruments list
        if (lib.expanded)
        {
            if (lib.instruments.isEmpty())
            {
                g.setColour (juce::Colour (0xff8a909e));
                g.setFont (10.5f);
                g.drawText ("(No .nm6i files in folder)", 12, y, w - 16, itemH, juce::Justification::centredLeft, true);
                y += itemH + 4;
            }
            else
            {
                for (int j = 0; j < lib.instruments.size(); ++j)
                {
                    juce::Rectangle<int> iRect (10, y, w - 12, itemH - 2);
                    g.setColour (juce::Colour (0xff252933));
                    g.fillRoundedRectangle (iRect.toFloat(), 3.0f);
                    g.setColour (juce::Colour (0xff3c4252));
                    g.drawRoundedRectangle (iRect.toFloat(), 3.0f, 0.8f);

                    g.setColour (juce::Colour (0xffd9a05b));
                    g.fillEllipse ((float)iRect.getX() + 6.0f, (float)iRect.getCentreY() - 3.0f, 6.0f, 6.0f);

                    g.setColour (juce::Colours::white);
                    g.setFont (11.0f);
                    g.drawText (lib.instruments[j].getFileNameWithoutExtension(),
                                iRect.withTrimmedLeft (16).reduced (2, 0),
                                juce::Justification::centredLeft, true);
                    y += itemH;
                }
                y += 4;
            }
        }
    }
}

void LibraryBrowserComponent::mouseDown (const juce::MouseEvent& e)
{
    int y = 4;
    int w = getWidth() - 8;
    for (int i = 0; i < (int)libraries.size(); ++i)
    {
        auto& lib = libraries[(size_t)i];
        juce::Rectangle<int> bRect (4, y, w, bannerH);
        if (bRect.contains (e.getPosition()))
        {
            selectedLibraryIdx = i;
            lib.expanded = !lib.expanded;
            refreshHeight();
            if (onLibrarySelected) onLibrarySelected (i);
            return;
        }
        y += bannerH + 4;

        if (lib.expanded)
        {
            if (lib.instruments.isEmpty())
            {
                y += itemH + 4;
            }
            else
            {
                for (int j = 0; j < lib.instruments.size(); ++j)
                {
                    juce::Rectangle<int> iRect (10, y, w - 12, itemH - 2);
                    if (iRect.contains (e.getPosition()))
                    {
                        if (onInstrumentClicked) onInstrumentClicked (lib.instruments[j]);
                        return;
                    }
                    y += itemH;
                }
                y += 4;
            }
        }
    }
}

// ==================== RootUICanvas ====================
void RootUICanvas::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d22));
    g.setColour (juce::Colour (0xff111317));
    g.fillRect (0, 0, getWidth(), 44);
    g.setColour (juce::Colour (0xff2e323c));
    g.drawHorizontalLine (44, 0.0f, (float)getWidth());

    g.setColour (juce::Colour (0xff23252b));
    g.fillRect (0, 44, 196, getHeight() - 44);
    g.setColour (juce::Colour (0xff3c4049));
    g.drawVerticalLine (196, 44.0f, (float)getHeight());

    g.setColour (juce::Colour (0xff22252b));
    g.fillRect (202, 48, getWidth() - 208, getHeight() - 124);
    g.setColour (juce::Colour (0xff4a4f59));
    g.drawRect (202, 48, getWidth() - 208, getHeight() - 124, 1);
}

// ==================== ColoredMidiKeyboardComponent ====================
ColoredMidiKeyboardComponent::ColoredMidiKeyboardComponent (NoiseMu6ixSamplerAudioProcessor& p,
                                                            juce::MidiKeyboardState& state)
    : juce::MidiKeyboardComponent (state, juce::MidiKeyboardComponent::horizontalKeyboard),
      processor (p)
{
    setAvailableRange (24, 96);
    setScrollButtonsVisible (false);
}

bool ColoredMidiKeyboardComponent::getKeyColourInfo (int note, juce::Colour& outCol, bool& isKeyswitch) const
{
    const juce::ScopedTryLock sl (processor.dataLock);
    if (!sl.isLocked()) return false;
    const auto& slot = processor.getSlot (activeSlotIdx);
    if (!slot.loaded) return false;

    if (slot.keyswitchMasterOn.load())
    {
        for (const auto& g : slot.groups)
        {
            if (g.keyswitchEnabled && g.keyswitchNote == note)
            {
                isKeyswitch = true;
                outCol = NoiseMu6ixSamplerAudioProcessor::getKeyswitchPaletteColour (g.keyswitchColorIdx);
                if (slot.activeKeyswitchNote.load() == note)
                    outCol = outCol.brighter (0.35f);
                return true;
            }
        }
    }

    for (const auto& z : slot.zones)
    {
        if (activeFilterGroupIdx >= 0 && z->groupIndex != activeFilterGroupIdx)
            continue;
        if (note >= z->lowKey && note <= z->highKey)
        {
            isKeyswitch = false;
            outCol = juce::Colour (0xff2b82e3);
            return true;
        }
    }
    return false;
}

void ColoredMidiKeyboardComponent::drawWhiteNote (int midiNoteNumber, juce::Graphics& g,
                                                  juce::Rectangle<float> area,
                                                  bool isDown, bool isOver,
                                                  juce::Colour lineColour, juce::Colour textColour)
{
    juce::Colour c = juce::Colour (0xfff0f2f5);
    juce::Colour customCol;
    bool isKS = false;
    if (getKeyColourInfo (midiNoteNumber, customCol, isKS)) c = customCol;
    if (isDown)      c = c.brighter (0.45f);
    else if (isOver) c = c.interpolatedWith (juce::Colours::white, 0.2f);

    g.setColour (c);
    g.fillRect (area);

    auto text = getWhiteNoteText (midiNoteNumber);
    if (text.isNotEmpty())
    {
        g.setColour (isKS || c != juce::Colour (0xfff0f2f5) ? juce::Colours::white : textColour);
        g.setFont (juce::jmin (10.5f, area.getWidth() * 0.7f));
        g.drawText (text, area.withTrimmedBottom (2.0f), juce::Justification::centredBottom, false);
    }
    g.setColour (lineColour);
    g.drawRect (area, 0.7f);
}

void ColoredMidiKeyboardComponent::drawBlackNote (int midiNoteNumber, juce::Graphics& g,
                                                  juce::Rectangle<float> area,
                                                  bool isDown, bool isOver,
                                                  juce::Colour noteFillColour)
{
    juce::Colour c = noteFillColour;
    juce::Colour customCol;
    bool isKS = false;
    if (getKeyColourInfo (midiNoteNumber, customCol, isKS)) c = customCol.darker (0.2f);
    if (isDown)      c = c.brighter (0.5f);
    else if (isOver) c = c.brighter (0.2f);

    g.setColour (c);
    g.fillRect (area);
    g.setColour (juce::Colours::black);
    g.drawRect (area, 0.7f);
}

// ==================== GroupCardsComponent ====================
GroupCardsComponent::GroupCardsComponent (NoiseMu6ixSamplerAudioProcessor& p) : processor (p) {}

juce::Rectangle<int> GroupCardsComponent::getCardRect (int idx) const
{
    int cols = std::max (1, getWidth() / 135);
    int cardW = (getWidth() - 8) / cols;
    int cardH = 38;
    return { 4 + (idx % cols) * cardW, 4 + (idx / cols) * (cardH + 4), cardW - 6, cardH };
}

void GroupCardsComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1e24));
    g.setColour (juce::Colour (0xff3e424c));
    g.drawRect (getLocalBounds(), 1);

    const juce::ScopedLock sl (processor.dataLock);
    const auto& slot = processor.getSlot (activeSlotIdx);
    for (int i = 0; i < (int)slot.groups.size(); ++i)
    {
        auto r = getCardRect (i);
        if (r.getY() >= getHeight()) break;

        const auto& grp = slot.groups[(size_t)i];
        bool isSel = (i == selectedGroupIdx);
        int zCount = 0;
        for (auto& z : slot.zones) if (z->groupIndex == i) ++zCount;

        g.setColour (isSel ? juce::Colour (0xffd9a05b) : juce::Colour (0xff2e323c));
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
        if (grp.keyswitchEnabled)
        {
            g.setColour (NoiseMu6ixSamplerAudioProcessor::getKeyswitchPaletteColour (grp.keyswitchColorIdx));
            g.fillRoundedRectangle ((float)r.getX(), (float)r.getY(), 5.0f, (float)r.getHeight(), 2.0f);
        }
        g.setColour (isSel ? juce::Colours::white : juce::Colour (0xff555a66));
        g.drawRoundedRectangle (r.toFloat(), 4.0f, isSel ? 1.6f : 1.0f);

        g.setColour (isSel ? juce::Colour (0xff111317) : juce::Colours::white);
        g.setFont (juce::Font (12.0f, isSel ? juce::Font::bold : juce::Font::plain));
        g.drawText (grp.name, r.reduced (10, 2).removeFromTop (18), juce::Justification::centredLeft, true);

        juce::String sub = juce::String (zCount) + "z";
        if (grp.keyswitchEnabled) sub += " | KS:" + juce::MidiMessage::getMidiNoteName (grp.keyswitchNote, true, true, 3);
        if (grp.solo) sub += " [S]";
        if (grp.mute) sub += " [M]";
        g.setColour (isSel ? juce::Colour (0xff2c2419) : juce::Colour (0xffa8adb8));
        g.setFont (10.0f);
        g.drawText (sub, r.reduced (10, 2).removeFromBottom (14), juce::Justification::centredLeft, true);
    }
}

void GroupCardsComponent::mouseDown (const juce::MouseEvent& e)
{
    const juce::ScopedLock sl (processor.dataLock);
    const auto& slot = processor.getSlot (activeSlotIdx);
    for (int i = 0; i < (int)slot.groups.size(); ++i)
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
    float frac = juce::jlimit (0.0f, 1.0f, x / (float)std::max (1, getWidth()));
    return juce::jlimit (minNote, maxNote, minNote + (int)std::round (frac * (maxNote - minNote)));
}

int MappingGridComponent::yToVel (float y) const
{
    float frac = juce::jlimit (0.0f, 1.0f, 1.0f - (y / (float)std::max (1, getHeight())));
    return juce::jlimit (0, 127, (int)std::round (frac * 127.0f));
}

juce::Rectangle<float> MappingGridComponent::getZoneRect (const SampleZone& z) const
{
    float w = (float)std::max (1, getWidth()), h = (float)std::max (1, getHeight());
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
    if (std::abs (pt.x - r.getX()) <= edgeX)      return DragMode::LeftEdge;
    if (std::abs (pt.x - r.getRight()) <= edgeX)  return DragMode::RightEdge;
    if (std::abs (pt.y - r.getY()) <= edgeY)      return DragMode::TopEdge;
    if (std::abs (pt.y - r.getBottom()) <= edgeY) return DragMode::BottomEdge;
    return DragMode::Move;
}

void MappingGridComponent::mouseMove (const juce::MouseEvent& e)
{
    const juce::ScopedLock sl (processor.dataLock);
    const auto& slot = processor.getSlot (activeSlotIdx);
    for (int i = (int)slot.zones.size() - 1; i >= 0; --i)
    {
        const auto& z = slot.zones[(size_t)i];
        if (activeGroupFilter >= 0 && z->groupIndex != activeGroupFilter) continue;
        auto r = getZoneRect (*z);
        if (r.contains (e.position))
        {
            auto mode = hitTestDragMode (r, e.position);
            if (mode == DragMode::LeftEdge || mode == DragMode::RightEdge) setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            else if (mode == DragMode::TopEdge || mode == DragMode::BottomEdge) setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            else setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            return;
        }
    }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void MappingGridComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff26282e));
    float w = (float)getWidth(), h = (float)getHeight();
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
    const auto& slot = processor.getSlot (activeSlotIdx);
    int visibleCount = 0;
    for (auto& z : slot.zones)
        if (activeGroupFilter < 0 || z->groupIndex == activeGroupFilter) ++visibleCount;

    if (visibleCount == 0)
    {
        g.setColour (juce::Colour (0xffb0b5c0));
        g.setFont (13.0f);
        g.drawFittedText ("Drag & Drop WAV Samples Here for this Group", getLocalBounds(), juce::Justification::centred, 2);
        return;
    }

    static const juce::uint32 palette[6] = { 0xffd9a05b, 0xff5ba8d9, 0xff7cc47f, 0xffc97575, 0xffa77fd4, 0xffd4c25b };
    for (size_t i = 0; i < slot.zones.size(); ++i)
    {
        const auto& z = slot.zones[i];
        if (activeGroupFilter >= 0 && z->groupIndex != activeGroupFilter) continue;

        auto rect = getZoneRect (*z);
        bool isSel = ((int)i == selectedZoneIdx);
        auto col = juce::Colour (palette[(size_t)(std::max (0, z->groupIndex)) % 6]);

        g.setColour (col.withAlpha (isSel ? 0.85f : 0.48f));
        g.fillRoundedRectangle (rect, 2.0f);
        g.setColour (isSel ? juce::Colours::white : col.brighter (0.3f));
        g.drawRoundedRectangle (rect, 2.0f, isSel ? 2.0f : 1.0f);

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
            g.setFont (juce::Font (10.5f, isSel ? juce::Font::bold : juce::Font::plain));
            g.drawFittedText (z->name, rect.toNearestInt().reduced (4, 2), juce::Justification::topLeft, 1);
        }
    }
}

void MappingGridComponent::mouseDown (const juce::MouseEvent& e)
{
    const juce::ScopedLock sl (processor.dataLock);
    const auto& slot = processor.getSlot (activeSlotIdx);
    int hitIdx = -1;
    juce::Rectangle<float> hitRect;
    for (int i = (int)slot.zones.size() - 1; i >= 0; --i)
    {
        const auto& z = slot.zones[(size_t)i];
        if (activeGroupFilter >= 0 && z->groupIndex != activeGroupFilter) continue;
        auto r = getZoneRect (*z);
        if (r.contains (e.position)) { hitIdx = i; hitRect = r; break; }
    }

    selectedZoneIdx = hitIdx;
    dragMode = DragMode::None;
    if (hitIdx >= 0)
    {
        auto& z = slot.zones[(size_t)hitIdx];
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
    auto& slot = processor.getSlot (activeSlotIdx);
    if (selectedZoneIdx >= (int)slot.zones.size()) return;

    auto& z = slot.zones[(size_t)selectedZoneIdx];
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
            if (std::abs (dVel) > 3)
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
        if (f.endsWithIgnoreCase (".nm6i")) processor.loadSlotMonolith (activeSlotIdx, juce::File (f));
        else processor.loadSampleFile (activeSlotIdx, juce::File (f), grp, files.size() > 1, files.size() == 1 ? dropNote : -1);
    }
    const juce::ScopedLock sl (processor.dataLock);
    auto& slot = processor.getSlot (activeSlotIdx);
    if (!slot.zones.empty()) selectedZoneIdx = (int)slot.zones.size() - 1;
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
        g.setFont (13.0f);
        g.drawFittedText ("Select a Sample Zone in Map Editor to edit Waveform & Auto-Loop",
                          getLocalBounds(), juce::Justification::centred, 1);
        return;
    }

    auto b = getLocalBounds().reduced (4);
    int w = b.getWidth(), h = b.getHeight();
    int numSamples = zone->buffer.getNumSamples();
    int visibleSamples = std::max (256, (int)((float)numSamples / zoomFactor));
    const float* data = zone->buffer.getReadPointer (0);

    auto sampleToX = [&](int s) -> float { return (float)b.getX() + ((float)s / (float)visibleSamples) * (float)w; };
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
        for (int k = 0; k < step && idx + k < numSamples; ++k) peak = std::max (peak, std::abs (data[idx + k]));
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

// ==================== PerformanceMacroWidget ====================
PerformanceMacroWidget::PerformanceMacroWidget (NoiseMu6ixSamplerAudioProcessor& p, int slotIndex, int macroIndex)
    : processor (p), slotIdx (slotIndex), mIdx (macroIndex)
{
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    nameLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    addAndMakeVisible (nameLabel);

    slider.setRange (0.0, 1.0, 0.001);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 15);
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffd9a05b));
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffd9a05b));
    slider.onValueChange = [this] {
        processor.getSlot (slotIdx).macroConfigs[mIdx].value = (float)slider.getValue();
    };
    addAndMakeVisible (slider);
    syncFromConfig();
}

void PerformanceMacroWidget::syncFromConfig()
{
    const auto& cfg = processor.getSlot (slotIdx).macroConfigs[mIdx];
    setVisible (cfg.visible);
    nameLabel.setText (cfg.name, juce::dontSendNotification);
    slider.setSliderStyle (cfg.isSlider ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setValue (cfg.value, juce::dontSendNotification);
    setBounds (cfg.posX, cfg.posY, cfg.width, cfg.height);
    repaint();
}

void PerformanceMacroWidget::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff232730));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f);
    g.setColour (juce::Colour (0xff454b57));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.0f);
}

void PerformanceMacroWidget::resized()
{
    auto r = getLocalBounds().reduced (5);
    nameLabel.setBounds (r.removeFromTop (18));
    r.removeFromTop (2);
    slider.setBounds (r);
}

// ==================== MultiRackSlotCard (Kontakt Multi-Rack Instrument Card) ====================
MultiRackSlotCard::MultiRackSlotCard (NoiseMu6ixSamplerAudioProcessor& p, int sIdx)
    : slotIdx (sIdx), processor (p)
{
    editBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffb86b28));
    editBtn.onClick = [this] { if (onEditClicked) onEditClicked (slotIdx); };
    addAndMakeVisible (editBtn);

    titleLbl.setFont (juce::Font (13.0f, juce::Font::bold));
    titleLbl.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (titleLbl);

    midiChBox.addItem ("Omni", 1);
    for (int ch = 1; ch <= 16; ++ch) midiChBox.addItem ("Ch " + juce::String (ch), ch + 1);
    midiChBox.onChange = [this] { processor.getSlot (slotIdx).midiChannel.store (midiChBox.getSelectedId() - 1); };
    addAndMakeVisible (midiChBox);

    for (int o = 1; o <= 16; ++o) outBusBox.addItem ("Out " + juce::String (o), o);
    outBusBox.onChange = [this] { processor.getSlot (slotIdx).outputBusIndex.store (outBusBox.getSelectedId() - 1); };
    addAndMakeVisible (outBusBox);

    volSlider.setRange (-24.0, 12.0, 0.1);
    volSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volSlider.onValueChange = [this] { processor.getSlot (slotIdx).slotVolumeDb.store ((float)volSlider.getValue()); };
    addAndMakeVisible (volSlider);

    panSlider.setRange (-1.0, 1.0, 0.01);
    panSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    panSlider.onValueChange = [this] { processor.getSlot (slotIdx).slotPan.store ((float)panSlider.getValue()); };
    addAndMakeVisible (panSlider);

    muteBtn.onClick = [this] {
        auto& s = processor.getSlot (slotIdx);
        s.slotMute.store (!s.slotMute.load());
        syncFromSlot();
    };
    addAndMakeVisible (muteBtn);

    soloBtn.onClick = [this] {
        auto& s = processor.getSlot (slotIdx);
        s.slotSolo.store (!s.slotSolo.load());
        syncFromSlot();
    };
    addAndMakeVisible (soloBtn);

    closeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff662c2c));
    closeBtn.onClick = [this] { if (onCloseClicked) onCloseClicked (slotIdx); };
    addAndMakeVisible (closeBtn);

    addAndMakeVisible (panelBody);
    for (int m = 0; m < InstrumentSlot::numMacros; ++m)
    {
        auto w = std::make_unique<PerformanceMacroWidget> (processor, slotIdx, m);
        panelBody.addAndMakeVisible (*w);
        macroWidgets.push_back (std::move (w));
    }

    syncFromSlot();
}

void MultiRackSlotCard::syncFromSlot()
{
    const auto& s = processor.getSlot (slotIdx);
    titleLbl.setText (juce::String (slotIdx + 1) + ". " + s.instrumentName, juce::dontSendNotification);
    midiChBox.setSelectedId (s.midiChannel.load() + 1, juce::dontSendNotification);
    outBusBox.setSelectedId (s.outputBusIndex.load() + 1, juce::dontSendNotification);
    volSlider.setValue (s.slotVolumeDb.load(), juce::dontSendNotification);
    panSlider.setValue (s.slotPan.load(), juce::dontSendNotification);
    muteBtn.setColour (juce::TextButton::buttonColourId, s.slotMute.load() ? juce::Colour (0xffc94040) : juce::Colour (0xff353944));
    soloBtn.setColour (juce::TextButton::buttonColourId, s.slotSolo.load() ? juce::Colour (0xffd9a05b) : juce::Colour (0xff353944));

    for (auto& mw : macroWidgets) mw->syncFromConfig();
    repaint();
}

void MultiRackSlotCard::paint (juce::Graphics& g)
{
    g.setColour (juce::Colour (0xff1c1f26));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    // Kontakt-style Instrument Header Strip
    g.setColour (juce::Colour (0xff2b2f3a));
    g.fillRoundedRectangle (0.0f, 0.0f, (float)getWidth(), 32.0f, 6.0f);

    g.setColour (juce::Colour (0xff4e5463));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.2f);
}

void MultiRackSlotCard::resized()
{
    auto r = getLocalBounds();
    auto hdr = r.removeFromTop (32).reduced (5, 4);

    editBtn.setBounds  (hdr.removeFromLeft (46)); hdr.removeFromLeft (6);
    closeBtn.setBounds (hdr.removeFromRight (24)); hdr.removeFromRight (4);
    soloBtn.setBounds  (hdr.removeFromRight (24)); hdr.removeFromRight (3);
    muteBtn.setBounds  (hdr.removeFromRight (24)); hdr.removeFromRight (6);
    panSlider.setBounds(hdr.removeFromRight (55)); hdr.removeFromRight (4);
    volSlider.setBounds(hdr.removeFromRight (70)); hdr.removeFromRight (6);
    outBusBox.setBounds(hdr.removeFromRight (72)); hdr.removeFromRight (4);
    midiChBox.setBounds(hdr.removeFromRight (68)); hdr.removeFromRight (6);
    titleLbl.setBounds (hdr);

    panelBody.setBounds (r.reduced (4));
}

// ==================== PanelDesignCanvas ====================
PanelDesignCanvas::PanelDesignCanvas (NoiseMu6ixSamplerAudioProcessor& p) : processor (p) {}

void PanelDesignCanvas::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff191c22));
    g.setColour (juce::Colour (0xff252933));
    for (int x = 20; x < getWidth(); x += 20)  g.drawVerticalLine (x, 0.0f, (float)getHeight());
    for (int y = 20; y < getHeight(); y += 20) g.drawHorizontalLine (y, 0.0f, (float)getWidth());
    g.setColour (juce::Colour (0xff4a4f5a));
    g.drawRect (getLocalBounds(), 1);

    const auto& slot = processor.getSlot (activeSlotIdx);
    for (int i = 0; i < InstrumentSlot::numMacros; ++i)
    {
        const auto& cfg = slot.macroConfigs[i];
        if (!cfg.visible) continue;

        juce::Rectangle<int> r (cfg.posX, cfg.posY, cfg.width, cfg.height);
        bool isSel = (i == selectedMacroIdx);

        g.setColour (juce::Colour (0xff262a34));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (isSel ? juce::Colour (0xff00e5ff) : juce::Colour (0xff565c6b));
        g.drawRoundedRectangle (r.toFloat(), 6.0f, isSel ? 2.0f : 1.0f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (11.5f, juce::Font::bold));
        g.drawText (cfg.name, r.reduced (4, 3).removeFromTop (16), juce::Justification::centred, true);

        int pMin = (int)std::round (cfg.minVal * 100.0f);
        int pMax = (int)std::round (cfg.maxVal * 100.0f);
        g.setColour (juce::Colour (0xffd9a05b));
        g.setFont (9.5f);
        g.drawText (juce::String (pMin) + "% - " + juce::String (pMax) + "%",
                    r.reduced (4, 3).removeFromBottom (14), juce::Justification::centred, true);

        auto body = r.reduced (12, 20);
        if (cfg.isSlider)
        {
            float cx = (float)body.getCentreX();
            g.setColour (juce::Colour (0xff3c414d));
            g.drawLine (cx, (float)body.getY() + 4, cx, (float)body.getBottom() - 4, 5.0f);
            g.setColour (juce::Colour (0xffd9a05b));
            g.fillEllipse (cx - 6.0f, (float)body.getCentreY() - 6.0f, 12.0f, 12.0f);
        }
        else
        {
            float rad = (float)std::min (body.getWidth(), body.getHeight()) * 0.42f;
            float cx = (float)body.getCentreX(), cy = (float)body.getCentreY();
            g.setColour (juce::Colour (0xff353a46));
            g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
            g.setColour (juce::Colour (0xffd9a05b));
            g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 2.5f);
            g.drawLine (cx, cy, cx + rad * 0.65f, cy - rad * 0.55f, 2.5f);
        }

        if (isSel)
        {
            g.setColour (juce::Colour (0xff00e5ff));
            g.fillRect (r.getRight() - 10, r.getBottom() - 10, 9, 9);
        }
    }
}

void PanelDesignCanvas::mouseMove (const juce::MouseEvent& e)
{
    const auto& slot = processor.getSlot (activeSlotIdx);
    for (int i = InstrumentSlot::numMacros - 1; i >= 0; --i)
    {
        const auto& cfg = slot.macroConfigs[i];
        if (!cfg.visible) continue;
        juce::Rectangle<int> r (cfg.posX, cfg.posY, cfg.width, cfg.height);
        if (r.contains (e.getPosition()))
        {
            juce::Rectangle<int> handle (r.getRight() - 14, r.getBottom() - 14, 14, 14);
            setMouseCursor (handle.contains (e.getPosition())
                            ? juce::MouseCursor::BottomRightCornerResizeCursor
                            : juce::MouseCursor::DraggingHandCursor);
            return;
        }
    }
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void PanelDesignCanvas::mouseDown (const juce::MouseEvent& e)
{
    dragOp = DragOp::None;
    auto& slot = processor.getSlot (activeSlotIdx);
    for (int i = InstrumentSlot::numMacros - 1; i >= 0; --i)
    {
        auto& cfg = slot.macroConfigs[i];
        if (!cfg.visible) continue;
        juce::Rectangle<int> r (cfg.posX, cfg.posY, cfg.width, cfg.height);
        if (r.contains (e.getPosition()))
        {
            selectedMacroIdx = i;
            dragStartMouse = e.getPosition();
            origX = cfg.posX; origY = cfg.posY; origW = cfg.width; origH = cfg.height;
            juce::Rectangle<int> handle (r.getRight() - 14, r.getBottom() - 14, 14, 14);
            dragOp = handle.contains (e.getPosition()) ? DragOp::Resize : DragOp::Move;
            repaint();
            if (onSelectionChanged) onSelectionChanged (i);
            return;
        }
    }
}

void PanelDesignCanvas::mouseDrag (const juce::MouseEvent& e)
{
    if (dragOp == DragOp::None || selectedMacroIdx < 0 || selectedMacroIdx >= InstrumentSlot::numMacros) return;
    auto& cfg = processor.getSlot (activeSlotIdx).macroConfigs[selectedMacroIdx];
    auto delta = e.getPosition() - dragStartMouse;
    if (dragOp == DragOp::Move)
    {
        cfg.posX = juce::jlimit (4, std::max (4, getWidth() - cfg.width - 4), origX + delta.x);
        cfg.posY = juce::jlimit (4, std::max (4, getHeight() - cfg.height - 4), origY + delta.y);
    }
    else if (dragOp == DragOp::Resize)
    {
        cfg.width  = juce::jlimit (60, std::max (60, getWidth() - cfg.posX - 4),  origW + delta.x);
        cfg.height = juce::jlimit (70, std::max (70, getHeight() - cfg.posY - 4), origH + delta.y);
    }
    repaint();
    if (onLayoutChanged) onLayoutChanged();
}

// ==================== FileBrowserListModel ====================
void FileBrowserListModel::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= files.size()) return;
    g.fillAll (selected ? juce::Colour (0xffd9a05b).withAlpha (0.35f) : juce::Colour (0xff24262b));
    g.setColour (selected ? juce::Colours::white : juce::Colour (0xffcccccc));
    g.setFont (11.5f);
    g.drawText (files[row].getFileName(), 6, 0, w - 12, h, juce::Justification::centredLeft, true);
}

void FileBrowserListModel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    if (row >= 0 && row < files.size() && onFileDoubleClicked) onFileDoubleClicked (files[row]);
}

// ==================== EditRackContent ====================
void EditRackContent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff25282f));
    int w = getWidth();
    auto drawHeader = [&](int y, const juce::String& title) {
        if (y < 0) return;
        g.setColour (juce::Colour (0xff181b21));
        g.fillRoundedRectangle (4.0f, (float)y, (float)(w - 8), 22.0f, 4.0f);
        g.setColour (juce::Colour (0xffd9a05b));
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText (title, 10, y, w - 20, 22, juce::Justification::centredLeft);
    };
    drawHeader (groupHeaderY, "GROUP EDITOR  (Unlimited Round Robin & Keyswitches)");
    drawHeader (mapHeaderY,   "MAPPING EDITOR  (Shows Selected Group | Blue Keys = Mapped)");
    drawHeader (waveHeaderY,  "WAVE EDITOR  (Zero-Cross Auto-Loop & Sample Start/End)");
    drawHeader (fxHeaderY,    "FX & ADSR RACK  (EQ, Filter, Drive, Chorus, Delay, Reverb, ADSR)");
    drawHeader (panelHeaderY, "PANEL EDITOR  (Compact Slot Panel: 740x210 | Drag, Resize & Set % Range)");
}

// ==================== Main Editor ====================
NoiseMu6ixSamplerAudioProcessorEditor::NoiseMu6ixSamplerAudioProcessorEditor (NoiseMu6ixSamplerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), groupCards (p), mappingGrid (p),
      panelDesignCanvas (p), keyboard (p, p.keyboardState)
{
    setResizable (false, false);
    addAndMakeVisible (rootCanvas);

    // Header
    logoBtn.onClick = [this] { showScaleMenu(); };
    rootCanvas.addAndMakeVisible (logoBtn);

    modeToggleBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffb86b28));
    modeToggleBtn.onClick = [this] {
        inEditMode = !inEditMode;
        modeToggleBtn.setButtonText (inEditMode ? "Multi Rack" : "Edit Mode");
        updateModeVisibility();
    };
    rootCanvas.addAndMakeVisible (modeToggleBtn);

    saveInstBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b5c3f));
    saveInstBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Save Slot as .nm6i Instrument", juce::File{}, "*.nm6i");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f != juce::File{})
                {
                    if (!f.hasFileExtension (".nm6i")) f = f.withFileExtension (".nm6i");
                    if (processor.saveSlotMonolith (activeSlotIdx, f))
                    {
                        rebuildMultiRackCards();
                        // Refresh any library entry pointing to that folder
                        for (auto& lib : libraryBrowser.libraries)
                            if (lib.folder == f.getParentDirectory()) scanLibraryEntry (lib);
                        libraryBrowser.refreshHeight();
                    }
                }
            });
    };
    rootCanvas.addAndMakeVisible (saveInstBtn);

    loadInstBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b475c));
    loadInstBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Load .nm6i into Current Slot", juce::File{}, "*.nm6i");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                if (processor.loadSlotMonolith (activeSlotIdx, fc.getResult()))
                {
                    setActiveSlot (activeSlotIdx);
                    rebuildMultiRackCards();
                }
            });
    };
    rootCanvas.addAndMakeVisible (loadInstBtn);

    for (int i = 0; i < NoiseMu6ixSamplerAudioProcessor::maxSlots; ++i)
        activeSlotBox.addItem ("Slot " + juce::String (i + 1), i + 1);
    activeSlotBox.setSelectedId (1, juce::dontSendNotification);
    activeSlotBox.onChange = [this] {
        int sIdx = activeSlotBox.getSelectedId() - 1;
        if (!processor.getSlot (sIdx).loaded)
        {
            const juce::ScopedLock sl (processor.dataLock);
            processor.getSlot (sIdx).initDefault (sIdx);
        }
        setActiveSlot (sIdx);
        rebuildMultiRackCards();
    };
    rootCanvas.addAndMakeVisible (activeSlotBox);

    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8d93a1));
    versionLabel.setFont (juce::Font (11.5f, juce::Font::bold));
    rootCanvas.addAndMakeVisible (versionLabel);

    voicesLabel.setJustificationType (juce::Justification::centredRight);
    voicesLabel.setColour (juce::Label::textColourId, juce::Colour (0xffd9a05b));
    voicesLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    rootCanvas.addAndMakeVisible (voicesLabel);

    // Left Sidebar Tabs
    sideLibTabBtn.onClick   = [this] { sidebarShowLibraries = true;  updateSidebarVisibility(); };
    sideFilesTabBtn.onClick = [this] { sidebarShowLibraries = false; updateSidebarVisibility(); };
    rootCanvas.addAndMakeVisible (sideLibTabBtn);
    rootCanvas.addAndMakeVisible (sideFilesTabBtn);

    // Libraries Banner Browser
    addLibraryBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2f6642));
    addLibraryBtn.onClick = [this] { addLibraryFolderDialog(); };
    rootCanvas.addAndMakeVisible (addLibraryBtn);

    setPhotoBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b475c));
    setPhotoBtn.onClick = [this] { setSelectedLibraryPhotoDialog(); };
    rootCanvas.addAndMakeVisible (setPhotoBtn);

    removeLibraryBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff5c2b2b));
    removeLibraryBtn.onClick = [this] {
        int idx = libraryBrowser.selectedLibraryIdx;
        if (idx >= 0 && idx < (int)libraryBrowser.libraries.size())
        {
            libraryBrowser.libraries.erase (libraryBrowser.libraries.begin() + idx);
            libraryBrowser.selectedLibraryIdx = -1;
            saveLibrariesList();
            libraryBrowser.refreshHeight();
        }
    };
    rootCanvas.addAndMakeVisible (removeLibraryBtn);

    libraryBrowser.onInstrumentClicked = [this](const juce::File& instFile) {
        int targetSlot = processor.findFirstEmptySlot();
        if (targetSlot < 0) targetSlot = activeSlotIdx;
        if (processor.loadSlotMonolith (targetSlot, instFile))
        {
            setActiveSlot (targetSlot);
            rebuildMultiRackCards();
        }
    };
    libraryViewport.setViewedComponent (&libraryBrowser, false);
    libraryViewport.setScrollBarsShown (true, false);
    rootCanvas.addAndMakeVisible (libraryViewport);

    // Files Browser
    browseFolderBtn.onClick = [this] { browseLocalFolder(); };
    rootCanvas.addAndMakeVisible (browseFolderBtn);

    addSelectedFileBtn.onClick = [this] {
        int row = fileListBox.getSelectedRow();
        if (row >= 0 && row < fileListModel.files.size())
        {
            auto f = fileListModel.files[row];
            if (f.hasFileExtension (".nm6i"))
            {
                int targetSlot = processor.findFirstEmptySlot();
                if (targetSlot < 0) targetSlot = activeSlotIdx;
                processor.loadSlotMonolith (targetSlot, f);
                setActiveSlot (targetSlot);
                rebuildMultiRackCards();
            }
            else
            {
                processor.loadSampleFile (activeSlotIdx, f, selectedGroupIdx, true, -1);
                const juce::ScopedLock sl (processor.dataLock);
                mappingGrid.selectedZoneIdx = (int)processor.getSlot (activeSlotIdx).zones.size() - 1;
                refreshGroupUI(); refreshSelectedZoneUI();
                mappingGrid.repaint(); keyboard.repaint();
            }
        }
    };
    rootCanvas.addAndMakeVisible (addSelectedFileBtn);
    folderPathLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    folderPathLabel.setFont (11.0f);
    rootCanvas.addAndMakeVisible (folderPathLabel);

    fileListModel.onFileDoubleClicked = [this](const juce::File& f) {
        if (f.hasFileExtension (".nm6i"))
        {
            int targetSlot = processor.findFirstEmptySlot();
            if (targetSlot < 0) targetSlot = activeSlotIdx;
            processor.loadSlotMonolith (targetSlot, f);
            setActiveSlot (targetSlot);
            rebuildMultiRackCards();
        }
        else
        {
            processor.loadSampleFile (activeSlotIdx, f, selectedGroupIdx, true, -1);
            const juce::ScopedLock sl (processor.dataLock);
            mappingGrid.selectedZoneIdx = (int)processor.getSlot (activeSlotIdx).zones.size() - 1;
            refreshGroupUI(); refreshSelectedZoneUI();
            mappingGrid.repaint(); keyboard.repaint();
        }
    };
    fileListBox.setRowHeight (22);
    fileListBox.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1a1c21));
    rootCanvas.addAndMakeVisible (fileListBox);

    // 5 Module ON/OFF Toggles
    togGroupBtn.onClick = [this] { showGroupSec = !showGroupSec; updateModeVisibility(); };
    togMapBtn.onClick   = [this] { showMapSec   = !showMapSec;   updateModeVisibility(); };
    togWaveBtn.onClick  = [this] { showWaveSec  = !showWaveSec;  updateModeVisibility(); };
    togFXBtn.onClick    = [this] { showFXSec    = !showFXSec;    updateModeVisibility(); };
    togPanelBtn.onClick = [this] { showPanelSec = !showPanelSec; updateModeVisibility(); };
    rootCanvas.addAndMakeVisible (togGroupBtn);
    rootCanvas.addAndMakeVisible (togMapBtn);
    rootCanvas.addAndMakeVisible (togWaveBtn);
    rootCanvas.addAndMakeVisible (togFXBtn);
    rootCanvas.addAndMakeVisible (togPanelBtn);

    editViewport.setViewedComponent (&editRack, false);
    editViewport.setScrollBarsShown (true, false);
    rootCanvas.addAndMakeVisible (editViewport);

    // --- 1. Group Editor ---
    addGroupBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2f6642));
    addGroupBtn.onClick = [this] {
        selectedGroupIdx = processor.addGroup (activeSlotIdx);
        groupCards.selectedGroupIdx = selectedGroupIdx;
        mappingGrid.targetDropGroup = selectedGroupIdx;
        mappingGrid.activeGroupFilter = showAllGroupsBtn.getToggleState() ? -1 : selectedGroupIdx;
        keyboard.activeFilterGroupIdx = mappingGrid.activeGroupFilter;
        mappingGrid.selectedZoneIdx = -1;
        refreshGroupUI(); refreshSelectedZoneUI(); mappingGrid.repaint(); keyboard.repaint();
    };
    editRack.addAndMakeVisible (addGroupBtn);

    deleteGroupBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff662f2f));
    deleteGroupBtn.onClick = [this] {
        processor.removeGroup (activeSlotIdx, selectedGroupIdx);
        selectedGroupIdx = std::max (0, selectedGroupIdx - 1);
        groupCards.selectedGroupIdx = selectedGroupIdx;
        mappingGrid.targetDropGroup = selectedGroupIdx;
        mappingGrid.activeGroupFilter = showAllGroupsBtn.getToggleState() ? -1 : selectedGroupIdx;
        keyboard.activeFilterGroupIdx = mappingGrid.activeGroupFilter;
        mappingGrid.selectedZoneIdx = -1;
        refreshGroupUI(); refreshSelectedZoneUI(); mappingGrid.repaint(); keyboard.repaint();
    };
    editRack.addAndMakeVisible (deleteGroupBtn);

    rrToggleBtn.onClick = [this] {
        auto& s = processor.getSlot (activeSlotIdx);
        s.rrEnabled.store (!s.rrEnabled.load());
        refreshGroupUI();
    };
    editRack.addAndMakeVisible (rrToggleBtn);

    rrModeBox.addItem ("RR: Cycle", 1); rrModeBox.addItem ("RR: No-Repeat", 2);
    rrModeBox.onChange = [this] { processor.getSlot (activeSlotIdx).rrMode.store (rrModeBox.getSelectedId() - 1); };
    editRack.addAndMakeVisible (rrModeBox);

    ksMasterBtn.onClick = [this] {
        auto& s = processor.getSlot (activeSlotIdx);
        s.keyswitchMasterOn.store (!s.keyswitchMasterOn.load());
        refreshGroupUI(); keyboard.repaint();
    };
    editRack.addAndMakeVisible (ksMasterBtn);

    showAllGroupsBtn.setToggleState (false, juce::dontSendNotification);
    showAllGroupsBtn.onClick = [this] {
        mappingGrid.activeGroupFilter = showAllGroupsBtn.getToggleState() ? -1 : selectedGroupIdx;
        keyboard.activeFilterGroupIdx = mappingGrid.activeGroupFilter;
        mappingGrid.repaint(); keyboard.repaint();
    };
    editRack.addAndMakeVisible (showAllGroupsBtn);

    groupCards.onGroupSelected = [this](int idx) {
        selectedGroupIdx = idx;
        mappingGrid.targetDropGroup = idx;
        mappingGrid.activeGroupFilter = showAllGroupsBtn.getToggleState() ? -1 : idx;
        keyboard.activeFilterGroupIdx = mappingGrid.activeGroupFilter;
        mappingGrid.selectedZoneIdx = -1;
        refreshGroupUI(); refreshSelectedZoneUI(); mappingGrid.repaint(); keyboard.repaint();
    };
    editRack.addAndMakeVisible (groupCards);

    grpNameLabel.setEditable (true);
    grpNameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1b1d22));
    grpNameLabel.setColour (juce::Label::outlineColourId, juce::Colour (0xff4a4e57));
    grpNameLabel.onTextChange = [this] {
        const juce::ScopedLock sl (processor.dataLock);
        auto& s = processor.getSlot (activeSlotIdx);
        if (selectedGroupIdx >= 0 && selectedGroupIdx < (int)s.groups.size())
            s.groups[(size_t)selectedGroupIdx].name = grpNameLabel.getText();
        groupCards.repaint();
    };
    editRack.addAndMakeVisible (grpNameLabel);

    groupSoloBtn.onClick = [this] { applyGroupInspector(); };
    groupMuteBtn.onClick = [this] { applyGroupInspector(); };
    editRack.addAndMakeVisible (groupSoloBtn); editRack.addAndMakeVisible (groupMuteBtn);

    auto setupRackHSlider = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV, std::function<void()> cb) {
        s.setRange (minV, maxV, step); s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 18);
        s.onValueChange = std::move (cb);
        editRack.addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centredRight); l.setFont (11.0f);
        editRack.addAndMakeVisible (l);
    };
    setupRackHSlider (grpVolSlider, grpVolLbl, -24, 12, 0.1, 0.0, [this]{ applyGroupInspector(); });
    setupRackHSlider (grpPanSlider, grpPanLbl, -1, 1, 0.01, 0.0, [this]{ applyGroupInspector(); });
    setupRackHSlider (grpTuneSlider, grpTuneLbl, -24, 24, 0.1, 0.0, [this]{ applyGroupInspector(); });

    grpKSEnabledBtn.onClick = [this] { applyGroupInspector(); keyboard.repaint(); };
    editRack.addAndMakeVisible (grpKSEnabledBtn);
    grpKSNoteSlider.setRange (24, 96, 1); grpKSNoteSlider.setValue (24, juce::dontSendNotification);
    grpKSNoteSlider.setSliderStyle (juce::Slider::IncDecButtons);
    grpKSNoteSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 36, 20);
    grpKSNoteSlider.onValueChange = [this] { applyGroupInspector(); keyboard.repaint(); };
    editRack.addAndMakeVisible (grpKSNoteSlider);
    grpKSNoteLbl.setFont (11.0f); grpKSNoteLbl.setJustificationType (juce::Justification::centredRight);
    editRack.addAndMakeVisible (grpKSNoteLbl);

    grpKSColorBox.addItem ("Red", 1); grpKSColorBox.addItem ("Green", 2); grpKSColorBox.addItem ("Yellow", 3);
    grpKSColorBox.addItem ("Purple", 4); grpKSColorBox.addItem ("Orange", 5); grpKSColorBox.addItem ("Cyan", 6);
    grpKSColorBox.setSelectedId (1, juce::dontSendNotification);
    grpKSColorBox.onChange = [this] { applyGroupInspector(); keyboard.repaint(); };
    editRack.addAndMakeVisible (grpKSColorBox);

    // --- 2. Mapping Editor ---
    mappingGrid.onZoneChanged = [this] { refreshSelectedZoneUI(); groupCards.repaint(); keyboard.repaint(); };
    editRack.addAndMakeVisible (mappingGrid);
    autoSpreadBtn.onClick = [this] {
        processor.autoSpreadKeyRanges (activeSlotIdx, showAllGroupsBtn.getToggleState() ? -1 : selectedGroupIdx);
        mappingGrid.repaint(); refreshSelectedZoneUI(); keyboard.repaint();
    };
    delZoneBtn.onClick = [this] {
        if (mappingGrid.selectedZoneIdx >= 0)
        {
            processor.removeZone (activeSlotIdx, mappingGrid.selectedZoneIdx);
            mappingGrid.selectedZoneIdx = -1;
            mappingGrid.repaint(); refreshSelectedZoneUI(); groupCards.repaint(); keyboard.repaint();
        }
    };
    editRack.addAndMakeVisible (autoSpreadBtn); editRack.addAndMakeVisible (delZoneBtn); editRack.addAndMakeVisible (selZoneNameLbl);

    auto setupZoneNum = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV) {
        s.setRange (minV, maxV, step); s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 38, 19);
        s.onValueChange = [this] { applyZoneBarToSelected(); };
        editRack.addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centred); l.setFont (10.5f);
        editRack.addAndMakeVisible (l);
    };
    setupZoneNum (zRootSlider, zRootLbl, 0, 127, 1, 60); setupZoneNum (zLowKSlider, zLowKLbl, 0, 127, 1, 48);
    setupZoneNum (zHighKSlider, zHighKLbl, 0, 127, 1, 72); setupZoneNum (zLowVSlider, zLowVLbl, 0, 127, 1, 0);
    setupZoneNum (zHighVSlider, zHighVLbl, 0, 127, 1, 127); setupZoneNum (zVolSlider, zVolLbl, -24, 12, 0.5, 0);
    setupZoneNum (zPanSlider, zPanLbl, -1, 1, 0.05, 0); setupZoneNum (zTuneSlider, zTuneLbl, -24, 24, 0.5, 0);

    // --- 3. Wave Editor ---
    editRack.addAndMakeVisible (waveEditor);
    editRack.addAndMakeVisible (loopToggle);
    loopToggle.onClick = [this] {
        const juce::ScopedLock sl (processor.dataLock);
        auto& s = processor.getSlot (activeSlotIdx);
        int idx = mappingGrid.selectedZoneIdx;
        if (idx >= 0 && idx < (int)s.zones.size()) { s.zones[(size_t)idx]->isLooping.store (loopToggle.getToggleState()); waveEditor.repaint(); }
    };
    autoLoopBtn.onClick = [this] {
        const juce::ScopedLock sl (processor.dataLock);
        auto& s = processor.getSlot (activeSlotIdx);
        int idx = mappingGrid.selectedZoneIdx;
        if (idx >= 0 && idx < (int)s.zones.size())
        {
            auto z = s.zones[(size_t)idx];
            z->isLooping.store (true);
            NoiseMu6ixSamplerAudioProcessor::autoDetectLoop (z);
            loopToggle.setToggleState (true, juce::dontSendNotification);
            crossfadeSlider.setValue (z->crossfadeSamples.load(), juce::dontSendNotification);
            waveEditor.repaint();
        }
    };
    editRack.addAndMakeVisible (autoLoopBtn);
    waveDragModeBox.addItem ("Drag: Loop Start/End", 1); waveDragModeBox.addItem ("Drag: Sample Start/End", 2);
    waveDragModeBox.setSelectedId (1, juce::dontSendNotification);
    waveDragModeBox.onChange = [this] { waveEditor.setEditMode (waveDragModeBox.getSelectedId() == 2); };
    editRack.addAndMakeVisible (waveDragModeBox);

    setupRackHSlider (crossfadeSlider, cfLbl, 64, 15000, 1, 2048, [this] {
        const juce::ScopedLock sl (processor.dataLock);
        auto& s = processor.getSlot (activeSlotIdx);
        int idx = mappingGrid.selectedZoneIdx;
        if (idx >= 0 && idx < (int)s.zones.size()) s.zones[(size_t)idx]->crossfadeSamples.store ((int)crossfadeSlider.getValue());
    });
    setupRackHSlider (zoomSlider, zoomLbl, 1.0, 16.0, 0.1, 1.0, [this] { waveEditor.setZoom ((float)zoomSlider.getValue()); });

    // --- 4. FX & ADSR ---
    auto setupFXToggle = [this](juce::ToggleButton& b) { b.onClick = [this] { applyFXToProcessor(); }; editRack.addAndMakeVisible (b); };
    setupFXToggle (fxEqOn); setupFXToggle (fxFltOn); setupFXToggle (fxDrvOn);
    setupFXToggle (fxChoOn); setupFXToggle (fxDlyOn); setupFXToggle (fxRevOn);
    fxFltTypeBox.addItem ("Lowpass", 1); fxFltTypeBox.addItem ("Highpass", 2); fxFltTypeBox.addItem ("Bandpass", 3);
    fxFltTypeBox.onChange = [this] { applyFXToProcessor(); };
    editRack.addAndMakeVisible (fxFltTypeBox);

    auto setupFXKnob = [this](juce::Slider& s, juce::Label& l, double minV, double maxV, double step, double defV) {
        s.setRange (minV, maxV, step); s.setValue (defV, juce::dontSendNotification);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 42, 14);
        s.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffd9a05b));
        s.setColour (juce::Slider::thumbColourId, juce::Colour (0xffd9a05b));
        s.onValueChange = [this] { applyFXToProcessor(); };
        editRack.addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centred); l.setFont (10.5f);
        editRack.addAndMakeVisible (l);
    };
    setupFXKnob (fxEqLow, lEqLow, -12, 12, 0.1, 0.0); setupFXKnob (fxEqMid, lEqMid, -12, 12, 0.1, 0.0); setupFXKnob (fxEqHigh, lEqHigh, -12, 12, 0.1, 0.0);
    setupFXKnob (fxFltCut, lFltCut, 0.0, 1.0, 0.01, 1.0); setupFXKnob (fxFltRes, lFltRes, 0.0, 1.0, 0.01, 0.1); setupFXKnob (fxDrvAmt, lDrvAmt, 0.0, 1.0, 0.01, 0.0);
    setupFXKnob (fxChoRate, lChoRate, 0.1, 6.0, 0.05, 1.2); setupFXKnob (fxChoDepth, lChoDepth, 0.0, 1.0, 0.01, 0.25); setupFXKnob (fxChoMix, lChoMix, 0.0, 1.0, 0.01, 0.35);
    setupFXKnob (fxDlyTime, lDlyTime, 30, 1200, 1, 320); setupFXKnob (fxDlyFb, lDlyFb, 0.0, 0.85, 0.01, 0.35); setupFXKnob (fxDlyMix, lDlyMix, 0.0, 1.0, 0.01, 0.25);
    setupFXKnob (fxRevSize, lRevSize, 0.0, 1.0, 0.01, 0.65); setupFXKnob (fxRevDamp, lRevDamp, 0.0, 1.0, 0.01, 0.45); setupFXKnob (fxRevMix, lRevMix, 0.0, 1.0, 0.01, 0.15);
    setupFXKnob (fxAtk, lAtk, 0.0, 1.0, 0.01, 0.02); setupFXKnob (fxDec, lDec, 0.0, 1.0, 0.01, 0.25); setupFXKnob (fxSus, lSus, 0.0, 1.0, 0.01, 1.0); setupFXKnob (fxRel, lRel, 0.0, 1.0, 0.01, 0.25);

    // --- 5. Panel Editor ---
    peAddKnobBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2f6642));
    peAddKnobBtn.onClick = [this] {
        auto& s = processor.getSlot (activeSlotIdx);
        for (int i = 0; i < InstrumentSlot::numMacros; ++i)
            if (!s.macroConfigs[i].visible)
            {
                s.macroConfigs[i].visible = true; s.macroConfigs[i].isSlider = false;
                s.macroConfigs[i].width = 110; s.macroConfigs[i].height = 135;
                panelDesignCanvas.selectedMacroIdx = i;
                refreshPanelEditorInspector(); rebuildMultiRackCards(); panelDesignCanvas.repaint();
                break;
            }
    };
    editRack.addAndMakeVisible (peAddKnobBtn);

    peAddSliderBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b4f66));
    peAddSliderBtn.onClick = [this] {
        auto& s = processor.getSlot (activeSlotIdx);
        for (int i = 0; i < InstrumentSlot::numMacros; ++i)
            if (!s.macroConfigs[i].visible)
            {
                s.macroConfigs[i].visible = true; s.macroConfigs[i].isSlider = true;
                s.macroConfigs[i].width = 82; s.macroConfigs[i].height = 180;
                panelDesignCanvas.selectedMacroIdx = i;
                refreshPanelEditorInspector(); rebuildMultiRackCards(); panelDesignCanvas.repaint();
                break;
            }
    };
    editRack.addAndMakeVisible (peAddSliderBtn);

    peDelCtrlBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff662f2f));
    peDelCtrlBtn.onClick = [this] {
        auto& s = processor.getSlot (activeSlotIdx);
        int idx = panelDesignCanvas.selectedMacroIdx;
        if (idx >= 0 && idx < InstrumentSlot::numMacros)
        {
            s.macroConfigs[idx].visible = false;
            panelDesignCanvas.selectedMacroIdx = -1;
            for (int i = 0; i < InstrumentSlot::numMacros; ++i)
                if (s.macroConfigs[i].visible) { panelDesignCanvas.selectedMacroIdx = i; break; }
            refreshPanelEditorInspector(); rebuildMultiRackCards(); panelDesignCanvas.repaint();
        }
    };
    editRack.addAndMakeVisible (peDelCtrlBtn);

    peNameLbl.setFont (11.0f); editRack.addAndMakeVisible (peNameLbl);
    peNameEdit.onTextChange = [this] { applyPanelEditorInspector(); };
    editRack.addAndMakeVisible (peNameEdit);

    peStyleBtn.onClick = [this] {
        auto& s = processor.getSlot (activeSlotIdx);
        int idx = panelDesignCanvas.selectedMacroIdx;
        if (idx >= 0 && idx < InstrumentSlot::numMacros)
        {
            s.macroConfigs[idx].isSlider = !s.macroConfigs[idx].isSlider;
            refreshPanelEditorInspector(); rebuildMultiRackCards(); panelDesignCanvas.repaint();
        }
    };
    editRack.addAndMakeVisible (peStyleBtn);

    peTargetBox.addItem ("None", (int)MacroTarget::None); peTargetBox.addItem ("Dynamics", (int)MacroTarget::Dynamics);
    peTargetBox.addItem ("Expression", (int)MacroTarget::Expression); peTargetBox.addItem ("Cutoff", (int)MacroTarget::FilterCutoff);
    peTargetBox.addItem ("Resonance", (int)MacroTarget::FilterRes); peTargetBox.addItem ("Reverb Mix", (int)MacroTarget::ReverbMix);
    peTargetBox.addItem ("Reverb Size", (int)MacroTarget::ReverbSize); peTargetBox.addItem ("Delay Mix", (int)MacroTarget::DelayMix);
    peTargetBox.addItem ("Chorus Mix", (int)MacroTarget::ChorusMix); peTargetBox.addItem ("Drive", (int)MacroTarget::Drive);
    peTargetBox.addItem ("Attack", (int)MacroTarget::Attack); peTargetBox.addItem ("Decay", (int)MacroTarget::Decay);
    peTargetBox.addItem ("Sustain", (int)MacroTarget::Sustain); peTargetBox.addItem ("Release", (int)MacroTarget::Release);
    peTargetBox.addItem ("Pitch", (int)MacroTarget::Pitch); peTargetBox.addItem ("Pan", (int)MacroTarget::Pan);
    peTargetBox.onChange = [this] { applyPanelEditorInspector(); };
    editRack.addAndMakeVisible (peTargetBox);

    peRangeLbl.setFont (11.0f); editRack.addAndMakeVisible (peRangeLbl);
    peToLbl.setFont (11.0f); peToLbl.setJustificationType (juce::Justification::centred);
    editRack.addAndMakeVisible (peToLbl);
    peMinEdit.setInputRestrictions (3, "0123456789"); peMaxEdit.setInputRestrictions (3, "0123456789");
    peMinEdit.onTextChange = [this] { applyPanelEditorInspector(); };
    peMaxEdit.onTextChange = [this] { applyPanelEditorInspector(); };
    editRack.addAndMakeVisible (peMinEdit); editRack.addAndMakeVisible (peMaxEdit);

    panelDesignCanvas.onSelectionChanged = [this](int) { refreshPanelEditorInspector(); };
    panelDesignCanvas.onLayoutChanged    = [this] { rebuildMultiRackCards(); };
    editRack.addAndMakeVisible (panelDesignCanvas);

    // --- 16-Slot Front Panel Multi-Rack ---
    addNewSlotBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2f6642));
    addNewSlotBtn.onClick = [this] {
        int newIdx = processor.activateNewEmptySlot();
        if (newIdx >= 0)
        {
            setActiveSlot (newIdx);
            rebuildMultiRackCards();
        }
    };
    rootCanvas.addAndMakeVisible (addNewSlotBtn);

    multiRackViewport.setViewedComponent (&multiRackContent, false);
    multiRackViewport.setScrollBarsShown (true, false);
    rootCanvas.addAndMakeVisible (multiRackViewport);

    rootCanvas.addAndMakeVisible (keyboard);

    loadSavedLibrariesList();
    updateSidebarVisibility();
    setActiveSlot (0);
    rebuildMultiRackCards();
    updateModeVisibility();
    setUIScale (1.0f);
    startTimerHz (10);
}

void NoiseMu6ixSamplerAudioProcessorEditor::setActiveSlot (int newSlotIdx)
{
    activeSlotIdx = juce::jlimit (0, NoiseMu6ixSamplerAudioProcessor::maxSlots - 1, newSlotIdx);
    activeSlotBox.setSelectedId (activeSlotIdx + 1, juce::dontSendNotification);
    groupCards.activeSlotIdx = activeSlotIdx;
    mappingGrid.activeSlotIdx = activeSlotIdx;
    panelDesignCanvas.activeSlotIdx = activeSlotIdx;
    keyboard.activeSlotIdx = activeSlotIdx;

    int ch = processor.getSlot (activeSlotIdx).midiChannel.load();
    keyboard.setMidiChannel (ch > 0 ? ch : 1);

    selectedGroupIdx = 0;
    groupCards.selectedGroupIdx = 0;
    mappingGrid.targetDropGroup = 0;
    mappingGrid.activeGroupFilter = showAllGroupsBtn.getToggleState() ? -1 : 0;
    keyboard.activeFilterGroupIdx = mappingGrid.activeGroupFilter;
    mappingGrid.selectedZoneIdx = -1;

    refreshGroupUI();
    refreshSelectedZoneUI();
    refreshFXUI();
    refreshPanelEditorInspector();
    mappingGrid.repaint();
    panelDesignCanvas.repaint();
    keyboard.repaint();
}

void NoiseMu6ixSamplerAudioProcessorEditor::rebuildMultiRackCards()
{
    multiRackContent.removeAllChildren();
    rackCards.clear();

    int w = std::max (700, multiRackViewport.getWidth() - multiRackViewport.getScrollBarThickness() - 4);
    int y = 6;
    const int cardH = 250; // 32px header + 210px compact Front Panel

    for (int i = 0; i < NoiseMu6ixSamplerAudioProcessor::maxSlots; ++i)
    {
        if (!processor.getSlot (i).loaded) continue;

        auto card = std::make_unique<MultiRackSlotCard> (processor, i);
        card->onEditClicked = [this](int sIdx) {
            setActiveSlot (sIdx);
            inEditMode = true;
            modeToggleBtn.setButtonText ("Multi Rack");
            updateModeVisibility();
        };
        card->onCloseClicked = [this](int sIdx) {
            processor.removeSlot (sIdx);
            rebuildMultiRackCards();
            keyboard.repaint();
        };
        card->setBounds (4, y, w - 8, cardH);
        multiRackContent.addAndMakeVisible (*card);
        rackCards.push_back (std::move (card));
        y += cardH + 8;
    }

    multiRackContent.setSize (w, std::max (y + 10, 300));
}

void NoiseMu6ixSamplerAudioProcessorEditor::showScaleMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Noise Mu6ix Studio - UI Size");
    menu.addItem (1, "25%",  true, std::abs (uiScale - 0.25f) < 0.01f);
    menu.addItem (2, "50%",  true, std::abs (uiScale - 0.50f) < 0.01f);
    menu.addItem (3, "75%",  true, std::abs (uiScale - 0.75f) < 0.01f);
    menu.addItem (4, "100% (Default)", true, std::abs (uiScale - 1.00f) < 0.01f);
    menu.addItem (5, "125%", true, std::abs (uiScale - 1.25f) < 0.01f);
    menu.addItem (6, "150%", true, std::abs (uiScale - 1.50f) < 0.01f);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&logoBtn),
        [this](int result) {
            if (result == 1)      setUIScale (0.25f);
            else if (result == 2) setUIScale (0.50f);
            else if (result == 3) setUIScale (0.75f);
            else if (result == 4) setUIScale (1.00f);
            else if (result == 5) setUIScale (1.25f);
            else if (result == 6) setUIScale (1.50f);
        });
}

void NoiseMu6ixSamplerAudioProcessorEditor::setUIScale (float newScale)
{
    uiScale = juce::jlimit (0.25f, 1.50f, newScale);
    rootCanvas.setBounds (0, 0, baseWidth, baseHeight);
    layoutBaseUI();
    rootCanvas.setTransform (juce::AffineTransform::scale (uiScale));
    setSize ((int)std::round ((float)baseWidth * uiScale),
             (int)std::round ((float)baseHeight * uiScale));
}

void NoiseMu6ixSamplerAudioProcessorEditor::timerCallback()
{
    voicesLabel.setText ("Voices: " + juce::String (processor.activeVoicesCount.load()), juce::dontSendNotification);
}

juce::File NoiseMu6ixSamplerAudioProcessorEditor::getLibraryConfigFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("NoiseMu6ixStudio");
    dir.createDirectory();
    return dir.getChildFile ("libraries_v2.xml");
}

void NoiseMu6ixSamplerAudioProcessorEditor::scanLibraryEntry (LibraryEntry& lib)
{
    lib.instruments.clear();
    if (lib.folder.isDirectory())
    {
        lib.instruments = lib.folder.findChildFiles (juce::File::findFiles, true, "*.nm6i");
        lib.instruments.sort();
        if (!lib.coverImageFile.existsAsFile())
        {
            for (const char* name : { "wallpaper.png", "wallpaper.jpg", "cover.png", "cover.jpg", "banner.png", "banner.jpg" })
            {
                auto cand = lib.folder.getChildFile (name);
                if (cand.existsAsFile()) { lib.coverImageFile = cand; break; }
            }
        }
    }
    if (lib.coverImageFile.existsAsFile())
        lib.cachedImage = juce::ImageFileFormat::loadFrom (lib.coverImageFile);
}

void NoiseMu6ixSamplerAudioProcessorEditor::loadSavedLibrariesList()
{
    libraryBrowser.libraries.clear();
    auto cfg = getLibraryConfigFile();
    if (cfg.existsAsFile())
    {
        if (auto xml = juce::parseXML (cfg))
        {
            for (auto* el : xml->getChildIterator())
            {
                juce::File folder (el->getStringAttribute ("folder"));
                if (folder.isDirectory())
                {
                    LibraryEntry lib;
                    lib.folder = folder;
                    lib.title  = el->getStringAttribute ("title", folder.getFileName());
                    lib.coverImageFile = juce::File (el->getStringAttribute ("photo"));
                    scanLibraryEntry (lib);
                    libraryBrowser.libraries.push_back (lib);
                }
            }
        }
    }
    libraryBrowser.refreshHeight();
}

void NoiseMu6ixSamplerAudioProcessorEditor::saveLibrariesList()
{
    juce::XmlElement root ("NoiseMu6ixLibrariesV2");
    for (const auto& lib : libraryBrowser.libraries)
    {
        auto* el = root.createNewChildElement ("Lib");
        el->setAttribute ("title", lib.title);
        el->setAttribute ("folder", lib.folder.getFullPathName());
        el->setAttribute ("photo", lib.coverImageFile.getFullPathName());
    }
    root.writeTo (getLibraryConfigFile());
}

void NoiseMu6ixSamplerAudioProcessorEditor::addLibraryFolderDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select Library Folder (containing .nm6i files)");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& fc) {
            auto dir = fc.getResult();
            if (dir.isDirectory())
            {
                LibraryEntry lib;
                lib.folder = dir;
                lib.title  = dir.getFileName();
                lib.expanded = true;
                scanLibraryEntry (lib);
                libraryBrowser.libraries.push_back (lib);
                libraryBrowser.selectedLibraryIdx = (int)libraryBrowser.libraries.size() - 1;
                saveLibrariesList();
                libraryBrowser.refreshHeight();
            }
        });
}

void NoiseMu6ixSamplerAudioProcessorEditor::setSelectedLibraryPhotoDialog()
{
    int idx = libraryBrowser.selectedLibraryIdx;
    if (idx < 0 || idx >= (int)libraryBrowser.libraries.size()) return;

    fileChooser = std::make_unique<juce::FileChooser> ("Select Cover Photo for Library", juce::File{}, "*.png;*.jpg;*.jpeg");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, idx](const juce::FileChooser& fc) {
            auto imgFile = fc.getResult();
            if (imgFile.existsAsFile() && idx < (int)libraryBrowser.libraries.size())
            {
                libraryBrowser.libraries[(size_t)idx].coverImageFile = imgFile;
                libraryBrowser.libraries[(size_t)idx].cachedImage = juce::ImageFileFormat::loadFrom (imgFile);
                saveLibrariesList();
                libraryBrowser.repaint();
            }
        });
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

void NoiseMu6ixSamplerAudioProcessorEditor::updateSidebarVisibility()
{
    sideLibTabBtn.setColour   (juce::TextButton::buttonColourId, sidebarShowLibraries  ? juce::Colour (0xffd9a05b) : juce::Colour (0xff323640));
    sideFilesTabBtn.setColour (juce::TextButton::buttonColourId, !sidebarShowLibraries ? juce::Colour (0xffd9a05b) : juce::Colour (0xff323640));

    addLibraryBtn.setVisible (sidebarShowLibraries);
    setPhotoBtn.setVisible (sidebarShowLibraries);
    removeLibraryBtn.setVisible (sidebarShowLibraries);
    libraryViewport.setVisible (sidebarShowLibraries);

    browseFolderBtn.setVisible (!sidebarShowLibraries);
    folderPathLabel.setVisible (!sidebarShowLibraries);
    fileListBox.setVisible (!sidebarShowLibraries);
    addSelectedFileBtn.setVisible (!sidebarShowLibraries);
}

void NoiseMu6ixSamplerAudioProcessorEditor::refreshGroupUI()
{
    const juce::ScopedLock sl (processor.dataLock);
    auto& s = processor.getSlot (activeSlotIdx);

    bool rrOn = s.rrEnabled.load();
    rrToggleBtn.setButtonText (rrOn ? "RR: ON (Unlimited)" : "RR: OFF (Layer)");
    rrToggleBtn.setColour (juce::TextButton::buttonColourId, rrOn ? juce::Colour (0xff2a7a4b) : juce::Colour (0xff4a4e57));
    rrModeBox.setEnabled (rrOn);
    rrModeBox.setSelectedId (s.rrMode.load() + 1, juce::dontSendNotification);

    bool ksOn = s.keyswitchMasterOn.load();
    ksMasterBtn.setButtonText (ksOn ? "Keyswitch: ON" : "Keyswitch: OFF");
    ksMasterBtn.setColour (juce::TextButton::buttonColourId, ksOn ? juce::Colour (0xff8a3232) : juce::Colour (0xff4a4e57));

    selectedGroupIdx = juce::jlimit (0, std::max (0, (int)s.groups.size() - 1), selectedGroupIdx);
    groupCards.selectedGroupIdx = selectedGroupIdx;
    groupCards.repaint();

    if (selectedGroupIdx < (int)s.groups.size())
    {
        const auto& g = s.groups[(size_t)selectedGroupIdx];
        grpNameLabel.setText (g.name, juce::dontSendNotification);
        groupSoloBtn.setToggleState (g.solo, juce::dontSendNotification);
        groupMuteBtn.setToggleState (g.mute, juce::dontSendNotification);
        grpVolSlider.setValue (g.volumeDb, juce::dontSendNotification);
        grpPanSlider.setValue (g.pan, juce::dontSendNotification);
        grpTuneSlider.setValue (g.tuneSemi, juce::dontSendNotification);
        grpKSEnabledBtn.setToggleState (g.keyswitchEnabled, juce::dontSendNotification);
        grpKSNoteSlider.setValue (g.keyswitchNote, juce::dontSendNotification);
        grpKSColorBox.setSelectedId (g.keyswitchColorIdx + 1, juce::dontSendNotification);
    }
}

void NoiseMu6ixSamplerAudioProcessorEditor::applyGroupInspector()
{
    const juce::ScopedLock sl (processor.dataLock);
    auto& s = processor.getSlot (activeSlotIdx);
    if (selectedGroupIdx < 0 || selectedGroupIdx >= (int)s.groups.size()) return;
    auto& g = s.groups[(size_t)selectedGroupIdx];
    g.solo              = groupSoloBtn.getToggleState();
    g.mute              = groupMuteBtn.getToggleState();
    g.volumeDb          = (float)grpVolSlider.getValue();
    g.pan               = (float)grpPanSlider.getValue();
    g.tuneSemi          = (float)grpTuneSlider.getValue();
    g.keyswitchEnabled  = grpKSEnabledBtn.getToggleState();
    g.keyswitchNote     = (int)grpKSNoteSlider.getValue();
    g.keyswitchColorIdx = std::max (0, grpKSColorBox.getSelectedId() - 1);
    groupCards.repaint();
}

void NoiseMu6ixSamplerAudioProcessorEditor::refreshSelectedZoneUI()
{
    const juce::ScopedLock sl (processor.dataLock);
    auto& s = processor.getSlot (activeSlotIdx);
    int idx = mappingGrid.selectedZoneIdx;
    if (idx < 0 || idx >= (int)s.zones.size())
    {
        selZoneNameLbl.setText ("Sample: (click a zone on grid)", juce::dontSendNotification);
        waveEditor.setZone (nullptr);
        return;
    }
    auto z = s.zones[(size_t)idx];
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
    auto& s = processor.getSlot (activeSlotIdx);
    int idx = mappingGrid.selectedZoneIdx;
    if (idx < 0 || idx >= (int)s.zones.size()) return;
    auto z = s.zones[(size_t)idx];
    z->rootNote = (int)zRootSlider.getValue(); z->lowKey = (int)zLowKSlider.getValue();
    z->highKey  = (int)zHighKSlider.getValue(); z->lowVel = (int)zLowVSlider.getValue();
    z->highVel  = (int)zHighVSlider.getValue(); z->volumeDb = (float)zVolSlider.getValue();
    z->pan      = (float)zPanSlider.getValue(); z->tuneSemi = (float)zTuneSlider.getValue();
    mappingGrid.repaint(); keyboard.repaint();
}

void NoiseMu6ixSamplerAudioProcessorEditor::refreshFXUI()
{
    auto& f = processor.getSlot (activeSlotIdx).fx;
    fxEqOn.setToggleState (f.eqOn.load(), juce::dontSendNotification);
    fxEqLow.setValue (f.eqLowDb.load(), juce::dontSendNotification);
    fxEqMid.setValue (f.eqMidDb.load(), juce::dontSendNotification);
    fxEqHigh.setValue (f.eqHighDb.load(), juce::dontSendNotification);
    fxFltOn.setToggleState (f.filterOn.load(), juce::dontSendNotification);
    fxFltTypeBox.setSelectedId (f.filterType.load() + 1, juce::dontSendNotification);
    fxFltCut.setValue (f.filterCutoff.load(), juce::dontSendNotification);
    fxFltRes.setValue (f.filterRes.load(), juce::dontSendNotification);
    fxDrvOn.setToggleState (f.driveOn.load(), juce::dontSendNotification);
    fxDrvAmt.setValue (f.driveAmount.load(), juce::dontSendNotification);
    fxChoOn.setToggleState (f.chorusOn.load(), juce::dontSendNotification);
    fxChoRate.setValue (f.chorusRate.load(), juce::dontSendNotification);
    fxChoDepth.setValue (f.chorusDepth.load(), juce::dontSendNotification);
    fxChoMix.setValue (f.chorusMix.load(), juce::dontSendNotification);
    fxDlyOn.setToggleState (f.delayOn.load(), juce::dontSendNotification);
    fxDlyTime.setValue (f.delayTimeMs.load(), juce::dontSendNotification);
    fxDlyFb.setValue (f.delayFeedback.load(), juce::dontSendNotification);
    fxDlyMix.setValue (f.delayMix.load(), juce::dontSendNotification);
    fxRevOn.setToggleState (f.reverbOn.load(), juce::dontSendNotification);
    fxRevSize.setValue (f.reverbSize.load(), juce::dontSendNotification);
    fxRevDamp.setValue (f.reverbDamp.load(), juce::dontSendNotification);
    fxRevMix.setValue (f.reverbMix.load(), juce::dontSendNotification);
    fxAtk.setValue (f.attack.load(), juce::dontSendNotification);
    fxDec.setValue (f.decay.load(), juce::dontSendNotification);
    fxSus.setValue (f.sustain.load(), juce::dontSendNotification);
    fxRel.setValue (f.release.load(), juce::dontSendNotification);
}

void NoiseMu6ixSamplerAudioProcessorEditor::applyFXToProcessor()
{
    auto& f = processor.getSlot (activeSlotIdx).fx;
    f.eqOn.store (fxEqOn.getToggleState()); f.eqLowDb.store ((float)fxEqLow.getValue());
    f.eqMidDb.store ((float)fxEqMid.getValue()); f.eqHighDb.store ((float)fxEqHigh.getValue());
    f.filterOn.store (fxFltOn.getToggleState()); f.filterType.store (fxFltTypeBox.getSelectedId() - 1);
    f.filterCutoff.store ((float)fxFltCut.getValue()); f.filterRes.store ((float)fxFltRes.getValue());
    f.driveOn.store (fxDrvOn.getToggleState()); f.driveAmount.store ((float)fxDrvAmt.getValue());
    f.chorusOn.store (fxChoOn.getToggleState()); f.chorusRate.store ((float)fxChoRate.getValue());
    f.chorusDepth.store ((float)fxChoDepth.getValue()); f.chorusMix.store ((float)fxChoMix.getValue());
    f.delayOn.store (fxDlyOn.getToggleState()); f.delayTimeMs.store ((float)fxDlyTime.getValue());
    f.delayFeedback.store ((float)fxDlyFb.getValue()); f.delayMix.store ((float)fxDlyMix.getValue());
    f.reverbOn.store (fxRevOn.getToggleState()); f.reverbSize.store ((float)fxRevSize.getValue());
    f.reverbDamp.store ((float)fxRevDamp.getValue()); f.reverbMix.store ((float)fxRevMix.getValue());
    f.attack.store ((float)fxAtk.getValue()); f.decay.store ((float)fxDec.getValue());
    f.sustain.store ((float)fxSus.getValue()); f.release.store ((float)fxRel.getValue());
}

void NoiseMu6ixSamplerAudioProcessorEditor::refreshPanelEditorInspector()
{
    auto& s = processor.getSlot (activeSlotIdx);
    int idx = panelDesignCanvas.selectedMacroIdx;
    bool valid = (idx >= 0 && idx < InstrumentSlot::numMacros && s.macroConfigs[idx].visible);
    peNameEdit.setEnabled (valid); peStyleBtn.setEnabled (valid); peTargetBox.setEnabled (valid);
    peMinEdit.setEnabled (valid); peMaxEdit.setEnabled (valid); peDelCtrlBtn.setEnabled (valid);
    if (valid)
    {
        const auto& cfg = s.macroConfigs[idx];
        peNameEdit.setText (cfg.name, juce::dontSendNotification);
        peStyleBtn.setButtonText (cfg.isSlider ? "Slider" : "Knob");
        peTargetBox.setSelectedId (cfg.target, juce::dontSendNotification);
        peMinEdit.setText (juce::String ((int)std::round (cfg.minVal * 100.0f)), juce::dontSendNotification);
        peMaxEdit.setText (juce::String ((int)std::round (cfg.maxVal * 100.0f)), juce::dontSendNotification);
    }
}

void NoiseMu6ixSamplerAudioProcessorEditor::applyPanelEditorInspector()
{
    auto& s = processor.getSlot (activeSlotIdx);
    int idx = panelDesignCanvas.selectedMacroIdx;
    if (idx < 0 || idx >= InstrumentSlot::numMacros) return;
    auto& cfg = s.macroConfigs[idx];
    cfg.name   = peNameEdit.getText();
    cfg.target = peTargetBox.getSelectedId();
    cfg.minVal = juce::jlimit (0.0f, 1.0f, peMinEdit.getText().getFloatValue() / 100.0f);
    cfg.maxVal = juce::jlimit (0.0f, 1.0f, peMaxEdit.getText().getFloatValue() / 100.0f);
    panelDesignCanvas.repaint();
    rebuildMultiRackCards();
}

void NoiseMu6ixSamplerAudioProcessorEditor::updateModeVisibility()
{
    togGroupBtn.setVisible (inEditMode); togMapBtn.setVisible (inEditMode);
    togWaveBtn.setVisible (inEditMode);  togFXBtn.setVisible (inEditMode);
    togPanelBtn.setVisible (inEditMode);

    auto onCol = juce::Colour (0xffd9a05b), offCol = juce::Colour (0xff343842);
    togGroupBtn.setColour (juce::TextButton::buttonColourId, showGroupSec ? onCol : offCol);
    togMapBtn.setColour   (juce::TextButton::buttonColourId, showMapSec   ? onCol : offCol);
    togWaveBtn.setColour  (juce::TextButton::buttonColourId, showWaveSec  ? onCol : offCol);
    togFXBtn.setColour    (juce::TextButton::buttonColourId, showFXSec    ? onCol : offCol);
    togPanelBtn.setColour (juce::TextButton::buttonColourId, showPanelSec ? onCol : offCol);

    editViewport.setVisible (inEditMode);
    addNewSlotBtn.setVisible (!inEditMode);
    multiRackViewport.setVisible (!inEditMode);

    if (!inEditMode) rebuildMultiRackCards();
    layoutBaseUI();
    rootCanvas.repaint();
}

void NoiseMu6ixSamplerAudioProcessorEditor::layoutScrollableRack (int w)
{
    int pad = 8, innerW = w - pad * 2, y = 6;

    // 1. Group Editor
    for (auto* c : std::initializer_list<juce::Component*>{
            &addGroupBtn, &deleteGroupBtn, &rrToggleBtn, &rrModeBox, &ksMasterBtn, &showAllGroupsBtn,
            &groupCards, &grpNameLabel, &groupSoloBtn, &groupMuteBtn,
            &grpVolLbl, &grpVolSlider, &grpPanLbl, &grpPanSlider, &grpTuneLbl, &grpTuneSlider,
            &grpKSEnabledBtn, &grpKSNoteLbl, &grpKSNoteSlider, &grpKSColorBox })
        c->setVisible (showGroupSec);

    if (showGroupSec)
    {
        editRack.groupHeaderY = y; y += 26;
        addGroupBtn.setBounds (pad, y, 92, 24); deleteGroupBtn.setBounds (pad + 96, y, 68, 24);
        rrToggleBtn.setBounds (pad + 170, y, 142, 24); rrModeBox.setBounds (pad + 316, y, 108, 24);
        ksMasterBtn.setBounds (pad + 428, y, 108, 24); showAllGroupsBtn.setBounds (pad + 542, y, 128, 24);
        y += 28; groupCards.setBounds (pad, y, innerW, 84);
        y += 90;
        grpNameLabel.setBounds (pad, y, 120, 22); groupSoloBtn.setBounds (pad + 124, y, 52, 22);
        groupMuteBtn.setBounds (pad + 178, y, 54, 22); grpVolLbl.setBounds (pad + 236, y, 28, 22);
        grpVolSlider.setBounds (pad + 266, y, 118, 22); grpPanLbl.setBounds (pad + 388, y, 28, 22);
        grpPanSlider.setBounds (pad + 418, y, 108, 22); grpTuneLbl.setBounds (pad + 530, y, 34, 22);
        grpTuneSlider.setBounds (pad + 566, y, 118, 22);
        y += 26;
        grpKSEnabledBtn.setBounds (pad, y, 88, 22); grpKSNoteLbl.setBounds (pad + 92, y, 48, 22);
        grpKSNoteSlider.setBounds (pad + 144, y, 95, 22); grpKSColorBox.setBounds (pad + 246, y, 110, 22);
        y += 30;
    }
    else editRack.groupHeaderY = -1;

    // 2. Map Editor
    for (auto* c : std::initializer_list<juce::Component*>{
            &selZoneNameLbl, &autoSpreadBtn, &delZoneBtn, &mappingGrid,
            &zRootLbl, &zRootSlider, &zLowKLbl, &zLowKSlider, &zHighKLbl, &zHighKSlider,
            &zLowVLbl, &zLowVSlider, &zHighVLbl, &zHighVSlider, &zVolLbl, &zVolSlider,
            &zPanLbl, &zPanSlider, &zTuneLbl, &zTuneSlider })
        c->setVisible (showMapSec);

    if (showMapSec)
    {
        editRack.mapHeaderY = y; y += 26;
        selZoneNameLbl.setBounds (pad, y, 320, 22);
        autoSpreadBtn.setBounds (w - pad - 186, y, 100, 22); delZoneBtn.setBounds (w - pad - 80, y, 80, 22);
        y += 26;
        int colW = innerW / 8;
        auto placeCol = [&](int idx, juce::Label& l, juce::Slider& s) {
            int cx = pad + idx * colW + 2;
            l.setBounds (cx, y, colW - 4, 14); s.setBounds (cx, y + 15, colW - 4, 20);
        };
        placeCol (0, zRootLbl, zRootSlider); placeCol (1, zLowKLbl, zLowKSlider);
        placeCol (2, zHighKLbl, zHighKSlider); placeCol (3, zLowVLbl, zLowVSlider);
        placeCol (4, zHighVLbl, zHighVSlider); placeCol (5, zVolLbl, zVolSlider);
        placeCol (6, zPanLbl, zPanSlider); placeCol (7, zTuneLbl, zTuneSlider);
        y += 40; mappingGrid.setBounds (pad, y, innerW, 210);
        y += 218;
    }
    else editRack.mapHeaderY = -1;

    // 3. Wave Editor
    for (auto* c : std::initializer_list<juce::Component*>{
            &loopToggle, &autoLoopBtn, &waveDragModeBox, &cfLbl, &crossfadeSlider, &zoomLbl, &zoomSlider, &waveEditor })
        c->setVisible (showWaveSec);

    if (showWaveSec)
    {
        editRack.waveHeaderY = y; y += 26;
        loopToggle.setBounds (pad, y, 95, 24); autoLoopBtn.setBounds (pad + 100, y, 152, 24);
        waveDragModeBox.setBounds (pad + 256, y, 156, 24); cfLbl.setBounds (pad + 416, y, 56, 24);
        crossfadeSlider.setBounds (pad + 474, y, 114, 24); zoomLbl.setBounds (pad + 592, y, 36, 24);
        zoomSlider.setBounds (pad + 630, y, 100, 24);
        y += 28; waveEditor.setBounds (pad, y, innerW, 215);
        y += 223;
    }
    else editRack.waveHeaderY = -1;

    // 4. FX Editor
    for (auto* c : std::initializer_list<juce::Component*>{
            &fxEqOn, &fxFltOn, &fxDrvOn, &fxChoOn, &fxDlyOn, &fxRevOn, &fxFltTypeBox,
            &fxEqLow, &fxEqMid, &fxEqHigh, &fxFltCut, &fxFltRes, &fxDrvAmt,
            &fxChoRate, &fxChoDepth, &fxChoMix, &fxDlyTime, &fxDlyFb, &fxDlyMix,
            &fxRevSize, &fxRevDamp, &fxRevMix, &fxAtk, &fxDec, &fxSus, &fxRel,
            &lEqLow, &lEqMid, &lEqHigh, &lFltCut, &lFltRes, &lDrvAmt,
            &lChoRate, &lChoDepth, &lChoMix, &lDlyTime, &lDlyFb, &lDlyMix,
            &lRevSize, &lRevDamp, &lRevMix, &lAtk, &lDec, &lSus, &lRel })
        c->setVisible (showFXSec);

    if (showFXSec)
    {
        editRack.fxHeaderY = y; y += 26;
        auto layoutMod = [](juce::Rectangle<int> box, juce::ToggleButton* tog, juce::ComboBox* combo,
                            std::vector<std::pair<juce::Label*, juce::Slider*>> knobs) {
            auto top = box.removeFromTop (22);
            if (tog) tog->setBounds (top.removeFromLeft (105));
            if (combo) combo->setBounds (top.removeFromRight (85));
            int kw = box.getWidth() / std::max (1, (int)knobs.size());
            for (auto& kv : knobs) { auto c = box.removeFromLeft (kw).reduced (2, 2); kv.first->setBounds (c.removeFromTop (14)); kv.second->setBounds (c); }
        };
        int w3 = innerW / 3;
        juce::Rectangle<int> r1 (pad, y, innerW, 130);
        layoutMod (r1.removeFromLeft (w3).reduced (4), &fxEqOn, nullptr, { {&lEqLow, &fxEqLow}, {&lEqMid, &fxEqMid}, {&lEqHigh, &fxEqHigh} });
        layoutMod (r1.removeFromLeft (w3).reduced (4), &fxFltOn, &fxFltTypeBox, { {&lFltCut, &fxFltCut}, {&lFltRes, &fxFltRes} });
        layoutMod (r1.reduced (4), &fxDrvOn, nullptr, { {&lDrvAmt, &fxDrvAmt} }); y += 136;

        juce::Rectangle<int> r2 (pad, y, innerW, 130);
        layoutMod (r2.removeFromLeft (w3).reduced (4), &fxChoOn, nullptr, { {&lChoRate, &fxChoRate}, {&lChoDepth, &fxChoDepth}, {&lChoMix, &fxChoMix} });
        layoutMod (r2.removeFromLeft (w3).reduced (4), &fxDlyOn, nullptr, { {&lDlyTime, &fxDlyTime}, {&lDlyFb, &fxDlyFb}, {&lDlyMix, &fxDlyMix} });
        layoutMod (r2.reduced (4), &fxRevOn, nullptr, { {&lRevSize, &fxRevSize}, {&lRevDamp, &fxRevDamp}, {&lRevMix, &fxRevMix} }); y += 136;

        juce::Rectangle<int> r3 (pad, y, innerW, 122);
        layoutMod (r3.reduced (4), nullptr, nullptr, { {&lAtk, &fxAtk}, {&lDec, &fxDec}, {&lSus, &fxSus}, {&lRel, &fxRel} }); y += 130;
    }
    else editRack.fxHeaderY = -1;

    // 5. Panel Editor (Matches exact 740x210 Multi-Rack Slot Panel!)
    for (auto* c : std::initializer_list<juce::Component*>{
            &peAddKnobBtn, &peAddSliderBtn, &peDelCtrlBtn, &peNameLbl, &peNameEdit,
            &peStyleBtn, &peTargetBox, &peRangeLbl, &peMinEdit, &peToLbl, &peMaxEdit, &panelDesignCanvas })
        c->setVisible (showPanelSec);

    if (showPanelSec)
    {
        editRack.panelHeaderY = y; y += 26;
        peAddKnobBtn.setBounds (pad, y, 64, 24); peAddSliderBtn.setBounds (pad + 68, y, 68, 24);
        peDelCtrlBtn.setBounds (pad + 140, y, 56, 24); peNameLbl.setBounds (pad + 202, y, 42, 24);
        peNameEdit.setBounds (pad + 244, y, 95, 24); peStyleBtn.setBounds (pad + 344, y, 54, 24);
        peTargetBox.setBounds (pad + 402, y, 108, 24); peRangeLbl.setBounds (pad + 514, y, 66, 24);
        peMinEdit.setBounds (pad + 580, y, 40, 24); peToLbl.setBounds (pad + 622, y, 20, 24);
        peMaxEdit.setBounds (pad + 644, y, 40, 24);
        y += 30;
        panelDesignCanvas.setBounds (pad, y, innerW, 212);
        y += 222;
    }
    else editRack.panelHeaderY = -1;

    editRack.setBounds (0, 0, w, std::max (y + 10, 440));
    editRack.repaint();
}

void NoiseMu6ixSamplerAudioProcessorEditor::layoutBaseUI()
{
    auto area = juce::Rectangle<int> (0, 0, baseWidth, baseHeight);

    // Header (44px)
    auto header = area.removeFromTop (44).reduced (6, 5);
    logoBtn.setBounds (header.removeFromLeft (188));
    header.removeFromLeft (10);
    modeToggleBtn.setBounds (header.removeFromLeft (90).reduced (0, 2)); header.removeFromLeft (6);
    activeSlotBox.setBounds (header.removeFromLeft (80).reduced (0, 2)); header.removeFromLeft (6);
    saveInstBtn.setBounds   (header.removeFromLeft (84).reduced (0, 2)); header.removeFromLeft (5);
    loadInstBtn.setBounds   (header.removeFromLeft (84).reduced (0, 2));

    voicesLabel.setBounds  (header.removeFromRight (76));
    versionLabel.setBounds (header.removeFromRight (52));

    // Left Sidebar (196px)
    auto left = area.removeFromLeft (196).reduced (6, 6);
    auto sideTabs = left.removeFromTop (24);
    sideLibTabBtn.setBounds (sideTabs.removeFromLeft (sideTabs.getWidth() / 2 - 2));
    sideTabs.removeFromLeft (4);
    sideFilesTabBtn.setBounds (sideTabs);
    left.removeFromTop (6);

    // Libraries view
    auto libArea = left;
    auto libBtns = libArea.removeFromTop (24);
    addLibraryBtn.setBounds (libBtns.removeFromLeft (72)); libBtns.removeFromLeft (3);
    setPhotoBtn.setBounds   (libBtns.removeFromLeft (56)); libBtns.removeFromLeft (3);
    removeLibraryBtn.setBounds (libBtns);
    libArea.removeFromTop (6);
    libraryViewport.setBounds (libArea);
    libraryBrowser.setSize (libArea.getWidth() - libraryViewport.getScrollBarThickness() - 2, libraryBrowser.getHeight());
    libraryBrowser.refreshHeight();

    // Files view
    auto fileArea = left;
    browseFolderBtn.setBounds (fileArea.removeFromTop (26)); fileArea.removeFromTop (4);
    folderPathLabel.setBounds (fileArea.removeFromTop (18)); fileArea.removeFromTop (4);
    addSelectedFileBtn.setBounds (fileArea.removeFromBottom (26)); fileArea.removeFromBottom (5);
    fileListBox.setBounds (fileArea);

    // Bottom Piano Keyboard
    auto kbArea = area.removeFromBottom (70).reduced (8, 4);
    keyboard.setBounds (kbArea);
    keyboard.setKeyWidth ((float)kbArea.getWidth() / 43.0f);

    // Right Main Area
    auto rack = area.reduced (6, 6);
    if (inEditMode)
    {
        auto togBar = rack.removeFromTop (26);
        int btnW = (togBar.getWidth() - 16) / 5;
        togGroupBtn.setBounds (togBar.removeFromLeft (btnW)); togBar.removeFromLeft (4);
        togMapBtn.setBounds   (togBar.removeFromLeft (btnW)); togBar.removeFromLeft (4);
        togWaveBtn.setBounds  (togBar.removeFromLeft (btnW)); togBar.removeFromLeft (4);
        togFXBtn.setBounds    (togBar.removeFromLeft (btnW)); togBar.removeFromLeft (4);
        togPanelBtn.setBounds (togBar);

        rack.removeFromTop (6);
        editViewport.setBounds (rack);
        layoutScrollableRack (rack.getWidth() - editViewport.getScrollBarThickness() - 2);
    }
    else
    {
        auto topBar = rack.removeFromTop (26);
        addNewSlotBtn.setBounds (topBar.removeFromLeft (185));
        rack.removeFromTop (6);
        multiRackViewport.setBounds (rack);
        rebuildMultiRackCards();
    }
}
