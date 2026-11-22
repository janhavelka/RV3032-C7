#pragma once
#include <cstdint>
enum esp_chip_model_t { CHIP_ESP32S2 = 2, CHIP_ESP32S3 = 9 };
struct esp_chip_info_t { esp_chip_model_t model; uint16_t revision; };
inline void esp_chip_info(esp_chip_info_t* chip) { *chip = {CHIP_ESP32S3, 101}; }
