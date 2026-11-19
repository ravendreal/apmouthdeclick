#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DeClickDSP.h"
#include <array>
#include <atomic>

// One column of the scrolling waveform (≈10 ms of audio).
struct WaveBin { float peak = 0.f; float removed = 0.f; bool repaired = false; };

class APDeClickProcessor : public juce::AudioProcessor
{
public:
    APDeClickProcessor();
    ~APDeClickProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AP Mouth DeClick"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- read by the editor ----
    std::atomic<bool> hasPlayed { false };
    int popWaveBins (WaveBin* dest, int maxBins);

private:
    std::array<declick::Channel, 2> channels;
    juce::AudioParameterFloat* sensitivity = nullptr;
    juce::AudioParameterFloat* frequency = nullptr;
    juce::AudioParameterFloat* widen = nullptr;
    juce::AudioParameterBool* listen = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    juce::SmoothedValue<float> bypassMix, listenMix;

    // waveform FIFO (audio thread -> UI thread, lock-free)
    static constexpr int kFifoSize = 2048;
    juce::AbstractFifo fifo { kFifoSize };
    std::array<WaveBin, kFifoSize> fifoData;
    int binLength = 480, binPos = 0;
    WaveBin current;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (APDeClickProcessor)
};
