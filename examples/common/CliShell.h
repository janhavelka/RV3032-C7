#pragma once

#include <Arduino.h>

#include "Log.h"

namespace cli_shell {

inline constexpr size_t MAX_LINE_LENGTH = 127U;
inline constexpr size_t MAX_INPUT_BYTES_PER_POLL = 256U;
inline size_t discardRemaining = 0U;

// Snapshot pending-time input without consuming another per-loop byte budget.
// The owner calls this at terminal completion before publishing its prompt.
inline void markPendingInputForDiscard() {
  const int available = LOG_SERIAL.available();
  discardRemaining = available > 0 ? static_cast<size_t>(available) : 0U;
}

// While busy, discard complete input and retain discard-until-newline state
// for a partial line, so its tail cannot become a command after completion.
inline bool readLine(String& outLine, bool discardInput = false) {
  static String buffer;
  static bool reserved = false;
  static bool overflowed = false;

  if (!reserved) {
    (void)buffer.reserve(MAX_LINE_LENGTH);
    reserved = true;
  }

  if (discardInput) {
    markPendingInputForDiscard();
    if (buffer.length() != 0U) {
      buffer = "";
      overflowed = true;
    }
  }
  for (size_t consumed = 0; consumed < MAX_INPUT_BYTES_PER_POLL &&
       LOG_SERIAL.available() > 0; ++consumed) {
    const char c = static_cast<char>(LOG_SERIAL.read());

    if (discardInput || discardRemaining != 0U) {
      // A job may finish before the bounded drain consumes its input snapshot.
      // Carry that backlog across completion without discarding newer lines.
      if (discardRemaining != 0U) --discardRemaining;
      overflowed = c != '\r' && c != '\n';
      continue;
    }

    if (c == '\b' || c == 0x7F) {
      if (!overflowed && buffer.length() > 0U) {
        buffer.remove(buffer.length() - 1U);
      }
      continue;
    }

    if (c == '\r' || c == '\n') {
      if (overflowed) {
        buffer = "";
        overflowed = false;
        continue;
      }
      if (buffer.length() == 0U) {
        continue;
      }
      outLine = buffer;
      buffer = "";
      outLine.trim();
      // Report a completed whitespace-only line to the owner as well. The
      // command handler treats it as a no-op, and loop() then repaints the
      // prompt instead of leaving the terminal on a blank line.
      return true;
    }

    if (overflowed) {
      continue;
    }

    if (buffer.length() < MAX_LINE_LENGTH) {
      buffer += c;
    } else {
      buffer = "";
      overflowed = true;
    }
  }
  return false;
}

}  // namespace cli_shell
