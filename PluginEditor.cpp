#include "PluginEditor.h"

// ==================== WaveformDisplay ====================
void WaveformDisplay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff12141a));
    g.setColour (juce::Colour (0xff2b3040));
    g.drawRect (getLocalBounds(), 1);

    if (!zone || zone->buffer.getNumSamples() == 0)
    {
        g.setColour (juce::Colours::grey);
        g.setFont (15.0f);
        g.drawFittedText ("Drag & Drop WAV Samples Here (or click Auto-Map Folder)",
                          getLocalBounds(), juce::Justification::centred, 1);
        return;
    }

    auto b = getLocalBounds().reduced (4);
    int w = b.getWidth();
    int h = b.getHeight();
    int numSamples = zone->buffer.getNumSamples();
    const float* data = zone->buffer.getReadPointer (0);

    // Draw Loop Region
    if (zone->isLooping.load())
    {
        float xStart = b.getX() + (float)zone->loopStart.load() / numSamples * w;
        float xEnd   = b.getX() + (float)zone->loopEnd.load()   / numSamples * w;
        g.setColour (juce::Colour (0x3300d4ff));
        g.fillRect (xStart, (float)b.getY(), std::max (2.0f, xEnd - xStart), (float)h);

        g.setColour (juce::Colour (0xff00e5ff));
        g.drawVerticalLine ((int)xStart, (float)b.getY(), (float)b.getBottom());
        g.setColour (juce::Colour (0xffff6b6b));
        g.drawVerticalLine ((int)xEnd, (float)b.getY(), (float)b.getBottom());
    }

    // Draw Waveform
    g.setColour (juce::Colour (0xff7aa2f7));
    float midY = b.getCentreY();
    int step = std::max (1, numSamples / std::max (1, w));

    for (int x = 0; x < w; ++x)
    {
        int idx = std::min (numSamples - 1, x * step);
        float peak = 0.0f;
        for (int k = 0; k < step && idx + k < numSamples; ++k)
            peak = std::max (peak, std::abs (data[idx + k]));

        float barH = peak * (h * 0.46f);
        g.drawVerticalLine (b.getX() + x, midY - barH, midY + barH);
    }
}

void WaveformDisplay::mouseDown (const juce::MouseEvent& e)
{
    if (!zone || zone->buffer.getNumSamples() == 0) return;
    int numSamples = zone->buffer.getNumSamples();
    int clickedSample = (int)(((float)e.x / (float)std::max (1, getWidth())) * numSamples);

    int distStart = std::abs (clickedSample - zone->loopStart.load());
    int distEnd   = std::abs (clickedSample - zone->loopEnd.load());
    draggingStart = (distStart <= distEnd);
    zone->isLooping.store (true);
    mouseDrag (e);
}

void WaveformDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (!zone || zone->buffer.getNumSamples() == 0) return;
    int numSamples = zone->buffer.getNumSamples();
    int rawSample = juce::jlimit (0, numSamples - 1,
                    (int)(((float)e.x / (float)std::max (1, getWidth())) * numSamples));
    int snapped = NoiseMu6ixSamplerAudioProcessor::findNearestZeroCrossing (zone->buffer, rawSample);

    if (draggingStart)
    {
        int end = zone->loopEnd.load();
        if (snapped < end - 64) zone->loopStart.store (snapped);
    }
    else
    {
        int start = zone->loopStart.load();
        if (snapped > start + 64) zone->loopEnd.store (snapped);
    }
    repaint();
}

// ==================== CustomMacroSlot ====================
CustomMacroSlot::CustomMacroSlot (NoiseMu6ixSamplerAudioProcessor& p, int slotIndex)
    : processor (p), index (slotIndex)
{
    auto& cfg = processor.macroConfigs[index];

    nameLabel.setText (cfg.name, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setEditable (true);
    nameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff232734));
    nameLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    nameLabel.onTextChange = [this] { processor.macroConfigs[index].name = nameLabel.getText(); };
    addAndMakeVisible (nameLabel);

    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 16);
    slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xff00e5ff));
    slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff00e5ff));
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "macro" + juce::String (index), slider);

    styleToggle.onClick = [this] {
        processor.macroConfigs[index].isSlider = !processor.macroConfigs[index].isSlider;
        refreshStyle();
    };
    addAndMakeVisible (styleToggle);

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

