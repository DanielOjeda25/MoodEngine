// F2H66 Bloque H: tests headless del PhysicsWorld Ragdoll API. Verifican
// que el wrapper sobre JPH::Ragdoll respete lifecycle + lea poses + aplique
// impulses sin crashear, y que un ragdoll bajo gravedad caiga "razonable"
// (no atravieza al infinito ni se desarma).

#include <doctest/doctest.h>

#include "engine/animation/skeleton/Skeleton.h"
#include "engine/physics/ragdoll/RagdollLayout.h"
#include "engine/physics/world/PhysicsWorld.h"
#include "test_ragdoll_helpers.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/mat4x4.hpp>

#include <vector>

using namespace Mood;
using Mood::tests::makeMixamoSkeleton;

namespace {

constexpr f32 k_dt = 0.016f;

// Genera world transforms iniciales para cada parte del ragdoll. Por
// simplicidad usamos identidad rotada + posicion derivada del layout.
// No tiene que matchear la pose real del Animator — solo posiciones
// validas + non-overlapping para que Jolt no rechace el setup.
std::vector<glm::mat4> makePartWorldTransformsAtOrigin(
    const ragdoll::RagdollLayout& layout) {
    std::vector<glm::mat4> out(layout.bones.size(), glm::mat4(1.0f));
    // Apilamos los bodies verticalmente con un gap entre ellos para que
    // Jolt no detecte overlaps internos antes de aplicar el group filter.
    f32 yCursor = 5.0f;
    for (usize i = 0; i < layout.bones.size(); ++i) {
        const f32 halfH = layout.bones[i].capsuleHalfHeight;
        const f32 r     = layout.bones[i].capsuleRadius;
        out[i] = glm::translate(glm::mat4(1.0f),
                                  glm::vec3(0.0f, yCursor, 0.0f));
        yCursor -= 2.0f * (halfH + r) + 0.05f;  // gap chico anti-overlap
    }
    return out;
}

} // anonymous

TEST_CASE("PhysicsWorld F2H66: createRagdoll devuelve handle valido + ragdollCount=1") {
    PhysicsWorld pw;
    const auto sk = makeMixamoSkeleton();
    const auto layout = ragdoll::buildMixamoLayout(sk, 70.0f, 0.05f);
    REQUIRE(!layout.empty());
    REQUIRE(pw.ragdollCount() == 0u);

    const auto transforms = makePartWorldTransformsAtOrigin(layout);
    const u32 id = pw.createRagdoll(layout, transforms, /*useGravity*/ false);
    CHECK(id != 0u);
    CHECK(pw.ragdollCount() == 1u);

    pw.destroyRagdoll(id);
    CHECK(pw.ragdollCount() == 0u);
}

TEST_CASE("PhysicsWorld F2H66: createRagdoll con layout vacio retorna 0") {
    PhysicsWorld pw;
    ragdoll::RagdollLayout empty{};
    std::vector<glm::mat4> noTransforms;
    const u32 id = pw.createRagdoll(empty, noTransforms, false);
    CHECK(id == 0u);
    CHECK(pw.ragdollCount() == 0u);
}

TEST_CASE("PhysicsWorld F2H66: createRagdoll con transforms tamanio mismatch retorna 0") {
    PhysicsWorld pw;
    const auto sk = makeMixamoSkeleton();
    const auto layout = ragdoll::buildMixamoLayout(sk, 70.0f, 0.05f);
    REQUIRE(!layout.empty());
    // Pasar 3 transforms cuando el layout pide 14 → error.
    std::vector<glm::mat4> wrongSize(3, glm::mat4(1.0f));
    const u32 id = pw.createRagdoll(layout, wrongSize, false);
    CHECK(id == 0u);
}

TEST_CASE("PhysicsWorld F2H66: readRagdollPose retorna 14 transforms para layout Mixamo") {
    PhysicsWorld pw;
    const auto sk = makeMixamoSkeleton();
    const auto layout = ragdoll::buildMixamoLayout(sk, 70.0f, 0.05f);
    const auto transforms = makePartWorldTransformsAtOrigin(layout);
    const u32 id = pw.createRagdoll(layout, transforms, false);
    REQUIRE(id != 0u);

    std::vector<glm::mat4> outPose;
    const bool ok = pw.readRagdollPose(id, outPose);
    CHECK(ok);
    CHECK(outPose.size() == 14u);

    // Ragdoll id invalido -> false.
    std::vector<glm::mat4> dummy;
    CHECK_FALSE(pw.readRagdollPose(99999u, dummy));
}

TEST_CASE("PhysicsWorld F2H66: destroyRagdoll idempotente para id invalido") {
    PhysicsWorld pw;
    pw.destroyRagdoll(0u);
    pw.destroyRagdoll(99999u);
    CHECK(pw.ragdollCount() == 0u);
}

TEST_CASE("PhysicsWorld F2H66: ragdoll bajo gravedad cae pero no atravieza el infinito") {
    PhysicsWorld pw;
    const auto sk = makeMixamoSkeleton();
    const auto layout = ragdoll::buildMixamoLayout(sk, 70.0f, 0.05f);
    auto transforms = makePartWorldTransformsAtOrigin(layout);
    // Subimos todo para que tenga margen de caida.
    for (auto& m : transforms) m[3].y += 10.0f;

    const u32 id = pw.createRagdoll(layout, transforms, /*useGravity*/ true);
    REQUIRE(id != 0u);

    std::vector<glm::mat4> initial;
    pw.readRagdollPose(id, initial);

    // 60 ticks ≈ 1 segundo. Caida libre sin floor seria ~5m (1/2 g t²).
    for (int i = 0; i < 60; ++i) pw.step(k_dt);

    std::vector<glm::mat4> after;
    REQUIRE(pw.readRagdollPose(id, after));
    REQUIRE(after.size() == initial.size());

    // El torso (index 3 = Spine2) debe haber caido pero no mas de 20m.
    const f32 dy = initial[3][3].y - after[3][3].y;
    CHECK(dy > 1.0f);   // al menos cayo algo
    CHECK(dy < 20.0f);  // no se disparo al vacio
}

TEST_CASE("PhysicsWorld F2H66: applyRagdollImpulse al torso lo mueve en la direccion del impulse") {
    PhysicsWorld pw;
    const auto sk = makeMixamoSkeleton();
    const auto layout = ragdoll::buildMixamoLayout(sk, 70.0f, 0.05f);
    const auto transforms = makePartWorldTransformsAtOrigin(layout);

    const u32 id = pw.createRagdoll(layout, transforms, /*useGravity*/ false);
    REQUIRE(id != 0u);

    std::vector<glm::mat4> before;
    pw.readRagdollPose(id, before);

    // Impulse +X grande al torso (Spine2 = index 3).
    pw.applyRagdollImpulse(id, layout.torsoIndex, glm::vec3(500.0f, 0.0f, 0.0f));

    for (int i = 0; i < 30; ++i) pw.step(k_dt);

    std::vector<glm::mat4> after;
    pw.readRagdollPose(id, after);

    // Torso se movio en +X (no en otra direccion).
    const f32 dx = after[3][3].x - before[3][3].x;
    CHECK(dx > 0.1f);

    // applyRagdollImpulse a partIndex invalido es no-op (no crashea).
    pw.applyRagdollImpulse(id, 999, glm::vec3(0.0f));
    pw.applyRagdollImpulse(99999u, 0, glm::vec3(0.0f));
}
