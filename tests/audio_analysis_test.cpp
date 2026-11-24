#include "SignalAcquisition.cpp"
#include <cassert>
#include <cstring>

SerialStub Serial;
SerialStub Serial2;
FastLEDStub FastLED;
CRGB CRGB::Black, CRGB::Green(0, 128, 0), CRGB::Yellow(255, 255, 0), CRGB::Red(255, 0, 0);
CRGB CRGB::White(255, 255, 255), CRGB::Blue(0, 0, 255), CRGB::Purple(128, 0, 128);

static unsigned long clockMs = 1000;
static int readFailure = 0;
static size_t sampleIndex = 0;
static double toneHz = 1000;
static double moduleLowpassHz = 0.0;  // Models the ADC module's input filter.
static double moduleLowpassState[2] = {};
static double amplitude = 10000000;
static bool invertedRight = false;
static AudioPayload published = {};
static unsigned publications = 0;
static uint32_t publishedBeats = 0;
static unsigned installs = 0;
static unsigned uninstalls = 0;
static unsigned sourceSelections = 0;
static unsigned modeSelections = 0;
static unsigned modeRepeats = 0;
static i2s_config_t lastConfig = {};
static i2s_pin_config_t lastPins = {};

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
    ++uninstalls;
    return ESP_OK;
}
esp_err_t i2s_driver_install (int, const i2s_config_t *config, int, void *) {
    ++installs;
    lastConfig = *config;
    return ESP_OK;
}
esp_err_t i2s_set_pin (int, const i2s_pin_config_t *pins) {
    lastPins = *pins;
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
        double sample = amplitude * std::sin (phase);
        ++sampleIndex;
        if (moduleLowpassHz > 0.0) {
            const double pole = std::exp (-6.283185307179586 * moduleLowpassHz / AudioAnalysis::SampleRate);
            moduleLowpassState[0] = sample + pole * (moduleLowpassState[0] - sample);
            sample = moduleLowpassState[0];
        }
        const int32_t value = int32_t (sample);

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
bool SourceAdapters::select(CubinaWire::Mode) {
    ++sourceSelections;
    return true;
}
static CubinaWire::Mode lastSelectedMode = CubinaWire::Mode::Animation;
void CommandState::selectMode(CubinaWire::Mode mode) {
    ++modeSelections;
    lastSelectedMode = mode;
}
static int pressedPin = -1;
int digitalRead(int pin) {
    return pin == pressedPin ? LOW : 1;
}
// One debounced press and release of a command-board button.
static void pressButton(int pin) {
    pressedPin = pin;
    pollButtons();
    clockMs += 40;
    pollButtons();
    pressedPin = -1;
    pollButtons();
    clockMs += 40;
    pollButtons();
}
void CommandState::togglePlaybackHold() {
}
static bool lastRepeatBack = false;
void CommandState::repeatModeDemand(bool back) {
    ++modeRepeats;
    lastRepeatBack = back;
}
void CommandState::setReady(bool) {
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

// Unified hit detector (NEW): silence never hits; a sharp periodic burst in
// one group hits that group at the burst rate; the same music 20 dB quieter
// gives the same hits (log onset strength); refractory limits the rate.
static void hitDetectorChecks() {
    const float hop = AudioAnalysis::HopSeconds;
    auto run = [&](float scale, float burstHz, unsigned band, float refractory, unsigned &groups) {
        HitDetector detector;
        detector.settings.refractorySeconds = refractory;
        unsigned count = 0;
        groups = 0;
        const unsigned hops = unsigned(20.0f / hop);
        for (unsigned i = 0; i < hops; ++i) {
            float bands[16];
            for (unsigned b = 0; b < 16; ++b) bands[b] = 0.01f * scale; // steady bed
            const float t = i * hop;
            const float age = t - floorf(t * burstHz) / burstHz;
            // +40 dB in two adjacent bands (a kick or hat spans several);
            // the group's onset strength is the mean over its bands.
            if (burstHz > 0.0f) {
                bands[band] += 1.0f * scale * expf(-age / 0.03f);
                bands[band + 1] += 1.0f * scale * expf(-age / 0.03f);
            }
            const auto r = detector.process(bands, true, hop);
            if (r.event) { ++count; groups |= r.groups; }
        }
        return count;
    };
    unsigned groups = 0;
    assert(run(1.0f, 0.0f, 0, 0.1f, groups) == 0);            // steady sound: no hits
    const unsigned loud = run(1.0f, 2.0f, 1, 0.1f, groups);
    assert(groups == 1u);                                       // low group only
    assert(loud >= 36 && loud <= 40);                           // ~2 Hz for 19 s after warm-up
    unsigned quietGroups = 0;
    const unsigned quiet = run(0.1f, 2.0f, 1, 0.1f, quietGroups);
    assert(quiet == loud && quietGroups == groups);             // level independent
    unsigned highGroups = 0;
    assert(run(1.0f, 2.0f, 12, 0.1f, highGroups) >= 36 && highGroups == 4u);
    unsigned fastGroups = 0;
    const unsigned limited = run(1.0f, 8.0f, 1, 0.5f, fastGroups); // 8 Hz bursts, 0.5 s refractory
    assert(limited <= 40);
    puts("Hit detector passed: silence/steady quiet, per-group hits at burst rate, level independence, refractory.");
}

int main () {
    hitDetectorChecks();
    // Numerical fixtures use explicit settings, independent of the user's
    // source calibration retained by the installer.
    uiMasterSensitivity = 1.0f;
    uiInputGainADC = 1.0f;
    uiInputGainMic = 4.0f;
    uiFullScaleRMS = 0.02f;
    uiNoiseFloorRMSADC = 0.00002f;
    uiNoiseFloorRMSMic = 0.00010f;
    uiSpectrumGamma = 0.60f;
    uiVolumeAttackMs = 16.0f;
    uiVolumeReleaseMs = 50.0f;
    uiBandAttackMs = 16.0f;
    uiBandReleaseMs = 50.0f;
    uiAverageMs = 800.0f;
    for (float &gain : binEQ) {
        gain = 1.0f;
    }
    // These fixtures check the PREVIOUS (fixed, uncorrected) path. AGC and
    // the input correction have their own tests below.
    uiNewProcessing = false;
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

    // Actual acquisition dispatch: next visual preserves the installed driver
    // and measurements. A real Mic/ADC change still sets the correct input pin.
    using CubinaWire::Mode;
    resetSource(MODE_ADC);
    selected = Mode::AudioBluetooth;
    volumeSmFloat = 73.0f;
    const unsigned initialInstalls = installs;
    const unsigned initialUninstalls = uninstalls;
    const unsigned initialSelections = sourceSelections;
    demand(Mode::AudioBluetooth);
    assert(modeRepeats == 1);
    assert(installs == initialInstalls);
    assert(uninstalls == initialUninstalls);
    assert(sourceSelections == initialSelections);
    assert(volumeSmFloat == 73.0f);

    demand(Mode::AudioMic);
    assert(installs == initialInstalls + 1);
    assert(lastPins.data_in_num == I2S_SD_MIC);
    assert(lastConfig.channel_format == I2S_CHANNEL_FMT_ONLY_RIGHT);
    demand(Mode::AudioMic);
    assert(modeRepeats == 2);
    assert(installs == initialInstalls + 1);

    // Even legacy SD/DJ requests collapse to the single physical PCM input.
    demand(Mode::AudioSD);
    assert(selected == Mode::AudioBluetooth);
    assert(currentMode == MODE_ADC);
    assert(lastPins.data_in_num == I2S_SD_ADC);
    assert(lastPins.bck_io_num == I2S_SCK && lastPins.ws_io_num == I2S_WS);
    assert(lastConfig.channel_format == I2S_CHANNEL_FMT_RIGHT_LEFT);
    assert(lastConfig.sample_rate == 31250);
    assert(lastConfig.communication_format == (PCM1808_LEFT_JUSTIFIED ? I2S_COMM_FORMAT_STAND_MSB : I2S_COMM_FORMAT_STAND_I2S));
    assert(lastConfig.use_apll == PCM1808_USE_APLL);
    demand(Mode::AudioDJ);
    assert(selected == Mode::AudioBluetooth);
    assert(modeRepeats == 3);
    assert(installs == initialInstalls + 2);

    // Repeating after failed setup retries acquisition, rather than claiming
    // that a non-running driver can remain active.
    i2sReady = false;
    demand(Mode::AudioBluetooth);
    assert(i2sReady);
    assert(installs == initialInstalls + 3);
    assert(modeSelections == 3);
    // Real acquisition path: PCM -> dedicated kick detector + FFT -> shared
    // tracker -> wire payload. No hand-injected tempo or kick events.
    resetSource(MODE_ADC);
    toneHz = 70.0;
    invertedRight = true;
    for (unsigned i = 0; i < 1600; ++i) {
        const double position = double(sampleIndex) / SAMPLING_FREQ * 126.0 / 60.0;
        const double age = (position - floor(position)) * 60.0 / 126.0;
        amplitude = 500000.0 + 80000000.0 * exp(-age / 0.045);
        hop();
    }
    assert(published.tempoFlags & CubinaWire::TEMPO_LOCKED);
    assert(std::abs(int(published.tempoBpm100) - 12600) < 300);
    const double position = double(sampleIndex) / SAMPLING_FREQ * 126.0 / 60.0;
    double error = double(published.tempoPhase) / 65536.0 - (position - floor(position));
    error -= floor(error + 0.5);
    assert(std::abs(error) < 0.10);
    puts("Kick-guided tempo passed through production PCM acquisition and wire publication at 126 BPM.");

    {
        // AutoGain alone: steady input settles to the reference, louder input
        // is pulled down quickly, quieter input recovers at the fall rate,
        // silence freezes, and limits hold.
        using AudioAnalysis::AutoGain;
        AutoGain::Settings settings;
        AutoGain agc;
        const float hop = AudioAnalysis::HopSeconds;
        auto run = [&](float rms, bool audible, float seconds) {
            for (float t = 0; t < seconds; t += hop) {
                agc.update(rms, audible, hop, settings);
            }
        };
        agc.reset(settings);
        assert(std::abs(agc.gainDb()) < 0.01f);
        run(settings.referenceRms, true, 5.0f);
        assert(std::abs(agc.gainDb()) < 0.5f);
        run(settings.referenceRms * 3.1623f, true, 3.0f);   // +10 dB
        assert(std::abs(agc.gainDb() + 10.0f) < 0.5f);
        const float beforeSilence = agc.gainDb();
        run(0.0f, false, 60.0f);                             // Gate closed.
        assert(agc.gainDb() == beforeSilence);
        run(settings.referenceRms, true, 10.0f);            // Back down 10 dB: slow.
        assert(agc.gainDb() < -5.0f && agc.gainDb() > -8.0f);  // ~0.3 dB/s for ~9.6 s.
        run(settings.referenceRms, true, 30.0f);
        assert(std::abs(agc.gainDb()) < 0.5f);
        // -60 dB: below the follow range, so it holds. Only the ~4 s while
        // the 0.4 s loudness falls through that range moves it (~1 dB).
        run(settings.referenceRms * 0.001f, true, 120.0f);
        assert(agc.gainDb() >= 0.0f && agc.gainDb() < 1.5f);
        // Idle hiss above the noise gate but below minAdaptRms must never
        // raise the gain, however long it lasts (seen on hardware: +18 dB).
        const float beforeHiss = agc.gainDb();
        run(settings.minAdaptRms * 0.5f, true, 600.0f);
        assert(std::abs(agc.gainDb() - beforeHiss) < 0.01f);
        // Quiet music above minAdaptRms still adapts; the boost stops at its limit.
        AutoGain::Settings lowFloor = settings;
        lowFloor.minAdaptRms = 0.0f;
        for (float t = 0; t < 300.0f; t += hop) {
            agc.update(settings.referenceRms * 0.0178f, true, hop, lowFloor);  // -35 dB
        }
        assert(std::abs(agc.gainDb() - settings.maxBoostDb) < 0.01f);
        run(settings.referenceRms * 1000.0f, true, 5.0f);   // +60 dB: cut stops at limit.
        assert(std::abs(agc.gainDb() + settings.maxCutDb) < 0.01f);
    }
    {
        // Through production acquisition: the same tone 12 dB apart settles
        // to the same display level with AGC, but not without it. Kick and
        // tempo results are identical with AGC on or off.
        toneHz = 1000.0;
        invertedRight = false;
        uiInputCorrectionHz = 0.0f;  // AGC alone; the correction is tested separately.
        auto settle = [&](bool agcOn, double level) {
            uiNewProcessing = agcOn;
            resetSource(MODE_ADC);
            amplitude = level;
            for (unsigned i = 0; i < 1200; ++i) {
                hop();
            }
            return int(published.volumeSm);
        };
        // A tone at the AGC reference RMS, and the same tone +9.5 dB. (A
        // quieter-than-reference source recovers at the slow fall rate, which
        // the AutoGain checks above cover; ten seconds here is not enough.)
        const double reference = uiAutoGainSettings.referenceRms * 1.41421356 * 2147483648.0;
        const int fixedQuiet = settle(false, reference);
        const int fixedLoud = settle(false, reference * 3.0);
        const int agcQuiet = settle(true, reference);
        const int agcLoud = settle(true, reference * 3.0);
        assert(fixedLoud > 5 * fixedQuiet / 2);
        assert(std::abs(agcLoud - agcQuiet) <= 3);

        // Toggle AGC alone within NEW: NEW's own hit detector and tempo
        // differ from OLD by design, but must not depend on display gain.
        auto kicks = [&](bool agcOn) {
            uiNewProcessing = true;
            uiAutoGain = agcOn;
            resetSource(MODE_ADC);
            toneHz = 70.0;
            for (unsigned i = 0; i < 1600; ++i) {
                const double position = double(sampleIndex) / SAMPLING_FREQ * 126.0 / 60.0;
                const double age = (position - floor(position)) * 60.0 / 126.0;
                amplitude = 500000.0 + 80000000.0 * exp(-age / 0.045);
                hop();
            }
            return published;
        };
        // attackCount is cumulative for the boot, so compare per-run deltas.
        uint32_t startCount = attackCount;
        AudioPayload off = kicks(false);
        off.attackCount -= startCount;
        startCount = attackCount;
        AudioPayload on = kicks(true);
        on.attackCount -= startCount;
        assert(off.attackCount == on.attackCount && off.tempoBpm100 == on.tempoBpm100 &&
               off.tempoPhase == on.tempoPhase && off.tempoFlags == on.tempoFlags);
        uiNewProcessing = false;
        uiAutoGain = true;
        uiInputCorrectionHz = 235.0f;
    }
    {
        // NEW is the reference: Game no longer flips processing. In an audio
        // mode it asks for the previous routine (no source change); outside
        // audio it still selects Game mode. The serial "ab" command and
        // uiNewProcessing still choose the path for debugging.
        uiNewProcessing = true;
        demand(Mode::AudioBluetooth);
        assert(newProcessingActive);
        const unsigned installsBefore = installs;
        const unsigned selectionsBefore = modeSelections;
        clockMs += 600;
        toneHz = 1000.0;
        amplitude = 20000000.0;
        for (unsigned i = 0; i < 200; ++i) {
            hop();
        }
        const unsigned repeatsBefore = modeRepeats;
        pressButton(COMMAND_BTN_GAME);
        assert(newProcessingActive && modeRepeats == repeatsBefore + 1 && lastRepeatBack);
        assert(installs == installsBefore && modeSelections == selectionsBefore);
        hop();
        assert(leds[0].r == CRGB::Blue.r && leds[0].g == CRGB::Blue.g && leds[0].b == CRGB::Blue.b);
        pressButton(COMMAND_BTN_ADC);   // Same source again: Next, not a reset.
        assert(modeRepeats == repeatsBefore + 2 && !lastRepeatBack);
        pressButton(COMMAND_BTN_ANIMATION);
        assert(newProcessingActive && lastSelectedMode == Mode::Animation);
        pressButton(COMMAND_BTN_GAME);
        assert(newProcessingActive && lastSelectedMode == Mode::Game);
        uiNewProcessing = false;
        demand(Mode::AudioBluetooth);
        assert(!newProcessingActive);
    }
    {
        // Input correction: undoing a modeled 235 Hz first-order low-pass
        // restores the original samples, and 0 Hz is an exact pass-through.
        AudioAnalysis::InputCorrection correction;
        correction.configure(235.0f);
        const float pole = expf(-6.283185307f * 235.0f / float(SAMPLING_FREQ));
        float filtered = 0.0f;
        float worst = 0.0f;
        for (int n = 0; n < 4000; ++n) {
            const float original = 0.3f * sinf(0.7f * n) + 0.2f * sinf(0.05f * n) + ((n * 7919) % 13 - 6) * 0.01f;
            filtered = original + pole * (filtered - original);
            worst = fmaxf(worst, fabsf(correction.process(filtered) - original));
        }
        assert(worst < 2e-4f);
        AudioAnalysis::InputCorrection passThrough;
        passThrough.configure(0.0f);
        assert(passThrough.process(0.25f) == 0.25f && passThrough.process(-0.5f) == -0.5f);
    }
    {
        // Through production acquisition with a 235 Hz module modeled in the
        // stub's samples: new processing reads a 6.4 kHz tone about as strongly
        // as a 400 Hz tone of the same original level (old processing reads it
        // ~22 dB weaker), sets SPECTRUM_CORRECTED, and the silence gate still
        // measures the uncorrected input.
        uiAutoGain = false;  // Compare levels without AGC.
        invertedRight = false;
        auto band = [&](bool corrected, double hz, unsigned index) {
            uiNewProcessing = corrected;
            resetSource(MODE_ADC);
            toneHz = hz;
            moduleLowpassHz = 235.0;
            amplitude = 20000000.0;
            for (unsigned i = 0; i < 300; ++i) {
                hop();
            }
            moduleLowpassHz = 0.0;
            return std::make_pair(int(published.fftResult[index]), published.tempoFlags);
        };
        // Tones exactly on FFT slots 6 (366 Hz, band 4) and 105 (6.41 kHz,
        // band 13) so each lands wholly inside one band.
        const double slotHz = double(SAMPLING_FREQ) / AudioAnalysis::WindowSamples;
        const auto oldLow = band(false, 6 * slotHz, 4);
        const auto oldHigh = band(false, 105 * slotHz, 13);
        const auto newLow = band(true, 6 * slotHz, 4);
        const auto newHigh = band(true, 105 * slotHz, 13);
        assert(!(oldLow.second & CubinaWire::SPECTRUM_CORRECTED));
        assert(newLow.second & CubinaWire::SPECTRUM_CORRECTED);
        assert(oldHigh.first * 3 < oldLow.first);              // Treble lost without correction.
        assert(std::abs(newHigh.first - newLow.first) * 5 <= newLow.first);  // Within ~20%.
        uiAutoGain = true;
        uiNewProcessing = false;
    }
    {
        // Calibration profiles: each physical input loads its assigned
        // profile; values are set, saved per profile, reloaded after a source
        // change, and fall back to compiled per-input defaults.
        handleCalibrationCommand("set gate 0.0005");          // Refused: no audio input yet.
        handleCalibrationCommand("mode adc");
        assert(selected == Mode::AudioBluetooth && strcmp(activeProfile, "bt-pcm1808") == 0);
        assert(uiInputCorrectionHz == 235.0f);                // Compiled ADC default.
        handleCalibrationCommand("set gate 0.0005");
        handleCalibrationCommand("set corr_hz 201");
        handleCalibrationCommand("set agc_ref 0.004");
        handleCalibrationCommand("set kick_min 0.0004");
        assert(uiNoiseFloorRMSADC == 0.0005f && uiInputCorrectionHz == 201.0f);
        assert(uiAutoGainSettings.referenceRms == 0.004f && uiKickMinRMS == 0.0004f);
        handleCalibrationCommand("set gate 5");               // Clamped, not rejected.
        assert(uiNoiseFloorRMSADC == 0.01f);
        handleCalibrationCommand("set gate 0.0005");
        handleCalibrationCommand("set nonsense 1");            // Unknown key changes nothing.
        assert(uiNoiseFloorRMSADC == 0.0005f);
        handleCalibrationCommand("save");
        handleCalibrationCommand("defaults");
        assert(uiInputCorrectionHz == 235.0f);
        handleCalibrationCommand("load");
        assert(uiNoiseFloorRMSADC == 0.0005f && uiInputCorrectionHz == 201.0f);

        // The microphone has its own profile: flat, no PCM1808 correction.
        handleCalibrationCommand("mode mic");
        assert(strcmp(activeProfile, "mic-inmp441") == 0 && uiInputCorrectionHz == 0.0f);
        // Back to ADC: its saved profile is restored automatically.
        handleCalibrationCommand("mode adc");
        assert(uiNoiseFloorRMSADC == 0.0005f && uiInputCorrectionHz == 201.0f);

        // A second hardware configuration on the same input, e.g. a DJ deck.
        handleCalibrationCommand("profile use dj-pcm1808");
        assert(strcmp(activeProfile, "dj-pcm1808") == 0 && uiInputCorrectionHz == 235.0f);
        handleCalibrationCommand("set gate 0.001");
        handleCalibrationCommand("save");
        handleCalibrationCommand("mode anim");
        handleCalibrationCommand("mode adc");                  // The assignment persists.
        assert(strcmp(activeProfile, "dj-pcm1808") == 0 && uiNoiseFloorRMSADC == 0.001f);
        handleCalibrationCommand("profile use Bad Name");      // Rejected: invalid characters.
        assert(strcmp(activeProfile, "dj-pcm1808") == 0);
        handleCalibrationCommand("forget");
        handleCalibrationCommand("load");                      // Nothing stored: compiled values.
        assert(uiNoiseFloorRMSADC == 0.0002f);
        handleCalibrationCommand("profile use bt-pcm1808");
        assert(uiInputCorrectionHz == 201.0f);

        handleCalibrationCommand("ab old");
        assert(!newProcessingActive);
        handleCalibrationCommand("ab new");
        assert(newProcessingActive);
        handleCalibrationCommand("measure 0.5");
        assert(calibrationMeter.busy());
        clockMs += 600;
        toneHz = 1000.0;
        amplitude = 20000000.0;
        for (unsigned i = 0; i < 70 && calibrationMeter.busy(); ++i) {
            hop();
        }
        assert(!calibrationMeter.busy() && calibrationMeter.hops >= 60);
        assert(calibrationMeter.rmsDb() > -60.0f && calibrationMeter.rmsDb() < -30.0f);
        handleCalibrationCommand("forget");
        handleCalibrationCommand("defaults");
        handleCalibrationCommand("mode anim");
        handleCalibrationCommand("measure 1");                 // Refused outside audio.
        assert(!calibrationMeter.busy());
        handleCalibrationCommand("debug off");
        assert(!audioDebugEnabled);
        handleCalibrationCommand("debug on");
        Preferences::store().clear();
        Preferences::strings().clear();
    }
    puts("Calibration profiles passed: per-input defaults, set/clamp, save/load per profile, mic without correction, second configuration on one input, A/B, timed measurement.");
    puts("Input correction passed: exact inverse of the module low-pass, pass-through at 0 Hz, "
         "treble restored through acquisition, flag on the wire.");
    puts("Game button passed: previous routine in audio modes without a source reset or processing change, "
         "Game mode still reachable, uiNewProcessing still selects the path.");
    puts("AGC passed: settles to reference, fast cut, slow recovery, silence freeze, limits; "
         "display level independent of input level; detectors unchanged.");
    puts("Acquisition dispatch passed: repeat preserves I2S and levels, Mic/ADC pins, single PCM source, failed-setup retry.");
    puts ("Common audio tests passed: overlap, DC removal, phase-safe RMS/spectrum, bass coverage, "
          "common source calibration, single-fed kick, partial reads, silence, clipping recovery, "
          "timed smoothing.");
}
