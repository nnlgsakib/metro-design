if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    set(METRO_WARNING_FLAGS
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
    set(METRO_VISIBILITY_FLAGS -fvisibility=hidden -fvisibility-inlines-hidden)
    add_compile_options(${METRO_WARNING_FLAGS} ${METRO_VISIBILITY_FLAGS})
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(MSVC)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    add_compile_options(/W4 /utf-8)
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# --- OFX bundle install paths ---

if(APPLE)
    set(METRO_BUNDLE_PLATFORM_DIR "MacOS")
elseif(WIN32)
    set(METRO_BUNDLE_PLATFORM_DIR "Win64")
else()
    set(METRO_BUNDLE_PLATFORM_DIR "Linux-x86-64")
endif()

set(METRO_BUNDLE_BASE "MetroEffects.bundle/Contents/${METRO_BUNDLE_PLATFORM_DIR}")

function(metro_add_plugin target)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES LINK_LIBS)
    cmake_parse_arguments(METRO_PLUGIN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_library(${target} MODULE ${METRO_PLUGIN_SOURCES})
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        SUFFIX "${CMAKE_SHARED_MODULE_SUFFIX}"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    if(METRO_PLUGIN_LINK_LIBS)
        target_link_libraries(${target} PRIVATE ${METRO_PLUGIN_LINK_LIBS})
    endif()

    if(APPLE)
        set_target_properties(${target} PROPERTIES
            BUNDLE ON
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/PluginInfo.plist.in"
        )
    endif()

    install(TARGETS ${target}
        LIBRARY DESTINATION "${METRO_BUNDLE_BASE}"
        RUNTIME DESTINATION "${METRO_BUNDLE_BASE}"
    )
endfunction()

function(metro_add_test target)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES LINK_LIBS)
    cmake_parse_arguments(METRO_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${target} ${METRO_TEST_SOURCES})
    target_link_libraries(${target} PRIVATE ${METRO_TEST_LINK_LIBS})
    add_test(NAME ${target} COMMAND ${target} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()
