#pragma once

// F2H81 (auditoría): split de Components.h por categoría. Este header agrupa
// los componentes de identidad / transform / render. Se incluye via el
// agregador `Components.h` (no incluir directo desde call-sites — usar
// Components.h para mantener el set completo disponible).

#include "core/Types.h"
#include "engine/assets/manager/AssetManager.h" // TextureAssetId, AudioAssetId, MeshAssetId

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp> // F2H70: glm::quat para sync sin gimbal
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

#include <string>
#include <utility>
#include <vector>

namespace Mood {

/// @brief Nombre legible de la entidad (para Hierarchy, logs, debug).
struct TagComponent {
    std::string name;

    TagComponent() = default;
    TagComponent(std::string n) : name(std::move(n)) {}
};

/// @brief ¿Es una wheel-entity interna del VehicleSystem? (tags canonicos
///        wheel_FL/FR/RL/RR). El VehicleSystem las spawnea y rematerializa
///        desde el VehicleComponent del chassis en cada load, asi que son
///        internas del engine: no se serializan (SceneSerializer), no se
///        listan en la jerarquia ni son pickeables en el viewport. Helper
///        compartido para no repetir el check de los 4 tags.
inline bool isWheelEntityTag(const std::string& name) {
    return name == "wheel_FL" || name == "wheel_FR"
        || name == "wheel_RL" || name == "wheel_RR";
}

/// @brief Transform 3D con posicion / rotacion Euler (grados) / escala.
///        Rotacion euler simplifica la UI del Inspector; F2H70 agrega un
///        camino paralelo via `rotation` quaternion + flag `useQuaternion`
///        para sistemas que necesitan sync sin gimbal lock (VehicleSystem
///        leyendo poses de Jolt). El path por defecto sigue siendo euler
///        (compatibilidad con serializacion existente + UI).
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotationEuler{0.0f}; // X=pitch, Y=yaw, Z=roll; en grados
    glm::vec3 scale{1.0f};

    // F2H70: rotacion como quaternion para sync sin gimbal lock. Cuando
    // `useQuaternion == true`, `worldMatrix()` usa `rotation` en lugar de
    // `rotationEuler`. Sistemas que extraen poses de matrices fisicas
    // (VehicleSystem, RagdollSystem en F2H71+) lo activan via setter de
    // matriz (mira `setWorldRotationFromMatrix`). El Inspector lo resetea
    // a false cuando el usuario edita los campos euler para que la
    // intencion del dev gane.
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // identity (w,x,y,z)
    bool useQuaternion = false;

    // F2H70 Bloque A: offset Y de auto-elevacion para el render.
    // `worldMatrix()` aplica `position.y + pivotYOffset` para que el modelo
    // se renderee elevado por encima de la posicion logica del entity.
    // Caso de uso principal: vehicles con modelos cuyo origin esta en su
    // centro vertical — el VehicleSystem (o el SceneLoader al cargar)
    // setea `pivotYOffset = -aabbMin.y` y `position.y=0` del moodmap
    // produce un auto apoyado en el piso. Sin persistir (runtime only) —
    // se recalcula al cargar/spawnear. Drop-in para cualquier vehicle.
    f32 pivotYOffset = 0.0f;

    // F2H70.2 D5: offset de yaw del MESH visual respecto al frame "logico"
    // del entity. `worldMatrix()` aplica `Ry(pivotYawOffsetDeg)` despues
    // de la rotacion del dev. Sirve para reconciliar la convencion forward
    // del engine (+Z) con GLBs autoreados en otras convenciones (-Z, ±X)
    // sin tocar el moodmap. El SceneLoader (y VehicleSystem al materialize)
    // setea este campo desde `VehicleConfig::meshYawOffsetDeg`. Sin
    // persistir (runtime only). Patron simetrico a `pivotYOffset`: TC
    // queda en "frame logico" mientras el render path lo absorbe.
    //
    // Aplicado en LOCAL space del entity (post-multiply al R del dev). Si
    // el dev rota la entity en el moodmap (eg. `rotationEuler [0, 45, 0]`
    // para apuntar al auto en diagonal), la rotacion se compone correcto:
    // primero el yaw offset alinea el mesh con el frame del chasis fisico,
    // luego la R del dev orienta el chasis en el mundo.
    f32 pivotYawOffsetDeg = 0.0f;

