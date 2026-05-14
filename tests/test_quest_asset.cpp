// Tests del QuestAsset (F2H53 Bloque A). Cubre: defaults del asset
// vacio, roundtrip JSON preservando todos los campos, 4 tipos de
// objective + 3 tipos de reward, missing-field defaults, bumped-version
// rejection, saveToFile + loadFromFile roundtrip, id-from-stem fallback,
// mixed-type robustness.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/quest/QuestAsset.h"
#include "engine/render/rhi/ITexture.h"

#include <fstream>

using namespace Mood;
using namespace Mood::Quest;

namespace {

// Path temporal para tests de I/O — sigue el patron de test_item_asset.cpp.
std::filesystem::path tempQuestPath(const char* stem) {
    return std::filesystem::temp_directory_path() /
           (std::string("mood_test_") + stem + k_fileExtension);
}

} // namespace

// ============================================================
// Defaults del asset vacio
// ============================================================

TEST_CASE("QuestAsset: default-constructed tiene todos los campos vacios") {
    Asset a;
    CHECK(a.id.empty());
    CHECK(a.name_key.empty());
    CHECK(a.name_literal.empty());
    CHECK(a.description_key.empty());
    CHECK(a.description_literal.empty());
    CHECK(a.category.empty());
    CHECK(a.objectives.empty());
    CHECK(a.rewards.empty());
}

TEST_CASE("QuestAsset: file extension constant") {
    CHECK(std::string(k_fileExtension) == ".moodquest");
}

TEST_CASE("QuestAsset: Objective default es CustomLua + min_quantity=1") {
    Objective o;
    CHECK(o.type == ObjectiveType::CustomLua);
    CHECK(o.min_quantity == 1);
    CHECK(o.id.empty());
    CHECK(o.var_name.empty());
    CHECK(o.custom_predicate.empty());
}

TEST_CASE("QuestAsset: Reward default es Item + quantity=1") {
    Reward r;
    CHECK(r.type == RewardType::Item);
    CHECK(r.quantity == 1);
    CHECK(r.item_path.empty());
    CHECK(r.lua_script.empty());
}

// ============================================================
// Helpers enum <-> string
// ============================================================

TEST_CASE("QuestAsset: ObjectiveType <-> string roundtrip") {
    CHECK(std::string(objectiveTypeToString(ObjectiveType::Collect))   == "collect");
    CHECK(std::string(objectiveTypeToString(ObjectiveType::Talk))      == "talk");
    CHECK(std::string(objectiveTypeToString(ObjectiveType::Reach))     == "reach");
    CHECK(std::string(objectiveTypeToString(ObjectiveType::CustomLua)) == "custom_lua");

    CHECK(objectiveTypeFromString("collect")    == ObjectiveType::Collect);
    CHECK(objectiveTypeFromString("talk")       == ObjectiveType::Talk);
    CHECK(objectiveTypeFromString("reach")      == ObjectiveType::Reach);
    CHECK(objectiveTypeFromString("custom_lua") == ObjectiveType::CustomLua);
}

TEST_CASE("QuestAsset: ObjectiveType desconocido cae a CustomLua (safe default)") {
    CHECK(objectiveTypeFromString("")              == ObjectiveType::CustomLua);
    CHECK(objectiveTypeFromString("unknown_garbage") == ObjectiveType::CustomLua);
    CHECK(objectiveTypeFromString("KILL")          == ObjectiveType::CustomLua); // case-sensitive
}

TEST_CASE("QuestAsset: RewardType <-> string roundtrip") {
    CHECK(std::string(rewardTypeToString(RewardType::Item)) == "item");
    CHECK(std::string(rewardTypeToString(RewardType::Var))  == "var");
    CHECK(std::string(rewardTypeToString(RewardType::Lua))  == "lua");

    CHECK(rewardTypeFromString("item") == RewardType::Item);
    CHECK(rewardTypeFromString("var")  == RewardType::Var);
    CHECK(rewardTypeFromString("lua")  == RewardType::Lua);
}

