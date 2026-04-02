cmake_minimum_required(VERSION 3.16)
project(Panelcast LANGUAGES C CXX)

# Compiler settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

# ---------------------------------------------------------
#  CLANG OPTIMIZATIONS
# ---------------------------------------------------------

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")

    message(STATUS "Building with Clang optimizations")

    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-deprecated-declarations
        -Wno-language-extension-token
        -Wno-missing-field-initializers
    )

    # Release optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math -fstrict-aliasing -funroll-loops -flto")
    set(CMAKE_C_FLAGS_RELEASE   "-O3 -ffast-math -fstrict-aliasing -funroll-loops -flto")

    # Debug flags
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS_DEBUG   "-O0 -g -fno-omit-frame-pointer")

    # LTO
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-flto")

endif()

# ---------------------------------------------------------
#  INCLUDE DIRECTORIES
# ---------------------------------------------------------

include_directories(
    ${CMAKE_SOURCE_DIR}/SDK/CHeaders/XPLM
    ${CMAKE_SOURCE_DIR}/SDK/CHeaders/Widgets
    ${CMAKE_SOURCE_DIR}/third_party
)

# ---------------------------------------------------------
#  DEFINES
# ---------------------------------------------------------

add_compile_definitions(
    WIN32
    _WINDOWS
    _USRDLL
    SIMDATA_EXPORTS
    IBM=1
    XPLM200=1
    XPLM210=1
    XPLM300=1
    XPLM301=1
    _CRT_SECURE_NO_WARNINGS
)

# ---------------------------------------------------------
#  SOURCES
# ---------------------------------------------------------

set(SOURCES
    src/Panelcast.cpp
    third_party/glad/glad.c
    third_party/lz4/lz4.c
    third_party/lz4/lz4frame.c
    third_party/lz4/lz4hc.c
    third_party/lz4/xxhash.c
)

# ---------------------------------------------------------
#  PLUGIN (DLL)
# ---------------------------------------------------------

add_library(Panelcast SHARED ${SOURCES})

set_target_properties(Panelcast PROPERTIES
    OUTPUT_NAME "win"
    SUFFIX ".xpl"
)

# ---------------------------------------------------------
#  LINKING
# ---------------------------------------------------------

target_link_directories(Panelcast PRIVATE
    ${CMAKE_SOURCE_DIR}/SDK/Libraries/Win
)

target_link_libraries(Panelcast
    XPLM_64
    XPWidgets_64
    opengl32
    user32
    kernel32
    gdi32
    winspool
    shell32
    ole32
    oleaut32
    uuid
    comdlg32
    advapi32
)

# ---------------------------------------------------------
#  INSTALL TO X-PLANE
# ---------------------------------------------------------

set(XPLANE_PLUGIN_DIR "C:/X-Plane 12/Resources/plugins/Panelcast")

add_custom_command(TARGET Panelcast POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${XPLANE_PLUGIN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:Panelcast>"
        "${XPLANE_PLUGIN_DIR}/win.xpl"
)
