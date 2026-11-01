#pragma once

#include "PlaylistMemoryProbeConfig.h"

#if CUBINA_PLAYLIST_MEMORY_DIAGNOSTICS

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// This probe is owned ONLY by the animation task. There is no mutex, new task,
// heap allocation, ISR hook or LED-driver change. All observations are stored
// in one small static record; formatting happens only AFTER Wild Mouse returns.
namespace PlaylistMemoryProbe {
    struct StackReading {
        uint32_t lifetimeMin;
        uint32_t currentGap;
    };

    struct HeapReading {
        uint32_t freeBytes;
        uint32_t largestBlock;
        uint32_t dmaFreeBytes;
        uint32_t dmaLargestBlock;
        uint32_t integrityOkay;
    };

    struct Record {
        StackReading before;
        StackReading initialized;
        StackReading leaving;
        StackReading returned;
        HeapReading heapBefore;
        HeapReading heapAfter;
        uint32_t run;
        uint32_t startedMs;
        uint32_t elapsedMs;
        uint32_t arrayBytes;
        bool active;
        bool initializedSeen;
        bool leavingSeen;
        bool returnedNormally;
    };

    inline Record& record() {
        // Zero-initialized static storage, NOT a record placed on the task's
        // stack and NOT another heap buffer. sizeof(Record) is printed so its
        // added memory cost is explicit (92 bytes on the target layout).
        // An inline function's local static is shared across translation units.
        static Record value = {};
        return value;
    }

    inline void readStack(StackReading& target) {
#if INCLUDE_uxTaskGetStackHighWaterMark
        // ESP-IDF 4.4 reports the task's lifetime MINIMUM unused stack in
        // bytes. Earlier scenes can set this minimum before Wild Mouse starts.
        target.lifetimeMin = uxTaskGetStackHighWaterMark(nullptr);
#else
        target.lifetimeMin = 0;
#endif

        target.currentGap = 0;

#if defined(__XTENSA__) && INCLUDE_pxTaskGetStackStart
        // Read the task's current stack pointer and its lowest allocated
        // address. This is an instantaneous distance at THIS checkpoint,
        // not a measurement of the deepest nested drawing call.
        // This runs on the normal animation task, never in the I2S ISR.
        const uintptr_t base = reinterpret_cast<uintptr_t>(pxTaskGetStackStart(nullptr));
        uintptr_t stackPointer;
        __asm__ __volatile__("mov %0, a1" : "=a"(stackPointer) : : "memory");

        if (base != 0 && stackPointer >= base) {
            target.currentGap = static_cast<uint32_t>(stackPointer - base);
        }
#endif
    }

    inline void readHeap(HeapReading& target) {
        // Heap integrity checks and allocator queries run only outside the
        // animation routine. Passing checks do not certify every object or
        // stack boundary. They use the SDK's currently configured checks.
        target.integrityOkay = heap_caps_check_integrity_all(true) ? 1U : 0U;
        target.freeBytes = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        target.largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        target.dmaFreeBytes = heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        target.dmaLargestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    }

    inline void begin() {
        Record& value = record();
        value.active = false;
        value.run++;
        value.arrayBytes = 0;
        value.initializedSeen = false;
        value.leavingSeen = false;
        value.returnedNormally = false;
        value.initialized.lifetimeMin = 0;
        value.initialized.currentGap = 0;
        value.leaving.lifetimeMin = 0;
        value.leaving.currentGap = 0;

        readHeap(value.heapBefore);
        readStack(value.before);
        value.startedMs = millis();
        value.active = true;
    }

    inline void mouseInitialized(size_t arrayBytes) {
        Record& value = record();

        if (value.active) {
            // Called once after the ORIGINAL arrays are initialized, in the
            // ORIGINAL Wild Mouse function and the REAL playlist caller chain.
            value.arrayBytes = static_cast<uint32_t>(arrayBytes);
            readStack(value.initialized);
            value.initializedSeen = true;
        }
    }

    inline void mouseLeaving() {
        Record& value = record();

        if (value.active) {
            // Called once after the normal loop, before the original arrays
            // leave scope. Source-level keepAlive barriers follow this call.
            readStack(value.leaving);
            value.leavingSeen = true;
        }
    }

