cmake_minimum_required(VERSION 3.20)
project(Dockless VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add source files
set(SOURCES
    src/main.cpp
    src/app/Window.cpp
    src/app/Config.cpp
    src/system/DwmManager.cpp
    src/docker/DockerClient.cpp
    src/qemu/QemuManager.cpp
    src/rendering/Renderer.cpp
    src/ui/Sidebar.cpp
    src/ui/ContainersView.cpp
    src/ui/ImagesView.cpp
    src/ui/VolumesView.cpp
    src/ui/NetworksView.cpp
    src/resource.rc
)

# Create executable
add_executable(Dockless WIN32 ${SOURCES})

# Copy resource files to output directory
add_custom_command(TARGET Dockless POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/resources" $<TARGET_FILE_DIR:Dockless>)

# Link libraries
target_link_libraries(Dockless PRIVATE
    d3d11
    dxgi
    dcomp
    d2d1
    dwrite
    windowscodecs
    dwmapi
    winhttp
    ws2_32
    shlwapi
)

# Compiler flags for MSVC
if(MSVC)
    target_compile_options(Dockless PRIVATE /W4 /WX- /permissive-)
    target_compile_definitions(Dockless PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
