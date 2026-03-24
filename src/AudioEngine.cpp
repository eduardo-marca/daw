#include "AudioEngine.h"

#include <portaudio.h>
#include <sndfile.h>

#include <algorithm>
#include <cmath>
#include <cstring>

AudioEngine::AudioEngine(double sr, unsigned long fpb)
    : sampleRate(sr), framesPerBuffer(fpb) {
    transport.currentTime = 0.0;
    transport.playing = false;
    transport.sampleRate = sampleRate;
}

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::start() {
    if (initialized) return true;

    PaError err = Pa_Initialize();
    if (err != paNoError) return false;

    err = Pa_OpenDefaultStream(
        &stream,
        0,
        2,
        paFloat32,
        sampleRate,
        framesPerBuffer,
        &AudioEngine::paCallback,
        this
    );
    if (err != paNoError) {
        Pa_Terminate();
        return false;
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        Pa_CloseStream(stream);
        Pa_Terminate();
        stream = nullptr;
        return false;
    }

    initialized = true;
    return true;
}

void AudioEngine::stop() {
    if (!initialized) return;

    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        stream = nullptr;
    }
    Pa_Terminate();
    initialized = false;
}

void AudioEngine::setPlaying(bool playing) {
    std::lock_guard<std::mutex> lock(mutex);
    transport.playing = playing;
}

void AudioEngine::setTime(double seconds) {
    std::lock_guard<std::mutex> lock(mutex);
    transport.currentTime = seconds;
}

double AudioEngine::getTime() const {
    std::lock_guard<std::mutex> lock(mutex);
    return transport.currentTime;
}

double AudioEngine::getSampleRate() const {
    return sampleRate;
}

unsigned long AudioEngine::getFramesPerBuffer() const {
    return framesPerBuffer;
}

Track& AudioEngine::addTrack() {
    std::lock_guard<std::mutex> lock(mutex);
    tracks.emplace_back();
    return tracks.back();
}

void AudioEngine::clearTracks() {
    std::lock_guard<std::mutex> lock(mutex);
    tracks.clear();
}

bool AudioEngine::loadAudioFile(const std::string& path, AudioClip& outClip, std::string* error) const {
    SF_INFO info;
    std::memset(&info, 0, sizeof(info));

    SNDFILE* file = sf_open(path.c_str(), SFM_READ, &info);
    if (!file) {
        if (error) *error = sf_strerror(nullptr);
        return false;
    }

    if (info.frames <= 0 || info.channels <= 0 || info.samplerate <= 0) {
        if (error) *error = "Invalid audio file metadata.";
        sf_close(file);
        return false;
    }

    outClip.sampleRate = info.samplerate;
    outClip.channels = info.channels;
    outClip.samples.resize(static_cast<size_t>(info.frames) * static_cast<size_t>(info.channels));

    sf_count_t framesRead = sf_readf_float(file, outClip.samples.data(), info.frames);
    if (framesRead < info.frames) {
        outClip.samples.resize(static_cast<size_t>(framesRead) * static_cast<size_t>(info.channels));
    }

    sf_close(file);
    return true;
}

int AudioEngine::paCallback(const void* /*input*/, void* output, unsigned long frameCount,
    const PaStreamCallbackTimeInfo* /*timeInfo*/, PaStreamCallbackFlags /*statusFlags*/, void* userData) {
    AudioEngine* engine = static_cast<AudioEngine*>(userData);
    return engine->process(static_cast<float*>(output), frameCount);
}

int AudioEngine::process(float* out, unsigned long frameCount) {
    const int outChannels = 2;
    std::fill(out, out + frameCount * outChannels, 0.0f);

    std::lock_guard<std::mutex> lock(mutex);

    if (!transport.playing) {
        return paContinue;
    }

    for (unsigned long i = 0; i < frameCount; ++i) {
        double t = transport.currentTime + static_cast<double>(i) / sampleRate;

        for (const Track& track : tracks) {
            float sample = 0.0f;
            for (const ClipInstance& clip : track.clips) {
                sample += readClipSample(clip, t);
            }

            for (Effect* effect : track.effects) {
                if (effect) sample = effect->process(sample);
            }

            float pan = std::clamp(track.pan, -1.0f, 1.0f);
            float leftGain = (pan <= 0.0f) ? 1.0f : 1.0f - pan;
            float rightGain = (pan >= 0.0f) ? 1.0f : 1.0f + pan;
            float finalSample = sample * track.volume;

            out[i * outChannels] += finalSample * leftGain;
            out[i * outChannels + 1] += finalSample * rightGain;
        }
    }

    transport.currentTime += static_cast<double>(frameCount) / sampleRate;
    return paContinue;
}

float AudioEngine::readClipSample(const ClipInstance& clipInstance, double timelineTime) const {
    if (!clipInstance.clip) return 0.0f;
    const AudioClip& clip = *clipInstance.clip;
    if (clip.samples.empty() || clip.sampleRate <= 0 || clip.channels <= 0) return 0.0f;

    int totalFrames = static_cast<int>(clip.samples.size() / clip.channels);
    double clipDuration = static_cast<double>(totalFrames) / clip.sampleRate;

    double clipStart = clipInstance.startTime;
    double clipOffset = std::max(0.0, clipInstance.offset);
    double clipLen = clipInstance.length > 0.0 ? clipInstance.length : (clipDuration - clipOffset);

    if (timelineTime < clipStart) return 0.0f;
    if (timelineTime >= clipStart + clipLen) return 0.0f;

    double clipTime = timelineTime - clipStart + clipOffset;
    if (clipTime < 0.0 || clipTime >= clipDuration) return 0.0f;

    double sourceIndex = clipTime * clip.sampleRate;
    int index = static_cast<int>(sourceIndex);
    double frac = sourceIndex - index;

    float s0 = sampleAtFrame(clip, index);
    float s1 = sampleAtFrame(clip, index + 1);
    return static_cast<float>((1.0 - frac) * s0 + frac * s1);
}

float AudioEngine::sampleAtFrame(const AudioClip& clip, int frameIndex) const {
    if (frameIndex < 0) return 0.0f;
    int channels = std::max(1, clip.channels);
    int base = frameIndex * channels;
    if (base >= static_cast<int>(clip.samples.size())) return 0.0f;

    float sum = 0.0f;
    for (int c = 0; c < channels; ++c) {
        int idx = base + c;
        if (idx < static_cast<int>(clip.samples.size())) {
            sum += clip.samples[idx];
        }
    }
    return sum / static_cast<float>(channels);
}
