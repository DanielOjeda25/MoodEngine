#include "core/MemoryStats.h"

// Windows: psapi.h provee GetProcessMemoryInfo + PROCESS_MEMORY_COUNTERS.
// Linkear contra psapi.lib (CMakeLists target_link_libraries en target).
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <psapi.h>
#endif

namespace Mood::MemoryStats {

u64 getRssBytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<u64>(pmc.WorkingSetSize);
    }
    return 0;
#else
    // Linux/macOS no implementado (MoodEngine es Windows-only por ahora).
    // Cuando se porte, leer /proc/self/statm o mach_task_basic_info.
    return 0;
#endif
}

} // namespace Mood::MemoryStats
