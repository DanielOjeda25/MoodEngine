# PLAN HITO F2H60 — Cascade Shadow Maps (CSM)

> **Estado**: DRAFT pendiente arranque (2026-05-16).
> **Tag previsto**: `v1.47.0-fase2-hito60`.
> **Mapeo plan original**: F2H22 de `PLAN_FASE2.md` — *"3-4 cascadas para shadows del directional light a distintas distancias. Mejor calidad sin perder rendimiento."*
>
> **Reset de scope (2026-05-16)**: F2H60 originalmente iba a ser "Source paradigm + meshes procedurales + UX polish acumulado" — propuesta del agente que se desvió del plan original. Pedido del dev: *"hagamos todo lo que falta que venia del plan original, y guardemos lo que falta, de pendientes, para no desviarnos"*. F2H60 retoma el plan: CSM para cerrar Sub-fase 2.3 (render polish) original. Los follow-ups acumulados (Source paradigm, Map Tools híbrido, etc.) quedan registrados en [`BACKLOG.md`](BACKLOG.md) sin priorización.

---

## 🎮 Mecánicas (lo que vivís como dev)

### El problema actual

MoodEngine tiene **una sola shadow map** ([`ShadowPass.h`](../src/systems/render/ShadowPass.h)) de tamaño fijo (default 2048²) que cubre el bounding sphere de toda la escena visible. Esto significa:

- **Sombras lejos**: razonables (texels grandes cubren mucho).
- **Sombras cerca**: pixelosas / blurry / con artifacts (los texels son demasiado grandes para detalle cercano).
- **Trade-off forzado**: subir el size del shadow map mejora cerca pero cuesta memoria + perf cuadráticamente (4096² = 16MB depth + 4× más fragments).

Esto es **el problema clásico** de shadow mapping single-map. La solución industrial es **PSSM/CSM** desde Crysis 2007 / id Tech 5 / Unreal 3.

### Cómo se aplica en MoodEngine post-F2H60

El directional light renderea **3-4 shadow maps independientes** (cascadas), cada una cubriendo una porción distinta del frustum de cámara:

- **Cascada 0**: muy cerca del player (0-5m). Texels finos → sombras nítidas.
- **Cascada 1**: media distancia (5-20m). Texels medios.
- **Cascada 2**: lejos (20-80m). Texels gruesos.
- **Cascada 3** (opcional): muy lejos (80-200m). Para mundos abiertos.

En el lit shader, cada fragment elige la cascada apropiada según su distancia a la cámara y samplea ese shadow map. Total memoria similar a un único map 4K, pero distribución mucho más eficiente.

### Convención industria

- **Crysis** (2007): primero PSSM (Parallel-Split Shadow Maps) — split log.
- **id Tech 5 / Rage** (2011): CSM con 4 cascadas hardcoded.
- **Unreal Engine 4+**: CSM configurable 1-10 cascadas + dynamic shadow distance.
- **Unity HDRP/URP**: CSM 1-4 cascadas con split por porcentaje configurable.
- **Godot 4**: CSM directional lights 1-4 cascadas + PCF blend.

MoodEngine adopta el patrón Unity URP / Godot 4 — 4 cascadas default con splits log + PCF dx/dy + blend entre cascadas adyacentes (sin "banding" visible al cruzar).

---

## 🧱 Bloques de ejecución

### Bloque A — Refactor del shadow map a "array de cascadas"

`OpenGLShadowMap` actualmente es un FBO 2D depth-only. Para CSM necesita un **GL_TEXTURE_2D_ARRAY** con N layers (uno por cascada) + N FBOs (uno por layer para attachar como target de render).

- **`OpenGLShadowMapArray.{h,cpp}` nuevo**: wrappea un `GL_DEPTH_COMPONENT24` 2D-array de tamaño `size × size × N`. `bindLayerAsTarget(layerIdx)` attacha esa slice como depth target. `glDepthTextureId()` retorna el array (sampler2DArrayShadow en shaders).
- **Migración**: `ShadowPass` mantiene API external pero internamente usa `OpenGLShadowMapArray` con N=1 si el sistema no quiere CSM (fallback compatibility). Para N>1, los métodos cambian a multi-pass.

**Estimado:** 2h.

**Criterio de aceptación:** tests headless validan creación + bind del array. ShadowPass con N=1 produce mismo resultado que pre-F2H60.

---

### Bloque B — Split scheme: distancias de las cascadas

El frustum de cámara se parte en N porciones por distancia. Esquemas estándar:

- **Lineal**: `splits[i] = near + (far - near) * (i / N)`. Distribución uniforme, malo para escenas con rango amplio.
- **Logarítmico**: `splits[i] = near * (far / near)^(i / N)`. Distribución exponencial — más resolución cerca, menos lejos. **Estándar industria** para juegos.
- **Práctico** (PSSM Zhang 2006): `mix(linear, log, λ)` con λ=0.5 default. Combina ambos.

Implementación en `engine/render/pipeline/ShadowMath.h::computeCSMSplits(near, far, N, lambda)` → devuelve `std::array<f32, N>` con los split distances.

**Estimado:** 1h.

**Criterio de aceptación:** tests unitarios validan splits con valores conocidos. N=4, near=0.1, far=200, lambda=0.5 produce splits aprox `{0.1, 1.4, 6.8, 33, 200}` (5 split points = 4 cascadas).

---

### Bloque C — Per-cascade frustum culling y lightSpace matrix

