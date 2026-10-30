#include "UARTSender.cpp"
#include <assert.h>
#include <stdio.h>

static uint32_t clockMs = 0;
unsigned long millis () {
    return clockMs;
}

int main () {
    SendTiming::Deadline deadline;
    unsigned sent = 0;

    // A task waking every millisecond still averages 60 sends per second;
    // small wake-up delays do not accumulate into a permanently slower rate.
    for (uint32_t now = 0; now < 1000000; now += 1000) {
        if (deadline.due (now)) {
            ++sent;
            deadline.advance (now, SendTiming::AudioPeriodUs);
        }
    }
    assert (sent == 60);

    // A long stall sends only current data, then schedules the NEXT slot.
    assert (deadline.due (5000000));
    deadline.advance (5000000, SendTiming::AudioPeriodUs);
    assert (!deadline.due (5000000));
    assert (!deadline.due (5016666));
    assert (deadline.due (5016667));

    // micros() wraps about every 71 minutes. A deadline spanning that wrap
    // must remain pending before wrap and become due at the correct time.
    deadline.restart (0xfffffff0u, 100);
    assert (!deadline.due (0xfffffff0u));
    assert (!deadline.due (83));
    assert (deadline.due (84));

    assert (dataPeriod (CubinaWire::Mode::AudioMic) == 16667);
    assert (dataPeriod (CubinaWire::Mode::AudioBluetooth) == 16667);
    assert (dataPeriod (CubinaWire::Mode::AudioSD) == 16667);
    assert (dataPeriod (CubinaWire::Mode::AudioDJ) == 16667);
    assert (dataPeriod (CubinaWire::Mode::Game) == 16667);
    assert (dataPeriod (CubinaWire::Mode::Depth) == 33334);

    // Exercise production packet construction and serialization. The wire
    // layout stays compatible; in particular, kick count survives even when
    // the transient beat flag was cleared in a later analysis block.
    CommandSnapshot source;
    source.mode = CubinaWire::Mode::AudioBluetooth;
    source.session = 123;
    source.revision = 7;
    source.ready = true;
    source.audio.mode = 2;
    source.audio.beat = 0;
    source.audio.volumeRaw = 90;
    source.audio.volumeSm = 80;
    source.audio.fftResult[5] = 200;
    source.beats = 19;

    CubinaWire::Packet packet = makePacket (source, true, false, 42);
    assert (packet.type == CubinaWire::Type::Audio);
    assert (packet.length == 25 && packet.sequence == 42);
    uint8_t bytes[CubinaWire::MAX_FRAME];
    const size_t length = CubinaWire::encode (packet, bytes, sizeof (bytes));
    assert (length && length <= 51);
    CubinaWire::Packet decoded;
    assert (CubinaWire::decode (bytes, length - 1, decoded));
    assert (CubinaWire::readAudio (decoded).fftResult[5] == 200);
    assert (CubinaWire::get32 (decoded.payload + 21) == 19);

    packet = makePacket (source, true, true, 43);
    assert (packet.type == CubinaWire::Type::Status);
    assert (packet.mode == source.mode && packet.modeRevision == 7);

    source.mode = CubinaWire::Mode::Game;
    source.pads[0].connected = true;
    source.presses[0] = 1;
    packet = makePacket (source, false, true, 44);
    assert (packet.type == CubinaWire::Type::Gamepads);
    assert (CubinaWire::get32 (packet.payload + 30) == 1);

    source.capturedMs = 0;
    clockMs = 251;
    assert (!freshSource (source));
    puts ("Sender tests passed: 60 Hz cadence, no catch-up burst, clock wrap, mode periods, packet "
          "compatibility, kick count, game taps, stale source.");
}
