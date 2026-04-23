# Plan — Hito 5: Asset Browser + gestión de texturas

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md`. Las secciones 4.12 (Assets) y 5.6 (VFS) de `MOODENGINE_CONTEXTO_TECNICO.md` definen el alcance planeado.
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Objetivo

Pasar de "motor que carga una textura hardcoded" a "motor con catálogo de assets consultable desde el editor". Concretamente:

1. `AssetManager` con cacheo por path y devolución de handles estables.
2. Textura de fallback rosa/negro ("missing") servida automáticamente cuando un asset no existe o falla al cargar.
3. VFS inicial que resuelve rutas lógicas (`textures/grid.png`) a paths físicos (`assets/textures/grid.png`). Primera pata del sistema que después permitirá empacar todo en `.pak`.
4. `AssetBrowserPanel` del editor lee una carpeta real y muestra miniaturas de las texturas encontradas.
5. Las paredes del grid pueden tener texturas distintas por tile (antes: todas la misma `grid.png`).
6. Panel `ConsolePanel` simple que muestra los últimos N logs del motor con filtros por canal.

---

## Criterios de aceptación

### Automáticos

- [ ] `cmake --build build/debug --config Debug` compila sin errores ni warnings nuevos.
- [ ] Log del canal `assets`: `AssetManager: cargada texture <ruta> -> handle <id>`, y `AssetManager: fallback a missing.png para <ruta>` cuando corresponde.
- [ ] Suite de tests suma casos para `AssetManager` (cacheo, fallback, handles) y `VFS` (resolución, path traversal rechazado).
- [ ] Cierre limpio, exit 0.

### Visuales

- [ ] `AssetBrowserPanel` muestra miniaturas reales de todas las `.png` bajo `assets/textures/`.
- [ ] Pared(es) del mapa renderizan con al menos 2 texturas distintas.
- [ ] Si borro `grid.png` y reabro el editor, las paredes se dibujan con la textura missing (rosa/negro) sin crashear.
- [ ] `ConsolePanel` aparece como panel acoplable nuevo y muestra los logs con su canal a la izquierda.

---

## Bloque 0 — Escala realista (arrastrado del Hito 4)

Aplicar antes de tocar AssetManager. El resto del Hito 5 asume estas dimensiones.

- [x] `GridMap` default `tileSize` de `1.0f` a `3.0f`. Default `WallHeight` sigue == `tileSize` (cúbico).
- [x] `EditorApplication::k_playerHalfExtents` de `(0.2, 0.45, 0.2)` a `(0.3, 0.9, 0.3)` (player 0.6×1.8×0.6 m).
- [x] `FpsCamera` default position de `(0, 1, 3)` a `(0, 1.6, 0)` (altura ojos humana). En `EditorApplication`, player spawn en tile (2,6) world `(-4.5, 1.6, 7.5)`. Velocidad `m_speed` de `3.0` a `3.0` m/s (subida a 4.0 y vuelta a 3.0 tras feedback del dev: "algo lenta pero bajaría a 3 m/s" — el paso humano sostenido).
- [x] `EditorCamera` default `radius` de `4.0` a `30.0` para que el mapa 8×8 (24×24 m) entre en cuadro. `EditorApplication` también arranca con 30.
- [x] Verificar tests. `test_cameras` asumía `speed=3` con distancia recorrida 3; actualizado.
- [x] Entrada en `DECISIONS.md`: **1 unidad = 1 metro SI**.

## Bloque 1 — Textura fallback "missing"

- [x] Script `tools/gen_missing_texture.py` que genere `assets/textures/missing.png` (256×256, patrón 2×2 de bloques rosa #FF00FF / negro, estilo Source).
- [x] Commitear el PNG + el script.
- [x] Logger `assets` agregado a `Log.{h,cpp}` (prep para Bloque 2).
- [>] `OpenGLTexture` / `AssetManager` wiring del fallback → diferido a Bloque 2 (el AssetManager es quien intercepta; `OpenGLTexture` sigue tirando excepción, el catch vive en el manager).

## Bloque 2 — AssetManager

- [x] `src/engine/assets/AssetManager.{h,cpp}`: interfaz minima.
  - `TextureAssetId loadTexture(std::string_view logicalPath)` — devuelve id cacheado. Nombrado `TextureAssetId` para no pisar `TextureHandle` (void* opaco del RHI).
  - `ITexture* getTexture(TextureAssetId)` — nunca null; ids invalidos caen al fallback.
  - `missingTextureId()` — vale 0 (slot reservado).
- [x] Interna: `std::unordered_map<std::string, TextureAssetId>` para cache por path, `std::vector<std::unique_ptr<ITexture>>` como storage. Catch de excepciones de `OpenGLTexture` -> cachear fallback y loguear warn en canal `assets`.
- [x] `EditorApplication` construye el AssetManager, carga `missing.png` en el constructor del manager y `textures/grid.png` al arrancar.
- [x] Reemplazado `m_gridTexture` crudo por `m_wallTextureId` pedido al manager. Bind en el loop: `m_assetManager->getTexture(m_wallTextureId)->bind(0)`.
- [x] Logger `assets` ya agregado en Bloque 1.

## Bloque 3 — VFS inicial

- [x] `src/platform/VFS.{h,cpp}`: `resolve(std::string_view logical) -> std::filesystem::path` + `isSafeLogicalPath` estático. La raíz se pasa al constructor (por ahora `"assets"` desde `AssetManager`).
- [x] Rechazar path traversal: `..`, `.`, paths absolutos (`C:/`), y leading `/` o `\` (Windows no los reporta como `is_absolute()` sin drive letter pero siguen siendo escape attempts).
- [x] Tests `tests/test_vfs.cpp`: 5 casos (acepta relativos simples, rechaza `..`/`.`, rechaza absolutos unix+windows, resolve concatena, resolve vacío para unsafe).
- [x] `AssetManager` usa `VFS` internamente; `loadTexture` ya acepta paths lógicos tipo `"textures/grid.png"`.

## Bloque 4 — AssetBrowserPanel real

- [x] Reemplazado el placeholder por listado real: escanea `assets/textures/`, carga cada PNG vía `AssetManager` y muestra grid de miniaturas 64×64 con `ImGui::ImageButton` (UV flip vertical porque stbi carga con flip, ImGui asume top-left origin). Columnas se recalculan al redimensionar el panel. Botón "Recargar" re-escanea.
- [x] Click en miniatura: guarda el path lógico en `std::optional<std::string> m_selected` dentro del panel (borde azul de la miniatura seleccionada). Loguea `[assets] AssetBrowserPanel: seleccionado 'textures/foo.png'`. Drag & drop sigue en Hito 6.
- [x] `EditorApplication` le inyecta el manager vía `m_ui.assetBrowser().setAssetManager(...)`.

> Feedback del dev: "las miniaturas aparecian y se seleccionaban, pero no tenian acciones" — comportamiento esperado; la acción de asignar textura al tile es posterior (Hito 6).

## Bloque 5 — Textura por tile

- [x] Decidido: arrays paralelos (`std::vector<u8> m_tiles` + `std::vector<TextureAssetId> m_tileTextures`), 0 overhead en los tests existentes y menor diff que un `struct Tile`. Si crece la info por tile (flags, variantes), promover a struct.
- [x] `setTile(x, y, type, texture)` como overload (+ `tileTextureAt(x, y)` para leer). El `setTile(x, y, type)` viejo queda como conveniencia.
- [x] Render bindea la textura por tile con tracking `lastBound` para evitar rebinds cuando tiles consecutivos comparten textura (optimización barata que aprovecha el orden del iterado).
- [x] Mapa de prueba: perímetro con `grid.png`, columna central con `brick.png` generada con `tools/gen_brick_texture.py` (256×256 RGBA, aparejo inglés dos tonos).

## Bloque 6 — ConsolePanel

- [x] `src/core/LogRingSink.{h,cpp}`: custom sink de spdlog con ring buffer de 512 entradas. Thread-safe con `std::mutex` propio (inherit `base_sink<null_mutex>` para evitar double-lock con el mutex interno de spdlog). `snapshot()` copia el contenido en orden cronológico.
- [x] `Log.{h,cpp}` registra el ring sink como tercer sink de cada logger. Expone `Log::ringSink()` para que el panel lo consuma.
- [x] `src/editor/panels/ConsolePanel.{h,cpp}`: renderiza las entradas con color por nivel (trace/debug/info/warn/err/critical), tag corto (`INF`, `WRN`, ...), filtro por substring de canal, auto-scroll opcional, botón "Limpiar".
- [x] Toggle en el menú "Ver" (automático, `EditorUI` lo agrega al vector de paneles).
- [x] Layout default: Console acoplado en la zona inferior derecha (split 55/45 del bottom dock), Asset Browser a la izquierda. Reset de `imgui.ini` para aplicar el nuevo default.

## Bloque 7 — Cierre

- [x] Recompilar y verificar todos los criterios (build limpio, 35/179 tests, editor arranca con Console+grid+brick, fallback missing cargado).
- [x] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [x] Commits atómicos en español (8 commits, uno por bloque + docs).
- [x] Tag `v0.5.0-hito5` + push.
- [x] Crear `docs/PLAN_HITO6.md`.

---

## Qué NO hacer en el Hito 5

- **No** implementar `.pak` files / empaquetado (Hito 21).
- **No** serializar el mapa en disco (Hito 6).
- **No** agregar Tracy todavía (diferido a Hito 5-6, pero no bloqueante; puede quedar para el 6).
- **No** implementar hot-reload de assets (útil pero no bloqueante; entra después si aparece la necesidad).
- **No** migrar a EnTT ni ECS (Hito 7).

---

## Decisiones durante implementación

_(llenar a medida que aparezcan)_

---

## Pendientes que quedan para Hito 6 o posterior

- **Tests de `AssetManager`**: no los agregué en este hito porque `OpenGLTexture` requiere contexto GL. Requiere refactor a factory inyectable (`AssetManager(TextureFactoryFn)`) o un `MockTexture` no-GL para testear cacheo, fallback e id-reuse sin tocar la GPU. Cubre `loadTexture` (cache hit), `loadTexture` de un path roto (devuelve missing, loguea warn), `getTexture(invalid_id)` (devuelve missing).
- **Drag & drop desde AssetBrowser a un tile del viewport**: planteado originalmente para este hito, la acción se difiere al Hito 6 porque requiere tile picking (raycast del mouse a la grid) que todavía no existe.
- **Asignar `TextureAssetId` al AABB del jugador para el debug draw**: no entró; el color del AABB jugador queda hardcoded verde.
- **Extraer `GridRenderer`**: el render del grid sigue inline en `EditorApplication::renderSceneToViewport`. Con las texturas por tile la duplicación empieza a doler (tracking de bind, loop por tiles); buen momento para mover a `src/systems/GridRenderer.{h,cpp}` en Hito 6.
- **Hot-reload del `AssetBrowserPanel`**: el botón "Recargar" re-escanea pero no invalida texturas ya cargadas. Si reemplazo un PNG en disco, sigo viendo el viejo en GPU hasta reiniciar. Agregar una comparación de mtime y `glTexImage2D` sobre la textura existente cuando cambió. No urgente.
- **Reset de layout desde menú**: cuando agrego un panel nuevo (`Console` en Hito 5), los usuarios con `imgui.ini` viejo lo ven flotando. Un item "Restablecer layout" en el menú "Ver" que invoque `DockBuilderRemoveNode` + rebuild solucionaría esto sin borrar el archivo a mano.
- **Tracy profiler**: diferido desde Hito 3, aún sin incorporar. Evaluar en Hito 6 si los frames empiezan a notarse pesados con más geometría.
