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

    # Incluimos las fuentes de las unidades testeadas directamente: ninguna
    # depende de GL, asi que no hace falta extraer una libreria solo para
    # testear. Cuando crezca, convertir a target `mood_core` compartido.
    ${CMAKE_SOURCE_DIR}/src/engine/scene/EditorCamera.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/FpsCamera.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/world/GridMap.cpp
    ${CMAKE_SOURCE_DIR}/src/systems/PhysicsSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/platform/VFS.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/assets/AssetManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Log.cpp
    ${CMAKE_SOURCE_DIR}/src/core/LogRingSink.cpp
)

# AssetManager.cpp usa spdlog (via core/Log). JsonHelpers requiere nlohmann_json.
target_link_libraries(mood_tests PRIVATE
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

target_include_directories(mood_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(mood_tests PRIVATE
    doctest::doctest
    glm::glm
)

add_test(NAME mood_tests COMMAND mood_tests)
