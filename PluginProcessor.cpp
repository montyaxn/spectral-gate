#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         ),
      FFT(10)
{
    fft_overwrap_order = 2;
    fft_order = 10;
    setupFFT();
    addParameter(fft_order_param = new juce::AudioParameterInt("fft_order",
                                                                 "FFT order",
                                                                 3,
                                                                 16,
                                                                 10));
    addParameter(fft_overwrap_order_param = new juce::AudioParameterInt("fft_overwrap_order",
                                                           "FFT overwrap",
                                                           1,
                                                           3,
                                                           1));
    addParameter(threshold = new juce::AudioParameterFloat("threshold",
                                                           "Threshold",
                                                           0.0f,
                                                           5.0f,
                                                           0.0f));
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void AudioPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                             juce::MidiBuffer &midiMessages)
{
    juce::ignoreUnused(midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    if (fft_order != *fft_order_param || fft_overwrap_order != *fft_overwrap_order_param)
    {
        fft_order = *fft_order_param;
        fft_overwrap_order = *fft_overwrap_order_param;
        setupFFT();
    }
    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < 2; ++channel)
    {
        auto *channelData = buffer.getWritePointer(channel);
        for (int i = 0; i < buffer.getNumSamples(); i++)
        {
            pushSample(channel, channelData[i]);
        }
        buffer.clear(channel, 0, buffer.getNumSamples());
        for (int ow = 0; ow < fft_overwrap; ow++)
        {
            buffer.addFrom(channel, 0, result_buffer[channel][ow].data(), buffer.getNumSamples());
            result_buffer[channel][ow].erase(result_buffer[channel][ow].begin(), result_buffer[channel][ow].begin() + buffer.getNumSamples());
        }
    }
    buffer.applyGain(1.0f / fft_overwrap);
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused(destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused(data, sizeInBytes);
}

void AudioPluginAudioProcessor::setupFFT()
{
    fft_size = 1 << fft_order;
    fft_overwrap = 1 << fft_overwrap_order;
    FFT = juce::dsp::FFT(fft_order);

    for (int channel = 0; channel < 2; channel++)
    {
        fft_audio[channel].resize(fft_overwrap);
        fft_freq[channel].resize(fft_overwrap);
        result_buffer[channel].resize(fft_overwrap);
        for (int j = 0; j < fft_overwrap; j++)
        {
            fft_audio[channel][j].clear();
            fft_audio[channel][j].reserve(fft_size);
            fft_audio[channel][j].insert(fft_audio[channel][j].end(), j * fft_size / fft_overwrap, 0.0f);
            fft_freq[channel][j].resize(fft_size);
            result_buffer[channel][j].clear();
            result_buffer[channel][j].insert(result_buffer[channel][j].end(), fft_size - j * fft_size / fft_overwrap, 0.0f);
        }
    }
    for (int i = 0; i < fft_size; i++)
    {
        hann.push_back(0.5f - 0.5 * cos(2.f * std::numbers::pi_v<float> * i / fft_size));
    }
}

void AudioPluginAudioProcessor::pushSample(int channel, float x)
{
    for (int ow = 0; ow < fft_overwrap; ow++)
    {
        fft_audio[channel][ow].push_back(std::complex(x, 0.0f));
        if (fft_audio[channel][ow].size() == fft_size)
        {
            for (int i = 0; i < fft_size; i++)
            {
                fft_audio[channel][ow][i] *= std::complex<float>(hann[i]);
            }
            FFT.perform(fft_audio[channel][ow].data(), fft_freq[channel][ow].data(), false);
            for (int i = 0; i < fft_size; i++)
            {
                if (std::abs(fft_freq[channel][ow][i]) < *threshold)
                {
                    fft_freq[channel][ow][i] = std::complex<float>(0.f, 0.f);
                }
            }
            FFT.perform(fft_freq[channel][ow].data(), fft_audio[channel][ow].data(), true);
            for (auto s : fft_audio[channel][ow])
            {
                result_buffer[channel][ow].push_back(s.real());
            }
            fft_audio[channel][ow].clear();
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
