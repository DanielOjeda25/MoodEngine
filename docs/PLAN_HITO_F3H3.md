# PLAN F3H3 — Auditoría + catalogación de valores hardcodeados

**Estado:** Planeado (tercer hito de Sub-fase 3.1, arranca tras `v2.2.0-fase3-hito2`).
**Predecesor:** F3H2 (User Preferences — gemelo de F3H1 per-instalación).
**Origen:** F3H1 y F3H2 dejaron los dos chasis (per-proyecto y per-instalación). Antes de empezar a colgar fields editables ahí (F3H4-F3H7), hay que saber **qué constantes hay en el código que el usuario debería poder editar**. Sin ese mapa, los hitos siguientes irían a tientas — y la regla "nada hardcodeado" (espina dorsal de Fase 3) se queda en intención.

---

## Qué siente el usuario

Nada directo. F3H3 es un hito interno — no agrega features ni cambia comportamiento visible. El "feel" se nota recién en F3H4-F3H7, cuando cada constante catalogada acá aparece como field editable en el panel correcto (Project Settings / Preferences / Inspector / .moodmap).

Pero el efecto indirecto es importante: los hitos F3H4+ saben **exactamente** qué tienen que exponer, en qué orden, y dónde. Sin sorpresas a mitad del flujo.

---

## Realidad técnica (qué sí / qué no)

**Sí en F3H3:**
- Recorrer el código (`src/`) e identificar constantes de **comportamiento** (no de implementación interna):
  - Constantes literales en expresiones de gameplay (`gravity = -9.81f`, `walkSpeed = 5.5f`, `coyoteWindow = 0.15f`).
  - Magic numbers en sistemas configurables (`autosaveIntervalSec`, `maxRecentProjects`, FPS targets adicionales).
  - Defaults de componentes que el dev/usuario quisiera tunear globalmente (sensitivities, snaps, threshold de impacto para ragdoll).
- Clasificar cada hallazgo por **destino**:
  - `.moodproj` (per-proyecto, vive en `ProjectSettings`)
  - `UserSettings` (per-instalación, vive en `%APPDATA%`)
  - Inspector (per-entidad, ya editable component-by-component)
  - `.moodmap` (per-mapa, hoy persiste algunos defaults de scene)
- Output: documento `docs/HARDCODED_AUDIT.md` con tabla:
  ```
  | file:line                            | valor       | uso                          | destino propuesto       | prioridad |
  | engine/physics/world/PhysicsWorld.cpp:42 | -9.81f  | gravity por default          | .moodproj > Physics     | media     |
  | engine/character/CharacterController.cpp:88 | 5.5f | walkSpeed por default        | .moodproj > Gameplay    | media     |
  | engine/saving/Autosave.cpp:N (no existe) | —     | autosave no existe aún       | UserSettings (F3H7)     | alta      |
  | editor/ui/EditorThemes.cpp:135         | 6/4/0     | rounding metrics             | hardcoded ok (estética) | descartar |
  ```
- **Sin código nuevo**. F3H3 NO mueve constantes a fields — solo las cataloga. La migración es F3H4-F3H7.

**NO en F3H3:**
- Refactor de ninguna constante. Si encuentro un valor que parece urgente, lo anoto en la tabla con prioridad "alta" — la migración va al hito que corresponda.
- Constantes de implementación interna (tamaños de buffer ImGui, offsets de struct, magic numbers de Jolt sin sentido para el usuario final). Solo lo que un usuario final querría tocar.
- Tuneo de los valores actuales. La auditoría documenta el estado, no propone nuevos defaults.

---

## Bloques

### A — Sweep de gameplay/physics
Recorrer:
- `src/engine/physics/**/*.cpp` — gravity, friction defaults, sleep thresholds, ragdoll impact thresholds.
- `src/engine/character/**/*.cpp` — speeds (walk/run/crouch), jump force, coyote window, jump buffer.
- `src/engine/game/state/GameState.cpp` — defaults de HUD/inventory/etc que afecten gameplay tunable.
- `src/systems/**/*.cpp` — autosave (si existe), trigger filters, particles defaults.

