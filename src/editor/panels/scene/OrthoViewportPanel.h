#pragma once

// F2H28: panel que muestra una vista ortografica del scene (Top / Front
// / Side) en el workspace "Editor de mapas". Patron: similar al
// ViewportPanel existente pero con camara ortografica + sin rotation
// input (solo pan MMB + zoom scroll). Click izquierdo registra un
// click-select que EditorApplication consume para hacer pick (Bloque F).
//
// El render del FBO lo hace EditorApplication invocando
// SceneRenderer::renderOrthoView(); este panel solo lo MUESTRA.

#include "core/Types.h"
#include "editor/panels/IPanel.h"
#include "editor/panels/scene/OrthoCamera.h"

namespace Mood {

class IFramebuffer;

class OrthoViewportPanel : public IPanel {
public:
    explicit OrthoViewportPanel(OrthoView view) {
        m_camera.view = view;
    }

    void onImGuiRender() override;
    const char* name() const override { return m_camera.label(); }

    /// FBO al que el SceneRenderer escribe esta vista cada frame.
    void setFramebuffer(IFramebuffer* fb) { m_framebuffer = fb; }

    /// Tamano deseado en pixeles. EditorApplication lo lee y resize
    /// el FBO al frame siguiente si difiere del actual.
    u32 desiredWidth() const { return m_desiredWidth; }
    u32 desiredHeight() const { return m_desiredHeight; }

    /// Camara editable (pan, zoom, view axes).
    OrthoCamera& camera() { return m_camera; }
    const OrthoCamera& camera() const { return m_camera; }

    /// Click izquierdo sobre la imagen del viewport — Bloque F lo
    /// consume para hacer pick y setear seleccion. Igual umbral 4 px
    /// que el ViewportPanel para distinguir click puro de drag.
    struct ClickSelect {
        bool pending = false;
        float ndcX = 0.0f;
        float ndcY = 0.0f;
    };

    ClickSelect consumeClickSelect() {
        ClickSelect r = m_pendingClick;
        m_pendingClick = ClickSelect{};
        return r;
    }

private:
    OrthoCamera m_camera;
    IFramebuffer* m_framebuffer = nullptr;
    u32 m_desiredWidth = 0;
    u32 m_desiredHeight = 0;

    bool m_panDragging = false;

    bool m_leftMouseDown = false;
    float m_leftDownX = 0.0f;
    float m_leftDownY = 0.0f;

    ClickSelect m_pendingClick{};
};

} // namespace Mood
