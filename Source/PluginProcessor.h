/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

using Filter = juce::dsp::IIR::Filter<float>;
using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;
using Coefficients = Filter::CoefficientsPtr;

enum Slope
{
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};

enum ChainPositions
{
    LowCut,
    Peak,
    HighCut
};

struct ChainSettings
{
    float peakFreq{0};
    float peakGainInDecibels{0};
    float peakQuality{1.f};
    float lowCutFreq{0};
    float highCutFreq{0};
    Slope lowCutSlope{Slope::Slope_12};
    Slope highCutSlope{Slope::Slope_12};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState &apvts);
Coefficients makePeakFilter(const ChainSettings &chainSettings, double sampleRate);
void updateCoefficients(Coefficients &old, const Coefficients &replacements);

template <typename ChainType, typename CoefficientType>
void updateCutFilter(ChainType &chain,
                     CoefficientType &coefficients,
                     const Slope &slope)
{
    chain.template setBypassed<0>(true);
    chain.template setBypassed<1>(true);
    chain.template setBypassed<2>(true);
    chain.template setBypassed<3>(true);

    switch (slope)
    {
    case Slope_48:
    {
        updateCoefficients(chain.template get<3>().coefficients, coefficients[3]);
        chain.template setBypassed<3>(false);
    }
    case Slope_36:
    {
        updateCoefficients(chain.template get<2>().coefficients, coefficients[2]);
        chain.template setBypassed<2>(false);
    }
    case Slope_24:
    {
        updateCoefficients(chain.template get<1>().coefficients, coefficients[1]);
        chain.template setBypassed<1>(false);
    }
    case Slope_12:
    {
        updateCoefficients(chain.template get<0>().coefficients, coefficients[0]);
        chain.template setBypassed<0>(false);
    }
    }
}

inline auto makeLowCutFilter(const ChainSettings &chainSettings, double sampleRate)
{
    return juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq, sampleRate, 2 * (chainSettings.lowCutSlope + 1));
}

inline auto makeHighCutFilter(const ChainSettings &chainSettings, double sampleRate)
{
    return juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq, sampleRate, 2 * (chainSettings.highCutSlope + 1));
}
//==============================================================================
/**
 */
class SimpleEQAudioProcessor : public juce::AudioProcessor
#if JucePlugin_Enable_ARA
    ,
                               public juce::AudioProcessorARAExtension
#endif
{
public:
    //==============================================================================
    SimpleEQAudioProcessor();
    ~SimpleEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout
    createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{
        *this,
        nullptr,
        "Parameters",
        createParameterLayout()};

private:
    MonoChain leftChain, rightChain;
    void updatePeakFilter(const ChainSettings &chainSettings);
    void updateLowCutFilters(const ChainSettings &chainSettings);
    void updateHighCutFilters(const ChainSettings &chainSettings);
    void updateFilters();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleEQAudioProcessor)
};