Output parcial: sección "Gameplay + Physics" de la tabla.

### B — Sweep de editor/UI
Recorrer:
- `src/editor/ui/**/*.cpp` — sensitivities (mouse look, gizmo drag), defaults de viewport (FOV, near/far), snap defaults, autosave interval (si existe).
- `src/editor/application/EditorApplication*.cpp` — defaults de comportamiento (auto-save on play? auto-reload on focus? estados que el dev querría togglear).
- `src/editor/panels/scene/ViewportPanel.cpp` y orto views — speeds de pan/zoom, defaults de grilla.

Output parcial: sección "Editor UX" de la tabla.

### C — Sweep de render/rendering settings
Recorrer:
- `src/engine/render/**/*.cpp` — defaults de shadow res, IBL prefilter mips, light grid params, post-fx defaults.
- Tocar lo que el dev querría editar **por proyecto** (calidad target), no lo que es decisión arquitectónica.

Output parcial: sección "Rendering" de la tabla.

### D — Catalogación + filtrado
- Para cada hit del sweep: ¿el usuario querría tocarlo? Si no → descartar (anotar como "implementación interna, no migrar").
- Para los que quedan: asignar destino (4 buckets) + prioridad (alta = afecta gameplay/UX directo / media = quality-of-life / baja = nice-to-have).
- Agrupar por destino para que F3H4-F3H7 sepan qué hito toma qué bucket.

### E — Output + cierre
- Escribir `docs/HARDCODED_AUDIT.md` con la tabla completa + sección "Resumen ejecutivo" (cuántos hits por bucket, recomendación de orden para F3H4+).
- Actualizar `PLAN_HITO_F3H4.md` (a crear en su cierre) con el bucket prioritario que F3H4 va a atacar primero.

---

## Decisiones tomadas (pre-implementación)

1. **Solo catalogación, sin migración.** F3H3 entrega un mapa, no cambios de comportamiento. La tentación de "ya que estoy, muevo este field a `ProjectSettings`" es contraria al principio "scope chico, no acoplar hitos". Cada constante se mueve en su hito.

2. **Filtro "el usuario querría tocarlo".** No todo número en el código es candidato. Buffer sizes de ImGui (256), offsets de UI (10px padding), magic numbers de Jolt sin sentido para usuario final → NO entran. Solo lo que un dev/jugador querría tunear.

3. **4 buckets de destino fijos.** `.moodproj` / `UserSettings` / Inspector / `.moodmap`. Si algo no encaja en ninguno → revisar si realmente es candidato a migrar.

4. **Sin docs nuevas en `docs/`** más allá del `HARDCODED_AUDIT.md`. Sin sub-páginas por bucket — la tabla única es el contrato con F3H4+.

---

## Riesgos / a confirmar temprano

- **Sweep masivo puede arrojar 200+ hits.** Mitigar con filtro estricto desde el primer pase (regla "¿el usuario lo tocaría?"). Si después del filtro siguen siendo 100+, agrupar buckets en sub-buckets y priorizar por hito.

- **Algunos hits van a estar en archivos compartidos editor/runtime.** Anotar el call-site real (no solo el define) — qué función lo consume importa para saber el destino.

- **Constantes de física pueden ser "ajustadas para feel" en cooperación con Jolt.** No todo lo que es número en Physics es candidato a migrar — algunos son tuneos delicados. Conservar el filtro.

---

## Tamaño estimado

Hito chico (auditoría pura, sin código). Tarea de lectura + clasificación, ~2-3h. Sin riesgo de regresión (no se toca código). El output es el mapa para F3H4+.

## Cierre del hito

- [ ] Suite verde (no se tocó código, suite sin cambios).
- [ ] `docs/HARDCODED_AUDIT.md` creado con tabla completa + resumen ejecutivo.
- [ ] Tag `v2.3.0-fase3-hito3`.
- [ ] Update `ESTADO_ACTUAL.md`, `HITOS.md`, `DECISIONS.md`. Crear `PLAN_HITO_F3H4.md` (primer hito de migración, con el bucket de prioridad alta del audit).
