# F2H2 — Instrucciones para correr el benchmark (sesión hand-off)

> **Para el agente que abra una sesión nueva en la PC con la GTX 1660.**
> El código de F2H2 ya está commiteado y pusheado a `main`. Este documento
> tiene todo lo necesario para compilar, correr el benchmark, y reportar
> los números. **No** continúes con F2H3 — el cierre de F2H2 lo hace la
> otra sesión cuando reciba tus números.

---

## Contexto

- **Hito en curso:** F2H2 — Tracy + benchmark sistemático.
- **Plan completo:** `docs/PLAN_HITO_F2H2.md`.
- **Bloques A-F:** ya cerrados (Tracy integrado, frame instrumentado,
  spawners stress-test, Performance HUD, plantilla `PERFORMANCE.md`).
- **Bloque G (este):** correr el benchmark y rellenar `docs/PERFORMANCE.md`.
- **Bloque H (siguiente):** cierre del hito (commit + tag `v1.1.0-fase2-hito2`).

Resumen de lo que se agregó:
- Tracy v0.11.1 vía CPM, controlado por la opción CMake `MOOD_PROFILE` (default ON).
- `src/core/Profiler.h` con macros `MOOD_PROFILE_FRAME / SCOPE / FUNCTION / PLOT`.
- Frame instrumentado: zonas en scripts/animation/nav/particles/audio/physics/UI/render,
  sub-zonas dentro del SceneRenderer (shadow, skybox, light grid, PBR static,
  PBR skinned, particle, debug flush, post-process).
- Menú **Ayuda > Stress test poligonos** con 4 entries (10K / 100K / 500K / 1M tris en cubos).
- Panel **Performance HUD** toggleable desde menú **Ver > Performance**: muestra FPS,
  frame ms, draw calls, triángulos, entity count. Off por default.

---

## Paso 1 — Pull + build

```bash
git pull
cmake --build build/debug --config Debug
```

**Avisos:**
- La primera build después del pull va a ser **larga** (~3-8 min): Tracy se
  compila desde fuente la primera vez vía CPM. Builds incrementales son rápidas.
- Si CMake falla porque no encuentra el target `Tracy::TracyClient`, regenerar
  con `cmake --preset debug` (o el preset que use el repo) antes del build.
- Suite esperada: **319/6613** sin regresiones. Correrla con
  `ctest --test-dir build/debug -C Debug` si querés verificar antes de medir.

---

## Paso 2 — Tracy server (opcional pero recomendado)

1. Bajar el binario `tracy-profiler.exe` de https://github.com/wolfpld/tracy/releases
   — **versión exacta v0.11.1**. El zip viene con el .exe portable.
2. Abrirlo, click **Connect** → host: `127.0.0.1`. Queda esperando un cliente.
3. Cuando lances MoodEditor.exe (paso 3), Tracy auto-conecta. Las zonas
   aparecen en la timeline en vivo.

Sin Tracy server abierto, el cliente sigue funcionando con ~1-2% overhead
(modo "on demand"). Si solo te interesan los números del HUD podés saltearte
este paso, pero **el "top 3 zonas Tracy" del PERFORMANCE.md sí lo necesita**.

---

## Paso 3 — Correr el editor + abrir el HUD

1. Lanzar `build/debug/Debug/MoodEditor.exe`.
2. Crear un proyecto nuevo o abrir uno existente (Welcome modal). Cualquiera
   sirve — el benchmark se hace sobre la escena vacía / poblada por los spawners.
3. Menú **Ver > Performance** → aparece la ventana del HUD con métricas en vivo.
4. Si Tracy server está abierto, ya debería estar conectado (mirá la timeline).

---

## Paso 4 — Tomar las mediciones

Para cada una de las 5 escenas, anotá: **FPS, frame ms, draw calls, triángulos**
del HUD + (si Tracy abierto) las **3 zonas con mayor `% of frame time`** del
panel **Statistics** de Tracy (orden descendente, abrí View > Statistics).

Dejá el editor corriendo unos segundos en cada escena para que el FPS se
estabilice antes de anotar.

### Escena 1 — Empty (proyecto recién creado o sin spawnear nada)

Solo está visible el grid del mapa.
- FPS: ?
- Frame ms: ?
- Draw calls: ?
- Tris: ?
- Top 3 zonas Tracy:
  1. ?
  2. ?
  3. ?

