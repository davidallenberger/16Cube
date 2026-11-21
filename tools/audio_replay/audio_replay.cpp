// Offline replay of the command board's audio analysis.
//
// This compiles the UNMODIFIED production SignalAcquisition.cpp (the same way
// tests/audio_analysis_test.cpp does) against host stubs, links the real
// arduinoFFT library from the PlatformIO cache, and feeds a 31,250 Hz stereo
// float WAV through the production i2s_read -> acquireAudio() path. Every
// published AudioPayload is logged, one row per 256-sample hop.
//
// What this models, and what it does not:
//   * Spotify-style loudness normalization (integrated BS.1770 loudness to a
//     target, default -14 LUFS) stands in for "Spotify at a fixed volume".
//   * --chain-db is ONE unknown scalar: Mac volume, Bluetooth receiver output
//     and PCM1808 input level together. Calibrate it once against a hardware
//     log (InRMS) of the same song. Until then, sweep it.
//   * Samples are quantized to 24 bits in the upper bits of an int32 word,
//     with deterministic white noise at --noise-dbfs, like the ADC's floor.
//   * NOT modeled: the Bluetooth codec, the receiver's analog EQ/filtering,
//     or UART packet coalescing (every hop is logged, not every 60 Hz send).

#include "SignalAcquisition.cpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

SerialStub Serial;
SerialStub Serial2;
FastLEDStub FastLED;
CRGB CRGB::Black, CRGB::Green, CRGB::Yellow, CRGB::Red;
CRGB CRGB::White, CRGB::Blue, CRGB::Purple;

// ---- Host clock tied to the audio sample count -----------------------------
static double clockMsExact = 1000.0;
unsigned long millis() { return (unsigned long)clockMsExact; }
uint32_t micros() { return uint32_t(clockMsExact * 1000.0); }
void delay(int) {}
long map(long x, long inLow, long inHigh, long outLow, long outHigh) {
    return (x - inLow) * (outHigh - outLow) / (inHigh - inLow) + outLow;
}
const char *esp_err_to_name(esp_err_t) { return "replay"; }
esp_err_t i2s_driver_uninstall(int) { return ESP_OK; }
esp_err_t i2s_driver_install(int, const i2s_config_t *, int, void *) { return ESP_OK; }
esp_err_t i2s_set_pin(int, const i2s_pin_config_t *) { return ESP_OK; }

// ---- Audio source -----------------------------------------------------------
static std::vector<float> wavLeft, wavRight;
static size_t cursor = 0;
static double inputGain = 1.0;
// --pcm-lowpass HZ models the PCM1808 module's measured input filter (a
// first-order low-pass, ~235 Hz on the 2026-11-19 module) so replays hear
// what the real ADC hears. 0 = flat, ideal hardware.
static double lowpassPole = 0.0;
static double lowpassLeft = 0.0, lowpassRight = 0.0;
static double stepSeconds = -1.0;   // --step T:DB changes the chain gain at time T.
static double stepFactor = 1.0;
static double noiseRms = 0.0;
static uint32_t noiseState = 0x12345678u;
static size_t clippedSamples = 0;

static double noise() {
    // Sum of four uniforms: roughly Gaussian, unit variance after scaling.
    double s = 0.0;
    for (int i = 0; i < 4; ++i) {
        noiseState = noiseState * 1664525u + 1013904223u;
        s += (noiseState >> 8) / 16777216.0 - 0.5;
    }
    return s * std::sqrt(3.0);
}

static int32_t toWord(double x, double gain) {
    x = x * gain + noise() * noiseRms;
    if (x > 0.99999988) { x = 0.99999988; ++clippedSamples; }
    if (x < -1.0) { x = -1.0; ++clippedSamples; }
    // 24-bit sample in bits 31..8; low padding bits zero, like the PCM1808.
    const int32_t s24 = int32_t(std::floor(x * 8388608.0));
    return int32_t(uint32_t(s24) << 8);
}

