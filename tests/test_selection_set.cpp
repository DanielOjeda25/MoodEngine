// Tests del modelo SelectionSet (F2H13). Cubren:
//   - clear, add, remove, toggle, replaceWithSingle, contains.
//   - Invariantes: selected.empty() => active == Entity{};
//                  active != Entity{} => contains(set, active);
//                  sin duplicados.
//   - Idempotencia: toggle dos veces vuelve al estado original.
//   - Edge cases: ops sobre Entity default-constructed son no-op.
//   - Determinismo: misma secuencia de ops produce mismo estado.

#include <doctest/doctest.h>

#include "core/Types.h"
#include "editor/selection/SelectionSet.h"
#include "engine/scene/core/Scene.h"

using namespace Mood;

namespace {

/// @brief Helper: scene + N entidades creadas para testing. Las
///        entidades viven mientras el scene viva.
struct TestScene {
    Scene scene;
    std::vector<Entity> e;

    explicit TestScene(int n) {
        e.reserve(n);
        for (int i = 0; i < n; ++i) {
            e.push_back(scene.createEntity("E_" + std::to_string(i)));
        }
    }
};

}  // namespace

// ============================================================
// estado inicial + invariantes
// ============================================================

TEST_CASE("SelectionSet: estado inicial es vacio + active=Entity{}") {
    SelectionSet s;
    CHECK(s.selected.empty());
    CHECK_FALSE(static_cast<bool>(s.active));
}

TEST_CASE("SelectionSet: clear sobre set vacio es no-op valido") {
    SelectionSet s;
    clear(s);
    CHECK(s.selected.empty());
    CHECK_FALSE(static_cast<bool>(s.active));
}

// ============================================================
// contains
// ============================================================

TEST_CASE("contains: false sobre Entity default") {
    SelectionSet s;
    CHECK_FALSE(contains(s, Entity{}));
}

TEST_CASE("contains: false antes de add, true despues") {
    TestScene t(1);
    SelectionSet s;
    CHECK_FALSE(contains(s, t.e[0]));
    add(s, t.e[0]);
    CHECK(contains(s, t.e[0]));
}

// ============================================================
// add
// ============================================================

TEST_CASE("add: primera vez agrega y setea active") {
    TestScene t(1);
    SelectionSet s;
    add(s, t.e[0]);
    REQUIRE(s.selected.size() == 1);
    CHECK(s.selected[0].handle() == t.e[0].handle());
    CHECK(s.active.handle() == t.e[0].handle());
}

TEST_CASE("add: segunda llamada con misma entity no duplica pero re-setea active") {
    TestScene t(2);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, t.e[1]);
    REQUIRE(s.selected.size() == 2);
    CHECK(s.active.handle() == t.e[1].handle());
    add(s, t.e[0]);  // re-add e0 -> sigue siendo size 2 pero active=e0
    REQUIRE(s.selected.size() == 2);
    CHECK(s.active.handle() == t.e[0].handle());
}

TEST_CASE("add con Entity{} es no-op") {
    TestScene t(1);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, Entity{});
    REQUIRE(s.selected.size() == 1);
    CHECK(s.active.handle() == t.e[0].handle());
}

TEST_CASE("add: 3 entidades distintas -> 3 selected, active = ultima") {
    TestScene t(3);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, t.e[1]);
    add(s, t.e[2]);
    CHECK(s.selected.size() == 3);
    CHECK(s.active.handle() == t.e[2].handle());
}

// ============================================================
// remove
// ============================================================

TEST_CASE("remove: quita y si era el active, active = ultimo restante") {
    TestScene t(3);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, t.e[1]);
    add(s, t.e[2]);
    remove(s, t.e[2]);
    REQUIRE(s.selected.size() == 2);
    CHECK_FALSE(contains(s, t.e[2]));
    CHECK(s.active.handle() == t.e[1].handle());  // ultimo restante
}

TEST_CASE("remove: quita uno NO active -> active no cambia") {
    TestScene t(3);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, t.e[1]);
    add(s, t.e[2]);  // active = e2
    remove(s, t.e[0]);
    REQUIRE(s.selected.size() == 2);
    CHECK(s.active.handle() == t.e[2].handle());  // sin cambio
}

TEST_CASE("remove: quita el ultimo -> active = Entity{}") {
    TestScene t(1);
    SelectionSet s;
    add(s, t.e[0]);
    remove(s, t.e[0]);
    CHECK(s.selected.empty());
    CHECK_FALSE(static_cast<bool>(s.active));
}

TEST_CASE("remove: entity ausente es no-op") {
    TestScene t(2);
    SelectionSet s;
    add(s, t.e[0]);
    remove(s, t.e[1]);  // e1 nunca fue agregada
    REQUIRE(s.selected.size() == 1);
    CHECK(s.active.handle() == t.e[0].handle());
}

TEST_CASE("remove de Entity{} es no-op") {
    TestScene t(1);
    SelectionSet s;
    add(s, t.e[0]);
    remove(s, Entity{});
    REQUIRE(s.selected.size() == 1);
}

