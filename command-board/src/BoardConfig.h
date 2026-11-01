#pragma once
// Numeric #if switches: 0 completely removes Bluepad32 headers/radio code.
#ifndef COMMAND_ENABLE_GAMEPAD_BLUETOOTH
#define COMMAND_ENABLE_GAMEPAD_BLUETOOTH 1
#endif
// Enable only with a compatible Bluepad32 Arduino-ESP32 2.x toolchain.
#define COMMAND_UART_RX 14
#define COMMAND_UART_TX 27
#define COMMAND_BTN_ANIMATION 18
#define COMMAND_BTN_MIC 19
#define COMMAND_BTN_ADC 32
#define COMMAND_BTN_GAME 25
#define COMMAND_BTN_DEPTH 33
// COMMAND_BTN_ADC is the single wired PCM1808/ADC input button.
// Mic and ADC buttons select their source; repeating the active button advances
// the render routine. There is no hold gesture or SD/DJ source cycling.
#define COMMAND_ACQUISITION_CORE 1
#define COMMAND_SENDER_CORE 0

// USB Serial diagnostics only. Never write text to binary Serial2.
#ifndef COMMAND_AUDIO_DEBUG
#define COMMAND_AUDIO_DEBUG 1
#endif
#ifndef COMMAND_AUDIO_SLOT_DEBUG
#define COMMAND_AUDIO_SLOT_DEBUG 1
#endif
