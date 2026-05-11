// Tests del DialogAsset (F2H47 Bloque G). Cubre: addLine con sockets
// default, parseLine/writeLine roundtrip, auto-sync de choices ↔ output
// sockets, serializacion JSON, schema versioning, metadata preservation,
// first-line-marks-start invariant.

#include <doctest/doctest.h>

#include "engine/dialog/DialogAsset.h"
#include "engine/nodegraph/Graph.h"

using namespace Mood;
using namespace Mood::NodeGraph;
using namespace Mood::Dialog;

// ============================================================
// addLine
// ============================================================

TEST_CASE("DialogAsset: empty asset has invalid start_node + empty graph") {
    Asset a;
    CHECK(a.metadata().start_node_id == k_invalidNodeId);
    CHECK(a.graph().nodeCount() == 0);
}

TEST_CASE("DialogAsset: first addLine sets it as start_node") {
    Asset a;
    NodeId first = a.addLine({0.0f, 0.0f});
    CHECK(first != k_invalidNodeId);
    CHECK(a.metadata().start_node_id == first);

    NodeId second = a.addLine({100.0f, 0.0f});
    // start_node NO se reasigna al agregar mas lineas.
    CHECK(a.metadata().start_node_id == first);
    CHECK(second != first);
}

TEST_CASE("DialogAsset: addLine creates node with default sockets (1 in + 1 out)") {
    Asset a;
    NodeId id = a.addLine({0.0f, 0.0f});
    const Node* n = a.graph().findNode(id);
    REQUIRE(n != nullptr);
    CHECK(n->typeTag == k_typeDialogLine);
    CHECK(n->inputs.size() == 1);
    CHECK(n->outputs.size() == 1);
    CHECK(n->inputs[0].typeTag == k_socketFlow);
    CHECK(n->outputs[0].typeTag == k_socketFlow);
}

// ============================================================
// parseLine / writeLine roundtrip
// ============================================================

TEST_CASE("DialogAsset: parseLine on freshly-added node returns empty fields") {
    Asset a;
    NodeId id = a.addLine({0.0f, 0.0f});
    Line line = a.parseLine(id);
    CHECK(line.text_key.empty());
    CHECK(line.text_literal.empty());
    CHECK(line.portrait.empty());
    CHECK(line.audio.empty());
    CHECK(line.animation.empty());
    CHECK(line.choices.empty());
}

TEST_CASE("DialogAsset: writeLine + parseLine roundtrip preserves all fields") {
    Asset a;
    NodeId id = a.addLine({0.0f, 0.0f});

    Line in;
    in.text_key     = "dialog.intro.line_1";
    in.text_literal = "Hola, viajero!";
    in.portrait     = "characters/guard.png";
    in.audio        = "audio/voice/intro_1.ogg";
    in.animation    = "talk_idle";
    in.choices = {
        Choice{"dialog.intro.choice_yes", "Sí",        "",            "dialog.set_var('greeted', true)"},
        Choice{"dialog.intro.choice_no",  "No, gracias", "has_quest()", ""},
    };
    a.writeLine(id, in);

    Line out = a.parseLine(id);
    CHECK(out.text_key == "dialog.intro.line_1");
    CHECK(out.text_literal == "Hola, viajero!");
    CHECK(out.portrait == "characters/guard.png");
    CHECK(out.audio == "audio/voice/intro_1.ogg");
    CHECK(out.animation == "talk_idle");
    REQUIRE(out.choices.size() == 2);
    CHECK(out.choices[0].label_key == "dialog.intro.choice_yes");
    CHECK(out.choices[0].on_select_lua == "dialog.set_var('greeted', true)");
    CHECK(out.choices[1].label_key == "dialog.intro.choice_no");
    CHECK(out.choices[1].condition_lua == "has_quest()");
}

TEST_CASE("DialogAsset: parseLine on non-dialog_line node returns empty Line") {
    Asset a;
    NodeId nonDialog = a.graph().addNode("custom_type", {0.0f, 0.0f});
    Line line = a.parseLine(nonDialog);
    CHECK(line.text_key.empty());
    CHECK(line.choices.empty());
}

// ============================================================
// Auto-sync sockets ↔ choices
// ============================================================

TEST_CASE("DialogAsset: writeLine with empty choices keeps exactly 1 output socket (continue)") {
    Asset a;
    NodeId id = a.addLine({0.0f, 0.0f});
    Line line;
    line.text_literal = "End of conversation.";
    a.writeLine(id, line);

    const Node* n = a.graph().findNode(id);
    REQUIRE(n != nullptr);
    CHECK(n->outputs.size() == 1);
    CHECK(n->outputs[0].name == "continue");
}

