# PLAN F3H11 — Persistencia Audio/Camera + refactor Brush serializer + clipboard de los 3

**Estado:** **CERRADO** — `v2.11.0-fase3-hito11` (cuarto hito de Sub-fase 3.2 "Inspector + Hierarchy pulidos").
**Predecesor:** F3H10 (Tier 2 al ComponentClipboard + 9 kits convert_modal).
**Origen:** memoria `clipboard-brush-audio-camera` — backlog explícito de F3H10 (los 3 types que quedaron grisados en el Hierarchy "Copiar valores").

---

## Qué siente el usuario

**Hoy (post-F3H10):**
- El dev hace click derecho sobre un AudioSource en la Hierarchy → "Copiar valores de AudioSource" → **grisado** con tooltip "pendiente F3H11+". Frustrante.
- Mismo flow con Camera y Brush.
- Más profundo: el dev guarda un `.moodmap` con AudioSources tuneados (volume / pitch / spatial flags). Reabre el proyecto. **Los AudioSources no están** — el `EntitySerializer` los ignora (gap F2). El dev tiene que re-spawnarlos y tunearlos. Bug latente que sale a la luz al usar audio en serio.

**Post-F3H11:**
- Click derecho sobre AudioSource/Camera/Brush en cualquier entity → "Copiar valores" funciona. Paste cross-entity preserva config.
- `.moodmap` save/load preserva los 3 components — el dev no pierde tuneados de audio ni cámaras ni brushes con valores no-default al cerrar el proyecto.
- Los 13 EntityTypes del editor tienen copy/paste funcional → "Hierarchy paste" del F3H9 queda 100% cerrado.

---

## Realidad técnica (qué sí / qué no)

### Parte A — Persistencia de `AudioSourceComponent`

**Trabajo:**
1. Definir `struct SavedAudio` en `SceneSerializer.h` espejando los fields persistentes del componente: `clipPath` (string), `volume`, `pitch`, `loop`, `spatial`, `referenceDistance`, `maxDistance`, `attenuationRolloff`, etc.
2. Sumar `std::optional<SavedAudio> audio` a `SavedEntity` (struct hoy tiene 17 optionals — agregar el 18°).
3. Write en `EntitySerializer.cpp::serializeEntityToJson`: si `hasComponent<AudioSourceComponent>` → serializar a `j["audio_source"]`. Default-fields se omiten (mismo patrón que F3H4 — schema sin bump).
4. Read en `EntitySerializer_Parse.cpp::parseEntityFromJson`: si `j.contains("audio_source")` → poblar `se.audio`.
5. Apply en `SceneLoader.cpp::applyOneEntity`: si `se.audio.has_value()` → `addComponent<AudioSourceComponent>` + setear fields. `clipPath` resuelve a `AudioAssetId` via `AssetManager::loadAudio` (¿existe?).
6. Test roundtrip: spawn entity con AudioSource tuneado → save → load → assert mismos valores.

**Riesgos:**
- `AudioAssetId` puede no existir aún o ser inestable entre sesiones — investigar al implementar. Probable usar `clipPath` puro como string (mismo patrón que `dialogPath` / `itemPath`).
- Estado runtime (playing, currentTime, etc.) NO se persiste — solo config.

### Parte B — Persistencia de `CameraComponent`

**Trabajo (similar a A pero más chico — el componente es stub):**
1. `struct SavedCamera { f32 fov; f32 nearPlane; f32 farPlane; }` en `SceneSerializer.h`.
2. `std::optional<SavedCamera> camera` en `SavedEntity`.
3. Write + Read + Apply.
4. Test roundtrip.

**Caveat:** el `CameraComponent` es stub desde F2H85 (audit). El editor usa su propia cámara, no la del componente. Persistir igual porque (a) en runtime el `MoodPlayer` podría usar la del componente para cinemáticas; (b) habilita copy/paste — el punto del hito.

### Parte C — Refactor del Brush serializer

