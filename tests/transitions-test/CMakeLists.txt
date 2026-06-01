set(SOURCES
    src/main.cpp
)

metro_add_test(transitions-test
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(transitions-test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/plugins/metro-transitions/src
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
