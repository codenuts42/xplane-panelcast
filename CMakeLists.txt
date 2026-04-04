cmake_minimum_required(VERSION 3.16)

project(Panelcast
    VERSION 1.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 17)
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
# Source files
# ============================================================
set(SRC_FILES
    src/Panelcast.cpp
    src/PanelcastPlugin.cpp
    src/UdpSender.cpp
    src/PanelCapturer.cpp
    src/FrameSender.cpp
)

set(HDR_FILES
    src/PanelROI.h
    src/RawPanelFrame.h
    src/UdpSender.h
    src/PanelCapturer.h
    src/FrameSender.h
    src/PanelcastPlugin.h
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
        ${XPLANE_SDK_PATH}/Libraries/Win/XPLM_64.lib
        ${XPLANE_SDK_PATH}/Libraries/Win/XPWidgets_64.lib
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
