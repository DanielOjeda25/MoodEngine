// F2H66 Bloque A: tests del layout puro (sin Jolt). Verifican que el
// auto-build a partir de un Skeleton Mixamo produce un layout coherente
// y que skeletons no-Mixamo retornan layout vacio + warn.

#include <doctest/doctest.h>

#include "engine/animation/skeleton/Skeleton.h"
#include "engine/physics/ragdoll/RagdollLayout.h"
#include "test_ragdoll_helpers.h"  // makeMixamoSkeleton

#include <glm/ext/matrix_transform.hpp>

#include <ostream>  // doctest necesita ostream para imprimir string_view en CHECK
#include <string>

using namespace Mood;
using namespace Mood::ragdoll;

using Mood::tests::makeMixamoSkeleton;

TEST_CASE("RagdollLayout F2H66 A: skeleton vacio -> layout vacio") {
    Skeleton sk{};
    auto layout = buildMixamoLayout(sk);
    CHECK(layout.empty());
    CHECK(layout.torsoIndex == -1);
}

TEST_CASE("RagdollLayout F2H66 A: skeleton no-Mixamo -> layout vacio + warn") {
    Skeleton sk;
    Bone b;
    b.name = "root";  // sin prefijo mixamorig:
    b.parent = -1;
    sk.bones.push_back(b);
    auto layout = buildMixamoLayout(sk);
    CHECK(layout.empty());
}

TEST_CASE("RagdollLayout F2H66 A: skeleton Mixamo standard -> 14 bones") {
    auto sk = makeMixamoSkeleton();
    auto layout = buildMixamoLayout(sk, 70.0f, 0.05f);
    REQUIRE(layout.bones.size() == 14);
    CHECK(layout.torsoIndex == 3);

    // Root es el Hips (sin constraint).
    CHECK(layout.bones[0].boneName == "Hips");
    CHECK(layout.bones[0].parentRagdollIndex == -1);
    CHECK(layout.bones[0].constraint == ConstraintKind::None);

    // Codos y rodillas son Hinge.
    CHECK(layout.bones[7].constraint == ConstraintKind::Hinge);   // LeftForeArm
    CHECK(layout.bones[9].constraint == ConstraintKind::Hinge);   // RightForeArm
    CHECK(layout.bones[11].constraint == ConstraintKind::Hinge);  // LeftLeg
    CHECK(layout.bones[13].constraint == ConstraintKind::Hinge);  // RightLeg

    // Hombros/caderas son SwingTwist.
    CHECK(layout.bones[6].constraint == ConstraintKind::SwingTwist);   // LeftArm
    CHECK(layout.bones[10].constraint == ConstraintKind::SwingTwist);  // LeftUpLeg

    // Limites del codo: solo flexion positiva (0 a 145°).
    const auto& elbowL = layout.bones[7].limits;
    CHECK(elbowL.hingeMinDeg == doctest::Approx(0.0f));
    CHECK(elbowL.hingeMaxDeg == doctest::Approx(145.0f));
}

TEST_CASE("RagdollLayout F2H66 A: mass distribution suma a totalMass (+-1%)") {
    auto sk = makeMixamoSkeleton();
    const f32 kTotal = 70.0f;
    auto layout = buildMixamoLayout(sk, kTotal, 0.05f);
    REQUIRE(!layout.empty());

    f32 sum = 0.0f;
    for (const auto& b : layout.bones) sum += b.mass;
    CHECK(sum == doctest::Approx(kTotal).epsilon(0.01));

    // Ningun body con masa < 0.1 kg (floor del layout).
    for (const auto& b : layout.bones) {
        CHECK(b.mass >= 0.1f);
    }
}

TEST_CASE("RagdollLayout F2H66 A: capsule sizes proporcionales al skeleton") {
    auto sk = makeMixamoSkeleton();
    auto layout = buildMixamoLayout(sk, 70.0f, 0.05f);
    REQUIRE(!layout.empty());

    // El radio de los limbs es exactamente limbRadius; el torso es ~1.8x.
    CHECK(layout.bones[7].capsuleRadius == doctest::Approx(0.05f));   // LeftForeArm
    CHECK(layout.bones[0].capsuleRadius == doctest::Approx(0.09f));   // Hips (torso)
    CHECK(layout.bones[3].capsuleRadius == doctest::Approx(0.09f));   // Spine2 (torso)

    // Todos los halfHeight son positivos (no degenerados).
    for (const auto& b : layout.bones) {
        CHECK(b.capsuleHalfHeight > 0.0f);
        CHECK(b.capsuleHalfHeight < 1.0f);  // sano para un cuerpo humano ~2m
    }
}

TEST_CASE("RagdollLayout F2H66 A: skeleton con bones faltantes (-5) -> layout vacio") {
    auto sk = makeMixamoSkeleton();
    // Removemos 5 bones (8 quedan, debajo del minimo 10).
    sk.bones.resize(8);
    auto layout = buildMixamoLayout(sk);
    CHECK(layout.empty());
}
