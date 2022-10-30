/*
 ==============================================================================
 
 This file contains the basic framework code for a JUCE plugin editor.
 
 ==============================================================================
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

void LookAndFeel::drawRotarySlider(juce::Graphics & g, int x, int y, int width, int height, float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, juce::Slider & slider)
{
    auto bounds = juce::Rectangle<float>(x, y, width, height);
    g.setColour(juce::Colour(97u, 18u, 167u));
    g.fillEllipse(bounds);
    g.setColour(juce::Colour(255u, 154u, 1u));
    g.drawEllipse(bounds, 1.f);
    
    if (auto* rotarySlideWithLabel = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto centre = bounds.getCentre();
        juce::Path p;
        juce::Rectangle<float> r;
        r.setLeft(centre.getX() - 2);
        r.setRight(centre.getX() + 2);
        r.setTop(bounds.getY());
        r.setBottom(centre.getY() - rotarySlideWithLabel->getTextHeight() * 1.5);
        p.addRoundedRectangle(r, 2.f);
        
        jassert(rotaryStartAngle < rotaryEndAngle);
        
        auto sliderAngleRadians = juce::jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);
        p.applyTransform(juce::AffineTransform().rotated(sliderAngleRadians, centre.getX(), centre.getY()));
        g.fillPath(p);
        
        g.setFont(rotarySlideWithLabel->getTextHeight());
        auto text = rotarySlideWithLabel->getDisplayString();
        auto stringWidth = g.getCurrentFont().getStringWidth(text);
        
        r.setSize(stringWidth + 4, rotarySlideWithLabel->getTextHeight() + 2);
        r.setCentre(bounds.getCentre());
        g.setColour(juce::Colours::white);
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
}

void RotarySliderWithLabels::paint(juce::Graphics &g)
{
    auto startAngle = juce::degreesToRadians(180.f + 45.f);
    auto endAngle = juce::degreesToRadians(180.f - 45.f) + juce::MathConstants<float>::twoPi;
    auto range = getRange();
    auto sliderBounds = getSliderBounds();
    getLookAndFeel().drawRotarySlider(g,
                                      sliderBounds.getX(),
                                      sliderBounds.getY(),
                                      sliderBounds.getWidth(),
                                      sliderBounds.getHeight(),
                                      juce::jmap(getValue(), range.getStart(),
                                                 range.getEnd(), 0.0, 1.0),
                                      startAngle,
                                      endAngle,
                                      *this);
    
    auto center = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;
    g.setColour(juce::Colour(0u, 172u, 1u));
    g.setFont(getTextHeight());
    
    auto numberOfChoices = labels.size();
    for (int i = 0; i < numberOfChoices; ++i)
    {
        auto position = labels[i].position;
        jassert(0.f <= position);
        jassert(position <= 1.f);
        auto angle = juce::jmap(position, 0.f, 1.f, startAngle, endAngle);
        auto labelCenter = center.getPointOnCircumference(radius + getTextHeight() + 0.5f + 1, angle);
        juce::Rectangle<float> labelRectangle;
        auto labelString = labels[i].label;
        labelRectangle.setSize(g.getCurrentFont().getStringWidth(labelString), getTextHeight());
        labelRectangle.setCentre(labelCenter);
        labelRectangle.setY(labelRectangle.getY() + getTextHeight());
        g.drawFittedText(labelString, labelRectangle.toNearestInt(), juce::Justification::centred, 1);
    }
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    size -= getTextHeight() * 2;
    juce::Rectangle<int> rectangle;
    rectangle.setSize(size, size);
    rectangle.setCentre(bounds.getCentreX(), 0);
    rectangle.setY(2);
    
    return rectangle;
}

juce::String RotarySliderWithLabels::getDisplayString() const {
    if (auto* choiceParameter = dynamic_cast<juce::AudioParameterChoice*>(param))
    {
        return choiceParameter->getCurrentChoiceName();
    }
    
    juce::String string;
    bool addK = false;
    if (auto* floatParameter = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float value = getValue();
        if (value > 999.f)
        {
            value /= 1000.f;
            addK = true;
        }
        
        string = juce::String(value, (addK ? 2 : 0));
    }
    else
    {
        jassertfalse;
    }
    
    if (suffix.isNotEmpty())
    {
        string << " ";
        if (addK)
        {
            string << "k";
        }
        string << suffix;
    }
    
    return string;
}

ResponseCurveComponent::ResponseCurveComponent(SimpleEQAudioProcessor& p) : audioProcessor(p)
{
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
    {
        param->addListener(this);
    }
    
    updateChain();
    startTimerHz(60);
}
ResponseCurveComponent::~ResponseCurveComponent()
{
    const auto& params = audioProcessor.getParameters();
    for (auto param : params)
    {
        param->removeListener(this);
    }
}

void ResponseCurveComponent::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::black);
    
    auto responseArea = getLocalBounds();
    auto width = responseArea.getWidth();
    auto& lowCut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highCut = monoChain.get<ChainPositions::HighCut>();
    
    auto sampleRate = audioProcessor.getSampleRate();
    std::vector<double> magnitudes;
    magnitudes.resize(width);
    for (int i = 0; i < width; ++i) {
        double magnitude = 1.f;
        auto frequency = juce::mapToLog10(double(i) / double(width), 20.0, 20000.0);
        
        if (!monoChain.isBypassed<ChainPositions::Peak>()) {
            magnitude *= peak.coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }
        if (!lowCut.isBypassed<0>()) {
            magnitude *= lowCut.get<0>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }
        if (!lowCut.isBypassed<1>()) {
            magnitude *= lowCut.get<1>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }
        if (!lowCut.isBypassed<2>()) {
            magnitude *= lowCut.get<2>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }
        if (!lowCut.isBypassed<3>()) {
            magnitude *= lowCut.get<3>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }
        if (!highCut.isBypassed<0>()) {
            magnitude *= highCut.get<0>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }
        if (!highCut.isBypassed<1>()) {
            magnitude *= highCut.get<1>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }
        if (!highCut.isBypassed<2>()) {
            magnitude *= highCut.get<2>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }
        if (!highCut.isBypassed<3>()) {
            magnitude *= highCut.get<3>().coefficients->getMagnitudeForFrequency(frequency, sampleRate);
        }
        
        magnitudes[i] = juce::Decibels::gainToDecibels(magnitude);
    }
    
    juce::Path responseCurve;
    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    auto map = [outputMin, outputMax](double input)
    {
        return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
    };
    
    responseCurve.startNewSubPath(responseArea.getX(), map(magnitudes.front()));
    for (size_t i = 1; i < magnitudes.size(); ++i)
    {
        responseCurve.lineTo(responseArea.getX() + i, map(magnitudes[i]));
    }
    g.setColour(juce::Colours::orange);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);
    g.setColour(juce::Colours::white);
    g.strokePath(responseCurve, juce::PathStrokeType(2.f));
}

void ResponseCurveComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void ResponseCurveComponent::timerCallback()
{
    if (parametersChanged.compareAndSetBool(false, true))
    {
        updateChain();
        // signal a repaint
        repaint();
    }
}

void ResponseCurveComponent::updateChain()
{
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    auto peakCoefficients = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());
    updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.highCutSlope);
}

SimpleEQAudioProcessorEditor::SimpleEQAudioProcessorEditor (SimpleEQAudioProcessor& p)
: AudioProcessorEditor (&p),
audioProcessor (p),
peakFreqSlider(*audioProcessor.apvts.getParameter("Peak Freq"), "Hz"),
peakGainSlider(*audioProcessor.apvts.getParameter("Peak Gain"), "dB"),
peakQualitySlider(*audioProcessor.apvts.getParameter("Peak Quality"), ""),
lowCutFreqSlider(*audioProcessor.apvts.getParameter("LowCut Freq"), "Hz"),
lowCutSlopeSlider(*audioProcessor.apvts.getParameter("LowCut Slope"), "dB/Oct"),
highCutFreqSlider(*audioProcessor.apvts.getParameter("HighCut Freq"), "Hz"),
highCutSlopeSlider(*audioProcessor.apvts.getParameter("HighCut Slope"), "dB/Oct"),
responseCurveComponent(audioProcessor),
peakFreqSliderAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
peakGainSliderAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
peakQualitySliderAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
lowCutFreqSliderAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
lowCutSlopeSliderAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
highCutFreqSliderAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
highCutSlopeSliderAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    
    peakFreqSlider.labels.add({0.f, "20Hz"});
    peakFreqSlider.labels.add({1.f, "20kHz"});
    peakGainSlider.labels.add({0.f, "-24dB"});
    peakGainSlider.labels.add({1.f, "24dB"});
    peakQualitySlider.labels.add({0.f, "0.1"});
    peakQualitySlider.labels.add({1.f, "10"});
    lowCutFreqSlider.labels.add({0.f, "20Hz"});
    lowCutFreqSlider.labels.add({1.f, "20kHz"});
    lowCutSlopeSlider.labels.add({0.f, "12dB/Oct"});
    lowCutSlopeSlider.labels.add({1.f, "48dB/Oct"});
    highCutFreqSlider.labels.add({0.f, "20Hz"});
    highCutFreqSlider.labels.add({1.f, "20kHz"});
    highCutSlopeSlider.labels.add({0.f, "12dB/Oct"});
    highCutSlopeSlider.labels.add({1.f, "48dB/Oct"});
    
    for (auto* component : getComponents())
    {
        addAndMakeVisible(component);
    }
    
    setSize (600, 480);
}

SimpleEQAudioProcessorEditor::~SimpleEQAudioProcessorEditor()
{
    
}

void SimpleEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::black);
}

void SimpleEQAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    auto bounds = getLocalBounds();
    auto heightRatio = 25.f  / 100.f; // JUCE_LIVE_CONSTANT(25) / 100.f;
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * heightRatio);
    responseCurveComponent.setBounds(responseArea);
    
    bounds.removeFromTop(16);
    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);
    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);
    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);
    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.5));
    peakQualitySlider.setBounds(bounds);
}

std::vector<juce::Component*> SimpleEQAudioProcessorEditor::getComponents()
{
    return
    {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQualitySlider,
        &lowCutFreqSlider,
        &lowCutSlopeSlider,
        &highCutFreqSlider,
        &highCutSlopeSlider,
        &responseCurveComponent
    };
}
