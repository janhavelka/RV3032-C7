#pragma once

#include "I2cTransport.h"
#include "Rv3032Cli.h"

namespace i2c_scanner {

// Borrow the application's bus for one address-only attempt. The scoped
// timeout restores the owner's setting before another address is examined.
inline rv3032cli::ScanResult probeAddress(TwoWire& wire, uint8_t address,
                                          uint16_t timeoutMs) {
  transport::ScopedWireTimeout timeout(wire, timeoutMs);
  wire.beginTransmission(address);
  const uint8_t error = wire.endTransmission(true);
  if (error == 0) return rv3032cli::ScanResult::PRESENT;
  if (error == 5) return rv3032cli::ScanResult::TIMEOUT;
  return rv3032cli::ScanResult::ABSENT;
}

}  // namespace i2c_scanner
