# Arquitectura de MoodEngine

> Documento vivo. La fuente de verdad detallada es `MOODENGINE_CONTEXTO_TECNICO.md`. Aquí se resume lo esencial para alguien que llega por primera vez al repo.

## Visión

MoodEngine es un motor gráfico 3D propio con editor visual integrado, escrito en C++17. Objetivo: juegos FPS como primer caso de uso, RPG más adelante. Calidad visual objetivo: AAA 2010-2015.

## Principios

- **Separación estricta entre editor y runtime.** El runtime puede correr sin el editor.
- **Separación estricta entre motor y juego.** El motor expone una API; el juego la consume.
- **Una sola aplicación, dos modos.** `MoodEditor.exe` corre en Editor Mode o Play Mode.
- **Abstracción de API gráfica (RHI).** Todo fuera de `src/engine/render/opengl/` es agnóstico de OpenGL.
- **Entity-Component-System** como modelo de escena (EnTT detrás de una fachada propia).

## Capas (de baja a alta)

1. **Núcleo:** logging, math, tiempo, tipos.
2. **Plataforma:** SDL2, filesystem, VFS.
3. **Backend OpenGL:** GLAD, shaders concretos.
4. **Renderer:** RHI abstracto.
5. **Escena y entidades:** fachada sobre EnTT.
6. **Sistemas de juego:** physics, audio, input, AI.
7. **Editor:** ImGui, herramientas, paneles.
8. **Juego:** scripts Lua y assets del usuario.

**Regla:** una capa solo depende de capas iguales o inferiores.

## Loop principal

```
while running:
    processEvents()
    if Play Mode: scene.update(dt); physics.step(dt); audio.update(dt)
    else:         editor.update(dt)
    renderer.renderScene(scene, camera)
    if Editor Mode: editor.drawUI()
    present()
```

## Extensiones de archivo propias

- `.moodproj` — proyecto
- `.moodmap` — mapa / escena
- `.moodprefab` — plantilla reutilizable de entidad

## Estado actual (Hito 1)

Solo la capa 1 (núcleo mínimo: logging + tiempo) y capa 2 (ventana SDL2 con contexto OpenGL 4.5) están implementadas, más el shell de UI del editor con ImGui docking. Sin renderer, sin escena, sin assets.
