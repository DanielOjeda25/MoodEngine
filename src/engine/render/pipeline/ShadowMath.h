#pragma once

// Calculo puro (sin GL) de las matrices del shadow pass para una luz
// direccional cubriendo un bounding sphere de la escena. Vive separado de
// `ShadowPass` para que sea testeable y reutilizable (ej. visualizar el
// frustum de la luz en un debug renderer en hitos posteriores).

#include "core/Types.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace Mood {

/// @brief F2H60: maximo numero de cascadas soportadas por CSM. El shader
///        declara `uniform mat4 uLightSpaces[MAX_CSM_CASCADES]` con este
///        size; cambiar requiere actualizar `lit.frag`.
constexpr u32 kMaxCsmCascades = 4;

struct ShadowMatrices {
    glm::mat4 view{1.0f};
    glm::mat4 proj{1.0f};
    /// @brief Producto `proj * view`. Lo que se sube al shader como uLightSpace.
    glm::mat4 lightSpace{1.0f};
};

/// @brief Up vector seguro para `lookAt`. Si `lightDir` es casi paralelo a
///        (0,1,0), GL `lookAt` se rompe; en ese caso usamos (0,0,1).
inline glm::vec3 chooseLightUp(const glm::vec3& lightDir) {
    return (std::abs(lightDir.y) > 0.99f)
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);
}

/// @brief Computa view + proj para shadow mapping con bounding sphere.
///
/// @param lightDir       Direccion a la que apunta la luz (no necesariamente
///                       normalizada; la funcion la normaliza). Si es ~zero,
///                       cae a (0,-1,0).
/// @param sceneCenter    Centro del bounding sphere.
/// @param sceneRadius    Radio del bounding sphere. La proyeccion ortografica
///                       cubre un cuadrado de lado `2 * radius * 1.5`
///                       (padding 1.5x para no clippear sombras al borde).
inline ShadowMatrices computeShadowMatrices(const glm::vec3& lightDir,
                                             const glm::vec3& sceneCenter,
                                             f32 sceneRadius) {
    glm::vec3 dir = lightDir;
    const f32 len = glm::length(dir);
    if (len < 1e-4f) {
        dir = glm::vec3(0.0f, -1.0f, 0.0f);
    } else {
        dir /= len;
    }

    const glm::vec3 lightPos = sceneCenter - dir * sceneRadius;
    const glm::mat4 view = glm::lookAt(lightPos, sceneCenter,
                                        chooseLightUp(dir));

    const f32 r = sceneRadius * 1.5f;
    const glm::mat4 proj = glm::ortho(-r, r, -r, r,
                                       0.0f, 2.0f * sceneRadius);

    ShadowMatrices out;
    out.view = view;
    out.proj = proj;
    out.lightSpace = proj * view;
    return out;
}

// =============================================================================
// F2H60: Cascade Shadow Maps (CSM)
// =============================================================================

/// @brief Splits del frustum de camara para CSM (esquema PSSM practico de
///        Zhang 2006). Devuelve `cascadeCount + 1` distances:
///        `[near, split1, split2, ..., far]`. La cascada `i` cubre
///        `[splits[i], splits[i+1]]`.
///
/// @param cameraNear Near plane de la camara (view-space distance > 0).
/// @param cameraFar  Far plane de la camara. Para CSM tipicamente se usa
///                   un cap explicito (ej. 80m) mas chico que el far real
///                   para evitar cascadas enormes; pasarlo aca.
/// @param cascadeCount  Numero de cascadas (1..kMaxCsmCascades). 1 cascada
///                      degenera a `[near, far]` (equivalente a shadow map
///                      single pre-F2H60).
/// @param lambda     Blend factor entre lineal (0) y logaritmico (1).
///                   0.5 default = hybrid practico, equilibra resolucion
///                   cerca/lejos.
inline std::array<f32, kMaxCsmCascades + 1>
computeCsmSplits(f32 cameraNear, f32 cameraFar,
                  u32 cascadeCount, f32 lambda = 0.5f) {
    std::array<f32, kMaxCsmCascades + 1> splits{};
    splits.fill(cameraFar);  // valores no usados quedan en far

    if (cascadeCount == 0 || cascadeCount > kMaxCsmCascades) cascadeCount = 1;
    if (cameraNear < 1e-4f) cameraNear = 1e-4f;
    if (cameraFar <= cameraNear) cameraFar = cameraNear + 1.0f;
    lambda = std::clamp(lambda, 0.0f, 1.0f);

    splits[0] = cameraNear;
    splits[cascadeCount] = cameraFar;

    const f32 ratio = cameraFar / cameraNear;
    const f32 range = cameraFar - cameraNear;
    for (u32 i = 1; i < cascadeCount; ++i) {
        const f32 si = static_cast<f32>(i) / static_cast<f32>(cascadeCount);
        const f32 logSplit = cameraNear * std::pow(ratio, si);
        const f32 linSplit = cameraNear + range * si;
        splits[i] = lambda * logSplit + (1.0f - lambda) * linSplit;
    }
    return splits;
}

