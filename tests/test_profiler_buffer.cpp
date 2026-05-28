// F3H23: tests headless del ProfilerBuffer. Cubre:
//   - resize + clamp [60, 1200]
//   - pushScope + endFrame wraparound del ring
//   - aggregate avg/min/max sobre los últimos N frames
//   - lastFrame() snapshot

#include <doctest/doctest.h>

#include "engine/profile/ProfilerBuffer.h"

using namespace Mood;

TEST_CASE("F3H23: ProfilerBuffer default capacity 240") {
    ProfilerBuffer b;
    CHECK(b.frameCapacity() == 240u);
    CHECK(b.framesRecorded() == 0u);
}

TEST_CASE("F3H23: ProfilerBuffer resize clamp [60, 1200]") {
    ProfilerBuffer b;
    b.resize(10);
    CHECK(b.frameCapacity() == 60u);  // clamp inferior
    b.resize(5000);
    CHECK(b.frameCapacity() == 1200u);  // clamp superior
    b.resize(600);
    CHECK(b.frameCapacity() == 600u);  // valido
}

TEST_CASE("F3H23: pushScope + endFrame snapshot al ring") {
    ProfilerBuffer b(60);
    b.pushScope("scope_a", 1.5f);
    b.pushScope("scope_b", 0.5f);
    b.endFrame();
    CHECK(b.framesRecorded() == 1u);
    CHECK(b.lastFrame().size() == 2u);
    CHECK(b.lastFrame()[0].milliseconds == doctest::Approx(1.5f));
    CHECK(b.lastFrame()[1].milliseconds == doctest::Approx(0.5f));
    CHECK(b.lastFrameTotalMs() == doctest::Approx(2.0f));
}

TEST_CASE("F3H23: ring buffer wraparound — framesRecorded cap a capacity") {
    ProfilerBuffer b(60);
    // 100 frames con un único scope cada uno; capacity 60.
    for (int i = 0; i < 100; ++i) {
        b.pushScope("scope_x", static_cast<f32>(i));
        b.endFrame();
    }
    CHECK(b.framesRecorded() == 60u);  // clamped a capacity
    // El último frame debe ser i=99 (último value pusheado).
    REQUIRE(b.lastFrame().size() == 1u);
    CHECK(b.lastFrame()[0].milliseconds == doctest::Approx(99.0f));
}

TEST_CASE("F3H23: aggregate avg/min/max correcto sobre N frames") {
    ProfilerBuffer b(60);
    // 5 frames con un scope variable.
    const f32 values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    for (f32 v : values) {
        b.pushScope("frame_total", v);
        b.endFrame();
    }
    const auto agg = b.aggregate(5);
    REQUIRE(agg.size() == 1u);
    const auto& a = agg[0];
    CHECK(a.avgMs == doctest::Approx(3.0f));  // (1+2+3+4+5)/5
    CHECK(a.minMs == doctest::Approx(1.0f));
    CHECK(a.maxMs == doctest::Approx(5.0f));
    CHECK(a.lastMs == doctest::Approx(5.0f));  // último pusheado
    CHECK(a.hitsPerFrame == 1u);
}

TEST_CASE("F3H23: aggregate con varias scopes por frame ordena por avg desc") {
    ProfilerBuffer b(60);
    // 1 frame: scope_slow (10 ms) > scope_fast (1 ms).
    b.pushScope("scope_fast", 1.0f);
    b.pushScope("scope_slow", 10.0f);
    b.endFrame();
    const auto agg = b.aggregate(1);
    REQUIRE(agg.size() == 2u);
    CHECK(std::string(agg[0].name) == "scope_slow");  // arriba por avg desc
    CHECK(std::string(agg[1].name) == "scope_fast");
}

TEST_CASE("F3H23: aggregate window 0 trata como 'todos los frames recorded'") {
    ProfilerBuffer b(60);
    for (int i = 0; i < 5; ++i) {
        b.pushScope("s", 2.0f);
        b.endFrame();
    }
    const auto agg = b.aggregate(0);
    REQUIRE(agg.size() == 1u);
    CHECK(agg[0].hitsPerFrame == 1u);
    CHECK(agg[0].avgMs == doctest::Approx(2.0f));
}

TEST_CASE("F3H23: endFrame sin pushes crea slot vacio sin crashear") {
    ProfilerBuffer b(60);
    b.endFrame();
    CHECK(b.framesRecorded() == 1u);
    CHECK(b.lastFrame().empty());
    CHECK(b.lastFrameTotalMs() == doctest::Approx(0.0f));
}
