#include "Audio.cpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>

SerialStub Serial;
SerialStub Serial2;
FastLEDStub FastLED;
CRGB CRGB::Black;
CRGB CRGB::Green;
CRGB CRGB::Yellow;
CRGB CRGB::Red;
CRGB CRGB::White;
CRGB CRGB::Blue;
CRGB CRGB::Purple;

static uint32_t clockMs = 0;
static uint32_t initialClock = 0;
static unsigned frame = 0;
static unsigned litVoxels = 0;
static unsigned voxelsThisFrame = 0;
static unsigned minX = 0;
static unsigned minY = 0;
static unsigned minZ = 0;
static unsigned maxX = 0;
static unsigned maxY = 0;
static unsigned maxZ = 0;
static unsigned firstExtents[3] = {};
static unsigned firstCount = 0;
static uint8_t frameBrightness = 0;
static std::vector<uint8_t> brightnessFrames;
static std::vector<RemoteInputs::Audio> input;
static std::vector<unsigned> lineFrames;
static uint8_t scratch[RNDR_X * RNDR_Y * RNDR_Z];
uint8_t *SharedAppMemory = scratch;

struct StopTest {
};

unsigned long millis() {
    return clockMs;
}

uint32_t micros() {
    return clockMs * 1000;
}

void delay(int) {
}

long random(long upper) {
    assert(upper > 0);
    return 0;
}

void setVoxel(uint8_t x, uint8_t y, uint8_t z, const CRGB &color) {
    assert(x < RNDR_X);
    assert(y < RNDR_Y);
    assert(z < RNDR_Z);
    if (voxelsThisFrame == 0) {
        minX = maxX = x;
        minY = maxY = y;
        minZ = maxZ = z;
    }
    minX = min(minX, unsigned(x));
    minY = min(minY, unsigned(y));
    minZ = min(minZ, unsigned(z));
    maxX = max(maxX, unsigned(x));
    maxY = max(maxY, unsigned(y));
    maxZ = max(maxZ, unsigned(z));
    frameBrightness = max(frameBrightness, color.r);
    ++litVoxels;
    ++voxelsThisFrame;
}

void fadeAll(uint8_t) {
}

void showCube() {
    if (voxelsThisFrame) {
        if (lineFrames.empty()) {
            firstExtents[0] = maxX - minX + 1;
            firstExtents[1] = maxY - minY + 1;
            firstExtents[2] = maxZ - minZ + 1;
            firstCount = voxelsThisFrame;
        }
        lineFrames.push_back(frame);
    }
    voxelsThisFrame = 0;
    brightnessFrames.push_back(frameBrightness);
    frameBrightness = 0;

    if (++frame >= input.size()) {
        // Host-only escape from the animation loop. Production still uses
        // CubeSystem's existing same-task checkpoint/longjmp mechanism.
        throw StopTest();
    }

    clockMs += 16;
}

void CubeSystem::frameCheckpoint() {
}

RemoteInputs::Audio RemoteInputs::audio() {
    return input.at(frame);
}

static void render(void (*routine)()) {
    clockMs = initialClock;
    frame = 0;
    litVoxels = 0;
    lineFrames.clear();
    brightnessFrames.clear();
    frameBrightness = 0;
    firstCount = 0;
    voxelsThisFrame = 0;

    try {
        routine();
    } catch (const StopTest &) {
        return;
    }

    assert(false && "Audio routine unexpectedly returned");
}

// Keep packet time separate from render time, as on the two real boards.
// Every test starts with an established, modest background in all bands.
static void makeStream(unsigned count = 80, uint32_t capturedStart = 1000) {
    input.assign(count, RemoteInputs::Audio());
    initialClock = 0;

    for (unsigned i = 0; i < count; ++i) {
        input[i].valid = true;
        input[i].capturedMs = capturedStart + i * 16;
        input[i].receivedMs = i * 16;
        input[i].values.volumeSm = 128;
        input[i].values.beat = 1; // Must not act as this effect's trigger.

        for (unsigned band = 0; band < 16; ++band) {
            input[i].values.fftResult[band] = 20;
        }
    }
}