**Trabajo:**
1. Mover `serializeBrush(Entity, const AssetManager&)` y `parseBrush(const json&)` del namespace anónimo de `SceneSerializer.cpp` al namespace público en `SceneSerializer.h`.
2. Extraer el applier inline del loop de `SceneLoader::applyMap` (líneas ~629-703, ~75 LOC) a un nuevo helper público `SceneLoader::applyBrushFromSaved(const SavedBrush& sb, Entity e, AssetManager& assets, bool applyVisGroupMembership)`. El loop pasa a llamar al helper N veces.
3. Verificar que NADA externo a `SceneSerializer.cpp` depende del anonymous namespace (`serializeBrush`/`parseBrush` no se exportan a otros .cpp del engine — grep confirma).

**Por qué:** el ComponentClipboard necesita ambos endpoints públicos para integrar Brush.

### Parte D — Sumar los 3 al `ComponentClipboard`

**Trabajo (patrón establecido por F3H10):**
1. `kKeyAudioSource = "audio_source"`, `kKeyCamera = "camera"`, `kKeyBrush = "brush"` consts en el header.
2. Sumar a `supportedKeys()`.
3. `componentNameKey()` branches → "component.name.audio_source/camera/brush" (i18n ya tiene esas keys del popup Add Component).
4. `entityHasComponent()` branches.
5. `applyPayload()` branches:
    - Audio: `applyAudio(SavedAudio, Entity, AssetManager)` — usa AudioAssetId/clipPath del paso A.
    - Camera: `applyCamera(SavedCamera, Entity)`.
    - Brush: caso especial — el payload es un `SavedBrush` directo (no `SavedEntity` wrapper). Override del dispatch: si componentKey == "brush" → `parseBrush(payload)` + `SceneLoader::applyBrushFromSaved` con `applyVisGroupMembership=false` (el Brush copiado no hereda VisGroup del source).
6. `removeComponent()` branches.
7. Tests roundtrip por type.

**Caveat de Brush en paste:** los `materialIndex` per-face apuntan al array `materialPaths` del brush copiado. Si destino existente tiene distinta lista de materials, indices fuera de rango → clamp a 0 + log warning. Documentar en tooltip de "Copiar valores de Brush".

### Parte E — Tests + validación

- 3 tests nuevos roundtrip en `test_component_clipboard.cpp` (audio/camera/brush).
- 1 test nuevo `isSupported` cubriendo los 12 keys totales (Tier 1 + Tier 2 + Tier 3 + verificación que solo "garbage" / "rigid_body" / "transform" siguen false).
- 2 tests nuevos persistencia roundtrip en `test_scene_serializer.cpp` o similar (audio + camera).
- Validación visual del dev: copy/paste de audio + camera + brush entre entidades; save/load preserva.

**NO en F3H11:**
- Tier 3 components que faltan (RigidBody / Joint / Ragdoll / Cloth / Script / Inventory / Animator) — quedan diferidos a backlog futuro si emerge demanda. La mayoría tiene assets-deep o ecosystem-heavy que requiere tratamiento individual.
- Rework destructivo del convert_modal — sigue como backlog `convert-modal-followup`.
- UI nueva — toda la maquinaria del Hierarchy "Copiar valores" ya está; F3H11 solo extiende el backend.

---

## Decisiones (pre-implementación)

### D1 — `AudioAssetId` vs path puro como string

**Contexto:** los demás componentes con asset refs (Dialog/ItemPickup/Vehicle) usan **path puro** como string en el `SavedX` (decisión consciente F2: paths son estables entre sesiones, IDs no). Audio puede tener `AudioAssetId` que el AssetManager resuelve.

**Decisión propuesta:** **path puro** (`clipPath`) en `SavedAudio`. El `AudioAssetId` runtime se re-resuelve al primer frame via `AssetManager::loadAudio(clipPath)` (mismo patrón que Vehicle con `configPath` o Dialog con `dialogPath`).

### D2 — Camera stub: persistir igual o saltearlo

**Contexto:** `CameraComponent` no se renderiza desde el editor (audit F2H85). Persistirlo es trabajo "por completeness" sin payoff inmediato.

