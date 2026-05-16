# PLAN HITO F2H58 — Color grading (LUT-based) + panel Environment consolidado

> **Estado**: DRAFT pendiente arranque (2026-05-15).
> **Tag previsto**: `v1.45.0-fase2-hito58`.
> **Origen**: continúa **Sub-fase 2.6 — Render polish** después del pivot UX de F2H57. Tercero del orden bloom (F2H55) → SSAO (F2H56) → **color grading (F2H58)** → god rays (F2H59).
>
> Pre-arranque el dev preguntó cómo organizan otros motores los settings de post-process. Convención industria es **un solo panel** (Unity Volume / Unreal Post Process Volume / Godot WorldEnvironment Inspector) — todos los efectos en secciones colapsables del mismo Inspector. El dev confirmó: *"me gusta, asi vamos ordenando esto y tiene todo un sentido, no quiero que este distribuido"*. F2H58 incorpora la consolidación como Bloque A para que color grading entre limpio en el panel correcto + queden ordenados los efectos previos (bloom + SSAO + tonemap que hoy viven en `InspectorPanel_Light.cpp` por accidente histórico).

---

## 🎮 Mecánicas (lo que vivís como dev)

### El "look" de tu escena

Color grading es la herramienta que le da personalidad cromática a tu juego sin tocar la iluminación ni los materiales. Es lo que diferencia el verde-cyan del primer Matrix del tono cálido-sepia del último Mad Max — ambos podrían tener idéntico bloom, SSAO y assets, pero el grade hace que se sientan películas distintas.

### Cómo se aplica en MoodEngine post-F2H58

Seleccionás la entidad **Environment** de tu escena (la misma donde hoy editás bloom, SSAO, tonemap). El Inspector ahora muestra UN solo panel "Environment / Post-Process" con secciones colapsables:

- **Tonemap + Exposure** (ya existía).
- **Bloom (glow)** (F2H55).
- **AO (sombras de rincón)** (F2H56).
- **Color Grading** ← lo nuevo.
- (futuro: God Rays — F2H59; otros — F2H6X+).

En la sección Color Grading tenés:
- **Checkbox** "activar color grading" — apagado = look default.
- **Slot LUT**: un campo path tipo `assets/luts/cinema_warm.png`. Click → file picker filtrado a `.png` (o `.cube` si se soporta — ver Bloque B). Si el campo está vacío y el checkbox activado, MoodEngine usa la LUT identidad (no cambia nada) — útil para tener el effect "armado" antes de elegir la paleta.
- **Slider intensidad** (0..1): cuánto del grade se mezcla con el color original. 0 = sin cambios, 1 = grade puro. 0.6-0.8 es el sweet spot para que el grade aporte personalidad sin saturar.
- **Preview**: la escena se actualiza en vivo al mover el slider o cambiar la LUT — convención de Unity/Unreal.

### Convención industria

Una LUT (Look-Up Table) es una textura que mapea colores de entrada → colores de salida. Formato más común: **PNG 256x16** organizado como 16 slices de 16x16 que representan el cubo RGB completo. Editás la LUT en DaVinci Resolve / Photoshop / Filmlight Baselight aplicando ajustes de color a una imagen de referencia (la "LUT identidad") y exportás el resultado. Cualquier motor (Unity, Unreal, Godot, Frostbite) la lee igual.

Esto significa que un colorista puede entregarte LUTs directamente desde su tool de cine — el motor solo las aplica.

---

## 🧱 Bloques de ejecución

### Bloque A — Consolidación del panel Environment

**Problema actual**: los controles de Bloom + SSAO + Tonemap + Exposure viven en `src/editor/panels/scene/InspectorPanel_Light.cpp::renderEnvironmentSection`. Eso significa que el dev solo ve esos controles si selecciona la entidad con `LightComponent`. Si su Environment es una entidad sin Light (caso normal con `EnvironmentComponent` puro), no ve nada.

**Solución**:
1. **Crear `InspectorPanel_Environment.cpp`** (nuevo archivo, ~300 líneas estimadas) con la función `renderEnvironmentSection(Entity)` extraída de `InspectorPanel_Light.cpp`.
2. **Trigger del render**: el panel se renderiza si la entidad seleccionada tiene `EnvironmentComponent`. Mismo pattern que el resto de los sub-paneles del Inspector (Mesh, Light, Physics, etc.).
3. **Secciones colapsables**: `ImGui::CollapsingHeader` por categoría (Tonemap+Exposure / Bloom / AO / Color Grading futuro). Default abierto en la primera apertura del proyecto (`ImGuiTreeNodeFlags_DefaultOpen`).
4. **Cleanup en `InspectorPanel_Light.cpp`**: remover la llamada a `renderEnvironmentSection` desde el panel Light. Si una entidad tiene tanto `LightComponent` como `EnvironmentComponent`, ambos paneles se renderizan apilados (sin duplicación).
5. **i18n**: ajustar las keys existentes (`environment.*`) no necesita cambios — se reusan. Agregar `environment.section.tonemap`, `environment.section.bloom`, `environment.section.ssao` para los headers colapsables.

