#include <lmic.h>
#include <hal/hal.h>
#include <string.h>

void os_getArtEui (u1_t* buf) { memset(buf, 0, 8); }
void os_getDevEui (u1_t* buf) { memset(buf, 0, 8); }
void os_getDevKey (u1_t* buf) { memset(buf, 0, 16); }
