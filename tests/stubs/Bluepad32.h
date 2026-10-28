#pragma once
#include <stdint.h>
struct FakeController {
 bool isGamepad(){return true;}bool isConnected(){return true;}void disconnect(){}
 uint8_t dpad(){return 0;}int16_t axisX(){return 0;}int16_t axisY(){return 0;}
 bool a(){return false;}bool b(){return false;}bool x(){return false;}bool y(){return false;}
 bool l1(){return false;}bool l2(){return false;}bool r1(){return false;}bool r2(){return false;}
 uint8_t miscButtons(){return 0;}
};
using ControllerPtr=FakeController*;
struct BP32Stub{void setup(void(*)(ControllerPtr),void(*)(ControllerPtr)){}void update(){}};
extern BP32Stub BP32;
