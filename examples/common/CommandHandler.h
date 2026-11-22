/**
 * @file CommandHandler.h
 * @brief Strict numeric token parsing for the interactive examples.
 *
 * NOT part of the library API. Every helper leaves its output unchanged when
 * the complete token is not valid in the requested domain.
 */

#pragma once

#include <cerrno>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace cmd {

// The editor counts physical bytes. NUL inside a token ends its C-string
// value, but only whitespace separates it from the next token.
inline char* nextToken(char*& cursor, const char* end) {
  while (cursor != end && std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
  if (cursor == end) return nullptr;
  char* token = cursor;
  while (cursor != end && !std::isspace(static_cast<unsigned char>(*cursor))) ++cursor;
  if (cursor != end) *cursor++ = '\0';
  return token;
}

// Split in place; returned pointers borrow the buffer until its next edit.
// An extra token reports capacity + 1, so callers can reject excess arguments.
inline size_t splitTokens(char* text, const char* end, const char** tokens, size_t capacity) {
  size_t count = 0;
  while (const char* token = nextToken(text, end)) {
    if (count == capacity) return capacity + 1;
    tokens[count++] = token;
  }
  return count;
}

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

inline bool parseUnsigned(const char* token, int base,
                          uint64_t maximum, uint64_t& parsed) {
  if (hasWhitespace(token) || token[0] == '-' || token[0] == '+') {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const unsigned long long value = strtoull(token, &end, base);
  if (errno == ERANGE || end == token || *end != '\0' || value > maximum) {
    return false;
  }
  parsed = static_cast<uint64_t>(value);
  return true;
}

}  // namespace detail

inline bool parseU8Token(const char* token, uint8_t& out) {
  uint64_t parsed = 0;
  if (!detail::parseUnsigned(token, 10, UINT8_MAX, parsed)) {
    return false;
  }
  out = static_cast<uint8_t>(parsed);
  return true;
}

inline bool parseU16Token(const char* token, uint16_t& out) {
  uint64_t parsed = 0;
  if (!detail::parseUnsigned(token, 10, UINT16_MAX, parsed)) {
    return false;
  }
  out = static_cast<uint16_t>(parsed);
  return true;
}

inline bool parseU32Token(const char* token, uint32_t& out) {
  uint64_t parsed = 0;
  if (!detail::parseUnsigned(token, 10, UINT32_MAX, parsed)) {
    return false;
  }
  out = static_cast<uint32_t>(parsed);
  return true;
}

inline bool parseFloatToken(const char* token, float& out) {
  if (detail::hasWhitespace(token)) {
    return false;
  }
  const char* unsignedText = token;
  if (*unsignedText == '+' || *unsignedText == '-') {
    ++unsignedText;
  }
  if (unsignedText[0] == '0' &&
      (unsignedText[1] == 'x' || unsignedText[1] == 'X')) {
    return false;
  }
  errno = 0;
  char* end = nullptr;
  const double parsed = strtod(token, &end);
  const double magnitude = std::fabs(parsed);
  if (errno == ERANGE || end == token || *end != '\0' ||
      !std::isfinite(parsed) ||
      magnitude > static_cast<double>(std::numeric_limits<float>::max()) ||
      (magnitude != 0.0 &&
       magnitude < static_cast<double>(std::numeric_limits<float>::denorm_min()))) {
    return false;
  }
  out = static_cast<float>(parsed);
  return true;
}

inline bool parseBool01Token(const char* token, bool& out) {
  uint8_t parsed = 0;
  if (!parseU8Token(token, parsed) || parsed > 1U) {
    return false;
  }
  out = parsed == 1U;
  return true;
}

inline bool parseRegisterToken(const char* token, uint8_t& out) {
  if (detail::hasWhitespace(token) || token[0] == '-' || token[0] == '+') {
    return false;
  }
  int base = 10;
  if (token[0] == '0' && (token[1] == 'x' || token[1] == 'X')) {
    if (token[2] == '\0') {
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
