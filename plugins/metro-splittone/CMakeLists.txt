set(SOURCES
    src/SplitTonePlugin.cpp
)

metro_add_plugin(metro-splittone
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-splittone
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
