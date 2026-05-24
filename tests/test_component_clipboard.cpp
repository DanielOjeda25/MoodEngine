// Tests del ComponentClipboard + PasteComponentCommand (F3H9). Cubre los
// 4 componentes Tier 1 (Light, Trigger, ForceField, ParticleEmitter) +
// el wiring undo/redo via PasteComponentCommand.

#include <doctest/doctest.h>

#include "editor/commands/PasteComponentCommand.h"
#include "editor/components/ComponentClipboard.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <nlohmann/json.hpp>

#include <memory>

using namespace Mood;
namespace CC = Mood::ComponentClipboard;

namespace {

// Minimal stub ITexture (mismo patron que test_asset_manager.cpp).
// AssetManager requiere una TextureFactory funcional para construirse.
class StubTexture : public ITexture {
public:
    void bind(u32) const override {}
    void unbind() const override {}
    u32 width() const override { return 1u; }
    u32 height() const override { return 1u; }
    TextureHandle handle() const override { return TextureHandle{}; }
};

std::unique_ptr<AssetManager> makeAssets() {
    auto factory = [](const std::string&) -> std::unique_ptr<ITexture> {
        return std::make_unique<StubTexture>();
    };
    return std::make_unique<AssetManager>("assets", factory);
}

} // namespace

TEST_CASE("ComponentClipboard::isSupported acepta Tier 1, rechaza desconocidos") {
    CHECK(CC::isSupported(CC::kKeyLight));
    CHECK(CC::isSupported(CC::kKeyTrigger));
    CHECK(CC::isSupported(CC::kKeyForceField));
    CHECK(CC::isSupported(CC::kKeyParticleEmitter));
    CHECK_FALSE(CC::isSupported("mesh_renderer"));    // Tier 2
    CHECK_FALSE(CC::isSupported("rigid_body"));       // Tier 2
    CHECK_FALSE(CC::isSupported("transform"));        // excluido
    CHECK_FALSE(CC::isSupported(""));
    CHECK_FALSE(CC::isSupported("garbage"));
}

TEST_CASE("ComponentClipboard::componentNameKey devuelve la key i18n") {
    CHECK(CC::componentNameKey(CC::kKeyLight) == "component.name.light");
    CHECK(CC::componentNameKey(CC::kKeyTrigger) == "component.name.trigger");
    CHECK(CC::componentNameKey("garbage").empty());
}

TEST_CASE("ComponentClipboard::entityHasComponent matchea hasComponent<T>") {
    Scene scene;
    Entity e = scene.createEntity("Test");

    CHECK_FALSE(CC::entityHasComponent(CC::kKeyLight, e));
    e.addComponent<LightComponent>();
    CHECK(CC::entityHasComponent(CC::kKeyLight, e));

    CHECK_FALSE(CC::entityHasComponent(CC::kKeyTrigger, e));
    e.addComponent<TriggerComponent>();
    CHECK(CC::entityHasComponent(CC::kKeyTrigger, e));
}

TEST_CASE("Roundtrip Light: serialize -> applyPayload mantiene fields") {
    auto assets = makeAssets();
    Scene scene;
    Entity src = scene.createEntity("A");
    auto& lt = src.addComponent<LightComponent>();
    lt.type = LightComponent::Type::Point;
    lt.color = glm::vec3(0.5f, 0.7f, 0.2f);
    lt.intensity = 4.5f;
    lt.radius = 8.0f;
    lt.enabled = true;

    auto payload = CC::serializeComponent(CC::kKeyLight, src, *assets);
    REQUIRE_FALSE(payload.is_null());

    Entity dst = scene.createEntity("B");
    REQUIRE(CC::applyPayload(CC::kKeyLight, payload, dst, *assets));
    REQUIRE(dst.hasComponent<LightComponent>());
    const auto& dlt = dst.getComponent<LightComponent>();
    CHECK(dlt.color.x == doctest::Approx(0.5f));
    CHECK(dlt.color.y == doctest::Approx(0.7f));
    CHECK(dlt.intensity == doctest::Approx(4.5f));
    CHECK(dlt.radius == doctest::Approx(8.0f));
    CHECK(dlt.enabled);
}

TEST_CASE("Roundtrip Trigger: requiredTag + flags preservados") {
    auto assets = makeAssets();
    Scene scene;
    Entity src = scene.createEntity("A");
    auto& tr = src.addComponent<TriggerComponent>();
    tr.halfExtents = glm::vec3(2.0f, 1.5f, 3.0f);
    tr.requiredTag = "VIP";
    tr.triggersOnPlayer = false;
    tr.oneShot = true;
    tr.enabled = true;

    auto payload = CC::serializeComponent(CC::kKeyTrigger, src, *assets);
    REQUIRE_FALSE(payload.is_null());

    Entity dst = scene.createEntity("B");
    REQUIRE(CC::applyPayload(CC::kKeyTrigger, payload, dst, *assets));
    const auto& dtr = dst.getComponent<TriggerComponent>();
    CHECK(dtr.halfExtents.x == doctest::Approx(2.0f));
    CHECK(dtr.halfExtents.z == doctest::Approx(3.0f));
    CHECK(dtr.requiredTag == "VIP");
    CHECK_FALSE(dtr.triggersOnPlayer);
    CHECK(dtr.oneShot);
    CHECK(dtr.enabled);
}

