# Assets de viewmodel FPS (brazos + arma) — Fase 4 (PANDEMONIUM)

> **Qué es esto:** la lista curada de modelos gratis (preferentemente **CC0**) para el
> viewmodel de primera persona del juego — los brazos que ves en pantalla + el arma. Se usan
> a partir de **F4H2/F4H3** (sistema de armas + HUD). Documento preparado por adelantado para
> que cuando llegues a esa parte sea solo bajar el `.glb` y soltarlo en
> `assets/meshes/viewmodel/`.
>
> **Importante (no se pudo automatizar la descarga):** los modelos están detrás del botón de
> descarga de itch.io / Sketchfab, que pide aceptar términos / login. La descarga la hacés vos
> (es un clic). Acá está exactamente qué bajar, de dónde, con qué licencia y dónde ponerlo.
>
> **Tu motor ya puede importarlos:** assimp carga GLB/GLTF/FBX/OBJ, hay carga de esqueleto y de
> clips de animación, y se maneja la escala estilo Mixamo. Formato preferido: **GLB**.

---

## Recomendación principal — Brazos

### ✅ PSX First Person Arms — Drillimpact  *(elegido)*
- **URL:** https://drillimpact.itch.io/psx-first-person-arms-free
- **Licencia: CC0** (dominio público — confirmado por el autor en los comentarios de la página:
  *"Yes CC0"*, *"free for commercial use, no credit required"*). Ideal para abrir el código.
- **Formatos:** FBX / **GLB** / Blend. Tamaño ~1.1 MB.
- **Incluye:** brazos riggeados + animaciones genéricas: `Combat_idle_start/loop`,
  `Combat_punch_left/right`, `Magic_spell_*`, `Relax_hands_idle_*`, `Collect_something`.
- **Estilo:** PS1/PSX low-poly retro → calza perfecto con la estética boomer-shooter (Doom).
- **Cómo bajarlo:** *Download Now* → "Name your own price" → podés poner **0** → descargás el
  `.zip` → extraés → usás el `.glb`.
- **Caveat conocido (de los comentarios):** el rig tiene un hueso de antebrazo oculto y el **IK
  puede romperse** en algunos motores; la **animación directa de huesos / clips bakeados
  funciona bien**. Como tu motor reproduce clips de animación bakeados (no resuelve IK en
  runtime), esto no debería afectarte — pero verificá las animaciones al importar.

> **Ojo:** estas animaciones son de **manos genéricas**, NO traen "disparar" ni "recargar"
> específicas de arma. Es una **base** excelente; las poses de arma se suman aparte (ver abajo).

---

## Recomendación — Arma (primer arma del juego)

Para mantener el proyecto **limpio de licencias al liberarlo**, conviene un arma **CC0**:

### Opción A (CC0, recomendada para open-source)
- **Firearms_Kit 1.0 (CC0)** — Sketchfab
  - URL: https://sketchfab.com/3d-models/firearms-kit--10-cc0-386f0582c28d4dff96da967f42ba20f7
  - Licencia: **CC0**. Kit de armas para prototipar/concept. Verificá formato de descarga (GLTF).
- **Free FPS Weapons** (colección de ThatJamGuy) — Sketchfab
  - URL: https://sketchfab.com/ThatJamGuy/collections/free-fps-weapons-c300195480644a1284e25cfb0bfdfb3f
  - Armas FPS animadas gratis. **Revisá la licencia de cada modelo** en su página (puede ser
    CC-BY = requiere crédito).

### Opción B (match de estilo perfecto, pero PAGA / no CC0)
- **Retro Low poly Shotgun & Ammo boxes** — Drillimpact (mismo autor que los brazos)
  - URL: https://drillimpact.itch.io/psx-shotgun-and-ammo-boxes
  - **Precio mínimo $2.99 USD.** Uso comercial OK, sin crédito requerido, **pero NO es CC0.**
  - Remington 870 retro + cajas de munición, 3 variantes de textura, FBX/GLB/Blend.
  - Una escopeta es el arma ideal para el Nivel 1. Si te cierra pagar un asset y no te importa
    que ese archivo no sea CC0, es el match de estilo perfecto. Si querés todo CC0, usá Opción A.

---

## Animaciones de arma (disparar / recargar / equipar)

El pack de brazos NO las trae. Tres caminos, de menos a más esfuerzo de licencia:
1. **Autorarlas vos en Blender** (recomendado para estilo retro): son poses simples (retroceso
   al disparar, bombeo de escopeta, equip). Pocos keyframes. Mantenés todo CC0/propio.
2. **MoCap Online — Free Pistol Animation Pack** (itch.io): 20+ animaciones de pistola, FBX.
   URL: https://mocaponline.itch.io/free-pistol-animation-starter-pack — revisá su licencia.
3. **Sketchfab — Cransh** (`Animated FPS hands (rifle animation pack)` / `FPS pistol animations`):
   animaciones listas, **probablemente CC-BY** (requieren crédito). Revisá la licencia en la
   página antes de usarlas en el release.

---

## Dónde colocar los archivos

```
assets/meshes/viewmodel/
  arms.glb           ← PSX First Person Arms (CC0)
  weapon_shotgun.glb ← arma elegida (idealmente CC0)
  CREDITS.md         ← crear: lista de assets + autor + licencia + URL (ver abajo)
```

Seguir `docs/asset_conventions.md`: **1 unidad = 1 metro, +Y arriba, +Z adelante**. Al importar,
verificá la escala (los brazos low-poly suelen venir bien; assimp aplica `GlobalScale` para los
exportados en cm estilo Mixamo). Si el viewmodel se ve gigante o diminuto, es escala del export.

---

## Higiene de licencias (para cuando abras el código)

- Crear `assets/meshes/viewmodel/CREDITS.md` con cada asset: nombre, autor, licencia, URL.
- CC0 no obliga a dar crédito, pero **darlo igual** es buena onda y los autores lo aprecian
  (Drillimpact pide opcionalmente un tag a @Drillimpact_dev en X).
- Si terminás mezclando un asset no-CC0 (ej. la escopeta paga), anotá su licencia aparte y
  asegurate de que sus términos permitan redistribuir el modelo dentro de un repo público
  (la escopeta de Drillimpact permite uso comercial; confirmá redistribución antes de subirla).
- Patrón usual y limpio: **código MIT/GPL + assets cada uno con su licencia documentada.**

---

## Integración en la Fase 4 (resumen — detalle en `PLAN_FASE4.md`)

- **F4H2/F4H3:** el viewmodel (brazos + arma frente a la cámara) es parte de construir el
  sistema de armas y el HUD. Se renderiza en un pass de primera persona (encima de la escena).
- **Animación:** usar el sistema de clips/skeletal ya existente para `idle` y, cuando las tengas,
  `fire`/`reload`. El loader de clips standalone (`MeshLoader_StandaloneClip`) permite sumar
  animaciones desde archivos separados.
- **Test de humo previo (se puede hacer ya, sin sistema de armas):** importar `arms.glb` como un
  mesh normal en una escena, ver que carga, que las animaciones se reproducen y que la escala es
  correcta. Eso valida el pipeline de import antes de F4H2.
