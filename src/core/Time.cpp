#include "core/Time.h"

namespace Mood {

f32 FpsCounter::tick(f64 deltaSeconds) {
    m_samples[m_nextIndex] = deltaSeconds;
    m_nextIndex = (m_nextIndex + 1) % k_SampleCount;
    if (m_filled < k_SampleCount) {
        ++m_filled;
    }

    f64 sum = 0.0;
    for (Mood::usize i = 0; i < m_filled; ++i) {
        sum += m_samples[i];
    }

    if (sum > 0.0 && m_filled > 0) {
        const f64 avg = sum / static_cast<f64>(m_filled);
        m_fps = static_cast<f32>(1.0 / avg);
    }
    return m_fps;
}

} // namespace Mood
