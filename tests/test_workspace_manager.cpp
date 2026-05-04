// Tests del WorkspaceManager (F2H7 Bloque H — escritos junto al
// Bloque B porque el manager es PURO y se valida en aislacion).
//
// Cubren: defaults al construir, setActiveByIndex con out-of-bounds,
// captureCurrentLayout actualiza solo el activo, switching preserva
// los iniLayouts entre cambios, setWorkspaces vacio cae a defaults.

#include <doctest/doctest.h>

#include "editor/workspace/WorkspaceManager.h"

using namespace Mood;

TEST_CASE("WorkspaceManager: ctor crea 4 defaults con activo = 0") {
    WorkspaceManager m;
    CHECK(m.count() == 4u);
    CHECK(m.activeIndex() == 0);
    CHECK(m.activeWorkspace().name == "Layout");
    // Los iniLayout estan vacios — DockBuilder los construye al activar.
    for (const auto& w : m.workspaces()) {
        CHECK(w.iniLayout.empty());
    }
}

TEST_CASE("WorkspaceManager: nombres de los 4 defaults") {
    WorkspaceManager m;
    CHECK(m.workspaces()[0].name == "Layout");
    CHECK(m.workspaces()[1].name == "Scripting");
    CHECK(m.workspaces()[2].name == "Profile");
    CHECK(m.workspaces()[3].name == "Materials");
}

TEST_CASE("WorkspaceManager: setActiveByIndex valida cambia el activo") {
    WorkspaceManager m;
    m.setActiveByIndex(2);
    CHECK(m.activeIndex() == 2);
    CHECK(m.activeWorkspace().name == "Profile");
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
    CHECK(m.workspaces()[3].iniLayout.empty());
}

TEST_CASE("WorkspaceManager: switching preserva los iniLayouts") {
    WorkspaceManager m;
    // Capturar layout del activo (Layout, idx 0).
    m.captureCurrentLayout("ini-Layout");
    // Cambiar a Scripting y capturar.
    m.setActiveByIndex(1);
    m.captureCurrentLayout("ini-Scripting");
    // Volver a Layout: el iniLayout previo debe seguir ahi.
    m.setActiveByIndex(0);
    CHECK(m.activeWorkspace().iniLayout == "ini-Layout");
    // Volver a Scripting: idem.
    m.setActiveByIndex(1);
    CHECK(m.activeWorkspace().iniLayout == "ini-Scripting");
}

TEST_CASE("WorkspaceManager: setWorkspaces reemplaza la lista") {
    WorkspaceManager m;
    std::vector<Workspace> custom = {
        Workspace{"Custom1", "ini-1"},
        Workspace{"Custom2", "ini-2"},
    };
    m.setWorkspaces(std::move(custom));
    CHECK(m.count() == 2u);
    CHECK(m.activeIndex() == 0);  // resetea el activo
    CHECK(m.activeWorkspace().name == "Custom1");
    CHECK(m.workspaces()[1].name == "Custom2");
    CHECK(m.workspaces()[1].iniLayout == "ini-2");
}

TEST_CASE("WorkspaceManager: setWorkspaces con vector vacio cae a defaults") {
    WorkspaceManager m;
    m.setActiveByIndex(2);
    m.setWorkspaces({});
    CHECK(m.count() == 4u);
    CHECK(m.activeIndex() == 0);
    CHECK(m.activeWorkspace().name == "Layout");
}

TEST_CASE("WorkspaceManager: captureCurrentLayout en idx invalido es no-op") {
    // Caso defensivo: el manager no deberia entrar en este estado en
    // produccion (siempre hay >=1 workspace), pero validamos que no
    // crashea si pasara.
    WorkspaceManager m;
    m.setWorkspaces({Workspace{"Solo", ""}});
    m.setActiveByIndex(0);
    m.captureCurrentLayout("dato");
    CHECK(m.workspaces()[0].iniLayout == "dato");
    // Ahora reemplazamos por uno vacio sin tocar activeIndex; el
    // capture no debe crashear.
    m.setWorkspaces({});  // restaura defaults, activo queda 0
    m.captureCurrentLayout("otro");
    CHECK(m.activeWorkspace().iniLayout == "otro");
}
