#include "UARTReceiver.cpp"
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
static uint32_t session = 0;
static bool variedPlacement = false;
static uint32_t placementState = 1;
static bool drawn[RNDR_X * RNDR_Y * RNDR_Z] = {};
static unsigned uniqueThisFrame = 0;
static unsigned firstUniqueCount = 0;
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
// Generated wire inputs; the REAL receiver, adapter and clock run below.
struct Input {
    AudioPayload values = {};
    uint32_t beatCount = 0;
    bool valid = true;
};
static std::vector<Input> input;
static void receiveFrame();
static std::vector<unsigned> lineFrames;
static std::vector<unsigned> writeCounts;
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
    if (!variedPlacement) {
        return 0;
    }
    placementState = placementState * 1664525u + 1013904223u;
    return long((placementState >> 8) % uint32_t(upper));
}

void setVoxel(uint8_t x, uint8_t y, uint8_t z, const CRGB &color) {
    assert(x < RNDR_X);
    assert(y < RNDR_Y);
    assert(z < RNDR_Z);
    const unsigned index = x + RNDR_X * (y + RNDR_Y * z);
    if (!drawn[index]) {
        drawn[index] = true;
        ++uniqueThisFrame;
    }
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

void clearAll() {
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
            firstUniqueCount = uniqueThisFrame;
        }
        lineFrames.push_back(frame);
    }
    writeCounts.push_back(voxelsThisFrame);
    voxelsThisFrame = 0;
    uniqueThisFrame = 0;
    std::memset(drawn, 0, sizeof(drawn));
    brightnessFrames.push_back(frameBrightness);
    frameBrightness = 0;

    if (++frame >= input.size()) {
        // Host-only escape from the animation loop. Production still uses
        // CubeSystem's existing same-task checkpoint/longjmp mechanism.
        throw StopTest();
    }

    clockMs += 16;
    receiveFrame();
}

void CubeSystem::frameCheckpoint() {
}

static void receiveFrame() {
    const Input &source = input.at(frame);
    CubinaWire::Packet packet;
    packet.session = session;
    packet.sequence = frame + 1;
    packet.modeRevision = 1;
    packet.mode = CubinaWire::Mode::AudioBluetooth;
    packet.flags = source.valid ? CubinaWire::SOURCE_READY : 0;
    packet.capturedMs = 1000 + frame * 16; // Deliberately unrelated board clock.
    CubinaWire::writeAudio(packet, source.values, source.beatCount);
    accept(packet);
    RemoteInputs::beginFrame();
}

static void render(void (*routine)()) {
    clockMs = initialClock;
    frame = 0;
    ++session;
    placementState = 1;
    firstUniqueCount = 0;
    uniqueThisFrame = 0;
    std::memset(drawn, 0, sizeof(drawn));
    litVoxels = 0;
    lineFrames.clear();
    writeCounts.clear();
    brightnessFrames.clear();
    frameBrightness = 0;
    firstCount = 0;
    voxelsThisFrame = 0;

    RemoteInputs::begin();
    receiveFrame();

    try {
        routine();
    } catch (const StopTest &) {
        return;
    }

    assert(false && "Audio routine unexpectedly returned");
}

static void makeStream(unsigned count = 80) {
    input.assign(count, Input());
    initialClock = 0;
    for (auto &sample : input) {
        sample.values.mode = 2;
        sample.values.volumeSm = 128;
        for (auto &band : sample.values.fftResult) {
            band = 20;
        }
    }
}

static void addAttack(unsigned at, uint16_t bands, uint8_t strength = 128) {
    for (unsigned i = at; i < input.size(); ++i) {
        ++input[i].values.attackCount;
    }
    input[at].values.attackStrength = strength;
    input[at].values.attackBands = bands;
}

