#include "engine/ui/RmlSystem.h"

#include "core/Log.h"

namespace Mood {

RmlSystem::RmlSystem() : m_start(std::chrono::steady_clock::now()) {}

double RmlSystem::GetElapsedTime() {
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - m_start;
    return elapsed.count();
}

bool RmlSystem::LogMessage(Rml::Log::Type type, const Rml::String& message) {
    using LT = Rml::Log::Type;
    switch (type) {
        case LT::LT_ERROR:
        case LT::LT_ASSERT:
            Log::editor()->error("[rmlui] {}", message);
            break;
        case LT::LT_WARNING:
            Log::editor()->warn("[rmlui] {}", message);
            break;
        case LT::LT_INFO:
            Log::editor()->info("[rmlui] {}", message);
            break;
        case LT::LT_DEBUG:
        case LT::LT_ALWAYS:
        default:
            Log::editor()->debug("[rmlui] {}", message);
            break;
    }
    return true;
}

} // namespace Mood
