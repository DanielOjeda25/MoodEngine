# F2H52 — Handoff para tour visual en PC de escritorio

**Estado:** funcional, suite verde (849 tests / 9372 asserts). Falta solo el
**tour visual** (Bloque M) y el **cierre + tag** (Bloque N).

Última PR pusheada: **9 commits** desde `0ffa37a` (Bloque H1+H2) hasta
`0c8a4f8` (Bloque L).

## Setup en la otra PC

```bash
git pull
cmake --build build/debug --config Debug --target MoodEditor MoodPlayer
```

Si no hay `build/debug`, regenerar con CMake desde VS Code o:
```bash
cmake -B build/debug -S .
```

## Qué probar (orden recomendado)

### 1 — Levantar items del mundo (Bloque C+D)
1. Abrí el editor, cargá un proyecto con items en `assets/items/`.
2. Drag de un item desde **Item Browser** → **Viewport**. Aparece un cubo/modelo.
3. **Play Mode** (F5 o boton Play).
4. Caminá hasta el pickup (WASD). Debería aparecer `[E] Levantar <nombre>`.
5. Apretá **E** → item entra al inventario + notif `+ <nombre> x1` en HUD.

### 2 — Panel de inventario (Bloque H1-H4)
- Apretá **Tab** en Play Mode → panel a la derecha de la pantalla.
- Si la entity Player tiene `InventoryComponent.mode = FlatList` → lista vertical.
- Cambiá `mode` a `Grid2D` desde el Inspector + Play → grilla de celdas; drag entre slots.
- Cambiá a `EquipmentSlots` con `equipment_slots` definidos → lista nombrada; drag con tag_filter.

### 3 — Tooltip + Usar/Tirar (Bloque H5) ⭐ NUEVO
- Con Tab abierto, **hover** sobre un item → mini-cartel con nombre/descripción/tags/stats.
- **Click derecho** sobre el item → menucito con dos botones:
  - **Usar** → invoca el callback Lua `on_use(entity, path)`. Si tenés `inventory_demo.lua` cargado:
    - `health_potion` → HP +20 + se consume.
    - `iron_sword` → cambia objective ("Equipped"), NO se consume.
  - **Tirar** → item sale del inventario y aparece pickup en el mundo 1.8m enfrente. Se puede levantar de nuevo con E.

### 4 — Container split (Bloque I) ⭐ NUEVO
Necesita un script Lua que llame `hud.open_container(entity)`. Setup rápido:

1. Creá una entity nueva (ej. tag "chest"), agregale `InventoryComponent` + algunos items vía Inspector.
2. Creá un script de prueba `assets/scripts/test_container.lua`:
   ```lua
   local OPENED = false
   function onUpdate(self, dt)
       if not OPENED then
           -- Asumo que `self` es la chest. Si no, buscarla por otro camino.
           hud.open_container(self)
           OPENED = true
       end
   end
   ```
3. Pegale el script al cofre.
4. Play → Tab → debería aparecer **split: tu inv izquierda, cofre derecha**.
5. **Drag de un lado al otro** → transfiere 1 unidad. Si está lleno, no toca.

Para cerrar el split desde Lua: `hud.close_container()`.

### 5 — Renderer override (Bloque J) ⭐ NUEVO
Difícil sin un panel custom. Test rápido para verificar el opt-out:
1. En cualquier `onUpdate`, pegá:
   ```lua
   if not REGISTERED then
       inventory.set_renderer(function(player, container)
           -- nada, no dibuja
       end)
       REGISTERED = true
   end
   ```
2. Tab → el panel **NO** debería aparecer (el motor cedió el render).
3. Llamá `inventory.clear_renderer()` para volver al default.

### 6 — Demo completo con inventory_demo.lua
- Adjuntá `assets/scripts/inventory_demo.lua` a cualquier entity (tipo "manager").
- Auto-registra los 3 hooks. Asegurate que tus items tengan:
  - `health_potion.mooditem` con `description_literal` + stat `heal: 20`.
  - `iron_sword.mooditem` con tag `weapon` + stat `damage: 10`.
- En Play: levantá la pocion → tutorial hint en objective. Tab → right-click → Usar → HP sube +20.

## Qué reportar

- ¿Tooltip se sale de la pantalla en algún borde?
- ¿El menú Usar/Tirar responde bien al click?
- ¿"Tirar" deja el item en lugar raro (dentro de una pared, muy lejos, etc.)?
- ¿Container split tiene problemas de drag (item no transfiere, transferencia se pierde)?
- ¿Algún crash o glitch visual (texto encimado, panel mal posicionado)?

## Si todo OK → cerrar F2H52

Decime "todo OK" o "todo OK menos X". Yo cierro con:
- `docs/ESTADO_ACTUAL.md` actualizado.
- `docs/HITOS.md` con F2H52 done.
- `docs/PLAN_HITO_F2H53.md` skeleton para el siguiente.
- `docs/DECISIONS.md` con notas de las decisiones engine-grade.
- Tag `v1.40.0-fase2-hito52`.

## Commits incluidos en este push

```
0c8a4f8 feat(F2H52 Bloque L): sample inventory_demo.lua
3218b79 test(F2H52 Bloque K): tests para Bloques I + J
48007b0 feat(F2H52 Bloque J): inventory.set_renderer override engine-grade
b1dfb2b feat(F2H52 Bloque I): container split mode + hud.open_container
28f5961 test(F2H52 H7): tests headless del ItemSpawn helper
8c2e1a6 feat(F2H52 Bloque H5): tooltip hover + right-click Usar/Tirar
ba373d7 feat(F2H52 Bloque H3+H4): inventory_panel Grid2D + EquipmentSlots con drag
0ffa37a feat(F2H52 Bloque H1+H2): HUD widget inventory_panel + Tab toggle + FlatList mode
```