void CustomMacroSlot::refreshStyle()
{
    bool isSl = processor.macroConfigs[index].isSlider;
    slider.setSliderStyle (isSl ? juce::Slider::LinearVertical : juce::Slider::RotaryHorizontalVerticalDrag);
    styleToggle.setButtonText (isSl ? "Slider" : "Knob");
    repaint();
}

void CustomMacroSlot::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1d26));
    g.setColour (juce::Colour (0xff2e3445));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.0f);
}

void CustomMacroSlot::resized()
{
    auto r = getLocalBounds().reduced (6);
    nameLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (4);
    targetBox.setBounds (r.removeFromBottom (22));
    r.removeFromBottom (4);
    styleToggle.setBounds (r.removeFromBottom (20));
    r.removeFromBottom (4);
    slider.setBounds (r);
}

// ==================== Main Editor ====================
NoiseMu6ixSamplerAudioProcessorEditor::NoiseMu6ixSamplerAudioProcessorEditor (NoiseMu6ixSamplerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize (1000, 660);

    // Top Buttons
    addAndMakeVisible (loadFilesBtn);
    loadFilesBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Select WAV Samples", juce::File{}, "*.wav;*.flac;*.aif");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectMultipleItems,
            [this](const juce::FileChooser& fc) {
                for (auto& f : fc.getResults()) processor.loadSampleFile (f, true);
                refreshZoneList();
            });
    };

    addAndMakeVisible (loadFolderBtn);
    loadFolderBtn.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser> ("Select Sample Folder");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc) {
                processor.loadSampleFolder (fc.getResult());
                refreshZoneList();
            });
    };

    addAndMakeVisible (clearBtn);
    clearBtn.onClick = [this] {
        processor.clearAllZones();
        refreshZoneList();
    };

    rrModeBox.addItem ("RR: Cycle", 1);
    rrModeBox.addItem ("RR: No-Repeat Random", 2);
    rrModeBox.addItem ("RR: True Random", 3);
    rrModeBox.setSelectedId (processor.rrMode.load() + 1, juce::dontSendNotification);
    rrModeBox.onChange = [this] { processor.rrMode.store (rrModeBox.getSelectedId() - 1); };
    addAndMakeVisible (rrModeBox);

    addAndMakeVisible (zoneSelector);
    zoneSelector.onChange = [this] { loadSelectedZoneToInspector(); };

    addAndMakeVisible (deleteZoneBtn);
    deleteZoneBtn.onClick = [this] {
        int idx = zoneSelector.getSelectedItemIndex();
        if (idx >= 0) { processor.removeZone (idx); refreshZoneList(); }
    };

    // Waveform & Loop Controls
    addAndMakeVisible (waveform);
    addAndMakeVisible (loopToggle);
    loopToggle.onClick = [this] { applyInspectorToZone(); };

    addAndMakeVisible (autoLoopBtn);
    autoLoopBtn.onClick = [this] {
        int idx = zoneSelector.getSelectedItemIndex();
        const juce::ScopedLock sl (processor.zoneLock);
        if (idx >= 0 && idx < (int)processor.zones.size())
        {
            processor.zones[(size_t)idx]->isLooping.store (true);
            processor.autoDetectLoop (processor.zones[(size_t)idx]);
            loopToggle.setToggleState (true, juce::dontSendNotification);
            crossfadeSlider.setValue (processor.zones[(size_t)idx]->crossfadeSamples.load(), juce::dontSendNotification);
            waveform.repaint();
        }
    };

    crossfadeSlider.setRange (64, 15000, 1);
    crossfadeSlider.setValue (2048);
    crossfadeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    crossfadeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 55, 20);
    crossfadeSlider.onValueChange = [this] { applyInspectorToZone(); };
    addAndMakeVisible (crossfadeSlider);
    addAndMakeVisible (cfLabel);

    // Zone Inspector Sliders
    auto setupIntSlider = [this](juce::Slider& s, juce::Label& l, int minV, int maxV, int defV) {
        s.setRange (minV, maxV, 1);
        s.setValue (defV);
        s.setSliderStyle (juce::Slider::IncDecButtons);
        s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 22);
        s.onValueChange = [this] { applyInspectorToZone(); };
        addAndMakeVisible (s);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    setupIntSlider (rootNoteSlider, rootLbl, 0, 127, 60);
    setupIntSlider (lowKeySlider,   lowKLbl, 0, 127, 0);
    setupIntSlider (highKeySlider,  highKLbl, 0, 127, 127);
    setupIntSlider (lowVelSlider,   lowVLbl, 0, 127, 0);
    setupIntSlider (highVelSlider,  highVLbl, 0, 127, 127);
    setupIntSlider (rrGroupSlider,  rrLbl,   1, 10,  1);

    // 6 Custom Macro Slots
    for (int i = 0; i < NoiseMu6ixSamplerAudioProcessor::numMacros; ++i)
    {
        auto slot = std::make_unique<CustomMacroSlot> (processor, i);
        addAndMakeVisible (*slot);
        macroSlots.push_back (std::move (slot));
    }

    // Keyboard
    keyboard.setAvailableRange (24, 96);
    addAndMakeVisible (keyboard);

    refreshZoneList();
}

