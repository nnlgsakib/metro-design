set(SOURCES
    src/SamplePlugin.cpp
)

metro_add_plugin(metro-sample
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-sample
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
