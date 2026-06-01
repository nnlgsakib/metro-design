add_library(motion_engine STATIC
    transitions/transition_engine.cpp
    titles/title_engine.cpp
)

target_include_directories(motion_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(motion_engine PUBLIC platform)
