set(SOURCES
    src/FilmGrainPlugin.cpp
)

set(CUDA_SOURCES
    src/FilmGrainKernels.cu
)

metro_add_gpu_plugin(metro-filmgrain
    SOURCES ${SOURCES}
    CUDA_SOURCES ${CUDA_SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-filmgrain
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-gpu/include
)
