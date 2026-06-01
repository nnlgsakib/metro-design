set(SOURCES
    src/ASCIIPlugin.cpp
)

metro_add_plugin(metro-ascii
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-ascii
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