    TransformComponent() = default;
    TransformComponent(glm::vec3 p, glm::vec3 s = glm::vec3(1.0f))
        : position(p), scale(s) {}

    /// @brief Matriz de modelo en coords de mundo. Orden: T * R * S.
    ///        Con `useQuaternion=false` (default): R = Ry * Rx * Rz
    ///        (yaw-pitch-roll; convencion FPS).
    ///        Con `useQuaternion=true`: R = mat4_cast(rotation) — sin gimbal.
    ///        F2H70: `pivotYOffset` se suma a `position.y` antes de translate.
    glm::mat4 worldMatrix() const {
        glm::vec3 effectivePos = position;
        effectivePos.y += pivotYOffset;
        glm::mat4 m = glm::translate(glm::mat4(1.0f), effectivePos);
        if (useQuaternion) {
            m = m * glm::mat4_cast(rotation);
        } else {
            m = glm::rotate(m, glm::radians(rotationEuler.y), glm::vec3(0, 1, 0));
            m = glm::rotate(m, glm::radians(rotationEuler.x), glm::vec3(1, 0, 0));
            m = glm::rotate(m, glm::radians(rotationEuler.z), glm::vec3(0, 0, 1));
        }
        // F2H70.2 D5: yaw offset visual (post-multiply en LOCAL space).
        if (pivotYawOffsetDeg != 0.0f) {
            m = glm::rotate(m, glm::radians(pivotYawOffsetDeg),
                              glm::vec3(0, 1, 0));
        }
        m = glm::scale(m, scale);
        return m;
    }
};

/// @brief Indica que la entidad se dibuja con un mesh + materiales asociados.
///        `mesh` es un id resolvible via `AssetManager::getMesh`; si el id es
///        invalido, getMesh() cae al slot 0 (cubo primitivo).
///        `materials` tiene un `MaterialAssetId` por submesh del MeshAsset.
///        Si es mas corto que el numero de submeshes, los submeshes
///        restantes usan el slot 0 (default material).
///
///        Antes del Hito 10 este componente tenia `IMesh* mesh + TextureAssetId
///        texture`. Hito 10: migrado a ids para persistir en .moodmap.
///        Hito 17: el slot pasa de `TextureAssetId` (textura sola) a
///        `MaterialAssetId` (material PBR completo). El upgrader del
///        SceneSerializer envuelve cada texture_path viejo (.moodmap v6)
///        en un material auto-generado al cargar.
struct MeshRendererComponent {
    MeshAssetId mesh = 0;                       // 0 = cubo primitivo (fallback)
    std::vector<MaterialAssetId> materials;     // 1 material por submesh

    /// @brief F2H67: si NO esta vacio, el render path filtra los sub-meshes
    ///        del MeshAsset y solo dibuja el que tenga `SubMesh.name ==
    ///        subMeshName`. Sirve para que multiples entities (chassis +
    ///        4 wheels) compartan UN solo MeshAsset (ej. `sedan.fbx`) pero
    ///        cada entity renderice solo su parte. Match case-sensitive
    ///        exacto; si no encuentra match, no dibuja nada para esa entity
    ///        (log warn una vez por mesh para no inundar).
    std::string subMeshName;

    /// @brief F2H70.3 H: si NO esta vacio, el render path SKIPea los sub-meshes
    ///        cuyo `SubMesh.name` empieza con este prefijo. Complementa a
    ///        `subMeshName` (que es "incluir solo uno"): esto es "excluir un
    ///        grupo". Lo usa el chassis de un vehiculo para NO dibujar las
    ///        ruedas (`wheel_*`) — esas las renderean 4 wheel-entities aparte
    ///        que rotan independiente. Runtime-only (lo setea VehicleSystem al
    ///        materializar); no se serializa.
    std::string hideSubMeshPrefix;

    MeshRendererComponent() = default;
    MeshRendererComponent(MeshAssetId m, MaterialAssetId mat)
        : mesh(m), materials{mat} {}
    MeshRendererComponent(MeshAssetId m, std::vector<MaterialAssetId> mats)
        : mesh(m), materials(std::move(mats)) {}

