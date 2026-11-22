#pragma once
enum esp_line_endings_t { ESP_LINE_ENDINGS_LF };
inline void usb_serial_jtag_vfs_use_driver() {}
inline void usb_serial_jtag_vfs_set_rx_line_endings(esp_line_endings_t) {}
inline void usb_serial_jtag_vfs_set_tx_line_endings(esp_line_endings_t) {}
