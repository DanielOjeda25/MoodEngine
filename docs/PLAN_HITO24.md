# Plan — Hito 24: TBD (candidatos por priorizar)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 23 cerrado).
>
> **Estado:** **TBD**. El Hito 23 cerró en `v0.23.0-hito23` con AI/pathfinding básico (A* + NavAgent + demo enemigo + fix general de orientación Z-up). Antes de empezar el Hito 24, conversar con el dev qué línea continúa.

---

## Candidato A: Polish del NavAgent + más AI

**Por qué importa:** el Hito 23 dejó la base, pero el "enemigo se mueve robóticamente sin rotar" — es un detalle visible que arruina la inmersión. Pequeños fixes acá hacen el demo MUCHO más vendible.

**Bloques tentativos:**
- **A1 — Rotación hacia el movimiento**: en `NavSystem::update`, después de calcular `dir`, setear `tf.rotationEuler.y = atan2(...)` con la fórmula correcta para que el modelo mire hacia donde camina. Smoothing opcional (lerp del yaw para no snappear).
- **A2 — Diagonales (8-connected) + corner-cutting check**: paths más naturales sin zigzag.
- **A3 — Estados básicos del agente**: `idle / chase / arrived`. NavAgent ya implícitamente los tiene (queda quieto cuando llega), pero declararlo explicito con un `state` enum permite triggers más complejos.
- **A4 — Multiple agents performance**: spawn de 8-16 enemigos para profile. Si el A* throttle (0.5s) escala bien, perfecto; sino mitigar con jump point search o cache compartido.

**Riesgos:** la fórmula del yaw depende del axis "forward" del modelo (varía por modelo). Probable que necesite un campo `MeshAsset::forwardAxis` o convención fija (todos los modelos importados se asumen forward = -Z).

---

## Candidato B: Mini editor de scripts in-place (arrastrado del Hito 22)

Bloque 5 stretch del Hito 22 que se difirió. Hoy el dev edita los `.lua` en VS Code y el hot-reload los levanta. Aceptable pero fricción.

**Bloques tentativos:**
- **B1 — Evaluación de `ImGuiColorTextEdit`** (license, tamaño).
- **B2 — `ScriptEditorPanel`**: nuevo panel registrado en `EditorUI`. Click en script del Asset Browser → abre acá.
- **B3 — Save con Ctrl+S**: escribe + dispara hot-reload.
- **B4 — Error highlighting**: la línea del último `lastError` en rojo.

---

## Candidato C: Asset pack realista (Quaternius / Sketchfab)

**Por qué importa:** el dev pidió esto explícitamente: "que podamos probar físicas, pero más adelante, sigue con cerrar el hito". Source assets descartados (legales + formatos turbios). Alternativas viables:
- **Quaternius** (CC0): packs de personajes humanoides + props industriales (cajas, tubos, mobiliario, vehículos).
- **Sketchfab** (CC0/CC-BY): escenarios completos (interiores, exteriores).
- **Polyhaven** (CC0): props alta calidad.
- **Mixamo**: humanoides con mocap (Adobe-owned pero free).

**Bloques tentativos:**
- **C1 — Curado**: descargar 8-12 props CC0 representativos (3-4 personajes humanoides con animación + 5-8 props físicos + 1 escenario completo).
- **C2 — Smoke tests**: dropear cada uno al editor para verificar pipeline (orientación, escala, animación).
- **C3 — Demo "playground físico"**: spawnea N propios cayendo desde altura sobre un escenario. Test stress de Jolt + render con muchos materiales.
- **C4 — Documentar en `assets/CREDITS.md`** las atribuciones requeridas por las licencias.

**Riesgos:** los assets más grandes pueden ser GLBs de varios MB cada uno. Repo crece. Considerar Git LFS si pesa > 50 MB.

---

## Candidato D: Save / load de gameplay

Schema `.moodsave` (snapshot del runtime: player position, HUD, scripts state, entity overrides). Hotkeys F5/F9 quick save/load. Lua API `save.write/load`. Detalle en `PLAN_HITO22.md` original (entrada Candidato D).

---

## Candidato E: Exposed properties Lua

Sintaxis declarativa `--[[exposed]] local speed = 5.0` + persistencia per-entity en `.moodmap` + Inspector dinámico. Detalle en `PLAN_HITO22.md` original.

---

## Decisión recomendada

Sin más contexto del dev, mi sugerencia:

1. **Candidato A (Polish AI)**: cierra el ciclo del Hito 23 con una experiencia visualmente mejor. El "robotic enemy" lo nota cualquiera. Bloques chicos.
2. **Candidato C (Asset pack)** como Hito siguiente o intercalado: enriquece los demos sin tocar engine. Trabajo en paralelo factible.
3. **Candidato B (mini editor)** si el dev valora cerrar la línea de scripting.
4. **Candidato D (save/load)** si el dev quiere "primer juego shippeable" — sin save no hay juego real.
5. **Candidato E (exposed properties)** después de A o D.

---

## Antes de arrancar

Cuando se elija el candidato:
1. Borrar las secciones que no apliquen y dejar solo el plan del candidato elegido.
2. Desglosar cada bloque con criterios de aceptación medibles.
3. Identificar dependencias externas nuevas.
4. Estimar bloques en sesiones (~2-4 hs cada uno).
5. Listar pendientes arrastrados de Hitos 21/22/23 que se podrían atacar de paso (ver `HITOS.md`).
