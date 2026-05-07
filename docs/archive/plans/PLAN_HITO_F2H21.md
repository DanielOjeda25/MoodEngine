# Plan — F2H21: Material editor con preview esférico off-screen

> **Leer primero:** `ESTADO_ACTUAL.md` (F2H20 cerrado), `PENDIENTES.md`
> (F2H22 candidato 4-viewport Hammer ya anotado), `PLAN_FASE2.md`
> entrada **F2H17** original ("Material editor con node-graph (visual)").

---

## Decisión de scope

El plan F2 original F2H17 agrupaba **node-graph visual + nodos básicos +
preview esférico + persistencia**. Tras evaluar el scope:

- Node graph visual implementado a mano: ~500-700 LOC de UI ImGui (cards
  de nodos, drag de conexiones, hit-testing, layout estable).
- Compilación runtime del graph a GLSL + cache por hash: ~300-500 LOC
  (necesario para que los nodos tengan efecto real, sino son cosmética).
- Refactor de `SceneRenderer` para shaders custom por material (cada
  material con graph distinto = shader distinto): ~150-300 LOC de
  cambios + risk de regresión en el path de batching de F2H4.

Total ≈ 1-2 semanas de hito grande.

**F2H21 entrega el componente de mayor valor inmediato del plan
original — preview esférico off-screen — sin el costo del node graph.**
El dev ve sus cambios de tint / sliders / drop de texturas en una esfera
preview en el mismo panel, sin tener que asignar el material a una
entidad del viewport. El node graph queda como hito futuro (probable
F2H23 tras el 4-viewport).

---

## Objetivo

Agregar un **preview esférico off-screen** dentro del `MaterialEditorPanel`
que renderice una esfera con el material seleccionado, sobre fondo
neutro + iluminación IBL, refrescando en vivo cuando el dev edita
sliders o dropea texturas. Plus polish UX:

1. **Save .material**: botón "Guardar" que escribe el material editado
   al `.material` JSON original (preservando back-compat con el formato
   actual).
2. **Layout columnas**: el panel pasa a 2 columnas — izquierda controles
   (combo + sliders + texture slots), derecha preview.

## Filosofía de diseño

- **Helper puro reusable**: `MaterialPreviewRenderer` en
  `engine/render/preview/` — un FBO 256×256 LDR + mesh esfera +
  shaders PBR ya existentes (no duplicar el shader). El SceneRenderer
  no se toca.
- **Render manual cada frame que el panel está visible**: render off-screen
  → bind ImGui::Image. ~0.2-0.5 ms por frame en GTX 1660; impacto
  marginal con el panel cerrado.
- **IBL compartido con el SceneRenderer**: el preview lee el mismo
  `irradiance` + `prefilter` + `brdfLut` que el SceneRenderer tiene
  cargados. Pasar handles via setter, no duplicar disk-load.
- **Camara fija**: orbit decorativa lenta o estática frontal. Sin
  interacción de mouse en el preview (mantener simple). Si emerge
  necesidad, hito futuro.
- **Sin runtime preview de UV / normal maps en separado**: solo material
  PBR full preview. El dev ya ve los maps en el AssetBrowser.
- **Save respeta el schema existente** (`albedo`, `metallic_roughness`,
  `normal`, `ao` paths + `albedo_tint` array + scalars `metallic` /
  `roughness` / `ao`). No bump.

## Decisiones arquitectónicas

| # | Decisión | Valor |
|---|---|---|
| 1 | Tamaño del FBO preview | 256×256 LDR. Suficiente para preview compacto, ~0.2 ms render. |
| 2 | Mesh del preview | Esfera primitiva del `AssetManager` (`primitiveSphereId`). Reusable. |
| 3 | Cámara del preview | Frontal estática a (0, 0, 2) mirando origen. Sin orbit por ahora. |
| 4 | Iluminación | IBL del scene renderer (compartido) + 1 directional fija (3/4 desde arriba-derecha). |
| 5 | Re-render | Cada frame el panel es visible. Skip cuando `!visible` (no costo). |
| 6 | Save | Botón "Guardar" → escribe el `.material` JSON al path lógico actual. Marca dirty del proyecto. |
| 7 | Tests | `MaterialPreviewRenderer` puro: instanciable sin GL via fakes (testear setMaterial / getOutputTexture interface). El render real solo se valida en el editor. |
| 8 | Refactor del SceneRenderer | **Ninguno**. El preview es un componente lateral; no toca el render pass principal. |

## Bloques

### A — Plan F2H21 (este archivo)

Documento del hito. Cierra al commit inicial.

### B — `MaterialPreviewRenderer`

`engine/render/preview/MaterialPreviewRenderer.{h,cpp}`. API:

```cpp
class MaterialPreviewRenderer {
public:
    MaterialPreviewRenderer(u32 width = 256, u32 height = 256);
    ~MaterialPreviewRenderer();

    /// Inyecta el shared IBL del SceneRenderer. Si nullptr, cae al
    /// ambient escalar.
    void setIblTextures(OpenGLCubemapTexture* irradiance,
                         OpenGLCubemapTexture* prefilter,
                         ITexture* brdfLut);

    /// Renderiza la esfera con el material indicado. Devuelve la
    /// textura LDR resultante para que el caller la muestre con
    /// ImGui::Image.
    void renderPreview(const MaterialAsset& mat,
                        AssetManager& assets);

    /// Handle del color attachment del FBO interno. Usar como
    /// `ImTextureID` en ImGui::Image.
    GLuint outputTextureId() const;

    u32 width() const { return m_fbWidth; }
    u32 height() const { return m_fbHeight; }
};
```

