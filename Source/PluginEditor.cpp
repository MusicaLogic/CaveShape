/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

class RoomShapeAudioProcessorEditor::EQPlaceholderComponent
    : public juce::Component
{
public:
    
    EQPlaceholderComponent(RoomShapeAudioProcessor& processor):
                                        processor(processor),
                                        paraGraphiQ(processor)
    {
        addAndMakeVisible(paraGraphiQ);
    }
    
    void resized() override
    {
        paraGraphiQ.setBounds(getLocalBounds());
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced (2);

        g.setColour (VisualStyle::panelBackground);
        g.fillRoundedRectangle (
            bounds.toFloat(),
            VisualStyle::Geometry::componentCornerRadius);

        g.setColour (VisualStyle::Palette::magenta.dim);
        g.drawRoundedRectangle (
            bounds.toFloat(),
            VisualStyle::Geometry::componentCornerRadius,
            VisualStyle::Geometry::componentBorderThickness);
    }
private:
    RoomShapeAudioProcessor& processor;
    ParaGraphiQComponent paraGraphiQ;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (
            EQPlaceholderComponent)
};

//==============================================================================
RoomShapeAudioProcessorEditor::RoomShapeAudioProcessorEditor (RoomShapeAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    //==========================================================================
    // reverb controls

    addAndMakeVisible (drySlider);
    configureSlider (drySlider, "Dry");

    addAndMakeVisible (wetSlider);
    configureSlider (wetSlider, "Wet");

    addAndMakeVisible (roomSizeSlider);
    configureSlider (roomSizeSlider, "Room");

    dryAttach=
        std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.parameters,
                "dry",
                drySlider);

    wetAttach =
        std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.parameters,
                "wet",
                wetSlider);

    roomSizeAttach =
        std::make_unique<
            juce::AudioProcessorValueTreeState::SliderAttachment>(
                audioProcessor.parameters,
                "roomSize",
                roomSizeSlider);
    
    //==========================================================================
    // ParaGraphiQ placeholder

    eqPanel =
        std::make_unique<EQPlaceholderComponent>(audioProcessor);
    
    addAndMakeVisible (*eqPanel);
    
    setSize (800, 400);
}

RoomShapeAudioProcessorEditor::~RoomShapeAudioProcessorEditor()
{
}

//==============================================================================
void RoomShapeAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (VisualStyle::background);

    auto bounds = getLocalBounds().reduced (2);

    g.setColour (VisualStyle::panelBackground);
    g.fillRoundedRectangle (
        bounds.toFloat(),
        VisualStyle::Geometry::componentCornerRadius);

    g.setColour (VisualStyle::Palette::magenta.dim);
    g.drawRoundedRectangle (
        bounds.toFloat(),
        VisualStyle::Geometry::componentCornerRadius,
        VisualStyle::Geometry::componentBorderThickness);
}

void RoomShapeAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto area = getLocalBounds();
    
    // for the eq panel when collapsed
    // Keep the upper controls above the EQ sheet.
    const int topRowY = area.getY() + 0;
//    const int rowHeight = 145;
    const int topRowHeight = area.getHeight()/3.0f;
    const int gap = 18;

    const int columnWidth =
        (area.getWidth() - 2 * gap) / 3;

    // Top row: WET | FEEDBACK | FREEZE
    drySlider.setBounds (
        area.getX(),
        topRowY,
        columnWidth,
        topRowHeight);

    wetSlider.setBounds (
        area.getX() + columnWidth + gap,
        topRowY,
        columnWidth,
        topRowHeight);

    roomSizeSlider.setBounds (
        area.getX() + 2 * (columnWidth + gap),
        topRowY,
        columnWidth,
        topRowHeight);

    // Bottom row: TIME | TAP
    const int bottomRowHeight = 2.0f*area.getHeight()/3.0f;
//    const int bottomY = topRowY + topRowHeight + 24;
//    const int bottomGap = 18;
//    const int bottomWidth =
//        (area.getWidth() - bottomGap) / 2;
//
//    //==========================================================================
//    // Sliding EQ bottom sheet
//
//    const int expandedHeight =
//        juce::jlimit (
//            220,
//            getHeight() - 50,
//            (int) (bottomRowHeight + 2.0f*collapsedHeight));
//
//    const float eased =
//        eqAnimationPosition
//        * eqAnimationPosition
//        * (3.0f - 2.0f * eqAnimationPosition);
//
//    const int panelHeight =
//        juce::roundToInt (
//            collapsedHeight
//            + (expandedHeight - collapsedHeight) * eased);

    eqPanel->setBounds (
        0,
        getHeight() - bottomRowHeight,
        getWidth(),
        bottomRowHeight);
}

void RoomShapeAudioProcessorEditor::configureSlider (
    juce::Slider& s,
    const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);

    s.setTextBoxStyle (
        juce::Slider::TextBoxBelow,
        false,
        78,
        20);

    s.setColour (
        juce::Slider::textBoxTextColourId,
        VisualStyle::Palette::magenta.highlight);

    s.setColour (
        juce::Slider::textBoxOutlineColourId,
        juce::Colours::transparentBlack);

    s.setColour (
        juce::Slider::rotarySliderFillColourId,
        VisualStyle::Palette::magenta.highlight);

    s.setColour (
        juce::Slider::rotarySliderOutlineColourId,
        VisualStyle::Palette::magenta.dim);

    s.setColour (
        juce::Slider::thumbColourId,
        juce::Colours::transparentBlack);

    s.setTextValueSuffix (" " + suffix);
}
