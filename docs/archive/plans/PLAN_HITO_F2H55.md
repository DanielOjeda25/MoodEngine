# PLAN HITO F2H55 — Bloom (glow) + Environment settings per scene

> **Estado**: DRAFT pendiente aprobación del dev (2026-05-14).
> **Tag previsto**: `v1.43.0-fase2-hito55`.
> **Origen**: primer hito de **Sub-fase 2.6 — Render polish**. La Sub-fase 2.5 (Diálogos / Inventario / Quests) cerró en F2H53. Ahora arranca la tanda de "cómo se ve" del motor: bloom, AO de rincones, color grading, rayos volumétricos. F2H55 abre con bloom — el efecto de mayor impacto visual inmediato y técnicamente más acotado.

---

## 🎮 Preguntas para el dev (en mecánicas — empezá leyendo esto)

Antes de tocar código, confirmá / ajustá estas 4 cosas:

### 1. ¿De qué cosas querés que salga el glow?

Hay dos formas de que el motor decida qué brilla:

- **Por brillo de pantalla** (propuesta): cualquier cosa que en pantalla supere cierto umbral de brillo desborda. Esto incluye automáticamente: el sol, la luz directa de una bombilla, una explosión de partículas, un material que pusiste muy emissive en el node-graph. Es lo que hace Unreal/Unity por default — universal sin tener que marcar nada.
- **Solo lo marcado**: solo brillan los materiales que vos taggees como "emissive" en el editor. Más control, menos automático.
- **Las dos cosas** (mi recomendación): umbral base + extra para emissive. El sol brilla solo, pero si querés que tu antorcha brille MÁS de lo natural, le subís el emissive.

**Mi voto: opción 3 (las dos)**, pero arrancar implementando solo la 1 (umbral) que ya cubre 90%. Materials emissive van a brillar igual porque dan output luminoso.

### 2. ¿Cuán fuerte querés que sea el glow?

- **Sutil** (Mass Effect 2, The Last of Us): apenas un halo, no llama la atención. Sensación cinematográfica.
- **Medio** (Witcher 3, RDR2): notorio pero no choca. Default de la industria.
- **Marcado** (Borderlands, anime, Doom Eternal): efecto bien presente, casi caricatura.

**Propuesta: medio por default**, con un slider visible en el editor para subir/bajar por mapa.

### 3. ¿La intensidad es la misma en todo el juego o por mapa?

Imaginá tu juego con una cueva, un bosque y una ciudad. ¿El glow se siente igual en los tres, o querés que la cueva tenga más bloom (linternas, hongos brillantes) y el desierto menos (sol plano)?

- **Por mapa** (propuesta): cada `.moodmap` guarda sus propios settings de environment (bloom + fog que ya existe + skybox + futuras opciones). Cuando cargás un mapa, se aplican los suyos. Esto va a ser la base donde se acumulan futuros polish (color grading, AO).
- **Global**: un solo setting para todo el juego.
- **Por entidad cámara**: cada cámara tiene su look (útil para cinemáticas custom, pero más complejo).

**Mi voto: por mapa**, con un default global como fallback.

### 4. ¿Lo querés tunear vivo en el editor o requiere reload?

- **Vivo** (propuesta): nuevo panel "Environment" en el editor con sliders. Movés el slider y ves cambiar la escena en tiempo real. Tipo Unity Lighting Settings o Unreal PostProcessVolume.
- **Por config + reload**: editás un JSON y recargás el mapa.

**Propuesta: vivo.** Es la única forma de tunear bien un look — necesitás ver el resultado mientras movés.

### Mecánicas que F2H55 entrega (resumen)

1. Las luces brillantes (sol, explosiones, partículas mágicas, materiales emissive) ahora desbordan halo en pantalla.
2. Hay un panel **Environment** en el editor con sliders: *Bloom intensity*, *Bloom threshold*, *Bloom radius*.
3. Los settings se guardan en el `.moodmap` — cada mapa tiene su look propio.
4. Por default cualquier mapa nuevo arranca con bloom medio (no apagado, no exagerado).
5. Lo podés desactivar por mapa (slider intensity = 0).

Cuando confirmes / ajustes las 4 preguntas, arranco con el Bloque A. El resto del documento (abajo) es el detalle técnico — no necesitás leerlo.

---

## Filosofía

**Polish no-destructivo.** Bloom es un efecto de presentación que se aplica DESPUÉS del render principal — no toca cómo se calculan materiales ni iluminación. Esto significa:

- Si bloom se desactiva (intensity = 0), la escena se ve igual que hoy. **Cero regresión.**
- El pipeline existente (`SceneRenderer` → HDR framebuffer → `PostProcessPass` → LDR → ImGui) NO se reescribe. Se inserta UN pass nuevo entre el HDR y el PostProcessPass.
- Settings nuevos se serializan **aditivos** al `.moodmap` (campo `environment` opcional). Mapas viejos cargan con defaults sin tocarlos.

