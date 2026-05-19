#pragma once

// F2H66: layout de ragdoll computado a partir de un Skeleton con
// convencion Mixamo. Header puro (sin Jolt) — usable headless en tests.
//
// Concepto: un ragdoll es una version reducida del esqueleto donde algunos
// huesos ganan un body fisico (capsule) y otros se "montan" sobre el
// parent ragdolleado. Los bodies estan unidos por constraints:
//   - Hinge (1 DOF) para codos y rodillas
//   - SwingTwist (3 DOF cono + twist) para hombros, caderas, spine, cuello
//
// El layout es DECLARATIVO (sin estado runtime). El PhysicsWorld lo
// consume al activar un ragdoll para crear los bodies+constraints reales
// en Jolt.

#include "core/Types.h"

#include <glm/vec3.hpp>

#include <array>
#include <string_view>
#include <vector>

namespace Mood {

struct Skeleton;

namespace ragdoll {

/// @brief Tipo de constraint que une un bone con su parent ragdolleado.
enum class ConstraintKind : u8 {
    None        = 0,  // root del ragdoll (Hips): no tiene parent
    Hinge       = 1,  // 1 eje (codos, rodillas)
    SwingTwist  = 2,  // 3 DOF cono + twist (hombros, caderas, spine, cuello)
};

/// @brief Limites angulares de un constraint. Para Hinge usa min/max;
///        para SwingTwist usa swingY/swingZ (cono) + twist (rotacion sobre
///        el eje del bone).
struct ConstraintLimits {
    // Hinge:
    f32 hingeMinDeg = 0.0f;
    f32 hingeMaxDeg = 0.0f;
    // SwingTwist:
    f32 swingYDeg = 0.0f;
    f32 swingZDeg = 0.0f;
    f32 twistMinDeg = 0.0f;
    f32 twistMaxDeg = 0.0f;
};

/// @brief Descripcion de un bone ragdolleado: que body crear + a quien lo
///        une el constraint hacia el parent.
struct RagdollBone {
    /// Indice del bone en el `Skeleton` original.
    int boneIndex = -1;
    /// Nombre del bone (cacheado del Skeleton para debug/tests).
    std::string_view boneName;
    /// Indice de este bone DENTRO del array `RagdollLayout::bones` del
    /// parent ragdolleado. -1 = es root del ragdoll (Hips), sin parent.
    /// OJO: NO es el `Skeleton::Bone::parent` directo — el ragdoll skipea
    /// huesos chicos (Hand, Foot, Shoulder, fingers) cuyo body no se crea.
    int parentRagdollIndex = -1;

    /// Capsule shape: largo del bone (al primer descendant ragdolleado),
    /// radio = `limbRadius` global (con override si emerge).
    f32 capsuleHalfHeight = 0.05f;  // metros
    f32 capsuleRadius     = 0.05f;  // metros

    /// Masa asignada al body. Calculada por volumen del capsule, escalada
    /// para que la suma del layout matchee `totalMass`.
    f32 mass = 1.0f;

    /// Constraint que une este body con el parent ragdolleado.
    ConstraintKind  constraint = ConstraintKind::None;
    ConstraintLimits limits{};
};

/// @brief Layout completo. `bones[0]` es el root (Hips). El indice donde
///        cae el torso (Spine2 o Spine1) se guarda en `torsoIndex` para
///        que el caller pueda aplicar el spawn impulse al body correcto.
struct RagdollLayout {
    std::vector<RagdollBone> bones;
    int torsoIndex = -1;  // -1 si no se identifico

    bool empty() const { return bones.empty(); }
};

/// @brief Lista de bones Mixamo standard que el layout incluye como
///        bodies independientes (14 bones). Constante para tests + debug.
///        Orden: root (Hips) primero, luego torso ascendente, luego
///        extremidades. Wrist/Foot/Shoulder/fingers NO se incluyen
///        (parte del parent ragdolleado, sin body propio).
extern const std::array<const char*, 14> k_mixamoRagdollBones;

/// @brief Prefijo Mixamo. Si NINGUN bone del skeleton tiene este prefijo
///        en su nombre, el layout retornado queda vacio + log warn.
constexpr std::string_view k_mixamoPrefix = "mixamorig:";

/// @brief Construye un layout completo a partir de un Skeleton Mixamo.
///        Bones esperados (con prefijo `mixamorig:`):
///          Hips (root)
///          Spine, Spine1, Spine2
///          Neck, Head
///          LeftArm, LeftForeArm
///          RightArm, RightForeArm
///          LeftUpLeg, LeftLeg
///          RightUpLeg, RightLeg
///        Resultado tipico: 13 bodies + 12 constraints (Hips root + 12
///        joints) si el skeleton trae todos.
///
///        Si el skeleton NO tiene ningun bone con prefijo `mixamorig:`,
///        retorna layout vacio (defensivo — esqueletos custom requieren
///        otra estrategia, fuera de scope F2H66).
///
/// @param skel Skeleton a analizar.
/// @param totalMass Masa total a distribuir entre los bodies (kg).
/// @param limbRadius Radio de los capsules de extremidades (m). El torso
///        usa ~2x este valor (mas grueso).
RagdollLayout buildMixamoLayout(const Skeleton& skel,
                                f32 totalMass = 70.0f,
                                f32 limbRadius = 0.05f);

} // namespace ragdoll
} // namespace Mood