/// @brief Computa view + proj + lightSpace para UNA cascada de CSM,
///        ajustando un ortho que envuelve "tight" el sub-frustum entre
///        `splitNear` y `splitFar` de la camara.
///
///        Usa **bounding sphere** del sub-frustum (no AABB) para que el
///        ortho sea estable al rotar la camara — evita shimmering del
///        borde de las sombras al girar el view (truco standard MS/Unreal).
///
/// @param cameraView    View matrix de la camara principal.
/// @param cameraProj    Proj matrix de la camara principal (perspectiva).
/// @param splitNear     Distancia view-space (positiva) del near de la cascada.
/// @param splitFar      Distancia view-space (positiva) del far de la cascada.
/// @param lightDir      Direccion de la luz (apunta DESDE la luz HACIA la
///                       escena, igual que `LightComponent::direction`).
/// @param shadowMapSize Resolucion del shadow map en texeles (cuadrado).
///                       Usado para snap del centro a la grilla de texeles
///                       y reducir shimmering al mover la camara.
inline ShadowMatrices computeCascadeShadowMatrices(const glm::mat4& cameraView,
                                                     const glm::mat4& cameraProj,
                                                     f32 splitNear,
                                                     f32 splitFar,
                                                     const glm::vec3& lightDir,
                                                     u32 shadowMapSize) {
    // 1) Las 8 corners del sub-frustum en NDC. Usaremos `splitNear` y
    //    `splitFar` para acotar el rango de Z. Tomamos el frustum FULL
    //    NDC para X/Y (±1) y sustituimos las Z por las correspondientes
    //    al sub-rango antes de inverso-proyectar.
    //
    //    Approach mas robusto: reconstruir las 8 corners del sub-frustum
    //    desde camera-space (donde las distancias son view-space directas).
    //    Para eso necesitamos las componentes near/far del proj NDC. Aca
    //    tomamos un atajo: usar inverse(proj * view) sobre las corners
    //    NDC del frustum completo y luego escalar Z linealmente. Esto
    //    funciona para proyecciones perspectivas estandar.
    //
    //    Mas simple: obtener las 4 corners del near (z_ndc = -1) y far
    //    (z_ndc = +1) del frustum FULL en world, y luego interpolar entre
    //    ellas usando t = (splitNear - cameraNear) / (cameraFar - cameraNear).
    //    Como `cameraNear`/`cameraFar` no los recibimos, los inferimos
    //    desproyectando los corners NDC ±1 — eso nos da los corners
    //    full en world; luego linealmente interpolamos por t.
    const glm::mat4 invViewProj = glm::inverse(cameraProj * cameraView);

    // 8 corners del frustum FULL en NDC.
    const glm::vec3 ndcCorners[8] = {
        {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},
        {-1.0f,  1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f},
        {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f},
        {-1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f},
    };
    glm::vec3 worldFull[8];
    for (int i = 0; i < 8; ++i) {
        const glm::vec4 ws = invViewProj * glm::vec4(ndcCorners[i], 1.0f);
        worldFull[i] = glm::vec3(ws) / ws.w;
    }

    // Recuperar cameraNear/Far implicitos del proj: el ojo en view space
    // esta a Z=0; los corners near tienen ZviewSpace = -cameraNear y los
    // far ZviewSpace = -cameraFar. Para obtenerlos, transformamos un
    // corner near y un corner far a view space y leemos la Z.
    const glm::vec4 nearViewSpace = cameraView * glm::vec4(worldFull[0], 1.0f);
    const glm::vec4 farViewSpace  = cameraView * glm::vec4(worldFull[4], 1.0f);
    const f32 cameraNear = std::abs(nearViewSpace.z);
    const f32 cameraFar  = std::abs(farViewSpace.z);
    const f32 range = std::max(cameraFar - cameraNear, 1e-4f);

    const f32 tNear = std::clamp((splitNear - cameraNear) / range, 0.0f, 1.0f);
    const f32 tFar  = std::clamp((splitFar  - cameraNear) / range, 0.0f, 1.0f);

    // 2) Interpolar entre near-corner y far-corner para obtener los
    //    corners del sub-frustum [splitNear, splitFar] en world space.
    //    Convention: worldFull[0..3] son corners NEAR, worldFull[4..7]
    //    son corners FAR. La interpolacion lineal entre los puntos
    //    correspondientes (0-4, 1-5, 2-6, 3-7) da el sub-frustum.
    glm::vec3 subFrustum[8];
    for (int i = 0; i < 4; ++i) {
        const glm::vec3 nearPt = worldFull[i];
        const glm::vec3 farPt  = worldFull[i + 4];
        subFrustum[i]     = nearPt + (farPt - nearPt) * tNear;  // 4 near de la cascada
        subFrustum[i + 4] = nearPt + (farPt - nearPt) * tFar;   // 4 far de la cascada
    }

    // 3) Bounding sphere del sub-frustum (centro + radio).
    glm::vec3 center(0.0f);
    for (int i = 0; i < 8; ++i) center += subFrustum[i];
    center /= 8.0f;

    f32 radius = 0.0f;
    for (int i = 0; i < 8; ++i) {
        radius = std::max(radius, glm::length(subFrustum[i] - center));
    }

    // 4) Snap del centro a la grilla de texeles del shadow map para
    //    evitar shimmering al mover la camara. El centro se cuantiza a
    //    una grilla de tamano `2 * radius / shadowMapSize` en light
    //    space, lo que asegura que cada texel del shadow map representa
    //    siempre el mismo "trozo" de mundo.
    glm::vec3 dir = lightDir;
    if (glm::length(dir) < 1e-4f) {
        dir = glm::vec3(0.0f, -1.0f, 0.0f);
    } else {
        dir = glm::normalize(dir);
    }
    const glm::vec3 up = chooseLightUp(dir);
    const glm::mat4 lightViewProvisional =
        glm::lookAt(center - dir * radius, center, up);
    glm::vec4 centerInLight = lightViewProvisional * glm::vec4(center, 1.0f);
    const f32 texelSize = (2.0f * radius) /
                            static_cast<f32>(std::max<u32>(shadowMapSize, 1));
    if (texelSize > 0.0f) {
        centerInLight.x = std::floor(centerInLight.x / texelSize) * texelSize;
        centerInLight.y = std::floor(centerInLight.y / texelSize) * texelSize;
    }
    const glm::mat4 invLightViewProv = glm::inverse(lightViewProvisional);
    const glm::vec4 snappedCenterW = invLightViewProv * centerInLight;
    const glm::vec3 snappedCenter = glm::vec3(snappedCenterW);

    // 5) Matrices finales.
    const glm::mat4 view = glm::lookAt(snappedCenter - dir * radius,
                                         snappedCenter, up);
    const glm::mat4 proj = glm::ortho(-radius, radius, -radius, radius,
                                        0.0f, 2.0f * radius);

    ShadowMatrices out;
    out.view = view;
    out.proj = proj;
    out.lightSpace = proj * view;
    return out;
}

} // namespace Mood
