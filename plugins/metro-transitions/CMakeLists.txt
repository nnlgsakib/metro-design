set(SOURCES
    src/TransitionsPlugin.cpp
)

metro_add_plugin(metro-transitions
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-transitions
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
