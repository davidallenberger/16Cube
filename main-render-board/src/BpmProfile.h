#pragma once

#include <Arduino.h>
#include "UARTReceiver.h"

// Temporary investigation playlist and measurement hooks. Set to 0 to restore
// the usual Audio playlist and remove the per-frame measurement hooks.
#ifndef AUDIO_BPM_PROFILING
#define AUDIO_BPM_PROFILING 1
#endif

// CubeCore supplies this bridge because it owns the actual FastLED controllers.
// Driver counters are read only between shows, after the I2S ISR is stopped.
// A missing vendor probe reports available=false; it never fabricates zeros
// as evidence that interrupt deadlines were met.
struct BpmDriverStats {
    bool available = false;
    uint32_t cpuMHz = 0;
    uint32_t refills = 0;
    uint64_t fillCycles = 0;
    uint32_t maximumFillCycles = 0;
    uint32_t maximumGapCycles = 0;
    uint32_t gapsOver40us = 0;
    uint32_t maximumFirstCycles = 0;
    uint32_t firstOver40us = 0;
    uint32_t fillsOver30us = 0;
};

void bpmProfileStartDriver();
BpmDriverStats bpmProfileStopDriver();

// All storage below is owned by the render task. UARTReceiver's render-side
// audio() accessor records its own wait/hold durations, AFTER releasing its
// mutex. Core 0 and the I2S ISR never write this class's state.
class BpmProfile {
  public:
    enum class Test { Off, RealQuiet, RealDebug, SnapshotFixed, MathFixed };
    enum class Stage {
        Snapshot, Control, Debug, Draw, Checkpoint, RateWait, SpacerAudit, Led,
        SnapshotWait, SnapshotHold, Count
    };

    static const char *name(Test test) {
        switch (test) {
        case Test::RealQuiet: return "A real audio, quiet";
        case Test::RealDebug: return "B real audio, USB debug";
        case Test::SnapshotFixed: return "C snapshot, fixed pixels";
        case Test::MathFixed: return "D audio math, fixed pixels";
        default: return "normal";
        }
    }

    static bool active() {
        return state().active;
    }

    static void begin(Test test, bool debugCompiled) {
        State &s = state();
        s = State();
        if (test == Test::Off) {
            return;
        }

        const RemoteInputs::Control control = RemoteInputs::control();
        const RemoteInputs::Stats rx = RemoteInputs::stats();
        s.test = test;
        s.source = control.mode == CubinaWire::Mode::AudioMic ? "Mic" : "ADC";
        s.acceptedAtStart = rx.accepted;
        s.rejectedAtStart = rx.rejected;
        s.duplicatesAtStart = rx.duplicates;

        // Drain the entry message before measuring or showing LEDs. Quiet
        // cases have NO recurring USB formatting/writes during the capture.
#ifdef CUBINA_I2S_PROFILE_AVAILABLE
        const bool driverProbe = true;
#else
        const bool driverProbe = false;
#endif
        Serial.printf("\r\nBPM_PROFILE START case=%s source=%s USBcompiled=%u I2Sprobe=%u\r\n"
                      "Run your timer; the next mode-button press prints this report.\r\n",
                      name(test), s.source, unsigned(debugCompiled), unsigned(driverProbe));
        Serial.flush();
        bpmProfileStartDriver();
        s.previousEnd = micros();
        s.active = true;
    }

    static void beginFrame() {
        State &s = state();
        if (!s.active) {
            return;
        }
        s.pending = Frame();
        s.pending.start = micros();
        s.havePending = true;
    }

    static void record(Stage stage, uint32_t microseconds) {
        State &s = state();
        if (s.active && s.havePending) {
            s.pending.us[size_t(stage)] += microseconds;
        }
    }

    static void audioContext(bool valid, uint8_t volume, uint8_t tempoFlags = 0, uint16_t bpm100 = 0) {
        State &s = state();
        if (s.active && s.havePending) {
            s.pending.valid = valid;
            s.pending.volume = volume;
            s.pending.tempoFlags = tempoFlags;
            s.pending.bpm100 = bpm100;
        }
    }

    static void spacers(uint32_t nonblack) {
        State &s = state();
        if (s.active && s.havePending) {
            s.pending.nonblackSpacers = nonblack;
        }
    }

    static void completeFrame() {
        State &s = state();
        if (!s.active || !s.havePending) {
            return;
        }
        const uint32_t end = micros();
        s.elapsedUs += uint32_t(end - s.previousEnd);
        s.previousEnd = end;
        const uint32_t frameUs = uint32_t(end - s.pending.start);
        add(s.frame, frameUs);
        uint32_t accounted = 0;
        for (size_t i = 0; i < size_t(Stage::Count); ++i) {
            add(s.stages[i], s.pending.us[i]);
            if (i <= size_t(Stage::Led)) {
                accounted += s.pending.us[i];
            }
        }
        add(s.other, frameUs > accounted ? frameUs - accounted : 0);
        if (s.pending.valid) {
            ++s.validFrames;
            s.audioActiveFrames += bool(s.pending.tempoFlags & CubinaWire::AUDIO_ACTIVE);
            s.tempoLockedFrames += bool(s.pending.tempoFlags & CubinaWire::TEMPO_LOCKED);
            s.lastBpm100 = s.pending.bpm100;
        }
        if (s.pending.nonblackSpacers) {
            ++s.badSpacerFrames;
            s.maximumBadSpacers = max(s.maximumBadSpacers, s.pending.nonblackSpacers);
        }
        s.maximumVolume = max(s.maximumVolume, s.pending.volume);
        s.havePending = false;
    }

