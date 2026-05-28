#include "editor/ui/ToastsOverlay.h"

#include "core/Toasts.h"
#include "core/UserSettings.h"
#include "editor/ui/IconsFontAwesome6.h"

#include <imgui.h>

#include <algorithm>

namespace Mood {

namespace {

// F3H24: paleta de colores por severidad. Tonos saturados pero con
// alpha alto suficiente para legibilidad sobre fondo oscuro del editor.
ImU32 bgColorFor(Toasts::Severity sev) {
    switch (sev) {
        case Toasts::Severity::Info:    return IM_COL32(50, 95, 150, 230);   // azul
        case Toasts::Severity::Success: return IM_COL32(50, 130, 70, 230);   // verde
        case Toasts::Severity::Warn:    return IM_COL32(180, 130, 30, 230);  // ambar
        case Toasts::Severity::Error:   return IM_COL32(170, 50, 50, 230);   // rojo
    }
    return IM_COL32(60, 60, 60, 230);
}

const char* iconFor(Toasts::Severity sev) {
    switch (sev) {
        case Toasts::Severity::Info:    return ICON_FA_CIRCLE_INFO;
        case Toasts::Severity::Success: return ICON_FA_CIRCLE_CHECK;
        case Toasts::Severity::Warn:    return ICON_FA_TRIANGLE_EXCLAMATION;
        case Toasts::Severity::Error:   return ICON_FA_CIRCLE_XMARK;
    }
    return ICON_FA_CIRCLE_INFO;
}

// F3H24: alpha del toast en función de su vida restante. Slide-in en
// los primeros 200 ms (0→1 lineal) + visible 100% mientras vida >
// 400ms + fade-out lineal en los últimos 400 ms.
float alphaFor(const Toasts::Toast& t) {
    constexpr float kSlideMs = 200.0f;
    constexpr float kFadeMs  = 400.0f;
    const float age = t.totalMs - t.remainingMs;
    if (age < kSlideMs) {
        return age / kSlideMs;  // slide-in
    }
    if (t.remainingMs < kFadeMs) {
        return std::max(0.0f, t.remainingMs / kFadeMs);  // fade-out
    }
    return 1.0f;
}

// F3H24: offset horizontal del slide-in. Empieza fuera de pantalla
// (offset > 0 = hacia la derecha, oculto). En los primeros 200 ms
// interpola de `+chipWidth` a 0.
float slideOffsetX(const Toasts::Toast& t, float chipWidth) {
    constexpr float kSlideMs = 200.0f;
    const float age = t.totalMs - t.remainingMs;
    if (age >= kSlideMs) return 0.0f;
    const float k = age / kSlideMs;
    // Easing easeOutCubic: 1 - (1-k)^3 — slide-in que desacelera.
    const float eased = 1.0f - (1.0f - k) * (1.0f - k) * (1.0f - k);
    return (1.0f - eased) * chipWidth;
}

} // namespace

void ToastsOverlay::draw() {
    if (!UserSettings::editor().toastsEnabled) return;

    const auto toasts = Toasts::snapshot();
    if (toasts.empty()) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    constexpr float kPadding   = 20.0f;
    constexpr float kChipWidth = 320.0f;  // ancho fijo para alineación
    constexpr float kGapY      = 8.0f;    // espacio entre chips

    // F3H24: stack vertical desde la esquina inferior-derecha hacia
    // arriba. El más nuevo (último en el vector) queda abajo; el más
    // viejo arriba — para que el dev vea la últimas notificaciones
    // sin que sean tapadas.
    float cursorY = vp->WorkPos.y + vp->WorkSize.y - kPadding;

    // Iteramos de atrás hacia adelante porque cada chip se ancla a
    // `cursorY` y luego cursorY sube por su altura.
    for (auto it = toasts.rbegin(); it != toasts.rend(); ++it) {
        const auto& t = *it;
        const float alpha = alphaFor(t);
        if (alpha <= 0.0f) continue;
        const float slideX = slideOffsetX(t, kChipWidth);

        // Pre-calcular altura: 1 línea de header + N líneas del wrap.
        // Sin wrap manual, ImGui calcula con `CalcTextSize` con max
        // width = chipWidth - paddings. Usamos AlwaysAutoResize para
        // que el toast se ajuste al contenido.
        constexpr ImGuiWindowFlags kFlags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;

        const float posX = vp->WorkPos.x + vp->WorkSize.x - kPadding
                            - kChipWidth + slideX;

        // Pivot (0.0, 1.0) ancla la esquina inferior-izquierda del chip
        // a (posX, cursorY) — apila hacia arriba.
        ImGui::SetNextWindowPos(ImVec2(posX, cursorY),
                                  ImGuiCond_Always, ImVec2(0.0f, 1.0f));
        ImGui::SetNextWindowSize(ImVec2(kChipWidth, 0.0f),
                                   ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(alpha);

        // F3H26: ID basado en Toast::id (monótono, persistente entre
        // frames). El bug pre-F3H26 usaba `&t` que apunta al vector
        // temporal de snapshot() — cada frame ImGui veía un ID distinto,
        // recreaba la ventana sin cache del size del frame previo, y
        // AlwaysAutoResize necesitaba 2 frames para estabilizar → flicker.
        char id[64];
        std::snprintf(id, sizeof(id), "##toast_%llu",
                      static_cast<unsigned long long>(t.id));

        const ImU32 bg = bgColorFor(t.severity);
        // Aplicar alpha al bg color
        const u32 bgA = static_cast<u32>(((bg >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
        const ImU32 bgWithAlpha =
            (bg & ~(0xFFu << IM_COL32_A_SHIFT)) |
            (bgA << IM_COL32_A_SHIFT);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, bgWithAlpha);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

        if (ImGui::Begin(id, nullptr, kFlags)) {
            // Icon + mensaje wrapped a chipWidth - padding.
            ImGui::TextUnformatted(iconFor(t.severity));
            ImGui::SameLine();
            ImGui::PushTextWrapPos(0.0f);  // wrap al borde del window
            ImGui::TextUnformatted(t.message.c_str());
            ImGui::PopTextWrapPos();

            // Mover cursorY hacia arriba por la altura del chip + gap.
            const ImVec2 size = ImGui::GetWindowSize();
            cursorY -= (size.y + kGapY);
        }
        ImGui::End();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
    }
}

} // namespace Mood