// ============================================================
// toggle
// ============================================================

TEST_CASE("toggle: 2 llamadas dejan el set igual al inicial") {
    TestScene t(1);
    SelectionSet s;
    toggle(s, t.e[0]);
    CHECK(contains(s, t.e[0]));
    CHECK(s.active.handle() == t.e[0].handle());
    toggle(s, t.e[0]);
    CHECK(s.selected.empty());
    CHECK_FALSE(static_cast<bool>(s.active));
}

TEST_CASE("toggle: agrega si no estaba (= add)") {
    TestScene t(2);
    SelectionSet s;
    add(s, t.e[0]);
    toggle(s, t.e[1]);
    CHECK(contains(s, t.e[1]));
    CHECK(s.active.handle() == t.e[1].handle());
}

TEST_CASE("toggle: quita si estaba (= remove)") {
    TestScene t(2);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, t.e[1]);
    toggle(s, t.e[0]);  // quita e0
    CHECK_FALSE(contains(s, t.e[0]));
    CHECK(s.active.handle() == t.e[1].handle());
}

// ============================================================
// replaceWithSingle
// ============================================================

TEST_CASE("replaceWithSingle: clear + add atomic") {
    TestScene t(3);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, t.e[1]);
    add(s, t.e[2]);
    replaceWithSingle(s, t.e[1]);
    REQUIRE(s.selected.size() == 1);
    CHECK(s.selected[0].handle() == t.e[1].handle());
    CHECK(s.active.handle() == t.e[1].handle());
}

TEST_CASE("replaceWithSingle con Entity{} = clear") {
    TestScene t(2);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, t.e[1]);
    replaceWithSingle(s, Entity{});
    CHECK(s.selected.empty());
    CHECK_FALSE(static_cast<bool>(s.active));
}

// ============================================================
// invariantes
// ============================================================

TEST_CASE("invariante: active esta siempre en selected o es Entity{}") {
    TestScene t(3);
    SelectionSet s;
    // Secuencia compleja de ops.
    add(s, t.e[0]);
    add(s, t.e[1]);
    toggle(s, t.e[2]);
    remove(s, t.e[1]);
    toggle(s, t.e[0]);

    if (static_cast<bool>(s.active)) {
        CHECK(contains(s, s.active));
    } else {
        CHECK(s.selected.empty());
    }
}

TEST_CASE("invariante: sin duplicados") {
    TestScene t(2);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, t.e[1]);
    add(s, t.e[0]);
    add(s, t.e[1]);
    add(s, t.e[0]);
    CHECK(s.selected.size() == 2);
}

// ============================================================
// F2H17: activeFaceIndex
// ============================================================

TEST_CASE("SelectionSet: activeFaceIndex default es -1") {
    SelectionSet s;
    CHECK(s.activeFaceIndex == -1);
}

TEST_CASE("clear: resetea activeFaceIndex a -1") {
    TestScene t(1);
    SelectionSet s;
    add(s, t.e[0]);
    s.activeFaceIndex = 3;
    clear(s);
    CHECK(s.activeFaceIndex == -1);
}

TEST_CASE("add con entity distinta resetea activeFaceIndex") {
    TestScene t(2);
    SelectionSet s;
    add(s, t.e[0]);
    s.activeFaceIndex = 5;
    add(s, t.e[1]);  // active cambia
    CHECK(s.activeFaceIndex == -1);
}

TEST_CASE("add con misma entity preserva activeFaceIndex") {
    TestScene t(1);
    SelectionSet s;
    add(s, t.e[0]);
    s.activeFaceIndex = 2;
    add(s, t.e[0]);  // re-add: active no cambia
    CHECK(s.activeFaceIndex == 2);
}

TEST_CASE("remove del active resetea activeFaceIndex") {
    TestScene t(2);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, t.e[1]);  // active = e1
    s.activeFaceIndex = 4;
    remove(s, t.e[1]);  // active cambia a e0
    CHECK(s.activeFaceIndex == -1);
}

TEST_CASE("remove de no-active preserva activeFaceIndex") {
    TestScene t(2);
    SelectionSet s;
    add(s, t.e[0]);
    add(s, t.e[1]);  // active = e1
    s.activeFaceIndex = 4;
    remove(s, t.e[0]);  // e0 no era active
    CHECK(s.activeFaceIndex == 4);
}

TEST_CASE("determinismo: misma secuencia produce mismo estado") {
    TestScene t(3);

    SelectionSet a, b;
    auto ops = [&](SelectionSet& s) {
        add(s, t.e[0]);
        toggle(s, t.e[1]);
        add(s, t.e[2]);
        remove(s, t.e[0]);
    };
    ops(a);
    ops(b);

    REQUIRE(a.selected.size() == b.selected.size());
    for (usize i = 0; i < a.selected.size(); ++i) {
        CHECK(a.selected[i].handle() == b.selected[i].handle());
    }
    CHECK(a.active.handle() == b.active.handle());
}
