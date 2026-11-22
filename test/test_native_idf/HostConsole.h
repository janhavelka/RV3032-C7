#pragma once

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#include "FakeIdf.h"

esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus, uint16_t address, int timeoutMs);

// Only app_main's fd setup uses these POSIX facilities. MinGW lacks them.
#ifdef _WIN32
constexpr int F_GETFL = 3, F_SETFL = 4, O_NONBLOCK = 0x800;
inline int fcntl(int, int, int) { return 0; }
#endif