esp_err_t i2s_read(int, void *data, size_t requested, size_t *received, int) {
    int32_t *words = static_cast<int32_t *>(data);
    const size_t frames = requested / (2 * sizeof(int32_t));
    for (size_t i = 0; i < frames; ++i) {
        const size_t n = cursor + i;
        const float l = n < wavLeft.size() ? wavLeft[n] : 0.0f;
        const float r = n < wavRight.size() ? wavRight[n] : 0.0f;
        const double gain = inputGain *
            (stepSeconds >= 0 && n >= stepSeconds * AudioAnalysis::SampleRate ? stepFactor : 1.0);
        lowpassLeft = l + lowpassPole * (lowpassLeft - l);
        lowpassRight = r + lowpassPole * (lowpassRight - r);
        words[2 * i] = toWord(lowpassLeft, gain);
        words[2 * i + 1] = toWord(lowpassRight, gain);
    }
    cursor += frames;
    clockMsExact += frames * 1000.0 / AudioAnalysis::SampleRate;
    *received = requested;
    return ESP_OK;
}

bool SourceAdapters::readSD(int32_t *, size_t) { return false; }
bool SourceAdapters::select(CubinaWire::Mode) { return true; }
void CommandState::selectMode(CubinaWire::Mode) {}
void CommandState::repeatModeDemand() {}
void CommandState::setReady(bool) {}

struct Row {
    double t;
    AudioPayload p;
    uint32_t beats;
    float agcDb;
    float kickPhase, kickConcentration, kickRise;
    float tb[16];
};
static std::vector<Row> rows;
void CommandState::publishAudio(const AudioPayload &audio, uint32_t beats) {
    rows.push_back({double(cursor) / AudioAnalysis::SampleRate, audio, beats,
                    newProcessingActive && uiAutoGain ? autoGain.gainDb() : 0.0f,
                    TempoTracker::debugKickPhase, TempoTracker::debugConcentration, debugKickRise, {}});
    std::copy(debugTempoBands, debugTempoBands + 16, rows.back().tb);
}

// ---- WAV reading (32-bit float stereo, as written by afconvert LEF32) --------
static bool readWav(const char *path) {
    FILE *f = std::fopen(path, "rb");
    if (!f) return false;
    char id[4];
    uint32_t size;
    if (std::fread(id, 1, 4, f) != 4 || std::memcmp(id, "RIFF", 4)) return false;
    std::fread(&size, 4, 1, f);
    std::fread(id, 1, 4, f);
    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    while (std::fread(id, 1, 4, f) == 4 && std::fread(&size, 4, 1, f) == 1) {
        if (!std::memcmp(id, "fmt ", 4)) {
            std::vector<uint8_t> fmt(size);
            std::fread(fmt.data(), 1, size, f);
            std::memcpy(&format, &fmt[0], 2);
            std::memcpy(&channels, &fmt[2], 2);
            std::memcpy(&rate, &fmt[4], 4);
            std::memcpy(&bits, &fmt[14], 2);
            if (format == 0xFFFE && size >= 26) std::memcpy(&format, &fmt[24], 2);
        } else if (!std::memcmp(id, "data", 4)) {
            if (format != 3 || bits != 32 || channels != 2 || rate != AudioAnalysis::SampleRate) {
                std::fprintf(stderr, "Need 2-channel float32 WAV at %u Hz (got fmt=%u ch=%u bits=%u rate=%u)\n",
                             unsigned(AudioAnalysis::SampleRate), format, channels, bits, rate);
                return false;
            }
            const size_t frames = size / 8;
            std::vector<float> buf(frames * 2);
            const size_t got = std::fread(buf.data(), 8, frames, f);
            wavLeft.resize(got);
            wavRight.resize(got);
            for (size_t i = 0; i < got; ++i) {
                wavLeft[i] = buf[2 * i];
                wavRight[i] = buf[2 * i + 1];
            }
            std::fclose(f);
            return true;
        } else {
            std::fseek(f, (size + 1) & ~1u, SEEK_CUR);
        }
    }
    std::fclose(f);
    return false;
}

