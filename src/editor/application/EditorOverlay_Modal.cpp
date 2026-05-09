// F2H30 Bloque D: implementacion del modal G/R/S estilo Blender
// (cursor-driven Grab/Rotate/Scale sin handles del gizmo). Conviene
// como archivo separado por el cap LOC en EditorOverlay.cpp y para
// aislar la logica del cursor + axis lock. La activacion (tecla G/R/S
// pulsada con conditions OK) vive en EditorOverlay.cpp; este archivo
// implementa start/update/confirm/cancel.

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "editor/commands/EditTransformCommand.h"
#include "editor/commands/MultiEditTransformCommand.h"
#include "editor/selection/SelectionSet.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace Mood {

namespace {

glm::vec3 readField(const TransformComponent& tf, int field) {
    switch (field) {
        case 0: return tf.position;
        case 1: return tf.rotationEuler;
        case 2: return tf.scale;
    }
    return glm::vec3(0.0f);
}

void writeField(TransformComponent& tf, int field, const glm::vec3& v) {
    switch (field) {
        case 0: tf.position = v; break;
        case 1: tf.rotationEuler = v; break;
        case 2:
            tf.scale = glm::vec3(
                std::clamp(v.x, 0.01f, 100.0f),
                std::clamp(v.y, 0.01f, 100.0f),
                std::clamp(v.z, 0.01f, 100.0f));
            break;
    }
}

/// @brief Proyecta worldCenter a screen, suma (mouseDx, mouseDy) px,
///        y unproject de vuelta al mundo a la MISMA profundidad. Da
///        un delta world 1:1 con el movimiento visible del cursor.
///        Funciona para ortho y perspective (la perspective preserva
///        la profundidad del worldCenter).
glm::vec3 screenDeltaToWorld(const glm::mat4& vp, float vx0, float vy0,
                              float vw, float vh,
                              const glm::vec3& worldCenter,
                              float mouseDx, float mouseDy) {
    if (vw <= 0.0f || vh <= 0.0f) return glm::vec3(0.0f);
    const glm::vec4 clip = vp * glm::vec4(worldCenter, 1.0f);
    if (std::abs(clip.w) < 1e-6f) return glm::vec3(0.0f);
    const float ndcStartX = clip.x / clip.w;
    const float ndcStartY = clip.y / clip.w;
    const float ndcZ      = clip.z / clip.w;
    // Pixels -> NDC. Y va invertida (screen Y crece hacia abajo).
    const float ndcDx =  (mouseDx / vw) * 2.0f;
    const float ndcDy = -(mouseDy / vh) * 2.0f;
    const glm::mat4 invVP = glm::inverse(vp);
    auto unproject = [&](float nx, float ny) -> glm::vec3 {
        glm::vec4 w = invVP * glm::vec4(nx, ny, ndcZ, 1.0f);
        if (std::abs(w.w) < 1e-6f) return glm::vec3(0.0f);
        return glm::vec3(w) / w.w;
    };
    return unproject(ndcStartX + ndcDx, ndcStartY + ndcDy)
         - unproject(ndcStartX,         ndcStartY);
}

} // anonymous

void EditorApplication::startModalShortcut(int field) {
    if (!m_scene) return;
    const SelectionSet& set = m_ui.selectionSet();
    if (set.selected.empty()) {
        Log::editor()->info("[modal] no hay seleccion — modal cancelado");
        return;
    }
    if (m_modalShortcut.active) {
        cancelModalShortcut();
    }

    m_modalShortcut.entries.clear();
    m_modalShortcut.entries.reserve(set.selected.size());
    glm::vec3 centroid(0.0f);
    int count = 0;
    for (const Entity& e : set.selected) {
        if (!m_scene->registry().valid(e.handle())) continue;
        if (!e.hasComponent<TransformComponent>()) continue;
        Entity copy = e;
        const auto& tf = copy.getComponent<TransformComponent>();
        m_modalShortcut.entries.push_back({copy, readField(tf, field)});
        centroid += tf.position;
        ++count;
    }
    if (m_modalShortcut.entries.empty()) {
        Log::editor()->info("[modal] seleccion sin transform — modal cancelado");
        return;
    }
    centroid /= static_cast<f32>(count);

    const ImVec2 mp = ImGui::GetMousePos();
    m_modalShortcut.active = true;
    m_modalShortcut.field = field;
    m_modalShortcut.axisLock = -1;
    m_modalShortcut.mouseStart = glm::vec2(mp.x, mp.y);
    m_modalShortcut.worldCenter = centroid;

    const char* lbl = "G/Grab";
    if (field == 1) lbl = "R/Rotate";
    else if (field == 2) lbl = "S/Scale";
    Log::editor()->info("[modal] {} START — {} entidades, click confirma, "
                         "Esc cancela, X/Y/Z lockea axis",
                         lbl, m_modalShortcut.entries.size());
}