**Estimado**: 1-2h. No introduce features nuevos, solo reubica.

**Criterio de aceptación**: dev selecciona una entidad con `EnvironmentComponent` (puede ser sin LightComponent) → ve los controles. Funcionalidad de bloom/SSAO/tonemap idéntica a F2H56.

---

### Bloque B — Shader + pase de Color Grading

**4 archivos nuevos en `shaders/`** (mismo patrón que bloom F2H55 / SSAO F2H56):
- `color_grading.vert` — fullscreen triangle (probablemente reuso de `post_process.vert` con `#include` o duplicación leve).
- `color_grading.frag` — sample del color HDR + sample de la LUT con conversión 3D->2D + `mix(originalColor, gradedColor, intensity)`.

**Implementación del lookup LUT**:
- LUT format: **textura 2D de 256x16** (16 slices de 16x16, layout horizontal — convención Unity / Unreal). Cada slice corresponde a un nivel de blue del cubo RGB.
- Fragment shader: convierte el color HDR a UV de la LUT usando el algoritmo estándar de Unity URP:
  ```glsl
  vec3 colorGrading(vec3 color, sampler2D lut, float intensity) {
      // Clamp to [0,1] — color grading no opera sobre HDR > 1 (eso es trabajo del tonemap).
      vec3 c = clamp(color, 0.0, 1.0);
      float blueSlice = c.b * 15.0;
      float blueFloor = floor(blueSlice);
      float blueFrac = blueSlice - blueFloor;
      vec2 uvLow = vec2(c.r / 16.0 + blueFloor / 16.0, c.g);
      vec2 uvHigh = vec2(c.r / 16.0 + (blueFloor + 1.0) / 16.0, c.g);
      vec3 graded = mix(texture(lut, uvLow).rgb, texture(lut, uvHigh).rgb, blueFrac);
      return mix(color, graded, intensity);
  }
  ```
- Ubicación en pipeline: **antes del tonemap final** (color grading opera en LDR-ish range, después de bloom + SSAO pero antes de la curva tonemap). En `SceneRenderer::endFrame`:
  ```
  scene HDR → SSAO → bloom → COLOR GRADING → post-process (tonemap+exposure)
  ```

**Clase C++ nueva**: `src/systems/render/ColorGradingPass.{h,cpp}` (~120 líneas estimadas, mismo patrón que `BloomPass`/`SSAOPass`):
- `apply(src, dst, lutTextureId, intensity) → bool`. Retorna `false` si LUT no cargada o intensity=0 — caller usa src directo.
- Sin FBs propios: lee de `src` (HDR del bloom/SSAO output), escribe a `dst`. El caller decide qué FB pasarle.
- Save/restore GL state.

**Carga de LUT**: extender `AssetManager` con un `loadLut(path) → TextureId` que reusa `loadTexture` pero validando dimensiones 256x16 (warn si no matchea + fallback a identidad). La textura identidad la sintetiza el AssetManager en su ctor (igual que el `__missing_cube` mesh): un buffer 256x16 RGBA8 con valores `color = (R/16 + floor(B*15)/16, G/16, 0)` etc.

**Estimado**: 3-4h.

**Criterio de aceptación**: con una LUT identidad cargada, la escena se ve idéntica a sin color grading (validación de que el algoritmo de lookup es correcto). Con una LUT de prueba (sepia, fría, etc.), la escena cambia de tono al subir intensity.

---

### Bloque C — Wireup en SceneRenderer + extensión EnvironmentComponent

- **Campos nuevos en `EnvironmentComponent`** (`Components.h`):
  - `bool colorGradingEnabled = false` (default OFF — a diferencia de bloom/SSAO; color grading sin LUT no aporta y agregar look default cuestionable).
  - `std::string colorGradingLutPath = ""` (path lógico, vacío = identidad).
  - `float colorGradingIntensity = 1.0f`.
- **`SavedEnvironment` paralelo** en `SceneSerializer.h`.
- **Serialización aditiva** en `EntitySerializer.cpp` (mismo patrón que bloom/SSAO — solo persiste si difiere del default).
- **`SceneLoader.cpp`**: copia Saved → Component.
- **`SceneRenderer`**: nuevos members `m_colorGradingPass` + `m_colorGradingFb` (HDR intermedio, igual que bloomFb). `m_lutTextureCache` mapa `path → TextureId` para no recargar.
- **`endFrame()`**: insertar el pase entre bloom y post-process. Si `colorGradingEnabled && lutLoaded && apply()` succeeded, `postProcessSrc = colorGradingFb`; else `postProcessSrc = afterBloom`.

**Estimado**: 1-2h.

**Criterio de aceptación**: en runtime el dev activa el checkbox, elige una LUT, mueve el slider → la escena cambia en vivo. Apaga checkbox → vuelve al look pre-F2H58.

---

### Bloque D — UI sliders en el panel Environment consolidado

