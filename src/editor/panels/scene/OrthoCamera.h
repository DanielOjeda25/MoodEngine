#pragma once

// F2H28: camara ortografica para los 3 viewports del workspace
// "Editor de mapas" (Top / Front / Side). Y-up — mismo eje que el
// resto del engine. Cada vista define el plano world que se proyecta
// a pantalla:
//   Top   (XZ): cam mira -Y. ScreenX=worldX, ScreenY=-worldZ.
//   Front (XY): cam mira -Z. ScreenX=worldX, ScreenY=worldY.
//   Side  (ZY): cam mira -X. ScreenX=worldZ, ScreenY=worldY.
//
// State editable por el user:
//   panOffset: posicion del centro de la vista en world units sobre
//              el plano. Mover con MMB drag.
//   worldHeight: altura del frustum ortografico en world units. Zoom
//                con scroll wheel.
//
// La camara NO almacena view/proj cacheadas — se computan on-demand.
// Tradeoff: 2 mat4 por frame por viewport (3 viewports = 6 mat4) es
// trivial vs. el costo de invalidacion al cambiar pan/zoom.

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace Mood {

enum class OrthoView {
    TopXZ,
    FrontXY,
    SideZY,
};

struct OrthoCamera {
    OrthoView view = OrthoView::TopXZ;
    glm::vec2 panOffset{0.0f};
    float worldHeight = 32.0f;  // ~32 unidades verticales visibles al inicio.

    static constexpr float k_minHeight = 1.0f;
    static constexpr float k_maxHeight = 8192.0f;

    /// Eje "right" del view en world space (X+ pantalla -> world).
    glm::vec3 rightAxis() const {
        switch (view) {
            case OrthoView::TopXZ:   return {1.0f, 0.0f, 0.0f};
            case OrthoView::FrontXY: return {1.0f, 0.0f, 0.0f};
            case OrthoView::SideZY:  return {0.0f, 0.0f, 1.0f};
        }
        return {1.0f, 0.0f, 0.0f};
    }

    /// Eje "up" del view en world space (Y+ pantalla -> world).
    /// Para TopXZ devuelvo -Z para que +Z apunte hacia abajo en pantalla
    /// (convencion mapas: norte/+Z arriba en la mayoria de editores
    /// estilo Hammer).
    glm::vec3 upAxis() const {
        switch (view) {
            case OrthoView::TopXZ:   return {0.0f, 0.0f, -1.0f};
            case OrthoView::FrontXY: return {0.0f, 1.0f, 0.0f};
            case OrthoView::SideZY:  return {0.0f, 1.0f, 0.0f};
        }
        return {0.0f, 1.0f, 0.0f};
    }

    /// Eje "forward" — apunta INTO la pantalla.
    glm::vec3 forwardAxis() const {
        switch (view) {
            case OrthoView::TopXZ:   return {0.0f, -1.0f,  0.0f};
            case OrthoView::FrontXY: return {0.0f,  0.0f, -1.0f};
            case OrthoView::SideZY:  return {-1.0f, 0.0f,  0.0f};
        }
        return {0.0f, 0.0f, -1.0f};
    }

    /// Centro de la vista en world space (panOffset proyectado al plano).
    glm::vec3 worldCenter() const {
        return rightAxis() * panOffset.x + upAxis() * panOffset.y;
    }

    /// Posicion del eye = centro - forward * dist.
    glm::vec3 eyePos(float dist = 1024.0f) const {
        return worldCenter() - forwardAxis() * dist;
    }

    glm::mat4 viewMatrix() const {
        const glm::vec3 eye = eyePos();
        return glm::lookAt(eye, eye + forwardAxis(), upAxis());
    }

    glm::mat4 projMatrix(float aspect) const {
        const float halfH = worldHeight * 0.5f;
        const float halfW = halfH * aspect;
        return glm::ortho(-halfW, halfW, -halfH, halfH, 0.1f, 4096.0f);
    }

    /// NDC (-1..1, Y up) -> world point sobre el plano de la vista.
    glm::vec3 worldFromNdc(float ndcX, float ndcY, float aspect) const {
        const float halfH = worldHeight * 0.5f;
        const float halfW = halfH * aspect;
        return worldCenter()
             + rightAxis() * (ndcX * halfW)
             + upAxis()    * (ndcY * halfH);
    }

    void zoom(float wheelDelta) {
        const float factor = std::pow(1.1f, -wheelDelta);
        worldHeight = std::clamp(worldHeight * factor, k_minHeight, k_maxHeight);
    }

    /// Etiqueta visible en la pestana del panel.
    const char* label() const {
        switch (view) {
            case OrthoView::TopXZ:   return "Top (XZ)";
            case OrthoView::FrontXY: return "Front (XY)";
            case OrthoView::SideZY:  return "Side (ZY)";
        }
        return "?";
    }
};

} // namespace Mood
