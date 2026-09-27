#include "88lib/c_interface.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
void WriteLE16(std::ofstream& File, uint16_t Value)
{
        const char Data[2] = {
                static_cast<char>(Value & 0xFF),
                static_cast<char>((Value >> 8) & 0xFF)
        };

        File.write(Data, sizeof(Data));
}

void WriteLE32(std::ofstream& File, uint32_t Value)
{
        const char Data[4] = {
                static_cast<char>(Value & 0xFF),
                static_cast<char>((Value >> 8) & 0xFF),
                static_cast<char>((Value >> 16) & 0xFF),
                static_cast<char>((Value >> 24) & 0xFF)
        };

        File.write(Data, sizeof(Data));
}

bool WriteWAV(
        const std::string& Path,
        const std::vector<int16_t>& Samples,
        uint32_t SampleRate
)
{
        if (
                Samples.size() >
                std::numeric_limits<uint32_t>::max() / sizeof(int16_t)
        )
        {
                std::cerr << "Audio output is too large for RIFF/WAV\n";
                return false;
        }

        const uint32_t DataSize =
                static_cast<uint32_t>(
                        Samples.size() * sizeof(int16_t)
                );

        const uint32_t RiffSize = 36U + DataSize;

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
        WriteLE32(File, SampleRate * 2U * sizeof(int16_t));
        WriteLE16(File, 2U * sizeof(int16_t));
        WriteLE16(File, 16);

        File.write("data", 4);
        WriteLE32(File, DataSize);

        File.write(
                reinterpret_cast<const char*>(Samples.data()),
                static_cast<std::streamsize>(DataSize)
        );

        return File.good();
}

void RenderAppend(
        emu88_context Context,
        std::vector<int16_t>& Destination,
        uint32_t Frames
)
{
        constexpr uint32_t BlockFrames = 512;

        std::vector<int16_t> Block(
                static_cast<size_t>(BlockFrames) * 2U
        );

        uint32_t Remaining = Frames;

        while (Remaining > 0)
        {
                const uint32_t Count =
                        std::min(Remaining, BlockFrames);

                emu88_render_bit16s(
                        Context,
                        Block.data(),
                        Count
                );

                Destination.insert(
                        Destination.end(),
                        Block.begin(),
                        Block.begin() +
                                static_cast<ptrdiff_t>(Count * 2U)
                );

                Remaining -= Count;
        }
}

