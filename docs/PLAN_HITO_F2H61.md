# PLAN HITO F2H61 — SSR (Screen-Space Reflections)

> **Estado**: STUB pendiente arranque (2026-05-17).
> **Tag previsto**: `v1.48.0-fase2-hito61`.
> **Origen**: F2H20 del plan original Fase 2. Cierre F2H60 dejó pendientes 16 hitos; F2H61 retoma `PLAN_FASE2.md` siguiendo la Sub-fase 2.3 (Renderer): F2H17 ✅ material editor, F2H19/F2H56 ✅ SSAO, F2H21/F2H55 ✅ bloom, F2H22/F2H60 ✅ CSM, **F2H20/F2H61 → SSR**, F2H18/F2H62 → shader graph runtime. Después de F2H61 + F2H62, Sub-fase 2.3 quedaría completa.

---

## 🎮 Mecánicas (lo que vivís como dev)

### Qué es SSR

Reflejos calculados a partir del **G-buffer screen-space** (color + normal + depth). Para cada fragmento metálico o de agua, el shader hace ray marching en screen-space desde la posición del fragmento siguiendo la dirección de reflexión hasta encontrar un hit con la geometría visible — y muestrea el color de ese hit como reflexión.

Trade-off vs cubemap reflections (ya implementadas vía IBL `m_iblPrefilter`):
- **SSR pros**: refleja la geometría *real* de la escena actual, en tiempo real (espejos, charcos, suelos pulidos, agua). Cubemap solo refleja el environment estático.
- **SSR contras**: solo refleja lo que está **en pantalla** (objetos fuera del frustum o detrás de cámara no se reflejan); puede generar artifacts en bordes; más caro que cubemap. Convención: SSR como **enhancement** del cubemap, no reemplazo (fallback al cubemap cuando ray marching falla).

### Cómo se aplicaría en MoodEngine

Inspector → Environment → nueva sección **SSR** (después de CSM):
- `enabled` (bool, default OFF — pedido del dev en F2H60: efectos opt-in).
- `maxSteps` (int 16-128) — pasos del ray marching.
- `thickness` (float 0.01-1.0) — tolerancia para considerar hit válido.
- `intensity` (float 0-1) — blend con el reflejo cubemap.

Sin override per-material en v1; SSR aplica a todas las superficies con `metallic > 0.5` o materiales con flag `enableSSR`. Settings globales por escena, mismo patrón que bloom/SSAO/CSM.

---

## 🧱 Bloques de ejecución (DRAFT — refinar al arrancar)

### Bloque A — G-buffer extension
- Agregar depth + normal RT al `m_sceneFb` (o crear `m_gbufferFb` separado con `viewSpace normal` + depth).
- Verificar que el depth pre-pass actual ya escribe el formato correcto (24-bit + 8-bit stencil quizá hay que reformatear).

### Bloque B — Shader SSR
- `shaders/ssr.frag` con ray marching en screen-space:
  1. Reconstruir worldPos desde depth + projection inverse.
  2. Reflejar `V` (view dir) sobre `N` (normal) → reflect ray.
  3. Marchar en screen-space N pasos, comparando depth de cada step contra depth buffer.
  4. Si hit (gap < thickness), muestrear color del frame previo (HDR FB) en ese pixel.
  5. Si no hit, fallback al cubemap IBL (mantener calidad).

### Bloque C — `SSRPass.{h,cpp}`
- Patrón establecido: `apply(srcColor, srcDepth, srcNormal, dst)` con save/restore GL state.
- FB intermedio `m_ssrFb` para el output (HDR para combinar con bloom luego).

### Bloque D — wireup en `SceneRenderer::endFrame`
- Pase SSR después del scene PBR + CSM, antes del bloom (el reflejo va en HDR).
- `EnvironmentComponent::ssrEnabled` gate.
- `applyEnvironmentFromScene` lee SSR params.

### Bloque E — Inspector + i18n
- Sección "SSR" en `InspectorPanel_Environment.cpp` con sliders + reset button.
- i18n keys `editor.panel.inspector.environment.ssr*`.

### Bloque F — Tests + cierre
- Round-trip de `SavedEnvironment` con SSR params.
- Validación visual: spawn esfera con material metallic + plane piso → verificar reflejo en piso.

---

## 🔬 Validación visual

- **Test 1**: Escena vacía + suelo metálico + caja roja arriba → la caja debe reflejarse en el suelo.
- **Test 2**: Mover la caja fuera del frustum → reflejo debe degradarse a cubemap (no quedarse "congelado").
- **Test 3**: Desactivar SSR → reflejos vuelven al cubemap puro (baseline pre-F2H61).

---

## 📚 Referencias

- Morgan McGuire, "Efficient GPU Screen-Space Ray Tracing" (2014) — algoritmo base de ray marching en screen-space (linear DDA en NDC).
- Unreal "Stochastic Screen-Space Reflections" (UE4 docs) — temporal accumulation + multiple samples por pixel para reducir noise.
- Unity HDRP SSR (URP no tiene SSR built-in pre-2024).

---

## 🎯 Out of scope (v1)

- **Stochastic / multiple samples per pixel** — convergencia temporal con TAA. Diferido.
- **Per-material SSR toggle** — todas las superficies metálicas reflejan. Si emerge demanda, agregar flag.
- **Hi-Z buffer optimization** — el ray marching iterativo simple es suficiente para v1. Hi-Z reduce pasos drásticamente pero requiere mip pyramid del depth — diferido si perf medible.
- **Refractions / SSR para vidrio transparente** — requiere otro orden de pases. Hito futuro.