int main() {
    // Artist-tuned defaults in Audio.h must not change this test fixture.
    Audio::controls().puddlesSensitivity = 1.0f;
    Audio::controls().puddlePeakSensitivity = 1.0f;
    Audio::controls().puddlePeakDensity = 1.0f;
    Audio::controls().bpmSensitivity = 1.0f;
    Audio::controls().attackSensitivity = 1.0f;
    Audio::controls().maxCubeEdge = 4;
    const unsigned expectedLength = 1 + 128 * (RNDR_X - 1) / 255;
    // Explicit expectations for the two supported canvases. Same fractional
    // footprint needs fewer lines on 10x10x16 than on the full 16^3 cube.
    const unsigned defaultLines = RNDR_X == 16 ? 6 : 4;

    // Every frequency group uses the same public event interface. The visual
    // must not contain another detector that examines compressed display bins.
    for (unsigned band = 0; band < 16; ++band) {
        makeStream();
        addAttack(10, uint16_t(1u << band));
        render(&Audio::animatePuddlePeak);
        assert(lineFrames == std::vector<unsigned>({10}));
        assert(litVoxels == defaultLines * expectedLength);
    }
    makeStream();
    addAttack(10, 0xFFFF);
    addAttack(30, 0x0080);
    addAttack(50, 0x8000);
    render(&Audio::animatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10, 30, 50}));

    // A dedicated kick merges with the general bass event into ONE spawn.
    makeStream();
    for (unsigned i = 10; i < input.size(); ++i) {
        input[i].beatCount = 1;
    }
    input[10].values.beat = 1;
    addAttack(10, 0x0001);
    render(&Audio::animatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10}));

    // Loud or fluctuating display bins alone must not invent shared events.
    makeStream();
    for (unsigned i = 0; i < input.size(); ++i) {
        for (auto &band : input[i].values.fftResult) {
            band = i % 2 ? 230 : 20;
        }
    }
    render(&Audio::animatePuddlePeak);
    assert(lineFrames.empty());

    makeStream();
    addAttack(10, 0x0080, 20);
    render(&Audio::animatePuddlePeak);
    assert(lineFrames.empty());
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 2.0f); });
    assert(lineFrames == std::vector<unsigned>({10}));
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 0.0f); });
    assert(lineFrames.empty());

    // Maximum volume fills one axis or a bounded solid cube, as before.
    makeStream(15);
    for (auto &sample : input) {
        sample.values.volumeSm = 255;
    }
    render(&Audio::animatePuddlesVertical);
    assert(firstExtents[0] == 1 && firstExtents[1] == 1 && firstExtents[2] == RNDR_Z);
    render(&Audio::animatePuddlesCubes);
    assert(firstCount == 64);
    render(+[] { Audio::puddles(false, Audio::PuddleShape::Cube, 1.0f, 200); });
    assert(firstCount == 125);
    render(+[] { Audio::puddles(false, Audio::PuddleShape::Cube, 0.0f); });
    assert(lineFrames.empty());

    makeStream();
    addAttack(10, 0x8000);
    render(&Audio::animatePuddlePeakVertical);
    assert(lineFrames == std::vector<unsigned>({10}));
    assert(firstExtents[0] == 1 && firstExtents[1] == 1 && firstExtents[2] > 1);
    render(&Audio::animatePuddlePeakCubes);
    assert(lineFrames == std::vector<unsigned>({10}));
    assert(firstExtents[0] == firstExtents[1] && firstExtents[1] == firstExtents[2]);

    // Calibrate the 16^3 default against the actual ordinary spawn cadence.
    // Sum the recent draw contributions with the specified exponential fade;
    // this is expected full-brightness-equivalent coverage before overlaps.
    // It is not a claim about exact physical occupancy or perceived brightness.
    if (RNDR_X == 16 && RNDR_Y == 16 && RNDR_Z == 16) {
        makeStream(200);
        render(&Audio::animatePuddles);
        const unsigned lastSpawn = lineFrames.back();
        float ordinaryCoverage = 0.0f;
        for (unsigned i = 0; i <= lastSpawn; ++i) {
            ordinaryCoverage += writeCounts[i] * expf(-float((lastSpawn - i) * 16) / 250.0f);
        }
        makeStream();
        addAttack(10, 0x0080);
        render(&Audio::animatePuddlePeak);
        assert(firstCount > ordinaryCoverage * 0.8f);
        assert(firstCount < ordinaryCoverage * 1.2f);
    }

    // Density changes BURST coverage, not event timing or shape length.
    makeStream();
    addAttack(10, 0x0080);
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 1.0f, 4, 0.5f); });
    assert(firstCount == defaultLines * expectedLength / 2);
    assert(lineFrames == std::vector<unsigned>({10}));
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 1.0f, 4, 2.0f); });
    assert(firstCount == defaultLines * expectedLength * 2);
    assert(lineFrames == std::vector<unsigned>({10}));
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 1.0f, 4, 0.0f); });
    assert(lineFrames.empty());
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 1.0f, 4, -5.0f); });
    assert(lineFrames.empty());

    // Random placement actually spreads the burst: check distinct voxels,
    // not only repeated writes to a single line in a deterministic stub.
    variedPlacement = true;
    render(&Audio::animatePuddlePeak);
    assert(firstUniqueCount >= 3 * expectedLength);
    assert(firstCount == defaultLines * expectedLength);
    variedPlacement = false;

    // Volume still controls EACH shape's size: density must not erase it.
    for (auto &sample : input) {
        sample.values.volumeSm = 255;
    }
    render(&Audio::animatePuddlePeak);
    const unsigned fullLines = RNDR_X == 16 ? 6 : 4;
    assert(firstCount == fullLines * RNDR_X);
    assert(firstExtents[0] == RNDR_X);

    // Extreme UI values cannot turn one event into unbounded work.
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 1.0f, 4, 1.0e30f); });
    assert(firstCount <= 32 * RNDR_X && firstCount <= 2048);
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Cube, 1.0f, 5, 1.0e30f); });
    assert(firstCount == 2000); // Sixteen 5^3 cubes fit the 2048-write budget.

    // Increasing density cannot admit a weak attack or invent a missing one.
    makeStream();
    addAttack(10, 0x0080, 20);
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 1.0f, 4, 100.0f); });
    assert(lineFrames.empty());
    makeStream();
    render(+[] { Audio::puddles(true, Audio::PuddleShape::Horizontal, 1.0f, 4, 100.0f); });
    assert(lineFrames.empty());

    // Idle keeps the full five-second dim breath; locked music drives it.
    makeStream(400);
    for (auto &sample : input) {
        sample.values.volumeSm = 0;
    }
    render(&Audio::animateBpmSphere);
    assert(*std::min_element(brightnessFrames.begin(), brightnessFrames.end()) <= 33);
    assert(*std::max_element(brightnessFrames.begin(), brightnessFrames.end()) >= 79);
    makeStream(160);
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

    makeStream();
    addAttack(10, 0x0080, 180);
    render(&Audio::animateAttackMonitor);
    assert(lineFrames == std::vector<unsigned>({10, 11, 12, 13, 14}));
    assert(firstCount == 64);
    makeStream();
    initialClock = 0xFFFFFFC0u;
    addAttack(10, 0x0001);
    addAttack(30, 0x8000);
    render(&Audio::animatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10, 30}));

    const char *names[] = {
        "Puddles Horizontal", "Puddles Vertical", "Puddles Cubes",
        "Puddle Peak Horizontal", "Puddle Peak Vertical", "Puddle Peak Cubes",
        "BPM Sphere", "BPM Checkerboard", "BPM Diagonal", "BPM Helix", "Attack Monitor"
    };
    for (unsigned entry = 0; entry < 11; ++entry) {
        makeStream();
        assert(std::strcmp(Audio::currentRoutineName(), names[entry]) == 0);
        render(&Audio::run);
        assert(((entry >= 3 && entry <= 5) || entry == 10) ? lineFrames.empty() : !lineFrames.empty());
        Audio::nextRoutine();
    }
    assert(std::strcmp(Audio::currentRoutineName(), names[0]) == 0);
    puts("Audio playback passed: real receiver/frame adapter, shared events, sensitivity, geometry, canvas-scaled density/placement/work bounds, playlist wrapping, musical/idle/stale BPM.");
}
