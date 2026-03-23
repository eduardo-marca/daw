#include <SDL2/SDL.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cmath>
#include <portaudio.h>

const int width = 1280;
const int height = 720;

#define SAMPLE_RATE 44100
#define FRAMES_PER_BUFFER 256

struct SineData {
    float phase;
};

static int audioCallback(const void* input, void* output, unsigned long frameCount,
const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void* userData) {
    float* out = (float*)output;
    SineData* data = (SineData*)userData;

    for (unsigned int i = 0; i < frameCount; i++) {
        *out++ = sin(data->phase);
        data->phase += 0.05f;
        if (data->phase > 2.0f * M_PI) data->phase -= 2.0f * M_PI;
    }

    return paContinue;
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

    Pa_Initialize();

    SineData data;
    data.phase = 0;

    PaStream* stream;

    Pa_OpenDefaultStream(
        &stream,
        0,          // no input channels
        1,          // mono output
        paFloat32,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        audioCallback,
        &data
    );

    Pa_StartStream(stream);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_RenderPresent(renderer);
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();

    SDL_Quit();
    return 0;
}
