#pragma once

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <string>

inline uint32_t arduinoStubMillis = 0;
inline uint32_t arduinoStubMicros = 0;

inline uint32_t millis() {
  return arduinoStubMillis;
}

inline uint32_t micros() {
  return arduinoStubMicros;
}

inline void delay(uint32_t delayMs) {
  arduinoStubMillis += delayMs;
  arduinoStubMicros += delayMs * 1000U;
}

inline void delayMicroseconds(uint32_t delayUs) {
  arduinoStubMicros += delayUs;
  arduinoStubMillis += delayUs / 1000U;
}

static constexpr int INPUT_PULLUP = 0x02;
static constexpr int OUTPUT = 0x03;
static constexpr int LOW = 0;
static constexpr int HIGH = 1;

inline void pinMode(int, int) {}
inline void digitalWrite(int, int) {}

inline void yield() {}

#ifndef F
#define F(value) value
#endif

class String {
 public:
  String() = default;
  String(const char* value) : _value(value == nullptr ? "" : value) {}
  String(const std::string& value) : _value(value) {}

  size_t length() const { return _value.length(); }
  const char* c_str() const { return _value.c_str(); }
  char operator[](size_t index) const { return _value[index]; }
  bool reserve(size_t capacity) {
    _value.reserve(capacity);
    return true;
  }
  void remove(size_t index) {
    if (index < _value.length()) {
      _value.erase(index);
    }
  }
  void trim() {
    size_t first = 0;
    while (first < _value.length() &&
           isspace(static_cast<unsigned char>(_value[first])) != 0) {
      ++first;
    }
    size_t last = _value.length();
    while (last > first &&
           isspace(static_cast<unsigned char>(_value[last - 1U])) != 0) {
      --last;
    }
    _value = _value.substr(first, last - first);
  }
  String substring(size_t start) const {
    return start < _value.length() ? String(_value.substr(start)) : String();
  }
  String substring(size_t start, size_t end) const {
    if (start >= _value.length() || end <= start) {
      return String();
    }
    return String(_value.substr(start, end - start));
  }
  void toLowerCase() {
    for (char& c : _value) {
      c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
  }
  String& operator=(const char* value) {
    _value = value == nullptr ? "" : value;
    return *this;
  }
  String& operator+=(char value) {
    _value += value;
    return *this;
  }
  bool operator==(const char* other) const {
    return _value == (other == nullptr ? "" : other);
  }
  bool operator!=(const char* other) const { return !(*this == other); }

 private:
  std::string _value;
};

class HardwareSerial {
 public:
  void resetInput() {
    _input.clear();
    _readIndex = 0;
  }
  void inject(const std::string& input) {
    _input.append(input);
  }
  void resetOutput() { _output.clear(); }
  const std::string& output() const { return _output; }
  int available() const {
    return static_cast<int>(_input.length() - _readIndex);
  }
  int read() {
    if (_readIndex >= _input.length()) {
      return -1;
    }
    return static_cast<unsigned char>(_input[_readIndex++]);
  }
  void begin(uint32_t) {}
  void flush() {}
  explicit operator bool() const { return true; }

  void printf(const char* format, ...) {
    char buffer[1024] = {};
    va_list args;
    va_start(args, format);
    const int written = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (written > 0) {
      const size_t length = static_cast<size_t>(written) < sizeof(buffer)
                                ? static_cast<size_t>(written)
                                : sizeof(buffer) - 1U;
      _output.append(buffer, length);
    }
  }
  void print(const char* value) {
    if (value != nullptr) _output += value;
  }
  void print(char value) { _output += value; }
  void print(const String& value) { _output += value.c_str(); }
  template <typename T>
  void print(const T&) {}
  void println(const char* value) {
    print(value);
    _output += '\n';
  }
  void println(char value) {
    print(value);
    _output += '\n';
  }
  void println(const String& value) {
    print(value);
    _output += '\n';
  }
  template <typename T>
  void println(const T& value) {
    print(value);
    _output += '\n';
  }
  void println() { _output += '\n'; }

 private:
  std::string _input;
  std::string _output;
  size_t _readIndex = 0;
};

inline HardwareSerial Serial;