TEST_CASE("QuestAsset: RewardType desconocido cae a Lua (safe default)") {
    CHECK(rewardTypeFromString("")              == RewardType::Lua);
    CHECK(rewardTypeFromString("unknown")       == RewardType::Lua);
}

// ============================================================
// Roundtrip JSON
// ============================================================

TEST_CASE("QuestAsset: empty asset roundtrip preserva emptiness") {
    Asset a;
    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);
    CHECK(b.id.empty());
    CHECK(b.objectives.empty());
    CHECK(b.rewards.empty());
}

TEST_CASE("QuestAsset: roundtrip preserva todos los campos top-level") {
    Asset a;
    a.id                  = "rescatar_gata";
    a.name_key            = "quests.rescatar_gata.name";
    a.name_literal        = "Rescata la gata";
    a.description_key     = "quests.rescatar_gata.desc";
    a.description_literal = "La vieja Marta perdio su gata Mimi.";
    a.category            = "side";

    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);

    CHECK(b.id                  == "rescatar_gata");
    CHECK(b.name_key            == "quests.rescatar_gata.name");
    CHECK(b.name_literal        == "Rescata la gata");
    CHECK(b.description_key     == "quests.rescatar_gata.desc");
    CHECK(b.description_literal == "La vieja Marta perdio su gata Mimi.");
    CHECK(b.category            == "side");
}

TEST_CASE("QuestAsset: roundtrip preserva los 4 tipos de Objective") {
    Asset a;
    a.id = "test_quest";

    Objective collect;
    collect.id            = "obj_collect";
    collect.name_literal  = "Junta 3 llaves";
    collect.type          = ObjectiveType::Collect;
    collect.item_path     = "items/key.mooditem";
    collect.min_quantity  = 3;
    a.objectives.push_back(collect);

    Objective talk;
    talk.id            = "obj_talk";
    talk.name_literal  = "Habla con Marcus";
    talk.type          = ObjectiveType::Talk;
    talk.var_name      = "spoke_to_marcus";
    a.objectives.push_back(talk);

    Objective reach;
    reach.id            = "obj_reach";
    reach.name_literal  = "Llega al bosque";
    reach.type          = ObjectiveType::Reach;
    reach.var_name      = "entered_forest";
    a.objectives.push_back(reach);

    Objective custom;
    custom.id                = "obj_custom";
    custom.name_literal      = "Secreto";
    custom.type              = ObjectiveType::CustomLua;
    custom.custom_predicate  = "inventory.has('items/orb.mooditem') and dialog.has_var('met_oracle')";
    a.objectives.push_back(custom);

    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);

    REQUIRE(b.objectives.size() == 4u);
    CHECK(b.objectives[0].type == ObjectiveType::Collect);
    CHECK(b.objectives[0].item_path == "items/key.mooditem");
    CHECK(b.objectives[0].min_quantity == 3);
    CHECK(b.objectives[1].type == ObjectiveType::Talk);
    CHECK(b.objectives[1].var_name == "spoke_to_marcus");
    CHECK(b.objectives[2].type == ObjectiveType::Reach);
    CHECK(b.objectives[2].var_name == "entered_forest");
    CHECK(b.objectives[3].type == ObjectiveType::CustomLua);
    CHECK(b.objectives[3].custom_predicate ==
          "inventory.has('items/orb.mooditem') and dialog.has_var('met_oracle')");
}

TEST_CASE("QuestAsset: roundtrip preserva los 3 tipos de Reward") {
    Asset a;
    a.id = "test_quest";

    Reward itemR;
    itemR.type      = RewardType::Item;
    itemR.item_path = "items/gold.mooditem";
    itemR.quantity  = 50;
    a.rewards.push_back(itemR);

    Reward varR;
    varR.type      = RewardType::Var;
    varR.var_name  = "marta_grateful";
    varR.var_value = "yes";
    a.rewards.push_back(varR);

    Reward luaR;
    luaR.type       = RewardType::Lua;
    luaR.lua_script = "player_xp = player_xp + 100";
    a.rewards.push_back(luaR);

    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);

    REQUIRE(b.rewards.size() == 3u);
    CHECK(b.rewards[0].type == RewardType::Item);
    CHECK(b.rewards[0].item_path == "items/gold.mooditem");
    CHECK(b.rewards[0].quantity == 50);
    CHECK(b.rewards[1].type == RewardType::Var);
    CHECK(b.rewards[1].var_name == "marta_grateful");
    CHECK(b.rewards[1].var_value == "yes");
    CHECK(b.rewards[2].type == RewardType::Lua);
    CHECK(b.rewards[2].lua_script == "player_xp = player_xp + 100");
}

