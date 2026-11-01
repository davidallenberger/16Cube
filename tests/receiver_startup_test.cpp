#define CUBINA_TEST_TASK_LIFECYCLE
#include "UARTReceiver.cpp"
#include <cassert>
#include <cstdio>

SerialStub Serial;
SerialStub Serial2;

static int runningCore = 1;
static int assignedCore = -1;
static void (*receiverEntry)(void *) = nullptr;

struct StopReceiver {
};

unsigned long millis() {
    return 0;
}

int xPortGetCoreID() {
    return runningCore;
}

BaseType_t testCreatePinnedTask(void (*entry)(void *), int core) {
    receiverEntry = entry;
    assignedCore = core;
    return pdPASS;
}

void testTaskDelay(unsigned) {
    // Stop the production infinite loop after its first iteration. Its UART
    // initialization and parser setup have run in the simulated task context.
    throw StopReceiver();
}

int main() {
    // CubeSystem starts the receiver from the animation core. This call must
    // create storage and the task, without installing the UART driver here.
    RemoteInputs::begin();
    assert(receiverEntry != nullptr);
    assert(assignedCore == 0);
    assert(Serial2.beginCalls == 0);
    assert(Serial2.rxBufferSize == 0);
    assert(!RemoteInputs::control().linked);
    assert(!RemoteInputs::audio().valid);

    runningCore = assignedCore;
    try {
        receiverEntry(nullptr);
        assert(false && "Receiver task unexpectedly returned");
    } catch (const StopReceiver &) {
    }

    assert(Serial2.beginCalls == 1);
    assert(Serial2.rxBufferSize == 8192);

    // A repeated begin must leave the running driver/task alone.
    runningCore = 1;
    RemoteInputs::begin();
    assert(Serial2.beginCalls == 1);

    puts("Receiver startup passed: UART installation occurs inside the Core 0 task.");
}
