set(SOURCES
    src/ChromaticAberrationPlugin.cpp
)

metro_add_plugin(metro-chromab
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-chromab
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
