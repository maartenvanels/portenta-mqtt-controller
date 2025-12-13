#pragma once

#include <cstddef>
#include <cstdint>

// Minimal UDP stub for native unit tests / IDE indexing.
// It is NOT a functional UDP implementation.
class UDP {
public:
  virtual ~UDP() = default;
  virtual int begin(uint16_t /*port*/) { return 1; }
  virtual void stop() {}
};


