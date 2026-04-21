# Flags de warnings y conformidad por compilador.
#
# Se invoca con: mood_set_compiler_warnings(<target>)
#
# Decisiones (ver docs/DECISIONS.md):
#  - /W4 en MSVC (warning level alto)
#  - /permissive- (conformidad estricta al estandar)
#  - /WX no se activa aun: el proyecto esta en fase temprana y queremos poder
#    iterar sin bloquear cada build por un warning menor. Se activara cuando el
#    codigo este mas estable (Hito 5+).

function(mood_set_compiler_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /MP
        )
        target_compile_definitions(${target} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
        )
    endif()
endfunction()
