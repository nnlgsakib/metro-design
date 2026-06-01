add_library(vfx_engine STATIC
    core/keying.cpp
    core/blur.cpp
    core/transforms.cpp
)

target_include_directories(vfx_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_link_libraries(vfx_engine PUBLIC platform)
