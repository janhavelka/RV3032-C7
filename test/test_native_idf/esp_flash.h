#pragma once
#include <cstdint>
#include "esp_err.h"
struct esp_flash_t;
inline esp_err_t esp_flash_get_physical_size(esp_flash_t*, uint32_t* bytes) {
  *bytes = 8U * 1024U * 1024U;
  return ESP_OK;
}
inline const char* esp_err_to_name(esp_err_t) { return "fake SDK error"; }
