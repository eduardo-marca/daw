#pragma once

#include <SDL2/SDL.h>

#include "AudioEngine.h"

struct UIContext {
    int mouseX = 0;
    int mouseY = 0;
    bool mouseDown = false;
    bool mousePressed = false;
    bool mouseReleased = false;
};

struct TimelineViewState {
    int activeTrack = -1;
    int activeClip = -1;
    float dragOffsetX = 0.0f;
};

struct TimelineStyle {
    float pixelsPerSecond = 120.0f;
    int top = 80;
    int leftGutter = 120;
    int trackHeight = 70;
    int trackPadding = 10;
};

void drawTimeline(SDL_Renderer* renderer, AudioEngine& engine, TimelineViewState& state,
    const UIContext& ui, const TimelineStyle& style);
