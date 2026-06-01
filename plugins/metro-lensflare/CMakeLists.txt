set(SOURCES
    src/LensFlarePlugin.cpp
)

if(METRO_HAVE_CUDA)
    list(APPEND SOURCES src/LensFlareKernels.cu)
endif()

metro_add_plugin(metro-lensflare
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core ofx-gpu
)

metro_target_cuda(metro-lensflare)

target_include_directories(metro-lensflare
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-gpu/include
)
