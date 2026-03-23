#pragma once

#include <portaudio.h>

#include <vector>
#include <cstddef>
#include <mutex>

struct Transport {
    double currentTime;   // in seconds
    bool playing;
    double sampleRate;

    void update(double dt) {
        if (playing) currentTime += dt;
    }
};

struct AudioClip {
    std::vector<float> samples;
    int sampleRate;
    int channels;
};

struct ClipInstance {
    AudioClip* clip;
    double startTime;   // when it starts in timeline
    double offset;      // where inside the clip to start
    double length;
};

class Effect {
public:
    virtual ~Effect() = default;
    virtual float process(float input) = 0;
};

class Gain : public Effect {
public:
    explicit Gain(float g) : gain(g) {}
    float process(float input) override { return input * gain; }
    void setGain(float g) { gain = g; }

private:
    float gain = 1.0f;
};

struct Track {
    std::vector<ClipInstance> clips;

    float volume = 1.0f;
    float pan = 0.0f;

    std::vector<Effect*> effects;
};

struct PaStreamCallbackTimeInfo;
using PaStreamCallbackFlags = unsigned long;

class AudioEngine {
public:
    AudioEngine(double sampleRate = 44100.0, unsigned long framesPerBuffer = 256);
    ~AudioEngine();

    bool start();
    void stop();

    void setPlaying(bool playing);
    void setTime(double seconds);
    double getTime() const;

    double getSampleRate() const;
    unsigned long getFramesPerBuffer() const;

    Track& addTrack();
    void clearTracks();

private:
    static int paCallback(const void* input, void* output, unsigned long frameCount,
        const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData);
    int process(float* out, unsigned long frameCount);
    float readClipSample(const ClipInstance& clipInstance, double timelineTime) const;
    float sampleAtFrame(const AudioClip& clip, int frameIndex) const;

    std::vector<Track> tracks;
    Transport transport;
    double sampleRate;
    unsigned long framesPerBuffer;
    PaStream* stream = nullptr;
    bool initialized = false;
    mutable std::mutex mutex;

    
};