int main() {
    const unsigned expectedLength = 1 + 128 * (RNDR_X - 1) / 255;

    // Every band can trigger, including the lowest and highest bands. Holding
    // a new loud note for over a second must not generate recurring peaks.
    for (unsigned band = 0; band < 16; ++band) {
        makeStream();
        for (unsigned i = 10; i < input.size(); ++i) {
            input[i].values.fftResult[band] = 90;
        }
        render(&Audio::animatePuddlePeak);
        assert(lineFrames == std::vector<unsigned>({10}));
        assert(litVoxels == expectedLength);
    }

    // Simultaneous spectral increases represent one burst, not sixteen lines.
    makeStream();
    for (unsigned band = 0; band < 16; ++band) {
        input[10].values.fftResult[band] = 90;
    }
    render(&Audio::animatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10}));

    // Separated attacks re-arm and produce distinct lines. Closely staggered
    // attacks are consumed during cooldown rather than queued for later.
    makeStream();
    input[10].values.fftResult[0] = 90;
    input[11].values.fftResult[8] = 90;
    input[14].values.fftResult[15] = 90;
    input[30].values.fftResult[8] = 90;
    input[50].values.fftResult[15] = 90;
    render(&Audio::animatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10, 30, 50}));

    // A quieter high-frequency accent still qualifies over steady loud bass.
    makeStream();
    for (auto &sample : input) {
        sample.values.fftResult[0] = 230;
    }
    input[10].values.fftResult[15] = 50;
    render(&Audio::animatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10}));

    // Neither a steady sound/kick flag nor small fluctuations are onsets.
    makeStream();
    for (unsigned i = 0; i < input.size(); ++i) {
        for (unsigned band = 0; band < 16; ++band) {
            input[i].values.fftResult[band] = 20 + (i % 2) * 4;
        }
    }
    render(&Audio::animatePuddlePeak);
    assert(lineFrames.empty());
    makeStream();
    for (auto &sample : input) {
        for (auto &band : sample.values.fftResult) {
            band = 200;
        }
    }
    render(&Audio::animatePuddlePeak);
    assert(lineFrames.empty());

    // Reject near-zero chatter even when the relative change is enormous.
    makeStream();
    for (auto &sample : input) {
        sample.values.fftResult[5] = 0;
    }
    input[10].values.fftResult[5] = 11;
    render(&Audio::animatePuddlePeak);
    assert(lineFrames.empty());

    // Do not reprocess levels with a repeated publication timestamp.
    makeStream();
    for (unsigned i = 10; i < input.size(); ++i) {
        input[i].capturedMs = input[9].capturedMs;
        input[i].values.fftResult[0] = 90;
    }
    render(&Audio::animatePuddlePeak);
    assert(lineFrames.empty());

    makeStream();
    input[10].values.fftResult[0] = 90;
    for (auto &sample : input) {
        sample.values.volumeSm = 0;
    }
    render(&Audio::animatePuddlePeak);
    assert(lineFrames.empty());

    // Resumption learns a new background. The later accent still triggers.
    makeStream();
    input[10].values.fftResult[0] = 90;
    for (unsigned i = 15; i <= 20; ++i) {
        input[i].valid = false;
    }
    for (unsigned i = 21; i < input.size(); ++i) {
        input[i].values.fftResult[0] = 200;
    }
    input[50].values.fftResult[15] = 90;
    render(&Audio::animatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10, 50}));

    // A long source-timestamp gap also resets history without a stale frame.
    makeStream();
    for (unsigned i = 20; i < input.size(); ++i) {
        input[i].capturedMs += 400;
        input[i].values.fftResult[0] = 200;
    }
    input[50].values.fftResult[15] = 90;
    render(&Audio::animatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({50}));

    // Sender and render clocks can each wrap independently.
    makeStream(80, 0xfffffff0u);
    initialClock = 0xffffffc0u;
    input[10].values.fftResult[0] = 90;
    input[30].values.fftResult[15] = 90;
    render(&Audio::animatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10, 30}));

    // External sensitivity changes onset response, without changing input gain.
    makeStream();
    input[10].values.fftResult[8] = 26;
    render(&Audio::animatePuddlePeak);
    assert(lineFrames.empty());
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 2.0f); });
    assert(lineFrames == std::vector<unsigned>({10}));
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 0.0f); });
    assert(lineFrames.empty());

    // Maximum volume fills the requested line axis, or a bounded solid cube.
    makeStream(15);
    for (auto &sample : input) {
        sample.values.volumeSm = 255;
    }
    render(&Audio::animatePuddlesVertical);
    assert(firstExtents[0] == 1 && firstExtents[1] == 1 && firstExtents[2] == RNDR_Z);
    render(&Audio::animatePuddlesCubes);
    assert(firstCount == 64);
    assert(firstExtents[0] == 4 && firstExtents[1] == 4 && firstExtents[2] == 4);
    render(+[] { Audio::puddles(false, Audio::PuddleShape::Cube, 1.0f, 200); });
    assert(firstCount == 125); // Oversized requests clamp to 5 and canvas bounds.
    render(+[] { Audio::puddles(false, Audio::PuddleShape::Cube, 0.0f); });
    assert(lineFrames.empty());

    // Same geometry choices apply to peak-triggered shapes.
    makeStream();
    input[10].values.fftResult[15] = 90;
    render(&Audio::animatePuddlePeakVertical);
    assert(lineFrames == std::vector<unsigned>({10}));
    assert(firstExtents[0] == 1 && firstExtents[1] == 1 && firstExtents[2] > 1);
    render(&Audio::animatePuddlePeakCubes);
    assert(lineFrames == std::vector<unsigned>({10}));
    assert(firstExtents[0] == firstExtents[1] && firstExtents[1] == firstExtents[2]);

    // Idle BPM remains visible and breathes over a full five-second cycle.
    makeStream(400);
    render(&Audio::animateBpmSphere);
    assert(*std::min_element(brightnessFrames.begin(), brightnessFrames.end()) < 40);
    assert(*std::max_element(brightnessFrames.begin(), brightnessFrames.end()) > 72);

    makeStream(80);
    for (unsigned i = 0; i < input.size(); ++i) {
        input[i].values.tempoBpm100 = 12000;
        input[i].values.tempoPhase = uint16_t(fmodf(i * 0.016f * 2.0f, 1.0f) * 65536.0f);
        input[i].values.tempoFlags = CubinaWire::AUDIO_ACTIVE | CubinaWire::TEMPO_LOCKED;
    }
    render(&Audio::animateBpmSphere);
    assert(*std::max_element(brightnessFrames.begin(), brightnessFrames.end()) > 140);
    for (auto &sample : input) {
        sample.valid = false;
    }
    render(&Audio::animateBpmSphere);
    assert(*std::max_element(brightnessFrames.begin(), brightnessFrames.end()) <= 80);

    // Exercise all real playlist entry points and wrapping.
    const char *names[] = {
        "Puddles Horizontal", "Puddles Vertical", "Puddles Cubes",
        "Puddle Peak Horizontal", "Puddle Peak Vertical", "Puddle Peak Cubes",
        "BPM Sphere", "BPM Checkerboard", "BPM Diagonal", "BPM Helix"
    };
    for (unsigned entry = 0; entry < 10; ++entry) {
        makeStream();
        assert(std::strcmp(Audio::currentRoutineName(), names[entry]) == 0);
        render(&Audio::run);
        assert(entry >= 3 && entry <= 5 ? lineFrames.empty() : !lineFrames.empty());
        Audio::nextRoutine();
    }
    assert(std::strcmp(Audio::currentRoutineName(), names[0]) == 0);

    puts("Audio playback passed: all-band onsets, sustain rejection, sensitivity, line/cube bounds, all playlist entries/wrap, musical/idle/stale BPM rendering.");
}
