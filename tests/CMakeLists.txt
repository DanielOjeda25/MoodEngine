# Test target para el motor. Compila las unidades puras que queremos verificar
# (por ahora solo la matematica de camaras) + un runner minimo con doctest.
#
# Ejecutar con: ctest --test-dir build/debug -C Debug

add_executable(mood_tests
    test_math.cpp
    test_cameras.cpp
    test_aabb.cpp
    test_grid.cpp
    test_collision.cpp
    test_vfs.cpp
    test_asset_manager.cpp
    test_json_helpers.cpp
    test_scene_serializer.cpp
    test_project_serializer.cpp
    test_scene.cpp
    test_lua.cpp
    test_lua_bindings.cpp
    test_audio.cpp
    test_mesh_asset.cpp
    test_lighting.cpp
    test_picking.cpp
    test_prefab_serializer.cpp
    test_fog.cpp
    test_shadow_proj.cpp
    test_pbr_brdf.cpp
    test_material_serializer.cpp
    test_light_grid.cpp
    test_animation.cpp
    test_game_state.cpp
    test_package_builder.cpp
    test_nav_system.cpp
    test_pathfinding.cpp
    test_scene_loader.cpp
    test_exposed_properties.cpp
    test_history_stack.cpp
    test_edit_transform_command.cpp
    test_delete_entity_command.cpp
    test_create_entity_command.cpp
    test_edit_property_command.cpp
    test_particle_system.cpp
    test_character_controller.cpp
    test_raycast.cpp
    test_trigger_system.cpp
    test_save_load.cpp
    test_frustum.cpp
    test_render_batching.cpp
    test_hierarchy_collect.cpp
    test_lod.cpp
    test_workspace_manager.cpp
    test_save_load_full_roundtrip.cpp
    test_tile_persistence.cpp
    test_maps_manager.cpp
    test_plane.cpp
    test_csg_brush.cpp
    test_brush_persistence.cpp
    test_csg_brush_ops.cpp
    test_selection_set.cpp
    test_csg_primitives.cpp
    test_edit_brush_material_command.cpp
    test_edit_brush_uv_command.cpp
    test_set_tile_command.cpp
    test_edit_mesh_renderer_material_command.cpp

    ${CMAKE_SOURCE_DIR}/src/engine/world/csg/Brush.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/csg/BrushMesh.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/csg/BrushOps.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/csg/Primitives.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/EditBrushMaterialCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/EditBrushUVCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/SetTileCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/EditMeshRendererMaterialCommand.cpp

    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/TilePersistence.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/project/MapsManager.cpp

    ${CMAKE_SOURCE_DIR}/src/engine/render/scene_renderer/RenderBatching.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/panels/scene/HierarchyCollect.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/workspace/WorkspaceManager.cpp

    # Incluimos las fuentes de las unidades testeadas directamente: ninguna
    # depende de GL, asi que no hace falta extraer una libreria solo para
    # testear. Cuando crezca, convertir a target `mood_core` compartido.
    ${CMAKE_SOURCE_DIR}/src/engine/scene/core/EditorCamera.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/core/FpsCamera.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/core/Scene.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/queries/ScenePick.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/grid/GridMap.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/grid/Pathfinding.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/ai/NavSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/particles/ParticleSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/physics/PhysicsSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/scripting/ScriptSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/physics/TriggerSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/exposed/ExposedProperty.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/bindings/LuaBindings.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/game/state/GameState.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/packaging/PackageBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/saving/SaveLoad.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/SceneLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/platform/VFS.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/loaders/MeshLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/cache/LodCache.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/primitives/PrimitiveMeshes.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/EntitySerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/SceneSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/ProjectSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/PrefabSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/audio/clips/AudioClip.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/audio/device/AudioDevice.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/audio/device/miniaudio_impl.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/audio/AudioSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/light/LightSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/pipeline/LightGrid.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Log.cpp
    ${CMAKE_SOURCE_DIR}/src/core/LogRingSink.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/HistoryStack.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/EditTransformCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/DeleteEntityCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/CreateEntityCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/physics/world/PhysicsWorld.cpp
)

# AssetManager.cpp usa spdlog (via core/Log). JsonHelpers requiere nlohmann_json.
target_link_libraries(mood_tests PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    EnTT::EnTT
    lua_static
    sol2::sol2
    miniaudio
    assimp::assimp
    Jolt
    meshoptimizer
)

target_include_directories(mood_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(mood_tests PRIVATE
    doctest::doctest
    glm::glm
)

# WORKING_DIRECTORY = root del repo: los tests que tocan el disco (ej.
# test_mesh_asset cargando assets/meshes/pyramid.obj via assimp) resuelven
# paths logicos relativos a ese root.
add_test(NAME mood_tests COMMAND mood_tests
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
