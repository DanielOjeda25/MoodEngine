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
//
// F2H28: agregado "Editor de mapas" como 4to workspace (4-viewport
// estilo Hammer). Los tests actualizados a count() == 4u y con la
// linea "Editor de mapas" como ultimo nombre default.

#include <doctest/doctest.h>

#include "editor/workspace/WorkspaceManager.h"

using namespace Mood;

TEST_CASE("WorkspaceManager: ctor crea 4 defaults con activo = 0") {
    WorkspaceManager m;
    CHECK(m.count() == 4u);
    CHECK(m.activeIndex() == 0);
    // F2H28: 4 workspaces (era 3 en F2H23; "Editor de mapas" agregado).
    CHECK(m.activeWorkspace().name == "Layout");
    // Los iniLayout estan vacios — DockBuilder los construye al activar.
    for (const auto& w : m.workspaces()) {
        CHECK(w.iniLayout.empty());
    }
}

TEST_CASE("WorkspaceManager: nombres de los 4 defaults (F2H28)") {
    WorkspaceManager m;
    CHECK(m.workspaces()[0].name == "Layout");
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Materiales");
    CHECK(m.workspaces()[3].name == "Editor de mapas");
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
    CHECK(m.workspaces()[3].iniLayout.empty()); // F2H28: Editor de mapas
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
    CHECK(m.count() == 4u);
    CHECK(m.activeIndex() == 0);
    CHECK(m.activeWorkspace().name == "Layout");
}

// ============================================================
// F2H22+F2H23: migracion de nombres viejos
// (F2H7 originals: Layout/Scripting/Profile/Materials)
// (F2H22: Modelar/Programar/Optimizar/Materiales)
// -> F2H23: Layout/Programar/Materiales (Optimizar eliminado).
// ============================================================

TEST_CASE("WorkspaceManager: setWorkspaces migra nombres F2H7 a F2H28") {
    WorkspaceManager m;
    std::vector<Workspace> oldNamed = {
        Workspace{"Layout",    "ini1"},
        Workspace{"Scripting", "ini2"},
        Workspace{"Profile",   "ini3"},  // se filtra (Optimizar eliminado)
        Workspace{"Materials", "ini4"},
    };
    m.setWorkspaces(std::move(oldNamed));

    // F2H28: Profile/Optimizar filtrado + "Editor de mapas" completado
    // por defaults -> 4 workspaces totales.
    REQUIRE(m.count() == 4u);
    CHECK(m.workspaces()[0].name == "Layout");
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Materiales");
    CHECK(m.workspaces()[3].name == "Editor de mapas");

    // iniLayout preservado para los 3 que vinieron del .moodproj viejo.
    CHECK(m.workspaces()[0].iniLayout == "ini1");
    CHECK(m.workspaces()[1].iniLayout == "ini2");
    CHECK(m.workspaces()[2].iniLayout == "ini4");  // Materials -> Materiales
    CHECK(m.workspaces()[3].iniLayout.empty());     // Editor de mapas: default vacio
}

TEST_CASE("WorkspaceManager: setWorkspaces migra nombres F2H22 a F2H28") {
    WorkspaceManager m;
    std::vector<Workspace> f2h22Named = {
        Workspace{"Modelar",    "a"},
        Workspace{"Programar",  "b"},
        Workspace{"Optimizar",  "c"},  // se filtra (eliminado en F2H23)
        Workspace{"Materiales", "d"},
    };
    m.setWorkspaces(std::move(f2h22Named));

    // F2H28: + "Editor de mapas" completado por defaults.
    REQUIRE(m.count() == 4u);
    CHECK(m.workspaces()[0].name == "Layout");      // Modelar -> Layout
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Materiales");
    CHECK(m.workspaces()[3].name == "Editor de mapas");

    CHECK(m.workspaces()[0].iniLayout == "a");
    CHECK(m.workspaces()[1].iniLayout == "b");
    CHECK(m.workspaces()[2].iniLayout == "d");
    CHECK(m.workspaces()[3].iniLayout.empty());
}

TEST_CASE("WorkspaceManager: setWorkspaces con set incompleto completa con defaults") {
    // F2H28: si el .moodproj solo trae 1-3 workspaces validos, el manager
    // completa con defaults para garantizar los 4 estandar.
    WorkspaceManager m;
    std::vector<Workspace> partial = {
        Workspace{"Layout", "x"},
    };
    m.setWorkspaces(std::move(partial));

    REQUIRE(m.count() == 4u);
    CHECK(m.workspaces()[0].name == "Layout");
    CHECK(m.workspaces()[0].iniLayout == "x");
    // Programar + Materiales + "Editor de mapas" agregados con iniLayout vacio.
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[1].iniLayout.empty());
    CHECK(m.workspaces()[2].name == "Materiales");
    CHECK(m.workspaces()[2].iniLayout.empty());
    CHECK(m.workspaces()[3].name == "Editor de mapas");
    CHECK(m.workspaces()[3].iniLayout.empty());
}

TEST_CASE("WorkspaceManager: setWorkspaces filtra nombres custom no reconocidos") {
    // F2H28: entradas con nombres que no son ni F2H7 ni F2H22 ni F2H23/F2H28
    // se descartan (no mapean a ningun workspace estandar). El manager
    // completa con los 4 defaults.
    WorkspaceManager m;
    std::vector<Workspace> custom = {
        Workspace{"MiCustom",  "x"},
        Workspace{"Otro",      "y"},
    };
    m.setWorkspaces(std::move(custom));

    REQUIRE(m.count() == 4u);
    CHECK(m.workspaces()[0].name == "Layout");
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Materiales");
    CHECK(m.workspaces()[3].name == "Editor de mapas");
}

TEST_CASE("WorkspaceManager: captureCurrentLayout en idx invalido es no-op") {
    // Caso defensivo: el manager no deberia entrar en este estado en
    // produccion (siempre hay >=1 workspace), pero validamos que no
    // crashea si pasara.
    WorkspaceManager m;
    // Forzar un estado con un solo workspace via custom — el filtro
    // F2H23 lo descarta y completa con defaults; F2H28 garantiza los
    // 4 estandar (Layout/Programar/Materiales/Editor de mapas).
    m.setWorkspaces({Workspace{"Solo", ""}});
    REQUIRE(m.count() == 4u);
    m.setActiveByIndex(0);
    m.captureCurrentLayout("dato");
    CHECK(m.workspaces()[0].iniLayout == "dato");
    // Restaurar defaults; capturar no debe crashear.
    m.setWorkspaces({});
    m.captureCurrentLayout("otro");
    CHECK(m.activeWorkspace().iniLayout == "otro");
}
