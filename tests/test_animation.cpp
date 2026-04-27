// Tests de animacion esqueletica (Hito 19). Cubre:
//   - Skeleton: indexBy, computeSkinningMatrices con padre/hijos.
//   - BoneTrack: sample con clamp / lerp / quat normalization.
//   - AnimationClip::evaluate con fallback al bind pose.
//
// Header-only puros: no requieren GL ni assimp. Construimos esqueletos
// chiquitos a mano y verificamos invariantes.

#include <doctest/doctest.h>

#include "engine/animation/AnimationClip.h"
#include "engine/animation/Skeleton.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace Mood;

namespace {

// Esqueleto trivial: 2 huesos en cadena (root + child a 1m sobre Y).
Skeleton makeChainSkeleton() {
    Skeleton skel;
    Bone root;
    root.name = "root";
    root.parent = -1;
    root.inverseBind = glm::mat4(1.0f); // bind: root en el origen
    root.localBindTransform = glm::mat4(1.0f);

    Bone child;
    child.name = "child";
    child.parent = 0;
    // Bind del child: 1m arriba del root.
    child.inverseBind = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    child.localBindTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    skel.bones.push_back(root);
    skel.bones.push_back(child);
    return skel;
}

} // namespace

// ---------------------------------------------------------------------------
// Skeleton
// ---------------------------------------------------------------------------

TEST_CASE("Skeleton::boneIndex resuelve nombres y devuelve -1 al fallar") {
    Skeleton skel = makeChainSkeleton();
    CHECK(skel.boneIndex("root")  == 0);
    CHECK(skel.boneIndex("child") == 1);
    CHECK(skel.boneIndex("hip")   == -1);
}

TEST_CASE("computeSkinningMatrices: bind pose -> matrices identidad") {
    // Si la pose local == localBindTransform por hueso, las skinning
    // matrices deben ser identity (mesh queda en bind pose, sin
    // deformacion).
    Skeleton skel = makeChainSkeleton();

    std::vector<glm::mat4> localPose;
    localPose.push_back(skel.bones[0].localBindTransform);
    localPose.push_back(skel.bones[1].localBindTransform);

    std::vector<glm::mat4> skinning;
    computeSkinningMatrices(skel, localPose, skinning);

    REQUIRE(skinning.size() == 2);
    // La identity en glm tiene componentes diagonales = 1, resto = 0.
    for (int b = 0; b < 2; ++b) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                const float expected = (i == j) ? 1.0f : 0.0f;
                CHECK(skinning[b][i][j] == doctest::Approx(expected).epsilon(1e-4f));
            }
        }
    }
}

TEST_CASE("computeSkinningMatrices: localPose vacio cae al bind") {
    // Misma idea: si no se pasa pose, se usan los localBindTransform
    // y el resultado es identity.
    Skeleton skel = makeChainSkeleton();
    std::vector<glm::mat4> skinning;
    computeSkinningMatrices(skel, {}, skinning);
    REQUIRE(skinning.size() == 2);
    CHECK(skinning[1][0][0] == doctest::Approx(1.0f));
    CHECK(skinning[1][3][3] == doctest::Approx(1.0f));
}

