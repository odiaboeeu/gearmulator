# SC-88 headless test

Experimental headless test for the Roland SC-88 emulation provided by
Gearmulator 88emu.

The test uses the public C interface from `88lib/c_interface.h` and the
bundled static library `lib88emu.a`.

## Current status

Validated on Linux x86-64 using the interpreter implementations of the
custom chips, with JIT disabled.

Validated behavior:

- SC-88 ROM set detection
- SC-88 firmware boot
- Two MIDI input ports
- MIDI Program Change
- MIDI Note On and Note Off
- Stereo 16-bit PCM rendering
- SC-88 front-panel text retrieval
- WAV output
- Non-silent audio output

Validated native sample rate:

    32000 Hz

Validated front-panel text after boot:

    A01001 Piano 1

## ROM files

ROM files are not included and must never be committed to this repository.

The test expects the following files:

    sc88_control.bin     524288 bytes
    sc88_wave0.bin      2097152 bytes
    sc88_wave1.bin      2097152 bytes
    sc88_wave2.bin      2097152 bytes
    sc88_wave3.bin      2097152 bytes

The default ROM directory is:

    /home/nelso/roms-sc88

A different ROM directory may be passed as the first argument.

## Configure lib88emu

Run from the Gearmulator repository root:

    rm -rf build-sc88-headless

    cmake -S . -B build-sc88-headless -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -Dgearmulator_BUILD_JUCEPLUGIN=OFF -Dgearmulator_SYNTH_OSIRUS=OFF -Dgearmulator_SYNTH_OSTIRUS=OFF -Dgearmulator_SYNTH_VAVRA=OFF -Dgearmulator_SYNTH_XENIA=OFF -Dgearmulator_SYNTH_NODALRED2X=OFF -Dgearmulator_SYNTH_JE8086=OFF -Dgearmulator_SYNTH_88EMU=ON -Dgearmulator_BUILD_SC88_HEADLESS_TEST=ON -DCHIPS_FORCE_NO_JIT=ON

The expected configuration includes:

    Custom-chip JIT through asmjit: FALSE
    Custom-chip JIT through wasmJit: FALSE

## Build lib88emu

    cmake --build build-sc88-headless --target 88emu_bundle --parallel "$(nproc)"

The resulting static library is:

    build-sc88-headless/source/ronaldo/88emu/88lib/lib88emu.a

## Build the test

Enable the test target while configuring:

    -Dgearmulator_BUILD_SC88_HEADLESS_TEST=ON

Build the target:

    cmake --build build-sc88-headless --target sc88_headless_test --parallel "$(nproc)"

Locate the resulting executable:

    find build-sc88-headless -type f -name sc88_headless_test -perm -111

## Run

    build-sc88-headless/sc88-headless-test/sc88_headless_test /home/nelso/roms-sc88 sc88-headless-test/sc88_test.wav

Expected final result:

    SC88_AUDIO_OUTPUT=PASS

## Initial validation

The initial Linux x86-64 validation produced:

    88emu library version: 2.2.26
    SC-88 available: 1
    Device sample rate: 32000
    Output sample rate: 32000
    MIDI ports: 2
    Rendered frames: 144000
    Non-zero frames: 101428
    Peak left: 1643
    Peak right: 1646
    WAV write: PASS
    SC88_AUDIO_OUTPUT=PASS

## Linux AArch64 cross-build

The SC-88 headless target has also been cross-compiled for Linux AArch64
with JIT disabled.

Required Ubuntu packages:

    sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu

Configure:

    rm -rf build-sc88-aarch64

    cmake -S . -B build-sc88-aarch64 -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/aarch64-linux-gnu.cmake" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=OFF -Dgearmulator_BUILD_JUCEPLUGIN=OFF -Dgearmulator_SYNTH_OSIRUS=OFF -Dgearmulator_SYNTH_OSTIRUS=OFF -Dgearmulator_SYNTH_VAVRA=OFF -Dgearmulator_SYNTH_XENIA=OFF -Dgearmulator_SYNTH_NODALRED2X=OFF -Dgearmulator_SYNTH_JE8086=OFF -Dgearmulator_SYNTH_88EMU=ON -Dgearmulator_BUILD_SC88_HEADLESS_TEST=ON -DCHIPS_FORCE_NO_JIT=ON

Build:

    cmake --build build-sc88-aarch64 --target sc88_headless_test --parallel "$(nproc)"

Resulting executable:

    build-sc88-aarch64/sc88-headless-test/sc88_headless_test

Validated properties:

    ELF64
    AArch64
    Little endian
    JIT disabled
    725 AArch64 compile commands
    No SSE, AVX or x86 compile flags
    Linux interpreter: /lib/ld-linux-aarch64.so.1

The resulting executable is dynamically linked for Linux AArch64. It is
not yet a bare-metal Circle binary.

## Scope

The current experiment targets only the Roland SC-88.

SC-88Pro support will be investigated after the SC-88 headless and
bare-metal paths are stable.

This is an experimental integration and is not an official Gearmulator
or 88emu release.