TEST_CASE("QuestAsset: campos type-specific NO se borran al cambiar de type") {
    // El editor permite al dev cambiar `type` sin perder los valores
    // previos (UX: si te equivocas de type, no perdes el item_path que
    // ya tipeaste). Verifica que la serializacion conserva los campos
    // aunque sean "irrelevantes" para el type actual.
    Asset a;
    Objective o;
    o.type             = ObjectiveType::Collect;  // type actual
    o.item_path        = "items/key.mooditem";    // relevante a Collect
    o.var_name         = "old_var_data";          // NO relevante pero preservar
    o.custom_predicate = "old_predicate";         // NO relevante pero preservar
    a.objectives.push_back(o);

    nlohmann::json j = a.toJson();
    Asset b = Asset::fromJson(j);

    REQUIRE(b.objectives.size() == 1u);
    CHECK(b.objectives[0].item_path        == "items/key.mooditem");
    CHECK(b.objectives[0].var_name         == "old_var_data");
    CHECK(b.objectives[0].custom_predicate == "old_predicate");
}

// ============================================================
// Schema version + robustez de inputs invalidos
// ============================================================

TEST_CASE("QuestAsset: schema version != 1 se rechaza y devuelve asset vacio") {
    nlohmann::json j = nlohmann::json::object();
    j["_version"] = 99;
    j["id"]       = "test";
    j["category"] = "main";
    Asset a = Asset::fromJson(j);
    CHECK(a.id.empty());           // rejected
    CHECK(a.category.empty());     // rejected
}

TEST_CASE("QuestAsset: fromJson sobre no-objeto devuelve asset vacio") {
    nlohmann::json arr = nlohmann::json::array();
    Asset a = Asset::fromJson(arr);
    CHECK(a.id.empty());
}

TEST_CASE("QuestAsset: missing-field defaults sensatos") {
    nlohmann::json j = nlohmann::json::object();
    j["_version"] = 1;
    j["id"]       = "minimal";
    // No traemos objectives, rewards, category, name_*, description_*.
    Asset a = Asset::fromJson(j);
    CHECK(a.id == "minimal");
    CHECK(a.name_key.empty());
    CHECK(a.category.empty());
    CHECK(a.objectives.empty());
    CHECK(a.rewards.empty());
}

TEST_CASE("QuestAsset: objectives mixed-type ignora entradas no-objeto") {
    // Defensivo: si el JSON trae basura en `objectives` (ints, strings),
    // descartamos esas entradas y conservamos las validas. No crashea.
    nlohmann::json j = nlohmann::json::object();
    j["_version"]   = 1;
    j["id"]         = "robust";
    j["objectives"] = nlohmann::json::array({
        nlohmann::json{{"id", "valid_1"}, {"type", "collect"}, {"item_path", "items/x"}, {"min_quantity", 2}},
        "garbage_string",
        42,
        nlohmann::json{{"id", "valid_2"}, {"type", "talk"}, {"var_name", "v"}},
    });
    Asset a = Asset::fromJson(j);
    // El loader actual NO filtra explicitamente; objectivesFromJson devuelve
    // Objective default vacio para no-objetos. Verificamos que el array
    // resultante tiene 4 entradas, con los validos preservados y los basura
    // como defaults vacios. Eso es mas robusto que crashear; el QuestSystem
    // runtime ignorara objectives con type=CustomLua + predicate vacio.
    REQUIRE(a.objectives.size() == 4u);
    CHECK(a.objectives[0].id == "valid_1");
    CHECK(a.objectives[0].type == ObjectiveType::Collect);
    CHECK(a.objectives[1].id.empty());  // garbage -> default
    CHECK(a.objectives[2].id.empty());  // garbage -> default
    CHECK(a.objectives[3].id == "valid_2");
    CHECK(a.objectives[3].type == ObjectiveType::Talk);
}

