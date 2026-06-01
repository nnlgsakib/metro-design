add_library(color_engine STATIC
    core/color_space.cpp
    core/lut_engine.cpp
    core/gamut_mapping.cpp
)

target_include_directories(color_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

target_link_libraries(color_engine PUBLIC platform)
