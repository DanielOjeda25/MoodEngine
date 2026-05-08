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

    /// F2H28 Bloque G: snap step actual (en world units) para que el
    /// panel lo muestre como label "Grid: Nu" arriba-derecha. El state
    /// real vive en EditorApplication; el caller lo inyecta cada frame.
    void setSnapStep(u32 step) { m_snapStep = step; }

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

    /// F2H29 Bloque B: drag con LMB sobre el viewport. Una vez que el
    /// usuario supera el umbral 4 px desde el down, `active=true` y se
    /// mantiene hasta soltar (en ese frame `justEnded=true` y `active`
    /// pasa a false en el frame siguiente). El caller (EditorApplication)
    /// usa `active` para mover en vivo brushes seleccionados / dibujar
    /// preview del block tool, y `justEnded` como pulso para pushear
    /// el command al HistoryStack.
    struct DragState {
        bool  active    = false;
        bool  justEnded = false;
        float ndcStartX = 0.0f;
        float ndcStartY = 0.0f;
        float ndcCurX   = 0.0f;
        float ndcCurY   = 0.0f;
    };

    /// Lee el estado actual del drag SIN consumirlo (`active` sigue
    /// siendo true mientras el LMB sigue down post-umbral). El caller
    /// usa esto cada frame para mover en vivo.
    const DragState& dragState() const { return m_dragState; }

    /// Pulso de "drag termino este frame". Devuelve el state final
    /// (start + cur snapshot) y resetea `justEnded`. El caller pushea
    /// el command si recibe `r.justEnded == true`.
    DragState consumeDragEnded() {
        DragState r = m_dragState;
        m_dragState.justEnded = false;
        return r;
    }

private:
    OrthoCamera m_camera;
    IFramebuffer* m_framebuffer = nullptr;
    u32 m_desiredWidth = 0;
    u32 m_desiredHeight = 0;
    u32 m_snapStep = 16u;  // F2H28 Bloque G: solo display en label.

    bool m_panDragging = false;

    bool m_leftMouseDown = false;
    float m_leftDownX = 0.0f;
    float m_leftDownY = 0.0f;

    ClickSelect m_pendingClick{};
    DragState   m_dragState{};
};

} // namespace Mood
