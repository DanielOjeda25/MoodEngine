# Bootstrap minimo de CPM.cmake.
#
# Descarga CPM.cmake de su repositorio oficial en la primera configuracion y lo
# cachea bajo el build dir. Una vez descargado no vuelve a tocar la red.
#
# CPM.cmake (https://github.com/cpm-cmake/CPM.cmake) es una capa fina sobre
# FetchContent que simplifica enormemente la declaracion de dependencias.

set(CPM_DOWNLOAD_VERSION 0.40.2)

if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)

if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "MoodEngine: descargando CPM.cmake v${CPM_DOWNLOAD_VERSION}")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
        TLS_VERIFY ON
    )
endif()

include(${CPM_DOWNLOAD_LOCATION})
