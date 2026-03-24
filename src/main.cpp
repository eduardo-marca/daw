#include <SDL2/SDL.h>
#include <iostream>
#include <cmath>
#include <string>

#include "AudioEngine.h"

const int width = 1280;
const int height = 720;

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 256

static AudioClip makeSineClip(float frequency, float seconds, int sampleRate) {
    AudioClip clip;
    clip.sampleRate = sampleRate;
    clip.channels = 1;
    int totalFrames = static_cast<int>(seconds * sampleRate);
    clip.samples.resize(static_cast<size_t>(totalFrames));

    double phase = 0.0;
    double phaseInc = 2.0 * M_PI * frequency / sampleRate;
    for (int i = 0; i < totalFrames; ++i) {
        clip.samples[static_cast<size_t>(i)] = static_cast<float>(std::sin(phase));
        phase += phaseInc;
        if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
    }

    return clip;
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "DAW",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width, height,
        0
    );

    if (!window) {
        std::cerr << "Window could not be created! SDL_GetError: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);

    bool running = true;
    SDL_Event event;

    AudioEngine engine(SAMPLE_RATE, FRAMES_PER_BUFFER);

    AudioClip tone;
    std::string loadError;
    bool loaded = false;
    if (argc > 1) {
        loaded = engine.loadAudioFile(argv[1], tone, &loadError);
        if (!loaded) {
            std::cerr << "Failed to load audio file: " << loadError << std::endl;
        }
    }
    if (!loaded) {
        tone = makeSineClip(220.0f, 2.0f, SAMPLE_RATE);
    }

    double toneDuration = 0.0;
    if (tone.sampleRate > 0 && tone.channels > 0) {
        toneDuration = static_cast<double>(tone.samples.size()) /
            (static_cast<double>(tone.sampleRate) * static_cast<double>(tone.channels));
    }
    Track& track = engine.addTrack();
    track.volume = 1.0f;
    track.pan = 0.0f;
    track.clips.push_back({ &tone, 0.0, 0.0, toneDuration });

    if (!engine.start()) {
        std::cerr << "Failed to start audio engine." << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool playing = true;
    engine.setPlaying(playing);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    playing = !playing;
                    engine.setPlaying(playing);
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
