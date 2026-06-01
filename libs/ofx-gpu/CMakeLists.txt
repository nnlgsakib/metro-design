add_library(ofx-gpu STATIC
    src/Pipeline.cpp
)

target_include_directories(ofx-gpu
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

metro_target_cuda(ofx-gpu)
