add_executable(sc88_memory_test
    sc88_memory_test.cpp
)

target_link_libraries(sc88_memory_test
    PRIVATE
        88lib
)

target_compile_features(sc88_memory_test
    PRIVATE
        cxx_std_17
)

set_property(
    TARGET sc88_memory_test
    PROPERTY FOLDER "Ronaldo/88emu/tests"
)
