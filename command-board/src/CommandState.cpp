#include "CommandState.h"

// SignalAcquisition is the writer; UARTSender copies the latest state.
// Hold the mutex only while copying/updating fields, never during I2S reads,
// UART writes, rendering, or source configuration. A mode revision is a control
// event; capturedMs changes only when a source change or publication records it.
namespace {
    SemaphoreHandle_t mutex = nullptr;
    CommandSnapshot state;
    uint32_t previous[2] = {};
} // namespace
void CommandState::begin() {
    mutex = xSemaphoreCreateMutex();
    configASSERT(mutex);
    state.session = esp_random();
}
CommandSnapshot CommandState::snapshot() {
    xSemaphoreTake(mutex, portMAX_DELAY);
    CommandSnapshot s = state;
    xSemaphoreGive(mutex);
    return s;
}
void CommandState::selectMode(CubinaWire::Mode m) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.mode = m;
    ++state.revision;
    state.ready = false;
    state.audio = {};
    state.pads[0] = GamepadState();
    state.pads[1] = GamepadState();
    state.presses[0] = 0;
    state.presses[1] = 0;
    previous[0] = 0;
    previous[1] = 0;
    memset(state.depth, 0, sizeof(state.depth));
    state.capturedMs = millis();
    xSemaphoreGive(mutex);
}
void CommandState::setReady(bool r) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.ready = r;
    xSemaphoreGive(mutex);
}
void CommandState::repeatModeDemand() {
    // This is a control event, not a source change. UARTSender sees the new
    // revision, while SignalAcquisition keeps filling the same audio stream.
    // Do not refresh capturedMs here: only new samples make old data fresh.
    xSemaphoreTake(mutex, portMAX_DELAY);
    ++state.revision;
    xSemaphoreGive(mutex);
}
void CommandState::publishAudio(const AudioPayload &a, uint32_t b) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    // Retain the strongest attack until an AUDIO transmission takes it.
    // Later quiet analysis windows must not erase an event between sends.
    const uint8_t strength = state.audio.attackStrength;
    const uint16_t bands = state.audio.attackBands;
    state.audio = a;
    if (strength > state.audio.attackStrength) {
        state.audio.attackStrength = strength;
    }
    state.audio.attackBands |= bands;
    state.beats = b;
    state.capturedMs = millis();
    state.ready = true;
    xSemaphoreGive(mutex);
}
void CommandState::publishGamepads(const GamepadState &a, const GamepadState &b) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    state.pads[0] = a;
    state.pads[1] = b;
    for (int i = 0; i < 2; ++i) {
        uint32_t now = CubinaWire::buttons(state.pads[i]);
        state.presses[i] |= now & ~previous[i];
        previous[i] = now;
    }
    state.capturedMs = millis();
    state.ready = true;
    xSemaphoreGive(mutex);
}
void CommandState::publishDepth(const uint8_t *bits) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    memcpy(state.depth, bits, sizeof(state.depth));
    state.capturedMs = millis();
    state.ready = true;
    xSemaphoreGive(mutex);
}
CommandSnapshot CommandState::takeForSend(bool consumeAudio) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    CommandSnapshot s = state;
    if (consumeAudio && CubinaWire::audioMode(state.mode)) {
        state.audio.attackStrength = 0;
        state.audio.attackBands = 0;
    }
    state.presses[0] = 0;
    state.presses[1] = 0;
    xSemaphoreGive(mutex);
    return s;
}
