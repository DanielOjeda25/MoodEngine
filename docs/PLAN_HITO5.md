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

- [ ] `GridMap` default `tileSize` de `1.0f` a `3.0f`. Default `WallHeight` sigue == `tileSize` (cúbico).
- [ ] `EditorApplication::k_playerHalfExtents` de `(0.2, 0.45, 0.2)` a `(0.3, 0.9, 0.3)` (player ~0.6×1.8×0.6 m).
- [ ] `FpsCamera` default position al centro del tile (2,6) en la nueva escala (world ~`(-4.5, 1.6, 7.5)`), eye height 1.6 m. Velocidad `m_speed` de `3.0` a `4.0` m/s.
- [ ] `EditorCamera` default `radius` de `12` a `30` para que el mapa 8×8 (24×24 m) entre en cuadro.
- [ ] Verificar tests (algunos fijan números concretos que podrían cambiar).
- [ ] Entrada en `DECISIONS.md`: **1 unidad = 1 metro SI**. Todos los assets importados (Hito 10+) asumen esto.

## Bloque 1 — Textura fallback "missing"

- [ ] Script `tools/gen_missing_texture.py` que genere `assets/textures/missing.png` (256×256, chequered rosa #FF00FF / negro #000000).
- [ ] Commitear el PNG + el script.
- [ ] `OpenGLTexture` que al fallar stbi_load registre `Log::assets` en vez de solo warn (o tirar excepción), y que el `AssetManager` intercepte para devolver la handle del fallback.

## Bloque 2 — AssetManager

- [ ] `src/engine/assets/AssetManager.{h,cpp}`: interfaz mínima.
  - `TextureHandle loadTexture(std::string_view path)` — devuelve handle cacheado.
  - `ITexture* getTexture(TextureHandle)` — resolver handle → textura.
  - `TextureHandle missingTexture()` — handle del fallback.
- [ ] Interna: `std::unordered_map<std::string, TextureHandle>` para cacheo por path, `std::vector<std::unique_ptr<ITexture>>` como storage.
- [ ] `EditorApplication` construye el AssetManager, carga `grid.png` + `missing.png` al arrancar.
- [ ] Reemplazar el `m_gridTexture` crudo por una handle pedida al AssetManager.
- [ ] Logger `assets` nuevo en `Log`.

## Bloque 3 — VFS inicial

- [ ] `src/platform/VFS.{h,cpp}`: `resolvePath(std::string_view logical) -> std::filesystem::path`. Inicial: `textures/grid.png` → `<cwd>/assets/textures/grid.png`.
- [ ] Rechazar path traversal (`..`, paths absolutos).
- [ ] Tests: resolución básica, rechazo de `..`, case-insensitive en Windows si aplica.
- [ ] `AssetManager::loadTexture` acepta path lógico (va por VFS).

## Bloque 4 — AssetBrowserPanel real

- [ ] Reemplazar el placeholder "Sin assets cargados" por listado real: itera `assets/textures/` y muestra grid de miniaturas (64×64 en ImGui con `ImGui::ImageButton`).
- [ ] Click en miniatura: por ahora solo selecciona (guarda en un `std::optional<TextureHandle> m_selected` dentro del panel). Drag & drop entra en Hito 6.

## Bloque 5 — Textura por tile

- [ ] `GridMap::tiles` pasa de `std::vector<u8>` a `std::vector<Tile>` donde `struct Tile { TileType type; TextureHandle texture; }`. O agregar `std::vector<TextureHandle> m_tileTextures` paralelo — decidir.
- [ ] `setTile(x, y, type, textureHandle)` sobrecarga.
- [ ] El render del grid bindea la textura correcta por tile antes del draw (rompe el "bind once fuera del loop" actual: hay que moverlo adentro o agrupar por textura).
- [ ] Mapa de prueba: borde perimetral con `grid.png`, columna central con una segunda textura (ej. `brick.png` generada nueva).

## Bloque 6 — ConsolePanel

- [ ] Sink custom de spdlog que empuja cada log a un ring buffer en memoria (512 entradas).
- [ ] `src/editor/panels/ConsolePanel.{h,cpp}`: lee el ring buffer, dibuja las líneas con color por nivel (info blanco, warn amarillo, error rojo) y filtro por canal.
- [ ] Toggle en el menú "Ver".

## Bloque 7 — Cierre

- [ ] Recompilar y verificar todos los criterios.
- [ ] Actualizar `docs/HITOS.md`, `docs/DECISIONS.md`, `docs/ESTADO_ACTUAL.md`.
- [ ] Commits atómicos en español.
- [ ] Tag `v0.5.0-hito5` + push.
- [ ] Crear `docs/PLAN_HITO6.md`.

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

_(llenar al cerrar el hito)_
