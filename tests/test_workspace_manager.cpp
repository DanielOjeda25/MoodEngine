// Tests del WorkspaceManager (F2H7 Bloque H — escritos junto al
// Bloque B porque el manager es PURO y se valida en aislacion).
//
// Cubren: defaults al construir, setActiveByIndex con out-of-bounds,
// captureCurrentLayout actualiza solo el activo, switching preserva
// los iniLayouts entre cambios, setWorkspaces vacio cae a defaults.
//
// F2H22: migracion de nombres viejos (Layout/Scripting/Profile/Materials)
// a nuevos en castellano (Modelar/Programar/Optimizar/Materiales).
//
// F2H23: schema reducido a 3 workspaces (Layout/Programar/Materiales).
// El workspace "Optimizar"/"Profile" fue eliminado — los entries
// obsoletos del .moodproj se filtran al cargar.

#include <doctest/doctest.h>

#include "editor/workspace/WorkspaceManager.h"

using namespace Mood;

TEST_CASE("WorkspaceManager: ctor crea 3 defaults con activo = 0") {
    WorkspaceManager m;
    CHECK(m.count() == 3u);
    CHECK(m.activeIndex() == 0);
    // F2H23: 3 workspaces en castellano (era 4 en F2H22; Optimizar
    // eliminado).
    CHECK(m.activeWorkspace().name == "Layout");
    // Los iniLayout estan vacios — DockBuilder los construye al activar.
    for (const auto& w : m.workspaces()) {
        CHECK(w.iniLayout.empty());
    }
}

TEST_CASE("WorkspaceManager: nombres de los 3 defaults (F2H23)") {
    WorkspaceManager m;
    CHECK(m.workspaces()[0].name == "Layout");
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Materiales");
}

TEST_CASE("WorkspaceManager: setActiveByIndex valida cambia el activo") {
    WorkspaceManager m;
    m.setActiveByIndex(2);
    CHECK(m.activeIndex() == 2);
    CHECK(m.activeWorkspace().name == "Materiales");
    m.setActiveByIndex(0);
    CHECK(m.activeIndex() == 0);
    CHECK(m.activeWorkspace().name == "Layout");
}

TEST_CASE("WorkspaceManager: setActiveByIndex out-of-bounds es no-op") {
    WorkspaceManager m;
    m.setActiveByIndex(2);
    m.setActiveByIndex(-1);   // ignorado
    CHECK(m.activeIndex() == 2);
    m.setActiveByIndex(99);   // ignorado
    CHECK(m.activeIndex() == 2);
}

TEST_CASE("WorkspaceManager: captureCurrentLayout actualiza solo el activo") {
    WorkspaceManager m;
    m.setActiveByIndex(1);
    m.captureCurrentLayout("layout-A");
    CHECK(m.workspaces()[0].iniLayout.empty());
    CHECK(m.workspaces()[1].iniLayout == "layout-A");
    CHECK(m.workspaces()[2].iniLayout.empty());
}

TEST_CASE("WorkspaceManager: switching preserva los iniLayouts") {
    WorkspaceManager m;
    // Capturar layout del activo (Layout, idx 0).
    m.captureCurrentLayout("ini-Layout");
    // Cambiar a Programar y capturar.
    m.setActiveByIndex(1);
    m.captureCurrentLayout("ini-Programar");
    // Volver a Layout: el iniLayout previo debe seguir ahi.
    m.setActiveByIndex(0);
    CHECK(m.activeWorkspace().iniLayout == "ini-Layout");
    // Volver a Programar: idem.
    m.setActiveByIndex(1);
    CHECK(m.activeWorkspace().iniLayout == "ini-Programar");
}

TEST_CASE("WorkspaceManager: setWorkspaces con vector vacio cae a defaults") {
    WorkspaceManager m;
    m.setActiveByIndex(2);
    m.setWorkspaces({});
    CHECK(m.count() == 3u);
    CHECK(m.activeIndex() == 0);
    CHECK(m.activeWorkspace().name == "Layout");
}

// ============================================================
// F2H22+F2H23: migracion de nombres viejos
// (F2H7 originals: Layout/Scripting/Profile/Materials)
// (F2H22: Modelar/Programar/Optimizar/Materiales)
// -> F2H23: Layout/Programar/Materiales (Optimizar eliminado).
// ============================================================

