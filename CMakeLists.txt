cmake_minimum_required(VERSION 3.20)
project(DynamicsCFD VERSION 1.0.0 LANGUAGES C CXX DESCRIPTION "Spacecraft Dynamics and Supersonic CFD Simulation")

# Require modern C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers (clangd)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimization and Warning Flags
if(MSVC)
    add_compile_options(/W3 /permissive- /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow)
endif()

# Global Include Directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ==============================================================================
# Target 1: Satellite ADCS MEKF Executable
# ==============================================================================
add_executable(mekf_adcs
    src/adcs/main_adcs.cpp
)
target_include_directories(mekf_adcs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ==============================================================================
# Target 2: Supersonic 2D Compressible CFD Solver Executable
# ==============================================================================
add_executable(supersonic_cfd
    src/cfd/main_cfd.cpp
)
target_include_directories(supersonic_cfd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ==============================================================================
# Target 3: ag.exe - Native 3D Simulation GUI
# ==============================================================================
option(BUILD_GUI "Build the interactive OpenGL studio" ON)
if(BUILD_GUI)
find_package(OpenGL REQUIRED)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

set(GUI_SOURCES
    src/gui/main_gui.cpp
    src/gui/gl_util.cpp
    src/gui/satellite_model.cpp
    src/gui/adcs_view.cpp
    src/gui/cfd_view.cpp
    external/glad/src/glad.c
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
    external/implot/implot.cpp
    external/implot/implot_items.cpp
)

add_executable(ag ${GUI_SOURCES})

target_include_directories(ag PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(ag SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/external/implot
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
)

target_link_libraries(ag PRIVATE
    glfw
    OpenGL::GL
)
if(WIN32)
    target_link_libraries(ag PRIVATE winmm)
endif()
add_custom_command(TARGET ag POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/earth_texture.jpg" "$<TARGET_FILE_DIR:ag>/earth_texture.jpg"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:ag>/assets")
endif()

# ==============================================================================
# Automated Testing Suite (CTest)
# ==============================================================================
include(CTest)
if(BUILD_TESTING)

add_executable(test_adcs
    tests/test_quaternion_mekf.cpp
)
target_include_directories(test_adcs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME Test_ADCS_MEKF COMMAND test_adcs)

add_executable(test_cfd
    tests/test_cfd_shock.cpp
)
target_include_directories(test_cfd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME Test_CFD_Shock COMMAND test_cfd)
add_executable(test_regressions tests/test_regressions.cpp)
add_test(NAME Test_Numerical_Regressions COMMAND test_regressions)
endif()