TEST_CASE("DialogAsset: writeLine with 3 choices syncs to 3 output sockets") {
    Asset a;
    NodeId id = a.addLine({0.0f, 0.0f});
    Line line;
    line.choices = {
        Choice{"a", "", "", ""},
        Choice{"b", "", "", ""},
        Choice{"c", "", "", ""},
    };
    a.writeLine(id, line);

    const Node* n = a.graph().findNode(id);
    REQUIRE(n != nullptr);
    CHECK(n->outputs.size() == 3);
    CHECK(n->outputs[0].name == "choice_0");
    CHECK(n->outputs[1].name == "choice_1");
    CHECK(n->outputs[2].name == "choice_2");
}

TEST_CASE("DialogAsset: writeLine shrinking choices removes surplus sockets") {
    Asset a;
    NodeId id = a.addLine({0.0f, 0.0f});
    Line line;
    // Primero, crecer a 3 choices.
    line.choices = {Choice{"a","","",""}, Choice{"b","","",""}, Choice{"c","","",""}};
    a.writeLine(id, line);
    CHECK(a.graph().findNode(id)->outputs.size() == 3);
    // Luego shrink a 1 choice.
    line.choices = {Choice{"only","","",""}};
    a.writeLine(id, line);
    const Node* n = a.graph().findNode(id);
    CHECK(n->outputs.size() == 1);
    CHECK(n->outputs[0].name == "choice_0");
}

TEST_CASE("DialogAsset: writeLine shrinking preserves link on outputs[0] (renamed, ID stable)") {
    Asset a;
    NodeId src = a.addLine({0.0f, 0.0f});
    NodeId dst = a.addLine({200.0f, 0.0f});
    Line line;
    line.choices = {Choice{"a","","",""}, Choice{"b","","",""}};
    a.writeLine(src, line);
    const Node* nSrc = a.graph().findNode(src);
    const Node* nDst = a.graph().findNode(dst);
    REQUIRE(nSrc->outputs.size() == 2);
    REQUIRE(nDst->inputs.size() == 1);
    a.graph().addLink(nSrc->outputs[0].id, nDst->inputs[0].id);
    CHECK(a.graph().linkCount() == 1);

    // Shrink a empty choices: deja 1 socket "continue" (== outputs[0]).
    // El socket NO se borra (su ID es estable, solo renombra) → el link
    // sobrevive.
    line.choices.clear();
    a.writeLine(src, line);
    CHECK(a.graph().findNode(src)->outputs.size() == 1);
    CHECK(a.graph().linkCount() == 1);
}

TEST_CASE("DialogAsset: writeLine shrinking from 2 to 1 borra outputs[1] + link incidente") {
    Asset a;
    NodeId src = a.addLine({0.0f, 0.0f});
    NodeId dst = a.addLine({200.0f, 0.0f});
    Line line;
    line.choices = {Choice{"a","","",""}, Choice{"b","","",""}};
    a.writeLine(src, line);
    const Node* nSrc = a.graph().findNode(src);
    const Node* nDst = a.graph().findNode(dst);
    REQUIRE(nSrc->outputs.size() == 2);
    // Linkear OUTPUTS[1] (el que va a desaparecer) a dst.input[0].
    LinkId surplusLink = a.graph().addLink(nSrc->outputs[1].id, nDst->inputs[0].id);
    REQUIRE(surplusLink != k_invalidLinkId);
    CHECK(a.graph().linkCount() == 1);

    // Shrink a 1 choice => outputs[1] desaparece + cascade del link.
    line.choices = {Choice{"only","","",""}};
    a.writeLine(src, line);
    CHECK(a.graph().findNode(src)->outputs.size() == 1);
    CHECK(a.graph().linkCount() == 0);  // el link incidente fue borrado
}

// ============================================================
// Serializacion roundtrip
// ============================================================

TEST_CASE("DialogAsset: empty asset roundtrip preserves emptiness") {
    Asset a;
    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);
    CHECK(b.metadata().start_node_id == k_invalidNodeId);
    CHECK(b.graph().nodeCount() == 0);
}

