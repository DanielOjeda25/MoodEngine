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
    test_scene_serializer_lighting_physics.cpp  # AUDIT-3 split
    test_scene_serializer_gameplay.cpp          # AUDIT-3 split
    test_environment_entity.cpp                 # F2H86
    test_ray.cpp                                # AUDIT-3 (pickRayFromNdc)
    test_project_serializer.cpp
    test_project_settings.cpp                  # F3H1
    test_user_settings_editor.cpp              # F3H7 (+F3H21/F3H22/F3H23 fields)
    test_hosek_wilkie.cpp                      # F3H31
    test_health.cpp                            # F4H1
    test_armor_component.cpp                   # F4H4
    test_pickup_system.cpp                     # F4H4
    test_projectile_system.cpp                 # F4H5
    test_weapon_asset.cpp                      # F4H2
    test_weapon_system.cpp                     # F4H2
    test_input_keybindings.cpp                 # F4H2 Bloque B
    test_editor_camera_lerp.cpp                # F3H21
    test_profiler_buffer.cpp                   # F3H23
    test_toasts.cpp                            # F3H24
    test_lock_file.cpp                         # F3H25
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
    test_i18n.cpp  # F2H43
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
    test_multi_edit_property_command.cpp  # F3H8
    test_f3h12_inspector_undo.cpp  # F3H12: bool/u32/string/vec4 round-trip
    test_asset_validator.cpp               # F3H18
    test_asset_ref_index.cpp               # F3H19
    test_rename_asset_command.cpp          # F3H19
    test_component_clipboard.cpp           # F3H9
    test_entity_type.cpp                   # F3H9 Stage 2
    test_add_component_command.cpp  # F2H45 Bloque A
    test_particle_system.cpp
    test_character_controller.cpp
    test_physics_constraints.cpp  # F2H65
    test_ragdoll_layout.cpp  # F2H66 Bloque A
    test_physics_ragdoll.cpp  # F2H66 Bloque H
    test_vehicle_config.cpp  # F2H67 Bloque A
    test_physics_vehicle.cpp  # F2H67 Bloque C/J
    test_vehicle_analyzer.cpp  # F2H82 Bloque A
    test_vehicle_writer.cpp  # F2H82 Bloque C
    test_physics_ragdoll_impact.cpp  # F2H68 Bloque D
    test_raycast.cpp
    test_trigger_system.cpp
    test_force_field_system.cpp  # F2H72
    test_cloth_layout.cpp  # F2H75 Bloque A
    test_physics_cloth.cpp  # F2H75 Bloque B
    test_save_load.cpp
    test_saveload_inventory.cpp        # break-A1+A2
    test_quest_no_script_entity.cpp    # break-A3
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
    test_edit_brush_face_material_command.cpp
    test_edit_brush_uv_command.cpp
    test_set_tile_command.cpp
    test_edit_mesh_renderer_material_command.cpp
    test_edit_script_component_command.cpp
    test_csg_pick_face.cpp
    test_compile_map.cpp
    test_compiled_mesh_persistence.cpp
    test_map_export_obj.cpp
    test_nodegraph.cpp  # F2H46
    test_shader_graph.cpp     # F2H62 Bloque G
    test_dialog_asset.cpp      # F2H47
    test_dialog_validator.cpp  # F2H47
    test_dialog_system.cpp        # F2H48
    test_dialog_script_host.cpp   # F2H48.1
    test_item_asset.cpp           # F2H51
    test_inventory_state.cpp      # F2H51
    test_item_pickup_system.cpp   # F2H52 Bloque C
    test_primitive_meshes.cpp     # F2H52 D investigacion: winding cubo
    test_lua_bindings_inventory.cpp  # F2H52 Bloque E
    test_item_spawn.cpp           # F2H52 Bloque H5
    test_quest_asset.cpp          # F2H53 Bloque A
    test_quest_system.cpp         # F2H53 Bloque C
    test_lua_bindings_quest.cpp   # F2H53 Bloque E

    ${CMAKE_SOURCE_DIR}/src/engine/world/csg/Brush.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/csg/BrushMesh.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/csg/BrushOps.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/csg/CompileMap.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/csg/MapExportObj.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/csg/Primitives.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/EditBrushMaterialCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/EditBrushFaceMaterialCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/EditBrushGeometryCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/EditBrushUVCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/SetTileCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/EditMeshRendererMaterialCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/EditScriptComponentCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/commands/PasteComponentCommand.cpp  # F3H9
    ${CMAKE_SOURCE_DIR}/src/editor/commands/RenameAssetCommand.cpp     # F3H19
    ${CMAKE_SOURCE_DIR}/src/editor/components/ComponentClipboard.cpp   # F3H9
    ${CMAKE_SOURCE_DIR}/src/engine/assets/validation/AssetValidator.cpp  # F3H18
    ${CMAKE_SOURCE_DIR}/src/engine/assets/refs/AssetRefIndex.cpp         # F3H19

    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/TilePersistence.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/project/MapsManager.cpp

    ${CMAKE_SOURCE_DIR}/src/engine/render/scene_renderer/RenderBatching.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/render/sky/HosekWilkie.cpp  # F3H31
    ${CMAKE_SOURCE_DIR}/src/engine/input/InputActions.cpp      # F4H2 Bloque B
    ${CMAKE_SOURCE_DIR}/src/editor/panels/scene/HierarchyCollect.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/workspace/WorkspaceManager.cpp

    # Incluimos las fuentes de las unidades testeadas directamente: ninguna
    # depende de GL, asi que no hace falta extraer una libreria solo para
    # testear. Cuando crezca, convertir a target `mood_core` compartido.
    ${CMAKE_SOURCE_DIR}/src/engine/scene/core/EditorCamera.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/core/FpsCamera.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/core/Scene.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/queries/ScenePick.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/VisGroup.cpp  # F2H33
    ${CMAKE_SOURCE_DIR}/src/engine/world/grid/GridMap.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/grid/Pathfinding.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/ai/NavSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/particles/ParticleSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/physics/PhysicsSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/physics/VehicleSystem.cpp  # F2H67 Bloque D
    ${CMAKE_SOURCE_DIR}/src/systems/scripting/ScriptSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/physics/TriggerSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/physics/ForceFieldSystem.cpp  # F2H72
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/exposed/ExposedProperty.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/bindings/LuaBindings.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/game/state/GameState.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/packaging/PackageBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/saving/SaveLoad.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/SceneLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/entity_type/EntityTypeTable.cpp  # F3H9
    ${CMAKE_SOURCE_DIR}/src/platform/VFS.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Audio.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Material.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Mesh.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Prefab.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Texture.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Dialog.cpp  # F2H48
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_AnimationClip.cpp  # F2H49
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Item.cpp  # F2H51
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Quest.cpp # F2H53 Bloque B
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Vehicle.cpp # F2H67 Polish 2
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Weapon.cpp  # F4H2
    ${CMAKE_SOURCE_DIR}/src/engine/assets/manager/AssetManager_Rename.cpp  # F3H19
    ${CMAKE_SOURCE_DIR}/src/engine/quest/QuestAsset.cpp                  # F2H53 Bloque A
    ${CMAKE_SOURCE_DIR}/src/engine/quest/QuestSystem.cpp                 # F2H53 Bloque C
    ${CMAKE_SOURCE_DIR}/src/engine/quest/QuestScriptHost.cpp             # break-A3
    ${CMAKE_SOURCE_DIR}/src/engine/assets/loaders/MeshLoader.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/loaders/MeshLoader_Animation.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/loaders/MeshLoader_Geometry.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/loaders/MeshLoader_Skeleton.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/loaders/MeshLoader_StandaloneClip.cpp  # F2H49
    ${CMAKE_SOURCE_DIR}/src/engine/assets/cache/LodCache.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/primitives/PrimitiveMeshes.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/EntitySerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/EntitySerializer_Parse.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/SceneSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/serialization/ProjectSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/project/ProjectSettings.cpp  # F3H1
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
    ${CMAKE_SOURCE_DIR}/src/engine/physics/world/PhysicsWorld_Constraints.cpp  # F2H66 Bloque 0
    ${CMAKE_SOURCE_DIR}/src/engine/physics/world/PhysicsWorld_Ragdoll.cpp  # F2H66 Bloque C
    ${CMAKE_SOURCE_DIR}/src/engine/physics/ragdoll/RagdollLayout.cpp  # F2H66 Bloque A
    ${CMAKE_SOURCE_DIR}/src/engine/physics/cloth/ClothLayout.cpp  # F2H75 Bloque A
    ${CMAKE_SOURCE_DIR}/src/engine/physics/world/PhysicsWorld_SoftBody.cpp  # F2H75 Bloque B
    ${CMAKE_SOURCE_DIR}/src/engine/physics/vehicle/VehicleConfig.cpp  # F2H67 Bloque A
    ${CMAKE_SOURCE_DIR}/src/engine/physics/vehicle/VehicleMeshAnalyzer.cpp  # F2H82 Bloque A
    ${CMAKE_SOURCE_DIR}/src/engine/physics/vehicle/VehiclePresets.cpp  # F2H82 Bloque C
    ${CMAKE_SOURCE_DIR}/src/engine/physics/vehicle/VehicleConfigWriter.cpp  # F2H82 Bloque C
    ${CMAKE_SOURCE_DIR}/src/engine/physics/vehicle/VehicleRenderOffset.cpp  # break-B1
    ${CMAKE_SOURCE_DIR}/src/engine/physics/world/PhysicsWorld_Vehicle.cpp  # F2H67 Bloque C
    ${CMAKE_SOURCE_DIR}/src/engine/physics/world/PhysicsWorld_Impact.cpp  # F2H68 Bloque A
    ${CMAKE_SOURCE_DIR}/src/core/i18n/I18n.cpp  # F2H43 (movido a core en AUDIT-3)
    ${CMAKE_SOURCE_DIR}/src/core/Toasts.cpp  # F3H24
    ${CMAKE_SOURCE_DIR}/src/core/UserSettings.cpp  # F2H43
    ${CMAKE_SOURCE_DIR}/src/editor/application/LockFile.cpp  # F3H25
    ${CMAKE_SOURCE_DIR}/src/engine/profile/ProfilerBuffer.cpp  # F3H23
    ${CMAKE_SOURCE_DIR}/src/engine/nodegraph/Graph.cpp  # F2H46
    ${CMAKE_SOURCE_DIR}/src/engine/shader_graph/ShaderGraphAsset.cpp     # F2H62
    ${CMAKE_SOURCE_DIR}/src/engine/shader_graph/ShaderGraphGenerator.cpp # F2H62
    ${CMAKE_SOURCE_DIR}/src/engine/dialog/DialogAsset.cpp      # F2H47
    ${CMAKE_SOURCE_DIR}/src/engine/dialog/DialogValidator.cpp  # F2H47
    ${CMAKE_SOURCE_DIR}/src/engine/dialog/DialogSystem.cpp     # F2H48
    ${CMAKE_SOURCE_DIR}/src/engine/inventory/ItemAsset.cpp     # F2H51
    ${CMAKE_SOURCE_DIR}/src/engine/inventory/InventoryState.cpp # F2H51
    ${CMAKE_SOURCE_DIR}/src/engine/inventory/ItemPickupSystem.cpp # F2H52
    ${CMAKE_SOURCE_DIR}/src/engine/inventory/InventoryHooks.cpp # F2H52 F
    ${CMAKE_SOURCE_DIR}/src/engine/inventory/ItemSpawn.cpp      # F2H52 H5
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/bindings/LuaBindings_Inventory.cpp # F2H52 E
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/bindings/LuaBindings_Quest.cpp # F2H53 E
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/bindings/LuaBindings_Ragdoll.cpp # F2H66 E
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/bindings/LuaBindings_Vehicle.cpp # F2H67 E
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/bindings/LuaBindings_Health.cpp # F4H1
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/bindings/LuaBindings_Armor.cpp  # F4H4
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/bindings/LuaBindings_Weapon.cpp # F4H2
    ${CMAKE_SOURCE_DIR}/src/engine/scripting/bindings/LuaBindings_Input.cpp  # F4H2 Bloque B
    ${CMAKE_SOURCE_DIR}/src/engine/dialog/DialogScriptHost.cpp # F2H48.1
    ${CMAKE_SOURCE_DIR}/src/engine/gameplay/Health.cpp # F4H1
    ${CMAKE_SOURCE_DIR}/src/engine/gameplay/pickup/PickupSystem.cpp # F4H4
    ${CMAKE_SOURCE_DIR}/src/engine/gameplay/projectile/ProjectileSystem.cpp # F4H5
    ${CMAKE_SOURCE_DIR}/src/engine/gameplay/weapon/WeaponSpec.cpp   # F4H2
    ${CMAKE_SOURCE_DIR}/src/engine/gameplay/weapon/WeaponSystem.cpp # F4H2
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
    SDL2::SDL2  # F4H2 Bloque B: InputActions usa SDL_GetKeyboardState/SDL_GetMouseState + scancodes
)

# WORKING_DIRECTORY = root del repo: los tests que tocan el disco (ej.
# test_mesh_asset cargando assets/meshes/pyramid.obj via assimp) resuelven
# paths logicos relativos a ese root.
add_test(NAME mood_tests COMMAND mood_tests
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
