// break-B2: per-entity material + shader-graph resolution extraido de
// SceneRenderer_Render.cpp.
//
// drawSceneMeshRenderer reemplaza a la lambda local `drawMeshRenderer`
// del frame loop. Por submesh de la entity:
//   - Aplica sub-mesh selector / hide-prefix / hide-names (vehiculos).
//   - Resuelve material (albedo, metallic-roughness, AO, tints, blend
//     mode, opacity, IOR, refraction).
//   - Si el material tiene shaderGraphPath y el cache de F2H62 Bloque E
//     da un IShader, swap al graph shader para ese submesh + rebind de
//     uniforms; restaura defaultSh al salir del submesh.

#include "engine/render/scene_renderer/SceneRenderer.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/IRenderer.h"
#include "engine/render/rhi/IShader.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"
#include "engine/shader_graph/ShaderGraphCache.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace Mood {

void SceneRenderer::drawSceneMeshRenderer(IShader& defaultSh,
                                            MeshRendererComponent& mr,
                                            const glm::mat4& model,
                                            AssetManager& assets,
                                            const FrameContext& frame) {
    MeshAsset* asset = assets.getMesh(mr.mesh);
    if (asset == nullptr) return;
    // F2H82 Bloque B: centrado en runtime. Si la entity es una rueda con un
    // hub offset (sub-mesh NO centrado en el .glb), restamos ese centroide
    // antes del world matrix para que rote en su hub. (0,0,0) = sin offset
    // (el caso comun: props, chassis, y ruedas ya centradas como el
    // DeLorean) => effectiveModel == model, sin costo extra.
    const bool hasPivot = (mr.subMeshPivotOffset.x != 0.0f
                           || mr.subMeshPivotOffset.y != 0.0f
                           || mr.subMeshPivotOffset.z != 0.0f);
    const glm::mat4 effectiveModel =
        hasPivot ? model * glm::translate(glm::mat4(1.0f),
                                          -mr.subMeshPivotOffset)
                 : model;
    for (usize i = 0; i < asset->submeshes.size(); ++i) {
        const auto& sub = asset->submeshes[i];
        if (sub.mesh == nullptr) continue;
        // F2H67: sub-mesh selector. Si la entity pidio uno especifico,
        // skipear el resto (case-sensitive exact match).
        if (!mr.subMeshName.empty() && sub.name != mr.subMeshName) {
            continue;
        }
        // F2H70.3 H: exclude-prefix. El chassis de un vehiculo skipea las
        // ruedas (`wheel_*`) — las renderean 4 wheel-entities aparte.
        if (!mr.hideSubMeshPrefix.empty()
            && sub.name.rfind(mr.hideSubMeshPrefix, 0) == 0) {
            continue;
        }
        // F2H82 Bloque B: exclude por nombre exacto. El chassis de un auto
        // importado (ruedas sin convencion `wheel_*`) lista los 4 nombres
        // reales aca. Lista chica (4) => busqueda lineal trivial.
        if (!mr.hideSubMeshNames.empty()
            && std::find(mr.hideSubMeshNames.begin(),
                         mr.hideSubMeshNames.end(), sub.name)
                   != mr.hideSubMeshNames.end()) {
            continue;
        }
        // F2H68: el pivot-offset auto-center se evaluó y descartó —
        // funcionaba para wheels pero desfasaba el chassis "body" porque
        // restarle el centro del AABB lo movía 60cm hacia abajo del TF.
        // Decisión: para vehículos, 1 entity por vehículo con UN mesh
        // (sin sub-mesh selector por wheel). Las wheels físicas del
        // VehicleConstraint siguen funcionando, solo se pierde animación
        // visual de wheels rotando — aceptable hasta que tengamos un
        // model authoring pipeline propio.

        const MaterialAssetId matId =
            mr.materialOrMissing(sub.materialIndex);
        const MaterialAsset* mat = assets.getMaterial(matId);

        // F2H62 Bloque E: si el material tiene shaderGraphPath, pedimos
        // al cache un IShader compilado. Si lo da, lo usamos en lugar
        // del defaultSh para este submesh; sino, fallback transparente.
        // El cache reusa el program compilado entre frames; solo
        // recompila si el hash del GLSL cambia (dev edito el grafo).
        IShader* sh = &defaultSh;
        bool usingGraph = false;
        if (mat != nullptr && !mat->shaderGraphPath.empty() &&
            m_shaderGraphCache != nullptr) {
            IShader* gSh = m_shaderGraphCache->getOrCompile(
                mat->shaderGraphPath, assets);
            if (gSh != nullptr) {
                sh = gSh;
                usingGraph = true;
                // Rebind program + uniforms (estabamos en defaultSh).
                applySceneShaderUniforms(*sh, frame);
                sh->setMat4("uModel", effectiveModel);
                sh->setFloat("uTime", m_currentTime);
            }
        }

        // useAlbedoMap distingue "tint puro" (gold/plastic, false) de
        // "samplear textura" (true). El default material tiene
        // useAlbedoMap=true con albedo=0 => muestra missing.png como
        // warning visible.
        const bool hasAlbedo = (mat != nullptr && mat->useAlbedoMap);
        glActiveTexture(GL_TEXTURE0);
        assets.getTexture(hasAlbedo ? mat->albedo : 0)->bind(0);
        sh->setInt("uHasAlbedoMap", hasAlbedo ? 1 : 0);

        const bool hasMR =
            (mat != nullptr && mat->metallicRoughness != 0);
        glActiveTexture(GL_TEXTURE2);
        if (hasMR) {
            assets.getTexture(mat->metallicRoughness)->bind(2);
        } else {
            frame.dummyTex->bind(2);
        }
        sh->setInt("uHasMetallicRoughness", hasMR ? 1 : 0);

        const bool hasAo = (mat != nullptr && mat->ao != 0);
        glActiveTexture(GL_TEXTURE3);
        if (hasAo) {
            assets.getTexture(mat->ao)->bind(3);
        } else {
            frame.dummyTex->bind(3);
        }
        sh->setInt("uHasAoMap", hasAo ? 1 : 0);

        if (mat != nullptr) {
            sh->setVec3 ("uAlbedoTint",   mat->albedoTint);
            sh->setFloat("uMetallicMult", mat->metallicMult);
            sh->setFloat("uRoughnessMult",mat->roughnessMult);
            sh->setFloat("uAoMult",       mat->aoMult);
            // F2H63: blending per-material. uBlendMode=0 (Opaque) hace
            // que el shader use el path tradicional — los otros tres
            // uniforms quedan sin efecto.
            sh->setInt  ("uBlendMode",          static_cast<int>(mat->blendMode));
            sh->setFloat("uOpacity",            mat->opacity);
            sh->setFloat("uIor",                mat->ior);
            sh->setFloat("uRefractionStrength", mat->refractionStrength);
        } else {
            sh->setVec3 ("uAlbedoTint",   glm::vec3(1.0f));
            sh->setFloat("uMetallicMult", 0.0f);
            sh->setFloat("uRoughnessMult",0.5f);
            sh->setFloat("uAoMult",       1.0f);
            sh->setInt  ("uBlendMode",          0);   // Opaque default
            sh->setFloat("uOpacity",            1.0f);
            sh->setFloat("uIor",                1.0f);
            sh->setFloat("uRefractionStrength", 0.0f);
        }

        // F2H82 Bloque B: el caller setea uModel=model en `sh` antes de la
        // lambda; si hay hub offset hay que reescribirlo con effectiveModel
        // (el graph-path ya lo hizo arriba con su propio shader).
        if (hasPivot && !usingGraph) {
            sh->setMat4("uModel", effectiveModel);
        }
        glActiveTexture(GL_TEXTURE0);
        m_renderer->drawMesh(*sub.mesh, *sh);

        // Si swappeamos al graph shader, volvemos a bindear el
        // defaultSh para no afectar el siguiente submesh o entity
        // del loop externo (que asume defaultSh activo).
        if (usingGraph) {
            applySceneShaderUniforms(defaultSh, frame);
        }
    }
}

} // namespace Mood
