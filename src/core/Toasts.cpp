#include "core/Toasts.h"

#include "core/UserSettings.h"

#include <algorithm>
#include <mutex>

namespace Mood::Toasts {

namespace {

// Cola global single-thread-safe. Mutex porque algunos toasts pueden
// emitirse desde el asset importer (que en futuro puede ser worker
// thread). Hoy todo en main thread, pero cubrimos defensivamente.
std::mutex s_mutex;
std::vector<Toast> s_queue;

f32 resolveLifetimeMs(f32 explicitLifetimeMs) {
    if (explicitLifetimeMs > 0.0f) return explicitLifetimeMs;
    // F3H24: default = UserSettings.editor.toastsLifetimeMs. Clamp ya
    // fue aplicado en `editorSettingsFromJson` — acá solo lo leemos.
    return static_cast<f32>(UserSettings::editor().toastsLifetimeMs);
}

} // namespace

void push(Severity sev, std::string message, f32 lifetimeMs) {
    // F3H24: si el dev desactivó toasts globalmente, drop silencioso.
    // Los logs del LogRingSink ya capturan todo — los toasts solo son
    // surfacing visual.
    if (!UserSettings::editor().toastsEnabled) return;

    const f32 lt = resolveLifetimeMs(lifetimeMs);
    Toast t;
    t.severity    = sev;
    t.message     = std::move(message);
    t.remainingMs = lt;
    t.totalMs     = lt;

    std::lock_guard<std::mutex> lock(s_mutex);
    s_queue.push_back(std::move(t));
}

std::vector<Toast> snapshot() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_queue;  // copy — el caller renderiza, podemos seguir mutando.
}

void tick(f32 dtMs) {
    if (dtMs <= 0.0f) return;
    std::lock_guard<std::mutex> lock(s_mutex);
    for (auto& t : s_queue) t.remainingMs -= dtMs;
    s_queue.erase(
        std::remove_if(s_queue.begin(), s_queue.end(),
                       [](const Toast& t) { return t.remainingMs <= 0.0f; }),
        s_queue.end());
}

void clear() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_queue.clear();
}

usize size() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_queue.size();
}

} // namespace Mood::Toasts
