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
#define COMMAND_BTN_LINE 32
#define COMMAND_BTN_GAME 25
#define COMMAND_BTN_DEPTH 33
// Extra dedicated source buttons are optional. -1 means not wired. The existing
// LINE button cycles Bluetooth -> SD -> DJ, so all modes work with five buttons.
#define COMMAND_BTN_SD -1
#define COMMAND_BTN_DJ -1
#define COMMAND_ACQUISITION_CORE 1
#define COMMAND_SENDER_CORE 0

// USB Serial diagnostics only. Binary Serial2 transmission remains unchanged.
#ifndef COMMAND_AUDIO_DEBUG
#define COMMAND_AUDIO_DEBUG 1
#endif
#ifndef COMMAND_AUDIO_SLOT_DEBUG
#define COMMAND_AUDIO_SLOT_DEBUG 1
#endif
