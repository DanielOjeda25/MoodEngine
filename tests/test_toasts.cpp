// F3H24: tests headless del sistema de toasts (core/Toasts).
// Verifica push, snapshot, tick (aging) y size. El gate de
// UserSettings.editor.toastsEnabled queda probado vía toggle en
// UserSettings::setEditor + push.

#include <doctest/doctest.h>

#include "core/Toasts.h"
#include "core/UserSettings.h"

using namespace Mood;

namespace {

// Helper: garantizar que toasts esten habilitados en UserSettings antes
// de cada test (cualquier test previo pudo haberlos apagado).
void ensureToastsEnabled() {
    auto ed = UserSettings::editor();
    ed.toastsEnabled = true;
    ed.toastsLifetimeMs = 3000;
    UserSettings::setEditor(ed);
}

void disableToasts() {
    auto ed = UserSettings::editor();
    ed.toastsEnabled = false;
    UserSettings::setEditor(ed);
}

void clearAll() {
    Toasts::clear();
}

} // namespace

TEST_CASE("F3H24: Toasts push + snapshot devuelve el orden de inserción") {
    ensureToastsEnabled();
    clearAll();
    Toasts::pushInfo("primero");
    Toasts::pushSuccess("segundo");
    Toasts::pushWarn("tercero");
    const auto snap = Toasts::snapshot();
    REQUIRE(snap.size() == 3u);
    CHECK(snap[0].message == "primero");
    CHECK(snap[0].severity == Toasts::Severity::Info);
    CHECK(snap[1].message == "segundo");
    CHECK(snap[1].severity == Toasts::Severity::Success);
    CHECK(snap[2].message == "tercero");
    CHECK(snap[2].severity == Toasts::Severity::Warn);
}

TEST_CASE("F3H24: tick decrementa remainingMs y descarta expirados") {
    ensureToastsEnabled();
    clearAll();
    Toasts::pushInfo("rapido", 100.0f);  // expira en 100 ms
    Toasts::pushInfo("lento", 500.0f);

    Toasts::tick(60.0f);  // -60ms ambos
    {
        const auto snap = Toasts::snapshot();
        REQUIRE(snap.size() == 2u);
        CHECK(snap[0].remainingMs == doctest::Approx(40.0f));
        CHECK(snap[1].remainingMs == doctest::Approx(440.0f));
    }

    Toasts::tick(60.0f);  // -60ms ambos → primero expira (-20)
    {
        const auto snap = Toasts::snapshot();
        REQUIRE(snap.size() == 1u);
        CHECK(snap[0].message == "lento");
        CHECK(snap[0].remainingMs == doctest::Approx(380.0f));
    }
}

TEST_CASE("F3H24: lifetime default usa UserSettings.toastsLifetimeMs") {
    ensureToastsEnabled();
    auto ed = UserSettings::editor();
    ed.toastsLifetimeMs = 2500;
    UserSettings::setEditor(ed);

    clearAll();
    Toasts::pushInfo("default lifetime");  // sin lifetime explícito
    const auto snap = Toasts::snapshot();
    REQUIRE(snap.size() == 1u);
    CHECK(snap[0].totalMs == doctest::Approx(2500.0f));
    CHECK(snap[0].remainingMs == doctest::Approx(2500.0f));
}

TEST_CASE("F3H24: lifetime explícito override del default") {
    ensureToastsEnabled();
    clearAll();
    Toasts::pushInfo("explícito", 1234.0f);
    const auto snap = Toasts::snapshot();
    REQUIRE(snap.size() == 1u);
    CHECK(snap[0].totalMs == doctest::Approx(1234.0f));
}

TEST_CASE("F3H24: toastsEnabled=false → push es no-op silencioso") {
    disableToasts();
    clearAll();
    Toasts::pushError("este toast no se ve");
    CHECK(Toasts::size() == 0u);
    ensureToastsEnabled();  // restaurar para tests siguientes
}

TEST_CASE("F3H24: clear() vacía la cola") {
    ensureToastsEnabled();
    clearAll();
    Toasts::pushInfo("a");
    Toasts::pushInfo("b");
    CHECK(Toasts::size() == 2u);
    Toasts::clear();
    CHECK(Toasts::size() == 0u);
}

TEST_CASE("F3H24: tick con dt 0 o negativo es no-op") {
    ensureToastsEnabled();
    clearAll();
    Toasts::pushInfo("estable", 1000.0f);
    Toasts::tick(0.0f);
    Toasts::tick(-50.0f);
    const auto snap = Toasts::snapshot();
    REQUIRE(snap.size() == 1u);
    CHECK(snap[0].remainingMs == doctest::Approx(1000.0f));
}
