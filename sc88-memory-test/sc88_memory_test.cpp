#include "sc88_headless_core.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
struct ContextDeleter
{
    void operator()(
        sc88_headless_context* Context
    ) const
    {
        sc88_headless_destroy(Context);
    }
};

using ContextPointer =
    std::unique_ptr<
        sc88_headless_context,
        ContextDeleter
    >;

std::vector<uint8_t> ReadFile(
    const std::string& Path,
    size_t ExpectedSize
)
{
    std::ifstream File(
        Path,
        std::ios::binary |
        std::ios::ate
    );

    if (!File)
    {
        std::cerr
            << "Unable to open ROM: "
            << Path
            << '\n';

        return {};
    }

    const std::streamsize Size =
        File.tellg();

    if (
        Size < 0 ||
        static_cast<size_t>(Size) !=
            ExpectedSize
    )
    {
        std::cerr
            << "Invalid ROM size: "
            << Path
            << " actual="
            << Size
            << " expected="
            << ExpectedSize
            << '\n';

        return {};
    }

    File.seekg(
        0,
        std::ios::beg
    );

    std::vector<uint8_t> Data(
        static_cast<size_t>(Size)
    );

    if (
        !File.read(
            reinterpret_cast<char*>(
                Data.data()
            ),
            Size
        )
    )
    {
        std::cerr
            << "Unable to read ROM: "
            << Path
            << '\n';

        return {};
    }

    return Data;
}

void WriteLE16(
    std::ofstream& File,
    uint16_t Value
)
{
    const char Data[2] = {
        static_cast<char>(
            Value & 0xFF
        ),
        static_cast<char>(
            (Value >> 8) & 0xFF
        )
    };

    File.write(
        Data,
        sizeof(Data)
    );
}

void WriteLE32(
    std::ofstream& File,
    uint32_t Value
)
{
    const char Data[4] = {
        static_cast<char>(
            Value & 0xFF
        ),
        static_cast<char>(
            (Value >> 8) & 0xFF
        ),
        static_cast<char>(
            (Value >> 16) & 0xFF
        ),
        static_cast<char>(
            (Value >> 24) & 0xFF
        )
    };

    File.write(
        Data,
        sizeof(Data)
    );
}

bool WriteWAV(
    const std::string& Path,
    const std::vector<int16_t>& Samples,
    uint32_t SampleRate
)
{
    if (
        Samples.size() >
        std::numeric_limits<uint32_t>::max() /
            sizeof(int16_t)
    )
    {
        std::cerr
            << "Audio output is too large for RIFF/WAV\n";

        return false;
    }

    const uint32_t DataSize =
        static_cast<uint32_t>(
            Samples.size() *
            sizeof(int16_t)
        );

    const uint32_t RiffSize =
        36U + DataSize;

    std::ofstream File(
        Path,
        std::ios::binary
    );

    if (!File)
    {
        std::cerr
            << "Unable to create WAV file: "
            << Path
            << '\n';

        return false;
    }

    File.write("RIFF", 4);
    WriteLE32(File, RiffSize);
    File.write("WAVE", 4);

    File.write("fmt ", 4);
    WriteLE32(File, 16);
    WriteLE16(File, 1);
    WriteLE16(File, 2);
    WriteLE32(File, SampleRate);
    WriteLE32(
        File,
        SampleRate * 2U *
            sizeof(int16_t)
    );
    WriteLE16(
        File,
        2U * sizeof(int16_t)
    );
    WriteLE16(File, 16);

    File.write("data", 4);
    WriteLE32(File, DataSize);

    File.write(
        reinterpret_cast<const char*>(
            Samples.data()
        ),
        static_cast<std::streamsize>(
            DataSize
        )
    );

    return File.good();
}

struct PcmStats
{
    uint64_t Frames = 0;
    uint64_t NonZeroFrames = 0;
    int32_t PeakLeft = 0;
    int32_t PeakRight = 0;
};

void UpdateStats(
    const int16_t Left,
    const int16_t Right,
    PcmStats& Stats
)
{
    ++Stats.Frames;

    const int32_t AbsLeft =
        Left < 0
            ? -static_cast<int32_t>(Left)
            : static_cast<int32_t>(Left);

    const int32_t AbsRight =
        Right < 0
            ? -static_cast<int32_t>(Right)
            : static_cast<int32_t>(Right);

    if (AbsLeft > Stats.PeakLeft)
        Stats.PeakLeft = AbsLeft;

    if (AbsRight > Stats.PeakRight)
        Stats.PeakRight = AbsRight;

    if (Left != 0 || Right != 0)
        ++Stats.NonZeroFrames;
}

