set(SOURCES
    src/main.cpp
)

metro_add_test(colorspace-test
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(colorspace-test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/plugins/metro-colorspace/src
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
