// Compile the shipped example adapter as its own translation unit against the fake SDK.
#include <unistd.h>
#include "FakeIdf.h"
#define usleep fakeIdf::sleepUs
#include "examples/esp_idf/basic/main/Rv3032IdfI2c.cpp"
#undef usleep
