#include "SourceAdapters.h"
bool __attribute__((weak)) SourceAdapters::select(CubinaWire::Mode m){
    // Bluetooth receiver and DJ deck use the SAME existing PCM1808 input.
    // This does not switch an unwired relay: plug in the desired source or
    // override this function to control your selector and report its readiness.
    return m!=CubinaWire::Mode::AudioSD && m!=CubinaWire::Mode::Depth;
}
bool __attribute__((weak)) SourceAdapters::readSD(int32_t*,size_t){return false;}
bool __attribute__((weak)) SourceAdapters::readDepth(uint8_t*){return false;}
