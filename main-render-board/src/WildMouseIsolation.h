#pragma once

#include "WildMouseTestConfig.h"

#if CUBINA_WILD_MOUSE_ISOLATION

// Streamers currently relies on its caller declaring GeometryEngine first.
// Supply that dependency on this new entry path too. Streamers can also own
// the include directly; #pragma once prevents a duplicate definition.
#include "GeometryEngine.h"
#include "Streamers.h"
#include "WildMouseArena.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Temporary app-entry test. The normal CubeSystem checkpoint and showCube
// remain in use. Diagnostics run only before/after a complete scene returns,
// never inside a frame loop or FastLED's interrupt/refill path.
namespace WildMouseIsolation {
    inline const char* primerName() {
#if CUBINA_WILD_MOUSE_FIREWORKS_FIRST && CUBINA_WILD_MOUSE_PIPES_FIRST
        return "Fireworks+Pipes";
#elif CUBINA_WILD_MOUSE_FIREWORKS_FIRST
        return "Fireworks";
#elif CUBINA_WILD_MOUSE_PIPES_FIRST
        return "Pipes";
#else
        return "none";
#endif
    }

    inline bool report(const char* boundary, unsigned int pass) {
        // The RTOS reports the lifetime MINIMUM unused stack, not the current
        // unused stack. ESP-IDF's API reports bytes, unlike vanilla FreeRTOS.
        // This can establish pressure; a nonzero value does not prove that
        // no transient corruption has happened.
#if INCLUDE_uxTaskGetStackHighWaterMark
        const unsigned int stackMinimum = uxTaskGetStackHighWaterMark(nullptr);
#else
        const unsigned int stackMinimum = 0;
#endif

        // Heap checks cover the allocator's configured integrity checks.
        // A passing result does not prove all animation arrays stayed inside
        // bounds. In particular, this is not a check of every stack variable.
        const bool heapOkay = heap_caps_check_integrity_all(true);
        const size_t freeBytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        const size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

        Serial.printf(
            "WILD_TEST: pass=%u boundary=%s storage=%s primer=%s core=%u "
            "arenaBytes=%u stateBytes=%u stackMinBytes=%u stackMetric=%s "
            "heapOK=%u heapFree=%u heapLargest=%u\n",
            pass,
            boundary,
            CUBINA_WILD_MOUSE_ARENA ? "arena" : "original-stack",
            primerName(),
            static_cast<unsigned int>(xPortGetCoreID()),
            static_cast<unsigned int>(SHARED_APP_MEMORY_BYTES),
            static_cast<unsigned int>(WildMouseArena::stateBytes()),
            stackMinimum,
#if INCLUDE_uxTaskGetStackHighWaterMark
            "available",
#else
            "unavailable",
#endif
            heapOkay ? 1U : 0U,
            static_cast<unsigned int>(freeBytes),
            static_cast<unsigned int>(largestBlock)
        );

        return heapOkay;
    }

    inline void stopOnHeapFailure() {
        Serial.println("WILD_TEST: STOP -- heap check failed; no next scene will run.");

        // Preserve the log and stop advancing scenes. Do not draw or jump
        // through other apps after detecting damaged heap state.
        for (;;) {
            delay(1000);
        }
    }

    inline void runPass(unsigned int pass) {
        if (!report("before", pass)) {
            stopOnHeapFailure();
        }

#if CUBINA_WILD_MOUSE_FIREWORKS_FIRST
        // Separate optional test of the user's preceding-scene hypothesis.
        // Keep this OFF for the first cold Wild Mouse comparison.
        Streamers::animateFireworks(CUBINA_WILD_MOUSE_FIREWORKS_MS);

        if (!report("after-fireworks", pass)) {
            stopOnHeapFailure();
        }
#endif

#if CUBINA_WILD_MOUSE_PIPES_FIRST
        // This is the immediate predecessor of Wild Mouse in the ordered
        // desktop TestDemo playlist. When both primer flags are set, preserve
        // that playlist order: Fireworks, then Pipes, then Wild Mouse.
        // 15 ms is the existing desktop SPEED_SCENE_PIPES setting.
        Streamers::animateVerticalPipes(CUBINA_WILD_MOUSE_PIPES_MS, 15);

        if (!report("after-pipes", pass)) {
            stopOnHeapFailure();
        }
#endif

#if CUBINA_WILD_MOUSE_ARENA
        WildMouseArena::animate(CUBINA_WILD_MOUSE_RUN_MS, 25);
#else
        // Use the current original method, without rewriting its algorithm
        // or changing its arrays, speed, rendering or per-frame delay.
        Streamers::animateWildMouse(CUBINA_WILD_MOUSE_RUN_MS, 25);
#endif

        if (!report("after-mouse", pass)) {
            stopOnHeapFailure();
        }
    }

    inline void run() {
        // No TestDemo scenes precede the FIRST pass after reboot. Later passes
        // repeat Wild Mouse; only the first pass is a cold scene-entry test.
        for (unsigned int pass = 1; ; pass++) {
            runPass(pass);
        }
    }
}

#endif