TEST_CASE("computeSkinningMatrices: rotar root afecta child por herencia") {
    // Si el root rota 90 grados en Y, la posicion del child (que esta
    // 1m arriba en bind) deberia pasar por la rotacion del padre. Como
    // el child esta en el eje Y, la rotacion en Y NO le cambia la
    // posicion (hereda yaw, no se desplaza). Pero la matriz skinning
    // del child SI tiene la rotacion porque se compone con el padre.
    Skeleton skel = makeChainSkeleton();

    std::vector<glm::mat4> localPose(2);
    localPose[0] = glm::rotate(glm::mat4(1.0f),
                                 glm::radians(90.0f), glm::vec3(0, 1, 0));
    localPose[1] = skel.bones[1].localBindTransform; // child en bind

    std::vector<glm::mat4> skinning;
    computeSkinningMatrices(skel, localPose, skinning);

    REQUIRE(skinning.size() == 2);
    // Aplicar la matriz skinning a un vertice del bind pose en mesh-space
    // que esta en la cabeza del child (0, 1, 0). El root la rotara 90
    // grados; el resultado deberia ser (~0, 1, 0) — eje Y invariante a
    // rotacion en Y.
    const glm::vec4 vertexBind(0.0f, 1.0f, 0.0f, 1.0f);
    const glm::vec4 deformed = skinning[1] * vertexBind;
    CHECK(deformed.x == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(deformed.y == doctest::Approx(1.0f).epsilon(1e-4f));
    CHECK(deformed.z == doctest::Approx(0.0f).epsilon(1e-4f));
}

// ---------------------------------------------------------------------------
// BoneTrack sampling
// ---------------------------------------------------------------------------

TEST_CASE("samplePosition clamp en t fuera de rango") {
    BoneTrack tr;
    tr.boneIndex = 0;
    tr.positionKeys.push_back({0.0f, glm::vec3(0.0f)});
    tr.positionKeys.push_back({1.0f, glm::vec3(10.0f, 0.0f, 0.0f)});

    // t < primer key -> primer key
    auto p0 = samplePosition(tr, -1.0f);
    CHECK(p0.x == doctest::Approx(0.0f));

    // t > ultimo key -> ultimo key
    auto p1 = samplePosition(tr, 5.0f);
    CHECK(p1.x == doctest::Approx(10.0f));
}

TEST_CASE("samplePosition lerp entre dos keys en el medio") {
    BoneTrack tr;
    tr.boneIndex = 0;
    tr.positionKeys.push_back({0.0f, glm::vec3(0.0f)});
    tr.positionKeys.push_back({2.0f, glm::vec3(10.0f, 0.0f, 0.0f)});

    // En t=1.0 (medio) deberia ser exactamente la mitad.
    auto p = samplePosition(tr, 1.0f);
    CHECK(p.x == doctest::Approx(5.0f));
}

TEST_CASE("sampleRotation devuelve identidad si no hay keys") {
    BoneTrack tr;
    auto q = sampleRotation(tr, 0.5f);
    CHECK(q.w == doctest::Approx(1.0f));
    CHECK(q.x == doctest::Approx(0.0f));
    CHECK(q.y == doctest::Approx(0.0f));
    CHECK(q.z == doctest::Approx(0.0f));
}

TEST_CASE("sampleRotation produce quaternion normalizado") {
    BoneTrack tr;
    tr.boneIndex = 0;
    // 0 -> identity, 1 -> rotacion 90 grados en Y
    tr.rotationKeys.push_back({0.0f, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)});
    const glm::quat ry90 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0, 1, 0));
    tr.rotationKeys.push_back({1.0f, ry90});

    auto q = sampleRotation(tr, 0.5f);
    const float len = std::sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    CHECK(len == doctest::Approx(1.0f).epsilon(1e-4f));
}

TEST_CASE("sampleScale devuelve uno si no hay keys") {
    BoneTrack tr;
    auto s = sampleScale(tr, 0.5f);
    CHECK(s.x == doctest::Approx(1.0f));
    CHECK(s.y == doctest::Approx(1.0f));
    CHECK(s.z == doctest::Approx(1.0f));
}

// ---------------------------------------------------------------------------
// AnimationClip::evaluate con fallback al bind
// ---------------------------------------------------------------------------

TEST_CASE("AnimationClip::evaluate usa bind para huesos sin track") {
    // Skeleton de 2 huesos. El clip solo anima el hueso 1; el hueso 0
    // (root) NO tiene track, asi que su pose deberia caer al
    // localBindTransform.
    Skeleton skel = makeChainSkeleton();
    // Modificamos el localBindTransform del root para que sea distinto
    // de identity — asi distinguimos "se uso el bind" vs "quedo en
    // identity".
    skel.bones[0].localBindTransform =
        glm::translate(glm::mat4(1.0f), glm::vec3(7.0f, 0.0f, 0.0f));

    AnimationClip clip;
    clip.duration = 1.0f;

    BoneTrack t1;
    t1.boneIndex = 1;
    t1.positionKeys.push_back({0.0f, glm::vec3(0.0f, 1.0f, 0.0f)});
    t1.positionKeys.push_back({1.0f, glm::vec3(0.0f, 5.0f, 0.0f)});
    clip.tracks.push_back(t1);

    std::vector<glm::mat4> localPose;
    clip.evaluate(0.5f, skel, localPose);

    REQUIRE(localPose.size() == 2);
    // Root: su posicion deberia ser (7, 0, 0) — del bind, no identity.
    CHECK(localPose[0][3][0] == doctest::Approx(7.0f));
    // Child: con t=0.5 entre (0,1,0) y (0,5,0) deberia estar en (0,3,0).
    CHECK(localPose[1][3][1] == doctest::Approx(3.0f));
}

TEST_CASE("AnimationClip::evaluate con clip vacio rellena al bind completo") {
    Skeleton skel = makeChainSkeleton();
    skel.bones[0].localBindTransform =
        glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 3.0f, 4.0f));

    AnimationClip empty;
    empty.duration = 1.0f;

    std::vector<glm::mat4> localPose;
    empty.evaluate(0.0f, skel, localPose);
    REQUIRE(localPose.size() == 2);
    CHECK(localPose[0][3][0] == doctest::Approx(2.0f));
    CHECK(localPose[0][3][1] == doctest::Approx(3.0f));
    CHECK(localPose[0][3][2] == doctest::Approx(4.0f));
}

TEST_CASE("BoneTrack: track sin keys es 'empty'") {
    BoneTrack tr;
    CHECK(tr.empty());
    tr.positionKeys.push_back({0.0f, glm::vec3(0.0f)});
    CHECK_FALSE(tr.empty());
}
