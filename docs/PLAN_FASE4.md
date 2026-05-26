# PLAN FASE 4 — PANDEMONIUM (un boomer-shooter sobre MoodEngine)

> **Codename:** PANDEMONIUM *(working title — renombrable en cualquier momento).*
> **Qué es la Fase 4:** dejar de construir/pulir el motor y **hacer un juego con él**. Un
> boomer-shooter de arena, rápido y arcade (Doom / Serious Sam), donde **apuntar importa**.
> **Cambio de filosofía respecto a Fases 1-3:** hasta acá el motor (y su editor) eran el fin.
> Ahora el motor es el medio. **El juego tira de los requisitos del motor, no al revés.** Solo
> se construye tech nueva cuando un nivel o el combate la pide — no "por si acaso".
>
> **Prerrequisito:** esta fase arranca **después** de cerrar la Fase 3 (pulido / "nada
> hardcodeado"), tag `v3.0.0`. No empezar F4H1 antes. La Fase 3 deja un editor profesional y
> data-driven — la Fase 4 lo aprovecha (ver §3).

---

## 1. El juego en una frase

> Caés en una arena, hordas de enemigos vienen hacia vos, y la única salida es matarlos a
> todos con armas que se sienten contundentes y premian la puntería. Rápido, sangriento,
> sin recargar la paciencia.

**Pilares de diseño (lo que NO se negocia):**
1. **Game feel sobre todo.** Un disparo tiene que sentirse contundente: muzzle flash, hit
   marker, sonido seco, el enemigo reacciona, y al morir se desarma en ragdoll. Si el
   combate básico no se siente bien, nada más importa.
2. **Apuntar importa.** Hitscan preciso. Recompensar la puntería (daño por zona / headshots),
   no premiar rociar. Movimiento rápido del jugador para esquivar.
3. **Hordas, no tácticas.** Muchos enemigos tontos pero divertidos (máquina de estados
   simple), no pocos enemigos inteligentes. La presión viene de la cantidad y el ritmo.
4. **Ritmo arcade.** Sin recargas tediosas, sin menús en medio del combate, sin esperar.
   Entrar, pelear, pickups, siguiente oleada.

---

## 2. Principio rector: la compuerta del vertical slice

El alcance elegido es **mini-campaña (3-5 niveles)**, pero con una regla que protege contra
el "nunca termino":

> **El Nivel 1 es una compuerta go / no-go.** Antes de construir los niveles 2-5, se termina
> **un (1) nivel completo y pulido** con el loop de combate entero. Si ese nivel no es
> divertido, se para y se arregla el *feel* — NO se construyen más niveles sobre un combate
> que no funciona. Si es divertido, los niveles 2-5 son repetir contenido sobre sistemas ya
> probados.

Esto es exactamente cómo trabajaban Carmack y Romero: una cosa que funcione, después escalar.
El entregable de la Sub-fase 4.3 (F4H16) es un **demo jugable** real, no un prototipo interno.

---

## 3. Qué reutilizamos vs qué construimos

**Ya está hecho (Fases 1-3) — el juego se apoya en esto, NO se reconstruye:**
- Render PBR + IBL + sombras + post-process (el look ya está).
- **Ragdolls** → muerte de enemigos. Conectar, no construir.
- **Navegación A\*** → `world/grid/Pathfinding::findPath` + `systems/ai/NavSystem` YA existen →
  base del movimiento de enemigos (mover hacia el jugador está casi resuelto; lo nuevo es el
  *cerebro* de combate, no la navegación).
- **Raycast** → `PhysicsWorld::raycast()` YA existe → base del hitscan (lo nuevo es la lógica
  del arma encima del rayo, no el rayo).
- **Pickups + inventario** → munición, salud, armas como ítems.
- **Editor CSG (Hammer-style) + CompileMap** → diseño de las arenas.
- **Triggers + joints (hinge)** → puertas, switches, gatillos de oleadas.
- **Audio (miniaudio) + partículas** → SFX, muzzle flash, explosiones.
- **Controller FPS + cámara** → movimiento del jugador.
- **Save/load** → progresión entre niveles, armas que persisten.
- **Lua + hot-reload** → tuning de armas/enemigos sin recompilar.
- **GameOverlay** → base del HUD.
- **Infraestructura "nada hardcodeado" de Fase 3** → el balance del juego (daño, cadencia,
  salud de enemigos, velocidad) vive en Project Settings / `.moodproj` y assets data-driven, no
  en magic numbers. La Fase 4 hereda esto gratis: iterar el balance es editar, no recompilar.

**Tech nueva de la Fase 4 (el corazón del trabajo) — acotada a 3 sistemas:**
1. **Salud / daño** — `HealthComponent`, eventos de daño, muerte.
2. **Armas** — definición de arma, hitscan + proyectiles, daño, munición, cambio de arma.
3. **IA de enemigos** — máquina de estados (Idle→Alert→Chase→Attack→Pain→Dead) + navegación
   con el pathfinding existente + muerte por ragdoll.

Todo lo demás de la Fase 4 es **contenido y diseño** (niveles, encuentros, balance, audio),
no tecnología de motor.

---

## 4. Componentes y formatos nuevos

**Componentes (en `engine/scene/components/Components_Gameplay.h`):**
- `HealthComponent { f32 current, max; bool dead; f32 lastDamageTime; }`
- `WeaponInventoryComponent { vector<WeaponSlot> weapons; int activeIndex; }` (reusar el patrón
  de inventario donde se pueda).
- `ProjectileComponent { f32 damage, speed, splashRadius; u32 ownerEntity; }`
- `EnemyComponent { State state; f32 aggroRange, attackRange, moveSpeed; EnemyKind kind; }`
- `SpawnerComponent { vector<WaveDef> waves; bool triggered; }`
- `PickupComponent` (o extender `ItemComponent` existente) — qué da al tocarlo (salud/munición/arma).
- `DamageZoneComponent` opcional — para daño por zona/headshot.

**Formatos de asset nuevos (seguir la convención `.mood*`):**
- `.moodweapon` — definición de arma (daño, cadencia, hitscan/proyectil, munición, spread,
  sonidos, modelo, feedback). Editable en un panel del editor + tuneable por Lua.
- `.moodenemy` — definición de enemigo (salud, velocidad, tipo de ataque, daño, rangos,
  modelo/anim, qué dropea). Editable + tuneable por Lua.

Estos dos formatos hacen que armas y enemigos sean **data-driven**: agregar una escopeta o un
enemigo nuevo es un archivo, no código. Clave para iterar rápido el balance (y alineado con la
regla "nada hardcodeado" que dejó la Fase 3).

---

## 5. Roadmap (F4H1 → F4H28)

> Nomenclatura: **Fase 4, Hito N = F4HN.** Tag por hito: `v3.X.0-fase4-hitoN`, escalando a
> **`v4.0.0`** en el release público (F4H28). Un hito = un cierre con tests + doc + tag.

### Sub-fase 4.1 — Núcleo de combate *(¿se siente bien disparar?)*
- **F4H1** — Sistema de salud/daño. `HealthComponent`, evento de daño, estado muerto. Probado
  contra un "maniquí" estático que recibe daño y muere. Tests headless de daño/muerte.
- **F4H2** — Primera arma hitscan (escopeta o pistola): raycast contra física, aplica daño,
  feedback mínimo (decal + sonido + partícula de impacto). Apuntar y matar al maniquí.
- **F4H3** — Munición + cambio de arma + HUD de combate (salud, munición, arma activa) sobre
  GameOverlay. Recoger munición/arma vía pickups.
- **F4H4** — Armas de proyectil (rocket/plasma): proyectil físico, daño por impacto + splash
  (reusar force fields para la explosión).
- **F4H5** — **Game feel pass del combate.** Muzzle flash, hit marker, screen shake, pain
  flash del jugador, reacción de impacto. *Acá se decide si disparar es satisfactorio.*

### Sub-fase 4.2 — Enemigos *(¿es divertido pelear?)*
- **F4H6** — `EnemyComponent` + máquina de estados (Idle→Alert→Chase→Attack→Pain→Dead) con un
  enemigo de prueba. Sin movimiento aún. Tests de transiciones de estado.
- **F4H7** — Navegación: el enemigo persigue al jugador usando el pathfinding A* existente,
  esquiva obstáculos básicos.
- **F4H8** — Ataques del enemigo (melee + ranged con proyectil) que dañan al jugador.
- **F4H9** — **Muerte con ragdoll** + impulso direccional (vuela en dirección del disparo).
  Conectar el sistema de ragdolls ya construido. Partículas de muerte opcionales.
- **F4H10** — Sistema de spawn / oleadas (`SpawnerComponent`): hordas estilo Serious Sam
  disparadas por triggers de zona. Ritmo de oleadas.
- **F4H11** — 2-3 tipos de enemigo data-driven (`.moodenemy`): rusher melee, tirador a
  distancia, tanque lento. La variedad es lo que hace divertida la horda.

### Sub-fase 4.3 — Vertical slice: Nivel 1 *(la compuerta go/no-go)*
- **F4H12** — Blockout del Nivel 1 con el editor CSG (layout de arena, alturas, coberturas).
- **F4H13** — Encounter design del Nivel 1: colocar enemigos y oleadas, afinar el ritmo.
- **F4H14** — Pickups en el nivel + puertas/switches (hinge joints + triggers) + 1 secreto.
- **F4H15** — Condiciones de victoria/derrota + transición + pantalla de fin de nivel. Loop
  completo de principio a fin.
- **F4H16** — **Pulido total del Nivel 1**: iluminación, audio ambiente, materiales, segunda
  pasada de game feel. **ENTREGABLE: demo jugable público.** ← *Compuerta: ¿es divertido?
  Sí → seguir. No → arreglar el feel antes de cualquier nivel nuevo.*

### Sub-fase 4.4 — Campaña *(niveles 2-5)*
- **F4H17 / F4H18 / F4H19 / F4H20** — Niveles 2, 3, 4 y 5 (cada uno: blockout → encounter →
  pulido), introduciendo de a poco enemigos/armas nuevos para mantener la curva.
- **F4H21** — Progresión entre niveles: armas que persisten (usar save/load), secuencia
  lineal o hub.
- **F4H22** — Clímax / jefe final (opcional pero recomendado para un cierre con fuerza).

### Sub-fase 4.5 — Release + Open Source *(el momento Doom)*
- **F4H23** — Menú principal + opciones + pausa.
- **F4H24** — Balance pass de toda la campaña (dificultad, daño, ritmo, munición).
- **F4H25** — Audio pass (música por nivel, SFX consistentes, mezcla).
- **F4H26** — Performance pass con Tracy + packaging → **build distribuible** (ejecutable que
  un jugador abre y juega).
- **F4H27** — Playtest con gente real + bugfix de lo que rompa la experiencia.
- **F4H28** — **Release público + liberación del código.** Tag `v4.0.0`. El juego primero, el
  código abierto después — como hizo id con Doom.

---

## 6. Detalle de F4H1 (para arrancar cuando cierre la Fase 3)

**Objetivo:** un `HealthComponent` funcional y un maniquí que recibe daño y "muere", sin armas
todavía. Es el cimiento de todo el combate.

**Alcance:**
- `HealthComponent { f32 current = 100; f32 max = 100; bool dead = false; }` en
  `Components_Gameplay.h`. Serializar simétrico (Entity/Scene serializer) + sección de Inspector.
  Defaults (vida máxima, etc.) viven en Project Settings (regla "nada hardcodeado" de Fase 3).
- API de daño: una función libre `applyDamage(Scene&, Entity target, f32 amount, glm::vec3 dir)`
  que resta vida, clampea a 0, marca `dead` y emite un evento/log. Sin acoplar a armas.
- Binding Lua: `health.damage(entity, amount)`, `health.get(entity)`, `health.heal(...)` para
  poder probar desde consola/script con hot-reload.
- Un comando del editor o spawn de demo: un cubo "maniquí" con `HealthComponent`; al recibir
  daño cambia de color y al morir hace algo visible (por ahora: se desactiva o cae con física).
- **Tests headless** (`test_health.cpp`): daño normal, sobre-daño (clamp a 0), muerte una sola
  vez, heal, roundtrip de serialización.

**Restricciones:** no construir armas ni enemigos en este hito. Solo el sistema de salud y la
forma de aplicarle daño. Mantener la suite verde. Tag `v3.1.0-fase4-hito1`.

---

## 7. Decisiones técnicas mayores

- **Hitscan como camino principal de daño** (apuntar importa): la primitiva `PhysicsWorld::raycast()`
  ya existe; lo nuevo es la lógica del arma (disparo → rayo → daño → feedback). Proyectiles solo
  para armas específicas (rocket/plasma).
- **Daño por zona / headshot**: si la query de raycast puede devolver qué parte golpeó
  (capsule del enemigo), un multiplicador de daño por zona recompensa la puntería. Si resulta
  caro, queda para después — no bloquea el vertical slice.
- **Enemigos data-driven** (`.moodenemy`) y **armas data-driven** (`.moodweapon`): el balance
  se hace editando archivos + Lua hot-reload, no recompilando. Esto es lo que hace iterable un
  shooter (y aprovecha la infraestructura de Fase 3).
- **IA simple a propósito**: máquina de estados, no behavior trees ni navmesh. Doom y Serious
  Sam tienen enemigos "tontos" y son divertidísimos. Usar el A* que ya existe.
- **Muerte = ragdoll existente**: cero motor nuevo, máxima satisfacción. Es el "wow" gratis que
  ya tenés construido.
- **Animación de enemigos**: usar el importador Mixamo + skeletal animation ya hechos. Idle /
  walk / attack / pain como mínimo.

---

## 8. Guardarraíles anti-scope-creep *(importante, dado el historial)*

- **No agregar features de motor que el juego no esté pidiendo *ahora*.** Si surge la
  tentación ("estaría bueno un sistema de clima", "y si agrego multiplayer"), va a un BACKLOG
  de Fase 5, no a la Fase 4.
- **El vertical slice (F4H16) es sagrado.** No se construye el Nivel 2 hasta que el Nivel 1 sea
  divertido y esté pulido.
- **Contenido > sistemas.** A partir de F4H12 el trabajo es mayormente diseño de niveles y
  balance. Si un hito de contenido "necesita" un sistema nuevo grande, parar y cuestionar si el
  diseño se puede resolver con lo que ya hay.
- **Cada hito tiene que dejar algo jugable o testeable**, no solo "infraestructura".

---

## 9. Definición de "terminado" (quality bar)

- El juego abre desde un ejecutable empaquetado, sin tocar el editor.
- Un jugador que nunca vio el proyecto puede empezar, entender qué hacer, pelear y terminar.
- El combate se siente contundente (los playtesters de F4H27 lo confirman, no nosotros).
- Corre fluido en tu hardware objetivo (Ryzen 5 5600G / GTX 1660) — verificado con Tracy.
- El código se libera limpio, con README y licencia, al cerrar `v4.0.0`.

---

## 10. Esquema de tags

| Momento | Tag |
|---|---|
| Cada hito de feature | `v3.X.0-fase4-hitoN` |
| Vertical slice jugable (F4H16) | `v3.X.0-fase4-hito16` *(milestone: demo)* |
| Release público + open source (F4H28) | **`v4.0.0`** |

---

## 11. Próximo paso

1. Terminar la **Fase 3 (pulido)** en curso y cerrarla con `v3.0.0`.
2. Recién entonces, pasarle al agente la instrucción de arranque de F4H1 (sección 6).
3. Tras cada hito, volver a charlar el siguiente — sobre todo en la **compuerta F4H16**, donde
   se decide en serio si el juego es divertido antes de escalar.