bool NoiseMu6ixSamplerAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".aif"))
            return true;
    return false;
}

void NoiseMu6ixSamplerAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        processor.loadSampleFile (juce::File (f), true);
    refreshZoneList();
}

void NoiseMu6ixSamplerAudioProcessorEditor::refreshZoneList()
{
    zoneSelector.clear (juce::dontSendNotification);
    const juce::ScopedLock sl (processor.zoneLock);
    for (size_t i = 0; i < processor.zones.size(); ++i)
    {
        auto& z = processor.zones[i];
        juce::String label = juce::String ((int)i + 1) + ". " + z->name
                           + " [RR" + juce::String (z->rrGroup)
                           + " | Vel:" + juce::String (z->lowVel) + "-" + juce::String (z->highVel) + "]";
        zoneSelector.addItem (label, (int)i + 1);
    }
    if (!processor.zones.empty())
        zoneSelector.setSelectedItemIndex ((int)processor.zones.size() - 1, juce::dontSendNotification);

    loadSelectedZoneToInspector();
}

void NoiseMu6ixSamplerAudioProcessorEditor::loadSelectedZoneToInspector()
{
    int idx = zoneSelector.getSelectedItemIndex();
    const juce::ScopedLock sl (processor.zoneLock);
    if (idx < 0 || idx >= (int)processor.zones.size())
    {
        waveform.setZone (nullptr);
        return;
    }

    auto z = processor.zones[(size_t)idx];
    waveform.setZone (z);
    loopToggle.setToggleState (z->isLooping.load(), juce::dontSendNotification);
    crossfadeSlider.setValue (z->crossfadeSamples.load(), juce::dontSendNotification);
    rootNoteSlider.setValue (z->rootNote, juce::dontSendNotification);
    lowKeySlider.setValue (z->lowKey, juce::dontSendNotification);
    highKeySlider.setValue (z->highKey, juce::dontSendNotification);
    lowVelSlider.setValue (z->lowVel, juce::dontSendNotification);
    highVelSlider.setValue (z->highVel, juce::dontSendNotification);
    rrGroupSlider.setValue (z->rrGroup, juce::dontSendNotification);
}

