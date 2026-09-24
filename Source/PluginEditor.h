/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Style/VisualStyle.h"
#include "UI/ParaGraphiQComponent.h"

//==============================================================================
/**
*/
class RoomShapeAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    RoomShapeAudioProcessorEditor (RoomShapeAudioProcessor&);
    ~RoomShapeAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    RoomShapeAudioProcessor& audioProcessor;
    
    void configureSlider (juce::Slider&, const juce::String& suffix);
    
    // Reverb controls
    juce::Slider drySlider;
    juce::Slider wetSlider;
    juce::Slider roomSizeSlider;
    
    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dryAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wetAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> roomSizeAttach;
    
    // Temporary ParaGraphiQ stand-in
    class EQPlaceholderComponent;
    std::unique_ptr<EQPlaceholderComponent> eqPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomShapeAudioProcessorEditor)
};
