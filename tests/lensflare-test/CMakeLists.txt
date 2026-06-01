set(SOURCES
    src/main.cpp
)

metro_add_test(lensflare-test
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(lensflare-test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/plugins/metro-lensflare/src
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
