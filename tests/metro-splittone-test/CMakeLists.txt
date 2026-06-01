set(SOURCES
    src/main.cpp
)

metro_add_test(metro-splittone-test
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-splittone-test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/plugins/metro-splittone/src
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
