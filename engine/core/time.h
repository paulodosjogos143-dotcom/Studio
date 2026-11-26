// ROXBOX - timing utilities
#pragma once
#include <cstdint>

namespace rb {

// high-resolution clock (microseconds)
uint64_t nowMicros();

struct fpsCounter {
    float fps = 0.f;
    float frameMs = 0.f;
    int64_t frames = 0;
    float accumMs = 0.f;
    float smoothedMs = 0.f;
    void tick(float dtMs) {
        accumMs += dtMs;
        smoothedMs = smoothedMs <= 0.f ? dtMs : smoothedMs * 0.9f + dtMs * 0.1f;
        if (accumMs >= 500.f) {
            fps = 1000.f / (accumMs / (float)frames);
            frameMs = accumMs / (float)frames;
            accumMs = 0.f; frames = 0;
        }
        ++frames;
    }
};

} // namespace rb