void EditorApplication::updateModalShortcut(const glm::mat4& vp,
                                              float vx0, float vy0,
                                              float vw, float vh) {
    if (!m_modalShortcut.active) return;
    if (!m_scene) {
        cancelModalShortcut();
        return;
    }

    // Axis lock: X/Y/Z toggle (re-apretar el mismo libera).
    auto handleAxisKey = [&](ImGuiKey k, int axis) {
        if (ImGui::IsKeyPressed(k, false)) {
            m_modalShortcut.axisLock =
                (m_modalShortcut.axisLock == axis) ? -1 : axis;
            const char* axisLbl = (axis == 0) ? "X" : (axis == 1) ? "Y" : "Z";
            if (m_modalShortcut.axisLock < 0) {
                Log::editor()->info("[modal] axis lock liberado");
            } else {
                Log::editor()->info("[modal] axis lock = {}", axisLbl);
            }
        }
    };
    handleAxisKey(ImGuiKey_X, 0);
    handleAxisKey(ImGuiKey_Y, 1);
    handleAxisKey(ImGuiKey_Z, 2);

    // Esc cancela (revert).
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        cancelModalShortcut();
        return;
    }
    // Click izq confirma (push command).
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        confirmModalShortcut();
        return;
    }

    // Delta del cursor en pixeles desde mouseStart.
    const ImVec2 mp = ImGui::GetMousePos();
    const f32 mouseDx = mp.x - m_modalShortcut.mouseStart.x;
    const f32 mouseDy = mp.y - m_modalShortcut.mouseStart.y;

    if (m_modalShortcut.field == 0) {
        // ---- G: Grab — translate por delta world en plano de la camara.
        glm::vec3 deltaWorld = screenDeltaToWorld(
            vp, vx0, vy0, vw, vh,
            m_modalShortcut.worldCenter, mouseDx, mouseDy);
        // Axis lock: zero los otros 2 ejes.
        if (m_modalShortcut.axisLock >= 0) {
            for (int i = 0; i < 3; ++i) {
                if (i != m_modalShortcut.axisLock) deltaWorld[i] = 0.0f;
            }
        }
        // Snap al grid de hammer si esta activo.
        const f32 snap = static_cast<f32>(m_hammerSnapStep);
        if (snap > 0.0f) {
            for (int i = 0; i < 3; ++i) {
                if (std::abs(deltaWorld[i]) > 1e-4f) {
                    deltaWorld[i] = std::round(deltaWorld[i] / snap) * snap;
                }
            }
        }
        for (auto& en : m_modalShortcut.entries) {
            if (!m_scene->registry().valid(en.entity.handle())) continue;
            if (!en.entity.hasComponent<TransformComponent>()) continue;
            auto& tf = en.entity.getComponent<TransformComponent>();
            writeField(tf, 0, en.startValue + deltaWorld);
        }
    } else if (m_modalShortcut.field == 1) {
        // ---- R: Rotate — angulo del cursor desde el centro proyectado.
        const glm::vec4 clip =
            vp * glm::vec4(m_modalShortcut.worldCenter, 1.0f);
        if (std::abs(clip.w) < 1e-6f) return;
        const f32 ndcCenterX = clip.x / clip.w;
        const f32 ndcCenterY = clip.y / clip.w;
        // ndc -> screen.
        const f32 cx = vx0 + (ndcCenterX * 0.5f + 0.5f) * vw;
        const f32 cy = vy0 + (1.0f - (ndcCenterY * 0.5f + 0.5f)) * vh;
        const f32 startAng = std::atan2(
            m_modalShortcut.mouseStart.y - cy,
            m_modalShortcut.mouseStart.x - cx);
        const f32 curAng = std::atan2(mp.y - cy, mp.x - cx);
        f32 dAngDeg = (curAng - startAng) * (180.0f / 3.1415926f);
        // Eje rotacion: axisLock determina (0=X/1=Y/2=Z); default = Y.
        int rotAxis = (m_modalShortcut.axisLock >= 0)
                       ? m_modalShortcut.axisLock : 1;
        for (auto& en : m_modalShortcut.entries) {
            if (!m_scene->registry().valid(en.entity.handle())) continue;
            if (!en.entity.hasComponent<TransformComponent>()) continue;
            auto& tf = en.entity.getComponent<TransformComponent>();
            glm::vec3 newRot = en.startValue;
            newRot[rotAxis] += dAngDeg;
            writeField(tf, 1, newRot);
        }
    } else if (m_modalShortcut.field == 2) {
        // ---- S: Scale — ratio de distancias al centro proyectado.
        const glm::vec4 clip =
            vp * glm::vec4(m_modalShortcut.worldCenter, 1.0f);
        if (std::abs(clip.w) < 1e-6f) return;
        const f32 ndcCenterX = clip.x / clip.w;
        const f32 ndcCenterY = clip.y / clip.w;
        const f32 cx = vx0 + (ndcCenterX * 0.5f + 0.5f) * vw;
        const f32 cy = vy0 + (1.0f - (ndcCenterY * 0.5f + 0.5f)) * vh;
        const f32 startDist = std::hypot(
            m_modalShortcut.mouseStart.x - cx,
            m_modalShortcut.mouseStart.y - cy);
        const f32 curDist = std::hypot(mp.x - cx, mp.y - cy);
        if (startDist < 1e-3f) return;
        const f32 ratio = curDist / startDist;
        for (auto& en : m_modalShortcut.entries) {
            if (!m_scene->registry().valid(en.entity.handle())) continue;
            if (!en.entity.hasComponent<TransformComponent>()) continue;
            auto& tf = en.entity.getComponent<TransformComponent>();
            glm::vec3 newScale = en.startValue;
            if (m_modalShortcut.axisLock >= 0) {
                newScale[m_modalShortcut.axisLock] *= ratio;
            } else {
                newScale *= ratio;
            }
            writeField(tf, 2, newScale);
        }
    }
}