### Escena 2 — 10K tris

Menú **Ayuda > Stress test poligonos > Spawn 10K tris (cubos)**. Esperar 2-3 segundos.
- FPS / Frame ms / Draws / Tris / Top 3 Tracy.

### Escena 3 — 100K tris

**Importante:** primero deshacé los 833 cubos del paso anterior con `Ctrl+Z`,
o cerrá y reabrí el editor sin guardar. Después: **Spawn 100K tris (cubos)**.
- FPS / Frame ms / Draws / Tris / Top 3 Tracy.

### Escena 4 — 500K tris

Igual: limpiar (Ctrl+Z o relaunch) → **Spawn 500K tris**. Probable que el FPS
caiga fuerte. Esperar a que se estabilice.
- FPS / Frame ms / Draws / Tris / Top 3 Tracy.

### Escena 5 — 1M tris

Idem → **Spawn 1M tris**.
- Si **crashea** o se cuelga >30 segundos, **anotá "no llega"** y pasá al paso 5.
  No insistas — F2H3 (frustum culling) y F2H4 (LOD) son lo que va a hacer
  esto viable, no es regresión.
- Si llega: anotar valores.

---

## Paso 5 — Rellenar `docs/PERFORMANCE.md`

Editar el archivo y completar:

1. **Hardware baseline**: rellenar GPU / CPU / RAM (los 3 placeholders `_(rellenar)_`).
2. **Tabla de resultados**: completar las 5 filas con los números medidos.
3. **Top 5 cuellos de botella**: identificar a partir de las "top 3 zonas Tracy"
   recurrentes en las escenas pesadas (100K-500K). Para cada cuello:
   - Nombre del cuello (ej: "PBR::staticPass scene iteration").
   - % del frame que se lleva.
   - Hito previsto que lo ataca (mirá `docs/PLAN_FASE2.md` sección 4 —
     candidatos típicos: F2H3 frustum culling, F2H4 LOD, F2H? batching).
4. **Conclusiones**: 2-3 líneas resumen.

---

## Paso 6 — Commit + reportar

```bash
git add docs/PERFORMANCE.md
git commit -m "docs(F2H2 Bloque G): rellenar baseline GTX 1660 + top 5 cuellos"
git push
```

Después dejá un mensaje al desarrollador con un resumen tipo:

```
F2H2 Bloque G cerrado:
- 1M tris: <FPS> / <frame ms> | top zona: <nombre> <%>
- 500K: <FPS> / <ms> | top zona: <nombre> <%>
- (etc)
- Top cuello identificado: <nombre> — F2H? lo ataca.
PERFORMANCE.md commiteado y pusheado. Listo para que F2H2 Bloque H cierre el hito.
```

---

## Lo que NO debés hacer

- **No empezar F2H3.** El cierre de F2H2 (Bloque H) — actualizar HITOS, ESTADO_ACTUAL,
  DECISIONS, taggear `v1.1.0-fase2-hito2` — lo hace la sesión que recibe tu reporte.
- **No tocar la instrumentación Tracy** (`Profiler.h`, las macros sembradas).
  Si encontrás un bug puntual, comentalo en el reporte — no es momento de cambiar
  el setup mientras estás midiendo.
- **No optimizar nada.** F2H2 es **solo medición**. Las optimizaciones tienen
  hitos propios.
- **No correr la suite con MOOD_PROFILE=OFF "para limpiar"** — la build con
  Tracy es la que vas a usar y es la suite válida para F2H2.

## Si algo se rompe

- **Build falla con errores en `tracy/Tracy.hpp`:** revisar que `cmake --build`
  haya generado bien el target `Tracy::TracyClient`. Si fall en el primer try,
  borrar `build/debug/_deps/tracy-*` y reconfigurar.
- **MoodEditor.exe arranca pero no aparece HUD:** menú "Ver" debe listar
  `Performance`. Si no, la build agarró código viejo — relinkear.
- **Tracy server dice "connection refused":** el cliente Tracy es **on-demand**
  — solo escucha cuando el server intenta conectar primero. Abrí Tracy primero,
  *después* lanzá MoodEditor.
- **Cualquier crash o regresión inesperada:** capturar el log, **abortar el
  benchmark**, y reportar al desarrollador con el commit hash (`git rev-parse HEAD`).
  No intentes "arreglarlo" como parte de F2H2.
