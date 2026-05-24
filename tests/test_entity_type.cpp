// Tests del EntityType + EntityTypeTable (F3H9 Stage 2). Cubre:
//   - enum <-> string roundtrip.
//   - baseComponentKeys per type.
//   - inferFromEntity con componentes presentes (back-compat).
//   - tag-based inference para Tile/Floor.

#include <doctest/doctest.h>

#include "engine/scene/components/BrushComponent.h"  // no en Components.h
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/entity_type/EntityType.h"
#include "engine/scene/entity_type/EntityTypeTable.h"

using namespace Mood;
namespace ETT = Mood::EntityTypeTable;

TEST_CASE("EntityType: toString roundtrip por todos los types") {
    const EntityType all[] = {
        EntityType::Generic, EntityType::Light, EntityType::Camera,
        EntityType::Audio, EntityType::Trigger, EntityType::ForceField,
        EntityType::ParticleEmitter, EntityType::Environment,
        EntityType::Npc, EntityType::Pickable, EntityType::Brush,
        EntityType::Mesh, EntityType::Vehicle, EntityType::Tile,
    };
    for (EntityType t : all) {
        const auto s = ETT::toString(t);
        CHECK_FALSE(s.empty());
        CHECK(ETT::fromString(s) == t);
    }
}

TEST_CASE("EntityType: fromString desconocido cae a Generic (back-compat)") {
    CHECK(ETT::fromString("garbage") == EntityType::Generic);
    CHECK(ETT::fromString("") == EntityType::Generic);
    CHECK(ETT::fromString("future_type") == EntityType::Generic);
}

TEST_CASE("EntityTypeTable: baseComponentKeys por type") {
    CHECK(ETT::baseComponentKeys(EntityType::Light) == std::vector<std::string>{"light"});
    CHECK(ETT::baseComponentKeys(EntityType::Trigger) == std::vector<std::string>{"trigger"});
    CHECK(ETT::baseComponentKeys(EntityType::Generic).empty());
    // Tipos compuestos: 2 bases ambos.
    const auto npcBases = ETT::baseComponentKeys(EntityType::Npc);
    CHECK(npcBases.size() == 2u);
    CHECK(npcBases[0] == "trigger");
    CHECK(npcBases[1] == "dialog");
    const auto pickBases = ETT::baseComponentKeys(EntityType::Pickable);
    CHECK(pickBases.size() == 2u);
    CHECK(pickBases[0] == "trigger");
    CHECK(pickBases[1] == "item_pickup");
}

TEST_CASE("EntityTypeTable: isBaseComponent") {
    CHECK(ETT::isBaseComponent(EntityType::Light, "light"));
    CHECK_FALSE(ETT::isBaseComponent(EntityType::Light, "trigger"));
    // NPC tiene 2 bases.
    CHECK(ETT::isBaseComponent(EntityType::Npc, "trigger"));
    CHECK(ETT::isBaseComponent(EntityType::Npc, "dialog"));
    CHECK_FALSE(ETT::isBaseComponent(EntityType::Npc, "light"));
    // Generic no tiene bases — cualquier componente es removable.
    CHECK_FALSE(ETT::isBaseComponent(EntityType::Generic, "light"));
    CHECK_FALSE(ETT::isBaseComponent(EntityType::Generic, "trigger"));
}

TEST_CASE("inferFromEntity: Light por LightComponent") {
    Scene scene;
    Entity e = scene.createEntity("A");
    e.addComponent<LightComponent>();
    CHECK(ETT::inferFromEntity(e) == EntityType::Light);
}

TEST_CASE("inferFromEntity: NPC tiene prioridad sobre Trigger solo") {
    Scene scene;
    Entity e = scene.createEntity("A");
    e.addComponent<TriggerComponent>();
    e.addComponent<DialogComponent>();
    CHECK(ETT::inferFromEntity(e) == EntityType::Npc);
}

TEST_CASE("inferFromEntity: Pickable (Trigger + ItemPickup) tiene prioridad sobre Trigger solo") {
    Scene scene;
    Entity e = scene.createEntity("A");
    e.addComponent<TriggerComponent>();
    e.addComponent<ItemPickupComponent>();
    CHECK(ETT::inferFromEntity(e) == EntityType::Pickable);
}

TEST_CASE("inferFromEntity: Trigger solo cuando no hay dialog/itempickup") {
    Scene scene;
    Entity e = scene.createEntity("A");
    e.addComponent<TriggerComponent>();
    CHECK(ETT::inferFromEntity(e) == EntityType::Trigger);
}

TEST_CASE("inferFromEntity: Brush > otros (CSG entity sin importar otros componentes)") {
    Scene scene;
    Entity e = scene.createEntity("A");
    e.addComponent<BrushComponent>();
    e.addComponent<LightComponent>();  // raro pero posible
    CHECK(ETT::inferFromEntity(e) == EntityType::Brush);
}

