#include "PluginProcessor.h"
#include "PluginEditor.h"

APDeClickProcessor::APDeClickProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    sensitivity = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("sensitivity"));
    frequency   = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("frequency"));
    widen       = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("widen"));
    listen      = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter ("listen"));
    bypassParam = dynamic_cast<juce::AudioParameterBool*>  (apvts.getParameter ("bypass"));
}

juce::AudioProcessorValueTreeState::ParameterLayout APDeClickProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "sensitivity", 1 }, "Hassasiyet",
        NormalisableRange<float> (0.0f, 10.0f, 0.1f), 5.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));

    NormalisableRange<float> freqRange (1000.0f, 8000.0f, 10.0f);
    freqRange.setSkewForCentre (3000.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "frequency", 1 }, "Frekans",
        freqRange, 3000.0f,
        AudioParameterFloatAttributes().withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int) { return String (v / 1000.0f, 1) + " kHz"; })));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "widen", 1 }, String::fromUTF8 ("Onarımı Genişlet"),
        NormalisableRange<float> (0.0f, 5.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " ms"; })));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "listen", 1 }, "Dinle (sadece silinen)", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, String::fromUTF8 ("Devre Dışı"), false));
    return { p.begin(), p.end() };
}

bool APDeClickProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void APDeClickProcessor::prepareToPlay (double sampleRate, int)
{
    for (auto& c : channels) c.prepare (sampleRate);
    setLatencySamples (declick::Channel::lookaheadSamples (sampleRate));
    bypassMix.reset (sampleRate, 0.02); bypassMix.setCurrentAndTargetValue (bypassParam->get() ? 1.f : 0.f);
    listenMix.reset (sampleRate, 0.02); listenMix.setCurrentAndTargetValue (listen->get() ? 1.f : 0.f);
    binLength = juce::jmax (64, (int) (sampleRate * 0.01));
    binPos = 0; current = {};
}

void APDeClickProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), (int) channels.size());
    const int numSamples = buffer.getNumSamples();

    declick::Params params;
    params.sensitivity = sensitivity->get();
    params.freqHz = frequency->get();
    params.widenMs = widen->get();
    for (int c = 0; c < numCh; ++c) channels[(size_t) c].setParams (params);

    bypassMix.setTargetValue (bypassParam->get() ? 1.f : 0.f);
    listenMix.setTargetValue (listen->get() ? 1.f : 0.f);

    if (numSamples > 0) hasPlayed.store (true);

    for (int i = 0; i < numSamples; ++i)
    {
        const float bm = bypassMix.getNextValue();
        const float lm = listenMix.getNextValue();
        float peak = 0.f, removed = 0.f; bool rep = false;

        for (int c = 0; c < numCh; ++c)
        {
            float* data = buffer.getWritePointer (c);
            float dry; bool r;
            const float wet = channels[(size_t) c].process (data[i], dry, r);
            const float diff = dry - wet;
            const float processed = (1.f - lm) * wet + lm * diff;
            data[i] = bm * dry + (1.f - bm) * processed;
            peak = juce::jmax (peak, std::abs (dry));
            removed = juce::jmax (removed, std::abs (diff));
            rep = rep || r;
        }

        current.peak = juce::jmax (current.peak, peak);
        current.removed = juce::jmax (current.removed, removed);
        current.repaired = current.repaired || rep;
        if (++binPos >= binLength)
        {
            const auto scope = fifo.write (1);
            if (scope.blockSize1 > 0) fifoData[(size_t) scope.startIndex1] = current;
            else if (scope.blockSize2 > 0) fifoData[(size_t) scope.startIndex2] = current;
            current = {}; binPos = 0;
        }
    }

    for (int c = 0; c < numCh; ++c) channels[(size_t) c].takeRepairCount();   // keeps the internal counter from growing

    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, numSamples);
}

int APDeClickProcessor::popWaveBins (WaveBin* dest, int maxBins)
{
    const auto scope = fifo.read (juce::jmin (maxBins, fifo.getNumReady()));
    for (int k = 0; k < scope.blockSize1; ++k) dest[k] = fifoData[(size_t) (scope.startIndex1 + k)];
    for (int k = 0; k < scope.blockSize2; ++k) dest[scope.blockSize1 + k] = fifoData[(size_t) (scope.startIndex2 + k)];
    return scope.blockSize1 + scope.blockSize2;
}

void APDeClickProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void APDeClickProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* APDeClickProcessor::createEditor() { return new APDeClickEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new APDeClickProcessor(); }
