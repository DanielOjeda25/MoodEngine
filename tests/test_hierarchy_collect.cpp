// Tests del helper collectHierarchyEntries (F2H5 Bloque C). El helper
// es PURO (no toca ImGui), asi que se prueba directo. Cubre:
//   - scene vacia -> vector vacio.
//   - 3 entidades con tag -> 3 entries con tags correctos.
//   - entidad sin TagComponent -> no aparece (consistente con el
//     comportamiento del panel previo a F2H5).
//   - reuso del vector: el helper hace clear() antes de rellenar.
//   - orden estable entre llamadas (necesario para que el seleccion
//     no salte cuando el clipper recalcula su rango visible).

#include <doctest/doctest.h>

#include "editor/panels/scene/HierarchyPanel.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

using namespace Mood;

TEST_CASE("collectHierarchyEntries: scene vacia devuelve vector vacio") {
    Scene s;
    std::vector<HierarchyEntry> out;
    collectHierarchyEntries(s, out);
    CHECK(out.empty());
}

TEST_CASE("collectHierarchyEntries: 3 entidades con tag aparecen todas") {
    Scene s;
    s.createEntity("alpha");
    s.createEntity("beta");
    s.createEntity("gamma");

    std::vector<HierarchyEntry> out;
    collectHierarchyEntries(s, out);

    CHECK(out.size() == 3u);
    // Cada entry tiene puntero valido al TagComponent. (handle es entt::entity
    // y `forEach` solo itera entidades validas, no hace falta chequearlo.)
    for (const auto& e : out) {
        REQUIRE(e.tag != nullptr);
        CHECK_FALSE(e.tag->name.empty());
    }
    // Los 3 tags estan presentes (orden no garantizado por entt; basta
    // con que aparezcan los 3).
    int alpha = 0, beta = 0, gamma = 0;
    for (const auto& e : out) {
        if (e.tag->name == "alpha") ++alpha;
        else if (e.tag->name == "beta") ++beta;
        else if (e.tag->name == "gamma") ++gamma;
    }
    CHECK(alpha == 1);
    CHECK(beta == 1);
    CHECK(gamma == 1);
}

TEST_CASE("collectHierarchyEntries: reusa el vector con clear()") {
    Scene s;
    std::vector<HierarchyEntry> out;
    out.push_back(HierarchyEntry{entt::null, nullptr}); // basura previa

    s.createEntity("solo");
    collectHierarchyEntries(s, out);

    // El helper debe haber clear()-ado la basura antes de rellenar.
    CHECK(out.size() == 1u);
    REQUIRE(out[0].tag != nullptr);
    CHECK(out[0].tag->name == "solo");
}

TEST_CASE("collectHierarchyEntries: orden estable entre llamadas") {
    Scene s;
    for (int i = 0; i < 10; ++i) {
        s.createEntity("e" + std::to_string(i));
    }

    std::vector<HierarchyEntry> a, b;
    collectHierarchyEntries(s, a);
    collectHierarchyEntries(s, b);

    REQUIRE(a.size() == b.size());
    REQUIRE(a.size() == 10u);
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].handle == b[i].handle);
        REQUIRE(a[i].tag != nullptr);
        REQUIRE(b[i].tag != nullptr);
        CHECK(a[i].tag->name == b[i].tag->name);
    }
}

TEST_CASE("collectHierarchyEntries: capacidad del vector se preserva") {
    Scene s;
    std::vector<HierarchyEntry> out;
    out.reserve(1000);
    const auto initialCapacity = out.capacity();

    s.createEntity("uno");
    collectHierarchyEntries(s, out);

    CHECK(out.size() == 1u);
    // clear() no debe reducir capacity — el reuse entre frames depende
    // de eso para no reallocar.
    CHECK(out.capacity() >= initialCapacity);
}
