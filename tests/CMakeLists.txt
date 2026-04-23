# Test target para el motor. Compila las unidades puras que queremos verificar
# (por ahora solo la matematica de camaras) + un runner minimo con doctest.
#
# Ejecutar con: ctest --test-dir build/debug -C Debug

add_executable(mood_tests
    test_math.cpp
    test_cameras.cpp
    test_aabb.cpp

    # Incluimos las fuentes de las camaras directamente: son pequenas y no
    # dependen de GL, asi que no hace falta extraer una libreria solo para
    # testear. Cuando crezca, convertir a target `mood_core` compartido.
    ${CMAKE_SOURCE_DIR}/src/engine/scene/EditorCamera.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/scene/FpsCamera.cpp
)

target_include_directories(mood_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(mood_tests PRIVATE
    doctest::doctest
    glm::glm
)

add_test(NAME mood_tests COMMAND mood_tests)