    /// @brief Devuelve el material del submesh i, o el slot 0 (default) si
    ///        la lista es mas corta.
    MaterialAssetId materialOrMissing(usize submeshIndex) const {
        return (submeshIndex < materials.size()) ? materials[submeshIndex] : 0;
    }
};

/// @brief Camara como componente. Stub por ahora; el editor sigue usando las
///        `EditorCamera`/`FpsCamera` dedicadas. Entra en uso real cuando
///        agreguemos sistema de "active camera" (Hito 13+).
struct CameraComponent {
    float fovDeg = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

/// @brief Luz. Activada en Hito 11 (Blinn-Phong forward).
///        - `Directional`: usa `direction` (no la posicion del Transform). Solo
///          se considera la PRIMERA encontrada (single sun).
///        - `Point`: usa la posicion del `TransformComponent`. Atenuacion
///          cuadratica suave hasta `radius`. Hasta MAX_POINT_LIGHTS=8 activas;
///          el resto se ignora con un warn al loguear.
struct LightComponent {
    enum class Type : u8 { Directional = 0, Point = 1 };

    Type type = Type::Point;
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;                    // solo Point
    glm::vec3 direction{0.0f, -1.0f, 0.0f};  // solo Directional, normalizada
    bool enabled = true;
    bool castShadows = false;                // solo Directional (Hito 16)
};

/// @brief Configuracion del entorno de render (Hito 15 Bloque 4):
///        skybox + fog + post-process (exposure + tonemap). Se agrega a UNA
///        entidad cualquiera de la scene (convencion: un objeto vacio
///        llamado "Environment"). Si hay mas de una entidad con este
///        componente, `EditorApplication` usa la primera que encuentra.
///        Si no hay ninguna, los uniforms quedan en sus defaults.
///
///        El campo `skyboxPath` esta reservado para un futuro asset
///        catalog de skyboxes (cubemap por proyecto). Hito 15 sigue
///        usando un cubemap fijo cargado al iniciar (`sky_day`); el campo
///        se persiste pero no se aplica todavia.
struct EnvironmentComponent {
    // Skybox (placeholder hasta que haya catalogo de cubemaps).
    std::string skyboxPath{"skyboxes/sky_day"};

    // Fog
    u32 fogMode = 0;                    // 0=Off, 1=Linear, 2=Exp, 3=Exp2
    glm::vec3 fogColor{0.55f, 0.65f, 0.75f};
    float fogDensity = 0.015f;
    float fogLinearStart = 5.0f;
    float fogLinearEnd = 50.0f;

    // Post-process
    float exposure = 0.0f;              // EVs
    u32 tonemapMode = 2;                // 0=None, 1=Reinhard, 2=ACES

    // IBL intensity (Hito 18). Multiplicador del aporte del IBL al
    // ambient del PBR. 1.0 = aporte completo del cubemap; 0.0 = sin
    // IBL (cae a `uAmbient` escalar). Util cuando el cubemap es muy
    // claro y "ahoga" las point lights y el directional. Tipicos:
    // 0.4-0.7 para escenas con luces directas; 1.0 para escenas
    // exteriores donde el cielo manda.
    float iblIntensity = 1.0f;

    // F2H55: bloom (glow). Algoritmo COD AW 2014, ver BloomPass.h.
    //   bloomEnabled:   master switch. Off salta el pass entero (sin costo).
    //   bloomThreshold: luminance HDR > threshold contribuye al halo.
    //                   1.0 = solo sobreexpuestos; 0.5 = aporte general.
    //   bloomIntensity: peso del bloom sobre el HDR. 0 = sin bloom,
    //                   1 = peso completo.
    //   bloomRadius:    escala del tent filter al upsamplear.
    //                   0.5 = ajustado, 2.0 = halo amplio.
    // F2H60 polish: defaults OFF -- pedido del dev: "todo deberia estar
    // desactivado por defecto y solo activar a gusto". Engine-grade
    // significa proveer la opcion, no imponer el look.
    bool  bloomEnabled   = false;
    float bloomThreshold = 1.0f;
    float bloomIntensity = 0.6f;
    float bloomRadius    = 1.0f;