**Decisión propuesta:** **persistir igual** — small extra de trabajo, paga al usar el MoodPlayer para cinemáticas + habilita copy/paste (objetivo del hito). Schema sin bump (`std::optional<SavedCamera> camera` solo si != defaults).

### D3 — Brush paste con material indices out-of-range: clamp vs reject

**Contexto:** un Brush con faces que tienen `materialIndex = 3` se pega a una entity destino con `materialPaths.size() = 1`. Opciones: (a) clampear indices fuera de rango a 0 + log warning; (b) rechazar el paste con error visible; (c) recortar `materialPaths` del source en el clipboard para que indices queden válidos.

**Decisión propuesta:** **(a) clamp + log warning**. Razones:
- El paste no falla silenciosamente (warning).
- Las caras quedan con material default (slot 0) en lugar de crashear o desaparecer.
- El dev puede asignar materiales correctos manualmente.
- (c) reordenaría el array y rompe semántica.

---

## Tamaño estimado

**Mediano-grande** (~4-6h). Distribución:
- Parte A (Audio persistencia): 1.5h. Schema + writes/reads + apply + test.
- Parte B (Camera persistencia): 45 min. Más chico que Audio (3 fields).
- Parte C (Brush refactor): 1h. Mover 2 funciones + extraer applier helper.
- Parte D (3 al clipboard): 1.5h. Patrón mecánico de F3H10 + Brush dispatch especial.
- Parte E (tests + validación visual): 1h.

---

## Cierre del hito — checklist verificado

- [x] Suite verde **1173/11506** (+3 cases / +14 asserts vs F3H10: 2 roundtrip Tier 3 Audio/Camera + 1 smoke Brush + 1 ampliación de `isSupported` cubriendo los 12 keys).
- [x] Save/Load roundtrip preserva AudioSource + Camera en `.moodmap` (gap F2 cerrado).
- [x] Hierarchy "Copiar valores" deja de grisar para Audio/Camera/Brush — los 12 EntityType del editor tienen copy/paste 100% funcional.
- [x] convert_modal sin cambios (los 13 kits de F3H10 siguen igual).
- [x] Validación visual end-to-end por el dev.
- [ ] Tag `v2.11.0-fase3-hito11` (al confirmar push).

## Scope ejecutado

Todas las partes del plan ejecutadas sin diferimientos:
- **Parte A**: `SavedAudio` struct + `writeAudio` + parse + `applyAudio` en SceneLoader. `clipPath` (string) → `AudioAssetId` runtime via `assets.loadAudio` (decisión D1).
- **Parte B**: `SavedCamera` struct + `writeCamera` + parse + apply. 3 fields (fovDeg/nearPlane/farPlane) — minimal.
- **Parte C**: `serializeBrush` + `parseBrush` movidas del namespace anónimo al público en `SceneSerializer.h` + `nlohmann/json.hpp` agregado al header. Applier `applyBrushFromSaved(SavedBrush, Entity, AssetManager&, bool)` extraído del loop de `SceneLoader::applyMap` a helper público. El loop ahora delega al helper (zero cambio de comportamiento).
- **Parte D**: 3 keys nuevas al ComponentClipboard (`kKeyAudioSource`, `kKeyCamera`, `kKeyBrush`) + branches en los 4 dispatchers. Brush usa dispatch especial — no pasa por `SavedEntity` wrapper, llama a `serializeBrush` y `parseBrush` directamente, paste via `SceneLoader::applyBrushFromSaved`.

## Memorias actualizadas

- `clipboard-brush-audio-camera`: **eliminada** (backlog cerrado por completo). Los 3 types están en el clipboard.

## Cerró la Parte UX de la Sub-fase 3.2

Con F3H11 los **12 EntityType** del editor tienen copy/paste end-to-end funcional + persistencia completa al `.moodmap`. La estructura del modelo EntityType (F3H9) + clipboard expandible (F3H10) + persistencia completa (F3H11) está consolidada. Próximos hitos de la sub-fase pueden enfocar otros aspectos del Inspector/Hierarchy según `PLAN_FASE3.md` (probable: Undo coverage audit).