    inline void keepAlive(const void* storage) {
        // An empty compiler barrier with the array address as input keeps
        // its storage alive past the leaving checkpoint. It reads no array
        // data, performs no I/O and adds no runtime function call when inlined.
        // This prevents a tail-call optimization from measuring only AFTER
        // the compiler has already released Wild Mouse's large stack frame.
#if defined(__GNUC__)
        __asm__ __volatile__("" : : "r"(storage) : "memory");
#else
        static_cast<void>(storage);
#endif
    }

    inline void printStack(const char* boundary, const StackReading& reading) {
        Serial.print("PLAYLIST_MEMORY_STACK: boundary=");
        Serial.print(boundary);
        Serial.print(" lifetimeMin=");
        Serial.print(static_cast<unsigned long>(reading.lifetimeMin));
        Serial.print(" currentGap=");
        Serial.println(static_cast<unsigned long>(reading.currentGap));
    }

    inline void printHeap(const char* boundary, const HeapReading& reading) {
        Serial.print("PLAYLIST_MEMORY_HEAP: boundary=");
        Serial.print(boundary);
        Serial.print(" heapOK=");
        Serial.print(static_cast<unsigned long>(reading.integrityOkay));
        Serial.print(" free=");
        Serial.print(static_cast<unsigned long>(reading.freeBytes));
        Serial.print(" largest=");
        Serial.print(static_cast<unsigned long>(reading.largestBlock));
        Serial.print(" dmaFree=");
        Serial.print(static_cast<unsigned long>(reading.dmaFreeBytes));
        Serial.print(" dmaLargest=");
        Serial.println(static_cast<unsigned long>(reading.dmaLargestBlock));
    }

    inline void finish() {
        Record& value = record();

        if (!value.active) {
            return;
        }

        value.elapsedMs = uint32_t(millis() - value.startedMs);
        readStack(value.returned);
        readHeap(value.heapAfter);
        value.returnedNormally = true;
        value.active = false;

        // Every line is printed HERE, in TestDemo, after the routine returns.
        // The stored stack observations were made earlier while it was live.
        // A longjmp/crash skips finish(); no destructor/held lock is involved.
        // Use small print operations instead of constructing a long formatted
        // string. The diagnostic adds no temporary heap formatting buffer.
        Serial.print("PLAYLIST_MEMORY: run=");
        Serial.print(static_cast<unsigned long>(value.run));
        Serial.print(" routine=WildMouse elapsedMs=");
        Serial.print(static_cast<unsigned long>(value.elapsedMs));
        Serial.print(" arrays=");
        Serial.print(static_cast<unsigned long>(value.arrayBytes));
        Serial.print(" diagStaticBytes=");
        Serial.print(static_cast<unsigned int>(sizeof(Record)));
        Serial.print(" initialized=");
        Serial.print(value.initializedSeen ? 1U : 0U);
        Serial.print(" leaving=");
        Serial.print(value.leavingSeen ? 1U : 0U);
        Serial.print(" returned=");
        Serial.print(value.returnedNormally ? 1U : 0U);
        Serial.print(" hwmMetric=");
#if INCLUDE_uxTaskGetStackHighWaterMark
        Serial.print("bytes");
#else
        Serial.print("unavailable");
#endif
        Serial.print(" gapMetric=");
#if defined(__XTENSA__) && INCLUDE_pxTaskGetStackStart
        Serial.println("bytes");
#else
        Serial.println("unavailable");
#endif

        printStack("before-call", value.before);
        printStack("arrays-initialized", value.initialized);
        printStack("before-return", value.leaving);
        printStack("after-return", value.returned);
        printHeap("before-call", value.heapBefore);
        printHeap("after-return", value.heapAfter);
        Serial.println("PLAYLIST_MEMORY_END");
    }
}

#else

// No SDK queries, static record or output in a disabled-probe comparison.
namespace PlaylistMemoryProbe {
    inline void begin() {}
    inline void finish() {}
}

#endif
