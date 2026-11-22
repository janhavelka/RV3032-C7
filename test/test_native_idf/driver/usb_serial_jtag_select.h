#pragma once
#include <cstddef>
#include <cstdint>
#include "esp_err.h"

struct usb_serial_jtag_driver_config_t {
  uint32_t tx_buffer_size;
  uint32_t rx_buffer_size;
};
#define USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT() usb_serial_jtag_driver_config_t{256, 256}

size_t usb_serial_jtag_get_read_bytes_available();
inline bool usb_serial_jtag_is_driver_installed() { return true; }
inline esp_err_t usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t*) { return ESP_OK; }
