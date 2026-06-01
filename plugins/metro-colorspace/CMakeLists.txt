set(SOURCES
    src/ColorSpacePlugin.cpp
)

metro_add_plugin(metro-colorspace
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-colorspace
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
