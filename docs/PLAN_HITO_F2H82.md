# PLAN F2H82 — Sistema de importación de vehículos (modal + autodetección de ruedas)

**Estado:** Planeado (abierto al cerrar F2H81, ampliado 2026-05-22 a pedido del dev).
**Predecesor:** F2H81.
**Origen:** el dev pidió, en vez de configurar 2 autos a mano, **un sistema** para importar cualquier modelo y que detecte ruedas/chasis solo. Decisiones tomadas: **modal de importación en el editor** con **preview 3D** + form de física; números físicos vía **presets por tipo + ajuste fino**.

---

## Qué siente el usuario

Un modal **"Importar vehículo"**:
1. Elige un modelo (`.glb`/`.fbx`).
2. El motor lo **analiza**: detecta ruedas vs chasis, ubica cada rueda (FL/FR/RL/RR) y mide la geometría.
3. Ve un **preview 3D** del auto con las ruedas marcadas; puede **corregir** si detectó mal.
4. Elige un **tipo** (deportivo / sedán / camioneta / blindado) que precarga peso/motor/torque/freno realistas, y **ajusta** lo que quiera.
5. **Guarda** → queda un `.moodvehicle` en `assets/vehicles/<nombre>/`, conducible como el DeLorean.

Importar un auto pasa de "editar JSON + correr scripts python + medir a mano" a **un modal de 1 minuto**.

---

## Realidad técnica (qué sí / qué no)

- **Autodetectable de la malla**: ruedas vs chasis, rol FL/FR/RL/RR (por posición), dimensiones del chasis, radio/ancho de rueda, track, wheelbase (offset_z), centro de masa aproximado, yaw forward.
- **NO está en la malla**: peso, HP, torque, freno → presets por tipo + ajuste fino.
- **Importador asistido** (no magia): detecta y propone, el dev confirma/corrige en el modal. Los GLB del mundo real varían mucho (ruedas pegadas, llanta separada del neumático, nombres en cualquier idioma).

---

## Bloques

### A — `VehicleMeshAnalyzer` (backend, headless, testeable)
`engine/physics/vehicle/VehicleMeshAnalyzer.{h,cpp}`. Carga el modelo con assimp (ya es dependencia vía MeshLoader), camina los nodos, computa AABB/centroide por nodo en un espacio común.
- **Detección de ruedas**: heurística combinada — por nombre (`wheel`/`rueda`/`tire`/`tyre`/`rim`/`llanta` + patrones FL/FR/RL/RR/`b_t_*`/`f_t_*`) **y/o** por geometría (nodos chicos, Y bajo, cerca de las 4 esquinas, tamaño similar, ~cilíndricos).
- **Clasificación FL/FR/RL/RR**: por signo de X (izq/der) y Z (frente/atrás) del centroide en espacio físico (tras detectar forward).
- **Medición**: dimensiones (AABB total), radio de rueda (AABB de la rueda), track (|X| izq-der), wheelbase (Z frente-atrás), centro de masa, `meshYawOffsetDeg` sugerido.
- **Salida**: `VehicleAnalysis { ChassisInfo, vector<WheelDetection>{nodeName, role, center, radius}, dims, suggestedYaw, confidence }`.
- **Tests headless**: dado un layout de nodos conocido (4 ruedas en esquinas + chasis), la clasificación es correcta. ESTA es la parte cubrible por el test suite.

### B — Procesamiento de ruedas (decisión técnica clave)
Las ruedas deben **rotar alrededor de su hub** (hoy `split_wheels.py` las centra y renombra a `wheel_*`; el `VehicleSystem` ya renderiza submeshes `wheel_*` centrados + auto-spawnea wheel-entities). Dos caminos para el importador:
- **(B1) Re-exportar la malla procesada** (centrar nodos rueda + renombrar a `wheel_*`) a un GLB nuevo con assimp. **Reusa el path de engine actual sin tocarlo** (riesgo en engine = 0; riesgo en fidelidad de export assimp = medio).
- **(B2) Centrar en runtime**: guardar en el config el nombre de submesh + offset del hub por rueda; el `VehicleSystem` resta el offset al renderizar. No reexporta (riesgo export = 0; toca el render path del vehículo).
- **Decisión**: arrancar por **B1** (reusa lo que ya anda); si el export de assimp da problemas de materiales/texturas, fallback a B2. Confirmar feasibility temprano.

### C — Writer de `.moodvehicle` + presets por tipo
- **Writer** `VehicleConfig → JSON` (hoy solo hay reader). Schema v2 ya definido (ver `delorean_dmc12.moodvehicle`).
- **Presets** (tabla en código): deportivo / sedán / camioneta / blindado → mass, HP/torque, brakes, damping, friction, steer angle realistas. Escalables por tamaño del modelo.

### D — Modal "Importar vehículo" (UI)
- Entry: botón **"+ Importar"** en el tab Vehículos del Asset Browser (o menú).
- File picker `.glb`/`.fbx` (reusa el path "Desde archivo del SO" existente).
- Al elegir → corre el analyzer → muestra: **preview 3D** (reusa `MeshThumbnailRenderer` o preview vivo), lista de **ruedas detectadas** con dropdown de rol (corregible) + chasis.
- **Form de física**: dropdown de tipo (preset) + campos editables (peso, HP, torque, freno, etc.).
- **Guardar** → escribe `.moodvehicle` (+ malla procesada B1) a `assets/vehicles/<nombre>/`, dispara rescan del Asset Browser.

### E — Validación: importar los 2 autos del backlog
Importar **`armor-car`** (ruedas `b_t_*`/`f_t_*`) y **`tesla`** (ruedas `RUEDRA_*`) por el modal nuevo. Confirmar: detección correcta, preview OK, se manejan (acelera/dobla/frena, ruedas rotando). Estos 2 son el caso de prueba real del sistema.

---

## Decisiones tomadas (pre-implementación)
- **Modal en editor** (no tool python offline) — el dev quiere "insertar acá".
- **Importador asistido** (detecta + confirma) — no auto-100% por la variabilidad de GLBs reales.
- **Presets por tipo + ajuste fino** para la física (no está en la malla).
- **B1 (re-export) primero**, B2 (runtime centering) como fallback.

## Riesgos / a confirmar temprano
- Fidelidad del export GLB de assimp (materiales/texturas embebidas) → decide B1 vs B2.
- Robustez de la heurística de detección en modelos raros → el paso de confirmación del modal lo cubre (el dev corrige).
- Tamaño del hito: es feature grande (5 bloques). Posible cierre parcial (A+B+C+D core) con polish en seguimiento.