**Engine-grade (regla VISION).** El motor expone el efecto y los knobs; el dev del juego decide cuándo prenderlo / cómo. Aplicado:

- El motor NO asume "todo mapa indoor = bloom alto, outdoor = bajo". El dev lo pone por mapa.
- El motor NO categoriza materials como "emissive vs no". Cualquier output que supere el threshold contribuye.

---

## Scope técnico

### Componente nuevo: `EnvironmentSettings` (scene-level, no entity)

Datos por escena (ya hay `FogSettings` en `pipeline/Fog.h` — la mejor convención es agruparlos):

```cpp
struct EnvironmentSettings {
    // Bloom (F2H55)
    bool   bloomEnabled   = true;
    f32    bloomThreshold = 1.0f;   // luminance threshold (HDR units, >1 = sobreexpuesto)
    f32    bloomIntensity = 0.6f;   // blend factor del bloom sobre el HDR original
    f32    bloomRadius    = 1.0f;   // upsample blur scale (0.5 = ajustado, 2.0 = amplio halo)

    // Fog (ya existe — migrar aquí en bloque D o dejar separado v1)
    // FogSettings fog;

    // Reservados para hitos siguientes: ssao*, colorGrading*, godRays*
};
```

**Vive en**: `Scene` (singleton del scene tree) o en una nueva tabla `scene.environment` del `.moodmap`. **Decisión abierta**: confirmar dónde lo hookea `SceneLoader`.

### Pipeline: BloomPass

Insertar **entre** `SceneRenderer` (que produce HDR RGBA16F) y `PostProcessPass` (que aplica tonemap + gamma):

```
Hoy:    [SceneRenderer → HDR] → [PostProcessPass → LDR] → ImGui

F2H55:  [SceneRenderer → HDR] → [BloomPass → HDR'] → [PostProcessPass → LDR] → ImGui
                                       ↑
                            (HDR' = HDR + bloomContribution)
```

**Algoritmo**: mip-chain estilo Call of Duty Advanced Warfare presentation (2014, ahora estándar):

1. **Downsample**: 6 niveles del HDR a 1/2, 1/4, 1/8, 1/16, 1/32, 1/64. Cada uno con un filtro 13-tap "partial Karis average" para evitar fireflies.
2. **Upsample + add**: del más chico al más grande, cada nivel se upsamplea con tent filter 9-tap y se suma al siguiente.
3. **Composite**: el upsample final se suma al HDR original con `lerp(hdr, hdr + bloom * intensity, ...)`.

**Threshold soft-knee**: en el primer downsample, se aplica `max(luminance - threshold, 0)` con un knee suave (no hard cutoff — evita parpadeo en pixels que cruzan el threshold).

**Decisión abierta**: 6 mips o 5. 6 da halo más amplio (más cinematográfico), 5 es más ajustado y rápido. Default 6, configurable como const.

---

## Bloques de ejecución

### Bloque A — Shaders bloom (downsample + upsample + composite)

**Fuente de los shaders: portar de Godot 4 / Filament** (no reinventar). Ambos son open-source con licencias permisivas (MIT / Apache 2.0). Algoritmo subyacente: presentación Call of Duty Advanced Warfare 2014 (Sledgehammer/Activision), estándar de facto en la industria desde hace 10 años. Adaptamos a nuestro RHI OpenGL + nombrado MoodEngine, anotamos la fuente en el commit.

3 fragment shaders nuevos en `shaders/`:

- `bloom_downsample.frag`: 13-tap filter con Karis weighted average en mip 0 (evita fireflies).
- `bloom_upsample.frag`: tent 9-tap para upsampling progresivo + suma.
- `bloom_composite.frag`: blend final `lerp(hdr, hdr + bloom * intensity, intensity)`.

Reuso del fullscreen-triangle pattern de `PostProcessPass` (sin VBO, `gl_VertexID`).

**Test**: ninguno headless (shaders no testeables sin GL context). Validación visual en bloque E.

**~80 líneas GLSL portadas, ~1.5h.**

### Bloque B — BloomPass C++ + pipeline wiring

Nuevo archivo `src/systems/render/BloomPass.{h,cpp}` con misma estructura que `PostProcessPass`:

```cpp
class BloomPass {
public:
    BloomPass();
    ~BloomPass();
    void resize(i32 width, i32 height);  // recrea mip chain
    void apply(OpenGLFramebuffer& hdrInOut,
               f32 threshold, f32 intensity, f32 radius);
};
```

Mip chain interno: array de 6 `OpenGLFramebuffer` RGBA16F. `resize()` se llama cuando cambia el viewport.

