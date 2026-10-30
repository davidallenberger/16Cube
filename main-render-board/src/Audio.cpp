#include "Audio.h"
#include <math.h>

#ifndef AUDIO_SERIAL_DEBUG
#define AUDIO_SERIAL_DEBUG 1
#endif

// CubeSystem calls run(); other directors can call a named animation directly.
// All audio routines belong to this class. Each routine owns its own local
// state; no animation-specific variables are shared at file or class scope.
void Audio::run () {
    animatePuddles ();
}

void Audio::renderIcon (TextureState &) {
    for (uint8_t y = 0; y < RNDR_Y; ++y) {
        for (uint8_t x = 0; x < RNDR_X; ++x) {
            setVoxel (x, y, RNDR_Z / 2, CRGB::Blue);
        }
    }
}

// Convenience entry point for a routine that has not already read audio.
// Routines with a snapshot should use debug(audio) to describe the exact data
// they are drawing, without taking a second audio snapshot for the report.
void Audio::debug () {
    debug (RemoteInputs::audio ());
}

void Audio::debug (const RemoteInputs::Audio &audio) {
#if AUDIO_SERIAL_DEBUG
    // Render task only. No printing in UARTReceiver and no lock held during TX.
    // Static storage survives a nonlocal app exit; no resource-owning locals.
    static char text[640];
    static size_t queued = 0;
    static size_t sent = 0;
    static uint32_t lastReport = 0;
    const uint32_t now = millis ();

    if (sent >= queued && uint32_t (now - lastReport) >= 500) {
        lastReport = now;
        sent = 0;
        queued = 0;
        const RemoteInputs::Control control = RemoteInputs::control ();
        const RemoteInputs::Stats stats = RemoteInputs::stats ();
        const char *source = "Unknown";
        switch (control.mode) {
        case CubinaWire::Mode::AudioMic:
            source = "AudioMic";
            break;
        case CubinaWire::Mode::AudioBluetooth:
            source = "AudioBluetooth";
            break;
        case CubinaWire::Mode::AudioSD:
            source = "AudioSD";
            break;
        case CubinaWire::Mode::AudioDJ:
            source = "AudioDJ";
            break;
        case CubinaWire::Mode::Animation:
            source = "Animation";
            break;
        case CubinaWire::Mode::Game:
            source = "Game";
            break;
        case CubinaWire::Mode::Depth:
            source = "Depth";
            break;
        }
        const long age = audio.valid ? long (uint32_t (now - audio.receivedMs)) : -1;
        int n = snprintf (text, sizeof (text),
                          "Audio: source=%s linked=%u valid=%u session=%lu rev=%lu age=%ldms "
                          "VRaw=%u VSm=%u VAvg=%u Beats=%lu RXok=%lu RXbad=%lu\r\nBins:[",
                          source, unsigned (control.linked), unsigned (audio.valid),
                          (unsigned long)control.session, (unsigned long)control.revision, age,
                          unsigned (audio.values.volumeRaw), unsigned (audio.values.volumeSm),
                          unsigned (audio.values.sampleAvg), (unsigned long)audio.beatCount,
                          (unsigned long)stats.accepted, (unsigned long)stats.rejected);
        if (n <= 0 || size_t (n) >= sizeof (text)) {
            return;
        }
        queued = size_t (n);

        for (unsigned i = 0; i < 16; ++i) {
            n = snprintf (text + queued, sizeof (text) - queued, i == 15 ? "%u]\r\n" : "%u ",
                          unsigned (audio.values.fftResult[i]));
            if (n <= 0 || size_t (n) >= sizeof (text) - queued) {
                queued = 0;
                return;
            }
            queued += size_t (n);
        }
    }

    if (sent < queued) {
        const int space = Serial.availableForWrite ();
        if (space > 0) {
            size_t chunk = min (size_t (space), queued - sent);
            chunk = min (chunk, size_t (128));
            sent += Serial.write (reinterpret_cast<uint8_t *> (text + sent), chunk);
        }
    }
#endif
}

void Audio::animatePuddles () {
    // These control the visual effect, not the ADC gain or speaker loudness.
    constexpr uint32_t PUDDLE_SPAWN_INTERVAL_MS = 34; // at most about 29 lines/sec
    constexpr float PUDDLE_FADE_TIME_MS = 250.0f;     // brightness falls to ~37% per 250 ms
    constexpr uint8_t PUDDLE_MIN_VOLUME = 2;

    uint32_t lastSpawn = millis ();
    uint32_t lastFade = millis ();
    uint8_t hue = 0;

    for (;;) {
        const uint32_t now = millis ();
        const uint32_t elapsed = uint32_t (now - lastFade);
        if (elapsed > 0) {
            // Fade by elapsed time, so a slower render rate does not accumulate
            // more persistent bars simply because fewer frames were drawn.
            const float remaining = expf (-float (elapsed) / PUDDLE_FADE_TIME_MS);
            const uint8_t fade = uint8_t (min (255.0f, max (1.0f, 255.0f * (1.0f - remaining))));
            fadeAll (fade);
            lastFade = now;
        }

        const RemoteInputs::Audio audio = RemoteInputs::audio ();
        Audio::debug (audio);
        // Use the SAME volumeSm measurement as the command board's LED bar.
        // No spectrum bins or peak/kick flag participate in this effect.
        const uint8_t volume = audio.valid ? audio.values.volumeSm : 0;
        if (volume >= PUDDLE_MIN_VOLUME && uint32_t (now - lastSpawn) >= PUDDLE_SPAWN_INTERVAL_MS) {
            lastSpawn = now;
            const uint8_t y = random (RNDR_Y);
            const uint8_t z = random (RNDR_Z);
            const uint8_t length = 1 + uint16_t (volume) * (RNDR_X - 1) / 255;
            const uint8_t start = random (RNDR_X - length + 1);
            const CRGB color = CHSV (hue++, 220, 255);
            for (uint8_t x = start; x < start + length; ++x) {
                setVoxel (x, y, z, color);
            }
        }
        // Missing input stops spawning, while existing lines continue fading.
        showCube ();
    }
}