Para cada cascada:
1. Computar las 8 esquinas del subfrustum (entre `splits[i]` y `splits[i+1]`).
2. Transformar las esquinas a world space.
3. Calcular bounding sphere o AABB en light space (con el lightDir dado).
4. Construir lightProj ortográfico que cubre exactamente ese AABB.
5. lightView = `glm::lookAt(center - lightDir * dist, center, up)`.
6. lightSpace = lightProj × lightView.

`ShadowPass::record` itera N veces: por cada cascada, calcula lightSpace + renderea con frustum culling per-cascade (solo las entidades dentro de ese subfrustum aportan al map).

**Estimado:** 3h. Bloque crítico — bugs sutiles posibles en cálculos AABB.

**Criterio de aceptación:** debug overlay (toggle) muestra los 4 frustums (líneas de colores) en el viewport. Cada uno encierra solo la porción de escena que le corresponde.

---

### Bloque D — Lit shader: sampling de la cascada apropiada + blend

En `lit.frag`:
- Recibir `uLightSpace[4]` (array de 4 matrices) + `uCascadeSplits[4]` (distancias en view space).
- Computar la distancia view-space del fragment al ojo.
- Elegir la cascada `i` tal que `dist < uCascadeSplits[i]` (primera que matchea).
- Sampler2DArrayShadow → samplear el shadow map de la cascada `i` con PCF dx/dy.
- **Blend** entre cascadas adyacentes en la frontera (último 10% de la cascada actual): mezclar resultado con el de la cascada `i+1` para evitar banding visible.

**Estimado:** 2h.

**Criterio de aceptación:** sin banding visible al cruzar fronteras de cascadas. Sombras cerca del player nítidas, sombras lejos suaves (cascada gruesa).

---

### Bloque E — Per-scene settings + Inspector

`EnvironmentComponent` extendido con 3 campos nuevos (back-compat aditivo):
- `bool csmEnabled = true` (default ON — engine-grade visual mejora notoria).
- `u32 csmCascadeCount = 4` (1-4, default 4 — 1 cascada = compatible con shadow pre-F2H60).
- `f32 csmSplitLambda = 0.5f` (0=lineal, 1=log, 0.5=hybrid).

Inspector panel Environment > Post-procesado > nueva sub-sección **"Shadows (CSM)"**:
- Checkbox `activar CSM`.
- Slider `cascadas` (1-4).
- Slider `split lambda` (0-1, default 0.5).
- Botón `⟲ Restablecer` (estilo F2H58 Bloque G).

Persistencia aditiva en `EntitySerializer.cpp` (mismo patrón que bloom/SSAO/colorGrading — solo si difiere del default).

**Estimado:** 2h.

**Criterio de aceptación:** dev modifica cascadas en runtime → ve cambio en vivo. Round-trip al `.moodmap` correcto.

---

### Bloque F — Validación visual + cierre

Dev abre proyecto con muchos objetos a distancias variadas. Activa CSM con 4 cascadas. Confirma:
- Sombras cerca del player nítidas.
- Sin banding entre cascadas.
- Performance similar a pre-F2H60 (cascada 0 chica + cascada 3 grande no debería costar más que un único shadow map 4K — la mejora es cualitativa).

HITOS + ESTADO + DECISIONS + plan archivado + tag `v1.47.0-fase2-hito60`. ~30 min.

---

## Total estimado

**6 bloques A-F, ~10-12h**. Hito mediano. Si Bloque C (frustum per-cascade) tarda más por bugs sutiles, podemos partir CSM básico (Bloques A-E) en F2H60 + blend polish (Bloque D refinado) en sub-bloque dentro del mismo hito o follow-up.

---

## NO entra en F2H60

- **Shadow PCF avanzado / VSM / MSM** — F2H60 mantiene PCF dx/dy estándar (mismo que pre-F2H60). Si emerge demanda de soft shadows aún más suaves, follow-up con Variance Shadow Maps o Moment Shadow Maps.
- **Contact shadows** (screen-space ray-marched contact darkening) — efecto independiente de CSM. Hito propio.
- **Point light shadows** (cubemap shadow maps por point light) — pendiente original del plan, NO de Sub-fase 2.3. Re-evaluar.
- **Shadow distance global** (cap de distancia max a la que CSM cubre) — la cascada 3 ya tapa lejos hasta `far` plane. Si emerge demanda de cap explícito, follow-up.

---

## Riesgos identificados

- **Bug del bounding sphere vs AABB per-cascade**: si el AABB se calcula mal (ej. en world space en vez de light space), la cascada cubre la zona equivocada → sombras desalineadas. Mitigación: tests unitarios del cálculo de subfrustum y debug overlay visible.
- **Texel snapping**: el centro de la cascada debe "snap" a texels enteros del shadow map para evitar shimmering al mover la cámara. Patrón estándar: round del lightSpace position a `texelSize`. Aplicar desde el primer commit del Bloque C.
- **GPU memory**: 4 cascadas × 2048² × 4 bytes (depth24+stencil8) = ~64 MB. Aceptable en hardware moderno. Si emerge presión de memoria, default a size 1024 con bump opcional.
- **Performance del N×draw**: renderear la escena 4 veces (una por cascada) puede saturar el pase de shadows. Mitigación: frustum culling agresivo per-cascade en Bloque C — la cascada 0 solo dibuja objetos cerca, la cascada 3 solo dibuja muy lejos.
- **Shader compilation**: `sampler2DArrayShadow` puede no estar disponible en hardware muy viejo. MoodEngine ya requiere OpenGL 4.5 Core, que soporta GLSL 450 + 2D-array shadow samplers desde el inicio. No es riesgo real.