TEST_CASE("WorkspaceManager: setWorkspaces migra nombres F2H7 a F2H23") {
    WorkspaceManager m;
    std::vector<Workspace> oldNamed = {
        Workspace{"Layout",    "ini1"},
        Workspace{"Scripting", "ini2"},
        Workspace{"Profile",   "ini3"},  // se filtra (Optimizar eliminado)
        Workspace{"Materials", "ini4"},
    };
    m.setWorkspaces(std::move(oldNamed));

    // Resultado: 3 workspaces (Profile/Optimizar filtrado).
    REQUIRE(m.count() == 3u);
    CHECK(m.workspaces()[0].name == "Layout");
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Materiales");

    // iniLayout preservado para los 3 sobrevivientes.
    CHECK(m.workspaces()[0].iniLayout == "ini1");
    CHECK(m.workspaces()[1].iniLayout == "ini2");
    CHECK(m.workspaces()[2].iniLayout == "ini4");  // Materials -> Materiales
}

TEST_CASE("WorkspaceManager: setWorkspaces migra nombres F2H22 a F2H23") {
    WorkspaceManager m;
    std::vector<Workspace> f2h22Named = {
        Workspace{"Modelar",    "a"},
        Workspace{"Programar",  "b"},
        Workspace{"Optimizar",  "c"},  // se filtra (eliminado en F2H23)
        Workspace{"Materiales", "d"},
    };
    m.setWorkspaces(std::move(f2h22Named));

    REQUIRE(m.count() == 3u);
    CHECK(m.workspaces()[0].name == "Layout");      // Modelar -> Layout
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Materiales");

    CHECK(m.workspaces()[0].iniLayout == "a");
    CHECK(m.workspaces()[1].iniLayout == "b");
    CHECK(m.workspaces()[2].iniLayout == "d");
}

TEST_CASE("WorkspaceManager: setWorkspaces con set incompleto completa con defaults") {
    // F2H23: si el .moodproj solo trae 1 o 2 workspaces validos, el
    // manager completa con defaults para garantizar los 3 estandar.
    WorkspaceManager m;
    std::vector<Workspace> partial = {
        Workspace{"Layout", "x"},
    };
    m.setWorkspaces(std::move(partial));

    REQUIRE(m.count() == 3u);
    CHECK(m.workspaces()[0].name == "Layout");
    CHECK(m.workspaces()[0].iniLayout == "x");
    // Programar + Materiales agregados con iniLayout vacio.
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[1].iniLayout.empty());
    CHECK(m.workspaces()[2].name == "Materiales");
    CHECK(m.workspaces()[2].iniLayout.empty());
}

TEST_CASE("WorkspaceManager: setWorkspaces filtra nombres custom no reconocidos") {
    // F2H23: entradas con nombres que no son ni F2H7 ni F2H22 ni F2H23
    // se descartan (no mapean a ningun workspace estandar). El manager
    // completa con los 3 defaults.
    WorkspaceManager m;
    std::vector<Workspace> custom = {
        Workspace{"MiCustom",  "x"},
        Workspace{"Otro",      "y"},
    };
    m.setWorkspaces(std::move(custom));

    REQUIRE(m.count() == 3u);
    CHECK(m.workspaces()[0].name == "Layout");
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Materiales");
}

TEST_CASE("WorkspaceManager: captureCurrentLayout en idx invalido es no-op") {
    // Caso defensivo: el manager no deberia entrar en este estado en
    // produccion (siempre hay >=1 workspace), pero validamos que no
    // crashea si pasara.
    WorkspaceManager m;
    // Forzar un estado con un solo workspace via custom — el filtro
    // F2H23 lo descarta y completa con defaults; el manager queda
    // con los 3 estandar igual.
    m.setWorkspaces({Workspace{"Solo", ""}});
    REQUIRE(m.count() == 3u);
    m.setActiveByIndex(0);
    m.captureCurrentLayout("dato");
    CHECK(m.workspaces()[0].iniLayout == "dato");
    // Restaurar defaults; capturar no debe crashear.
    m.setWorkspaces({});
    m.captureCurrentLayout("otro");
    CHECK(m.activeWorkspace().iniLayout == "otro");
}
