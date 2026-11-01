#include "TempoTracker.h"
#include <cassert>
#include <cstdio>
#include <cmath>
#include <initializer_list>

static TempoTracker::Result rhythm(TempoTracker &tracker, float bpm, float duration,
                                   bool busy = false, bool drop = false) {
    TempoTracker::Result result;
    constexpr float dt = 256.0f / 31250.0f;
    const unsigned hops = unsigned(duration / dt);
    for (unsigned i = 0; i < hops; ++i) {
        const float time = i * dt;
        const float position = time * bpm / 60.0f;
        const unsigned beat = unsigned(position);
        const float phase = position - beat;
        const bool hit = phase < bpm * dt / 60.0f * 2.0f && (!drop || beat % 5 != 3);
        float bands[16];
        for (float &band : bands) {
            band = 0.00002f;
        }
        if (hit) {
            bands[0] = 0.02f;
            bands[1] = 0.01f;
        }
        if (busy && fmodf(position * 2.0f, 1.0f) < bpm * dt / 60.0f * 4.0f) {
            bands[12] = 0.004f;
        }
        result = tracker.process(bands, true, dt);
        assert(std::isfinite(result.bpm));
        assert(result.phase >= 0.0f && result.phase < 1.0f);
        if (!busy && !drop && time > 8.0f && result.locked) {
            float error = result.phase - phase;
            error -= std::floor(error + 0.5f);
            assert(std::fabs(error) < 0.22f);
        }
    }
    return result;
}

int main() {
    for (float bpm : {60.0f, 90.0f, 120.0f, 128.0f, 150.0f, 180.0f, 200.0f}) {
        TempoTracker tracker;
        const auto result = rhythm(tracker, bpm, 16.0f);
        std::printf("Tempo %.0f -> %.2f confidence %.2f locked %u\n", bpm, result.bpm,
                    result.confidence, unsigned(result.locked));
        assert(result.locked);
        assert(std::fabs(result.bpm - bpm) < 3.0f);
    }
    TempoTracker tracker;
    auto result = rhythm(tracker, 128.0f, 18.0f, true, true);
    std::printf("Busy/missed 128 -> %.2f confidence %.2f\n", result.bpm, result.confidence);
    assert(result.locked);
    assert(std::fabs(result.bpm - 128.0f) < 3.0f);
    float quiet[16] = {};
    for (unsigned i = 0; i < 500; ++i) {
        result = tracker.process(quiet, false, 256.0f / 31250.0f);
    }
    assert(!result.active && !result.locked && result.confidence < 0.05f);
    tracker.reset();
    for (unsigned i = 0; i < 2000; ++i) {
        for (float &band : quiet) {
            band = 0.01f;
        }
        result = tracker.process(quiet, true, 256.0f / 31250.0f);
    }
    assert(!result.locked && result.confidence < 0.05f);
    tracker.reset();
    rhythm(tracker, 120.0f, 14.0f);
    result = rhythm(tracker, 150.0f, 20.0f);
    assert(result.locked && std::fabs(result.bpm - 150.0f) < 3.0f);
    tracker.reset();
    unsigned randomState = 12345;
    for (unsigned i = 0; i < 3000; ++i) {
        for (float &band : quiet) {
            randomState = randomState * 1664525u + 1013904223u;
            band = 0.0001f + (randomState >> 16) * 0.00000005f;
        }
        result = tracker.process(quiet, true, 256.0f / 31250.0f);
    }
    assert(!result.locked);
    tracker.reset();
    result = rhythm(tracker, 120.0f, 66.0f);
    assert(result.locked && std::fabs(result.bpm - 120.0f) < 3.0f);
    puts("Tempo tests passed: synthetic rates, extra subdivisions, missing hits, silence and steady sound.");
}
