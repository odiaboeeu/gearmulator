add_library(sc88_headless_display STATIC
    ${CMAKE_SOURCE_DIR}/source/framework/hardwareLib/hd44780.cpp
)

target_include_directories(sc88_headless_display
    PUBLIC
        ${CMAKE_SOURCE_DIR}/source/framework
)

target_compile_features(sc88_headless_display
    PUBLIC
        cxx_std_20
)

set_property(
    TARGET sc88_headless_display
    PROPERTY FOLDER "Ronaldo/88emu/headless"
)

add_library(sc88_headless_midi STATIC
    ${CMAKE_SOURCE_DIR}/source/framework/synthLib/midiBufferParser.cpp
    ${CMAKE_SOURCE_DIR}/source/framework/synthLib/midiRateLimiter.cpp
)

target_include_directories(sc88_headless_midi
    PUBLIC
        ${CMAKE_SOURCE_DIR}/source/framework
)

target_compile_features(sc88_headless_midi
    PUBLIC
        cxx_std_20
)

set_property(
    TARGET sc88_headless_midi
    PROPERTY FOLDER "Ronaldo/88emu/headless"
)

add_library(sc88_headless_engine STATIC
    ${CMAKE_SOURCE_DIR}/source/ronaldo/88emu/88lib/boards/sc88.cpp
    ${CMAKE_SOURCE_DIR}/source/ronaldo/88emu/88lib/mcu/sc88_submcu.cpp
    sc88_wave_rom.cpp
)

target_include_directories(sc88_headless_engine
    PRIVATE
        ${CMAKE_SOURCE_DIR}/source/ronaldo/88emu
        ${CMAKE_SOURCE_DIR}/source/ronaldo
)

target_link_libraries(sc88_headless_engine
    PUBLIC
        sc88_headless_display
        sc88_headless_midi
        h8500
        xpLib
)

target_compile_features(sc88_headless_engine
    PUBLIC
        cxx_std_20
)

set_property(
    TARGET sc88_headless_engine
    PROPERTY FOLDER "Ronaldo/88emu/headless"
)

add_library(sc88_headless_core STATIC
    sc88_headless_core.cpp
)

target_include_directories(sc88_headless_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/source/ronaldo/88emu
        ${CMAKE_SOURCE_DIR}/source/ronaldo
        ${CMAKE_SOURCE_DIR}/source/framework
)

target_link_libraries(sc88_headless_core
    PRIVATE
        sc88_headless_engine
)

target_compile_features(sc88_headless_core
    PUBLIC
        cxx_std_20
)

set_property(
    TARGET sc88_headless_core
    PROPERTY FOLDER "Ronaldo/88emu/headless"
)