- **Sección nueva** "Color Grading" en `InspectorPanel_Environment.cpp` (post Bloque A).
- **Widgets**:
  - Checkbox `activar color grading` (binding a `colorGradingEnabled`).
  - Path field con botón "..." que abre file picker filtrado a `.png` (start dir: `<project>/assets/luts/` si existe, fallback `<project>/assets`).
  - Slider `intensidad` (0..1, default 1.0).
  - Botón "Cargar LUT identidad" como reset.
- **i18n nuevas** (`environment.color_grading.*`): 6 keys en es.json + en.json (label, tooltip, path, identity reset, etc.).
- **Undo**: `pushEditIfDone` al soltar el slider y al cambiar la LUT path (mismo patrón F2H55/F2H56).

**Estimado**: 1-2h.

**Criterio de aceptación**: el dev navega Inspector → Environment → Color Grading section → controles funcionan + cambios persisten al `.moodmap` round-trip.

---

### Bloque E — LUT samples shipados

Shipear 3-4 LUTs de ejemplo en `assets/luts/`:
- `identity.png` (la LUT identidad — útil para reset por código y testing).
- `cinema_warm.png` (sepia/cálido — atardecer / westerns).
- `matrix_cool.png` (verde-cyan tipo Matrix).
- `noir_high_contrast.png` (alto contraste blanco-negro — opcional).

Generar las LUTs custom desde una identity + aplicar curve en GIMP/Photoshop/DaVinci. Documentar el proceso en un README corto en `assets/luts/` para que el dev pueda generar las suyas.

**Estimado**: 1h.

**Criterio de aceptación**: dev puede cargar `cinema_warm.png` y ver la escena cambiar al tono cálido sin haber tenido que generar la LUT él mismo.

---

### Bloque F — Validación visual

Dev abre proyecto → selecciona Environment → secciones colapsables se muestran ordenadas (Tonemap / Bloom / AO / Color Grading) → activa color grading → carga `cinema_warm.png` → mueve intensity → escena vira a cálido. Confirma que el reorden del panel no rompió ninguno de los efectos previos (bloom + SSAO + tonemap siguen funcionando idénticos a F2H56). ~15 min.

---

### Bloque G — Cierre

HITOS + ESTADO + DECISIONS + plan archivado + tag `v1.45.0-fase2-hito58`. ~30 min.

---

## Total estimado

**7 bloques A-G, ~9-12h**. Hito mediano (similar a F2H55/F2H56 + el refactor de consolidación que es chico).

---

## NO entra en F2H58

- **Color grading "tone-mapped" (post-tonemap LUT)**: hay dos escuelas — aplicar LUT en HDR pre-tonemap (lo que hace F2H58, convención Unity URP) vs LDR post-tonemap (convención Unreal). v1 va con HDR pre-tonemap por ser más simple. Si emerge que las LUTs de DaVinci/Resolve no quedan bien (ellas se generan asumiendo input post-tonemap), evaluar switch.
- **Soporte de `.cube` files** (formato Adobe — texto ASCII). v1 solo PNG por simplicidad. Si emerge demanda, parser de `.cube` → textura 2D 256x16 en runtime.
- **LUTs de 32x32x32 o 64x64x64** (más resolución que las 16x16x16 standard). v1 hardcoded a 16. Switch a parámetro si emerge necesidad.
- **F2H59**: god rays / shafts de luz volumétrica (siguiente hito).
- **F2H6X (backlog)**: shadow polish (CSM cascades, contact shadows, soft shadows PCF).
- **Preview 3D del mesh en el modal "+ Crear Entidad"** (memoria de proyecto, hito futuro independiente).
- **Snapshot undoable del paso convert** (limitación documentada de F2H57).

---

## Riesgos identificados

- **Consolidar el panel sin romper undo**: `InspectorPanel_Light.cpp::renderEnvironmentSection` ya tiene cableado el `pushEditIfDone` para undo. Al mover la función a `InspectorPanel_Environment.cpp` hay que mantener el cableado intacto. Refactor de literal copy-paste + verificar que Ctrl+Z sobre sliders de bloom siga funcionando post-Bloque A.
- **LUT identidad sintetizada vs shipped**: si el dev no ship `identity.png` pero el motor la sintetiza en runtime, hay riesgo de divergencia (motor genera mal la identidad y todas las LUTs custom basadas en ella quedan rotas). Mitigación: ship `identity.png` también + test que valida que `texture(identityLut, color) ≈ color` para varios sample colors.
- **Color space subtleties**: LUTs de DaVinci suelen exportarse en sRGB. Las texturas en MoodEngine se cargan con `GL_SRGB8_ALPHA8` para meshes pero las LUTs probablemente no quieren sRGB conversion (la LUT es una tabla de mapeo, no una imagen para mirar). Cargarlas con `GL_RGBA8` linear + documentarlo.
- **Performance**: 1 sample extra por pixel (color grading sample). Negligible vs SSAO (16 samples) o bloom (mip chain). No emerge presión esperada.
