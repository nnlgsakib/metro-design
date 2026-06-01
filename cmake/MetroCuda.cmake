option(METRO_USE_CUDA "Enable CUDA GPU pipeline" ON)

if(METRO_USE_CUDA)
    find_package(CUDAToolkit QUIET)
endif()

if(METRO_USE_CUDA AND CUDAToolkit_FOUND)
    message(STATUS "MetroCuda: CUDA toolkit found (${CUDAToolkit_VERSION}) at ${CUDAToolkit_INCLUDE_DIRS}")

    set(METRO_CUDA_ARCHS "52;61;70;75;80;86;89;90" CACHE STRING
        "CUDA compute capability architectures to target (semicolon-separated)")

    add_compile_definitions(METRO_HAVE_CUDA=1)

    set(METRO_CUDA_DEFINITIONS METRO_HAVE_CUDA=1)
    set(METRO_CUDA_INCLUDE_DIRS ${CUDAToolkit_INCLUDE_DIRS})
    set(METRO_CUDA_LIBRARIES CUDA::cudart)

    message(STATUS "MetroCuda: targeting architectures ${METRO_CUDA_ARCHS}")
    message(STATUS "MetroCuda: GPU pipeline ENABLED")
else()
    message(STATUS "MetroCuda: CUDA disabled or toolkit not found — using CPU fallback")

    add_compile_definitions(METRO_HAVE_CUDA=0)

    set(METRO_CUDA_DEFINITIONS METRO_HAVE_CUDA=0)
    set(METRO_CUDA_INCLUDE_DIRS "")
    set(METRO_CUDA_LIBRARIES "")
endif()

# Function for targets that need CUDA dependency headers
function(metro_target_cuda target)
    if(METRO_USE_CUDA AND CUDAToolkit_FOUND)
        target_compile_definitions(${target} PUBLIC ${METRO_CUDA_DEFINITIONS})
        target_include_directories(${target} PUBLIC ${METRO_CUDA_INCLUDE_DIRS})
        target_link_libraries(${target} PUBLIC ${METRO_CUDA_LIBRARIES})
    else()
        target_compile_definitions(${target} PUBLIC METRO_HAVE_CUDA=0)
    endif()
endfunction()
