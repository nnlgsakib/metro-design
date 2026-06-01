add_library(platform STATIC
    plugin-host/plugin_host.cpp
    licensing/license_client.cpp
    telemetry/src/telemetry.cpp
    telemetry/src/sentry_client.cpp
    telemetry/src/metrics.cpp
)

target_include_directories(platform PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(platform PRIVATE
    telemetry/include
)

target_link_libraries(platform PRIVATE
    ${CMAKE_DL_LIBS}
)

if(WIN32)
    target_compile_definitions(platform PRIVATE PLATFORM_WINDOWS)
elseif(APPLE)
    target_compile_definitions(platform PRIVATE PLATFORM_MACOS)
else()
    target_compile_definitions(platform PRIVATE PLATFORM_LINUX)
endif()

if(MET_BUILD_TESTS)
    add_subdirectory(test)
endif()
