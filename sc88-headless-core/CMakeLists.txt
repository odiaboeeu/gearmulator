add_library(sc88_headless_core STATIC
    sc88_headless_core.cpp
)

target_include_directories(sc88_headless_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sc88_headless_core
    PRIVATE
        88lib
)

target_compile_features(sc88_headless_core
    PUBLIC
        cxx_std_17
)

set_property(
    TARGET sc88_headless_core
    PROPERTY FOLDER "Ronaldo/88emu/headless"
)
