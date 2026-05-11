// Tests de animacion esqueletica (Hito 19). Cubre:
//   - Skeleton: indexBy, computeSkinningMatrices con padre/hijos.
//   - BoneTrack: sample con clamp / lerp / quat normalization.
//   - AnimationClip::evaluate con fallback al bind pose.
//
// Header-only puros: no requieren GL ni assimp. Construimos esqueletos
// chiquitos a mano y verificamos invariantes.

#include <doctest/doctest.h>

#include "engine/animation/clips/AnimationClip.h"
#include "engine/animation/skeleton/Skeleton.h"

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

// ---------------------------------------------------------------------------
// F2H49: bind pass + evaluate con remap (clips standalone skeleton-agnosticos).
// ---------------------------------------------------------------------------

namespace {

// Helper: clip que pone al hueso `boneName` en (5, 6, 7) al final del clip.
// Track con position keys solamente; boneIndex queda en -1 (clip standalone).
AnimationClip makeClipWithTrackOn(std::string_view boneName) {
    AnimationClip clip;
    clip.name = "anim_test";
    clip.duration = 1.0f;

    BoneTrack tr;
    tr.boneName = std::string(boneName);
    tr.boneIndex = -1; // standalone: lo resuelve el bind pass
    tr.positionKeys.push_back({0.0f, glm::vec3(0.0f)});
    tr.positionKeys.push_back({1.0f, glm::vec3(5.0f, 6.0f, 7.0f)});
    clip.tracks.push_back(std::move(tr));
    return clip;
}

} // namespace

TEST_CASE("bindClipToSkeleton: clip vacio produce remap vacio") {
    Skeleton skel = makeChainSkeleton();
    AnimationClip empty;
    empty.duration = 1.0f;

    std::vector<int> remap;
    bindClipToSkeleton(empty, skel, remap);
    CHECK(remap.empty());
}

TEST_CASE("bindClipToSkeleton: tracks matchean al esqueleto destino") {
    Skeleton skel = makeChainSkeleton(); // huesos: "root", "child"
    AnimationClip clip = makeClipWithTrackOn("child");

    std::vector<int> remap;
    bindClipToSkeleton(clip, skel, remap);
    REQUIRE(remap.size() == 1);
    CHECK(remap[0] == 1); // "child" es indice 1 en makeChainSkeleton
}

TEST_CASE("bindClipToSkeleton: track huerfano queda en -1") {
    Skeleton skel = makeChainSkeleton();
    AnimationClip clip = makeClipWithTrackOn("nonexistent_bone");

    std::vector<int> remap;
    bindClipToSkeleton(clip, skel, remap);
    REQUIRE(remap.size() == 1);
    CHECK(remap[0] == -1);
}

TEST_CASE("evaluateClipWithRemap: remap stale (mismatch size) cae al bind pose") {
    Skeleton skel = makeChainSkeleton();
    skel.bones[0].localBindTransform =
        glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));

    AnimationClip clip = makeClipWithTrackOn("child");

    // Remap intencionalmente con tamano distinto a tracks.size().
    std::vector<int> badRemap; // size = 0, tracks.size() = 1

    std::vector<glm::mat4> localPose;
    evaluateClipWithRemap(clip, 1.0f, skel, badRemap, localPose);
    REQUIRE(localPose.size() == 2);
    // Root debe estar en su localBindTransform (2, 0, 0).
    CHECK(localPose[0][3][0] == doctest::Approx(2.0f));
}

TEST_CASE("evaluateClipWithRemap: orphan remap deja el hueso en bind") {
    Skeleton skel = makeChainSkeleton();
    skel.bones[1].localBindTransform =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 99.0f, 0.0f));

    AnimationClip clip = makeClipWithTrackOn("child");
    std::vector<int> remap = {-1}; // track huerfano

    std::vector<glm::mat4> localPose;
    evaluateClipWithRemap(clip, 1.0f, skel, remap, localPose);
    REQUIRE(localPose.size() == 2);
    // child no se evaluo (orphan) → bind: y=99.
    CHECK(localPose[1][3][1] == doctest::Approx(99.0f));
}

TEST_CASE("evaluateClipWithRemap: track resuelto evalua sobre el hueso correcto") {
    Skeleton skel = makeChainSkeleton();
    AnimationClip clip = makeClipWithTrackOn("child");
    std::vector<int> remap = {1}; // track -> child (indice 1)

    std::vector<glm::mat4> localPose;
    evaluateClipWithRemap(clip, 1.0f, skel, remap, localPose);
    REQUIRE(localPose.size() == 2);
    // En t=1.0 la posicion es (5, 6, 7) — el sample del ultimo key.
    CHECK(localPose[1][3][0] == doctest::Approx(5.0f));
    CHECK(localPose[1][3][1] == doctest::Approx(6.0f));
    CHECK(localPose[1][3][2] == doctest::Approx(7.0f));
}

TEST_CASE("evaluateClipWithRemap: mismo clip + 2 esqueletos = remaps distintos, ambos validos") {
    // El punto F2H49: un mismo clip standalone se reusa entre distintos
    // esqueletos compatibles. El bind genera remaps distintos sin mutar el clip.
    AnimationClip clip = makeClipWithTrackOn("child");

    // Esqueleto A: "child" es indice 1 (orden chain root, child).
    Skeleton skelA = makeChainSkeleton();
    std::vector<int> remapA;
    bindClipToSkeleton(clip, skelA, remapA);
    REQUIRE(remapA.size() == 1);
    CHECK(remapA[0] == 1);

    // Esqueleto B: "child" es indice 0 (un solo hueso llamado "child").
    Skeleton skelB;
    Bone single;
    single.name = "child";
    single.parent = -1;
    single.inverseBind = glm::mat4(1.0f);
    single.localBindTransform = glm::mat4(1.0f);
    skelB.bones.push_back(single);

    std::vector<int> remapB;
    bindClipToSkeleton(clip, skelB, remapB);
    REQUIRE(remapB.size() == 1);
    CHECK(remapB[0] == 0); // mismo nombre, indice distinto

    // Verificacion clave: el clip no se mutó.
    CHECK(clip.tracks[0].boneIndex == -1);

    // Y ambos remaps producen poses validas sobre sus esqueletos.
    std::vector<glm::mat4> poseA, poseB;
    evaluateClipWithRemap(clip, 1.0f, skelA, remapA, poseA);
    evaluateClipWithRemap(clip, 1.0f, skelB, remapB, poseB);
    CHECK(poseA[1][3][0] == doctest::Approx(5.0f)); // A: child es bone 1
    CHECK(poseB[0][3][0] == doctest::Approx(5.0f)); // B: child es bone 0
}
