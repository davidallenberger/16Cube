// Render-side replay: run the REAL audio routines on the Mac against a song.
//
// Input: a per-hop CSV written by tools/audio_replay (--csv). It is turned
// into the 60 Hz packets the command board would send (latest payload per
// frame, attacks retained between sends like CommandState), then fed through
// the real UARTReceiver/AudioFrameClock into the real effects, which draw
// with the real FastLED, texture, geometry, shape and particle engines.
//
// Output: one CSV row per rendered frame with what the routine DID (how much
// of the cube is lit, brightness, motion, where the light sits) next to the
// audio it was given (volume, bands, events, tempo), plus the routine's own
// debug detail whenever it reports one. Deterministic: the replay owns the
// clock and seeds FastLED's random generator.
//
//   render_replay audio.csv "Routine Name" out.csv [--list] [--seed N] [--set CONTROL=VALUE ...]

#include "UARTReceiver.cpp"
#include "Audio.cpp"
#include "TestDemoAudio.cpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------- platform
static uint32_t clockUs = 0;
extern "C" {
uint32_t millis(void) { return clockUs / 1000; }
uint32_t micros(void) { return clockUs; }
void delay(int ms) { clockUs += uint32_t(ms) * 1000; }
void delayMicroseconds(int us) { clockUs += uint32_t(us); }
// Busy-wait loops (e.g. "until a millisecond passes") must see time move.
void yield(void) { clockUs += 1000; }
}
static uint32_t lcg = 12345;
long random(long upper) {
    if (upper <= 0) return 0;
    lcg = lcg * 1664525u + 1013904223u;
    return long((lcg >> 8) % uint32_t(upper));
}
long random(long lower, long upper) { return upper > lower ? lower + random(upper - lower) : lower; }
uint32_t esp_random() { lcg = lcg * 1664525u + 1013904223u; return lcg; }

static std::string serialLine;
static std::string lastDetail;   // Latest "Audio: ..." debug line from the routine.
static bool detailUpdated = false;
size_t ReplaySerial::write(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        const char c = char(data[i]);
        if (c == '\n' || c == '\r') {
            if (serialLine.rfind("Audio:", 0) == 0) {
                lastDetail = serialLine;
                detailUpdated = true;
            }
            serialLine.clear();
        } else {
            serialLine += c;
        }
    }
    return length;
}
int ReplaySerial::printf(const char *format, ...) {
    char text[1024];
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (n > 0) write(reinterpret_cast<const uint8_t *>(text), size_t(std::min(n, int(sizeof(text) - 1))));
    return n;
}
ReplaySerial Serial;
ReplaySerial Serial2;

// ---------------------------------------------------------------- canvas
static CRGB canvas[RNDR_X * RNDR_Y * RNDR_Z];
static CRGB previous[RNDR_X * RNDR_Y * RNDR_Z];
static inline unsigned replayIndex(unsigned x, unsigned y, unsigned z) { return x + RNDR_X * (y + RNDR_Y * z); }
void setVoxel(uint8_t x, uint8_t y, uint8_t z, const CRGB &c) {
    if (x < RNDR_X && y < RNDR_Y && z < RNDR_Z) canvas[replayIndex(x, y, z)] = c;
}
CRGB getVoxel(uint8_t x, uint8_t y, uint8_t z) {
    return (x < RNDR_X && y < RNDR_Y && z < RNDR_Z) ? canvas[replayIndex(x, y, z)] : CRGB::Black;
}
void clearAll() { for (auto &c : canvas) c = CRGB::Black; }
void fadeAll(uint8_t amount) { for (auto &c : canvas) c.nscale8(255 - amount); }
void initCube() {}
void seedAllRng() {}
void debugVoxel(uint8_t, uint8_t, uint8_t) {}
void getPerimeterCoords(uint8_t p, uint8_t &u, uint8_t &w) {
    const uint8_t side = RNDR_X - 1;
    if (p < side) { u = p; w = 0; }
    else if (p < 2 * side) { u = side; w = p - side; }
    else if (p < 3 * side) { u = side - (p - 2 * side); w = side; }
    else { u = 0; w = side - (p - 3 * side); }
}
void setVoxelByAxis(uint8_t u, uint8_t w, uint8_t h, uint8_t axis, CRGB color) {
    if (axis == 1) setVoxel(u, h, w, color);
    else if (axis == 0) setVoxel(h, u, w, color);
    else setVoxel(u, w, h, color);
}
void TestDemo::Detail::advanceAnimation() {}
void TestDemo::Detail::stopAnimation() {}

