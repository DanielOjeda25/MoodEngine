# PLAN F2H82 — Integrar autos del backlog: `armor-car` + `tesla`

**Estado:** Planeado (abierto al cerrar F2H81, 2026-05-22).
**Predecesor:** F2H81 (preview animaciones + Asset Browser visual + Inspector + auditoría).
**Origen:** backlog de vehículos (movidos a `assets/vehicles/<nombre>/` en F2H70.4, geométricamente válidos pero sin `.moodvehicle` ni ruedas procesadas). El dev pidió integrarlos "ya que estamos tocando autos".

---

## Qué siente el usuario

Dos autos más conducibles en el catálogo de Vehículos, que se manejan como el
DeLorean (ruedas que giran/rotan independientes, física de peso/motor/freno
**con datos reales** de cada vehículo).

---

## Alcance

Replicar para cada auto lo que F2H70.3/70.4 hizo para el DeLorean:

### 1. Procesar ruedas (`tools/glb/split_wheels.py`)
Ambos GLB tienen las ruedas como nodos separados con nombres no-canónicos:
- **armor-car**: `b_t_l` / `b_t_r` / `f_t_l` / `f_t_r` (back/front tire left/right).
- **tesla**: `RUEDRA_TRASERA_IZQUIERDA` / `..._DERECHA` / `..._DELANTERA_IZQUIERDA` / `..._DERECHA`.

`split_wheels.py` clasifica **por posición** (no por nombre) → centra cada rueda
en su hub y renombra a `wheel_FL/FR/RL/RR`. Verificar `--yaw` correcto por auto.
Backup del GLB original (`.bak`) como con el DeLorean.

### 2. Crear el `.moodvehicle` con datos reales
El agente investiga specs públicas reales (decisión del dev: "yo investigo specs
reales") y **muestra los números al dev antes de aplicar**:
- **tesla**: Tesla Model 3/S (peso ~1.6-2.1 t, ~283-1020 HP según versión, torque,
  freno regenerativo + disco).
- **armor-car**: blindado real tipo Lenco BearCat (muy pesado ~8-10 t, lento, motor
  diésel, freno acorde).

Medir del mesh: dimensiones, `mass_center_override`, ejes (`offset_z`, `track`),
radio/ancho de rueda, `mesh_yaw_offset_deg` (cada GLB puede venir en otra
convención forward).

### 3. Probar en editor
Spawn de cada auto, montar y conducir (acelera/dobla/frena), ruedas rotando, sin
crasheos. Validación visual del dev.

---

## Notas
- Specs: el agente investiga, muestra antes de aplicar.
- Tooling ya existe: `tools/glb/split_wheels.py`, `reorient.py`, `scale.py`, `verify.py`.
- El sistema de vehículos (`VehicleSystem` + `VehicleConfig` + auto-spawn de
  wheel-entities) ya está completo desde F2H70.4 — esto es **solo asset pipeline +
  config**, sin código de engine nuevo (salvo que aparezca un bug).
