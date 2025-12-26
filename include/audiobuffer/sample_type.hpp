#pragma once

// *****************************************************************************

#include <cstdint>
#include <limits>
#include <type_traits>
#include <cassert>

// *****************************************************************************

namespace audiobuffer {

// *****************************************************************************

enum class SampleType {
  UNSUPPORTED = 0,
  INT,
  UINT,
  IEEE_FLOAT,
};

// *****************************************************************************

} // namespace audiobuffer
