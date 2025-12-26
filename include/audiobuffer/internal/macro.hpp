#pragma once

// *****************************************************************************

#if (defined(NDEBUG) || __cpp_exceptions != 199711 || defined(audiobuffer__disable_exceptions))
  #define audiobuffer__disable_exceptions 1
#else
  #define audiobuffer__disable_exceptions 0
#endif

#if (audiobuffer__disable_exceptions)
  #define audiobuffer__noexcept noexcept
#else
  #define audiobuffer__noexcept
#endif