// ---------------------------------------------------------------- input stream
struct Hop {
    double t;
    AudioPayload p;
    uint32_t beats;
};
static std::vector<Hop> hops;
static bool loadHops(const char *path) {
    std::ifstream in(path);
    std::string line;
    if (!std::getline(in, line)) return false;
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string cell;
        std::vector<double> v;
        while (std::getline(ss, cell, ',')) v.push_back(atof(cell.c_str()));
        if (v.size() < 29) continue;
        Hop h = {};
        h.t = v[0];
        h.p.mode = 2;
        h.p.volumeRaw = uint8_t(v[1]);
        h.p.volumeSm = uint8_t(v[2]);
        h.p.sampleAvg = uint8_t(v[3]);
        for (int b = 0; b < 16; ++b) h.p.fftResult[b] = uint8_t(v[4 + b]);
        h.beats = uint32_t(v[20]);
        h.p.beat = uint8_t(v[21]);
        h.p.attackCount = uint32_t(v[22]);
        h.p.attackStrength = uint8_t(v[23]);
        h.p.attackBands = uint16_t(v[24]);
        h.p.tempoBpm100 = uint16_t(lround(v[25] * 100));
        h.p.tempoPhase = uint16_t(v[26]);
        h.p.tempoConfidence = uint8_t(v[27]);
        h.p.tempoFlags = uint8_t(v[28]);
        hops.push_back(h);
    }
    return !hops.empty();
}

// ---------------------------------------------------------------- frame loop
struct StopReplay {};
static unsigned frameIndex = 0;
static double songEnd = 0;                 // Seconds.
static uint64_t lastShowUs = 0;
constexpr uint32_t FRAME_US = 16667;       // showCube's 60 Hz cap on the board.
static size_t hopCursor = 0;
static uint32_t lastAttackCount = 0;
static FILE *out = nullptr;
static uint32_t session = 7;

// Latest payload at the frame time; attacks since the previous frame are
// merged (strongest strength, OR of bands), as the command board retains them.
static void sendFrame() {
    const double t = clockUs / 1e6;
    uint8_t strength = 0;
    uint16_t bands = 0;
    while (hopCursor + 1 < hops.size() && hops[hopCursor + 1].t <= t) {
        ++hopCursor;
        const Hop &h = hops[hopCursor];
        if (h.p.attackCount != lastAttackCount) {
            strength = std::max(strength, h.p.attackStrength);
            bands |= h.p.attackBands;
            lastAttackCount = h.p.attackCount;
        }
    }
    AudioPayload payload = hops[hopCursor].p;
    payload.attackStrength = strength;
    payload.attackBands = bands;
    CubinaWire::Packet packet;
    packet.session = session;
    packet.sequence = frameIndex + 1;
    packet.modeRevision = 1;
    packet.mode = CubinaWire::Mode::AudioBluetooth;
    packet.flags = CubinaWire::SOURCE_READY;
    packet.capturedMs = 5000 + clockUs / 1000;
    CubinaWire::writeAudio(packet, payload, hops[hopCursor].beats);
    accept(packet);
    RemoteInputs::beginFrame();
}

static void csvEscape(FILE *f, const std::string &s) {
    fputc('"', f);
    for (char c : s) { if (c == '"') fputc('"', f); fputc(c, f); }
    fputc('"', f);
}

void showCube() {
    // What the routine drew this frame.
    const unsigned count = RNDR_X * RNDR_Y * RNDR_Z;
    unsigned lit = 0;
    double light = 0, change = 0, zSum = 0, rSum = 0;
    for (unsigned z = 0; z < RNDR_Z; ++z)
        for (unsigned y = 0; y < RNDR_Y; ++y)
            for (unsigned x = 0; x < RNDR_X; ++x) {
                const CRGB &c = canvas[replayIndex(x, y, z)];
                const CRGB &p = previous[replayIndex(x, y, z)];
                const int peak = std::max(c.r, std::max(c.g, c.b));
                if (peak > 8) ++lit;
                light += peak;
                change += std::abs(int(c.r) - p.r) + std::abs(int(c.g) - p.g) + std::abs(int(c.b) - p.b);
                zSum += peak * z;
                const double dx = x - RNDR_CX, dy = y - RNDR_CY, dz = z - RNDR_CZ;
                rSum += peak * std::sqrt(dx * dx + dy * dy + dz * dz);
            }
    std::copy(canvas, canvas + count, previous);
    // What it was given.
    const auto audio = RemoteInputs::audio();
    unsigned low = 0, mid = 0, high = 0;
    for (int b = 0; b < 4; ++b) low += audio.fftResult[b];
    for (int b = 4; b < 11; ++b) mid += audio.fftResult[b];
    for (int b = 11; b < 16; ++b) high += audio.fftResult[b];
    fprintf(out, "%.4f,%.4f,%.2f,%.4f,%.3f,%.3f,%u,%u,%.1f,%.1f,%.1f,%d,%d,%d,%d,%.2f,%.3f,",
            clockUs / 1e6, double(lit) / count, light / (255.0 * count), change / (3 * 255.0 * count),
            light > 0 ? zSum / light / (RNDR_Z - 1) : 0.0, light > 0 ? rSum / light : 0.0,
            audio.volumeSm, audio.volumeRaw, low / 4.0, mid / 7.0, high / 5.0, audio.bassKick,
            audio.dedicatedBassKick, audio.midAttack, audio.highAttack, audio.bpm, audio.beatPulse(1));
    if (detailUpdated) {
        csvEscape(out, lastDetail);
        detailUpdated = false;
    }
    fputc('\n', out);

    ++frameIndex;
    // Like the board: at most one frame per 16.667 ms; slower routines
    // (their own delay()) simply arrive later.
    const uint64_t due = lastShowUs + FRAME_US;
    if (clockUs < due) clockUs = uint32_t(due);
    lastShowUs = clockUs;
    if (clockUs / 1e6 >= songEnd) throw StopReplay();
    sendFrame();
}

