#include <Arduino.h>

#define UART_RX_PIN 14
#define UART_TX_PIN 27

unsigned long lastBeacon = 0;

void setup() {
    Serial.begin(115200); // USB to Mac
    
    //DESKTOP
    // SENDER CONFIG: RX = 14, TX = 27
    Serial2.begin(115200, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    
    Serial.println("Command Board Booted. Starting UART Transmission...");
}

void loop() {
    // Non-blocking 1-second timer
    if (millis() - lastBeacon > 1000) {
        Serial2.println("BEACON: Command Board is alive!");
        Serial.println("Beacon sent:" + String(millis()));
        lastBeacon = millis();
    }
}