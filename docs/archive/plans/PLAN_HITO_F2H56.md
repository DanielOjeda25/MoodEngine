# PLAN HITO F2H56 — AO (Ambient Occlusion / sombras de rincón)

> **Estado**: DRAFT pendiente aprobación del dev (2026-05-14).
> **Tag previsto**: `v1.43.0-fase2-hito56`.
> **Origen**: segundo hito de **Sub-fase 2.6 — Render polish**. F2H55 cerró bloom. AO era la siguiente opción del plan original (orden de impacto visual: bloom → AO → color grading → god rays). Dev confirmó *"mantengamos el plan"* — F2H56 = AO.
>
> **NO entra**: shadow polish (CSM cascadas, contact shadows, soft shadows PCF). El motor ya tiene shadow mapping desde **Hito 16** (F1, tag `v0.16.0-hito16`) — funciona pero solo cubre un directional light con shadow map único 2048x2048. Mejorar la calidad de sombras (cascadas para distancias largas, contact shadows en grietas, blur PCF para edges suaves) es un hito propio futuro — el dev lo flagueó como importante post-F2H55. Tagged como **F2H6X candidate (Sub-fase 2.6 backlog)** — se atacará después de F2H56-F2H58 del plan original. No ahora.

---

## 🎮 Preguntas para el dev (en mecánicas — empezá leyendo esto)

Antes de tocar código, confirmá / ajustá estas 4 cosas:

### 1. ¿De qué tipo de sombras hablamos?

AO (Ambient Occlusion) **NO es lo mismo que la sombra de la sombra de un objeto contra la luz** (eso ya existe — Hito 16). AO es el efecto sutil donde:

- Los rincones donde dos paredes se juntan se ven un poco más oscuros.
- El piso debajo de un mueble, mesa o cubo se oscurece donde el objeto lo toca.
- Las grietas entre piedras de un muro se ven con sombra suave.
- Bajo los párpados de un personaje, dentro de las orejas, alrededor de la nariz.

Es el "el objeto está apoyado en el mundo" effect. Sin AO los objetos parecen flotar aunque tengan sombra direccional. Con AO se sienten parte del mundo.

**Propuesta**: implementación screen-space AO. Esto significa que cualquier geometría en pantalla recibe el efecto automáticamente sin que vos configures nada por objeto.

### 2. ¿Cuán marcado lo querés?

- **Sutil** (Mass Effect, The Last of Us): apenas perceptible, le da peso pero no se nota explícitamente.
- **Medio** (Witcher 3, RDR2): claramente visible en rincones y bajo objetos. Mainstream.
- **Marcado** (Borderlands, anime style): líneas oscuras casi como contorno. Estilizado.

**Propuesta: medio** por default, con slider de intensidad en el panel Environment.

### 3. ¿Globalmente igual o por mapa?

Mismo dilema que tuvimos con bloom — vos elegiste **por mapa** ahí. ¿Mantenemos la misma decisión?

**Propuesta**: per-mapa. Los settings van al mismo `EnvironmentComponent` (campos `ssaoEnabled`, `ssaoIntensity`, `ssaoRadius`). Esto sigue acumulando la "configuración visual del mapa" — bloom + AO + futuros (color grading, god rays) todos en el mismo lugar.

### 4. ¿Librería externa o port?

Para bloom decidimos portar de Godot/Filament porque no había lib plug-and-play. Para AO la situación es **diferente** — **AMD FidelityFX CACAO** es una librería real, MIT, mantenida por AMD, calidad superior a SSAO genérico. Usada en juegos comerciales.

- **Opción A — AMD FidelityFX CACAO**: integrar el SDK. Pros: mejor calidad, código mantenido externamente, menos bugs que un port. Contras: dependencia nueva en el motor, más superficie de integración (~1 día extra).
- **Opción B — Port de SSAO de Filament**: ~150 líneas de GLSL portadas, no agrega dependencia. Pros: consistencia con cómo se hizo bloom. Contras: calidad inferior a CACAO.
- **Opción C — Empezar simple (SSAO básico)** y migrar a CACAO si la calidad pincha: arrancar barato, evaluar al ver el resultado.

**Mi voto: Opción C** — implementar SSAO portado de Filament primero (~3h). Si visualmente queda bien, listo. Si queda flojo, swap a CACAO en un sub-hito (no rehace el wireup, solo el shader).

### Mecánicas que F2H56 entrega (resumen — para releer cuando vuelvas)

1. Las grietas, rincones, debajo de muebles/cubos/personajes se oscurecen sutilmente.
2. El piso debajo de tu caja blanca de testing se va a ver un poco más oscuro donde la caja toca el piso — "apoyada", no flotando.
3. Panel Environment gana 3 controles nuevos: checkbox `activar AO`, slider `intensidad`, slider `radio`.
4. Settings se guardan por mapa en el `.moodmap` (mismo lugar que bloom).
5. Mapas viejos siguen cargando con defaults.
6. Lo podés apagar por mapa (slider intensity = 0).

Cuando confirmes / ajustes las 4 preguntas, arranco con el Bloque A. El resto del documento (abajo) es el detalle técnico para mí.