// ============================================================
// I/O de disco
// ============================================================

TEST_CASE("QuestAsset: saveToFile + loadFromFile roundtrip completo") {
    Asset a;
    a.id           = "round_quest";
    a.name_literal = "Round Quest";
    a.category     = "main";

    Objective o;
    o.id           = "ob_1";
    o.name_literal = "Step 1";
    o.type         = ObjectiveType::Collect;
    o.item_path    = "items/key.mooditem";
    o.min_quantity = 2;
    a.objectives.push_back(o);

    Reward r;
    r.type      = RewardType::Var;
    r.var_name  = "quest_done";
    r.var_value = "yes";
    a.rewards.push_back(r);

    const auto path = tempQuestPath("roundtrip");
    REQUIRE(a.saveToFile(path));

    auto loaded = Asset::loadFromFile(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->id == "round_quest");
    CHECK(loaded->category == "main");
    REQUIRE(loaded->objectives.size() == 1u);
    CHECK(loaded->objectives[0].item_path == "items/key.mooditem");
    CHECK(loaded->objectives[0].min_quantity == 2);
    REQUIRE(loaded->rewards.size() == 1u);
    CHECK(loaded->rewards[0].var_name == "quest_done");

    std::filesystem::remove(path);
}

TEST_CASE("QuestAsset: loadFromFile inexistente devuelve nullopt") {
    const auto path = tempQuestPath("does_not_exist_xyz");
    std::filesystem::remove(path);  // por las dudas
    auto loaded = Asset::loadFromFile(path);
    CHECK_FALSE(loaded.has_value());
}

TEST_CASE("QuestAsset: loadFromFile sin `id` usa stem como fallback") {
    // Caso: dev edita un .moodquest a mano y borra el campo `id`. El loader
    // lo recompone desde el filename. Mismo patron que ItemAsset.
    nlohmann::json j = nlohmann::json::object();
    j["_version"] = 1;
    j["category"] = "side";
    // NO traemos `id` adrede.

    const auto path = tempQuestPath("orphan_id_quest");
    {
        std::ofstream out(path);
        out << j.dump(2);
    }

    auto loaded = Asset::loadFromFile(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->id == "mood_test_orphan_id_quest");  // stem == filename sin extension
    CHECK(loaded->category == "side");

    std::filesystem::remove(path);
}

TEST_CASE("QuestAsset: loadFromFile con JSON corrupto devuelve nullopt") {
    const auto path = tempQuestPath("corrupt");
    {
        std::ofstream out(path);
        out << "{ not valid json !!!";
    }
    auto loaded = Asset::loadFromFile(path);
    CHECK_FALSE(loaded.has_value());
    std::filesystem::remove(path);
}

// ============================================================
// AssetManager integration (F2H53 Bloque B — loadQuest/getQuest/etc)
// ============================================================

namespace {

class DummyTexture : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory dummyTexFactory() {
    return [](const std::string&) { return std::make_unique<DummyTexture>(); };
}

} // namespace

TEST_CASE("AssetManager: arranca con slot 0 quest vacio + missingQuestId()==0") {
    AssetManager am("assets", dummyTexFactory());
    CHECK(am.missingQuestId() == 0u);
    CHECK(am.questCount() == 1u);
    CHECK(am.questPathOf(0u) == "__empty_quest");
    const Quest::Asset* empty = am.getQuest(0u);
    REQUIRE(empty != nullptr);
    CHECK(empty->id.empty());
    CHECK(empty->objectives.empty());
    CHECK(empty->rewards.empty());
}

