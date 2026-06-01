set(SOURCES
    src/GlowPlugin.cpp
)

if(METRO_HAVE_CUDA)
    list(APPEND SOURCES src/GlowKernels.cu)
endif()

metro_add_plugin(metro-glow
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core ofx-gpu
)

metro_target_cuda(metro-glow)

target_include_directories(metro-glow
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-gpu/include
)
