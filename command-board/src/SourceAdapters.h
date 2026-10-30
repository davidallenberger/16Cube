#pragma once
#include <Arduino.h>
#include "SharedProtocol.h"
// Called only by SignalAcquisition. Override these weak functions in a separate
// .cpp for your chosen SD decoder/DAC, analog relay, and depth-camera interface.
// No Serial2 calls here. Keep work bounded; return false if no new frame.
namespace SourceAdapters {
bool select(CubinaWire::Mode mode);
// Signed upper-aligned stereo int32 PCM at 31,250 Hz. The SD implementation
// must deliver exactly `frames` pairs per true result (resample if necessary).
bool readSD(int32_t* interleaved,size_t frames);
// One bit/voxel; index x+16*(y+16*z), bit 0 first, x fastest.
bool readDepth(uint8_t* occupancy512);
}
