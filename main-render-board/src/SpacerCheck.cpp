#include "SpacerCheck.h"

#if CUBINA_CHECK_SPACERS

#include <Arduino.h>
#include <stdio.h>

namespace SpacerCheck {
    namespace {
        // There is one shared record because only the animation task draws.
        // No UART task or interrupt reads or writes these diagnostic values.
        struct SceneRecord {
            SceneInfo info;
            char paletteName[48];
            const char *previousName = "<boot>";

            uint32_t run = 0;           // Monotonic scene invocation number.
            uint32_t startedMs = 0;     // Time this invocation began drawing.
            uint32_t frames = 0;        // Actual pre-show inspections.
            uint32_t badFrames = 0;     // Inspections with ANY nonblack spacer.
            uint32_t badSamples = 0;    // Nonblack spacer observations, not unique LEDs.
            uint32_t firstBadFrame = 0; // One-based frame number of the first observation.
            uint32_t maxScanUs = 0;     // Longest measured sparse scan, including interruptions.

            uint16_t spacers = 0;       // Positions inspected per frame: 265 on desktop.
            uint16_t firstIndex = 0;    // Zero-based physical index within its output.
            uint8_t firstOutput = 0;    // Zero-based output number, 0 through 7.
            uint8_t red = 0;            // Unscaled CRGB value at first bad spacer.
            uint8_t green = 0;
            uint8_t blue = 0;
        };

        SceneRecord record;
        bool active = false;
        uint32_t runNumber = 0;
        const char *lastSceneName = "<boot>";

        void printSummary(const char *status) {
            // This function is called only after the drawing loop returned,
            // or before a replacement scene starts. It is never called from
            // showCube(), the DMA interrupt, or the sparse scan itself.
            char first[96];

            if (record.badFrames == 0) {
                snprintf(first, sizeof(first), "none");
            } else {
                snprintf(first, sizeof(first),
                         "out=%u,index=%u,frame=%lu,rgb=(%u,%u,%u)",
                         unsigned(record.firstOutput), unsigned(record.firstIndex),
                         static_cast<unsigned long>(record.firstBadFrame),
                         unsigned(record.red), unsigned(record.green), unsigned(record.blue));
            }

            Serial.printf(
                "SPACER_CHECK: run=%lu scene=\"%s\" previous=\"%s\" "
                "order=%s step=%u/%u render=\"%s\" motion=\"%s\" "
                "shader=\"%s\" palette=\"%s\" frames=%lu spacers=%u "
                "badFrames=%lu badSamples=%lu first=[%s] scanMax=%luus "
                "elapsed=%lums UART=0 DMA=%u diagBytes=%u status=%s\n",
                static_cast<unsigned long>(record.run), record.info.name, record.previousName,
                record.info.ordered ? "Ordered" : "Random", unsigned(record.info.step),
                unsigned(record.info.totalSteps), record.info.render, record.info.motion,
                record.info.shader, record.paletteName,
                static_cast<unsigned long>(record.frames), unsigned(record.spacers),
                static_cast<unsigned long>(record.badFrames),
                static_cast<unsigned long>(record.badSamples), first,
                static_cast<unsigned long>(record.maxScanUs),
                static_cast<unsigned long>(uint32_t(millis() - record.startedMs)),
                unsigned(FASTLED_ESP32_I2S_NUM_DMA_BUFFERS),
                unsigned(sizeof(record) + sizeof(active) + sizeof(runNumber) + sizeof(lastSceneName)),
                status);
        }
    }

    void beginScene(const SceneInfo &info) {
        // longjmp does not unwind C++ objects. If an app was abandoned, the
        // next dispatcher entry can still close its record explicitly. There
        // is no destructor-based logging that a jump could accidentally skip.
        if (active) {
            active = false;
            printSummary("interrupted");
        }

        record.info = info;
        snprintf(record.paletteName, sizeof(record.paletteName), "%s", info.palette);
        record.previousName = lastSceneName;
        record.run = ++runNumber;
        record.startedMs = millis();
        record.frames = 0;
        record.badFrames = 0;
        record.badSamples = 0;
        record.firstBadFrame = 0;
        record.maxScanUs = 0;
        record.spacers = 0;
        record.firstIndex = 0;
        record.firstOutput = 0;
        record.red = 0;
        record.green = 0;
        record.blue = 0;

        lastSceneName = info.name;
        active = true;
    }

    bool sceneActive() {
        return active;
    }

    void recordFrame(const FrameScan &scan, uint32_t scanUs) {
        ++record.frames;
        record.spacers = scan.checked;

        if (scanUs > record.maxScanUs) {
            record.maxScanUs = scanUs;
        }

        if (scan.nonblack == 0) {
            return;
        }

        if (record.badFrames == 0) {
            record.firstBadFrame = record.frames;
            record.firstOutput = scan.firstOutput;
            record.firstIndex = scan.firstIndex;
            record.red = scan.red;
            record.green = scan.green;
            record.blue = scan.blue;
        }

        ++record.badFrames;
        record.badSamples += scan.nonblack;
    }

    void endScene() {
        if (!active) {
            return;
        }

        active = false;
        printSummary("complete");
    }
}

#endif
