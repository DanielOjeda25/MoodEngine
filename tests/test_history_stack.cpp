// Tests del HistoryStack del editor (Hito 27 Bloque 1). Pure C++ sin GL,
// con un comando mock que muta un int para verificar el ciclo undo/redo.

#include <doctest/doctest.h>

#include "editor/commands/Command.h"
#include "editor/commands/HistoryStack.h"

#include <memory>
#include <string>

using namespace Mood;

namespace {

/// Comando mock: setea un int al valor `to`, undo lo deja en `from`.
class SetIntCommand : public ICommand {
public:
    SetIntCommand(int& target, int from, int to, std::string label)
        : m_target(target), m_from(from), m_to(to), m_label(std::move(label)) {}
    void execute() override { m_target = m_to; }
    void undo() override    { m_target = m_from; }
    std::string name() const override { return m_label; }
private:
    int& m_target;
    int m_from;
    int m_to;
    std::string m_label;
};

} // namespace

TEST_CASE("HistoryStack: estado inicial vacio") {
    HistoryStack h;
    CHECK_FALSE(h.canUndo());
    CHECK_FALSE(h.canRedo());
    CHECK(h.undoCount() == 0u);
    CHECK(h.redoCount() == 0u);
    CHECK(h.undoName().empty());
    CHECK(h.redoName().empty());
}

TEST_CASE("HistoryStack: push ejecuta el comando + lo guarda en undo") {
    HistoryStack h;
    int v = 0;
    h.push(std::make_unique<SetIntCommand>(v, 0, 7, "Set 7"));
    CHECK(v == 7);
    CHECK(h.canUndo());
    CHECK_FALSE(h.canRedo());
    CHECK(h.undoName() == "Set 7");
}

TEST_CASE("HistoryStack: undo revierte y mueve al redo stack") {
    HistoryStack h;
    int v = 0;
    h.push(std::make_unique<SetIntCommand>(v, 0, 7, "Set 7"));
    REQUIRE(v == 7);

    REQUIRE(h.undo() == true);
    CHECK(v == 0);
    CHECK_FALSE(h.canUndo());
    CHECK(h.canRedo());
    CHECK(h.redoName() == "Set 7");
}

TEST_CASE("HistoryStack: redo re-ejecuta y vuelve al undo stack") {
    HistoryStack h;
    int v = 0;
    h.push(std::make_unique<SetIntCommand>(v, 0, 7, "Set 7"));
    h.undo();
    REQUIRE(v == 0);

    REQUIRE(h.redo() == true);
    CHECK(v == 7);
    CHECK(h.canUndo());
    CHECK_FALSE(h.canRedo());
}

TEST_CASE("HistoryStack: undo/redo no-op cuando stacks vacios") {
    HistoryStack h;
    CHECK_FALSE(h.undo()); // nada que deshacer
    CHECK_FALSE(h.redo()); // nada que rehacer

    int v = 0;
    h.push(std::make_unique<SetIntCommand>(v, 0, 1, "S"));
    h.undo();
    CHECK(h.canRedo());
    h.redo();
    CHECK_FALSE(h.canRedo());
}

TEST_CASE("HistoryStack: nuevo push tras undo limpia el redo stack") {
    // Convencion estandar de cualquier editor: si deshaces y luego haces
    // algo nuevo, lo deshecho deja de ser alcanzable.
    HistoryStack h;
    int v = 0;
    h.push(std::make_unique<SetIntCommand>(v, 0, 1, "Set 1"));
    h.push(std::make_unique<SetIntCommand>(v, 1, 2, "Set 2"));
    h.undo();
    REQUIRE(h.canRedo());

    h.push(std::make_unique<SetIntCommand>(v, 1, 99, "Set 99"));
    CHECK(v == 99);
    CHECK_FALSE(h.canRedo());
    CHECK(h.undoName() == "Set 99");
}

TEST_CASE("HistoryStack: cap k_maxSize descarta el comando mas viejo") {
    HistoryStack h;
    int v = 0;
    // Empujamos 1 mas que el cap. El primero (i=0) debe quedar fuera.
    for (usize i = 0; i < HistoryStack::k_maxSize + 1; ++i) {
        h.push(std::make_unique<SetIntCommand>(v, static_cast<int>(i),
                                                 static_cast<int>(i + 1),
                                                 "S" + std::to_string(i)));
    }
    CHECK(h.undoCount() == HistoryStack::k_maxSize);
    // Tras deshacer todos, el v deberia volver al valor previo al comando
    // mas viejo del stack actual — que es i=1, from=1. NO al estado
    // original (v=0), porque ese se perdio al evictar i=0.
    while (h.canUndo()) h.undo();
    CHECK(v == 1);
}

TEST_CASE("HistoryStack: push de comando null es no-op") {
    HistoryStack h;
    h.push(nullptr);
    CHECK_FALSE(h.canUndo());
}

TEST_CASE("HistoryStack: clear vacia ambos stacks") {
    HistoryStack h;
    int v = 0;
    h.push(std::make_unique<SetIntCommand>(v, 0, 1, "A"));
    h.push(std::make_unique<SetIntCommand>(v, 1, 2, "B"));
    h.undo();
    REQUIRE(h.canUndo());
    REQUIRE(h.canRedo());

    h.clear();
    CHECK_FALSE(h.canUndo());
    CHECK_FALSE(h.canRedo());
}

TEST_CASE("HistoryStack: ciclo execute/undo varias veces preserva el estado") {
    // Garantia de la convencion: execute->undo->execute->undo deja v en el
    // mismo estado inicial despues de cualquier numero impar de undos.
    HistoryStack h;
    int v = 100;
    h.push(std::make_unique<SetIntCommand>(v, 100, 200, "Set 200"));
    REQUIRE(v == 200);
    h.undo(); CHECK(v == 100);
    h.redo(); CHECK(v == 200);
    h.undo(); CHECK(v == 100);
    h.redo(); CHECK(v == 200);
}
