#include <SDL2/SDL.h>
#include <iostream>
#include <cmath>
#include <string>
#include <vector>
#include <memory>

#include "AudioEngine.h"
#include "UI.h"

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

static double clipDurationSeconds(const AudioClip& clip) {
    if (clip.sampleRate <= 0 || clip.channels <= 0) return 0.0;
    return static_cast<double>(clip.samples.size()) /
        (static_cast<double>(clip.sampleRate) * static_cast<double>(clip.channels));
}

int main() {
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
    UIContext ui;
    TimelineViewState timelineState;
    TimelineStyle timelineStyle;

    AudioEngine engine(SAMPLE_RATE, FRAMES_PER_BUFFER);
    engine.addTrack();
    engine.addTrack();

    std::vector<std::unique_ptr<AudioClip>> clipPool;
    auto addClipToTrack = [&](AudioClip clip, double startTime, int trackIndex = 0) {
        double duration = clipDurationSeconds(clip);
        clipPool.push_back(std::make_unique<AudioClip>(std::move(clip)));
        auto lock = engine.lock();
        std::vector<Track>& tracks = engine.getTracks();
        if (tracks.empty()) tracks.emplace_back();
        tracks[trackIndex].clips.push_back({ clipPool.back().get(), startTime, 0.0, duration });
    };

    bool loaded = false;
    std::string loadError;
    AudioClip clip1;
    if(!engine.loadAudioFile("sounds/sound1.mp3", clip1, &loadError)) {
        std::cerr << "Failed to load audio file: " << loadError << std::endl;
        addClipToTrack(makeSineClip(220.0f, 2.0f, SAMPLE_RATE), 0.0, 0);
    } else {
        addClipToTrack(std::move(clip1), 0.0, 0);
    }

    AudioClip clip2;
    if(!engine.loadAudioFile("sounds/sound2.mp3", clip2, &loadError)) {
        std::cerr << "Failed to load audio file: " << loadError << std::endl;
        addClipToTrack(makeSineClip(220.0f, 2.0f, SAMPLE_RATE), 0.0, 1);
    } else {
        addClipToTrack(std::move(clip2), 0.0, 1);
    }

    if (!engine.start()) {
        std::cerr << "Failed to start audio engine." << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool playing = true;
    engine.setPlaying(playing);

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    while (running) {
        ui.mousePressed = false;
        ui.mouseReleased = false;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    playing = !playing;
                    engine.setPlaying(playing);
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                ui.mouseX = event.motion.x;
                ui.mouseY = event.motion.y;
            } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                ui.mouseDown = true;
                ui.mousePressed = true;
                ui.mouseX = event.button.x;
                ui.mouseY = event.button.y;
            } else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
                ui.mouseDown = false;
                ui.mouseReleased = true;
                ui.mouseX = event.button.x;
                ui.mouseY = event.button.y;
            } else if (event.type == SDL_DROPFILE) {
                char* droppedFile = event.drop.file;
                if (droppedFile) {
                    AudioClip clip;
                    std::string loadError;
                    if (engine.loadAudioFile(droppedFile, clip, &loadError)) {
                        addClipToTrack(std::move(clip), engine.getTime());
                    } else {
                        std::cerr << "Failed to load dropped file: " << loadError << std::endl;
                    }
                    SDL_free(droppedFile);
                }
            }
        }

        int mx = 0;
        int my = 0;
        SDL_GetMouseState(&mx, &my);
        ui.mouseX = mx;
        ui.mouseY = my;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        drawTimeline(renderer, engine, timelineState, ui, timelineStyle);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
