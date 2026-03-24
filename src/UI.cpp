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

static float sampleAtFrame(const AudioClip& clip, int frameIndex) {
    if (frameIndex < 0 || clip.channels <= 0) return 0.0f;
    int base = frameIndex * clip.channels;
    if (base >= static_cast<int>(clip.samples.size())) return 0.0f;
    float sum = 0.0f;
    int channels = std::max(1, clip.channels);
    for (int c = 0; c < channels; ++c) {
        int idx = base + c;
        if (idx < static_cast<int>(clip.samples.size())) {
            sum += clip.samples[idx];
        }
    }
    return sum / static_cast<float>(channels);
}

static void drawTrack(SDL_Renderer* renderer, const SDL_Rect& rect) {
    SDL_SetRenderDrawColor(renderer, 32, 32, 36, 255);
    SDL_RenderFillRect(renderer, &rect);
}

static void drawClipWaveform(SDL_Renderer* renderer, const SDL_Rect& rect,
    const ClipInstance& clipInstance, double lengthSeconds) {
    if (!clipInstance.clip || rect.w <= 2 || rect.h <= 2) return;
    const AudioClip& clip = *clipInstance.clip;
    if (clip.samples.empty() || clip.sampleRate <= 0 || clip.channels <= 0) return;

    int totalFrames = static_cast<int>(clip.samples.size() / clip.channels);
    double clipOffset = std::max(0.0, clipInstance.offset);
    double clipDuration = clipDurationSeconds(clip);
    double drawLength = std::min(lengthSeconds, clipDuration - clipOffset);
    if (drawLength <= 0.0) return;

    int offsetFrames = static_cast<int>(clipOffset * clip.sampleRate);
    int framesToDraw = static_cast<int>(drawLength * clip.sampleRate);
    if (offsetFrames >= totalFrames || framesToDraw <= 0) return;

    int usableFrames = std::min(framesToDraw, totalFrames - offsetFrames);
    float pixelsToFrames = static_cast<float>(usableFrames) / static_cast<float>(rect.w);

    SDL_SetRenderDrawColor(renderer, 170, 200, 230, 255);
    int midY = rect.y + rect.h / 2;
    int maxHalf = rect.h / 2 - 2;

    for (int x = 0; x < rect.w; ++x) {
        int frameStart = offsetFrames + static_cast<int>(x * pixelsToFrames);
        int frameEnd = offsetFrames + static_cast<int>((x + 1) * pixelsToFrames);
        frameEnd = std::min(frameEnd, offsetFrames + usableFrames);
        if (frameEnd <= frameStart) frameEnd = frameStart + 1;

        int span = frameEnd - frameStart;
        int step = std::max(1, span / 200);
        float minV = 1.0f;
        float maxV = -1.0f;

        for (int f = frameStart; f < frameEnd; f += step) {
            float s = sampleAtFrame(clip, f);
            minV = std::min(minV, s);
            maxV = std::max(maxV, s);
        }

        int y1 = midY - static_cast<int>(maxV * maxHalf);
        int y2 = midY - static_cast<int>(minV * maxHalf);
        y1 = std::clamp(y1, rect.y + 2, rect.y + rect.h - 2);
        y2 = std::clamp(y2, rect.y + 2, rect.y + rect.h - 2);

        SDL_RenderDrawLine(renderer, rect.x + x, y1, rect.x + x, y2);
    }
}

static void drawClip(SDL_Renderer* renderer, const SDL_Rect& rect,
    const ClipInstance& clip, bool active, double lengthSeconds) {
    if (active) {
        SDL_SetRenderDrawColor(renderer, 80, 160, 220, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 70, 90, 130, 255);
    }
    SDL_RenderFillRect(renderer, &rect);

    drawClipWaveform(renderer, rect, clip, lengthSeconds);
}

struct ClipRenderData {
    SDL_Rect rect;
    ClipInstance clip;
    bool active = false;
    double lengthSeconds = 0.0;
    int trackIndex = -1;
    int clipIndex = -1;
};

void drawTimeline(SDL_Renderer* renderer, AudioEngine& engine, TimelineViewState& state,
    const UIContext& ui, const TimelineStyle& style) {
    SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
    SDL_Rect background = { 0, style.top - 40, 2000, 2000 };
    SDL_RenderFillRect(renderer, &background);

    double playhead = engine.getTime();

    int trackCount = 0;
    std::vector<ClipRenderData> clipsToDraw;
    int hoveredTrack = -1;
    int hoveredClip = -1;
    SDL_Rect hoveredRect = { 0, 0, 0, 0 };

    {
        auto lock = engine.lock();
        std::vector<Track>& tracks = engine.getTracks();
        trackCount = static_cast<int>(tracks.size());

        for (size_t t = 0; t < tracks.size(); ++t) {
            int y = style.top + static_cast<int>(t) * (style.trackHeight + style.trackPadding);

            for (size_t c = 0; c < tracks[t].clips.size(); ++c) {
                const ClipInstance& clip = tracks[t].clips[c];
                double length = clip.length > 0.0 ? clip.length :
                    (clip.clip ? clipDurationSeconds(*clip.clip) : 0.0);
                int x = style.leftGutter + static_cast<int>(clip.startTime * style.pixelsPerSecond);
                int w = std::max(6, static_cast<int>(length * style.pixelsPerSecond));

                SDL_Rect clipRect = { x, y + 6, w, style.trackHeight - 12 };

                bool isActive = (state.activeTrack == static_cast<int>(t) && state.activeClip == static_cast<int>(c));
                clipsToDraw.push_back({ clipRect, clip, isActive, length, static_cast<int>(t), static_cast<int>(c) });

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

            for (auto& renderClip : clipsToDraw) {
                if (renderClip.trackIndex == state.activeTrack && renderClip.clipIndex == state.activeClip) {
                    renderClip.rect.x = style.leftGutter + static_cast<int>(clip.startTime * style.pixelsPerSecond);
                    break;
                }
            }
        }
    }

    for (int t = 0; t < trackCount; ++t) {
        int y = style.top + t * (style.trackHeight + style.trackPadding);
        SDL_Rect trackRect = { style.leftGutter, y, 1000, style.trackHeight };
        drawTrack(renderer, trackRect);
    }

    for (const auto& renderClip : clipsToDraw) {
        drawClip(renderer, renderClip.rect, renderClip.clip, renderClip.active, renderClip.lengthSeconds);
    }

    int playheadX = style.leftGutter + static_cast<int>(playhead * style.pixelsPerSecond);
    SDL_SetRenderDrawColor(renderer, 220, 80, 80, 255);
    SDL_RenderDrawLine(renderer, playheadX, style.top - 10, playheadX, style.top + 600);
}
