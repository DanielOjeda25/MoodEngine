#include "editor/panels/scene/InspectorPanel.h"

#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/audio/clips/AudioClip.h"
#include "engine/render/resources/MaterialAsset.h"
#include "engine/render/resources/MeshAsset.h"
#include "editor/selection/SelectionSet.h"            // F2H13
#include "engine/scene/components/BrushComponent.h"  // F2H11
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"

#include <imgui.h>

#include <cstdio>
#include <string>
#include <vector>

namespace Mood {

namespace {

// Hito 32 D: helper para empujar un EditPropertyCommand cuando el dev
// suelta un drag/edit en un widget del Inspector. Se llama
// INMEDIATAMENTE despues del widget para que `IsItem*` se refiera a el.
// Captura history desde el ui (puede ser null si todavia no inyectado).
template<typename T>
void pushEditIfDone(InspectorEditTracker& tracker, EditorUI* ui, Entity e,
                     const T& current,
                     typename EditPropertyCommand<T>::Setter setter,
                     const std::string& label) {
    HistoryStack* h = ui ? ui->historyStack() : nullptr;
    if (h == nullptr) return;
    trackPropertyEdit<T>(tracker, current, e, *h, std::move(setter), label);
}

// F2H23: helper estandar de ImGui samples — texto gris "(?)" con tooltip
// al hover. Sirve para descubribilidad sin inflar el panel con texto.
// Llamar INMEDIATAMENTE despues del widget que se quiere documentar.
void helpMarker(const char* desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", desc);
    }
}

// F2H23: detecta si el dev tiene un drag activo de tipo `type` (ej.
// "MOOD_TEXTURE_ASSET"). Sirve para cambiar el color de los botones
// drop-target (que el dev sepa "este boton acepta lo que arrastras").
bool isDragActiveOfType(const char* type) {
    const ImGuiPayload* p = ImGui::GetDragDropPayload();
    return p != nullptr && p->IsDataType(type);
}

} // namespace

void InspectorPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_ui == nullptr) {
        ImGui::TextDisabled("Editor UI no inyectado");
        ImGui::End();
        return;
    }

    Entity e = m_ui->selectedEntity();
    if (!e) {
        ImGui::TextDisabled("No hay entidad seleccionada");
        ImGui::TextDisabled("Click en el panel Hierarchy para elegir una.");
        ImGui::End();
        return;
    }

    // F2H13: header "+N adicionales" cuando hay multi-seleccion.
    // Inspector solo edita la `active`; multi-edit es diferido.
    if (m_ui != nullptr) {
        const SelectionSet& set = m_ui->selectionSet();
        if (set.selected.size() > 1) {
            ImGui::TextDisabled(
                "+%d entidad(es) adicional(es) seleccionada(s) — solo se "
                "edita la activa",
                static_cast<int>(set.selected.size() - 1));
            ImGui::Separator();
        }
    }

    // --- TagComponent ---
    if (e.hasComponent<TagComponent>()) {
        auto& tag = e.getComponent<TagComponent>();
        ImGui::SeparatorText("Tag");
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s", tag.name.c_str());
        if (ImGui::InputText("##tag", buf, sizeof(buf))) {
            tag.name = buf;
            m_editedThisFrame = true;
        }
        // Hito 32 D: undo/redo del nombre. InputText reporta IsItemDeactivatedAfterEdit
        // cuando el dev sale del campo (Tab, click fuera, Enter).
        pushEditIfDone<std::string>(m_editTracker, m_ui, e, tag.name,
            [](Entity& en, const std::string& v) {
                en.getComponent<TagComponent>().name = v;
            },
            "Renombrar entidad");
        ImGui::Separator();
    }

    // --- TransformComponent ---
    // Scale + rotation solo tienen efecto visible si hay MeshRenderer
    // (o es un tile rendereado). Para Light/Audio puros (sin mesh)
    // ocultarlos evita confundir al usuario con controles sin efecto.
    if (e.hasComponent<TransformComponent>()) {
        auto& t = e.getComponent<TransformComponent>();
        // F2H23: SeparatorText agrupa visualmente cada componente (antes
        // era TextDisabled + Separator separados; menos claro).
        ImGui::SeparatorText("Transform");
        if (ImGui::DragFloat3("position##tr", &t.position.x, 0.05f)) {
            m_editedThisFrame = true;
        }
        helpMarker("Posicion en metros (X derecha, Y arriba, Z atras).");
        pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, t.position,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<TransformComponent>().position = v;
            },
            "Mover entidad (Inspector)");

        // Rotation + scale: visibles si la entidad tiene geometria
        // visible (MeshRenderer / BrushComponent) o TriggerComponent
        // (rotation afecta el OBB del trigger — Hito 40 B). Para
        // Light/Audio sin geometria siguen ocultos.
        const bool showRotScale =
            e.hasComponent<MeshRendererComponent>() ||
            e.hasComponent<BrushComponent>() ||  // F2H14
            e.hasComponent<TriggerComponent>();
        if (showRotScale) {
            if (ImGui::DragFloat3("rotation (deg)##tr", &t.rotationEuler.x, 0.5f)) {
                m_editedThisFrame = true;
            }
            helpMarker("Rotacion en grados Euler (X, Y, Z) — orden YXZ.");
            pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, t.rotationEuler,
                [](Entity& en, const glm::vec3& v) {
                    en.getComponent<TransformComponent>().rotationEuler = v;
                },
                "Rotar entidad (Inspector)");

            if (ImGui::DragFloat3("scale##tr", &t.scale.x, 0.05f, 0.01f, 100.0f)) {
                m_editedThisFrame = true;
            }
            helpMarker("Escala (rango 0.01-100). 1.0 = tamano original.");
            pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, t.scale,
                [](Entity& en, const glm::vec3& v) {
                    en.getComponent<TransformComponent>().scale = v;
                },
                "Escalar entidad (Inspector)");
        }
    }

    // --- MeshRendererComponent ---
    // Con Hito 10, `mesh` es un MeshAssetId (no un IMesh* crudo) y `materials`
    // es un vector de TextureAssetId (1 por submesh). Muestra metadata del
    // mesh resuelto y la lista de materiales. Dropdown para cambiar el mesh
    // entra en Bloque 5 (drag & drop desde AssetBrowser).
    if (e.hasComponent<MeshRendererComponent>()) {
        auto& mr = e.getComponent<MeshRendererComponent>();
        ImGui::SeparatorText("MeshRenderer");
        if (m_assets != nullptr) {
            ImGui::Text("mesh: %s (id %u)",
                         m_assets->meshPathOf(mr.mesh).c_str(), mr.mesh);
            MeshAsset* asset = m_assets->getMesh(mr.mesh);
            if (asset != nullptr) {
                ImGui::Text("submeshes: %u, vertices: %u",
                             static_cast<u32>(asset->submeshes.size()),
                             asset->totalVertexCount());

                // F2H6: info de LODs (read-only en v1). Editar manualmente
                // o regenerar = hito futuro.
                const u32 lod0Tris = asset->totalVertexCount() / 3;
                u32 lod1Tris = 0, lod2Tris = 0;
                for (const auto& s : asset->lod1Submeshes) lod1Tris += s.vertexCount / 3;
                for (const auto& s : asset->lod2Submeshes) lod2Tris += s.vertexCount / 3;
                if (lod1Tris > 0 || lod2Tris > 0) {
                    ImGui::TextDisabled("LOD 0: %u tris (full)", lod0Tris);
                    ImGui::TextDisabled("LOD 1: %u tris", lod1Tris);
                    ImGui::TextDisabled("LOD 2: %u tris", lod2Tris);
                    ImGui::TextDisabled("LOD distances: %.0fm / %.0fm",
                                          static_cast<double>(asset->lodDistances.x),
                                          static_cast<double>(asset->lodDistances.y));
                } else if (asset->hasSkeleton()) {
                    ImGui::TextDisabled("LODs: skinned (no generado)");
                } else {
                    ImGui::TextDisabled("LODs: no aplicable (mesh chico)");
                }
            }
        } else {
            ImGui::Text("mesh id: %u", mr.mesh);
        }
        ImGui::Text("materials: %u", static_cast<u32>(mr.materials.size()));
        for (usize i = 0; i < mr.materials.size(); ++i) {
            const MaterialAssetId matId = mr.materials[i];
            const std::string matPath = m_assets->materialPathOf(matId);
            ImGui::PushID(static_cast<int>(i));
            // Header del slot: path del material (read-only).
            ImGui::SeparatorText(("Material slot " + std::to_string(i)).c_str());
            ImGui::TextDisabled("%s (id %u)", matPath.c_str(),
                                  static_cast<unsigned>(matId));

            MaterialAsset* mat = m_assets->getMaterial(matId);
            if (mat != nullptr) {
                // Sliders de los multiplicadores escalares. Editar muta el
                // MaterialAsset in-place: el render del frame siguiente lo
                // recoge automaticamente (no hay copia por entidad).
                // Hito 32 D: cada slider/color empuja un EditPropertyCommand
                // al soltar el drag. Setters capturan matId + AssetManager
                // porque el material vive en AssetManager, no en la entidad.
                AssetManager* assetsCap = m_assets;
                const MaterialAssetId matIdCap = matId;
                if (ImGui::ColorEdit3("albedoTint",
                        &mat->albedoTint.x, ImGuiColorEditFlags_NoInputs)) {
                    m_editedThisFrame = true;
                }
                pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, mat->albedoTint,
                    [assetsCap, matIdCap](Entity&, const glm::vec3& v) {
                        if (auto* m = assetsCap->getMaterial(matIdCap)) m->albedoTint = v;
                    },
                    "Editar albedoTint");

                if (ImGui::SliderFloat("metallic",
                        &mat->metallicMult, 0.0f, 1.0f, "%.2f")) {
                    m_editedThisFrame = true;
                }
                pushEditIfDone<f32>(m_editTracker, m_ui, e, mat->metallicMult,
                    [assetsCap, matIdCap](Entity&, const f32& v) {
                        if (auto* m = assetsCap->getMaterial(matIdCap)) m->metallicMult = v;
                    },
                    "Editar metallic");

                if (ImGui::SliderFloat("roughness",
                        &mat->roughnessMult, 0.04f, 1.0f, "%.2f")) {
                    m_editedThisFrame = true;
                }
                pushEditIfDone<f32>(m_editTracker, m_ui, e, mat->roughnessMult,
                    [assetsCap, matIdCap](Entity&, const f32& v) {
                        if (auto* m = assetsCap->getMaterial(matIdCap)) m->roughnessMult = v;
                    },
                    "Editar roughness");

                if (ImGui::SliderFloat("ao",
                        &mat->aoMult, 0.0f, 1.0f, "%.2f")) {
                    m_editedThisFrame = true;
                }
                pushEditIfDone<f32>(m_editTracker, m_ui, e, mat->aoMult,
                    [assetsCap, matIdCap](Entity&, const f32& v) {
                        if (auto* m = assetsCap->getMaterial(matIdCap)) m->aoMult = v;
                    },
                    "Editar ao");
                // Estado de las texturas — read-only por ahora salvo el
                // drop de albedo (Hito 35 A).
                ImGui::TextDisabled(
                    "albedo: %u  MR: %u  normal: %u  ao: %u",
                    mat->albedo, mat->metallicRoughness,
                    mat->normal,  mat->ao);
            }

            // Hito 35 A: drop de textura del AssetBrowser sobre este slot
            // -> reemplaza el material entero por uno nuevo (instance
            // unico) con la textura como albedo. Anti-contagio: nunca
            // muta el material previo (otras entidades pueden compartirlo).
            // Hito 36 A: ahora undoable — push de EditPropertyCommand<u32>
            // (MaterialAssetId == u32) con setter que indexa el slot via
            // captura por valor. Si no hay history disponible, fallback
            // a asignacion directa.
            // F2H23: highlight visual del drop target cuando el dev
            // tiene un drag activo de textura — color verde claro para
            // que el ojo encuentre rapido los slots aceptables.
            const bool dragTex = isDragActiveOfType("MOOD_TEXTURE_ASSET");
            if (dragTex) {
                ImGui::PushStyleColor(ImGuiCol_Button,
                                        ImVec4(0.20f, 0.55f, 0.25f, 1.0f));
            }
            ImGui::Button("Drop textura para reemplazar material",
                            ImVec2(-FLT_MIN, 0));
            if (dragTex) ImGui::PopStyleColor();
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* p =
                        ImGui::AcceptDragDropPayload("MOOD_TEXTURE_ASSET")) {
                    if (m_assets != nullptr && p->DataSize == sizeof(TextureAssetId)) {
                        const TextureAssetId tex =
                            *static_cast<const TextureAssetId*>(p->Data);
                        const MaterialAssetId oldMatId = mr.materials[i];
                        const MaterialAssetId newMatId =
                            m_assets->createMaterialFromTexture(tex);
                        const usize slotIndex = i;
                        HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
                        if (h != nullptr) {
                            auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                                e, oldMatId, newMatId,
                                [slotIndex](Entity& en, const u32& v) {
                                    auto& mrc = en.getComponent<MeshRendererComponent>();
                                    if (slotIndex < mrc.materials.size()) {
                                        mrc.materials[slotIndex] = v;
                                    }
                                },
                                "Reemplazar textura material");
                            h->push(std::move(cmd));  // execute() asigna newMatId
                        } else {
                            mr.materials[i] = newMatId;
                        }
                        m_editedThisFrame = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::PopID();
        }
        ImGui::Separator();
    }

    // --- CameraComponent ---
    if (e.hasComponent<CameraComponent>()) {
        auto& cam = e.getComponent<CameraComponent>();
        ImGui::SeparatorText("Camera");
        if (ImGui::DragFloat("fov (deg)##cam", &cam.fovDeg, 0.1f, 1.0f, 179.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<f32>(m_editTracker, m_ui, e, cam.fovDeg,
            [](Entity& en, const f32& v) {
                en.getComponent<CameraComponent>().fovDeg = v;
            },
            "Editar camera fov");
        if (ImGui::DragFloat("near##cam", &cam.nearPlane, 0.001f, 0.001f, 100.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<f32>(m_editTracker, m_ui, e, cam.nearPlane,
            [](Entity& en, const f32& v) {
                en.getComponent<CameraComponent>().nearPlane = v;
            },
            "Editar camera near");
        if (ImGui::DragFloat("far##cam", &cam.farPlane, 0.1f, 1.0f, 10000.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<f32>(m_editTracker, m_ui, e, cam.farPlane,
            [](Entity& en, const f32& v) {
                en.getComponent<CameraComponent>().farPlane = v;
            },
            "Editar camera far");
        ImGui::Separator();
    }

    // --- LightComponent (Hito 11) ---
    // Activado: tiene type / color / intensity (Hito 7) + radius (point) +
    // direction (directional) + enabled.
    if (e.hasComponent<LightComponent>()) {
        auto& lt = e.getComponent<LightComponent>();
        ImGui::SeparatorText("Light");

        if (ImGui::Checkbox("enabled##lt", &lt.enabled)) m_editedThisFrame = true;

        const char* items[] = {"Directional", "Point"};
        int current = static_cast<int>(lt.type);
        if (ImGui::Combo("type##lt", &current, items, 2)) {
            // Hito 40 F: undo del light type combo.
            const auto oldType = lt.type;
            const auto newType = static_cast<LightComponent::Type>(current);
            if (oldType != newType) {
                HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
                if (h != nullptr) {
                    auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                        e, static_cast<u32>(oldType), static_cast<u32>(newType),
                        [](Entity& en, const u32& v) {
                            en.getComponent<LightComponent>().type =
                                static_cast<LightComponent::Type>(v);
                        },
                        "Cambiar Light type");
                    h->push(std::move(cmd));
                } else {
                    lt.type = newType;
                }
                m_editedThisFrame = true;
            }
        }
        if (ImGui::ColorEdit3("color##lt", &lt.color.x)) m_editedThisFrame = true;
        pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, lt.color,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<LightComponent>().color = v;
            },
            "Editar light color");
        if (ImGui::DragFloat("intensity##lt", &lt.intensity, 0.01f, 0.0f, 100.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<f32>(m_editTracker, m_ui, e, lt.intensity,
            [](Entity& en, const f32& v) {
                en.getComponent<LightComponent>().intensity = v;
            },
            "Editar light intensity");

        if (lt.type == LightComponent::Type::Point) {
            if (ImGui::DragFloat("radius (m)##lt", &lt.radius, 0.1f, 0.1f, 1000.0f)) {
                m_editedThisFrame = true;
            }
            pushEditIfDone<f32>(m_editTracker, m_ui, e, lt.radius,
                [](Entity& en, const f32& v) {
                    en.getComponent<LightComponent>().radius = v;
                },
                "Editar light radius");
        } else {
            if (ImGui::DragFloat3("direction##lt", &lt.direction.x, 0.01f, -1.0f, 1.0f)) {
                m_editedThisFrame = true;
            }
            // Hito 16: solo directional puede emitir shadow map (point shadows
            // requeririan cubemap depth, fuera de scope).
            if (ImGui::Checkbox("castShadows##lt", &lt.castShadows)) {
                m_editedThisFrame = true;
            }
        }
        ImGui::Separator();
    }

    // --- EnvironmentComponent (Hito 15 Bloque 4) ---
    // Sky + fog + post-process. Solo el primer Environment encontrado en la
    // Scene se aplica al frame; varios = warning suave (no aca, en
    // EditorApplication). Cambios en vivo via DragFloat / Combo.
    if (e.hasComponent<EnvironmentComponent>()) {
        auto& env = e.getComponent<EnvironmentComponent>();
        ImGui::SeparatorText("Environment");

        ImGui::TextDisabled("Skybox: %s (asset catalog futuro)",
                             env.skyboxPath.c_str());

        ImGui::Separator();
        ImGui::TextUnformatted("Fog");
        const char* fogModes[] = {"Off", "Linear", "Exp", "Exp2"};
        int fogIdx = static_cast<int>(env.fogMode);
        if (ImGui::Combo("mode##env", &fogIdx, fogModes, 4)) {
            env.fogMode = static_cast<u32>(fogIdx);
            m_editedThisFrame = true;
        }
        if (ImGui::ColorEdit3("color##envfog", &env.fogColor.x)) m_editedThisFrame = true;
        pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, env.fogColor,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<EnvironmentComponent>().fogColor = v;
            },
            "Editar fog color");
        if (env.fogMode == 1) {
            if (ImGui::DragFloat("start (m)##env", &env.fogLinearStart, 0.1f, 0.0f, 500.0f)) m_editedThisFrame = true;
            pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogLinearStart,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().fogLinearStart = v;
                },
                "Editar fog linear start");
            if (ImGui::DragFloat("end (m)##env",   &env.fogLinearEnd,   0.1f, 0.0f, 500.0f)) m_editedThisFrame = true;
            pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogLinearEnd,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().fogLinearEnd = v;
                },
                "Editar fog linear end");
        } else if (env.fogMode == 2 || env.fogMode == 3) {
            if (ImGui::DragFloat("density##env", &env.fogDensity, 0.001f, 0.0f, 1.0f)) m_editedThisFrame = true;
            pushEditIfDone<f32>(m_editTracker, m_ui, e, env.fogDensity,
                [](Entity& en, const f32& v) {
                    en.getComponent<EnvironmentComponent>().fogDensity = v;
                },
                "Editar fog density");
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Post-process");
        if (ImGui::DragFloat("exposure (EV)##env", &env.exposure, 0.05f, -5.0f, 5.0f)) m_editedThisFrame = true;
        pushEditIfDone<f32>(m_editTracker, m_ui, e, env.exposure,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().exposure = v;
            },
            "Editar exposure");
        const char* tonemaps[] = {"None", "Reinhard", "ACES"};
        int toneIdx = static_cast<int>(env.tonemapMode);
        if (ImGui::Combo("tonemap##env", &toneIdx, tonemaps, 3)) {
            env.tonemapMode = static_cast<u32>(toneIdx);
            m_editedThisFrame = true;
        }
        // Hito 18: control del aporte del IBL al ambient. Util cuando el
        // skybox es muy claro y "ahoga" la directional + point lights.
        if (ImGui::SliderFloat("IBL intensity##env",
                                &env.iblIntensity, 0.0f, 2.0f, "%.2f")) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<f32>(m_editTracker, m_ui, e, env.iblIntensity,
            [](Entity& en, const f32& v) {
                en.getComponent<EnvironmentComponent>().iblIntensity = v;
            },
            "Editar IBL intensity");
        ImGui::Separator();
    }

    // --- ScriptComponent ---
    // Cambiar el path o pulsar Recargar pone `loaded=false`: el ScriptSystem
    // crea un sol::state fresco la proxima vez que corra update(). Eso es
    // mas fuerte que el hot-reload por mtime (que reutiliza el state);
    // cuando el usuario recarga manualmente lo habitual es querer un reset
    // limpio de globals.
    if (e.hasComponent<ScriptComponent>()) {
        auto& sc = e.getComponent<ScriptComponent>();
        ImGui::SeparatorText("Script");
        char buf[512];
        std::snprintf(buf, sizeof(buf), "%s", sc.path.c_str());
        if (ImGui::InputText("path##sc", buf, sizeof(buf))) {
            sc.path = buf;
            sc.loaded = false;
            sc.lastError.clear();
            m_editedThisFrame = true;
        }
        if (ImGui::Button("Recargar##sc")) {
            sc.loaded = false;
            sc.lastError.clear();
        }
        if (!sc.lastError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
            ImGui::TextWrapped("Error: %s", sc.lastError.c_str());
            ImGui::PopStyleColor();
        }

        // --- Exposed properties (Hito 24) ---
        // Listadas en el orden que aparecieron al cargar el script —
        // estable mientras el script no edite las llamadas
        // engine.exposed. El widget depende del tipo inferido por el
        // binding; un boton "Reset" borra el override (vuelve al
        // default del script).
        if (!sc.exposedProps.empty()) {
            ImGui::TextDisabled("Exposed properties");
            for (const auto& prop : sc.exposedProps) {
                ImGui::PushID(prop.name.c_str());

                // Resolver el valor a mostrar: override si esta, sino
                // default. El editor escribe siempre al overrides map,
                // asi al primer edit se "materializa".
                ExposedValue current = prop.defaultValue;
                auto ovIt = sc.overrides.find(prop.name);
                if (ovIt != sc.overrides.end()) current = ovIt->second;

                bool changed = false;
                switch (prop.type) {
                    case ExposedType::Number: {
                        f32 v = std::get<f32>(current);
                        if (ImGui::DragFloat(prop.name.c_str(), &v, 0.1f)) {
                            sc.overrides[prop.name] = v;
                            changed = true;
                        }
                        break;
                    }
                    case ExposedType::Bool: {
                        bool v = std::get<bool>(current);
                        if (ImGui::Checkbox(prop.name.c_str(), &v)) {
                            sc.overrides[prop.name] = v;
                            changed = true;
                        }
                        break;
                    }
                    case ExposedType::String: {
                        std::string v = std::get<std::string>(current);
                        char sbuf[256];
                        std::snprintf(sbuf, sizeof(sbuf), "%s", v.c_str());
                        if (ImGui::InputText(prop.name.c_str(),
                                             sbuf, sizeof(sbuf))) {
                            sc.overrides[prop.name] = std::string(sbuf);
                            changed = true;
                        }
                        break;
                    }
                    case ExposedType::Vec3: {
                        glm::vec3 v = std::get<glm::vec3>(current);
                        const bool isColor =
                            prop.name.find("color") != std::string::npos ||
                            prop.name.find("Color") != std::string::npos;
                        bool edited = false;
                        if (isColor) {
                            edited = ImGui::ColorEdit3(prop.name.c_str(), &v.x);
                        } else {
                            edited = ImGui::DragFloat3(prop.name.c_str(), &v.x, 0.1f);
                        }
                        if (edited) {
                            sc.overrides[prop.name] = v;
                            changed = true;
                        }
                        break;
                    }
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("Reset")) {
                    sc.overrides.erase(prop.name);
                    changed = true;
                }

                // Cualquier cambio fuerza reload — el chunk top-level
                // se re-corre y engine.exposed devuelve el nuevo valor.
                if (changed) {
                    sc.loaded = false;
                    sc.lastError.clear();
                    m_editedThisFrame = true;
                }

                ImGui::PopID();
            }
        }

        ImGui::Separator();
    }

    // --- RigidBodyComponent (Hito 12) ---
    // Edit del shape/mass requiere recrear el body en Jolt, asi que por
    // ahora los campos quedan read-only en Play Mode y editables en Editor.
    // Al salir de Play, cualquier cambio toma efecto via rebuildSceneFromMap
    // si se recarga el proyecto, o al destruir/recrear el body manualmente.
    if (e.hasComponent<RigidBodyComponent>()) {
        auto& rb = e.getComponent<RigidBodyComponent>();
        ImGui::SeparatorText("RigidBody");

        const char* typeNames[] = {"Static", "Kinematic", "Dynamic"};
        int typeIdx = static_cast<int>(rb.type);
        if (ImGui::Combo("type##rb", &typeIdx, typeNames, 3)) {
            // Hito 40 F: undo via EditPropertyCommand<u32> (cambio
            // estructural, atomico — no usa el tracker drag-end).
            const auto oldType = rb.type;
            const auto newType = static_cast<RigidBodyComponent::Type>(typeIdx);
            if (oldType != newType) {
                HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
                if (h != nullptr) {
                    auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                        e, static_cast<u32>(oldType), static_cast<u32>(newType),
                        [](Entity& en, const u32& v) {
                            en.getComponent<RigidBodyComponent>().type =
                                static_cast<RigidBodyComponent::Type>(v);
                        },
                        "Cambiar RigidBody type");
                    h->push(std::move(cmd));
                } else {
                    rb.type = newType;
                }
                m_editedThisFrame = true;
            }
        }

        const char* shapeNames[] = {"Box", "Sphere", "Capsule"};
        int shapeIdx = static_cast<int>(rb.shape);
        if (ImGui::Combo("shape##rb", &shapeIdx, shapeNames, 3)) {
            // Hito 40 F: undo del shape combo.
            const auto oldShape = rb.shape;
            const auto newShape = static_cast<RigidBodyComponent::Shape>(shapeIdx);
            if (oldShape != newShape) {
                HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
                if (h != nullptr) {
                    auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                        e, static_cast<u32>(oldShape), static_cast<u32>(newShape),
                        [](Entity& en, const u32& v) {
                            en.getComponent<RigidBodyComponent>().shape =
                                static_cast<RigidBodyComponent::Shape>(v);
                        },
                        "Cambiar RigidBody shape");
                    h->push(std::move(cmd));
                } else {
                    rb.shape = newShape;
                }
                m_editedThisFrame = true;
            }
        }

        if (ImGui::DragFloat3("halfExtents##rb", &rb.halfExtents.x, 0.05f, 0.01f, 100.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, rb.halfExtents,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<RigidBodyComponent>().halfExtents = v;
            },
            "Editar rigid body halfExtents");
        if (rb.type == RigidBodyComponent::Type::Dynamic) {
            if (ImGui::DragFloat("mass (kg)##rb", &rb.mass, 0.1f, 0.001f, 10000.0f)) {
                m_editedThisFrame = true;
            }
            pushEditIfDone<f32>(m_editTracker, m_ui, e, rb.mass,
                [](Entity& en, const f32& v) {
                    en.getComponent<RigidBodyComponent>().mass = v;
                },
                "Editar rigid body mass");
        }
        // Hito 34 A: friction. Aplica a static + dynamic (el contacto en
        // ambos lados afecta el comportamiento).
        if (ImGui::DragFloat("friction##rb", &rb.friction, 0.01f, 0.0f, 2.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<f32>(m_editTracker, m_ui, e, rb.friction,
            [](Entity& en, const f32& v) {
                en.getComponent<RigidBodyComponent>().friction = v;
            },
            "Editar friction (RigidBody)");
        ImGui::TextDisabled("body id: %u (0 = no materializado) — re-Play para aplicar friction", rb.bodyId);
        ImGui::Separator();
    }

    // --- AudioSourceComponent (Hito 9) ---
    // Dropdown poblado a partir de los clips cargados en el AssetManager.
    // Botón "Reproducir" resetea `started` para que AudioSystem dispare de
    // nuevo en el próximo update (util para preview sin entrar a Play Mode).
    if (e.hasComponent<AudioSourceComponent>()) {
        auto& asrc = e.getComponent<AudioSourceComponent>();
        ImGui::SeparatorText("AudioSource");

        // Lista de clips del manager como combo. Si aun no hay assets manager
        // inyectado, mostrar solo el id crudo.
        if (m_assets != nullptr) {
            std::vector<std::string> labels;
            labels.reserve(m_assets->audioCount());
            for (AudioAssetId i = 0; i < m_assets->audioCount(); ++i) {
                labels.push_back(m_assets->audioPathOf(i));
            }
            int current = static_cast<int>(asrc.clip);
            if (current < 0 || current >= static_cast<int>(labels.size())) {
                current = 0;
            }
            // BeginCombo para soportar paths largos con scroll.
            if (ImGui::BeginCombo("clip##as", labels[current].c_str())) {
                for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
                    const bool selected = (current == i);
                    if (ImGui::Selectable(labels[i].c_str(), selected)) {
                        asrc.clip = static_cast<AudioAssetId>(i);
                        asrc.started = false; // obligar a re-arrancar con el nuevo clip
                        m_editedThisFrame = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::Text("clip id: %u (asset manager no inyectado)", asrc.clip);
        }

        if (ImGui::SliderFloat("volume##as", &asrc.volume, 0.0f, 1.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<f32>(m_editTracker, m_ui, e, asrc.volume,
            [](Entity& en, const f32& v) {
                en.getComponent<AudioSourceComponent>().volume = v;
            },
            "Editar audio volume");
        if (ImGui::Checkbox("loop##as", &asrc.loop)) { m_editedThisFrame = true; }
        pushEditIfDone<bool>(m_editTracker, m_ui, e, asrc.loop,
            [](Entity& en, const bool& v) {
                en.getComponent<AudioSourceComponent>().loop = v;
            },
            "Toggle audio loop");
        ImGui::SameLine();
        if (ImGui::Checkbox("playOnStart##as", &asrc.playOnStart)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<bool>(m_editTracker, m_ui, e, asrc.playOnStart,
            [](Entity& en, const bool& v) {
                en.getComponent<AudioSourceComponent>().playOnStart = v;
            },
            "Toggle audio playOnStart");
        ImGui::SameLine();
        if (ImGui::Checkbox("is3D##as", &asrc.is3D)) { m_editedThisFrame = true; }
        pushEditIfDone<bool>(m_editTracker, m_ui, e, asrc.is3D,
            [](Entity& en, const bool& v) {
                en.getComponent<AudioSourceComponent>().is3D = v;
            },
            "Toggle audio is3D");

        // Preview: resetear `started` fuerza al AudioSystem a volver a
        // disparar la reproduccion en el proximo frame. Requiere playOnStart.
        if (ImGui::Button("Reproducir##as")) {
            asrc.started = false;
            if (!asrc.playOnStart) {
                // Preview implicito: encender playOnStart temporalmente no
                // es ideal (persiste). Mejor: advertir al usuario.
                ImGui::OpenPopup("audio_preview_note");
            }
        }
        if (ImGui::BeginPopup("audio_preview_note")) {
            ImGui::TextUnformatted("Activa 'playOnStart' para que suene al disparar.");
            ImGui::EndPopup();
        }
        ImGui::Separator();
    }

    // --- AnimatorComponent (Hito 19) ---
    // Combo de clips poblado desde el MeshAsset de la entidad. Editar
    // el clipName resetea `time` para que el clip nuevo arranque desde
    // el frame 0.
    if (e.hasComponent<AnimatorComponent>()) {
        auto& anim = e.getComponent<AnimatorComponent>();
        ImGui::SeparatorText("Animator");

        // Necesitamos el MeshAsset para listar los clips. Lo resolvemos
        // via MeshRenderer (mismo flujo que el AnimationSystem).
        if (m_assets != nullptr && e.hasComponent<MeshRendererComponent>()) {
            const auto& mr = e.getComponent<MeshRendererComponent>();
            const MeshAsset* mesh = m_assets->getMesh(mr.mesh);
            if (mesh != nullptr && !mesh->animations.empty()) {
                std::vector<std::string> clipNames;
                clipNames.reserve(mesh->animations.size());
                for (const auto& c : mesh->animations) clipNames.push_back(c.name);

                int currentIdx = 0;
                for (int i = 0; i < static_cast<int>(clipNames.size()); ++i) {
                    if (clipNames[i] == anim.clipName) { currentIdx = i; break; }
                }
                if (ImGui::BeginCombo("clip##anim", clipNames[currentIdx].c_str())) {
                    for (int i = 0; i < static_cast<int>(clipNames.size()); ++i) {
                        const bool selected = (currentIdx == i);
                        if (ImGui::Selectable(clipNames[i].c_str(), selected)) {
                            // Hito 40 F: undo del clipName combo (string).
                            const std::string oldClip = anim.clipName;
                            const std::string newClip = clipNames[i];
                            if (oldClip != newClip) {
                                HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
                                if (h != nullptr) {
                                    auto cmd = std::make_unique<EditPropertyCommand<std::string>>(
                                        e, oldClip, newClip,
                                        [](Entity& en, const std::string& v) {
                                            auto& a = en.getComponent<AnimatorComponent>();
                                            a.clipName = v;
                                            a.time = 0.0f;  // reset al cambiar
                                        },
                                        "Cambiar Animator clip");
                                    h->push(std::move(cmd));
                                } else {
                                    anim.clipName = newClip;
                                    anim.time = 0.0f;
                                }
                                m_editedThisFrame = true;
                            }
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::TextDisabled(
                    "duration: %.2fs  time: %.2fs",
                    mesh->animations[currentIdx].duration, anim.time);
            } else {
                ImGui::TextDisabled("Mesh sin animaciones");
            }
        }

        if (ImGui::DragFloat("speed##anim", &anim.speed, 0.05f, 0.0f, 10.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<f32>(m_editTracker, m_ui, e, anim.speed,
            [](Entity& en, const f32& v) {
                en.getComponent<AnimatorComponent>().speed = v;
            },
            "Editar animator speed");
        if (ImGui::Checkbox("playing##anim", &anim.playing)) { m_editedThisFrame = true; }
        pushEditIfDone<bool>(m_editTracker, m_ui, e, anim.playing,
            [](Entity& en, const bool& v) {
                en.getComponent<AnimatorComponent>().playing = v;
            },
            "Toggle animator playing");
        ImGui::SameLine();
        if (ImGui::Checkbox("loop##anim", &anim.loop)) { m_editedThisFrame = true; }
        pushEditIfDone<bool>(m_editTracker, m_ui, e, anim.loop,
            [](Entity& en, const bool& v) {
                en.getComponent<AnimatorComponent>().loop = v;
            },
            "Toggle animator loop");
        ImGui::SameLine();
        if (ImGui::Button("Reset##anim")) {
            anim.time = 0.0f;
            m_editedThisFrame = true;
        }
        ImGui::Separator();
    }

    // --- ParticleEmitterComponent (Hito 29) ---
    // Editar campos en vivo afecta la simulacion del frame siguiente.
    // El render del proximo frame ya muestra los nuevos parametros.
    if (e.hasComponent<ParticleEmitterComponent>()) {
        auto& em = e.getComponent<ParticleEmitterComponent>();
        ImGui::TextDisabled("Particle Emitter");

        if (ImGui::Checkbox("emitting##pe", &em.emitting)) m_editedThisFrame = true;
        pushEditIfDone<bool>(m_editTracker, m_ui, e, em.emitting,
            [](Entity& en, const bool& v) {
                en.getComponent<ParticleEmitterComponent>().emitting = v;
            },
            "Toggle particle emitting");
        ImGui::SameLine();
        if (ImGui::Checkbox("additive##pe", &em.additive)) m_editedThisFrame = true;
        pushEditIfDone<bool>(m_editTracker, m_ui, e, em.additive,
            [](Entity& en, const bool& v) {
                en.getComponent<ParticleEmitterComponent>().additive = v;
            },
            "Toggle particle additive");
        ImGui::SameLine();
        if (ImGui::Checkbox("localSpace##pe", &em.localSpace)) m_editedThisFrame = true;
        pushEditIfDone<bool>(m_editTracker, m_ui, e, em.localSpace,
            [](Entity& en, const bool& v) {
                en.getComponent<ParticleEmitterComponent>().localSpace = v;
            },
            "Toggle particle localSpace");

        // Hito 37 C / 39 A: shape de emision (Point/Box/Sphere/Disc/Cone).
        using ES = ParticleEmitterComponent::EmissionShape;
        const char* shapeNames[] = {"Point", "Box", "Sphere", "Disc", "Cone"};
        int shapeIdx = static_cast<int>(em.emissionShape);
        if (ImGui::Combo("emit shape##pe", &shapeIdx, shapeNames, 5)) {
            em.emissionShape = static_cast<ES>(shapeIdx);
            m_editedThisFrame = true;
        }
        if (em.emissionShape != ES::Point) {
            if (ImGui::DragFloat("shape size (m)##pe",
                                  &em.emissionShapeSize, 0.05f, 0.01f, 100.0f)) {
                m_editedThisFrame = true;
            }
            pushEditIfDone<f32>(m_editTracker, m_ui, e, em.emissionShapeSize,
                [](Entity& en, const f32& v) {
                    en.getComponent<ParticleEmitterComponent>().emissionShapeSize = v;
                },
                "Editar emission shape size");
        }
        // Hito 40 A: cone axis solo visible si shape == Cone.
        if (em.emissionShape == ES::Cone) {
            if (ImGui::DragFloat3("cone axis##pe",
                                    &em.emissionConeAxis.x, 0.01f, -1.0f, 1.0f)) {
                m_editedThisFrame = true;
            }
            pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, em.emissionConeAxis,
                [](Entity& en, const glm::vec3& v) {
                    en.getComponent<ParticleEmitterComponent>().emissionConeAxis = v;
                },
                "Editar cone axis");
        }

        if (ImGui::DragFloat("rate (1/s)##pe", &em.emitRate, 1.0f, 0.0f, 10000.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<f32>(m_editTracker, m_ui, e, em.emitRate,
            [](Entity& en, const f32& v) {
                en.getComponent<ParticleEmitterComponent>().emitRate = v;
            },
            "Editar emit rate");
        if (ImGui::DragFloatRange2("lifetime (s)##pe",
                                     &em.lifetimeMin, &em.lifetimeMax,
                                     0.05f, 0.05f, 60.0f)) {
            m_editedThisFrame = true;
        }
        // Hito 40 E: undo del DragFloatRange2 (par de floats).
        pushEditIfDone<std::pair<f32, f32>>(m_editTracker, m_ui, e,
            std::pair<f32, f32>{em.lifetimeMin, em.lifetimeMax},
            [](Entity& en, const std::pair<f32, f32>& v) {
                auto& emc = en.getComponent<ParticleEmitterComponent>();
                emc.lifetimeMin = v.first;
                emc.lifetimeMax = v.second;
            },
            "Editar lifetime range");
        if (ImGui::DragFloat3("velMin (m/s)##pe", &em.velocityMin.x, 0.05f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, em.velocityMin,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<ParticleEmitterComponent>().velocityMin = v;
            },
            "Editar particle velocityMin");
        if (ImGui::DragFloat3("velMax (m/s)##pe", &em.velocityMax.x, 0.05f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, em.velocityMax,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<ParticleEmitterComponent>().velocityMax = v;
            },
            "Editar particle velocityMax");
        if (ImGui::DragFloatRange2("size (m)##pe",
                                     &em.sizeStart, &em.sizeEnd,
                                     0.005f, 0.0f, 5.0f)) {
            m_editedThisFrame = true;
        }
        // Hito 40 E: undo del DragFloatRange2 (par de floats) — size.
        pushEditIfDone<std::pair<f32, f32>>(m_editTracker, m_ui, e,
            std::pair<f32, f32>{em.sizeStart, em.sizeEnd},
            [](Entity& en, const std::pair<f32, f32>& v) {
                auto& emc = en.getComponent<ParticleEmitterComponent>();
                emc.sizeStart = v.first;
                emc.sizeEnd   = v.second;
            },
            "Editar size range");
        if (ImGui::ColorEdit4("colorStart##pe", &em.colorStart.x)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<glm::vec4>(m_editTracker, m_ui, e, em.colorStart,
            [](Entity& en, const glm::vec4& v) {
                en.getComponent<ParticleEmitterComponent>().colorStart = v;
            },
            "Editar particle colorStart");
        if (ImGui::ColorEdit4("colorEnd##pe", &em.colorEnd.x)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<glm::vec4>(m_editTracker, m_ui, e, em.colorEnd,
            [](Entity& en, const glm::vec4& v) {
                en.getComponent<ParticleEmitterComponent>().colorEnd = v;
            },
            "Editar particle colorEnd");
        if (ImGui::DragFloat("gravityFactor##pe", &em.gravityFactor, 0.01f,
                              -2.0f, 2.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<f32>(m_editTracker, m_ui, e, em.gravityFactor,
            [](Entity& en, const f32& v) {
                en.getComponent<ParticleEmitterComponent>().gravityFactor = v;
            },
            "Editar gravity factor (particles)");
        if (ImGui::DragInt("maxParticles##pe",
                             reinterpret_cast<int*>(&em.maxParticles),
                             1.0f, 1, 4096)) {
            // Resize lazy en el proximo update; vaciamos pool ahora para
            // que no haya particulas vivas con indices fuera de rango.
            em.alive.clear();
            em.positions.clear();
            em.velocities.clear();
            em.ages.clear();
            em.lifetimes.clear();
            em.aliveCount = 0;
            m_editedThisFrame = true;
        }
        // Hito 36 B: undo del DragInt. El setter del comando reaplica el
        // mismo cleanup de la pool runtime que el handler manual de arriba —
        // sin esto, un undo dejaria particulas vivas con indices >= la
        // nueva capacidad.
        pushEditIfDone<u32>(m_editTracker, m_ui, e, em.maxParticles,
            [](Entity& en, const u32& v) {
                auto& emc = en.getComponent<ParticleEmitterComponent>();
                emc.maxParticles = v;
                emc.alive.clear();
                emc.positions.clear();
                emc.velocities.clear();
                emc.ages.clear();
                emc.lifetimes.clear();
                emc.aliveCount = 0;
            },
            "Editar maxParticles");
        ImGui::TextDisabled("vivas: %u / %u",
                             em.aliveCount, em.maxParticles);

        if (m_assets != nullptr) {
            const std::string texPath = m_assets->pathOf(em.texture);
            ImGui::TextDisabled("textura: %s", texPath.c_str());
        }
        ImGui::Separator();
    }

    // --- TriggerComponent (Hito 33) ---
    // AABB centrado en TransformComponent.position con tamaño halfExtents*2.
    // halfExtents se persiste en el .moodmap; playerInside es runtime.
    if (e.hasComponent<TriggerComponent>()) {
        auto& tc = e.getComponent<TriggerComponent>();
        ImGui::SeparatorText("Trigger");
        if (ImGui::DragFloat3("halfExtents##trig", &tc.halfExtents.x,
                                0.05f, 0.01f, 100.0f)) {
            m_editedThisFrame = true;
        }
        pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, tc.halfExtents,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<TriggerComponent>().halfExtents = v;
            },
            "Editar trigger halfExtents");
        ImGui::TextDisabled("playerInside: %s",
                             tc.playerInside ? "si" : "no");
        ImGui::Separator();
    }

    // --- BrushComponent (F2H11) ---
    // Read-only en F2H11. F2H13 agrega edicion de primitivas (cilindro,
    // esfera, etc.); F2H14 agrega editor de UV per-cara con drag &
    // drop de materiales; F2H15 agrega seleccion de cara individual.
    if (e.hasComponent<BrushComponent>()) {
        auto& bc = e.getComponent<BrushComponent>();
        ImGui::TextDisabled("Brush (CSG)");

        ImGui::Text("faces: %u", static_cast<u32>(bc.brush.faces.size()));

        const glm::vec3 size = bc.brush.localAabb.size();
        ImGui::TextDisabled("local AABB: %.2f x %.2f x %.2f",
                             static_cast<double>(size.x),
                             static_cast<double>(size.y),
                             static_cast<double>(size.z));

        if (m_assets != nullptr) {
            // F2H17: el brush tiene N slots de material (uno por
            // material distinto entre las caras). Mostrar todos.
            ImGui::Text("materiales: %u slots",
                         static_cast<u32>(bc.materials.size()));
            for (u32 i = 0; i < bc.materials.size(); ++i) {
                const MaterialAssetId mid = bc.materials[i];
                const std::string matPath = (mid == 0)
                    ? std::string{"(blank look)"}
                    : m_assets->materialPathOf(mid);
                ImGui::TextDisabled("  [%u] %s (id %u)", i,
                                       matPath.c_str(),
                                       static_cast<unsigned>(mid));
            }
        }

        ImGui::TextDisabled("mesh cache: %u submeshes",
                             static_cast<u32>(bc.meshCache.size()));
        ImGui::TextDisabled("dirty: %s", bc.dirty ? "si" : "no");

        // Recompute mesh: util para debug si la mesh quedo stale por
        // un cambio que no marcamos dirty (no deberia pasar pero es
        // breadcrumb util cuando F2H12+ agreguen booleanos).
        if (ImGui::Button("Recompute mesh")) {
            bc.dirty = true;
        }

        ImGui::Separator();

        // --- F2H15 + F2H17: UV editor ---
        // F2H17: si Face Mode + activeFaceIndex >= 0, los widgets
        // editan SOLO esa cara. Sino, fallback al modo global F2H15
        // (todas las caras). El header cambia entre "UV (Brush)" y
        // "UV (Cara N)" para feedback visual.
        const bool faceMode = (m_ui != nullptr) &&
            (m_ui->subMode() == EditorSubMode::Face) &&
            (m_ui->selectionSet().activeFaceIndex >= 0) &&
            (static_cast<u32>(m_ui->selectionSet().activeFaceIndex)
                < bc.brush.faces.size());
        const i32 faceIdx = faceMode
            ? m_ui->selectionSet().activeFaceIndex : -1;

        if (faceMode) {
            ImGui::TextDisabled("UV (Cara %d)", faceIdx);
        } else {
            ImGui::TextDisabled("UV (Brush)");
        }

        if (!bc.brush.faces.empty()) {
            // En faceMode, el "representante" es la cara activa;
            // en object mode, sigue siendo la cara 0.
            auto& faceRef = faceMode ? bc.brush.faces[faceIdx]
                                       : bc.brush.faces[0];
            const std::string entityTag = e.hasComponent<TagComponent>()
                ? e.getComponent<TagComponent>().name : std::string{};

            // Helper: aplicar mutacion a TODAS las caras (object mode)
            // o solo a la cara activa (face mode).
            auto applyToScope = [&](auto&& fn) {
                if (faceMode) {
                    fn(bc.brush.faces[faceIdx]);
                } else {
                    for (auto& f : bc.brush.faces) fn(f);
                }
            };

            auto captureSnapshotIfActivated = [&]() {
                if (ImGui::IsItemActivated()) {
                    m_uvSnapshotPre = captureBrushUV(bc.brush);
                    m_uvSnapshotValid = true;
                    m_uvSnapshotEntityTag = entityTag;
                }
            };
            auto pushCommandIfChanged = [&](const char* label) {
                if (!ImGui::IsItemDeactivatedAfterEdit()) return;
                if (!m_uvSnapshotValid) return;
                if (m_ui == nullptr || m_ui->scene() == nullptr) return;
                const BrushUVSnapshot postSnap = captureBrushUV(bc.brush);
                if (snapshotsEqual(m_uvSnapshotPre, postSnap)) {
                    m_uvSnapshotValid = false;
                    return;
                }
                if (HistoryStack* h = m_ui->historyStack()) {
                    h->push(std::make_unique<EditBrushUVCommand>(
                        m_ui->scene(), m_uvSnapshotEntityTag,
                        std::move(m_uvSnapshotPre), std::move(postSnap),
                        std::string{label}));
                }
                m_uvSnapshotValid = false;
            };

            glm::vec2 uvScale = faceRef.uvScale;
            if (ImGui::DragFloat2("uv scale##uvbrush", &uvScale.x,
                                    0.05f, 0.01f, 100.0f)) {
                applyToScope([&](auto& f) { f.uvScale = uvScale; });
                bc.dirty = true;
                m_editedThisFrame = true;
            }
            captureSnapshotIfActivated();
            pushCommandIfChanged(faceMode ? "Editar UV scale (cara)"
                                            : "Editar UV scale");

            f32 uvRotDeg = glm::degrees(faceRef.uvRotation);
            if (ImGui::DragFloat("uv rotation (deg)##uvbrush",
                                    &uvRotDeg, 1.0f, -360.0f, 360.0f)) {
                const f32 uvRotRad = glm::radians(uvRotDeg);
                applyToScope([&](auto& f) { f.uvRotation = uvRotRad; });
                bc.dirty = true;
                m_editedThisFrame = true;
            }
            captureSnapshotIfActivated();
            pushCommandIfChanged(faceMode ? "Editar UV rotation (cara)"
                                            : "Editar UV rotation");

            glm::vec2 uvOffset = faceRef.uvOffset;
            if (ImGui::DragFloat2("uv offset##uvbrush", &uvOffset.x,
                                    0.05f)) {
                applyToScope([&](auto& f) { f.uvOffset = uvOffset; });
                bc.dirty = true;
                m_editedThisFrame = true;
            }
            captureSnapshotIfActivated();
            pushCommandIfChanged(faceMode ? "Editar UV offset (cara)"
                                            : "Editar UV offset");

            // Checkbox: instantaneo. Capturar pre + post al click + push.
            bool lockToWorld = faceRef.lockToWorld;
            BrushUVSnapshot lockPreSnap;
            const bool lockChanged = [&]() {
                if (ImGui::Checkbox("lock to world##uvbrush", &lockToWorld)) {
                    lockPreSnap = captureBrushUV(bc.brush);
                    applyToScope([&](auto& f) { f.lockToWorld = lockToWorld; });
                    bc.dirty = true;
                    // Recompute cache flag global (puede haber otras
                    // caras todavia con lock-to-world).
                    bc.anyFaceLockToWorld = false;
                    for (const auto& f : bc.brush.faces) {
                        if (f.lockToWorld) {
                            bc.anyFaceLockToWorld = true;
                            break;
                        }
                    }
                    m_editedThisFrame = true;
                    return true;
                }
                return false;
            }();
            if (lockChanged && m_ui != nullptr && m_ui->scene() != nullptr) {
                if (HistoryStack* h = m_ui->historyStack()) {
                    BrushUVSnapshot postSnap = captureBrushUV(bc.brush);
                    h->push(std::make_unique<EditBrushUVCommand>(
                        m_ui->scene(), entityTag,
                        std::move(lockPreSnap), std::move(postSnap),
                        faceMode ? std::string{"Toggle lock-to-world (cara)"}
                                 : std::string{"Toggle lock-to-world"}));
                }
            }

            // Conteo informativo de caras con lock-to-world (util
            // cuando F2H17 permita per-cara y haya mezcla).
            u32 lockedCount = 0;
            for (const auto& f : bc.brush.faces) {
                if (f.lockToWorld) ++lockedCount;
            }
            ImGui::TextDisabled("%u/%u caras con lock-to-world",
                                  lockedCount,
                                  static_cast<u32>(bc.brush.faces.size()));
        }

        ImGui::Separator();
    }

    ImGui::End();
}

} // namespace Mood
