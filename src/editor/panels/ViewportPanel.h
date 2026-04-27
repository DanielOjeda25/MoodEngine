#pragma once

#include "core/Types.h"
#include "editor/panels/IPanel.h"

#include <functional>

struct ImDrawList;

namespace Mood {

class IFramebuffer;

class ViewportPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Viewport"; }

    /// @brief Asocia el framebuffer cuya color attachment se muestra en el panel.
    ///        La escena se renderiza a este FB desde fuera (EditorApplication).
    void setFramebuffer(IFramebuffer* fb) { m_framebuffer = fb; }

    /// @brief Tamano deseado por el panel en pixeles. Se actualiza al renderizar
    ///        y lo lee el runner del frame siguiente para redimensionar el FB.
    u32 desiredWidth() const { return m_desiredWidth; }
    u32 desiredHeight() const { return m_desiredHeight; }

    /// @brief Delta en pixeles del right-drag del mouse sobre el panel (rotacion).
    ///        Cero si no hay drag activo. Se resetea cada frame al renderizar.
    float cameraRotateDx() const { return m_cameraRotateDx; }
    float cameraRotateDy() const { return m_cameraRotateDy; }

    /// @brief Delta en pixeles del middle-drag del mouse sobre el panel (paneo).
    ///        Estilo Blender. Cero si no hay drag activo.
    float cameraPanDx() const { return m_cameraPanDx; }
    float cameraPanDy() const { return m_cameraPanDy; }

    /// @brief Delta de la rueda del mouse sobre el panel (zoom). Cero si no aplica.
    float cameraWheel() const { return m_cameraWheel; }

    /// @brief `true` si el cursor esta sobre la imagen del viewport en el
    ///        frame actual. Requisito para usar `mouseNdcX/Y`.
    bool imageHovered() const { return m_imageHovered; }

    /// @brief Tipo del drag-and-drop activo en este frame, si lo hay.
    ///        El editor usa esta info para decidir QUE highlight dibujar:
    ///        - Texture/Mesh/Prefab apuntan a UN TILE -> cubo cyan sobre
    ///          el tile bajo el cursor.
    ///        - Material apunta a UNA ENTIDAD -> outline OBB sobre el
    ///          mesh bajo el cursor.
    ///        `None` significa que no hay drag activo (no se dibuja nada).
    enum class AssetDragKind { None, Texture, Mesh, Prefab, Material };
    AssetDragKind assetDragKind() const { return m_assetDragKind; }

    /// @brief Helper booleano para callsites que solo necesitan saber si
    ///        hay algun drag activo (sin importar el tipo).
    bool assetDragActive() const {
        return m_assetDragKind != AssetDragKind::None;
    }

    /// @brief Coordenadas NDC del cursor dentro del area de la imagen,
    ///        rango [-1, 1], Y+ arriba (convencion OpenGL). Sin sentido si
    ///        `imageHovered()` es false. Usar con `pickTile()`.
    float mouseNdcX() const { return m_mouseNdcX; }
    float mouseNdcY() const { return m_mouseNdcY; }

    /// @brief Drop de una textura sobre el viewport (desde AssetBrowser).
    ///        El panel solo captura el evento; el receiver (EditorApplication)
    ///        computa el tile con `pickTile(ndcX, ndcY)` y aplica el cambio.
    struct TextureDrop {
        bool pending = false;
        float ndcX = 0.0f;
        float ndcY = 0.0f;
        u32 textureId = 0; // castear a TextureAssetId en el consumidor
    };

    /// @brief Consume el ultimo drop. Devuelve `pending=false` si no hay.
    TextureDrop consumeTextureDrop() {
        TextureDrop r = m_pendingDrop;
        m_pendingDrop = TextureDrop{};
        return r;
    }

    /// @brief Drop de un mesh sobre el viewport (Hito 10 Bloque 5). El
    ///        consumidor usa el tile picking para poner la nueva entidad en
    ///        el centro del tile bajo el cursor. Si el pick falla (cursor
    ///        fuera del mapa), el spawn se descarta.
    struct MeshDrop {
        bool pending = false;
        float ndcX = 0.0f;
        float ndcY = 0.0f;
        u32 meshId = 0; // castear a MeshAssetId en el consumidor
    };

