cmake_minimum_required(VERSION 3.18)
project(metrovfx VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(MetroCompileSettings)
include(MetroCuda)

add_subdirectory(libs/ofx-core)
add_subdirectory(libs/ofx-gpu)

option(METRO_BUILD_PLUGINS "Build plugin targets" ON)
option(METRO_BUILD_TESTS "Build test targets" ON)

if(METRO_BUILD_PLUGINS)
    add_subdirectory(plugins/metro-sample)
    add_subdirectory(plugins/metro-ascii)
    add_subdirectory(plugins/metro-blobtrack)
    add_subdirectory(plugins/metro-chromab)
    add_subdirectory(plugins/metro-vrteams)
    add_subdirectory(plugins/metro-glow)
    add_subdirectory(plugins/metro-filmgrain)
    add_subdirectory(plugins/metro-lensflare)
    add_subdirectory(plugins/metro-splittone)
    add_subdirectory(plugins/metro-colorspace)
    add_subdirectory(plugins/metro-transitions)
endif()

if(METRO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/ofx-core-test)
    add_subdirectory(tests/metro-ascii-test)
    add_subdirectory(tests/metro-blobtrack-test)
    add_subdirectory(tests/chromab-test)
    add_subdirectory(tests/glow-test)
endif()

# --- Packaging ---

install(FILES README.md DESTINATION "." COMPONENT "Runtime")

set(CPACK_PACKAGE_NAME "MetroEffects")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Metro Design")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Metro Design OFX plugin pack for DaVinci Resolve")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://metrodesign.ai")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/dist")

set(CPACK_MONOLITHIC_INSTALL ON)

if(WIN32)
    set(CPACK_GENERATOR "NSIS;TGZ")
    set(CPACK_NSIS_DISPLAY_NAME "MetroEffects ${PROJECT_VERSION}")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64\\Common Files")
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "OFX/Plugins")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop;TGZ")
    set(CPACK_DMG_VOLUME_NAME "MetroEffects-${PROJECT_VERSION}")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
