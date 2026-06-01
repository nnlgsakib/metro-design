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

# Function for targets that need CUDA dependency headers (libs, engines)
function(metro_target_cuda target)
    if(METRO_USE_CUDA AND CUDAToolkit_FOUND)
        target_compile_definitions(${target} PUBLIC ${METRO_CUDA_DEFINITIONS})
        target_include_directories(${target} PUBLIC ${METRO_CUDA_INCLUDE_DIRS})
        target_link_libraries(${target} PUBLIC ${METRO_CUDA_LIBRARIES})
    else()
        target_compile_definitions(${target} PUBLIC METRO_HAVE_CUDA=0)
    endif()
endfunction()

# Function for plugin targets that need CUDA support.
# Wraps metro_add_plugin() with GPU pipeline linkage and optional .cu compilation.
function(metro_add_gpu_plugin target)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES LINK_LIBS CUDA_SOURCES)
    cmake_parse_arguments(METRO_GPU "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(METRO_USE_CUDA AND CUDAToolkit_FOUND AND METRO_GPU_CUDA_SOURCES)
        enable_language(CUDA)
        list(APPEND METRO_GPU_SOURCES ${METRO_GPU_CUDA_SOURCES})
    endif()

    metro_add_plugin(${target}
        SOURCES ${METRO_GPU_SOURCES}
        LINK_LIBS ${METRO_GPU_LINK_LIBS}
    )

    metro_target_cuda(${target})

    if(METRO_USE_CUDA AND CUDAToolkit_FOUND)
        target_link_libraries(${target} PRIVATE ofx-gpu)
    endif()
endfunction()
