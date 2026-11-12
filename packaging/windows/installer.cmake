# Runtime staging is shared by the installer and portable builds.
enable_language(RC)
configure_file("${PROJECT_SOURCE_DIR}/packaging/windows/sattrak.rc.in"
    "${PROJECT_BINARY_DIR}/sattrak.rc" @ONLY)
target_sources(ag PRIVATE "${PROJECT_BINARY_DIR}/sattrak.rc")
add_executable(sattrak_updater WIN32 "${PROJECT_SOURCE_DIR}/packaging/windows/updater.cpp")
target_link_libraries(sattrak_updater PRIVATE bcrypt advapi32 shell32)
target_link_libraries(ag PRIVATE bcrypt advapi32)
if(MSVC)
    set_property(TARGET sattrak_updater PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
add_dependencies(ag sattrak_updater)
install(TARGETS sattrak_updater RUNTIME DESTINATION . COMPONENT Runtime)
file(WRITE "${PROJECT_BINARY_DIR}/release-version.txt" "${SATTRAK_RELEASE_VERSION}\n")
install(FILES "${PROJECT_BINARY_DIR}/release-version.txt" DESTINATION . COMPONENT Runtime)
add_executable(sattrak_installer_audio WIN32 "${PROJECT_SOURCE_DIR}/packaging/windows/installer_audio.cpp")
target_include_directories(sattrak_installer_audio SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/external/miniaudio")
target_link_libraries(sattrak_installer_audio PRIVATE winmm ole32 shell32)
# The temporary helper must run before the app's MSVC runtime DLLs are installed.
if(MSVC)
    set_property(TARGET sattrak_installer_audio PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
install(TARGETS ag RUNTIME DESTINATION . COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/earth_texture.jpg" "${PROJECT_SOURCE_DIR}/LICENSE"
    "${PROJECT_SOURCE_DIR}/TERMS.md" DESTINATION . COMPONENT Runtime)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets/" DESTINATION assets COMPONENT Runtime)
if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT Runtime)
    include(InstallRequiredSystemLibraries)
endif()

find_program(ISCC_EXECUTABLE NAMES ISCC ISCC.exe
    HINTS "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6" "$ENV{LOCALAPPDATA}/Programs/Inno Setup 7"
    "$ENV{PROGRAMFILES}/Inno Setup 6" "$ENV{PROGRAMFILES}/Inno Setup 7"
    "$ENV{SystemDrive}/Program Files (x86)/Inno Setup 6")
if(ISCC_EXECUTABLE)
    add_custom_target(windows_installer
        COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}" --config $<CONFIG>
            --component Runtime --prefix "${PROJECT_BINARY_DIR}/installer-stage/$<CONFIG>"
        COMMAND "${ISCC_EXECUTABLE}"
            "/DSourceDir=${PROJECT_BINARY_DIR}/installer-stage/$<CONFIG>"
            "/DOutputDir=${PROJECT_BINARY_DIR}/installer"
            "/DAppVersion=${SATTRAK_RELEASE_VERSION}"
            "/DAudioHelper=$<TARGET_FILE:sattrak_installer_audio>"
            "${PROJECT_SOURCE_DIR}/packaging/windows/sattrak.iss"
        DEPENDS ag sattrak_installer_audio sattrak_updater USES_TERMINAL VERBATIM
        COMMENT "Build the branded, per-user Windows installation wizard")
else()
    message(STATUS "Inno Setup not found; set ISCC_EXECUTABLE to enable windows_installer")
endif()
