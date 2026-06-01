set(SOURCES
    src/main.cpp
    ${CMAKE_SOURCE_DIR}/plugins/metro-blobtrack/src/BlobDetector.cpp
)

metro_add_test(metro-blobtrack-test
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-blobtrack-test
    PRIVATE
        ${CMAKE_SOURCE_DIR}/plugins/metro-blobtrack/include
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