Internamente:
- 1 framebuffer LDR (RGBA8) 256×256 + depth.
- Reusa el `pbr.vert` + `pbr.frag` del repo.
- Reusa `assets.primitiveSphereId()`.
- Vista fija: `glm::lookAt({0, 0, 2.5}, {0, 0, 0}, {0, 1, 0})`.
- Proyección perspectiva 30° aspect 1:1.
- 1 directional light hardcoded: dir = `normalize({-0.4, -0.6, -0.7})`,
  color blanco, intensity 3.0.
- Setea uniformes del shader igual que el SceneRenderer (uView /
  uProjection / uModel identity / lights / IBL / fog off).
- `glViewport(0, 0, 256, 256)` antes del draw, restaurado por el
  caller (`MaterialEditorPanel` lo restaura tras render).

### C — Integración en `MaterialEditorPanel`

- Layout 2 columnas: `ImGui::Columns(2)`. Izquierda: combo + sliders +
  texture slots (igual que hoy). Derecha: preview con `ImGui::Image`.
- Inyectar `MaterialPreviewRenderer*` desde `EditorApplication`
  (alocado al ctor, IBL textures inyectadas desde el SceneRenderer
  tras crearlo).
- Cada frame que el panel está visible: `previewRenderer->renderPreview(mat,
  assets)`; mostrar `ImGui::Image(previewRenderer->outputTextureId(), {256, 256})`.
- Botón "Guardar" debajo del preview: serializa el `MaterialAsset`
  al path lógico actual (`assets.materialPathOf(matId)`). Si el path
  es vacío o el sentinel `__default_material`, deshabilitado con
  tooltip explicativo.

### D — Tests

`tests/test_material_preview_renderer.cpp`. Cobertura limitada (no
podemos crear contexto GL en tests sin headless setup), pero:
- Constructor con 0×0 = no crashea (early return).
- `setIblTextures(nullptr, nullptr, nullptr)` = válido (ambient mode).
- `width()` / `height()` devuelven el tamaño del ctor.

Test del `Save .material`: crear un `MaterialAsset` mock, escribirlo
con la nueva función `saveMaterialJson(mat, path)`, leerlo de vuelta
con `loadMaterial`, verificar igualdad de campos (round-trip). Esto
lo cubrimos en `tests/test_material_serializer.cpp` existente o
nuevo `test_save_material.cpp`.

### E — Validación visual + suite

- Build editor + tests.
- Suite verde, esperado **~602 → ~610 cases / ~8323 → ~8350 asserts**.
- Validación visual del dev:
  1. Abrir `Material Editor` panel desde menú Ver.
  2. Seleccionar un material existente del combo (ej. `oro_pulido.material`).
  3. Verificar que la esfera se ve con el material en la mitad derecha
     del panel. Texturas, tint, metallic / roughness se aplican.
  4. Mover sliders → la esfera refresca en vivo.
  5. Drop una textura sobre el slot albedo → la esfera la muestra.
  6. Botón "Guardar" → el `.material` JSON se sobreescribe en disco
     con los nuevos valores. Reabrir editor y verificar que persisten.

### G — Cierre

- `docs/HITOS.md`: nueva entrada F2H21 closed.
- `docs/ESTADO_ACTUAL.md`: F2H21 al top.
- `docs/DECISIONS.md`: entrada `2026-05-07` "F2H21: preview esférico
  off-screen, scope acotado vs node-graph del plan original".
- Tag: `v1.12.0-fase2-hito21`.
- `PENDIENTES.md`: mover **node-graph + shader runtime compilation** del
  plan original a backlog activo (probable F2H23 tras 4-viewport).

---

## Suite

Todo el código nuevo es **aditivo**:
- `MaterialPreviewRenderer.{h,cpp}` (~200 LOC).
- Cambios en `MaterialEditorPanel` (~80 LOC).
- 1 método helper `AssetManager::saveMaterialJson` o función libre
  (~40 LOC).
- 1-2 archivos de test (~80 LOC).

No toca `SceneRenderer`, no toca el shader PBR, no toca persistencia
del scene. Riesgo de regresión bajísimo.

## Riesgos

- **GPU integrada Intel Iris Xe** (vista en F2H20 logs): probable que
  el FBO LDR + un draw call extra por frame no afecte. Si emerge slow,
  toggle visible-only render (ya planeado).
- **IBL textures null al arrancar**: si el SceneRenderer falla cargando
  IBL (ver el try/catch en su ctor), el preview cae a ambient — no
  crashea, solo se ve con menos pop. Documentado.
- **Save .material**: si el path lógico no es escribible (asset embedded
  en zip futuro, etc.), abrir dialog `pfd::message` con error.
  Sentinel paths (`__default_material`) deshabilitan el botón.

## Criterios de cierre

- [ ] `MaterialPreviewRenderer` compila + linkea en Debug.
- [ ] Panel `Material Editor` muestra esfera preview en columna derecha.
- [ ] Sliders / texture drops refrescan en vivo.
- [ ] Botón "Guardar" persiste cambios al `.material`.
- [ ] Suite verde, sin regresiones.
- [ ] Tag `v1.12.0-fase2-hito21` pusheado.
- [ ] `PENDIENTES.md` actualizado con node-graph como F2H23 candidato.