TEST_CASE("Roundtrip ForceField: shape + mode + strength preservados") {
    auto assets = makeAssets();
    Scene scene;
    Entity src = scene.createEntity("A");
    auto& ff = src.addComponent<ForceFieldComponent>();
    ff.shape = ForceFieldComponent::Shape::Box;
    ff.mode = ForceFieldComponent::Mode::Directional;
    ff.halfExtents = glm::vec3(3.0f);
    ff.direction = glm::vec3(1.0f, 0.0f, 0.0f);
    ff.strength = 25.0f;
    ff.ignoreMass = true;

    auto payload = CC::serializeComponent(CC::kKeyForceField, src, *assets);
    REQUIRE_FALSE(payload.is_null());

    Entity dst = scene.createEntity("B");
    REQUIRE(CC::applyPayload(CC::kKeyForceField, payload, dst, *assets));
    const auto& dff = dst.getComponent<ForceFieldComponent>();
    CHECK(dff.shape == ForceFieldComponent::Shape::Box);
    CHECK(dff.mode == ForceFieldComponent::Mode::Directional);
    CHECK(dff.strength == doctest::Approx(25.0f));
    CHECK(dff.ignoreMass);
}

TEST_CASE("applyPayload sobre componente existente sobrescribe los fields") {
    auto assets = makeAssets();
    Scene scene;
    Entity src = scene.createEntity("A");
    auto& slt = src.addComponent<LightComponent>();
    slt.color = glm::vec3(1.0f, 0.0f, 0.0f);

    Entity dst = scene.createEntity("B");
    auto& dlt = dst.addComponent<LightComponent>();
    dlt.color = glm::vec3(0.0f, 0.0f, 1.0f);  // azul

    auto payload = CC::serializeComponent(CC::kKeyLight, src, *assets);
    CC::applyPayload(CC::kKeyLight, payload, dst, *assets);

    // dst tenia azul, ahora deberia tener rojo.
    CHECK(dst.getComponent<LightComponent>().color.x == doctest::Approx(1.0f));
    CHECK(dst.getComponent<LightComponent>().color.z == doctest::Approx(0.0f));
}

TEST_CASE("removeComponent quita el componente identificado por key") {
    Scene scene;
    Entity e = scene.createEntity("A");
    e.addComponent<LightComponent>();
    REQUIRE(e.hasComponent<LightComponent>());
    CHECK(CC::removeComponent(CC::kKeyLight, e));
    CHECK_FALSE(e.hasComponent<LightComponent>());
    // Llamar 2da vez devuelve false (ya no esta).
    CHECK_FALSE(CC::removeComponent(CC::kKeyLight, e));
}

TEST_CASE("applyPayload rechaza componentKey desconocido") {
    auto assets = makeAssets();
    Scene scene;
    Entity e = scene.createEntity("A");
    CHECK_FALSE(CC::applyPayload("mesh_renderer",
                                   nlohmann::json::object(), e, *assets));
    CHECK_FALSE(CC::applyPayload("garbage",
                                   nlohmann::json::object(), e, *assets));
}

TEST_CASE("PasteComponentCommand: execute aplica after, undo restaura before (hadComponentBefore=true)") {
    auto assets = makeAssets();
    Scene scene;
    Entity src = scene.createEntity("A");
    src.addComponent<LightComponent>().color = glm::vec3(1.0f, 0.0f, 0.0f);
    auto beforePayload = CC::serializeComponent(CC::kKeyLight, src, *assets);

    Entity dst = scene.createEntity("B");
    dst.addComponent<LightComponent>().color = glm::vec3(0.0f, 1.0f, 0.0f);  // verde
    auto dstBeforePayload = CC::serializeComponent(CC::kKeyLight, dst, *assets);

    PasteComponentCommand cmd(dst, CC::kKeyLight,
                                dstBeforePayload,   // before = verde
                                beforePayload,       // after = rojo (de src)
                                /*hadComponentBefore=*/true,
                                assets.get(), "Test paste");
    CHECK_FALSE(cmd.isNoOp());

    cmd.execute();
    CHECK(dst.getComponent<LightComponent>().color.x == doctest::Approx(1.0f));  // rojo

    cmd.undo();
    CHECK(dst.getComponent<LightComponent>().color.y == doctest::Approx(1.0f));  // verde restaurado
}

TEST_CASE("PasteComponentCommand: hadComponentBefore=false -> undo remueve el componente") {
    auto assets = makeAssets();
    Scene scene;
    Entity src = scene.createEntity("A");
    src.addComponent<LightComponent>();
    auto payload = CC::serializeComponent(CC::kKeyLight, src, *assets);

    Entity dst = scene.createEntity("B");
    REQUIRE_FALSE(dst.hasComponent<LightComponent>());

    PasteComponentCommand cmd(dst, CC::kKeyLight,
                                nlohmann::json{},   // before vacio
                                payload,
                                /*hadComponentBefore=*/false,
                                assets.get(), "Paste as new");
    cmd.execute();
    CHECK(dst.hasComponent<LightComponent>());

    cmd.undo();
    CHECK_FALSE(dst.hasComponent<LightComponent>());

    cmd.execute();  // redo
    CHECK(dst.hasComponent<LightComponent>());
}

TEST_CASE("PasteComponentCommand: skip silencioso si la entidad fue destruida") {
    auto assets = makeAssets();
    Scene scene;
    Entity src = scene.createEntity("A");
    src.addComponent<LightComponent>();
    auto payload = CC::serializeComponent(CC::kKeyLight, src, *assets);

    Entity dst = scene.createEntity("B");
    PasteComponentCommand cmd(dst, CC::kKeyLight, nlohmann::json{},
                                payload, false, assets.get(), "Test");
    scene.destroyEntity(dst);
    // No debe crashear.
    cmd.execute();
    cmd.undo();
}
