set(SOURCES
    src/BlobDetector.cpp
    src/BlobTrackPlugin.cpp
)

metro_add_plugin(metro-blobtrack
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-blobtrack
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
