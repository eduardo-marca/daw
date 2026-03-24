#include "UI.h"

#include <algorithm>

static bool pointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.w && y < rect.y + rect.h;
}

static double clipDurationSeconds(const AudioClip& clip) {
    if (clip.sampleRate <= 0 || clip.channels <= 0) return 0.0;
    return static_cast<double>(clip.samples.size()) /
        (static_cast<double>(clip.sampleRate) * static_cast<double>(clip.channels));
}

void drawTimeline(SDL_Renderer* renderer, AudioEngine& engine, TimelineViewState& state,
    const UIContext& ui, const TimelineStyle& style) {
    SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    SDL_Rect background = { 0, style.top - 40, 2000, 2000 };
    SDL_RenderFillRect(renderer, &background);

    double playhead = engine.getTime();
    auto lock = engine.lock();
    std::vector<Track>& tracks = engine.getTracks();

    int hoveredTrack = -1;
    int hoveredClip = -1;
    SDL_Rect hoveredRect = { 0, 0, 0, 0 };

    for (size_t t = 0; t < tracks.size(); ++t) {
        int y = style.top + static_cast<int>(t) * (style.trackHeight + style.trackPadding);

        SDL_SetRenderDrawColor(renderer, 32, 32, 36, 255);
        SDL_Rect trackRect = { style.leftGutter, y, 1000, style.trackHeight };
        SDL_RenderFillRect(renderer, &trackRect);

        for (size_t c = 0; c < tracks[t].clips.size(); ++c) {
            const ClipInstance& clip = tracks[t].clips[c];
            double length = clip.length > 0.0 ? clip.length :
                (clip.clip ? clipDurationSeconds(*clip.clip) : 0.0);
            int x = style.leftGutter + static_cast<int>(clip.startTime * style.pixelsPerSecond);
            int w = std::max(6, static_cast<int>(length * style.pixelsPerSecond));

            SDL_Rect clipRect = { x, y + 6, w, style.trackHeight - 12 };

            bool isActive = (state.activeTrack == static_cast<int>(t) && state.activeClip == static_cast<int>(c));
            if (isActive) {
                SDL_SetRenderDrawColor(renderer, 80, 160, 220, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 70, 90, 130, 255);
            }
            SDL_RenderFillRect(renderer, &clipRect);

            if (pointInRect(ui.mouseX, ui.mouseY, clipRect)) {
                hoveredTrack = static_cast<int>(t);
                hoveredClip = static_cast<int>(c);
                hoveredRect = clipRect;
            }
        }
    }

    if (ui.mousePressed && hoveredTrack >= 0 && hoveredClip >= 0) {
        state.activeTrack = hoveredTrack;
        state.activeClip = hoveredClip;
        state.dragOffsetX = static_cast<float>(ui.mouseX - hoveredRect.x);
    } else if (ui.mouseReleased) {
        state.activeTrack = -1;
        state.activeClip = -1;
        state.dragOffsetX = 0.0f;
    }

    if (ui.mouseDown && state.activeTrack >= 0 && state.activeClip >= 0) {
        Track& track = tracks[static_cast<size_t>(state.activeTrack)];
        ClipInstance& clip = track.clips[static_cast<size_t>(state.activeClip)];
        double newStart = (static_cast<double>(ui.mouseX) - state.dragOffsetX - style.leftGutter) /
            style.pixelsPerSecond;
        clip.startTime = std::max(0.0, newStart);
    }

    int playheadX = style.leftGutter + static_cast<int>(playhead * style.pixelsPerSecond);
    SDL_SetRenderDrawColor(renderer, 220, 80, 80, 255);
    SDL_RenderDrawLine(renderer, playheadX, style.top - 10, playheadX, style.top + 600);
}
