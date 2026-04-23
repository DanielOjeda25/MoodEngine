#include "core/LogRingSink.h"

namespace Mood {

LogRingSink::LogRingSink(usize capacity)
    : m_ring(capacity),
      m_capacity(capacity) {}

void LogRingSink::sink_it_(const spdlog::details::log_msg& msg) {
    std::lock_guard<std::mutex> lk(m_mutex);
    Entry& slot = m_ring[m_head];
    slot.channel.assign(msg.logger_name.begin(), msg.logger_name.end());
    slot.text.assign(msg.payload.begin(), msg.payload.end());
    slot.level = msg.level;

    m_head = (m_head + 1) % m_capacity;
    if (m_count < m_capacity) ++m_count;
}

std::vector<LogRingSink::Entry> LogRingSink::snapshot() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<Entry> out;
    out.reserve(m_count);
    // Recorrer en orden cronologico: si el buffer no esta lleno empezamos en
    // 0; si esta lleno, empezamos en m_head (la entrada mas vieja).
    const usize start = (m_count < m_capacity) ? 0 : m_head;
    for (usize i = 0; i < m_count; ++i) {
        out.push_back(m_ring[(start + i) % m_capacity]);
    }
    return out;
}

void LogRingSink::clear() {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_head = 0;
    m_count = 0;
}

} // namespace Mood