int main(int argc, char **argv) {
    if (argc >= 2 && std::string(argv[1]) == "--list") {
        for (const auto &r : audioRoutines) printf("%s%s\n", r.name, r.enabled ? "" : "  (disabled)");
        return 0;
    }
    if (argc < 4) {
        fprintf(stderr, "usage: render_replay audio.csv \"Routine Name\" out.csv [--seed N]\n       render_replay --list\n");
        return 2;
    }
    uint16_t seed = 1;
    for (int i = 4; i + 1 < argc; ++i) if (std::string(argv[i]) == "--seed") seed = uint16_t(atoi(argv[i + 1]));
    // --set NAME=VALUE overrides a render control, for gain sweeps.
    for (int i = 4; i + 1 < argc; ++i) {
        if (std::string(argv[i]) != "--set") continue;
        const std::string kv = argv[i + 1];
        const size_t eq = kv.find('=');
        const std::string key = kv.substr(0, eq);
        const double v = atof(kv.c_str() + eq + 1);
        auto &c = Audio::controls();
        if (key == "cornerBurstGain") c.cornerBurstGain = float(v);
        else if (key == "cornerBurstTrialSwell") c.cornerBurstTrialSwell = float(v);
        else if (key == "cornerBurstTrialFullSwell") c.cornerBurstTrialFullSwell = float(v);
        else if (key == "cornerBurstTrialRangeRelax") c.cornerBurstTrialRangeRelax = float(v);
        else if (key == "surroundGeqGain") c.surroundGeqGain = float(v);
        else if (key == "surroundGeqBassGain") c.surroundGeqBassGain = float(v);
        else if (key == "particleGeqGain") c.particleGeqGain = float(v);
        else if (key == "frequencyPlanesLevelForFullArea") c.frequencyPlanesLevelForFullArea = uint8_t(v);
        else if (key == "frequencyShapesLevelForFullSize") c.frequencyShapesLevelForFullSize = uint8_t(v);
        else if (key == "plasmoidIntensity") c.plasmoidIntensity = uint8_t(v);
        else if (key == "driftRoseVolumeForFullReach") c.driftRoseVolumeForFullReach = uint8_t(v);
        else if (key == "bouncingBallsVolumeForFullHeight") c.bouncingBallsVolumeForFullHeight = uint8_t(v);
        else if (key == "shapeVolumeForFullCube") c.shapeVolumeForFullCube = uint8_t(v);
        else if (key == "juliaSensitivity") c.juliaSensitivity = float(v);
        else if (key == "juliaVolumeGamma") c.juliaVolumeGamma = float(v);
        else if (key == "waverlySensitivity") c.waverlySensitivity = float(v);
        else if (key == "puddlesSensitivity") c.puddlesSensitivity = float(v);
        else if (key == "bpmSensitivity") c.bpmSensitivity = float(v);
        else { fprintf(stderr, "unknown control '%s'\n", key.c_str()); return 2; }
    }
    if (!loadHops(argv[1])) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    const AudioRoutine *routine = nullptr;
    for (const auto &r : audioRoutines) if (std::string(r.name) == argv[2]) routine = &r;
    if (!routine) { fprintf(stderr, "unknown routine '%s' (try --list)\n", argv[2]); return 1; }
    out = fopen(argv[3], "w");
    fprintf(out, "t,lit,light,change,height,radius,volumeSm,volumeRaw,low,mid,high,bassKick,kick,midAttack,"
                 "highAttack,bpm,beatPulse,detail\n");
    songEnd = hops.back().t;
    initAppMemory();  // The firmware allocates the shared 8 KB arena at boot.
    random16_set_seed(seed);
    lcg = seed;
    clearAll();
    RemoteInputs::begin();
    sendFrame();
    try {
        routine->draw();
    } catch (const StopReplay &) {
    }
    fclose(out);
    printf("%s: %u frames in %.1f s (%.1f fps)\n", routine->name, frameIndex, clockUs / 1e6,
           frameIndex / std::max(0.001, clockUs / 1e6));
    return 0;
}
