cmake_minimum_required(VERSION 3.15)

add_executable(sc88_headless_test
    sc88_headless_test.cpp
)

target_link_libraries(sc88_headless_test
    PRIVATE
        88lib
)

target_compile_features(sc88_headless_test
    PRIVATE
        cxx_std_17
)

set_property(
    TARGET sc88_headless_test
    PROPERTY FOLDER "Ronaldo/88emu/tests"
)
