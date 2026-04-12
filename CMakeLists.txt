cmake_minimum_required(VERSION 3.16)

project(Panelcast
    VERSION 1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# LZ4 via FetchContent (manually build library)
# ============================================================
include(FetchContent)

FetchContent_Declare(
    lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG        v1.10.0
)
FetchContent_MakeAvailable(lz4)

# Build LZ4 manually (the official repo does NOT export targets)
add_library(lz4 STATIC
    ${lz4_SOURCE_DIR}/lib/lz4.c
    ${lz4_SOURCE_DIR}/lib/lz4hc.c
)

target_include_directories(lz4 PUBLIC
    ${lz4_SOURCE_DIR}/lib
)

# ------------------------------------------------------------
# WASM LZ4 (für Browser-Client)
# ------------------------------------------------------------
# Nur bauen, wenn Emscripten verwendet wird
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")

    message(STATUS "Building LZ4 WASM module")

    add_executable(lz4wasm
        ${lz4_SOURCE_DIR}/lib/lz4.c
        ${CMAKE_CURRENT_SOURCE_DIR}/web/lz4_wasm.c
    )

    set_target_properties(lz4wasm PROPERTIES
        OUTPUT_NAME "lz4-wasm"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/web"
    )

    target_include_directories(lz4wasm PRIVATE
        ${lz4_SOURCE_DIR}/lib
    )

    target_link_options(lz4wasm PRIVATE
        "-O3"
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sEXPORTED_FUNCTIONS=['_lz4_decompress','_lz4_free']"
        "-sEXPORTED_RUNTIME_METHODS=['cwrap','getValue','malloc','free']"
        "--no-entry"
    )

endif()

# ============================================================
# nlohmann/json via FetchContent
# ============================================================
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)

FetchContent_MakeAvailable(nlohmann_json)

# ============================================================
# GLAD (generiert)
# ============================================================
add_library(glad STATIC
    third_party/glad/src/glad.c
)

target_include_directories(glad PUBLIC
    third_party/glad/include
)

# ============================================================
# Mongoose WebServer (HTTP + WebSocket)
# ============================================================
add_library(mongoose STATIC
    third_party/mongoose/mongoose.c
)

target_include_directories(mongoose PUBLIC
    third_party/mongoose
)

# ============================================================
# Source files
# ============================================================
set(SRC_FILES
    src/ConfigManager.cpp
    src/Panelcast.cpp
    src/PanelcastPlugin.cpp
    src/UdpSender.cpp
    src/PanelCapturer.cpp
    src/FrameSender.cpp
    src/WsSender.cpp
)

set(HDR_FILES
    src/ConfigManager.h
    src/PanelROI.h
    src/RawPanelFrame.h
    src/UdpSender.h
    src/PanelCapturer.h
    src/FrameSender.h
    src/PanelcastPlugin.h
    src/WsSender.h
)

# ============================================================
# Include directories
# ============================================================
include_directories(
    ${CMAKE_SOURCE_DIR}/SDK/CHeaders/XPLM
    ${CMAKE_SOURCE_DIR}/SDK/CHeaders/Widgets
    ${lz4_SOURCE_DIR}/lib
    third_party/glad/include
)

# ============================================================
# Platform libs + X‑Plane platform defines
# ============================================================
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX)
    add_definitions(-DIBM)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(PLATFORM_LIBS ws2_32 opengl32)
    set(XPLM_LIBS
        ${CMAKE_SOURCE_DIR}/SDK/Libraries/Win/XPLM_64.lib
        ${CMAKE_SOURCE_DIR}/SDK/Libraries/Win/XPWidgets_64.lib
    )

elseif(APPLE)
    add_definitions(-DAPL)
    find_library(COCOA Cocoa)
    find_library(OpenGL OpenGL)
    set(PLATFORM_LIBS ${COCOA} ${OpenGL} pthread)

else() # Linux
    add_definitions(-DLIN)
    set(PLATFORM_LIBS GL pthread)
endif()

# ============================================================
# Plugin target
# ============================================================
add_library(Panelcast SHARED
    ${SRC_FILES}
    ${HDR_FILES}
)

target_link_libraries(Panelcast
    glad
    lz4
    mongoose
    nlohmann_json
    ${XPLM_LIBS}
    ${PLATFORM_LIBS}
)

set_target_properties(Panelcast PROPERTIES
    PREFIX ""
    SUFFIX ".xpl"
)

# ============================================================
# Installation directory
# ============================================================
set(PLUGIN_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/dist/Panelcast")

install(TARGETS Panelcast
    RUNTIME DESTINATION ${PLUGIN_OUTPUT_DIR}
    LIBRARY DESTINATION ${PLUGIN_OUTPUT_DIR}
)

install(FILES config/config.json
    DESTINATION ${PLUGIN_OUTPUT_DIR}
)

# ---------------------------------------------------------
#  INSTALL TO X-PLANE (Windows only)
# ---------------------------------------------------------

if(WIN32)
    set(XPLANE_PLUGIN_DIR "C:/X-Plane 12/Resources/plugins/Panelcast")

    add_custom_command(TARGET Panelcast POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${XPLANE_PLUGIN_DIR}"

        # Web-Ordner löschen, damit copy_directory immer alles neu kopiert
        COMMAND ${CMAKE_COMMAND} -E remove_directory
            "${XPLANE_PLUGIN_DIR}/web"

        # Web-Ordner neu kopieren
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/web"
            "${XPLANE_PLUGIN_DIR}/web"

        COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:Panelcast>"
            "${XPLANE_PLUGIN_DIR}/win.xpl"

        COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/config/config.json"
            "${XPLANE_PLUGIN_DIR}/config.json"

        COMMENT "Installing Panelcast plugin, config.json and web assets"
    )
endif()


