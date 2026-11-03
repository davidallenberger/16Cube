#include "AppMemory.h"

#include <cstdlib>
#include <cstring>
#include <new>

uint8_t* SharedAppMemory = nullptr;

void initAppMemory() {
    // Allocate once at boot. Repeated setup calls must keep the same arena
    // and must not erase the current animation's state.
    if (SharedAppMemory != nullptr) {
        return;
    }

    // Keep the actual original 8 KB allocation, including on the 16x16x16
    // cube. The old "#if RNDR_X > 8" could not read a C++ constexpr and always
    // selected 8 KB. We now make that budget explicit and limit its users.
    SharedAppMemory = new (std::nothrow) uint8_t[SHARED_APP_MEMORY_BYTES];

    // Stop before any animation dereferences a missing arena. Do not fall
    // back to fewer bytes while callers still assume the published capacity.
    if (SharedAppMemory == nullptr) {
        Serial.printf(
            "AppMemory: failed to allocate %u bytes; stopping startup.\n",
            static_cast<unsigned int>(SHARED_APP_MEMORY_BYTES)
        );
        std::abort();
    }

    std::memset(SharedAppMemory, 0, SHARED_APP_MEMORY_BYTES);

    // One startup message only. No frame-loop or interrupt instrumentation.
    Serial.printf(
        "AppMemory: allocated %u bytes for %ux%ux%u render dimensions.\n",
        static_cast<unsigned int>(SHARED_APP_MEMORY_BYTES),
        static_cast<unsigned int>(RNDR_X),
        static_cast<unsigned int>(RNDR_Y),
        static_cast<unsigned int>(RNDR_Z)
    );
}
