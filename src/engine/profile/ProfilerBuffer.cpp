#include "engine/profile/ProfilerBuffer.h"

#include "core/Profiler.h"  // Mood::detail::ScopeTimer (out-of-line dtor)

#include <algorithm>
#include <limits>

namespace Mood {

namespace detail {

// F3H23: out-of-line dtor para que el include de ProfilerBuffer.h NO
// sea requerido desde core/Profiler.h (que se incluye en casi todos los
// .cpp). El cuerpo accede a `profilerBuffer()` por nombre completo —
// hace inlining trivial al optimizar.
ScopeTimer::~ScopeTimer() {
    const auto end = std::chrono::steady_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        end - m_start).count();
    const f32 ms = static_cast<f32>(micros) / 1000.0f;
    profilerBuffer().pushScope(m_name, ms);
}

} // namespace detail


ProfilerBuffer::ProfilerBuffer(u32 frameCapacity) {
    resize(frameCapacity);
    m_currentFrame.reserve(128);  // pre-reserve para evitar realloc en hot path
    m_lastFrame.reserve(128);
}

void ProfilerBuffer::resize(u32 frameCapacity) {
    if (frameCapacity < 60) frameCapacity = 60;
    if (frameCapacity > 1200) frameCapacity = 1200;
    if (frameCapacity == m_capacity && !m_ring.empty()) return;
    m_capacity = frameCapacity;
    m_ring.assign(m_capacity, {});
    for (auto& slot : m_ring) slot.reserve(64);
    m_framesRecorded = 0;
    m_writeIndex = 0;
}

void ProfilerBuffer::pushScope(const char* name, f32 milliseconds) {
    if (name == nullptr) return;
    m_currentFrame.push_back({name, milliseconds});
}

void ProfilerBuffer::endFrame() {
    // Swap del frame en construcción al slot del ring; reset del frame
    // actual (vector::clear preserva capacity, no realloca).
    if (m_capacity == 0) return;  // defensa, no debería pasar tras resize
    auto& slot = m_ring[m_writeIndex];
    slot.swap(m_currentFrame);    // O(1) — solo swap de punteros internos
    m_lastFrame = slot;            // copy para vista "último frame"
    m_currentFrame.clear();
    m_writeIndex = (m_writeIndex + 1) % m_capacity;
    if (m_framesRecorded < m_capacity) ++m_framesRecorded;
}

std::vector<ProfilerBuffer::ScopeAggregate>
ProfilerBuffer::aggregate(u32 lastNFrames) const {
    if (m_framesRecorded == 0) return {};
    if (lastNFrames == 0 || lastNFrames > m_framesRecorded) {
        lastNFrames = m_framesRecorded;
    }

    // Acumulamos sum / min / max / hits por nombre (const char* como key —
    // los scopes son string literals con direcciones estables).
    struct Acc {
        f32 sum = 0.0f;
        f32 min = std::numeric_limits<f32>::infinity();
        f32 max = 0.0f;
        f32 last = 0.0f;
        u32 hits = 0;
    };
    std::unordered_map<const char*, Acc> bag;
    bag.reserve(64);

    // Walk de los últimos N frames hacia atrás desde writeIndex.
    for (u32 i = 0; i < lastNFrames; ++i) {
        const u32 idx = (m_writeIndex + m_capacity - 1 - i) % m_capacity;
        const auto& frame = m_ring[idx];
        for (const auto& s : frame) {
            auto& acc = bag[s.name];
            acc.sum += s.milliseconds;
            acc.min = std::min(acc.min, s.milliseconds);
            acc.max = std::max(acc.max, s.milliseconds);
            ++acc.hits;
            if (i == 0) acc.last = s.milliseconds;  // primer iter = más reciente
        }
    }

    std::vector<ScopeAggregate> out;
    out.reserve(bag.size());
    for (const auto& [name, acc] : bag) {
        ScopeAggregate agg;
        agg.name = name;
        agg.avgMs = (acc.hits > 0) ? (acc.sum / static_cast<f32>(acc.hits)) : 0.0f;
        agg.minMs = (acc.hits > 0) ? acc.min : 0.0f;
        agg.maxMs = acc.max;
        agg.lastMs = acc.last;
        agg.hitsPerFrame = acc.hits / lastNFrames;
        out.push_back(agg);
    }
    // Ordenar por avg descendente — los hot scopes arriba.
    std::sort(out.begin(), out.end(),
              [](const ScopeAggregate& a, const ScopeAggregate& b) {
                  return a.avgMs > b.avgMs;
              });
    return out;
}

f32 ProfilerBuffer::lastFrameTotalMs() const {
    f32 sum = 0.0f;
    for (const auto& s : m_lastFrame) sum += s.milliseconds;
    return sum;
}

ProfilerBuffer& profilerBuffer() {
    // F3H23: instancia global lazy. El editor llama `resize()` al init
    // con el valor de UserSettings.editor.profilerFrameCount.
    static ProfilerBuffer s_instance(240);
    return s_instance;
}

} // namespace Mood