bool RenderFrames(
    sc88_headless_context* Context,
    uint32_t Frames,
    std::vector<int16_t>& Samples,
    PcmStats& Stats
)
{
    if (!Context)
        return false;

    const size_t OldSize =
        Samples.size();

    const size_t AdditionalSamples =
        static_cast<size_t>(Frames) * 2U;

    if (
        AdditionalSamples >
        Samples.max_size() - OldSize
    )
    {
        return false;
    }

    Samples.resize(
        OldSize + AdditionalSamples
    );

    sc88_headless_render_int16(
        Context,
        Samples.data() + OldSize,
        Frames
    );

    for (
        size_t Index = OldSize;
        Index < Samples.size();
        Index += 2U
    )
    {
        UpdateStats(
            Samples[Index],
            Samples[Index + 1U],
            Stats
        );
    }

    return true;
}
}

int main(int argc, char** argv)
{
    const std::string RomPath =
        argc >= 2
            ? argv[1]
            : "/home/nelso/roms-sc88";

    const std::string WavPath =
        argc >= 3
            ? argv[2]
            : "sc88-memory-test/sc88_memory_test.wav";

    std::cout
        << "ROM path: "
        << RomPath
        << '\n';

    auto ControlRom = ReadFile(
        RomPath + "/sc88_control.bin",
        SC88_HEADLESS_CONTROL_ROM_SIZE
    );

    if (ControlRom.empty())
    {
        std::cerr
            << "SC88_CONTROL_ROM=FAIL\n";

        return 1;
    }

    std::array<
        std::vector<uint8_t>,
        SC88_HEADLESS_WAVE_ROM_COUNT
    > WaveRoms;

    for (
        size_t Index = 0;
        Index < WaveRoms.size();
        ++Index
    )
    {
        const std::string Path =
            RomPath +
            "/sc88_wave" +
            std::to_string(Index) +
            ".bin";

        WaveRoms[Index] = ReadFile(
            Path,
            SC88_HEADLESS_WAVE_ROM_SIZE
        );

        if (WaveRoms[Index].empty())
        {
            std::cerr
                << "SC88_WAVE_ROM_"
                << Index
                << "=FAIL\n";

            return 2;
        }
    }

    std::cout
        << "Control ROM size: "
        << ControlRom.size()
        << '\n';

    for (
        size_t Index = 0;
        Index < WaveRoms.size();
        ++Index
    )
    {
        std::cout
            << "Wave ROM "
            << Index
            << " size: "
            << WaveRoms[Index].size()
            << '\n';
    }

    sc88_headless_rom_set RomSet{};

    RomSet.control =
        ControlRom.data();

    RomSet.control_size =
        ControlRom.size();

    for (
        size_t Index = 0;
        Index < WaveRoms.size();
        ++Index
    )
    {
        RomSet.wave[Index] =
            WaveRoms[Index].data();

        RomSet.wave_size[Index] =
            WaveRoms[Index].size();
    }

    const auto CreateStart =
        std::chrono::steady_clock::now();

    ContextPointer Context(
        sc88_headless_create(
            &RomSet
        )
    );

    const auto CreateEnd =
        std::chrono::steady_clock::now();

    const double CreateSeconds =
        std::chrono::duration<double>(
            CreateEnd - CreateStart
        ).count();

    if (
        !Context ||
        !sc88_headless_is_valid(
            Context.get()
        )
    )
    {
        std::cerr
            << "SC88_HEADLESS_CREATE=FAIL\n";

        return 3;
    }

    std::cout
        << "Context creation CPU time: "
        << CreateSeconds
        << " seconds\n";

    const auto BootStart =
        std::chrono::steady_clock::now();

    const int BootOK =
        sc88_headless_boot(
            Context.get()
        );

    const auto BootEnd =
        std::chrono::steady_clock::now();

    const double BootSeconds =
        std::chrono::duration<double>(
            BootEnd - BootStart
        ).count();

    if (!BootOK)
    {
        std::cerr
            << "SC88_HEADLESS_BOOT=FAIL\n";

        return 4;
    }

    std::array<char, 256> Display{};

    const size_t DisplayLength =
        sc88_headless_get_display_text(
            Context.get(),
            Display.data(),
            Display.size()
        );

    std::cout
        << "Sample rate: "
        << sc88_headless_sample_rate()
        << '\n'
        << "Boot frames: "
        << SC88_HEADLESS_SAMPLE_RATE * 10U
        << '\n'
        << "Boot CPU time: "
        << BootSeconds
        << " seconds\n"
        << "Display length: "
        << DisplayLength
        << '\n'
        << "Display:\n"
        << Display.data()
        << '\n';

    std::vector<int16_t> Samples;

    Samples.reserve(
        static_cast<size_t>(
            SC88_HEADLESS_SAMPLE_RATE
        ) * 9U
    );

    PcmStats Stats;

    const auto RenderStart =
        std::chrono::steady_clock::now();

    if (
        !RenderFrames(
            Context.get(),
            SC88_HEADLESS_SAMPLE_RATE / 2U,
            Samples,
            Stats
        )
    )
    {
        std::cerr
            << "SC88_HEADLESS_RENDER=FAIL\n";

        return 5;
    }

    if (
        !sc88_headless_play_short_message(
            Context.get(),
            0,
            0xC0,
            0,
            0
        )
    )
    {
        std::cerr
            << "SC88_HEADLESS_PROGRAM_CHANGE=FAIL\n";

        return 6;
    }

    if (
        !sc88_headless_play_short_message(
            Context.get(),
            0,
            0x90,
            60,
            100
        )
    )
    {
        std::cerr
            << "SC88_HEADLESS_NOTE_ON=FAIL\n";

        return 7;
    }

    if (
        !RenderFrames(
            Context.get(),
            SC88_HEADLESS_SAMPLE_RATE * 2U,
            Samples,
            Stats
        )
    )
    {
        std::cerr
            << "SC88_HEADLESS_RENDER=FAIL\n";

        return 8;
    }

    if (
        !sc88_headless_play_short_message(
            Context.get(),
            0,
            0x80,
            60,
            0
        )
    )
    {
        std::cerr
            << "SC88_HEADLESS_NOTE_OFF=FAIL\n";

        return 9;
    }

    if (
        !RenderFrames(
            Context.get(),
            SC88_HEADLESS_SAMPLE_RATE * 2U,
            Samples,
            Stats
        )
    )
    {
        std::cerr
            << "SC88_HEADLESS_RENDER=FAIL\n";

        return 10;
    }

    const auto RenderEnd =
        std::chrono::steady_clock::now();

    const double RenderSeconds =
        std::chrono::duration<double>(
            RenderEnd - RenderStart
        ).count();

    const bool WavOK =
        WriteWAV(
            WavPath,
            Samples,
            sc88_headless_sample_rate()
        );

    std::cout
        << "PCM rendered frames: "
        << Stats.Frames
        << '\n'
        << "Non-zero PCM frames: "
        << Stats.NonZeroFrames
        << '\n'
        << "PCM peak left: "
        << Stats.PeakLeft
        << '\n'
        << "PCM peak right: "
        << Stats.PeakRight
        << '\n'
        << "Render CPU time: "
        << RenderSeconds
        << " seconds\n"
        << "MIDI backlog: "
        << sc88_headless_midi_backlog(
            Context.get()
        )
        << '\n'
        << "WAV output: "
        << WavPath
        << '\n'
        << "WAV write: "
        << (
            WavOK
                ? "PASS"
                : "FAIL"
        )
        << '\n';

    if (Stats.NonZeroFrames == 0)
    {
        std::cerr
            << "SC88_HEADLESS_AUDIO=FAIL_SILENCE\n";

        return 11;
    }

    if (!WavOK)
    {
        std::cerr
            << "SC88_HEADLESS_WAV_WRITE=FAIL\n";

        return 12;
    }

    std::cout
        << "SC88_HEADLESS_ROM_LOAD=PASS\n"
        << "SC88_HEADLESS_CREATE=PASS\n"
        << "SC88_HEADLESS_BOOT=PASS\n"
        << "SC88_HEADLESS_MIDI=PASS\n"
        << "SC88_HEADLESS_AUDIO=PASS\n"
        << "SC88_HEADLESS_WAV_WRITE=PASS\n";

    return 0;
}