TEST_CASE("DialogAsset: roundtrip preserves metadata + graph + customData") {
    Asset a;
    a.metadata().name = "intro_npc_guard";
    a.metadata().default_portrait = "characters/guard.png";
    a.metadata().default_audio_bus = "voice";
    NodeId n1 = a.addLine({10.0f, 20.0f});
    NodeId n2 = a.addLine({110.0f, 20.0f});

    Line line1;
    line1.text_key = "dialog.intro.line_1";
    line1.choices = {Choice{"dialog.intro.choice_yes", "Sí", "", "set_flag()"}};
    a.writeLine(n1, line1);

    // Linkear n1.output[0] → n2.input[0] (el "choice_0" → next).
    const Node* nN1 = a.graph().findNode(n1);
    const Node* nN2 = a.graph().findNode(n2);
    a.graph().addLink(nN1->outputs[0].id, nN2->inputs[0].id);

    // Roundtrip.
    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);

    CHECK(b.metadata().name == "intro_npc_guard");
    CHECK(b.metadata().start_node_id == n1);  // ID preservado
    CHECK(b.metadata().default_portrait == "characters/guard.png");
    CHECK(b.graph().nodeCount() == 2);
    CHECK(b.graph().linkCount() == 1);
    Line restored = b.parseLine(n1);
    CHECK(restored.text_key == "dialog.intro.line_1");
    REQUIRE(restored.choices.size() == 1);
    CHECK(restored.choices[0].label_literal == "Sí");
    CHECK(restored.choices[0].on_select_lua == "set_flag()");
}

TEST_CASE("DialogAsset: fromJson rejects wrong schema version") {
    nlohmann::json j;
    j["_version"] = 999u;
    j["graph"] = nlohmann::json::object();
    j["metadata"] = nlohmann::json::object();
    Asset a = Asset::fromJson(j);
    CHECK(a.graph().nodeCount() == 0);
    CHECK(a.metadata().name.empty());
}

TEST_CASE("DialogAsset: fromJson rejects non-object JSON") {
    nlohmann::json arr = nlohmann::json::array();
    Asset a = Asset::fromJson(arr);
    CHECK(a.graph().nodeCount() == 0);
}

TEST_CASE("DialogAsset: file extension constant") {
    CHECK(std::string(k_fileExtension) == ".mooddialog");
}

// ============================================================
// I/O de disco (loadFromFile / saveToFile)
// ============================================================

TEST_CASE("DialogAsset: saveToFile + loadFromFile roundtrip preserva todo") {
    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "mood_dialog_io_test";
    fs::create_directories(tmpDir);
    const fs::path filePath = tmpDir / "intro.mooddialog";
    fs::remove(filePath);

    Asset a;
    a.metadata().name = "intro_test";
    a.metadata().default_portrait = "guard.png";
    NodeId n1 = a.addLine({0, 0});
    Line line1;
    line1.text_literal = "Hola viajero";
    line1.choices = {Choice{"", "Continuar", "", ""}};
    a.writeLine(n1, line1);

    REQUIRE(a.saveToFile(filePath));
    REQUIRE(fs::exists(filePath));

    auto loaded = Asset::loadFromFile(filePath);
    REQUIRE(loaded.has_value());
    CHECK(loaded->metadata().name == "intro_test");
    CHECK(loaded->metadata().default_portrait == "guard.png");
    CHECK(loaded->graph().nodeCount() == 1);
    Line outLine = loaded->parseLine(n1);
    CHECK(outLine.text_literal == "Hola viajero");
    REQUIRE(outLine.choices.size() == 1);
    CHECK(outLine.choices[0].label_literal == "Continuar");

    fs::remove(filePath);
}

TEST_CASE("DialogAsset: loadFromFile retorna nullopt para archivo inexistente") {
    namespace fs = std::filesystem;
    const fs::path noPath = fs::temp_directory_path() / "no_existe_12345.mooddialog";
    auto loaded = Asset::loadFromFile(noPath);
    CHECK_FALSE(loaded.has_value());
}

TEST_CASE("DialogAsset: loadFromFile usa stem si metadata.name vacio") {
    namespace fs = std::filesystem;
    const fs::path tmpDir = fs::temp_directory_path() / "mood_dialog_io_stem";
    fs::create_directories(tmpDir);
    const fs::path filePath = tmpDir / "auto_named.mooddialog";

    Asset a;
    // NO seteo metadata.name. Tampoco agrego nodos.
    REQUIRE(a.saveToFile(filePath));
    auto loaded = Asset::loadFromFile(filePath);
    REQUIRE(loaded.has_value());
    CHECK(loaded->metadata().name == "auto_named");

    fs::remove(filePath);
}
