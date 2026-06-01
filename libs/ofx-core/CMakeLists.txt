add_library(ofx-core
    src/Host.cpp
    src/Plugin.cpp
    src/param/ParamManager.cpp
)

target_include_directories(ofx-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/ofx-core>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
)

target_include_directories(ofx-core SYSTEM BEFORE
    PUBLIC
        ${CMAKE_SOURCE_DIR}/third_party/openfx/include
)

set_target_properties(ofx-core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