---

## Filosofía

**Polish no-destructivo + per-mapa** (igual que F2H55). AO se aplica como un pass adicional al pipeline de iluminación; si se desactiva, el render es idéntico a pre-F2H56 (cero regresión).

**Engine-grade strict**: el motor expone el efecto y los knobs; el dev del juego decide cuándo prenderlo / cómo. Aplicado:
- El motor NO asume "todo mapa indoor = AO alto, outdoor = bajo". El dev lo pone por mapa.
- El motor NO categoriza objetos como "recibe AO o no". Todo lo que aparece en pantalla contribuye.

---

## Scope técnico

### `EnvironmentComponent` extendido (Bloque C de F2H55 ya sentó el patrón)

3 campos nuevos:
```cpp
bool  ssaoEnabled   = true;
f32   ssaoIntensity = 1.0f;     // 0 = sin efecto, 2 = doble
f32   ssaoRadius    = 0.5f;     // en world units; mas alto = AO mas amplio
```

`SavedEnvironment` paralelo. EntitySerializer write aditivo (solo si != default). Parse con defaults explícitos.

### Pipeline: SSAOPass

Insertar antes del cálculo de iluminación o al fin del scene render. Lee el depth buffer (y opcionalmente normales) del GBuffer / scene FB, calcula un factor [0..1] por píxel (0=oscuro, 1=sin oclusión), multiplica el término ambient del PBR por ese factor.

**Decisión abierta** (a confirmar con el dev al arrancar): ¿usar G-buffer separado o reconstruir normales del depth?

- **G-buffer**: más correcto, pero requiere modificar `SceneRenderer` para escribir normales a un attachment extra (RGBA16F adicional).
- **Reconstruir normales del depth**: menos correcto en superficies con normales tangenciales, pero no requiere tocar el render existente.

**Propuesta**: reconstruir desde depth (cheap, ya tenemos depth attachment). Si emerge calidad pobre, migrar a G-buffer.

---

## Bloques de ejecución (DRAFT a refinar al confirmar las 4 preguntas)

### Bloque A — Shaders SSAO

Portar de Filament `assets/materials/postprocess/ssao.mat` o Godot 4 `servers/rendering/.../ssao.glsl`.

- `ssao.frag`: sample N puntos en hemisferio orientado por la normal del píxel, contar cuántos están "detrás" del depth → factor AO.
- `ssao_blur.frag`: bilateral blur para suavizar el noise inherente al sampling.

~120 líneas GLSL portadas. ~2h.

### Bloque B — `SSAOPass` C++

Análogo a `BloomPass`. Mip chain NO necesario — un FB single-channel R8 alcanza para guardar el AO factor.

~150 líneas C++. ~2h.

### Bloque C — EnvironmentComponent + serialización

3 campos nuevos + Saved + write/parse + SceneLoader + SceneRenderer cache. Patrón idéntico a F2H55 Bloque C, ~30 min de boilerplate.

### Bloque D — Panel sliders

Sección "AO (sombras de rincón)" en el Inspector debajo de "Bloom (glow)". Checkbox + 2 sliders. 3 i18n keys nuevas. ~30 min.

### Bloque E — Validación visual

Dev verifica con su caja blanca de testing: el piso debajo de la caja se oscurece sutilmente donde la caja toca. Movés sliders y ves cambio en vivo.

### Bloque F — Cierre

HITOS + ESTADO + DECISIONS + plan archivado + tag.

---

## Total estimado

**6 bloques A-F, ~6-8h** (más liviano que F2H55 porque la estructura de wiring ya está montada).

---

## Criterios de aceptación

1. Caja blanca sobre piso muestra **oscurecimiento sutil** donde toca el piso.
2. Panel Environment permite mover los 2 sliders **vivo** sin reload.
3. Setear `ssaoIntensity = 0` deja la escena visualmente idéntica a F2H55 cerrado (cero regresión).
4. Persistencia roundtrip: editás settings → guardás → reabrís → settings restaurados.
5. Mapas pre-F2H56 cargan con defaults sin error.
6. Tour visual del dev: *"se ve bien"* o equivalente.

---

## NO entra en F2H56 (queda para hitos siguientes)

- **F2H57**: Color grading LUT-based.
- **F2H58**: Rayos volumétricos / god rays.
- **F2H6X (backlog)**: **Shadow polish** — Cascade Shadow Maps (CSM), contact shadows, soft shadows PCF, shadow distance fade. El dev flagueó como importante post-F2H55. Atacar después de cerrar el ciclo planeado (F2H58).

---

## Riesgos identificados

- **Calidad SSAO básico**: si el AO portado de Filament queda "noisy" o "halo-y" en bordes, sumar TAA (Temporal AA) lo arregla. Pero TAA es otro hito. Si emerge, swap a CACAO de AMD (Opción A de la pregunta 4).
- **Cost en GPU**: SSAO suma ~1-2 ms a 1080p en GPU integrada. Aceptable.
- **Default visualmente intrusivo**: si el default `intensity = 1.0` se siente exagerado, bajar a 0.7 en defaults code. Iteración trivial.
