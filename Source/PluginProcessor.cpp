/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
RoomShapeAudioProcessor::RoomShapeAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
}

RoomShapeAudioProcessor::~RoomShapeAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
RoomShapeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dry",1},
        "Dry",
        juce::NormalisableRange<float>(0.01f, 1.0f, 0.01f),
        1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"wet",1},
        "Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"roomSize",1},
        "RoomSize",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01),
        0.5f));

    return { params.begin(), params.end() };
}

//==============================================================================
const juce::String RoomShapeAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool RoomShapeAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool RoomShapeAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool RoomShapeAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double RoomShapeAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int RoomShapeAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int RoomShapeAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RoomShapeAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String RoomShapeAudioProcessor::getProgramName (int index)
{
    return {};
}

void RoomShapeAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void RoomShapeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    // EQ DSP-related
    for (auto& rv : reverb_)
        rv.prepare(sampleRate, EQConstants::frequencies);
    
    inputSpectrumAnalyzer.setSampleRate(sampleRate);
    outputSpectrumAnalyzer.setSampleRate(sampleRate);

    inputSpectrumAnalyzer.reset();
    outputSpectrumAnalyzer.reset();
    
    // Load parameters from value tree
    float dry = *parameters.getRawParameterValue("dry");
    float wet = *parameters.getRawParameterValue("wet");
    float roomSize = *parameters.getRawParameterValue("roomSize");
    
    for (auto& rv : reverb_){
        rv.setDry(dry);
        rv.setWet(wet);
        rv.setRoomSize(roomSize);
        
        // We don't currently expose damping as a RoomShape parameter.
        // Keep the existing Freeverb-style default.
        rv.setDamping(0.999f);

        // Apply the stored GraphicEQ state.
        rv.setEQGains(eqState.gains);
    }
    
    parameters.addParameterListener("dry", this);
    parameters.addParameterListener("wet", this);
    parameters.addParameterListener("roomSize", this);
}

void RoomShapeAudioProcessor::parameterChanged(const juce::String& id, float newValue)
{
    if(id == "dry"){
        for (auto& rv : reverb_) rv.setDry(newValue);
    }else if(id == "wet"){
        for (auto& rv : reverb_) rv.setWet(newValue);
    }else if(id == "roomSize"){
        for (auto& rv : reverb_) rv.setRoomSize(newValue);
    }
}

void RoomShapeAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool RoomShapeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void RoomShapeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    
    const auto numSamples =
            static_cast<std::size_t>(buffer.getNumSamples());
    const auto numChannels =
            std::min(
                static_cast<std::size_t>(buffer.getNumChannels()),
                NumChannels);
    
    // TODO: analyze sum of left and right channels
    // TODO: both in input and output
    
    // analyze input spectrum before processing
    inputSpectrumAnalyzer.processBlock(
        buffer.getReadPointer(0),
        buffer.getNumSamples());
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        reverb_[channel].process(
                    buffer.getWritePointer(
                        static_cast<int>(channel)),
                    numSamples);
    }
    
    // analyze output spectrum after processing
    outputSpectrumAnalyzer.processBlock(
        buffer.getReadPointer(0),
        buffer.getNumSamples());
    
    // ============================================================
    // MAKE LATEST DATA AVAILABLE TO GUI
    // ============================================================

    spectrumData.publishInput(
        inputSpectrumAnalyzer.getSpectrum());

    spectrumData.publishOutput(
        outputSpectrumAnalyzer.getSpectrum());
}

//==============================================================================
bool RoomShapeAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* RoomShapeAudioProcessor::createEditor()
{
    return new RoomShapeAudioProcessorEditor (*this);
}

//==============================================================================
void RoomShapeAudioProcessor::getStateInformation(
    juce::MemoryBlock& destData)
{
    juce::ValueTree state("RoomShapeState");

    state.setProperty(
        "version",
        1,
        nullptr);

    // ============================================================
    // EQ state
    // ============================================================
    
    state.addChild(
        createEQState(),
        -1,
        nullptr);
    
    // ============================================================
    // Effect parameters
    // ============================================================
    
    state.addChild(
        parameters.copyState(),
        -1,
        nullptr);
    
    // ============================================================
    // Serialize
    // ============================================================

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void RoomShapeAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
    auto xml = getXmlFromBinary(
        data,
        sizeInBytes);

    if (xml == nullptr)
        return;

    if (!xml->hasTagName("RoomShapeState"))
        return;

    juce::ValueTree state =
        juce::ValueTree::fromXml(*xml);

    if (!state.isValid())
        return;

    // ============================================================
    // Version
    // ============================================================

    const int version =
        state.getProperty("version", 1);

    // Future migration code can go here.
    juce::ignoreUnused(version);
    
    // ============================================================
    // Effect parameters
    // ============================================================

    auto parameterState =
        state.getChildWithName(
            parameters.state.getType());

    if (parameterState.isValid())
        parameters.replaceState(parameterState);

    // ============================================================
    // EQ state
    // ============================================================

    auto eqState =
        state.getChildWithName("EQState");

    if (eqState.isValid())
        restoreEQState(eqState);
}

juce::ValueTree RoomShapeAudioProcessor::createEQState() const
{
    juce::ValueTree state("EQState");

    for (std::size_t i = 0;
         i < EQState::NumBands;
         ++i)
    {
        state.setProperty(
            "gain_" + juce::String(i),
            eqState.gains[i],
            nullptr);
    }

    return state;
}

bool RoomShapeAudioProcessor::restoreEQState(
    const juce::ValueTree& state)
{
    if (!state.isValid() ||
        !state.hasType("EQState"))
        return false;

    EQState newState;

    for (std::size_t i = 0;
         i < EQState::NumBands;
         ++i)
    {
        newState.gains[i] =
            static_cast<float>(
                state.getProperty(
                    "gain_" + juce::String(i),
                    0.0f));
    }

    setEQState(newState);

    return true;
}

void RoomShapeAudioProcessor::reset()
{
    for (auto& rv : reverb_)
        rv.reset();
}

// EQ DSP-related functions
void RoomShapeAudioProcessor::setGain(std::size_t band, float gainDb){
    for (auto& rv : reverb_)
        rv.setEQGain(band, gainDb);
}
void RoomShapeAudioProcessor::setGains(const GraphicEQ::Gains& gains){
    for (std::size_t i = 0;
         i < EQState::NumBands;
         ++i)
    {
        eqState.gains[i] = gains[i];
    }
    for (auto& rv : reverb_)
        rv.setEQGains(gains);
}
float RoomShapeAudioProcessor::getGain(std::size_t band) const{
    if (band >= GraphicEQ::NumBands)
        return 0.0f;

    return reverb_[0].getEQGain(band);
}

GraphicEQ::Gains RoomShapeAudioProcessor::getGains() const
{
    GraphicEQ::Gains gains{};

    for (std::size_t i = 0;
         i < GraphicEQ::NumBands;
         ++i)
    {
        gains[i] = eqState.gains[i];
    }

    return gains;
}

EQState RoomShapeAudioProcessor::getEQState() const
{
    return eqState;
}

void RoomShapeAudioProcessor::setEQState(
    const EQState& state)
{
    eqState = state;

    for (auto& rv : reverb_)
        rv.setEQGains(eqState.gains);
    
    // communicate to editor
    sendChangeMessage();
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RoomShapeAudioProcessor();
}