TEST_CASE("inferFromEntity: Environment / Camera / ParticleEmitter / ForceField / Audio") {
    {
        Scene s; Entity e = s.createEntity("A");
        e.addComponent<EnvironmentComponent>();
        CHECK(ETT::inferFromEntity(e) == EntityType::Environment);
    }
    {
        Scene s; Entity e = s.createEntity("A");
        e.addComponent<CameraComponent>();
        CHECK(ETT::inferFromEntity(e) == EntityType::Camera);
    }
    {
        Scene s; Entity e = s.createEntity("A");
        e.addComponent<ParticleEmitterComponent>();
        CHECK(ETT::inferFromEntity(e) == EntityType::ParticleEmitter);
    }
    {
        Scene s; Entity e = s.createEntity("A");
        e.addComponent<ForceFieldComponent>();
        CHECK(ETT::inferFromEntity(e) == EntityType::ForceField);
    }
    {
        Scene s; Entity e = s.createEntity("A");
        e.addComponent<AudioSourceComponent>();
        CHECK(ETT::inferFromEntity(e) == EntityType::Audio);
    }
}

TEST_CASE("inferFromEntity: Mesh por MeshRendererComponent sin otros indicadores") {
    Scene scene;
    Entity e = scene.createEntity("A");
    e.addComponent<MeshRendererComponent>();
    CHECK(ETT::inferFromEntity(e) == EntityType::Mesh);
}

TEST_CASE("inferFromEntity: Vehicle gana sobre Mesh cuando ambos componentes coexisten") {
    Scene scene;
    Entity e = scene.createEntity("Car");
    e.addComponent<MeshRendererComponent>();  // chasis visual
    e.addComponent<VehicleComponent>();        // mecanica
    CHECK(ETT::inferFromEntity(e) == EntityType::Vehicle);
}

TEST_CASE("canAddComponent: Vehicle acepta script/audio/trigger pero NO light/environment/dialog") {
    using namespace Mood::EntityTypeTable;
    // Allow-list publica.
    CHECK(canAddComponent(EntityType::Vehicle, "script"));
    CHECK(canAddComponent(EntityType::Vehicle, "audio_source"));
    CHECK(canAddComponent(EntityType::Vehicle, "trigger"));
    CHECK(canAddComponent(EntityType::Vehicle, "animator"));
    // Bases no se ofrecen para "Add".
    CHECK_FALSE(canAddComponent(EntityType::Vehicle, "vehicle"));
    // Componentes sin sentido para un auto.
    CHECK_FALSE(canAddComponent(EntityType::Vehicle, "light"));
    CHECK_FALSE(canAddComponent(EntityType::Vehicle, "environment"));
    CHECK_FALSE(canAddComponent(EntityType::Vehicle, "dialog"));
    CHECK_FALSE(canAddComponent(EntityType::Vehicle, "inventory"));
    CHECK_FALSE(canAddComponent(EntityType::Vehicle, "brush"));
}

TEST_CASE("inferFromEntity: Generic si no hay ningun indicador") {
    Scene scene;
    Entity e = scene.createEntity("A");
    // Solo Tag + Transform.
    CHECK(ETT::inferFromEntity(e) == EntityType::Generic);
}

TEST_CASE("inferFromEntity: tag 'Floor' detectado como Tile aunque tenga mesh+rigidbody") {
    Scene scene;
    Entity e = scene.createEntity("Floor");
    e.addComponent<MeshRendererComponent>();
    e.addComponent<RigidBodyComponent>();
    CHECK(ETT::inferFromEntity(e) == EntityType::Tile);
}

TEST_CASE("inferFromEntity: tag 'Tile_3_5' detectado como Tile") {
    Scene scene;
    Entity e = scene.createEntity("Tile_3_5");
    e.addComponent<MeshRendererComponent>();
    CHECK(ETT::inferFromEntity(e) == EntityType::Tile);
}

TEST_CASE("inferFromEntityWithTag: tagHint Floor sin componentes auto-gen igual da Tile") {
    Scene scene;
    Entity e = scene.createEntity("__placeholder__");
    // e tiene tag "__placeholder__" pero pasamos tagHint "Floor" — caso
    // edge del SceneLoader que infiere antes de materializar el Tag.
    CHECK(ETT::inferFromEntityWithTag(e, "Floor") == EntityType::Tile);
}

TEST_CASE("TagComponent: default entityType es Generic") {
    Scene scene;
    Entity e = scene.createEntity("Test");
    CHECK(e.getComponent<TagComponent>().entityType == EntityType::Generic);
}

TEST_CASE("TagComponent: ctor con type explicito") {
    TagComponent tc("MyLight", EntityType::Light);
    CHECK(tc.name == "MyLight");
    CHECK(tc.entityType == EntityType::Light);
}
