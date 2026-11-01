#pragma once

#include <Arduino.h>
#include <cstddef>
#include "CubeCore.h"

// Fixed shared scratch budget for animations on either cube.
// This is the same 8,192 bytes the original allocator actually reserved.
// Animation pools must fit this budget; cube dimensions do not increase it.
// Changing this value requires a separate memory-budget review and test.
constexpr size_t SHARED_APP_MEMORY_BYTES = 8192U;

// One byte per rendered voxel is the largest dimension-dependent scratch
// format used by the shared BPM phase map. Reject an unsupported future
// canvas at compile time rather than silently overrunning the arena.
static_assert(
    static_cast<size_t>(RNDR_X) * RNDR_Y * RNDR_Z <= SHARED_APP_MEMORY_BYTES,
    "The rendered voxel map does not fit the fixed shared app memory budget"
);

// The animation task owns this arena. Different apps reuse it in sequence;
// UART reception does not write it. It is allocated once during setup.
extern uint8_t* SharedAppMemory;

void initAppMemory();