// ---- ITU-R BS.1770-4 integrated loudness (K-weighting at this sample rate) ---
struct Biquad {
    double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    double run(double x) {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};
static void kWeighting(double fs, Biquad &shelf, Biquad &hp) {
    {   // High shelf: +4 dB above ~1.7 kHz (head effects).
        const double G = 3.999843853973347, fc = 1681.974450955533, Q = 0.7071752369554196;
        const double A = std::pow(10.0, G / 40.0), w = 2 * M_PI * fc / fs;
        const double c = std::cos(w), alpha = std::sin(w) / (2 * Q), sA = std::sqrt(A);
        const double a0 = (A + 1) - (A - 1) * c + 2 * sA * alpha;
        shelf.b0 = A * ((A + 1) + (A - 1) * c + 2 * sA * alpha) / a0;
        shelf.b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
        shelf.b2 = A * ((A + 1) + (A - 1) * c - 2 * sA * alpha) / a0;
        shelf.a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
        shelf.a2 = ((A + 1) - (A - 1) * c - 2 * sA * alpha) / a0;
    }
    {   // RLB high-pass near 38 Hz.
        const double fc = 38.13547087602444, Q = 0.5003270373238773;
        const double w = 2 * M_PI * fc / fs, c = std::cos(w), alpha = std::sin(w) / (2 * Q);
        const double a0 = 1 + alpha;
        hp.b0 = (1 + c) / 2 / a0;
        hp.b1 = -(1 + c) / a0;
        hp.b2 = (1 + c) / 2 / a0;
        hp.a1 = -2 * c / a0;
        hp.a2 = (1 - alpha) / a0;
    }
}
static double integratedLufs() {
    const double fs = AudioAnalysis::SampleRate;
    Biquad sl, hl, sr, hr;
    kWeighting(fs, sl, hl);
    kWeighting(fs, sr, hr);
    const size_t step = size_t(fs * 0.1);  // 100 ms
    std::vector<double> segment;           // mean-square sum per 100 ms
    double acc = 0;
    size_t inSeg = 0;
    for (size_t i = 0; i < wavLeft.size(); ++i) {
        const double l = hl.run(sl.run(wavLeft[i]));
        const double r = hr.run(sr.run(wavRight[i]));
        acc += l * l + r * r;
        if (++inSeg == step) {
            segment.push_back(acc / step);
            acc = 0;
            inSeg = 0;
        }
    }
    std::vector<double> blocks;  // 400 ms blocks, 75% overlap
    for (size_t i = 3; i < segment.size(); ++i) {
        blocks.push_back((segment[i] + segment[i - 1] + segment[i - 2] + segment[i - 3]) / 4);
    }
    auto lufs = [](double ms) { return -0.691 + 10 * std::log10(std::max(ms, 1e-20)); };
    double sum = 0;
    size_t n = 0;
    for (double b : blocks) if (lufs(b) > -70) { sum += b; ++n; }
    if (!n) return -70;
    const double relative = lufs(sum / n) - 10;
    sum = 0;
    n = 0;
    for (double b : blocks) if (lufs(b) > -70 && lufs(b) > relative) { sum += b; ++n; }
    return n ? lufs(sum / n) : -70;
}

// ---- Summary statistics -----------------------------------------------------
static double pct(std::vector<double> v, double q) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    const size_t i = std::min(v.size() - 1, size_t(q * (v.size() - 1) + 0.5));
    return v[i];
}