uint32_t PackMIDI(
        uint8_t Status,
        uint8_t Data1,
        uint8_t Data2 = 0
)
{
        return
                static_cast<uint32_t>(Status) |
                (
                        static_cast<uint32_t>(Data1)
                        << 8
                ) |
                (
                        static_cast<uint32_t>(Data2)
                        << 16
                );
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
                        : "sc88_test.wav";

        std::cout
                << "88emu library version: "
                << emu88_get_library_version_string()
                << '\n';

        std::cout
                << "ROM path: "
                << RomPath
                << '\n';

        emu88_return_code Result =
                emu88_set_rom_path(
                        RomPath.c_str()
                );

        if (Result != EMU88_RC_OK)
        {
                std::cerr
                        << "emu88_set_rom_path failed: "
                        << static_cast<int>(Result)
                        << '\n';

                return 1;
        }

        const int Available =
                emu88_is_device_available(
                        EMU88_DEVICE_SC88
                );

        std::cout
                << "SC-88 available: "
                << Available
                << '\n';

        if (!Available)
        {
                const size_t Length =
                        emu88_describe_device_roms(
                                EMU88_DEVICE_SC88,
                                nullptr,
                                0
                        );

                std::vector<char> Description(
                        Length + 1U,
                        '\0'
                );

                emu88_describe_device_roms(
                        EMU88_DEVICE_SC88,
                        Description.data(),
                        Description.size()
                );

                std::cerr
                        << "SC-88 ROM requirements:\n"
                        << Description.data()
                        << '\n';

                return 2;
        }

        emu88_context Context =
                emu88_create_context();

        if (!Context)
        {
                std::cerr
                        << "emu88_create_context failed\n";

                return 3;
        }

        Result = emu88_select_device(
                Context,
                EMU88_DEVICE_SC88
        );

        if (Result != EMU88_RC_OK)
        {
                std::cerr
                        << "emu88_select_device failed: "
                        << static_cast<int>(Result)
                        << '\n';

                emu88_free_context(Context);
                return 4;
        }

        emu88_set_boot_flags(
                Context,
                EMU88_BOOT_DEFAULT
        );

        const auto OpenStart =
                std::chrono::steady_clock::now();

        Result = emu88_open_synth(Context);

        const auto OpenEnd =
                std::chrono::steady_clock::now();

        if (Result != EMU88_RC_OK)
        {
                std::cerr
                        << "emu88_open_synth failed: "
                        << static_cast<int>(Result)
                        << '\n';

                emu88_free_context(Context);
                return 5;
        }

        const double OpenSeconds =
                std::chrono::duration<double>(
                        OpenEnd - OpenStart
                ).count();

        const uint32_t DeviceRate =
                emu88_get_device_samplerate(Context);

        const uint32_t OutputRate =
                emu88_get_actual_stereo_output_samplerate(
                        Context
                );

        const int MidiPorts =
                emu88_get_midi_port_count(Context);

        std::cout
                << "Open: PASS\n"
                << "Device sample rate: "
                << DeviceRate
                << '\n'
                << "Output sample rate: "
                << OutputRate
                << '\n'
                << "MIDI ports: "
                << MidiPorts
                << '\n'
                << "Open CPU time: "
                << OpenSeconds
                << " seconds\n";

        if (OutputRate == 0)
        {
                std::cerr
                        << "Invalid output sample rate\n";

                emu88_close_synth(Context);
                emu88_free_context(Context);
                return 6;
        }

        char Display[256] = {};

        const size_t DisplayLength =
                emu88_get_display_text(
                        Context,
                        0,
                        Display,
                        sizeof(Display)
                );

        std::cout
                << "Display length: "
                << DisplayLength
                << '\n';

        if (DisplayLength > 0)
        {
                std::cout
                        << "Display:\n"
                        << Display
                        << '\n';
        }

        std::vector<int16_t> Samples;

        Samples.reserve(
                static_cast<size_t>(OutputRate) *
                5U *
                2U
        );

        const auto RenderStart =
                std::chrono::steady_clock::now();

        RenderAppend(
                Context,
                Samples,
                OutputRate / 2U
        );

        Result = emu88_play_msg(
                Context,
                PackMIDI(0xC0, 0)
        );

        if (Result != EMU88_RC_OK)
        {
                std::cerr
                        << "Program Change failed: "
                        << static_cast<int>(Result)
                        << '\n';
        }

        Result = emu88_play_msg(
                Context,
                PackMIDI(0x90, 60, 100)
        );

        if (Result != EMU88_RC_OK)
        {
                std::cerr
                        << "Note On failed: "
                        << static_cast<int>(Result)
                        << '\n';
        }

        RenderAppend(
                Context,
                Samples,
                OutputRate * 2U
        );

        Result = emu88_play_msg(
                Context,
                PackMIDI(0x80, 60, 0)
        );

        if (Result != EMU88_RC_OK)
        {
                std::cerr
                        << "Note Off failed: "
                        << static_cast<int>(Result)
                        << '\n';
        }

        RenderAppend(
                Context,
                Samples,
                OutputRate * 2U
        );

        const auto RenderEnd =
                std::chrono::steady_clock::now();

        const double RenderSeconds =
                std::chrono::duration<double>(
                        RenderEnd - RenderStart
                ).count();

        uint64_t NonZeroFrames = 0;

        for (size_t Frame = 0;
             Frame + 1 < Samples.size();
             Frame += 2)
        {
                const int16_t Left =
                        Samples[Frame];

                const int16_t Right =
                        Samples[Frame + 1];

                const int32_t AbsLeft =
                        Left == std::numeric_limits<int16_t>::min()
                                ? 32768
                                : std::abs(
                                        static_cast<int>(Left)
                                );

                const int32_t AbsRight =
                        Right == std::numeric_limits<int16_t>::min()
                                ? 32768
                                : std::abs(
                                        static_cast<int>(Right)
                                );

                if (Left != 0 || Right != 0)
                        ++NonZeroFrames;
        }

        int32_t ActualPeakLeft = 0;
        int32_t ActualPeakRight = 0;

        for (size_t Frame = 0;
             Frame + 1 < Samples.size();
             Frame += 2)
        {
                ActualPeakLeft = std::max(
                        ActualPeakLeft,
                        std::abs(
                                static_cast<int>(
                                        Samples[Frame]
                                )
                        )
                );

                ActualPeakRight = std::max(
                        ActualPeakRight,
                        std::abs(
                                static_cast<int>(
                                        Samples[Frame + 1]
                                )
                        )
                );
        }

        const bool WavOK =
                WriteWAV(
                        WavPath,
                        Samples,
                        OutputRate
                );

        std::cout
                << "Rendered frames: "
                << Samples.size() / 2U
                << '\n'
                << "Non-zero frames: "
                << NonZeroFrames
                << '\n'
                << "Peak left: "
                << ActualPeakLeft
                << '\n'
                << "Peak right: "
                << ActualPeakRight
                << '\n'
                << "Render CPU time: "
                << RenderSeconds
                << " seconds\n"
                << "WAV output: "
                << WavPath
                << '\n'
                << "WAV write: "
                << (WavOK ? "PASS" : "FAIL")
                << '\n';

        emu88_close_synth(Context);
        emu88_free_context(Context);

        if (!WavOK)
                return 7;

        if (NonZeroFrames == 0)
        {
                std::cerr
                        << "SC88_AUDIO_OUTPUT=FAIL_SILENCE\n";

                return 8;
        }

        std::cout
                << "SC88_AUDIO_OUTPUT=PASS\n";

        return 0;
}