    // F2H56: SSAO (Ambient Occlusion / sombras de rincon). Algoritmo
    // screen-space, ver SSAOPass.h. Multiplica el HDR scene color por
    // un factor [0..1] computado del depth buffer.
    //   ssaoEnabled:   master switch.
    //   ssaoRadius:    en view-space units. Tipico 0.3-0.8. Mas alto =
    //                  oclusion mas amplia (rincones grandes).
    //   ssaoIntensity: multiplicador del efecto. 0 = sin AO,
    //                  1 = standard, 2 = exagerado.
    // F2H60 polish: default OFF (ver bloomEnabled).
    bool  ssaoEnabled   = false;
    float ssaoRadius    = 0.5f;
    float ssaoIntensity = 1.0f;

    // F2H58: Color Grading (LUT 2D 256x16, layout Unity URP). Pre-tonemap.
    //   colorGradingEnabled:   master switch. Default OFF -- a diferencia
    //                          de bloom/SSAO, color grading sin LUT no
    //                          aporta y un look default cuestionable.
    //   colorGradingLutPath:   path logico al .png. Vacio = LUT identidad
    //                          (lookup(c) == c, no cambia nada).
    //   colorGradingIntensity: blend con el color original. 0 = sin grade,
    //                          1 = grade puro. Sweet spot 0.6-0.8.
    bool        colorGradingEnabled   = false;
    std::string colorGradingLutPath   = "";
    float       colorGradingIntensity = 1.0f;

    // F2H60: Cascade Shadow Maps. 3-4 cascadas para shadows del directional
    // light a distintas distancias -- mejor calidad de sombras sin perder
    // rendimiento. Sin "master switch" -- el gate de sombras es per-light
    // via LightComponent::castShadows. Aca solo viven los knobs de calidad.
    //   csmCascadeCount: 1..4. 1 = legacy single shadow map (back-compat).
    //                     Default 4 = mejor distribucion de resolucion.
    //   csmSplitLambda:  0..1. 0 = lineal (cascadas mismo tamano), 1 = log
    //                     (mas resolucion cerca), 0.5 = hybrid practico
    //                     (sweet spot PSSM Zhang 2006).
    u32   csmCascadeCount = 4;
    float csmSplitLambda  = 0.5f;

    // F2H61: SSR (Screen-Space Reflections). Algoritmo Morgan McGuire 2014
    // linear DDA en view-space. Ver SSRPass.h / shaders/ssr.frag.
    //   ssrEnabled:   master switch. Default OFF (mismo criterio que el
    //                 resto de los efectos post-F2H60 polish).
    //   ssrMaxSteps:  pasos del ray marching. 16=performance, 64+=calidad.
    //                 Lineal en costo.
    //   ssrThickness: tolerancia view-space para considerar hit valido.
    //                 0.1-1.0. Mas alto = mas reflejos pero artifacts al
    //                 cruzar geometria fina.
    //   ssrStepSize:  tamano de cada step view-space (units). 0.05=fino,
    //                 0.3=grueso. Combinar con maxSteps: rango = maxSteps
    //                 * stepSize.
    //   ssrIntensity: factor del reflejo aditivo. 0=apagado, 1=full.
    bool  ssrEnabled   = false;
    u32   ssrMaxSteps  = 32u;
    float ssrThickness = 0.5f;
    float ssrStepSize  = 0.2f;
    float ssrIntensity = 0.5f;
};

/// @brief Marca a una entidad como instancia de un prefab (Hito 14).
///        Solo guarda el path logico del `.moodprefab` que la origino — sin
///        propagacion bidireccional (cambiar el prefab no actualiza esta
///        instancia, eso queda para hitos posteriores).
///        El SceneSerializer persiste este path como `prefab_path` en el
///        `.moodmap`, asi un round-trip preserva el link.
struct PrefabLinkComponent {
    std::string path; // logico, ej. "prefabs/torch.moodprefab"

    PrefabLinkComponent() = default;
    PrefabLinkComponent(std::string p) : path(std::move(p)) {}
};

} // namespace Mood
