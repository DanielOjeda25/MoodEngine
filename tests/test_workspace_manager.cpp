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
    // F2H22: nombres orientados a tareas (era Layout / Scripting /
    // Profile / Materials).
    CHECK(m.activeWorkspace().name == "Modelar");
    // Los iniLayout estan vacios — DockBuilder los construye al activar.
    for (const auto& w : m.workspaces()) {
        CHECK(w.iniLayout.empty());
    }
}

TEST_CASE("WorkspaceManager: nombres de los 4 defaults (F2H22)") {
    WorkspaceManager m;
    CHECK(m.workspaces()[0].name == "Modelar");
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Optimizar");
    CHECK(m.workspaces()[3].name == "Materiales");
}

TEST_CASE("WorkspaceManager: setActiveByIndex valida cambia el activo") {
    WorkspaceManager m;
    m.setActiveByIndex(2);
    CHECK(m.activeIndex() == 2);
    CHECK(m.activeWorkspace().name == "Optimizar");
    m.setActiveByIndex(0);
    CHECK(m.activeIndex() == 0);
    CHECK(m.activeWorkspace().name == "Modelar");
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
    CHECK(m.activeWorkspace().name == "Modelar");
}

// ============================================================
// F2H22: migración de nombres viejos (Layout/Scripting/Profile/
// Materials) -> nuevos (Modelar/Programar/Optimizar/Materiales)
// ============================================================

TEST_CASE("WorkspaceManager: setWorkspaces migra nombres viejos a nuevos") {
    WorkspaceManager m;
    std::vector<Workspace> oldNamed = {
        Workspace{"Layout",    "ini1"},
        Workspace{"Scripting", "ini2"},
        Workspace{"Profile",   "ini3"},
        Workspace{"Materials", "ini4"},
    };
    m.setWorkspaces(std::move(oldNamed));

    REQUIRE(m.count() == 4u);
    CHECK(m.workspaces()[0].name == "Modelar");
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Optimizar");
    CHECK(m.workspaces()[3].name == "Materiales");

    // El iniLayout se preserva intacto — la migracion es solo del label.
    CHECK(m.workspaces()[0].iniLayout == "ini1");
    CHECK(m.workspaces()[1].iniLayout == "ini2");
    CHECK(m.workspaces()[2].iniLayout == "ini3");
    CHECK(m.workspaces()[3].iniLayout == "ini4");
}

TEST_CASE("WorkspaceManager: setWorkspaces con mezcla viejos/nuevos") {
    WorkspaceManager m;
    std::vector<Workspace> mixed = {
        Workspace{"Modelar",   "a"},  // ya nuevo
        Workspace{"Scripting", "b"},  // viejo -> Programar
        Workspace{"Optimizar", "c"},  // ya nuevo
        Workspace{"Materials", "d"},  // viejo -> Materiales
    };
    m.setWorkspaces(std::move(mixed));

    CHECK(m.workspaces()[0].name == "Modelar");
    CHECK(m.workspaces()[1].name == "Programar");
    CHECK(m.workspaces()[2].name == "Optimizar");
    CHECK(m.workspaces()[3].name == "Materiales");
}

TEST_CASE("WorkspaceManager: setWorkspaces preserva nombres custom no reconocidos") {
    WorkspaceManager m;
    std::vector<Workspace> custom = {
        Workspace{"MiCustom",  "x"},
        Workspace{"Otro",      "y"},
    };
    m.setWorkspaces(std::move(custom));

    REQUIRE(m.count() == 2u);
    CHECK(m.workspaces()[0].name == "MiCustom");
    CHECK(m.workspaces()[1].name == "Otro");
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
