#include "AppMemory.h"

uint8_t* SharedAppMemory = nullptr;

void initAppMemory() {
    // Only allocate once at boot
    if (SharedAppMemory == nullptr) {
        #if RNDR_X > 8
            // 16x16x16 Desktop Cube
            SharedAppMemory = new uint8_t[29000];
        #else
            // 8x8x16 Burn Cube
            SharedAppMemory = new uint8_t[8192];
        #endif
        
        // Zero out the memory for safety
        #if RNDR_X > 8
            memset(SharedAppMemory, 0, 29000);
        #else
            memset(SharedAppMemory, 0, 8192);
        #endif
    }
}