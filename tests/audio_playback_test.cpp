#include "UARTReceiver.cpp"
#include "Audio.cpp"
#include "TestDemoAudio.cpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>
#include <string>
#include <limits>

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
static CRGB buffer[RNDR_X * RNDR_Y * RNDR_Z];
static bool checkWaverly = false;
static std::vector<unsigned> lightTotals;
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
static std::vector<std::string> routineNames;
static unsigned buttonFrame = std::numeric_limits<unsigned>::max();
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
    buffer[index] = color;
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

CRGB getVoxel(uint8_t x, uint8_t y, uint8_t z) {
    assert(x < RNDR_X && y < RNDR_Y && z < RNDR_Z);
    return buffer[x + RNDR_X * (y + RNDR_Y * z)];
}

void clearAll() {
    for (auto &color : buffer) {
        color = CRGB();
    }
}

void fadeAll(uint8_t amount) {
    for (auto &color : buffer) {
        color.nscale8(255 - amount);
    }
}

void showCube() {
    if (checkWaverly) {
        unsigned total = 0;
        for (unsigned i = 0; i < RNDR_X * RNDR_Y * RNDR_Z; ++i) {
            const auto &a = buffer[i];
            const auto &b = buffer[RNDR_X * RNDR_Y * RNDR_Z - 1 - i];
            assert(a.r == b.r && a.g == b.g && a.b == b.b);
            total += a.r + a.g + a.b;
        }
        assert(voxelsThisFrame <= 2 * RNDR_X * RNDR_Y * RNDR_Z);
        lightTotals.push_back(total);
    }
    routineNames.push_back(TestDemo::currentAudioRoutineName());
    if (frame == buttonFrame) {
        TestDemo::requestNext(TestDemo::Deck::Audio);
    }
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

void TestDemo::Detail::advanceAnimation() {
}

void TestDemo::Detail::stopAnimation() {
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
    TestDemo::stopPlayback();
    clearAll();
    lightTotals.clear();
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
    routineNames.clear();
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

static void testanimatePuddles() {
    Audio::puddles(false, Audio::PuddleShape::Horizontal,
        Audio::controls().puddlesSensitivity,
        Audio::controls().maxCubeEdge, Audio::controls().puddlePeakDensity);
}

static void testanimatePuddlesVertical() {
    Audio::puddles(false, Audio::PuddleShape::Vertical,
        Audio::controls().puddlesSensitivity,
        Audio::controls().maxCubeEdge, Audio::controls().puddlePeakDensity);
}

static void testanimatePuddlesCubes() {
    Audio::puddles(false, Audio::PuddleShape::Cube,
        Audio::controls().puddlesSensitivity,
        Audio::controls().maxCubeEdge, Audio::controls().puddlePeakDensity);
}

static void testanimatePuddlePeak() {
    Audio::puddles(true, Audio::PuddleShape::Horizontal,
        Audio::controls().puddlePeakSensitivity,
        Audio::controls().maxCubeEdge, Audio::controls().puddlePeakDensity);
}

static void testanimatePuddlePeakVertical() {
    Audio::puddles(true, Audio::PuddleShape::Vertical,
        Audio::controls().puddlePeakSensitivity,
        Audio::controls().maxCubeEdge, Audio::controls().puddlePeakDensity);
}

static void testanimatePuddlePeakCubes() {
    Audio::puddles(true, Audio::PuddleShape::Cube,
        Audio::controls().puddlePeakSensitivity,
        Audio::controls().maxCubeEdge, Audio::controls().puddlePeakDensity);
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
    TestDemo::audioPlayback().order = TestDemo::PlaybackOrder::Sequential;
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
        render(&testanimatePuddlePeak);
        assert(lineFrames == std::vector<unsigned>({10}));
        assert(litVoxels == defaultLines * expectedLength);
    }
    makeStream();
    addAttack(10, 0xFFFF);
    addAttack(30, 0x0080);
    addAttack(50, 0x8000);
    render(&testanimatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10, 30, 50}));

    // A dedicated kick merges with the general bass event into ONE spawn.
    makeStream();
    for (unsigned i = 10; i < input.size(); ++i) {
        input[i].beatCount = 1;
    }
    input[10].values.beat = 1;
    addAttack(10, 0x0001);
    render(&testanimatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10}));

    // Loud or fluctuating display bins alone must not invent shared events.
    makeStream();
    for (unsigned i = 0; i < input.size(); ++i) {
        for (auto &band : input[i].values.fftResult) {
            band = i % 2 ? 230 : 20;
        }
    }
    render(&testanimatePuddlePeak);
    assert(lineFrames.empty());

    makeStream();
    addAttack(10, 0x0080, 20);
    render(&testanimatePuddlePeak);
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
    render(&testanimatePuddlesVertical);
    assert(firstExtents[0] == 1 && firstExtents[1] == 1 && firstExtents[2] == RNDR_Z);
    render(&testanimatePuddlesCubes);
    assert(firstCount == 64);
    render(+[] { Audio::puddles(false, Audio::PuddleShape::Cube, 1.0f, 200); });
    assert(firstCount == 125);
    render(+[] { Audio::puddles(false, Audio::PuddleShape::Cube, 0.0f); });
    assert(lineFrames.empty());

    makeStream();
    addAttack(10, 0x8000);
    render(&testanimatePuddlePeakVertical);
    assert(lineFrames == std::vector<unsigned>({10}));
    assert(firstExtents[0] == 1 && firstExtents[1] == 1 && firstExtents[2] > 1);
    render(&testanimatePuddlePeakCubes);
    assert(lineFrames == std::vector<unsigned>({10}));
    assert(firstExtents[0] == firstExtents[1] && firstExtents[1] == firstExtents[2]);

    // Calibrate the 16^3 default against the actual ordinary spawn cadence.
    // Sum the recent draw contributions with the specified exponential fade;
    // this is expected full-brightness-equivalent coverage before overlaps.
    // It is not a claim about exact physical occupancy or perceived brightness.
    if (RNDR_X == 16 && RNDR_Y == 16 && RNDR_Z == 16) {
        makeStream(200);
        render(&testanimatePuddles);
        const unsigned lastSpawn = lineFrames.back();
        float ordinaryCoverage = 0.0f;
        for (unsigned i = 0; i <= lastSpawn; ++i) {
            ordinaryCoverage += writeCounts[i] * expf(-float((lastSpawn - i) * 16) / 250.0f);
        }
        makeStream();
        addAttack(10, 0x0080);
        render(&testanimatePuddlePeak);
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
    render(&testanimatePuddlePeak);
    assert(firstUniqueCount >= 3 * expectedLength);
    assert(firstCount == defaultLines * expectedLength);
    variedPlacement = false;

    // Volume still controls EACH shape's size: density must not erase it.
    for (auto &sample : input) {
        sample.values.volumeSm = 255;
    }
    render(&testanimatePuddlePeak);
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

    // The real receiver snapshot drives the shared Julia renderer. Silence
    // holds its open endpoint, full volume holds the closed endpoint; neither
    // should oscillate on its own while the color clock keeps running.
    Audio::controls().juliaSensitivity = 1.5f;
    Audio::controls().juliaVolumeGamma = 1.0f;
    makeStream(12);
    for (auto &sample : input) sample.values.volumeSm = 0;
    render(&Audio::animateJulia);
    const auto openJulia = writeCounts;
    assert(std::all_of(openJulia.begin(), openJulia.end(), [&](unsigned n) { return n == openJulia[0]; }));
    assert(juliaAudioClosure() == 0.0f);
    makeStream(12);
    for (auto &sample : input) sample.values.volumeSm = 255;
    render(&Audio::animateJulia);
    const auto closedJulia = writeCounts;
    assert(std::all_of(closedJulia.begin(), closedJulia.end(), [&](unsigned n) { return n == closedJulia[0]; }));
    assert(closedJulia[0] > openJulia[0]);
    assert(closedJulia[0] <= RNDR_X * RNDR_Y * RNDR_Z * 8 / 10);
    assert(juliaAudioClosure() == 1.0f);
    makeStream(12);
    render(&Audio::animateJulia);
    const float linearJulia = juliaAudioClosure();
    Audio::controls().juliaVolumeGamma = 2.0f;
    const float restrainedJulia = juliaAudioClosure();
    Audio::controls().juliaVolumeGamma = 0.5f;
    assert(juliaAudioClosure() > linearJulia && restrainedJulia < linearJulia);
    Audio::controls().juliaSensitivity = 0.0f;
    assert(juliaAudioClosure() == 0.0f);
    Audio::controls().juliaSensitivity = std::numeric_limits<float>::quiet_NaN();
    Audio::controls().juliaVolumeGamma = std::numeric_limits<float>::infinity();
    assert(std::isfinite(juliaAudioClosure()));
    Audio::controls().juliaSensitivity = 1.5f;
    Audio::controls().juliaVolumeGamma = 1.0f;

    // Geometry follows volume steps gradually; setting both time constants
    // to zero reproduces the direct response. Check release and clock wrap.
    makeStream(20);
    for (unsigned i = 0; i < input.size(); ++i) input[i].values.volumeSm = i ? 255 : 0;
    render(&Audio::animateJulia);
    const auto smoothedRise = writeCounts;
    assert(smoothedRise[0] == openJulia[0]);
    assert(smoothedRise[1] != closedJulia[0]);
    initialClock = 0xffffffe0u;
    render(&Audio::animateJulia);
    assert(writeCounts == smoothedRise);
    Audio::controls().juliaAttackMs = 0;
    Audio::controls().juliaReleaseMs = 0;
    render(&Audio::animateJulia);
    assert(writeCounts[1] == closedJulia[0]);
    Audio::controls().juliaAttackMs = 100;
    Audio::controls().juliaReleaseMs = 220;
    makeStream(20);
    for (unsigned i = 0; i < input.size(); ++i) input[i].values.volumeSm = i ? 0 : 255;
    render(&Audio::animateJulia);
    assert(writeCounts[0] == closedJulia[0]);
    assert(writeCounts[1] != openJulia[0]);

    // Checkerboard follows transported musical phase, not local volume speed.
    makeStream(160);
    for (unsigned i = 0; i < input.size(); ++i) {
        input[i].values.tempoBpm100 = 12000;
        input[i].values.tempoFlags = CubinaWire::AUDIO_ACTIVE | CubinaWire::TEMPO_LOCKED;
        input[i].values.tempoPhase = uint16_t(fmodf(i * 0.016f * 2.0f, 1.0f) * 65536.0f);
    }
    render(&Audio::animateBpmCheckerboard);
    const std::vector<CRGB> onBeat(buffer, buffer + RNDR_X * RNDR_Y * RNDR_Z);
    for (auto &sample : input) {
        sample.values.tempoPhase += 16384;
    }
    render(&Audio::animateBpmCheckerboard);
    assert(std::memcmp(onBeat.data(), buffer, sizeof(buffer)) != 0);

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
    render(&testanimatePuddlePeak);
    assert(lineFrames == std::vector<unsigned>({10, 30}));

    // Separate bass, volume and frequency inputs on both canvases.
    makeStream(1);
    input[0].values.volumeSm = 0;
    for (auto &band : input[0].values.fftResult) band = 0;
    render(&Audio::animateSurroundGEQ);
    assert(writeCounts[0] == 0);
    input[0].values.volumeSm = 255;
    render(&Audio::animateSurroundGEQ);
    assert(getVoxel(0, 0, 2).r == 192 && getVoxel(0, 0, 3).r == 255);
    assert(getVoxel(0, 0, 4).r == 0);
    assert(getVoxel(int(RNDR_CX), int(RNDR_CY), int(RNDR_CZ)).r == 0);
    input[0].values.fftResult[4] = 255;
    input[0].values.fftResult[0] = 255;
    render(&Audio::animateSurroundGEQ);
    assert(getVoxel(0, 0, RNDR_Z - 1).r == 255);
    assert(getVoxel(RNDR_X - 1, RNDR_Y - 1, RNDR_Z - 1).r == 255);
    assert(getVoxel(RNDR_X - 1, 0, RNDR_Z - 1).r == 0);
    const int cx = int(RNDR_CX), cy = int(RNDR_CY), cz = int(RNDR_CZ);
    assert(getVoxel(cx, cy, cz).r > 0);

    // An isolated bass hit lights the sphere promptly and leaves a dim
    // sphere on the next frame. After the recession interval it is dark.
    makeStream(1);
    input[0].values.volumeSm = 0;
    for (auto &band : input[0].values.fftResult) band = 0;
    input[0].values.fftResult[0] = 255;
    render(&Audio::animateSurroundGEQ);
    const auto bright = getVoxel(cx, cy, cz).r;
    assert(bright > 0);
    Input silent = input[0];
    silent.values.fftResult[0] = 0;
    input.push_back(silent);
    render(&Audio::animateSurroundGEQ);
    const auto faded = getVoxel(cx, cy, cz).r;
    assert(faded > 0 && faded < bright);
    input.resize(14, silent);
    render(&Audio::animateSurroundGEQ);
    assert(getVoxel(cx, cy, cz).r == 0);

    // A short peak leaves a same-color marker that falls after the bar goes dark.
    makeStream(16);
    for (auto &sample : input) {
        sample.values.volumeSm = 0;
        for (auto &band : sample.values.fftResult) band = 0;
    }
    input[0].values.fftResult[4] = 255;
    render(&Audio::animateSurroundGEQ);
    assert(getVoxel(0, 0, 0).r == 0);
    assert(getVoxel(RNDR_X - 1, 0, RNDR_Z - 1).r == 0);
    assert(getVoxel(0, 0, RNDR_Z - 2).r == 255);
    assert(getVoxel(0, 0, RNDR_Z - 2).g == 0);

    // A brief dip cancels at rebound. A sustained drop releases the
    // front past the crest and it travels even after volume reaches zero.
    const auto furthestBurstVoxel = [] {
        int depth = -1;
        for (int z = 0; z < RNDR_Z; ++z) {
            for (int y = 0; y < RNDR_Y; ++y) {
                for (int x = 0; x < RNDR_X; ++x) {
                    if (getVoxel(x, y, z).r) depth = max(depth, RNDR_Y - 1 + x - y + z);
                }
            }
        }
        return depth;
    };
    makeStream(20);
    for (unsigned i = 0; i < input.size(); ++i) {
        input[i].values.volumeSm = i == 3 || i == 4 ? 0 : 80;
    }
    render(&Audio::animateCornerBurst);
    const int crestDepth = furthestBurstVoxel();
    assert(crestDepth > 0 && crestDepth < (RNDR_X + RNDR_Y + RNDR_Z - 3) * 0.7f);
    for (unsigned i = 3; i < input.size(); ++i) input[i].values.volumeSm = 0;
    render(&Audio::animateCornerBurst);
    assert(furthestBurstVoxel() > crestDepth);
    input.resize(55, input.back());
    render(&Audio::animateCornerBurst);
    assert(furthestBurstVoxel() == -1);

    // Every shape starts at the clockwise bottom corner.
    for (auto draw : {&Audio::animateCornerBurst, &Audio::animateCornerBurstSphere,
                      &Audio::animateCornerBurstBox}) {
        makeStream(1);
        input[0].values.volumeSm = 80;
        render(draw);
        const auto fill = getVoxel(0, RNDR_Y - 1, 0).r;
        assert(fill > 0 && fill <= 39);
        assert(getVoxel(RNDR_X - 1, 0, RNDR_Z - 1).r == 0);
        assert(brightnessFrames.back() > 4 * fill);

        // VolumeSm 75 reaches the common bottom point for every front.
        input[0].values.volumeSm = 75;
        render(draw);
        const int capOffset = min(11, min(int(RNDR_X) - 1, int(RNDR_Y) - 1));
        assert(getVoxel(capOffset, RNDR_Y - 1 - capOffset, 0).r == 255);
    }

    // Same geometry across every named look, independent of texturing. Real
    // receiver snapshots, silence, dimensions, symmetry, cadence and work cap.
    assert(WAVERLY_LOOK_COUNT == 11);
    // Every allowed palette-driven mechanism is crossed with Rainbow and Party,
    // without duplicates. Forbidden/frozen shaders cannot enter this table.
    unsigned coverage[5][5] = {};
    for (unsigned i = 0; i < WAVERLY_LOOK_COUNT; ++i) {
        const auto &look = waverlyLooks[i];
        if (look.shader == MODE_HIPHOTIC) {
            assert(i == 10 && !look.columnGradient);
            continue;
        }
        assert(look.palette == 0 || look.palette == 4);
        unsigned group = 0;
        if (!look.columnGradient) {
            if (look.shader == MODE_LINEAR_FLOW) group = 1;
            else if (look.shader == MODE_NOISE_FIELD) group = 2;
            else if (look.shader == MODE_DISTANCE_FIELD) group = 3;
            else if (look.shader == MODE_DISTORTION_WAVES) group = 4;
            else assert(false && "Excluded shader in audition");
        }
        ++coverage[group][look.palette];
    }
    for (const auto &group : coverage) {
        for (unsigned palette = 0; palette < 5; ++palette) {
            assert(group[palette] == unsigned(palette == 0 || palette == 4));
        }
    }
    checkWaverly = true;
    std::vector<unsigned> referenceWrites;
    for (unsigned i = 0; i < WAVERLY_LOOK_COUNT; ++i) {
        makeStream(40);
        waverlyLook = i;
        waverlyLookSelected = true;
        render(&drawWaverly);
        assert(!lineFrames.empty());
        if (i == 0) {
            referenceWrites = writeCounts;
        } else {
            assert(writeCounts == referenceWrites);
        }
    }
    while (std::strncmp(TestDemo::currentAudioRoutineName(), "Waverly ", 8) != 0) {
        TestDemo::requestNext(TestDemo::Deck::Audio);
    }
    // Global duration=0 holds an audition beyond its normal 15-second slot.
    waverlyLook = 0;
    waverlyLookSelected = true;
    TestDemo::audioPlayback().durationMs = 0;
    makeStream(1000);
    render(&TestDemo::runAudio);
    for (const auto &name : routineNames) {
        assert(name == "Waverly Gradient Cycle Rainbow");
    }
    assert(waverlyLook == 0);
    TestDemo::audioPlayback().durationMs = 30000;

    // At modest volume, neighboring depth slices must have distinct lower
    // profiles: the new spatial dimension cannot be a repeated 2D curtain.
    makeStream(1);
    for (auto &sample : input) {
        sample.values.volumeSm = 64;
    }
    waverlyLook = 0;
    waverlyLookSelected = true;
    render(&drawWaverly);
    bool differentDepth = false;
    for (unsigned y = 1; y < RNDR_Y; ++y) {
        for (unsigned x = 0; x < RNDR_X; ++x) {
            for (unsigned z = 0; z < RNDR_Z / 2; ++z) {
                if (getVoxel(x, y, z).r != getVoxel(x, 0, z).r) {
                    differentDepth = true;
                }
            }
        }
    }
    assert(differentDepth);
    makeStream(40);
    for (auto &sample : input) {
        sample.values.volumeSm = 0;
    }
    waverlyLook = 0;
    waverlyLookSelected = true;
    render(&drawWaverly);
    assert(lineFrames.empty());
    assert(*std::max_element(lightTotals.begin(), lightTotals.end()) == 0);
    makeStream(80);
    for (unsigned i = 12; i < input.size(); ++i) {
        input[i].values.volumeSm = 0;
    }
    waverlyLook = 0;
    waverlyLookSelected = true;
    render(&drawWaverly);
    assert(lightTotals.back() == 0);
    assert(*std::max_element(lightTotals.begin(), lightTotals.end()) > 0);
    makeStream(12);
    initialClock = 0xFFFFFFC0u;
    waverlyLook = 0;
    waverlyLookSelected = true;
    render(&drawWaverly);
    assert(!lineFrames.empty());
    // Duration is also usable by direct calls, independently of the deck.
    makeStream(80);
    render(+[]() {
        Audio::waverly(MODE_LINEAR_FLOW, RainbowColors_p, true, 64);
        throw StopTest();
    });
    assert(frame == 4);
    // Same volume/noise trajectory: only accepted non-bass events add reach.
    makeStream(80);
    waverlyLook = 0;
    waverlyLookSelected = true;
    render(&drawWaverly);
    const auto baseline = writeCounts;
    const uint16_t regions[] = {0x0001, 0x0080, 0x8000};
    for (uint16_t region : regions) {
        makeStream(80);
        addAttack(10, region, 180);
        render(&drawWaverly);
        if (region == 0x0001) {
            assert(writeCounts == baseline);
        } else {
            bool grew = false;
            for (unsigned i = 10; i < 28; ++i) {
                grew |= writeCounts[i] > baseline[i];
            }
            assert(grew);
            for (unsigned i = 32; i < writeCounts.size(); ++i) {
                assert(writeCounts[i] == baseline[i]);
            }
        }
    }
    // Amount zero restores the original geometry even with mid/high attacks.
    Audio::controls().waverlyAttackReach = 0.0f;
    makeStream(80);
    addAttack(10, 0xFF80, 180);
    render(&drawWaverly);
    assert(writeCounts == baseline);
    Audio::controls().waverlyAttackReach = 0.15f;
    checkWaverly = false;

    const char *names[] = {
        "Puddles", "Puddle Peak", "Breathe Sphere", "Attack Monitor"
    };
    while (std::strcmp(TestDemo::currentAudioRoutineName(), names[0]) != 0) {
        TestDemo::requestNext(TestDemo::Deck::Audio);
    }
    for (unsigned entry = 0; entry < 4; ++entry) {
        if (!audioRoutines[entry + 2].enabled) {
            continue;
        }
        makeStream();
        assert(std::strcmp(TestDemo::currentAudioRoutineName(), names[entry]) == 0);
        render(&TestDemo::runAudio);
        assert((entry == 1 || entry == 3) ? lineFrames.empty() : !lineFrames.empty());
        TestDemo::requestNext(TestDemo::Deck::Audio);
    }
    assert(std::strcmp(TestDemo::currentAudioRoutineName(), "Surround GEQ") == 0);
    while (std::strcmp(TestDemo::currentAudioRoutineName(), names[0]) != 0) {
        TestDemo::requestNext(TestDemo::Deck::Audio);
    }
    // Actual effects return to the actual TestDemo player on expiry. This
    // crosses the real player/receiver/effect boundary, rather than testing
    // only a fake timer callback or a separate selection helper.
    makeStream(32);
    TestDemo::audioPlayback().durationMs = 64;
    render(&TestDemo::runAudio);
    assert(routineNames[0] == "Puddles");
    assert(routineNames[3] == "Puddles");
    assert(routineNames[4] == "Puddle Peak");
    assert(routineNames[8] == "Breathe Sphere");
    TestDemo::stopPlayback();
    while (std::strcmp(TestDemo::currentAudioRoutineName(), names[0]) != 0) {
        TestDemo::requestNext(TestDemo::Deck::Audio);
    }

    // A press at the deadline is still ONE step. Then hold mode preserves
    // the next real effect indefinitely without restarting it every frame.
    makeStream(16);
    buttonFrame = 3;
    render(&TestDemo::runAudio);
    assert(routineNames[4] == "Puddle Peak");
    TestDemo::stopPlayback();
    while (std::strcmp(TestDemo::currentAudioRoutineName(), names[0]) != 0) {
        TestDemo::requestNext(TestDemo::Deck::Audio);
    }
    TestDemo::audioPlayback().durationMs = 0;
    buttonFrame = 1;
    render(&TestDemo::runAudio);
    assert(routineNames[0] == "Puddles");
    assert(routineNames[1] == "Puddles");
    for (unsigned i = 2; i < routineNames.size(); ++i) {
        assert(routineNames[i] == "Puddle Peak");
    }
    buttonFrame = std::numeric_limits<unsigned>::max();

    puts("Audio playback passed: real receiver/frame adapter, shared events, sensitivity, geometry, canvas-scaled density/placement/work bounds, playlist wrapping, musical/idle/stale BPM.");
}
