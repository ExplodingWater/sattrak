cmake_minimum_required(VERSION 3.20)
project(sattrak VERSION 1.0.0 LANGUAGES C CXX DESCRIPTION "Spacecraft Dynamics and Supersonic CFD Simulation")
if(APPLE)
    enable_language(OBJCXX)
endif()
set(SATTRAK_RELEASE_VERSION "1.0.0" CACHE STRING "Release tag version used by the update checker")
if(NOT SATTRAK_RELEASE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+[-+A-Za-z0-9.]*$")
    message(FATAL_ERROR "SATTRAK_RELEASE_VERSION must be a semantic release version")
endif()

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

file(GLOB SGP4_SOURCES CONFIGURE_DEPENDS external/sgp4/*.cc)
add_library(sattrak_sgp4 STATIC ${SGP4_SOURCES})
target_include_directories(sattrak_sgp4 PUBLIC external/sgp4)

set(GUI_SOURCES
    src/gui/main_gui.cpp
    src/gui/update_checker.cpp
    src/gui/gl_util.cpp
    src/gui/sky_catalog.cpp
    src/gui/sky_renderer.cpp
    src/gui/satellite_model.cpp
    src/gui/research_catalog.cpp
    src/gui/catalog_updates.cpp
    src/gui/music_player.cpp
    src/gui/alarm_audio.cpp
    src/gui/radio_commands.cpp
    src/gui/developer_console.cpp
    src/gui/action_bindings.cpp
    src/gui/interaction_controls.cpp
    src/gui/adcs_view.cpp
    src/gui/multi_view.cpp
    src/gui/orbit_replay.cpp
    src/gui/orbit_recording.cpp
    src/gui/recording_controls.cpp
    src/gui/display_settings.cpp
    src/gui/catalog_details.cpp
    src/gui/orbit_controls.cpp
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
target_compile_definitions(ag PRIVATE SATTRAK_RELEASE_VERSION="${SATTRAK_RELEASE_VERSION}")
find_package(Threads REQUIRED)
target_link_libraries(ag PRIVATE Threads::Threads)
if(NOT WIN32)
    find_package(CURL REQUIRED)
    target_link_libraries(ag PRIVATE CURL::libcurl)
    target_sources(ag PRIVATE src/gui/native_update.cpp)
    add_executable(sattrak_launcher packaging/native/launcher.cpp src/gui/native_update.cpp)
    if(APPLE)
        target_sources(ag PRIVATE src/gui/platform_application.mm)
        set_source_files_properties(src/gui/platform_application.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
        target_link_libraries(ag PRIVATE "-framework Cocoa")
        target_link_libraries(sattrak_launcher PRIVATE "-framework CoreFoundation")
    endif()
    add_custom_target(native_update_helper
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:ag>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/packaging/native/native_updater.py" "$<TARGET_FILE_DIR:ag>/native_updater.py")
    add_dependencies(ag native_update_helper)
    find_package(Python3 3.9 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND AND (APPLE OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
        add_custom_target(native_package
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/package_native.py"
                --binary "$<TARGET_FILE:ag>" --source "${PROJECT_SOURCE_DIR}"
                --version "${SATTRAK_RELEASE_VERSION}"
                --output "${PROJECT_BINARY_DIR}/packages"
            DEPENDS ag USES_TERMINAL VERBATIM)
        if(APPLE)
            set(SATTRAK_APP_SIGNING_IDENTITY "-" CACHE STRING "Developer ID Application identity; - means ad hoc")
            set(SATTRAK_INSTALLER_SIGNING_IDENTITY "" CACHE STRING "Developer ID Installer identity; empty means unsigned")
            add_custom_target(macos_installer
                COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/package_installers.py"
                    --binary "$<TARGET_FILE:ag>" --launcher "$<TARGET_FILE:sattrak_launcher>"
                    --source "${PROJECT_SOURCE_DIR}" --version "${SATTRAK_RELEASE_VERSION}"
                    --output "${PROJECT_BINARY_DIR}/installers" --format macos
                    --app-signing-identity "${SATTRAK_APP_SIGNING_IDENTITY}"
                    --installer-signing-identity "${SATTRAK_INSTALLER_SIGNING_IDENTITY}"
                DEPENDS ag sattrak_launcher USES_TERMINAL VERBATIM)
        else()
            add_custom_target(debian_installer
                COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/package_installers.py"
                    --binary "$<TARGET_FILE:ag>" --launcher "$<TARGET_FILE:sattrak_launcher>"
                    --source "${PROJECT_SOURCE_DIR}" --version "${SATTRAK_RELEASE_VERSION}"
                    --output "${PROJECT_BINARY_DIR}/installers" --format debian
                DEPENDS ag sattrak_launcher USES_TERMINAL VERBATIM)
        endif()
    endif()
endif()

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
    ${CMAKE_CURRENT_SOURCE_DIR}/external/miniaudio
)

target_link_libraries(ag PRIVATE
    sattrak_sgp4
    glfw
    OpenGL::GL
)
if(UNIX AND NOT APPLE)
    target_link_libraries(ag PRIVATE ${CMAKE_DL_LIBS} pthread m)
elseif(APPLE)
    target_link_libraries(ag PRIVATE "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
endif()
if(WIN32)
    target_link_libraries(ag PRIVATE winmm comdlg32 winhttp)
endif()
add_custom_command(TARGET ag POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/earth_texture.jpg" "$<TARGET_FILE_DIR:ag>/earth_texture.jpg"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/TERMS.md" "$<TARGET_FILE_DIR:ag>/TERMS.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:ag>/LICENSE"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:ag>/assets")
if(WIN32)
    include(packaging/windows/installer.cmake)
endif()
endif()

# ==============================================================================
# Automated Testing Suite (CTest)
# ==============================================================================
include(CTest)
if(BUILD_TESTING)
add_executable(test_solar_environment tests/test_solar_environment.cpp)
add_test(NAME Test_Solar_Environment COMMAND test_solar_environment)
if(UNIX)
    find_package(Python3 3.9 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_executable(sattrak_native_updater_fixture tests/native_updater_fixture.cpp src/gui/native_update.cpp)
        add_test(NAME Test_Native_Update COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_native_update.py")
        set_tests_properties(Test_Native_Update PROPERTIES TIMEOUT 30
            ENVIRONMENT "SATTRAK_NATIVE_FIXTURE=$<TARGET_FILE:sattrak_native_updater_fixture>")
        if(TARGET sattrak_launcher)
            set_property(TEST Test_Native_Update APPEND PROPERTY ENVIRONMENT
                "SATTRAK_BOOTSTRAP_LAUNCHER=$<TARGET_FILE:sattrak_launcher>")
        endif()
        add_test(NAME Test_Installer_Packaging COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/test_installer_packaging.py")
    endif()
endif()
add_executable(test_sky_catalog tests/test_sky_catalog.cpp src/gui/sky_catalog.cpp)
add_test(NAME Test_Sky_Catalog COMMAND test_sky_catalog "${CMAKE_CURRENT_SOURCE_DIR}/assets/sky")

if(WIN32)
    add_executable(test_auto_update tests/test_auto_update.cpp)
    target_link_libraries(test_auto_update PRIVATE bcrypt advapi32)
    add_test(NAME Test_Auto_Update_Verification COMMAND test_auto_update)
    add_executable(sattrak_updater_fixture WIN32 EXCLUDE_FROM_ALL tests/updater_fixture.cpp)
    target_link_libraries(sattrak_updater_fixture PRIVATE bcrypt advapi32 shell32)
    if(MSVC)
        set_property(TARGET sattrak_updater_fixture PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()
if(BUILD_GUI)
    add_test(NAME Test_Update_Worker COMMAND ag --update-smoke-test)
    set_tests_properties(Test_Update_Worker PROPERTIES TIMEOUT 10)
endif()
add_executable(test_update_release tests/test_update_release.cpp)
add_test(NAME Test_Update_Release COMMAND test_update_release)
add_executable(test_tutorial_state tests/test_tutorial_state.cpp)
add_test(NAME Test_Tutorial_State COMMAND test_tutorial_state)

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
add_executable(test_orbit_replay tests/test_orbit_replay.cpp src/gui/orbit_replay.cpp)
add_test(NAME Test_Orbit_Replay COMMAND test_orbit_replay "${CMAKE_CURRENT_SOURCE_DIR}/assets/orbits")
add_executable(test_orbit_recording tests/test_orbit_recording.cpp src/gui/orbit_recording.cpp src/gui/orbit_replay.cpp)
add_test(NAME Test_Orbit_Recording COMMAND test_orbit_recording)
if(BUILD_GUI)
    # Optional real-GPU check; run manually from a graphical desktop session.
    add_executable(test_sky_renderer tests/test_sky_renderer.cpp
        src/gui/sky_catalog.cpp src/gui/sky_renderer.cpp src/gui/gl_util.cpp external/glad/src/glad.c)
    target_include_directories(test_sky_renderer SYSTEM PRIVATE external/glad/include external/stb)
    target_link_libraries(test_sky_renderer PRIVATE glfw OpenGL::GL)
    if(UNIX AND Python3_Interpreter_FOUND)
        # Opt in explicitly from a real desktop; ordinary CTest stays headless.
        add_custom_target(gui_checks
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/run_gui_checks.py"
                --binary "$<TARGET_FILE:ag>" --sky-renderer "$<TARGET_FILE:test_sky_renderer>"
                --source-root "${PROJECT_SOURCE_DIR}" --output "${PROJECT_BINARY_DIR}/gui-checks"
            DEPENDS ag test_sky_renderer USES_TERMINAL VERBATIM)
    endif()
    add_executable(test_radio_player tests/test_radio_player.cpp src/gui/music_player.cpp src/gui/alarm_audio.cpp src/gui/radio_commands.cpp)
    add_executable(test_research_catalog tests/test_research_catalog.cpp src/gui/research_catalog.cpp)
    target_link_libraries(test_research_catalog PRIVATE sattrak_sgp4)
    add_test(NAME Test_Research_Catalog COMMAND test_research_catalog "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_include_directories(test_radio_player SYSTEM PRIVATE external/miniaudio)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_radio_player PRIVATE ${CMAKE_DL_LIBS} pthread m)
    elseif(APPLE)
        target_link_libraries(test_radio_player PRIVATE "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
    endif()
endif()
endif()
