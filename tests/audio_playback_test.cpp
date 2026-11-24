#include "UARTReceiver.cpp"
#include "Audio.cpp"
#include "TestDemoAudio.cpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <cstring>
#include <string>
#include <limits>

// The host playback fixture isolates the effects it exercises from the shape and particle renderers.
void Audio::animateCylinder() {}
void Audio::animateOctahedron() {}
void Audio::animateParticleGeqClusters() {}
void Audio::animateParticleGeqTiles() {}
void Audio::animateParticleGeqPairedBands() {}

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
static bool checkBouncingBalls = false;
static bool checkBouncingBallsPairs = false;
static std::vector<unsigned> ballLaunches;
static unsigned airborneSections = 0;
static bool ballHeightVaries = false;
static bool sawFirstLift = false;
static bool sawFirstLanding = false;
static bool sawFirstRebound = false;
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
    if (checkBouncingBalls) {
        assert(voxelsThisFrame == RNDR_X * RNDR_Y);
        int heights[8];
        std::fill(heights, heights + 8, -1);
        unsigned activePerTile[16] = {};
        unsigned inFlight = 0;
        for (int y = 0; y < RNDR_Y; ++y) {
            for (int x = 0; x < RNDR_X; ++x) {
                int height = -1;
                bool moving = false;
                for (int z = 0; z < RNDR_Z; ++z) {
                    const auto color = getVoxel(x, y, z);
                    if (color.r || color.g || color.b) {
                        assert(height == -1);
                        height = z;
                        moving = color.r > 36; // Host palette stub is grayscale.
                    }
                }
                assert(height >= 0);
                const int column = ((x + 1) * 4 - 1) / RNDR_X;
                const int section = y < RNDR_Y / 2 ? column : 7 - column;
                ballHeightVaries |= heights[section] >= 0 && heights[section] != height;
                heights[section] = height;
                if (moving) {
                    if (checkBouncingBallsPairs) {
                        const int row = ((y + 1) * 4 - 1) / RNDR_Y;
                        const int tile = row * 4 + column;
                        inFlight |= 1u << tile;
                        ++activePerTile[tile];
                    } else {
                        inFlight |= 1u << section;
                    }
                }
                if (x == 0 && y == 0 && clockMs < 3000) {
                    sawFirstRebound |= sawFirstLanding && height > 0;
                    sawFirstLanding |= sawFirstLift && height == 0;
                    sawFirstLift |= height > 0;
                }
            }
        }
        const unsigned launched = inFlight & ~airborneSections;
        if (checkBouncingBallsPairs) {
            for (int tile = 0; tile < 16; ++tile) {
                if (!(launched & (1u << tile))) continue;
                const int column = tile % 4;
                const int row = tile / 4;
                const int width = (column + 1) * RNDR_X / 4 - column * RNDR_X / 4;
                const int depth = (row + 1) * RNDR_Y / 4 - row * RNDR_Y / 4;
                assert(activePerTile[tile] == unsigned(width * depth));
            }
            if (launched) ballLaunches.push_back(launched);
        } else {
            for (int section = 0; section < 8; ++section) {
                if (launched & (1u << section)) ballLaunches.push_back(section);
            }
        }
        airborneSections = inFlight;
    }
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
        // CubeSystem's same-task app exit at the frame checkpoint.
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
        Audio::controls().peakAttackSensitivity,
        Audio::controls().maxCubeEdge, Audio::controls().puddlePeakDensity);
}

static void testanimatePuddlePeakVertical() {
    Audio::puddles(true, Audio::PuddleShape::Vertical,
        Audio::controls().peakAttackSensitivity,
        Audio::controls().maxCubeEdge, Audio::controls().puddlePeakDensity);
}

