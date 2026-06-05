if(METRO_HAVE_CUDA)
    enable_language(CUDA)

    set(SOURCES
        src/VerificationMain.cu
        ${CMAKE_SOURCE_DIR}/plugins/metro-filmgrain/src/FilmGrainKernels.cu
        ${CMAKE_SOURCE_DIR}/plugins/metro-glow/src/GlowKernels.cu
        ${CMAKE_SOURCE_DIR}/plugins/metro-lensflare/src/LensFlareKernels.cu
    )

    metro_add_test(cuda-verification
        SOURCES ${SOURCES}
        LINK_LIBS ofx-core
    )

    target_include_directories(cuda-verification
        PRIVATE
            ${CMAKE_SOURCE_DIR}/plugins/metro-filmgrain/src
            ${CMAKE_SOURCE_DIR}/plugins/metro-glow/src
            ${CMAKE_SOURCE_DIR}/plugins/metro-lensflare/src
            ${CMAKE_SOURCE_DIR}/third_party/openfx/include
            ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
    )

    target_link_libraries(cuda-verification PRIVATE CUDA::cudart)

    set_tests_properties(cuda-verification PROPERTIES
        LABELS "gpu"
        TIMEOUT 120
    )
endif()
