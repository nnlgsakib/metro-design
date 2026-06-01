set(SOURCES
    src/VrTeamsPlugin.cpp
)

metro_add_plugin(metro-vrteams
    SOURCES ${SOURCES}
    LINK_LIBS ofx-core
)

target_include_directories(metro-vrteams
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
        ${CMAKE_SOURCE_DIR}/libs/ofx-core/include
)
