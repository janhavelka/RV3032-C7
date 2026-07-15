/**
 * @file CommandHandler.h
 * @brief Strict numeric token parsing for the interactive examples.
 *
 * NOT part of the library API. Every helper leaves its output unchanged when
 * the complete token is not valid in the requested domain.
 */

#pragma once

#include <Arduino.h>

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace cmd {
namespace detail {

inline bool hasWhitespace(const char* text) {
  if (text == nullptr || *text == '\0') {
    return true;
  }
  for (const unsigned char* p =
           reinterpret_cast<const unsigned char*>(text);
       *p != '\0'; ++p) {
    if (isspace(*p) != 0) {
      return true;
    }
  }
  return false;
}

inline bool parseUnsigned(const String& token, int base,
                          uint64_t maximum, uint64_t& parsed) {
  const char* text = token.c_str();
  if (hasWhitespace(text) || text[0] == '-') {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const unsigned long long value = strtoull(text, &end, base);
  if (errno == ERANGE || end == text || *end != '\0' || value > maximum) {
    return false;
  }
  parsed = static_cast<uint64_t>(value);
  return true;
}

}  // namespace detail

inline bool parseU8Token(const String& token, uint8_t& out) {
  uint64_t parsed = 0;
  if (!detail::parseUnsigned(token, 10, UINT8_MAX, parsed)) {
    return false;
  }
  out = static_cast<uint8_t>(parsed);
  return true;
}

inline bool parseU16Token(const String& token, uint16_t& out) {
  uint64_t parsed = 0;
  if (!detail::parseUnsigned(token, 10, UINT16_MAX, parsed)) {
    return false;
  }
  out = static_cast<uint16_t>(parsed);
  return true;
}

inline bool parseU32Token(const String& token, uint32_t& out) {
  uint64_t parsed = 0;
  if (!detail::parseUnsigned(token, 10, UINT32_MAX, parsed)) {
    return false;
  }
  out = static_cast<uint32_t>(parsed);
  return true;
}

inline bool parseInt32Token(const String& token, int32_t& out) {
  const char* text = token.c_str();
  if (detail::hasWhitespace(text)) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const long long parsed = strtoll(text, &end, 10);
  if (errno == ERANGE || end == text || *end != '\0' ||
      parsed < INT32_MIN || parsed > INT32_MAX) {
    return false;
  }
  out = static_cast<int32_t>(parsed);
  return true;
}

inline bool parseFloatToken(const String& token, float& out) {
  const char* text = token.c_str();
  if (detail::hasWhitespace(text)) {
    return false;
  }
  const char* unsignedText = text;
  if (*unsignedText == '+' || *unsignedText == '-') {
    ++unsignedText;
  }
  if (unsignedText[0] == '0' &&
      (unsignedText[1] == 'x' || unsignedText[1] == 'X')) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const double parsed = strtod(text, &end);
  const double magnitude = std::fabs(parsed);
  if (errno == ERANGE || end == text || *end != '\0' ||
      !std::isfinite(parsed) ||
      magnitude > static_cast<double>(std::numeric_limits<float>::max()) ||
      (magnitude != 0.0 &&
       magnitude < static_cast<double>(std::numeric_limits<float>::denorm_min()))) {
    return false;
  }
  out = static_cast<float>(parsed);
  return true;
}

inline bool parseBool01Token(const String& token, bool& out) {
  uint8_t parsed = 0;
  if (!parseU8Token(token, parsed) || parsed > 1U) {
    return false;
  }
  out = parsed == 1U;
  return true;
}

inline bool parseRegisterToken(const String& token, uint8_t& out) {
  const char* text = token.c_str();
  if (detail::hasWhitespace(text) || text[0] == '-') {
    return false;
  }
  int base = 10;
  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    if (text[2] == '\0') {
      return false;
    }
    base = 16;
  }
  uint64_t parsed = 0;
  if (!detail::parseUnsigned(token, base, UINT8_MAX, parsed)) {
    return false;
  }
  out = static_cast<uint8_t>(parsed);
  return true;
}

}  // namespace cmd