    /// @brief Consume el ultimo mesh drop. Devuelve `pending=false` si no hay.
    MeshDrop consumeMeshDrop() {
        MeshDrop r = m_pendingMeshDrop;
        m_pendingMeshDrop = MeshDrop{};
        return r;
    }

    /// @brief Drop de un prefab sobre el viewport (Hito 14 Bloque 5).
    ///        Mismo flow que MeshDrop: el consumidor hace pick y spawnea
    ///        una instancia del prefab en el world-point.
    struct PrefabDrop {
        bool pending = false;
        float ndcX = 0.0f;
        float ndcY = 0.0f;
        u32 prefabId = 0; // castear a PrefabAssetId en el consumidor
    };

    PrefabDrop consumePrefabDrop() {
        PrefabDrop r = m_pendingPrefabDrop;
        m_pendingPrefabDrop = PrefabDrop{};
        return r;
    }

    /// @brief Drop de un material sobre el viewport (Hito 17 Bloque 4).
    ///        Reasigna el primer slot del MeshRenderer de la entidad bajo
    ///        el cursor.
    struct MaterialDrop {
        bool pending = false;
        float ndcX = 0.0f;
        float ndcY = 0.0f;
        u32 materialId = 0; // castear a MaterialAssetId en el consumidor
    };

    MaterialDrop consumeMaterialDrop() {
        MaterialDrop r = m_pendingMaterialDrop;
        m_pendingMaterialDrop = MaterialDrop{};
        return r;
    }

    /// @brief Click izquierdo sobre la imagen del viewport (Hito 13).
    ///        Distingue click puro de drag: dispara solo si el mouse bajó
    ///        y subió sin desplazarse más de 4 pixeles.
    struct ClickSelect {
        bool pending = false;
        float ndcX = 0.0f;
        float ndcY = 0.0f;
    };

    /// @brief Consume el ultimo click-select. `EditorApplication` hace el
    ///        raycast y setea la seleccion.
    ClickSelect consumeClickSelect() {
        ClickSelect r = m_pendingClick;
        m_pendingClick = ClickSelect{};
        return r;
    }

    /// @brief Callback invocado DESPUES de dibujar la imagen del framebuffer,
    ///        usando un DrawList con clip al area de la imagen. Recibe el
    ///        origen (top-left) y tamano del rect. Uso: overlays 2D como
    ///        iconos de Light/Audio y gizmos de seleccion (Hito 13).
    using OverlayDraw = std::function<void(ImDrawList* drawList,
                                            float imageMinX, float imageMinY,
                                            float imageSizeX, float imageSizeY)>;
    void setOverlayDraw(OverlayDraw cb) { m_overlayDraw = std::move(cb); }

private:
    IFramebuffer* m_framebuffer = nullptr;
    u32 m_desiredWidth = 0;
    u32 m_desiredHeight = 0;

    float m_cameraRotateDx = 0.0f;
    float m_cameraRotateDy = 0.0f;
    float m_cameraPanDx = 0.0f;
    float m_cameraPanDy = 0.0f;
    float m_cameraWheel = 0.0f;
    bool m_rightDragging = false;
    bool m_middleDragging = false;

    bool m_imageHovered = false;
    float m_mouseNdcX = 0.0f;
    float m_mouseNdcY = 0.0f;
    AssetDragKind m_assetDragKind = AssetDragKind::None; // refrescado cada frame

    TextureDrop m_pendingDrop{};
    MeshDrop m_pendingMeshDrop{};
    PrefabDrop m_pendingPrefabDrop{};
    MaterialDrop m_pendingMaterialDrop{};
    ClickSelect m_pendingClick{};
    OverlayDraw m_overlayDraw{};

    // Para diferenciar click vs drag del left-button.
    bool m_leftMouseDown = false;
    float m_leftDownX = 0.0f;
    float m_leftDownY = 0.0f;
};

} // namespace Mood
