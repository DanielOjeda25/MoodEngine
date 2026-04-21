#pragma once

// Macros de asercion. En Debug abortan con mensaje. En Release se compilan a
// nada (no dejan rastro en el binario).

#include "core/Log.h"

#if defined(_MSC_VER)
    #define MOOD_DEBUG_BREAK() __debugbreak()
#else
    #include <csignal>
    #define MOOD_DEBUG_BREAK() std::raise(SIGTRAP)
#endif

#if !defined(NDEBUG)
    #define MOOD_ASSERT(cond, ...)                                                                 \
        do {                                                                                       \
            if (!(cond)) {                                                                         \
                MOOD_LOG_CRITICAL("Assertion failed: {} ({}:{})", #cond, __FILE__, __LINE__);      \
                MOOD_LOG_CRITICAL(__VA_ARGS__);                                                    \
                MOOD_DEBUG_BREAK();                                                                \
            }                                                                                      \
        } while (0)
#else
    #define MOOD_ASSERT(cond, ...) ((void)0)
#endif