Wireup en `EditorRenderPass.cpp` + `PlayerApplication`: instanciar BloomPass al lado del PostProcessPass, llamar `apply()` solo si `EnvironmentSettings.bloomEnabled && intensity > 0`.

**Test**: smoke test que construya/destruya el pass sin crash + verifique que los FBs se crean. Sin GL context real, esto requiere el harness existente.

**~200 líneas C++, ~3h.**

### Bloque C — EnvironmentSettings struct + scene hookup

Nuevo struct en `engine/scene/environment/EnvironmentSettings.h`. `Scene` gana getter/setter. SceneLoader lee/escribe el campo opcional `environment` del .moodmap (back-compat: ausente → defaults).

**Tests** (headless, sin GL):
- Roundtrip serialización: settings → JSON → settings idéntico.
- Back-compat: `.moodmap` v1 sin `environment` carga con defaults.
- 4 tests nuevos.

**~150 líneas, ~1.5h.**

### Bloque D — Panel "Environment" en el editor

Nuevo panel `src/editor/panels/scene/EnvironmentPanel.{h,cpp}` registrado en la categoría "Scene". Sliders ImGui:

- `[checkbox]` Bloom enabled
- `[slider 0..3]` Threshold
- `[slider 0..2]` Intensity
- `[slider 0.5..3]` Radius

Cada cambio marca scene dirty (para que el `*` aparezca en el title bar como con cualquier otra edición).

**Test**: ninguno (panel ImGui). Validación visual.

**~120 líneas, ~1.5h.**

### Bloque E — Demo / sample scene

Sample `.moodmap` con varios materials emissive (un cubo brillante, un sol direccional alto, una luz puntual fuerte) para que el dev vea el bloom obvio. Reuso del menú **Ayuda > Demos** que ya existe.

**Validación visual**: dev abre la demo, ve los emisivos desbordar. Sin slider en 0 → todo plano (regresión cero).

**~50 líneas + .moodmap nuevo, ~1h.**

### Bloque F — Cierre del hito

- Suite verde (sumar los 4 tests del bloque C al total).
- `HITOS.md` + `ESTADO_ACTUAL.md` + `DECISIONS.md` actualizados.
- Mover este plan a `docs/archive/plans/PLAN_HITO_F2H55.md`.
- Tag `v1.43.0-fase2-hito55`.
- Próximo plan: `PLAN_HITO_F2H56.md` (AO de rincones — la propuesta natural según el orden de impacto visual).

**~1h.**

---

## Total estimado

**6 bloques A-F, ~10h** (con margen para imprevistos en el wireup OpenGL).

---

## Criterios de aceptación

1. Demo `bloom_demo.moodmap` arranca con materiales emissive **claramente desbordando halo** en pantalla.
2. Panel Environment permite mover los 3 sliders **vivo** (sin reload) y se ve el cambio inmediato.
3. Setear bloom intensity = 0 deja la escena visualmente idéntica a hoy (validación de no-regresión).
4. Persistencia roundtrip: editás settings → guardás .moodmap → cerrás editor → reabrís → settings restaurados.
5. Mapas viejos sin campo `environment` cargan con defaults sin error.
6. Suite verde con +4 tests roundtrip/back-compat.
7. Tour visual del dev: *"se ve bien"* o equivalente.

---

## NO entra en F2H55 (queda para hitos siguientes de Sub-fase 2.6)

- **F2H56**: SSAO (sombras de rincón) — `EnvironmentSettings.ssao*` se agrega ahí.
- **F2H57**: Color grading (LUT-based "look") — `EnvironmentSettings.colorGrading*`.
- **F2H58**: Rayos volumétricos / god rays — `EnvironmentSettings.godRays*`.
- **F2H59+**: cualquier otro polish (DoF, motion blur, screen-space reflections) — evaluar al cierre de F2H58.

Lens flares: NO (separado de bloom, propio hito si emerge).
Auto-exposure / adaptive exposure: NO (el `exposure` actual del PostProcessPass ya es manual y suficiente).
Per-camera environment override: NO (per-scene v1 cubre 95% — cinemáticas custom evaluar si emerge presión).

---

## Riesgos identificados

- **Mip chain framebuffers en resize**: 6 FBs RGBA16F a 1920x1080 = ~30 MB total. Aceptable. Recrear todos en cada resize → gasto puntual al cambiar viewport. Mitigación: solo recrear si `width/height` cambian de verdad (cache de last size).
- **Fireflies (pixels HDR muy intensos en escena con poca cobertura)**: el filtro Karis average del bloque A los aplaca. Si emergen igual, ajustar threshold por default.
- **Default visualmente intrusivo**: si el default `intensity = 0.6` se siente exagerado, el dev lo ve en la demo y lo bajamos a 0.4 o 0.3. Iteración trivial.
- **Back-compat de `.moodmap`**: estándar resuelto (campo opcional, defaults). Tests cubren.
