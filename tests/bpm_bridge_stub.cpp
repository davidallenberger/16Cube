#include "BpmProfile.h"
static uint8_t scratch[8192];
uint8_t *SharedAppMemory = scratch;
void bpmProfileStartDriver() {}
BpmDriverStats bpmProfileStopDriver() { return BpmDriverStats(); }
