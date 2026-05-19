#include "engine/physics/ragdoll/RagdollLayout.h"

#include "core/Log.h"
#include "engine/animation/skeleton/Skeleton.h"

#include <glm/mat4x4.hpp>

#include <cmath>
#include <string>
#include <unordered_map>

namespace Mood::ragdoll {

const std::array<const char*, 14> k_mixamoRagdollBones = {
    "Hips",         // 0 root
    "Spine",        // 1
    "Spine1",       // 2
    "Spine2",       // 3 torso (anchor de brazos)
    "Neck",         // 4
    "Head",         // 5
    "LeftArm",      // 6 upper arm L
    "LeftForeArm",  // 7
    "RightArm",     // 8
    "RightForeArm", // 9
    "LeftUpLeg",    // 10
    "LeftLeg",      // 11
    "RightUpLeg",   // 12
    "RightLeg",     // 13
};

namespace {

// Mapping de cada bone Mixamo a su parent DENTRO del ragdoll (no en el
// skeleton). -1 = root. Indices son posiciones en k_mixamoRagdollBones.
constexpr std::array<int, 14> k_ragdollParents = {
    -1,  // 0 Hips
     0,  // 1 Spine
     1,  // 2 Spine1
     2,  // 3 Spine2
     3,  // 4 Neck
     4,  // 5 Head
     3,  // 6 LeftArm  -> Spine2
     6,  // 7 LeftForeArm -> LeftArm
     3,  // 8 RightArm -> Spine2
     8,  // 9 RightForeArm -> RightArm
     0,  // 10 LeftUpLeg -> Hips
    10,  // 11 LeftLeg -> LeftUpLeg
     0,  // 12 RightUpLeg -> Hips
    12,  // 13 RightLeg -> RightUpLeg
};

// Constraint kind por par parent->child. Codos y rodillas son Hinge;
// el resto es SwingTwist (cono + giro).
ConstraintKind constraintKindForBone(int ragdollIndex) {
    // Hinges: ForeArm (codos) y Leg (rodillas).
    if (ragdollIndex == 7  /*LeftForeArm*/  ||
        ragdollIndex == 9  /*RightForeArm*/ ||
        ragdollIndex == 11 /*LeftLeg*/      ||
        ragdollIndex == 13 /*RightLeg*/) {
        return ConstraintKind::Hinge;
    }
    // Hips es root, sin constraint.
    if (ragdollIndex == 0) return ConstraintKind::None;
    // Resto = SwingTwist.
    return ConstraintKind::SwingTwist;
}

// Limites angulares "naturales" del cuerpo humano (en grados). Conservadores
// — un atleta tiene mas rango pero queremos ragdoll legible, no contorsionista.
ConstraintLimits limitsForBone(int ragdollIndex) {
    ConstraintLimits L{};
    switch (ragdollIndex) {
        case 1: case 2: case 3:  // Spine, Spine1, Spine2
            L.swingYDeg = 25.0f; L.swingZDeg = 25.0f;
            L.twistMinDeg = -20.0f; L.twistMaxDeg = 20.0f;
            break;
        case 4:  // Neck
            L.swingYDeg = 35.0f; L.swingZDeg = 35.0f;
            L.twistMinDeg = -45.0f; L.twistMaxDeg = 45.0f;
            break;
        case 5:  // Head
            L.swingYDeg = 20.0f; L.swingZDeg = 20.0f;
            L.twistMinDeg = -20.0f; L.twistMaxDeg = 20.0f;
            break;
        case 6: case 8:  // Arm (hombros — el rango mas amplio del cuerpo)
            L.swingYDeg = 90.0f; L.swingZDeg = 90.0f;
            L.twistMinDeg = -60.0f; L.twistMaxDeg = 60.0f;
            break;
        case 7: case 9:  // ForeArm (codos) — Hinge
            L.hingeMinDeg = 0.0f; L.hingeMaxDeg = 145.0f;  // no flexion negativa
            break;
        case 10: case 12:  // UpLeg (caderas)
            L.swingYDeg = 70.0f; L.swingZDeg = 45.0f;
            L.twistMinDeg = -30.0f; L.twistMaxDeg = 30.0f;
            break;
        case 11: case 13:  // Leg (rodillas) — Hinge
            L.hingeMinDeg = 0.0f; L.hingeMaxDeg = 140.0f;
            break;
        default: break;  // root, no limits
    }
    return L;
}

// Distancia entre dos bones en bind-pose mesh-space. Util para dimensionar
// el largo de los capsules. Usa global bind transforms acumulados.
f32 distanceBetweenBones(const Skeleton& skel,
                         const std::vector<glm::mat4>& bindGlobals,
                         int boneA, int boneB) {
    if (boneA < 0 || boneB < 0) return 0.0f;
    const glm::vec3 pa(bindGlobals[boneA][3]);
    const glm::vec3 pb(bindGlobals[boneB][3]);
    return glm::length(pb - pa);
}

// Compone bind globals para todo el skeleton (acumulando localBindTransform
// segun parent). Necesario para medir distancias entre huesos.
std::vector<glm::mat4> computeBindGlobals(const Skeleton& skel) {
    std::vector<glm::mat4> out(skel.bones.size(), glm::mat4(1.0f));
    for (usize i = 0; i < skel.bones.size(); ++i) {
        const auto& b = skel.bones[i];
        if (b.parent < 0) {
            out[i] = b.localBindTransform;
        } else {
            out[i] = out[static_cast<usize>(b.parent)] * b.localBindTransform;
        }
    }
    return out;
}

// Volumen aproximado de un capsule (cilindro + 2 hemisferios).
f32 capsuleVolume(f32 halfHeight, f32 radius) {
    const f32 kPi = 3.14159265f;
    return kPi * radius * radius * 2.0f * halfHeight
         + (4.0f / 3.0f) * kPi * radius * radius * radius;
}

} // anonymous

RagdollLayout buildMixamoLayout(const Skeleton& skel,
                                f32 totalMass,
                                f32 limbRadius) {
    RagdollLayout layout;
    if (skel.empty()) return layout;

    // Sanity check: el skeleton debe ser Mixamo (al menos UN bone con el
    // prefijo). Sino, layout vacio + warn — el dev usa otro path.
    bool isMixamo = false;
    for (const auto& b : skel.bones) {
        if (b.name.size() >= k_mixamoPrefix.size() &&
            b.name.compare(0, k_mixamoPrefix.size(), k_mixamoPrefix) == 0) {
            isMixamo = true;
            break;
        }
    }
    if (!isMixamo) {
        Log::physics()->warn(
            "RagdollLayout: skeleton sin convencion Mixamo (no se encontro "
            "prefijo '{}'). Ragdoll desactivado para esta entidad.",
            std::string(k_mixamoPrefix));
        return layout;
    }

    // Resolvemos cada bone Mixamo standard al indice del skeleton. Si
    // alguno falta, lo registramos como -1 y skipeamos el ragdoll completo
    // (un ragdoll con bones faltantes seria un fenomeno raro).
    std::array<int, 14> boneIndices{};
    int presentCount = 0;
    for (usize i = 0; i < k_mixamoRagdollBones.size(); ++i) {
        const std::string fullName = std::string(k_mixamoPrefix)
                                       + k_mixamoRagdollBones[i];
        const int idx = skel.boneIndex(fullName);
        boneIndices[i] = idx;
        if (idx >= 0) ++presentCount;
    }
    if (presentCount < 10) {
        Log::physics()->warn(
            "RagdollLayout: solo {} de {} bones Mixamo standard presentes. "
            "Ragdoll desactivado (necesitamos al menos 10).",
            presentCount, k_mixamoRagdollBones.size());
        return layout;
    }

    const auto bindGlobals = computeBindGlobals(skel);

    // Construir los 14 RagdollBone. Para los que tienen skeleton index
    // valido, dimensionamos el capsule usando distancia al hijo principal.
    layout.bones.reserve(k_mixamoRagdollBones.size());
    for (usize i = 0; i < k_mixamoRagdollBones.size(); ++i) {
        RagdollBone rb;
        rb.boneIndex          = boneIndices[i];
        rb.boneName           = k_mixamoRagdollBones[i];
        rb.parentRagdollIndex = k_ragdollParents[i];
        rb.constraint         = constraintKindForBone(static_cast<int>(i));
        rb.limits             = limitsForBone(static_cast<int>(i));

        // Capsule size: largo = distancia al child principal (si existe),
        // radio = limbRadius. El torso usa radio mas grueso.
        f32 length = 0.15f;  // default conservador (15 cm)
        if (rb.boneIndex >= 0) {
            // Child principal del bone en el ragdoll. Buscamos el primer
            // ragdoll bone que nos tenga como parent.
            int childIdx = -1;
            for (usize j = i + 1; j < k_mixamoRagdollBones.size(); ++j) {
                if (k_ragdollParents[j] == static_cast<int>(i)) {
                    childIdx = boneIndices[j];
                    break;
                }
            }
            if (childIdx >= 0) {
                length = distanceBetweenBones(skel, bindGlobals,
                                                rb.boneIndex, childIdx);
                length = std::max(length, 0.05f);
            }
        }
        // Hips/Spine/Head: torso/cabeza un poco mas gruesos.
        const bool isTorso = (i == 0 || i == 1 || i == 2 || i == 3 || i == 5);
        rb.capsuleRadius = isTorso ? (limbRadius * 1.8f) : limbRadius;
        rb.capsuleHalfHeight = std::max(length * 0.5f - rb.capsuleRadius, 0.02f);

        layout.bones.push_back(rb);

        if (i == 3) layout.torsoIndex = static_cast<int>(i);  // Spine2 = torso
    }

    // Mass distribution por volumen. Calculamos volumenes y luego escalamos
    // para que la suma = totalMass.
    f32 totalVol = 0.0f;
    for (const auto& b : layout.bones) {
        totalVol += capsuleVolume(b.capsuleHalfHeight, b.capsuleRadius);
    }
    if (totalVol < 1e-6f) totalVol = 1.0f;  // defensivo
    const f32 massPerVol = totalMass / totalVol;
    for (auto& b : layout.bones) {
        b.mass = capsuleVolume(b.capsuleHalfHeight, b.capsuleRadius) * massPerVol;
        b.mass = std::max(b.mass, 0.1f);  // floor para evitar bodies "fantasma"
    }

    return layout;
}

} // namespace Mood::ragdoll
