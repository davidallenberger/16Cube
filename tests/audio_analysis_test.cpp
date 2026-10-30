#include "SignalAcquisition.cpp"
#include <cassert>
#include <cstring>

SerialStub Serial;
SerialStub Serial2;
FastLEDStub FastLED;
CRGB CRGB::Black, CRGB::Green, CRGB::Yellow, CRGB::Red;
CRGB CRGB::White, CRGB::Blue, CRGB::Purple;

static unsigned long clockMs = 1000;
static int readFailure = 0;
static size_t sampleIndex = 0;
static double toneHz = 1000;
static double amplitude = 10000000;
static bool invertedRight = false;
static AudioPayload published = {};
static unsigned publications = 0;
static uint32_t publishedBeats = 0;

unsigned long millis () {
    return clockMs;
}
uint32_t micros () {
    return uint32_t (clockMs * 1000);
}
void delay (int) {
}
long map (long x, long inLow, long inHigh, long outLow, long outHigh) {
    return (x - inLow) * (outHigh - outLow) / (inHigh - inLow) + outLow;
}
const char *esp_err_to_name (esp_err_t) {
    return "host-test";
}
esp_err_t i2s_driver_uninstall (int) {
    return ESP_OK;
}
esp_err_t i2s_driver_install (int, const i2s_config_t *, int, void *) {
    return ESP_OK;
}
esp_err_t i2s_set_pin (int, const i2s_pin_config_t *) {
    return ESP_OK;
}

esp_err_t i2s_read (int, void *data, size_t requested, size_t *received, int) {
    assert (requested ==
            AudioAnalysis::HopSamples * sizeof (int32_t) * (currentMode == MODE_ADC ? 2 : 1));

    if (readFailure) {
        *received = requested / 2;
        return ESP_OK;
    }

    int32_t *words = static_cast<int32_t *> (data);
    const size_t channels = currentMode == MODE_ADC ? 2 : 1;

    for (size_t i = 0; i < AudioAnalysis::HopSamples; ++i) {
        const double phase = 6.283185307179586 * toneHz * sampleIndex / AudioAnalysis::SampleRate;
        const int32_t value = int32_t (amplitude * std::sin (phase));
        ++sampleIndex;

        for (size_t channel = 0; channel < channels; ++channel) {
            words[i * channels + channel] = channel == 1 && invertedRight ? -value : value;
        }
    }

    *received = requested;
    return ESP_OK;
}

bool SourceAdapters::readSD (int32_t *, size_t) {
    return false;
}
void CommandState::publishAudio (const AudioPayload &audio, uint32_t beats) {
    published = audio;
    publishedBeats = beats;
    ++publications;
}

static void resetSource (SystemMode mode) {
    assert (configureI2SForMode (mode));
    currentMode = mode;
    clockMs += 600;
    sampleIndex = 0;
    publications = 0;
    readFailure = 0;
}

static void hop () {
    clockMs += 8;
    acquireAudio (false);
}

int main () {
    // Independent overlap histories and FFT scratch storage, including DC.
    AudioAnalysis::OverlapWindow window;
    float l[256];
    float r[256];
    float real[512];
    float imag[512];
    for (int block = 0; block < 4; ++block) {
        for (int i = 0; i < 256; ++i) {
            l[i] = float (block * 256 + i);
            r[i] = -l[i];
        }
        assert (window.appendHop (l, r, true) == (block > 0));
        if (block > 0) {
            const float rms = window.copyForFFT (real, imag);
            assert (rms > 147 && rms < 149);
            for (int i = 0; i < 512; ++i) {
                assert (real[i] == float (i) - 255.5f);
                real[i] = -999;
                assert (imag[i] == 0);
            }
        }
    }
    window.reset ();
    assert (!window.appendHop (l, r, true));

    resetSource (MODE_ADC);
    toneHz = 16.0 * 31250 / 512;
    hop ();
    assert (publications == 0);
    recordKickEvent (millis ());
    hop ();
    assert (publications == 1 && published.beat == 1);
    const unsigned normalVolume = published.volumeRaw;
    assert (normalVolume >= 41 && normalVolume <= 43);

    // Same waveform power, opposite stereo polarity: volume/spectrum survive.
    resetSource (MODE_ADC);
    invertedRight = true;
    hop ();
    hop ();
    assert (published.volumeRaw == normalVolume);
    bool spectrumPresent = false;
    for (uint8_t level : published.fftResult) {
        spectrumPresent |= level > 0;
    }
    assert (spectrumPresent);
    invertedRight = false;

    // Equal-amplitude coherent bass and midrange produce equal overall RMS.
    resetSource (MODE_ADC);
    toneHz = 31250.0 / 512;
    hop ();
    hop ();
    assert (published.volumeRaw == normalVolume);
    assert (published.fftResult[0] > 0);

    // Source differences are calibration only, not a different algorithm.
    uiInputGainMic = uiInputGainADC;
    resetSource (MODE_I2S_MIC);
    hop ();
    hop ();
    assert (published.volumeRaw == normalVolume);
    assert (published.fftResult[0] > 0);

    // Identical detector input on left: reference must see each NEW sample once.
    BassKickDetector reference;
    reference.begin (SAMPLING_FREQ);
    for (size_t i = 0; i < sampleIndex; ++i) {
        const double phase = 6.283185307179586 * toneHz * i / SAMPLING_FREQ;
        const int32_t value = int32_t (amplitude * std::sin (phase));
        reference.processSample (float (value) / 2147483648.0f);
        if ((i + 1) % 256 == 0) {
            reference.finishBlock (uiKickSensitivity, uiKickMinRMS);
        }
    }
    assert (std::abs (reference.bassRms - kickDetector.bassRms) < 1e-8f);

    // Errors discard history/state; no publication from partial buffers.
    const unsigned before = publications;
    readFailure = 1;
    hop ();
    assert (audioHealth.readErrors == 1 && publications == before);
    readFailure = 0;
    hop ();
    assert (publications == before);
    hop ();
    assert (publications == before + 1);

    // Quiet mic has no fabricated VAvg=64 floor.
    amplitude = 0;
    resetSource (MODE_I2S_MIC);
    hop ();
    hop ();
    assert (published.volumeRaw == 0 && published.volumeSm == 0 && published.sampleAvg == 0);
    for (uint8_t level : published.fftResult) {
        assert (level == 0);
    }

    // No hidden values above 255 are allowed to hold the envelope at the ceiling.
    amplitude = 500000000;
    resetSource (MODE_ADC);
    hop ();
    hop ();
    assert (published.volumeRaw == 255 && volumeSmFloat <= 255);
    amplitude = 0;
    hop ();
    hop ();
    const float previous = volumeSmFloat;
    hop ();
    assert (volumeSmFloat < previous && published.volumeRaw == 0);

    // Smoothing is specified in elapsed AUDIO time, independent of FFT size.
    const float once = AudioAnalysis::smoothLevel (100, 0, 0.016f, 16, 50);
    float twice = AudioAnalysis::smoothLevel (100, 0, 0.008f, 16, 50);
    twice = AudioAnalysis::smoothLevel (twice, 0, 0.008f, 16, 50);
    assert (std::abs (once - twice) < 0.0001f);
    puts ("Common audio tests passed: overlap, DC removal, phase-safe RMS/spectrum, bass coverage, "
          "common source calibration, single-fed kick, partial reads, silence, clipping recovery, "
          "timed smoothing.");
}
