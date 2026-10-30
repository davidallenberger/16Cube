#include <Arduino.h>
#include "CommandState.h"
#include "SignalAcquisition.h"
#include "UARTSender.h"
void setup(){
    Serial.setTxBufferSize(1024);Serial.begin(115200);
    CommandState::begin();
    UARTSender::begin();
    SignalAcquisition::begin();
}
void loop(){delay(1000);} // tasks own all work; no second owner of UART or BP32