void EditorApplication::confirmModalShortcut() {
    if (!m_modalShortcut.active) return;
    if (!m_scene) {
        cancelModalShortcut();
        return;
    }

    const auto field =
        static_cast<EditTransformCommand::Field>(m_modalShortcut.field);

    // Construir entries del MultiEditTransformCommand: leer el valor
    // CURRENT como `after`, usar el `startValue` cacheado como `before`.
    std::vector<MultiEditTransformCommand::Entry> cmdEntries;
    cmdEntries.reserve(m_modalShortcut.entries.size());
    for (auto& en : m_modalShortcut.entries) {
        if (!m_scene->registry().valid(en.entity.handle())) continue;
        if (!en.entity.hasComponent<TransformComponent>()) continue;
        const auto& tf = en.entity.getComponent<TransformComponent>();
        const glm::vec3 after = readField(tf, m_modalShortcut.field);
        cmdEntries.push_back({en.entity, en.startValue, after});
    }

    auto cmd = std::make_unique<MultiEditTransformCommand>(
        field, std::move(cmdEntries));
    if (cmd->isNoOp()) {
        // Nada cambio — solo limpiar state.
        Log::editor()->info("[modal] confirm: no-op (cursor sin movimiento)");
        m_modalShortcut.active = false;
        m_modalShortcut.entries.clear();
        return;
    }

    // Mismo patron que finalizeGizmoDrag: revertir al before antes del
    // push (execute() del command los re-aplica).
    for (auto& en : m_modalShortcut.entries) {
        if (!m_scene->registry().valid(en.entity.handle())) continue;
        if (!en.entity.hasComponent<TransformComponent>()) continue;
        auto& tf = en.entity.getComponent<TransformComponent>();
        writeField(tf, m_modalShortcut.field, en.startValue);
    }

    Log::editor()->info("[modal] CONFIRM: push MultiEditTransformCommand "
                         "({} entidades, field={})",
                         m_modalShortcut.entries.size(),
                         m_modalShortcut.field);
    m_history.push(std::move(cmd));
    m_modalShortcut.active = false;
    m_modalShortcut.entries.clear();
}

void EditorApplication::cancelModalShortcut() {
    if (!m_modalShortcut.active) return;
    // Revert cada entidad a su startValue.
    if (m_scene) {
        for (auto& en : m_modalShortcut.entries) {
            if (!m_scene->registry().valid(en.entity.handle())) continue;
            if (!en.entity.hasComponent<TransformComponent>()) continue;
            auto& tf = en.entity.getComponent<TransformComponent>();
            writeField(tf, m_modalShortcut.field, en.startValue);
        }
    }
    Log::editor()->info("[modal] CANCEL ({} entidades restauradas)",
                         m_modalShortcut.entries.size());
    m_modalShortcut.active = false;
    m_modalShortcut.entries.clear();
}

} // namespace Mood