void NoiseMu6ixSamplerAudioProcessorEditor::applyInspectorToZone()
{
    int idx = zoneSelector.getSelectedItemIndex();
    const juce::ScopedLock sl (processor.zoneLock);
    if (idx < 0 || idx >= (int)processor.zones.size()) return;

    auto z = processor.zones[(size_t)idx];
    z->isLooping.store (loopToggle.getToggleState());
    z->crossfadeSamples.store ((int)crossfadeSlider.getValue());
    z->rootNote = (int)rootNoteSlider.getValue();
    z->lowKey   = (int)lowKeySlider.getValue();
    z->highKey  = (int)highKeySlider.getValue();
    z->lowVel   = (int)lowVelSlider.getValue();
    z->highVel  = (int)highVelSlider.getValue();
    z->rrGroup  = (int)rrGroupSlider.getValue();
    waveform.repaint();
}

void NoiseMu6ixSamplerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0f1117));

    // Header
    g.setColour (juce::Colour (0xff1a1e29));
    g.fillRect (0, 0, getWidth(), 44);
    g.setColour (juce::Colour (0xff00e5ff));
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("NOISE MU6IX SAMPLER", 16, 8, 300, 28, juce::Justification::centredLeft);

    g.setColour (juce::Colours::lightgrey);
    g.setFont (12.0f);
    g.drawText ("10-RR | Zero-Cross Auto-Loop | Custom Controls", 260, 14, 320, 20, juce::Justification::centredLeft);
}

void NoiseMu6ixSamplerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (44).reduced (8, 8);
    clearBtn.setBounds (header.removeFromRight (80));
    header.removeFromRight (6);
    loadFolderBtn.setBounds (header.removeFromRight (130));
    header.removeFromRight (6);
    loadFilesBtn.setBounds (header.removeFromRight (95));
    header.removeFromRight (10);
    rrModeBox.setBounds (header.removeFromRight (165));

    // Zone selector bar
    auto zoneBar = area.removeFromTop (34).reduced (10, 4);
    deleteZoneBtn.setBounds (zoneBar.removeFromRight (80));
    zoneBar.removeFromRight (8);
    zoneSelector.setBounds (zoneBar);

    // Waveform
    waveform.setBounds (area.removeFromTop (150).reduced (10, 4));

    // Loop & Crossfade Bar
    auto loopBar = area.removeFromTop (34).reduced (10, 4);
    loopToggle.setBounds (loopBar.removeFromLeft (110));
    autoLoopBtn.setBounds (loopBar.removeFromLeft (175));
    loopBar.removeFromLeft (16);
    cfLabel.setBounds (loopBar.removeFromLeft (75));
    crossfadeSlider.setBounds (loopBar.removeFromLeft (240));

    // Zone Mapping & Velocity Bar
    auto mapArea = area.removeFromTop (52).reduced (10, 2);
    int colW = mapArea.getWidth() / 6;
    auto placeParam = [&](juce::Label& l, juce::Slider& s) {
        auto col = mapArea.removeFromLeft (colW).reduced (4, 0);
        l.setBounds (col.removeFromTop (18));
        s.setBounds (col);
    };
    placeParam (rootLbl,  rootNoteSlider);
    placeParam (lowKLbl,  lowKeySlider);
    placeParam (highKLbl, highKeySlider);
    placeParam (lowVLbl,  lowVelSlider);
    placeParam (highVLbl, highVelSlider);
    placeParam (rrLbl,    rrGroupSlider);

    // Keyboard at bottom
    keyboard.setBounds (area.removeFromBottom (85).reduced (10, 4));

    // 6 Custom Macro Slots in middle
    auto macroArea = area.reduced (10, 8);
    int slotW = macroArea.getWidth() / NoiseMu6ixSamplerAudioProcessor::numMacros;
    for (auto& slot : macroSlots)
        slot->setBounds (macroArea.removeFromLeft (slotW).reduced (4, 0));
}
