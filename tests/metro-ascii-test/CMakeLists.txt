set(SOURCES
    src/main.cpp
)

metro_add_test(metro-ascii-test
    SOURCES ${SOURCES}
)

target_include_directories(metro-ascii-test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
)