static void usage() {
    std::fprintf(stderr,
        "usage: audio_replay in.wav [--chain-db D] [--target-lufs L | --no-normalize]\n"
        "                    [--noise-dbfs N] [--bpm TRUE_BPM] [--csv out.csv] [--label NAME]\n"
        "                    [--path new|old] [--agc on|off] [--correct HZ] [--gate RMS] [--agc-ref RMS] [--agc-rise MS] [--agc-fall DB_PER_S] [--step SECONDS:DB] [--pad-silence S] [--pcm-lowpass HZ]\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 2; }
    const char *wavPath = argv[1];
    // Defaults model the CURRENT hardware: the module filter calibrate.py
    // measured on 2026-11-19 (203 Hz, fit error 0.05 dB).
    double chainDb = 0, targetLufs = -14, noiseDbfs = -96, trueBpm = 0, padSeconds = 0, lowpassHz = 203;
    bool normalize = true;
    const char *csvPath = nullptr;
    std::string label = wavPath;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() { if (i + 1 >= argc) { usage(); std::exit(2); } return argv[++i]; };
        if (a == "--chain-db") chainDb = std::atof(next());
        else if (a == "--target-lufs") targetLufs = std::atof(next());
        else if (a == "--no-normalize") normalize = false;
        else if (a == "--noise-dbfs") noiseDbfs = std::atof(next());
        else if (a == "--bpm") trueBpm = std::atof(next());
        else if (a == "--csv") csvPath = next();
        else if (a == "--label") label = next();
        else if (a == "--agc") uiAutoGain = std::string(next()) == "on";
        else if (a == "--path") uiNewProcessing = std::string(next()) == "new";
        else if (a == "--arb-conc") TempoTracker::ArbiterConcentration = float(std::atof(next()));
        else if (a == "--arb-snap") TempoTracker::ArbiterSnap = float(std::atof(next()));
        else if (a == "--arb-pull") TempoTracker::ArbiterPullSeconds = float(std::atof(next()));
        else if (a == "--hit-sigma") uiHitSettings.sigma = float(std::atof(next()));
        else if (a == "--hit-fixed") uiHitSettings.fixedExcess = float(std::atof(next()));
        else if (a == "--hit-keep") uiHitSettings.keepQuantile = float(std::atof(next()));
        else if (a == "--hit-refractory") uiHitSettings.refractorySeconds = float(std::atof(next()));
        else if (a == "--hit-stats") uiHitSettings.statsSeconds = float(std::atof(next()));
        else if (a == "--no-arbiter") uiKickPhaseArbiter = false;
        else if (a == "--arb-memory") TempoTracker::ArbiterMemorySeconds = float(std::atof(next()));
        else if (a == "--correct") uiInputCorrectionHz = float(std::atof(next()));
        else if (a == "--gate") uiNoiseFloorRMSADC = float(std::atof(next()));
        else if (a == "--agc-ref") uiAutoGainSettings.referenceRms = float(std::atof(next()));
        else if (a == "--pcm-lowpass") lowpassHz = std::atof(next());
        else if (a == "--pad-silence") padSeconds = std::atof(next());
        else if (a == "--agc-rise") uiAutoGainSettings.riseMs = float(std::atof(next()));
        else if (a == "--agc-fall") uiAutoGainSettings.fallDbPerSecond = float(std::atof(next()));
        else if (a == "--step") {
            const std::string v = next();
            const size_t colon = v.find(':');
            stepSeconds = std::atof(v.substr(0, colon).c_str());
            stepFactor = std::pow(10.0, std::atof(v.substr(colon + 1).c_str()) / 20.0);
        }
        else { usage(); return 2; }
    }
    lowpassPole = 0;  // set after argument parsing below
    if (!readWav(wavPath)) { std::fprintf(stderr, "cannot read %s\n", wavPath); return 1; }
    const size_t songSamples = wavLeft.size();

    const double lufs = integratedLufs();
    double peak = 0;
    for (size_t i = 0; i < wavLeft.size(); ++i)
        peak = std::max(peak, double(std::max(std::fabs(wavLeft[i]), std::fabs(wavRight[i]))));
    double normDb = 0;
    if (normalize) {
        normDb = targetLufs - lufs;
        // Spotify only raises quiet tracks as far as their peak headroom allows.
        const double headroom = -1.0 - 20 * std::log10(std::max(peak, 1e-9));
        if (normDb > headroom) normDb = std::max(0.0, headroom);
    }
    inputGain = std::pow(10.0, (normDb + chainDb) / 20.0);
    lowpassPole = lowpassHz > 0 ? std::exp(-2 * M_PI * lowpassHz / AudioAnalysis::SampleRate) : 0.0;
    // --pad-silence S appends S seconds of digital silence (only ADC noise).
    wavLeft.resize(songSamples + size_t(padSeconds * AudioAnalysis::SampleRate), 0.0f);
    wavRight.resize(wavLeft.size(), 0.0f);
    noiseRms = std::pow(10.0, noiseDbfs / 20.0);

    // Production defaults, exactly as SignalAcquisition.cpp initializes them.
    // (Nothing here overrides a ui* value.)
    configureI2SForMode(MODE_ADC);
    currentMode = MODE_ADC;
    clockMsExact += 600;  // Past the 500 ms reset/fade-in drain.

    const size_t hops = wavLeft.size() / AudioAnalysis::HopSamples;
    for (size_t h = 0; h < hops; ++h) acquireAudio(false);

    if (csvPath) {
        FILE *c = std::fopen(csvPath, "w");
        std::fprintf(c, "t,volumeRaw,volumeSm,sampleAvg");
        for (int b = 0; b < 16; ++b) std::fprintf(c, ",fft%d", b);
        std::fprintf(c, ",beatCount,beat,attackCount,attackStrength,attackBands,bpm,phase,confidence,flags,agcDb,kickPhase,kickConc,kickRise");
        for (int b = 0; b < 16; ++b) std::fprintf(c, ",tb%d", b);
        std::fprintf(c, "\n");
        for (const Row &r : rows) {
            std::fprintf(c, "%.4f,%u,%u,%u", r.t, r.p.volumeRaw, r.p.volumeSm, r.p.sampleAvg);
            for (int b = 0; b < 16; ++b) std::fprintf(c, ",%u", r.p.fftResult[b]);
            std::fprintf(c, ",%u,%u,%u,%u,%u,%.2f,%u,%u,%u,%.2f,%.3f,%.3f,%.3f", r.beats, r.p.beat, r.p.attackCount,
                         r.p.attackStrength, r.p.attackBands, r.p.tempoBpm100 / 100.0, r.p.tempoPhase,
                         r.p.tempoConfidence, r.p.tempoFlags, r.agcDb, r.kickPhase, r.kickConcentration, r.kickRise);
            for (int b = 0; b < 16; ++b) std::fprintf(c, ",%.3g", r.tb[b]);
            std::fprintf(c, "\n");
        }
        std::fclose(c);
    }

    // ---- Summary ----
    const double seconds = rows.empty() ? 0 : rows.back().t - rows.front().t;
    const double minutes = seconds / 60.0;
    std::vector<double> vSm, bandAll[16];
    size_t rawClip = 0, smOver75 = 0, bandClip[16] = {}, locked = 0, nearTrue = 0, nearOctave = 0;
    std::vector<double> lockedBpm;
    uint32_t lastAttack = rows.empty() ? 0 : rows.front().p.attackCount;
    size_t groupEvents[3] = {}, groupAdmitted[3] = {};
    const uint16_t masks[3] = {0x000F, 0x07F0, 0xF800};
    for (const Row &r : rows) {
        vSm.push_back(r.p.volumeSm);
        rawClip += r.p.volumeRaw == 255;
        smOver75 += r.p.volumeSm >= 75;
        for (int b = 0; b < 16; ++b) {
            bandAll[b].push_back(r.p.fftResult[b]);
            bandClip[b] += r.p.fftResult[b] == 255;
        }
        if (r.p.attackCount != lastAttack) {
            for (int g = 0; g < 3; ++g) {
                if (r.p.attackBands & masks[g]) {
                    ++groupEvents[g];
                    groupAdmitted[g] += r.p.attackStrength >= 32;  // Render admission at sensitivity 1.
                }
            }
            lastAttack = r.p.attackCount;
        }
        if (r.p.tempoFlags & CubinaWire::TEMPO_LOCKED) {
            ++locked;
            const double bpm = r.p.tempoBpm100 / 100.0;
            lockedBpm.push_back(bpm);
            if (trueBpm > 0) {
                nearTrue += std::fabs(bpm - trueBpm) <= trueBpm * 0.02;
                nearOctave += std::fabs(bpm - 2 * trueBpm) <= trueBpm * 0.04 ||
                              std::fabs(bpm - trueBpm / 2) <= trueBpm * 0.01;
            }
        }
    }
    const double n = std::max<size_t>(1, rows.size());
    const uint32_t kicks = rows.empty() ? 0 : rows.back().beats;
    const uint32_t attacks = rows.empty() ? 0 : rows.back().p.attackCount;
    std::vector<double> agc;
    for (const Row &r : rows) agc.push_back(r.agcDb);
    std::printf("== %s  chain %+.1f dB  path %s (AGC %s, correction %.0f Hz)  module low-pass %.0f Hz ==\n",
                label.c_str(), chainDb, uiNewProcessing ? "new" : "old", uiAutoGain ? "on" : "off",
                uiInputCorrectionHz, lowpassHz);
    if (uiNewProcessing && uiAutoGain) std::printf("agc     gain p10/p50/p90 = %+.1f/%+.1f/%+.1f dB\n", pct(agc, .1), pct(agc, .5), pct(agc, .9));
    std::printf("source  %.1f LUFS, peak %.1f dBFS, normalization %+.1f dB, total gain %+.1f dB, ADC clipped samples %zu\n",
                lufs, 20 * std::log10(std::max(peak, 1e-9)), normDb, normDb + chainDb, clippedSamples);
    std::printf("volume  Sm p10/p50/p90 = %.0f/%.0f/%.0f   Raw at 255: %.1f%%   Sm>=75: %.1f%%\n",
                pct(vSm, .1), pct(vSm, .5), pct(vSm, .9), 100 * rawClip / n, 100 * smOver75 / n);
    std::printf("bands   p50:");
    for (int b = 0; b < 16; ++b) std::printf(" %3.0f", pct(bandAll[b], .5));
    std::printf("\n        p90:");
    for (int b = 0; b < 16; ++b) std::printf(" %3.0f", pct(bandAll[b], .9));
    std::printf("\n        @255%%:");
    for (int b = 0; b < 16; ++b) std::printf(" %3.0f", 100 * bandClip[b] / n);
    std::printf("\nevents  kicks %.0f/min, attacks %.0f/min; per group (events/min, %% admitted at sens 1): "
                "low %.0f (%.0f%%) mid %.0f (%.0f%%) high %.0f (%.0f%%)\n",
                kicks / minutes, attacks / minutes,
                groupEvents[0] / minutes, groupEvents[0] ? 100.0 * groupAdmitted[0] / groupEvents[0] : 0,
                groupEvents[1] / minutes, groupEvents[1] ? 100.0 * groupAdmitted[1] / groupEvents[1] : 0,
                groupEvents[2] / minutes, groupEvents[2] ? 100.0 * groupAdmitted[2] / groupEvents[2] : 0);
    std::printf("tempo   locked %.0f%% of time, locked BPM p10/p50/p90 = %.1f/%.1f/%.1f",
                100 * locked / n, pct(lockedBpm, .1), pct(lockedBpm, .5), pct(lockedBpm, .9));
    if (trueBpm > 0) {
        std::printf(", true %.0f: within 2%% %.0f%% of locked time, at half/double %.0f%%",
                    trueBpm, locked ? 100.0 * nearTrue / locked : 0, locked ? 100.0 * nearOctave / locked : 0);
    }
    std::printf("\n\n");
    return 0;
}