static void testanimatePuddlePeakCubes() {
    Audio::puddles(true, Audio::PuddleShape::Cube,
        Audio::controls().peakAttackSensitivity,
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
    // These full-scale values are the fixture's original (pre-NEW) tuning.
    Audio::controls().cornerBurstGain = 255.0f / 75.0f;
    Audio::controls().surroundGeqGain = 3.0f;
    Audio::controls().frequencyPlanesLevelForFullArea = 85;
    Audio::controls().frequencyShapesLevelForFullSize = 85;
    Audio::controls().plasmoidIntensity = 128;
    Audio::controls().driftRoseVolumeForFullReach = 75;
    Audio::controls().bouncingBallsVolumeForFullHeight = 75;
    Audio::controls().shapeVolumeForFullCube = 75;
    Audio::controls().juliaSensitivity = 1.5f;
    Audio::controls().juliaVolumeGamma = 1.0f;
    Audio::controls().puddlesSensitivity = 1.0f;
    Audio::controls().peakAttackSensitivity = 1.0f;
    Audio::controls().puddlePeakDensity = 1.0f;
    Audio::controls().bpmSensitivity = 1.0f;
    Audio::controls().attackSensitivity = 1.0f;
    Audio::controls().maxCubeEdge = 4;
    const unsigned expectedLength = 1 + 160 * (RNDR_X - 1) / 255;
    // Explicit expectations for the two supported canvases. Same fractional
    // footprint needs fewer lines on 10x10x16 than on the full 16^3 cube.
    const unsigned defaultLines = RNDR_X == 16 ? 6 : 4;

    // Admit one beat so every lit frame must contain exactly one wireframe.
    // A fractional layer crossfade used to light additional parallel strands.
    makeStream(145);
    for (unsigned i = 0; i < input.size(); ++i) {
        input[i].values.tempoBpm100 = 12000;
        input[i].values.tempoFlags = CubinaWire::AUDIO_ACTIVE | CubinaWire::TEMPO_LOCKED;
        input[i].values.tempoPhase = (i * 16 % 500) * 65536 / 500;
        input[i].values.volumeSm = i < 55 ? 3 : 0;
    }
    render([] { Audio::freqwave(Audio::FreqwaveShape::WireframeBox); });
    for (uint8_t brightness : brightnessFrames) assert(brightness == 0 || brightness == 255);
    std::vector<unsigned> wireframeSizes = {0};
    for (unsigned side = 2; side <= RNDR_X; side += 2) wireframeSizes.push_back(12 * side - 16);
    wireframeSizes.push_back(0);
    assert(std::vector<unsigned>(writeCounts.begin(), std::unique(writeCounts.begin(), writeCounts.end())) ==
           wireframeSizes);

    // Exercise a full clockwise lap and wrap using the real shared beat clock.
    makeStream(295);
    for (unsigned i = 0; i < input.size(); ++i) {
        input[i].values.tempoBpm100 = 12000;
        input[i].values.tempoFlags = CubinaWire::AUDIO_ACTIVE | CubinaWire::TEMPO_LOCKED;
        input[i].values.tempoPhase = (i * 16 % 500) * 65536 / 500;
    }
    checkBouncingBalls = true;
    variedPlacement = true;
    render([] { Audio::bouncingBalls(Audio::BouncingBallsPattern::Clockwise); });
    assert(ballLaunches == std::vector<unsigned>({0, 1, 2, 3, 4, 5, 6, 7, 0}));
    assert(ballHeightVaries && sawFirstRebound);
    checkBouncingBallsPairs = true;
    ballLaunches.clear();
    airborneSections = 0;
    render([] { Audio::bouncingBalls(Audio::BouncingBallsPattern::OrbitInward); });
    assert(ballLaunches.size() == 9 && ballLaunches.front() == ballLaunches.back());
    unsigned visited = 0;
    for (unsigned beat = 0; beat < 8; ++beat) {
        const unsigned pair = ballLaunches[beat];
        const unsigned second = pair & (pair - 1);
        assert(second && !(second & (second - 1))); // Exactly two tiles.
        assert(!(visited & pair));
        visited |= pair;
    }
    assert(visited == 0xFFFF);
    checkBouncingBallsPairs = false;
    variedPlacement = false;
    checkBouncingBalls = false;

    // Compare the same wave phase at three volumes, then verify that the
    // mirrored mapping matches corresponding front and back pixels.
    makeStream(1);
    input[0].values.volumeSm = 1;
    render(&Audio::animatePlasmoidMirrored);
    // Silence keeps a sparse, faint copy of the pattern alive.
    const unsigned idlePlasmoid = writeCounts[0];
    assert(idlePlasmoid > 0);
    for (uint8_t z = 0; z < RNDR_Z; ++z) {
        for (uint8_t y = 0; y < RNDR_Y; ++y) {
            for (uint8_t x = 0; x < RNDR_X; ++x) {
                const CRGB idle = getVoxel(x, y, z);
                assert(max(idle.r, max(idle.g, idle.b)) <= 70);
            }
        }
    }
    input[0].values.volumeSm = 25;
    render(&Audio::animatePlasmoidMirrored);
    const unsigned quietPlasmoid = writeCounts[0];
    assert(quietPlasmoid >= idlePlasmoid);
    input[0].values.volumeSm = 128;
    render(&Audio::animatePlasmoidMirrored);
    assert(writeCounts[0] > quietPlasmoid);
    for (uint8_t z = 0; z < RNDR_Z; ++z) {
        for (uint8_t y = 0; y < RNDR_Y / 2; ++y) {
            for (uint8_t x = 0; x < RNDR_X; ++x) {
                assert(getVoxel(x, y, z).r == getVoxel(x, RNDR_Y - 1 - y, z).r);
            }
        }
    }

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
        const unsigned ordinaryLength = 1 + 128 * (RNDR_X - 1) / 255;
        const float normalizedBurst = float(firstCount) * ordinaryLength / expectedLength;
        assert(normalizedBurst > ordinaryCoverage * 0.8f);
        assert(normalizedBurst < ordinaryCoverage * 1.2f);
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

    // One spectrum sample moves down the four side faces at the next row tick.
    makeStream(5);
    for (auto& sample : input) {
        for (auto& band : sample.values.fftResult) band = 0;
    }
    input[0].values.fftResult[1] = 28; // The second bin drives the first pair red.
    input[4].values.fftResult[0] = 11; // Top of the indigo bucket (OLD thresholds).
    input[4].values.fftResult[2] = 12; // Start of the blue bucket.
    render(&Audio::animateFunkyPlank);
    const auto isRed = [](CRGB c) { return c.r == 255 && c.g == 0 && c.b == 0; };
    assert(isRed(getVoxel(0, 0, RNDR_Z - 2)));
    assert(isRed(getVoxel(RNDR_X - 1, RNDR_Y - 1, RNDR_Z - 2)));
    assert(getVoxel(RNDR_X - 1, 0, RNDR_Z - 2).r == 0);
    const auto indigo = getVoxel(0, 0, RNDR_Z - 1);
    assert(indigo.r == 0x10 && indigo.g == 0 && indigo.b == 0x60);
    const auto blue = getVoxel((RNDR_X + 7) / 8, 0, RNDR_Z - 1);
    assert(blue.r == 0 && blue.g == 0x30 && blue.b == 255);
    assert(getVoxel(1, 1, RNDR_Z - 2).r == 0);
    // NEW (corrected) frames use the rebalanced thresholds: 28 is indigo.
    for (auto &sample : input) sample.values.tempoFlags |= CubinaWire::SPECTRUM_CORRECTED;
    render(&Audio::animateFunkyPlank);
    const auto corrected = getVoxel(0, 0, RNDR_Z - 2);
    assert(corrected.r == 0x10 && corrected.g == 0 && corrected.b == 0x60);

    // One mirrored pair at partial area, a second pair at full area, and
    // a high bin at the raw noise floor that stays dark after compensation.
    makeStream(1);
    for (auto &bin : input[0].values.fftResult) bin = 0;
    input[0].values.fftResult[1] = 21;
    input[0].values.fftResult[3] = 85;
    input[0].values.fftResult[15] = 6;
    render(&Audio::animateFrequencyPlanes);
    const int middle = RNDR_Z / 2;
    for (int y = 0; y < RNDR_Y; ++y) {
        for (int x = 0; x < RNDR_X; ++x) {
            const CRGB bass = getVoxel(x, y, middle);
            assert(bass.r == getVoxel(x, y, middle - 1).r);
            assert(bass.g == 0 && bass.b == 0);
            for (int z : {middle + 1, middle - 2}) {
                const CRGB second = getVoxel(x, y, z);
                assert(second.r == 255 && second.g == 128 && second.b == 0);
            }
            // Silent bands keep only a dim center dot on their layers. (The
            // host CRGB::White stub is black, so check band 6's violet layer.)
            const bool center = x >= (RNDR_X - 1) / 2 && x <= RNDR_X / 2 &&
                                y >= (RNDR_Y - 1) / 2 && y <= RNDR_Y / 2;
            for (int z : {RNDR_Z - 1, 0}) {
                if (!center) assert(getVoxel(x, y, z).r == 0);
            }
            for (int z : {RNDR_Z - 2, 1}) {
                const CRGB violet = getVoxel(x, y, z);
                if (center) assert(violet.b > 0 && violet.b <= 48);
                else assert(violet.b == 0);
            }
        }
    }
    assert(getVoxel(0, 0, middle).r == 0);
    assert(getVoxel(RNDR_X / 2, RNDR_Y / 2, middle).r == 255);

    render(+[] { Audio::frequencyPlanes(true); });
    assert(getVoxel(0, 0, middle + 1).r == 0);
    assert(getVoxel(RNDR_X / 2, RNDR_Y / 2, middle + 1).r == 255);
    assert(getVoxel(RNDR_X / 2, RNDR_Y / 2, middle - 2).r == 255);

    for (const auto drawPlanes : {&Audio::animateFrequencyPlanes, +[] { Audio::frequencyPlanes(true); }}) {
        makeStream(12);
        for (auto &sample : input) {
            for (auto &bin : sample.values.fftResult) bin = 0;
        }
        input[0].values.fftResult[0] = 85;
        render(drawPlanes);
        assert(writeCounts[0] > writeCounts[1]);
        assert(writeCounts[1] > 0);
        // Fully decayed: only the silence dots remain, two layers per band.
        const unsigned dots = unsigned(RNDR_X / 2 - (RNDR_X - 1) / 2 + 1) *
                              unsigned(RNDR_Y / 2 - (RNDR_Y - 1) / 2 + 1) * 2 * 8;
        assert(writeCounts.back() == dots);
    }

    // Silence leaves one fully lit anchor in each tile. An active band
    // feathers its outer layer and reaches full coverage at the size limit.
    const int anchorX = RNDR_X / 2 - 1;
    const int anchorY = RNDR_Y / 2 - 1;
    const int anchorZ = middle - 1;
    const int cubeEdge = min(RNDR_X / 2, min(RNDR_Y / 2, middle));
    for (int level : {0, 7, 75, 85}) {
        makeStream(1);
        for (auto &bin : input[0].values.fftResult) bin = 0;
        input[0].values.fftResult[0] = level;
        render(&Audio::frequencyCubes);
        for (int z : {anchorZ, middle})
            for (int y : {anchorY, RNDR_Y / 2})
                for (int x : {anchorX, RNDR_X / 2}) {
                    const CRGB anchor = getVoxel(x, y, z);
                    assert(anchor.r || anchor.g || anchor.b);
                }
        assert(getVoxel(anchorX, anchorY, anchorZ).r == 255);
        if (level == 0) assert(writeCounts[0] == 8);
        if (level == 7) {
            const auto face = getVoxel(anchorX - 1, anchorY, anchorZ).r;
            const auto edge = getVoxel(anchorX - 1, anchorY - 1, anchorZ).r;
            assert(face > edge && edge > 0 && face < 255);
        }
        if (level >= 75) {
            const auto tip = getVoxel(anchorX - cubeEdge + 1, anchorY, anchorZ).r;
            if (level == 75) assert(tip > 0 && tip < 255);
            else assert(tip == 255 && writeCounts[0] == unsigned(cubeEdge * cubeEdge * cubeEdge + 7));
        }
    }

    // Separate bass, volume and frequency inputs on both canvases.
    makeStream(1);
    input[0].values.volumeSm = 0;
    for (auto &band : input[0].values.fftResult) band = 0;
    render(&Audio::animateSurroundGEQ);
    assert(getVoxel(0, 0, 0).r == 255);
    assert(getVoxel(0, 0, 1).r == 0);
    const auto idleSphere = getVoxel(int(RNDR_CX), int(RNDR_CY), int(RNDR_CZ)).r;
    assert(idleSphere > 0);
    input[0].values.volumeSm = 255;
    render(&Audio::animateSurroundGEQ);
    assert(getVoxel(0, 0, 2).r == 192 && getVoxel(0, 0, 3).r == 255);
    assert(getVoxel(0, 0, 4).r == 0);
    assert(getVoxel(int(RNDR_CX), int(RNDR_CY), int(RNDR_CZ)).r == idleSphere);
    input[0].values.fftResult[4] = 255;
    input[0].values.fftResult[0] = 255;
    render(&Audio::animateSurroundGEQ);
    assert(getVoxel(0, 0, RNDR_Z - 1).r == 255);
    assert(getVoxel(RNDR_X - 1, RNDR_Y - 1, RNDR_Z - 1).r == 255);
    assert(getVoxel(RNDR_X - 1, 0, RNDR_Z - 1).r == 0);
    const int cx = int(RNDR_CX), cy = int(RNDR_CY), cz = int(RNDR_CZ);
    assert(getVoxel(cx, cy, cz).r > 0);

    // An isolated bass hit lights the sphere promptly and leaves a dim
    // sphere on the next frame. After the recession interval it returns to idle.
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
    assert(getVoxel(cx, cy, cz).r == idleSphere);

    // A short peak leaves a same-color marker that falls after the bar returns to idle.
    makeStream(16);
    for (auto &sample : input) {
        sample.values.volumeSm = 0;
        for (auto &band : sample.values.fftResult) band = 0;
    }
    input[0].values.fftResult[4] = 255;
    render(&Audio::animateSurroundGEQ);
    assert(getVoxel(0, 0, 0).r == 255);
    assert(getVoxel(RNDR_X - 1, 0, RNDR_Z - 1).r == 0);
    assert(getVoxel(0, 0, RNDR_Z - 2).r == 255);
    assert(getVoxel(0, 0, RNDR_Z - 2).g == 0);

    // The curve lifts real mids and cuts the measured idle floor. Toggling it
    // changes the bars while preserving the bass response.
    RemoteInputs::AudioFrame quietSpectrum{};
    const uint8_t idleBins[16] = {5, 4, 3, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3};
    std::copy(idleBins, idleBins + 16, quietSpectrum.fftResult);
    uint8_t displayBins[16];
    assert(Audio::displaySpectrum(quietSpectrum, displayBins));
    assert(std::equal(idleBins, idleBins + 4, displayBins));
    assert(std::all_of(displayBins + 4, displayBins + 16, [](uint8_t level) { return level == 0; }));
    makeStream(1);
    input[0].values.volumeSm = 0;
    for (auto &band : input[0].values.fftResult) band = 0;
    input[0].values.fftResult[0] = 40;
    input[0].values.fftResult[8] = 15;
    const int blueBarX = (4 * RNDR_X + 7) / 8;
    render(&Audio::animateSurroundGEQ);
    assert(getVoxel(blueBarX, 0, 4).b > 0);
    const auto compensatedSphere = getVoxel(cx, cy, cz);
    Audio::controls().spectrumCompensationEnabled = false;
    assert(!Audio::displaySpectrum(quietSpectrum, displayBins));
    assert(std::equal(idleBins, idleBins + 16, displayBins));
    render(&Audio::animateSurroundGEQ);
    assert(getVoxel(blueBarX, 0, 4).b == 0);
    const auto rawSphere = getVoxel(cx, cy, cz);
    assert(compensatedSphere.r == rawSphere.r && compensatedSphere.g == rawSphere.g &&
           compensatedSphere.b == rawSphere.b);
    Audio::controls().spectrumCompensationEnabled = true;
    // When the command board already restored the treble, the boost is skipped
    // even with compensation enabled, so it is never applied twice.
    RemoteInputs::AudioFrame correctedSpectrum = quietSpectrum;
    correctedSpectrum.spectrumCorrected = true;
    assert(!Audio::displaySpectrum(correctedSpectrum, displayBins));
    assert(std::equal(idleBins, idleBins + 16, displayBins));

    // The filled-bar audition repeats the same band at the same position in
    // every block. Silence retains the one-row floor of the normal GEQ.
    makeStream(1);
    input[0].values.volumeSm = 0;
    for (auto &band : input[0].values.fftResult) band = 0;
    render(&Audio::animateInwardGEQ);
    for (int x = 0; x < RNDR_X; ++x) {
        for (int y = 0; y < RNDR_Y; ++y) {
            assert(getVoxel(x, y, 0).r > 0 && getVoxel(x, y, 1).r == 0);
        }
    }
    constexpr int testBand = 6;
    input[0].values.fftResult[testBand] = 80;
    render(&Audio::animateInwardGEQ);
    bool expectedColumn[RNDR_X][RNDR_Y] = {};
    for (int block = 0; block < 16; ++block) {
        const int tile = AudioLayouts::orbitInwardTiles[testBand];
        const int x = lroundf(AudioLayouts::inwardCoordinate(block % 4, tile % 4, RNDR_X));
        const int y = lroundf(AudioLayouts::inwardCoordinate(block / 4, tile / 4, RNDR_Y));
        expectedColumn[x][y] = true;
    }
    for (int x = 0; x < RNDR_X; ++x) {
        for (int y = 0; y < RNDR_Y; ++y) {
            assert((getVoxel(x, y, 2).r > 0) == expectedColumn[x][y]);
        }
    }

    // Both bass bins would clip at the standard bar gain. With bass alone,
    // the stronger one reaches half height and the weaker one stays shorter.
    input[0].values.fftResult[testBand] = 0;
    input[0].values.fftResult[0] = 200;
    input[0].values.fftResult[1] = 100;
    render(&Audio::animateInwardGEQ);
    const int halfHeightPeak = RNDR_Z / 2;
    assert(maxZ == unsigned(halfHeightPeak));
    // These two positions are distinct on the 16-wide cube. On the narrower
    // cuboid, adjacent columns can share a voxel.
    const auto inwardBarTop = [](int block, int band) {
        const int tile = AudioLayouts::orbitInwardTiles[band];
        const int x = lroundf(AudioLayouts::inwardCoordinate(block % 4, tile % 4, RNDR_X));
        const int y = lroundf(AudioLayouts::inwardCoordinate(block / 4, tile / 4, RNDR_Y));
        for (int z = RNDR_Z - 1; z > 0; --z) {
            if (getVoxel(x, y, z).r > 0) return z;
        }
        return 0;
    };
    if (RNDR_X == 16 && RNDR_Y == 16) {
        assert(inwardBarTop(0, 0) == halfHeightPeak);
        assert(inwardBarTop(0, 1) == int(ceilf((RNDR_Z - 1) * 0.25f)));
        // Strong mids raise the bass ceiling and use their usual height.
        input[0].values.fftResult[testBand] = 40;
        render(&Audio::animateInwardGEQ);
        const int sharedPeak = inwardBarTop(0, testBand);
        assert(sharedPeak > halfHeightPeak);
        assert(inwardBarTop(0, 0) == sharedPeak);
    }

    // A brief dip cancels at rebound. A sustained drop releases the
    // front past the crest and it travels even after volume reaches zero.
    const auto furthestBurstVoxel = [] {
        int depth = -1;
        for (int z = 0; z < RNDR_Z; ++z) {
            for (int y = 0; y < RNDR_Y; ++y) {
                for (int x = 0; x < RNDR_X; ++x) {
                    const int offset = RNDR_Y - 1 + x - y + z;
                    if (offset > 0 && getVoxel(x, y, z).r) depth = max(depth, offset);
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
    assert(writeCounts.back() == 1);

    // Every shape starts at the clockwise bottom corner.
    for (auto draw : {&Audio::animateCornerBurst, &Audio::animateCornerBurstSphere,
                      &Audio::animateCornerBurstBox}) {
        makeStream(1);
        input[0].values.volumeSm = 80;
        render(draw);
        const auto fill = getVoxel(1, RNDR_Y - 1, 0).r;
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
    assert(firstExtents[0] == RNDR_X && firstExtents[1] == RNDR_Y);
    assert(firstCount == 2 * RNDR_X * RNDR_Y);
    makeStream(80);
    for (unsigned i = 12; i < input.size(); ++i) {
        input[i].values.volumeSm = 0;
    }
    waverlyLook = 0;
    waverlyLookSelected = true;
    render(&drawWaverly);
    assert(lightTotals.back() > 0);
    assert(getVoxel(0, 0, 1).r == 0);
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
    Audio::controls().midrangeBoostEnabled = false;
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
    // FFT fallback grows quiet instrumental passages, but bass/volume suppress
    // it. Disabling the switch leaves FFT-only changes invisible to geometry.
    auto fftStream = [] (bool rise, unsigned bass, unsigned volume) {
        makeStream(100);
        for (unsigned i = 0; i < input.size(); ++i) {
            input[i].values.volumeSm = volume;
            input[i].values.fftResult[0] = bass;
            if (rise && i >= 10 && i < 20) input[i].values.fftResult[4] = 55;
        }
    };
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        const unsigned bass = scenario == 1 ? 97 : 20;
        const unsigned volume = scenario == 2 ? 67 : 8;
        Audio::controls().midrangeBoostEnabled = scenario != 3;
        fftStream(false, bass, volume);
        render(&drawWaverly);
        const auto quietWrites = writeCounts;
        fftStream(true, bass, volume);
        render(&drawWaverly);
        if (scenario == 0) {
            bool grew = false;
            for (unsigned i = 10; i < 30; ++i) grew |= writeCounts[i] > quietWrites[i];
            assert(grew);
            for (unsigned i = 50; i < writeCounts.size(); ++i) assert(writeCounts[i] == quietWrites[i]);
        } else {
            assert(writeCounts == quietWrites);
        }
    }
    // NEW processing marks its spectrum corrected. The boost is a patch for the
    // old path only, so a corrected frame must leave FFT-only rises invisible.
    Audio::controls().midrangeBoostEnabled = true;
    auto correctedStream = [&fftStream] (bool rise) {
        fftStream(rise, 20, 8);
        for (auto &item : input) item.values.tempoFlags |= CubinaWire::SPECTRUM_CORRECTED;
    };
    correctedStream(false);
    render(&drawWaverly);
    const auto correctedQuiet = writeCounts;
    correctedStream(true);
    render(&drawWaverly);
    assert(writeCounts == correctedQuiet);
    checkWaverly = false;

    // Table entries 2..6; the Breathe trial sits right after Breathe.
    const char *names[] = {
        "Puddles", "Puddle Peak", "Breathe Sphere",
        "Trial Breathe (sound color)", "Attack Monitor"
    };
    constexpr unsigned nameCount = sizeof(names) / sizeof(names[0]);
    while (std::strcmp(TestDemo::currentAudioRoutineName(), names[0]) != 0) {
        TestDemo::requestNext(TestDemo::Deck::Audio);
    }
    for (unsigned entry = 0; entry < nameCount; ++entry) {
        if (!audioRoutines[entry + 2].enabled) {
            continue;
        }
        makeStream();
        assert(std::strcmp(TestDemo::currentAudioRoutineName(), names[entry]) == 0);
        render(&TestDemo::runAudio);
        const bool drawsNoLines = entry == 1 || entry == nameCount - 1;
        assert(drawsNoLines ? lineFrames.empty() : !lineFrames.empty());
        TestDemo::requestNext(TestDemo::Deck::Audio);
    }
    assert(std::strcmp(TestDemo::currentAudioRoutineName(), "Corner Burst") == 0);
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
