set(SOURCES
    src/main.cpp
)

metro_add_test(glow-test
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(glow-test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/plugins/metro-glow/src
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
