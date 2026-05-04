// Tests del MapsManager (F2H8 Bloque B). PURO — no toca disco ni ImGui.
// Cubre los invariantes:
//   - Siempre >= 1 mapa.
//   - default + current dentro de la lista.
//   - addMap deduplica.
//   - removeMap del default reasigna automatico.
//   - removeMap del ultimo es no-op.

#include <doctest/doctest.h>

#include "editor/project/MapsManager.h"

using namespace Mood;

TEST_CASE("MapsManager: ctor inicial — vacio") {
    MapsManager m;
    CHECK(m.count() == 0u);
    CHECK(m.maps().empty());
}

TEST_CASE("MapsManager: setMaps inicializa lista + default + current") {
    MapsManager m;
    m.setMaps({"maps/a.moodmap", "maps/b.moodmap"},
               "maps/a.moodmap", "maps/b.moodmap");
    CHECK(m.count() == 2u);
    CHECK(m.defaultMap().generic_string() == "maps/a.moodmap");
    CHECK(m.currentMap().generic_string() == "maps/b.moodmap");
}

TEST_CASE("MapsManager: setMaps con maps vacio agrega el default automatico") {
    MapsManager m;
    m.setMaps({}, "maps/foo.moodmap", "maps/foo.moodmap");
    CHECK(m.count() == 1u);
    CHECK(m.defaultMap().generic_string() == "maps/foo.moodmap");
}

TEST_CASE("MapsManager: setMaps con current fuera de lista cae al default") {
    MapsManager m;
    m.setMaps({"maps/a.moodmap"}, "maps/a.moodmap", "maps/no_existe.moodmap");
    CHECK(m.currentMap().generic_string() == "maps/a.moodmap");
}

TEST_CASE("MapsManager: addMap agrega + dedup") {
    MapsManager m;
    m.setMaps({"maps/a.moodmap"}, "maps/a.moodmap", "maps/a.moodmap");

    CHECK(m.addMap("maps/b.moodmap"));
    CHECK(m.count() == 2u);

    // Duplicado: false + count no cambia.
    CHECK_FALSE(m.addMap("maps/a.moodmap"));
    CHECK(m.count() == 2u);
}

TEST_CASE("MapsManager: addMap con paths equivalentes (slash distinto) deduplica") {
    MapsManager m;
    m.setMaps({"maps/a.moodmap"}, "maps/a.moodmap", "maps/a.moodmap");

    // generic_string normaliza separadores: "maps\\a.moodmap" -> "maps/a.moodmap".
    CHECK_FALSE(m.addMap(std::filesystem::path("maps") / "a.moodmap"));
    CHECK(m.count() == 1u);
}

TEST_CASE("MapsManager: removeMap del default reasigna automatico al primer restante") {
    MapsManager m;
    m.setMaps({"maps/a.moodmap", "maps/b.moodmap", "maps/c.moodmap"},
               "maps/a.moodmap", "maps/b.moodmap");
    CHECK(m.removeMap("maps/a.moodmap"));
    CHECK(m.count() == 2u);
    CHECK(m.defaultMap().generic_string() == "maps/b.moodmap");
}

TEST_CASE("MapsManager: removeMap del current NO toca current (caller maneja)") {
    MapsManager m;
    m.setMaps({"maps/a.moodmap", "maps/b.moodmap"},
               "maps/a.moodmap", "maps/b.moodmap");
    CHECK(m.removeMap("maps/b.moodmap"));
    // Current sigue apuntando al borrado — el caller debe cambiar.
    CHECK(m.currentMap().generic_string() == "maps/b.moodmap");
}

TEST_CASE("MapsManager: removeMap del ultimo mapa es false (invariante >=1)") {
    MapsManager m;
    m.setMaps({"maps/solo.moodmap"}, "maps/solo.moodmap", "maps/solo.moodmap");
    CHECK_FALSE(m.removeMap("maps/solo.moodmap"));
    CHECK(m.count() == 1u);
}

TEST_CASE("MapsManager: removeMap de path inexistente es false") {
    MapsManager m;
    m.setMaps({"maps/a.moodmap", "maps/b.moodmap"},
               "maps/a.moodmap", "maps/a.moodmap");
    CHECK_FALSE(m.removeMap("maps/no_existe.moodmap"));
    CHECK(m.count() == 2u);
}

TEST_CASE("MapsManager: setCurrentMap valida que el path este en la lista") {
    MapsManager m;
    m.setMaps({"maps/a.moodmap", "maps/b.moodmap"},
               "maps/a.moodmap", "maps/a.moodmap");

    m.setCurrentMap("maps/b.moodmap");
    CHECK(m.currentMap().generic_string() == "maps/b.moodmap");

    // Path fuera de lista = no-op.
    m.setCurrentMap("maps/no_existe.moodmap");
    CHECK(m.currentMap().generic_string() == "maps/b.moodmap");
}

TEST_CASE("MapsManager: setDefaultMap valida que el path este en la lista") {
    MapsManager m;
    m.setMaps({"maps/a.moodmap", "maps/b.moodmap"},
               "maps/a.moodmap", "maps/a.moodmap");

    m.setDefaultMap("maps/b.moodmap");
    CHECK(m.defaultMap().generic_string() == "maps/b.moodmap");

    // Path fuera = no-op.
    m.setDefaultMap("maps/no_existe.moodmap");
    CHECK(m.defaultMap().generic_string() == "maps/b.moodmap");
}

TEST_CASE("MapsManager: contains, isDefault, isCurrent") {
    MapsManager m;
    m.setMaps({"maps/a.moodmap", "maps/b.moodmap"},
               "maps/a.moodmap", "maps/b.moodmap");
    CHECK(m.contains("maps/a.moodmap"));
    CHECK(m.contains("maps/b.moodmap"));
    CHECK_FALSE(m.contains("maps/no.moodmap"));

    CHECK(m.isDefault("maps/a.moodmap"));
    CHECK_FALSE(m.isDefault("maps/b.moodmap"));

    CHECK(m.isCurrent("maps/b.moodmap"));
    CHECK_FALSE(m.isCurrent("maps/a.moodmap"));
}
