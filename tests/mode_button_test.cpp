#include "ModeButton.h"
#include <cassert>
#include <cstdio>

static uint32_t clockMs = 0;
static int pinLevel = 1;

unsigned long millis() {
    return clockMs;
}

int digitalRead(int) {
    return pinLevel;
}

int main() {
    using namespace CommandButtons;
    ModeButton button(32);
    pinLevel = LOW;
    assert(button.poll() == Event::None);
    clockMs = 29;
    assert(button.poll() == Event::None);
    clockMs = 30;
    assert(button.poll() == Event::Press);
    clockMs = 1500;
    assert(button.poll() == Event::None);
    pinLevel = 1;
    assert(button.poll() == Event::None);
    clockMs += 30;
    assert(button.poll() == Event::None);
    pinLevel = LOW;
    assert(button.poll() == Event::None);
    clockMs += 30;
    assert(button.poll() == Event::Press);

    // Bounce does not create another press; a disabled pin remains inert.
    ModeButton bouncing(19);
    pinLevel = LOW;
    assert(bouncing.poll() == Event::None);
    clockMs += 5;
    pinLevel = 1;
    assert(bouncing.poll() == Event::None);
    clockMs += 5;
    pinLevel = LOW;
    assert(bouncing.poll() == Event::None);
    clockMs += 30;
    assert(bouncing.poll() == Event::Press);
    ModeButton disabled(-1);
    assert(disabled.poll() == Event::None);

    ModeButton wrapping(32);
    clockMs = 0xfffffff0u;
    assert(wrapping.poll() == Event::None);
    clockMs += 30;
    assert(wrapping.poll() == Event::Press);
    puts("Mode buttons passed: debounce, one-shot hold behavior, repeated presses, disabled input, clock wrap.");
}