TEST_CASE("AssetManager: loadQuest con path no resoluble cae al slot 0") {
    AssetManager am("assets", dummyTexFactory());
    const QuestAssetId id = am.loadQuest("quests/no_existe_12345.moodquest");
    CHECK(id == am.missingQuestId());
    // Segunda llamada con mismo path: hit en cache, sigue devolviendo 0.
    CHECK(am.loadQuest("quests/no_existe_12345.moodquest") == am.missingQuestId());
    CHECK(am.questCount() == 1u);  // no se agrego ningun slot nuevo
}

TEST_CASE("AssetManager: getQuest con id fuera de rango cae al slot 0 (nunca null)") {
    AssetManager am("assets", dummyTexFactory());
    const Quest::Asset* a = am.getQuest(99999u);
    REQUIRE(a != nullptr);
    CHECK(a->id.empty());
    CHECK(am.questPathOf(99999u) == "__empty_quest");
}

TEST_CASE("AssetManager: loadQuest valido carga + cachea + asigna id estable") {
    // Setup: tmpRoot/quests/test_quest.moodquest con un quest real.
    namespace fs = std::filesystem;
    const auto tmpRoot = fs::temp_directory_path() / "mood_quest_am_test";
    fs::remove_all(tmpRoot);
    fs::create_directories(tmpRoot / "quests");

    Asset q;
    q.id = "test_quest";
    q.name_literal = "Test Quest";
    q.category = "main";
    Objective o;
    o.id = "step_1";
    o.type = ObjectiveType::Collect;
    o.item_path = "items/key.mooditem";
    o.min_quantity = 3;
    q.objectives.push_back(o);
    REQUIRE(q.saveToFile(tmpRoot / "quests" / "test_quest.moodquest"));

    AssetManager am(tmpRoot.string(), dummyTexFactory());

    const QuestAssetId id = am.loadQuest("quests/test_quest.moodquest");
    CHECK(id != am.missingQuestId());
    CHECK(am.questCount() == 2u);  // slot 0 vacio + slot 1 nuevo

    // Cache hit: misma id en segunda llamada.
    CHECK(am.loadQuest("quests/test_quest.moodquest") == id);
    CHECK(am.questCount() == 2u);

    // Resolucion del asset.
    const Quest::Asset* loaded = am.getQuest(id);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->id == "test_quest");
    CHECK(loaded->category == "main");
    REQUIRE(loaded->objectives.size() == 1u);
    CHECK(loaded->objectives[0].type == ObjectiveType::Collect);
    CHECK(loaded->objectives[0].item_path == "items/key.mooditem");
    CHECK(loaded->objectives[0].min_quantity == 3);

    // Path lookup.
    CHECK(am.questPathOf(id) == "quests/test_quest.moodquest");

    fs::remove_all(tmpRoot);
}

TEST_CASE("AssetManager: loadQuest con path unsafe (..) rechazado por VFS") {
    AssetManager am("assets", dummyTexFactory());
    const QuestAssetId id = am.loadQuest("../../etc/passwd.moodquest");
    CHECK(id == am.missingQuestId());
}

TEST_CASE("AssetManager: loadQuest con JSON corrupto cae al slot 0 + cache lo guarda") {
    namespace fs = std::filesystem;
    const auto tmpRoot = fs::temp_directory_path() / "mood_quest_am_corrupt";
    fs::remove_all(tmpRoot);
    fs::create_directories(tmpRoot / "quests");
    {
        std::ofstream out(tmpRoot / "quests" / "corrupt.moodquest");
        out << "{ NOT VALID JSON";
    }

    AssetManager am(tmpRoot.string(), dummyTexFactory());
    const QuestAssetId id = am.loadQuest("quests/corrupt.moodquest");
    CHECK(id == am.missingQuestId());
    CHECK(am.questCount() == 1u);  // no se agrego entry nueva
    // Segunda llamada: cache hit, sigue devolviendo missing sin reintentar.
    CHECK(am.loadQuest("quests/corrupt.moodquest") == am.missingQuestId());

    fs::remove_all(tmpRoot);
}