    static void finish() {
        State &s = state();
        if (!s.active) {
            return;
        }

        // The mode checkpoint can longjmp during an unfinished frame. Report
        // only COMPLETED frames; none of that partial frame's stage samples
        // have been committed. The previous FastLED.show has already returned.
        s.active = false;
        s.havePending = false;
        const BpmDriverStats driver = bpmProfileStopDriver();
        const RemoteInputs::Stats rx = RemoteInputs::stats();
        Serial.flush();
        const double seconds = double(s.elapsedUs) / 1000000.0;
        const double fps = seconds > 0 ? s.frame.count / seconds : 0;
        Serial.printf("\r\nBPM_PROFILE REPORT case=%s source=%s frames=%lu seconds=%.3f FPS=%.2f\r\n",
                      name(s.test), s.source, (unsigned long)s.frame.count, seconds, fps);
        print("Frame", s.frame);
        const char *names[] = {
            "AudioSnapshot", "ControlMath", "USBDebug", "VoxelDraw", "Checkpoint",
            "FrameCapWait", "SpacerAudit", "FastLEDshow", "SnapshotMutexWait", "SnapshotMutexHeld"
        };
        for (size_t i = 0; i < size_t(Stage::Count); ++i) {
            print(names[i], s.stages[i]);
        }
        print("OtherMeasuredFrameWork", s.other);
        Serial.printf("BPM_PROFILE inputs validFrames=%lu activeFrames=%lu lockedFrames=%lu "
                      "lastBPM=%.2f maxVSm=%u RXokDelta=%lu "
                      "RXbadDelta=%lu RXdupDelta=%lu spacerBadFrames=%lu maxBadSpacers=%lu\r\n",
                      (unsigned long)s.validFrames, (unsigned long)s.audioActiveFrames,
                      (unsigned long)s.tempoLockedFrames, double(s.lastBpm100) / 100.0,
                      unsigned(s.maximumVolume),
                      (unsigned long)uint32_t(rx.accepted - s.acceptedAtStart),
                      (unsigned long)uint32_t(rx.rejected - s.rejectedAtStart),
                      (unsigned long)uint32_t(rx.duplicates - s.duplicatesAtStart),
                      (unsigned long)s.badSpacerFrames, (unsigned long)s.maximumBadSpacers);

        if (driver.available && driver.cpuMHz) {
            const double fillUs = double(driver.fillCycles) / driver.cpuMHz;
            Serial.printf("BPM_PROFILE I2S probe=1 MHz=%lu refills=%lu FillAvg=%.3fus "
                          "FillMax=%.3fus IRQGapMax=%.3fus GapOver40us=%lu "
                          "FirstIRQMax=%.3fus FirstOver40us=%lu FillOver30us=%lu "
                          "FillWallShare=%.2f%%\r\n",
                          (unsigned long)driver.cpuMHz, (unsigned long)driver.refills,
                          driver.refills ? fillUs / driver.refills : 0,
                          double(driver.maximumFillCycles) / driver.cpuMHz,
                          double(driver.maximumGapCycles) / driver.cpuMHz,
                          (unsigned long)driver.gapsOver40us,
                          double(driver.maximumFirstCycles) / driver.cpuMHz,
                          (unsigned long)driver.firstOver40us,
                          (unsigned long)driver.fillsOver30us,
                          s.elapsedUs ? 100.0 * fillUs / s.elapsedUs : 0);
        } else {
            Serial.printf("BPM_PROFILE I2S probe=0 (driver timing unavailable)\r\n");
        }
        Serial.printf("BPM_PROFILE END\r\n");
    }

  private:
    struct Metric {
        uint64_t total = 0;
        uint32_t count = 0;
        uint32_t maximum = 0;
        uint32_t over1000 = 0;
        uint32_t over5000 = 0;
    };

    struct Frame {
        uint32_t start = 0;
        uint32_t us[size_t(Stage::Count)] = {};
        uint32_t nonblackSpacers = 0;
        bool valid = false;
        uint8_t volume = 0;
        uint8_t tempoFlags = 0;
        uint16_t bpm100 = 0;
    };

    struct State {
        bool active = false;
        bool havePending = false;
        Test test = Test::Off;
        const char *source = "?";
        uint32_t previousEnd = 0;
        uint64_t elapsedUs = 0;
        uint32_t acceptedAtStart = 0;
        uint32_t rejectedAtStart = 0;
        uint32_t duplicatesAtStart = 0;
        uint32_t validFrames = 0;
        uint32_t audioActiveFrames = 0;
        uint32_t tempoLockedFrames = 0;
        uint16_t lastBpm100 = 0;
        uint32_t badSpacerFrames = 0;
        uint32_t maximumBadSpacers = 0;
        uint8_t maximumVolume = 0;
        Frame pending;
        Metric frame;
        Metric other;
        Metric stages[size_t(Stage::Count)];
    };

    static State &state() {
        // Trivially destructible working state survives legacy longjmp.
        // There are no heap allocations, handles or locks to unwind here.
        static State s;
        return s;
    }

    static void add(Metric &metric, uint32_t value) {
        metric.total += value;
        ++metric.count;
        metric.maximum = max(metric.maximum, value);
        metric.over1000 += value > 1000;
        metric.over5000 += value > 5000;
    }

    static void print(const char *label, const Metric &metric) {
        const double average = metric.count ? double(metric.total) / metric.count : 0;
        Serial.printf("BPM_PROFILE %s avg=%.3fus max=%luus over1ms=%lu over5ms=%lu\r\n",
                      label, average, (unsigned long)metric.maximum,
                      (unsigned long)metric.over1000, (unsigned long)metric.over5000);
    }
};
