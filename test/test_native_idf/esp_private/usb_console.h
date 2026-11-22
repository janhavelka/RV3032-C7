#pragma once
#include <sys/types.h>

void esp_usb_console_poll_interrupts();
ssize_t esp_usb_console_available_for_read();
bool esp_usb_console_write_available();
ssize_t esp_usb_console_write_buf(const char* text, size_t length);
