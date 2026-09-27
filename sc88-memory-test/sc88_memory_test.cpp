#include "88lib/boards/sc88.h"
#include "88lib/rom/rom.h"
#include "synthLib/midiTypes.h"
#include "synthLib/dac.h"
#include "custom_chips/xp/xp.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
void WriteLE16(
        std::ofstream& File,
        uint16_t Value
)
{
        const char Data[2] = {
                static_cast<char>(Value & 0xFF),
                static_cast<char>((Value >> 8) & 0xFF)
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
                static_cast<char>(Value & 0xFF),
                static_cast<char>((Value >> 8) & 0xFF),
                static_cast<char>((Value >> 16) & 0xFF),
                static_cast<char>((Value >> 24) & 0xFF)
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
                SampleRate * 2U * sizeof(int16_t)
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

int16_t ConvertDacSample(
        int32_t Sample
)
{
        constexpr uint8_t DacBits = 18;

        constexpr float DacScale =
                1.0f /
                static_cast<float>(
                        xpLib::XP::outputFullScale
                );

        const float Value =
                static_cast<float>(
                        synthLib::quantiseDacWord(
                                Sample,
                                DacBits
                        )
                ) * DacScale;

        return static_cast<int16_t>(
                std::lround(
                        std::clamp(
                                Value,
                                -1.0f,
                                1.0f
                        ) * 32767.0f
                )
        );
}

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
                static_cast<size_t>(Size) != ExpectedSize
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
                        reinterpret_cast<char*>(Data.data()),
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

std::string ReadLcdText(
        const emu88Lib::Sc88& Board
)
{
        const auto& Lcd = Board.lcd();

        std::string Text;

        for (
                unsigned Line = 0;
                Line < Lcd.getVisibleLines();
                ++Line
        )
        {
                if (Line != 0)
                        Text += '\n';

                for (
                        unsigned Column = 0;
                        Column < Lcd.getVisibleColumns();
                        ++Column
                )
                {
                        Text += static_cast<char>(
                                Lcd.getVisibleCharacter(
                                        Line,
                                        Column
                                )
                        );
                }
        }

        return Text;
}

void SendShortMessage(
        emu88Lib::Sc88& Board,
        uint8_t Status,
        uint8_t Data1,
        uint8_t Data2 = 0
)
{
        const synthLib::SMidiEvent Event(
                synthLib::MidiEventSource::Host,
                Status,
                Data1,
                Data2,
                0
        );

        Board.addMidiEvent(
                Event,
                emu88Lib::Sc88::MidiInA
        );
}

struct AudioStats
{
        uint64_t Frames = 0;
        uint64_t NonZeroFrames = 0;
        int64_t PeakLeft = 0;
        int64_t PeakRight = 0;
};

struct PcmStats
{
        uint64_t Frames = 0;
        uint64_t NonZeroFrames = 0;
        int32_t PeakLeft = 0;
        int32_t PeakRight = 0;
};

void RenderFrames(
        emu88Lib::Sc88& Board,
        uint32_t Frames,
        AudioStats* Stats
)
{
        for (
                uint32_t FrameIndex = 0;
                FrameIndex < Frames;
                ++FrameIndex
        )
        {
                const auto Frame =
                        Board.renderSample();

                if (!Stats)
                        continue;

                ++Stats->Frames;

                const int64_t Left =
                        static_cast<int64_t>(
                                Frame.first
                        );

                const int64_t Right =
                        static_cast<int64_t>(
                                Frame.second
                        );

                const int64_t AbsLeft =
                        Left < 0 ? -Left : Left;

                const int64_t AbsRight =
                        Right < 0 ? -Right : Right;

                if (AbsLeft > Stats->PeakLeft)
                        Stats->PeakLeft = AbsLeft;

                if (AbsRight > Stats->PeakRight)
                        Stats->PeakRight = AbsRight;

                if (Left != 0 || Right != 0)
                        ++Stats->NonZeroFrames;
        }
}

void RenderPcmFrames(
        emu88Lib::Sc88& Board,
        uint32_t Frames,
        std::vector<int16_t>& Samples,
        PcmStats& Stats,
        AudioStats* RawStats
)
{
        Samples.reserve(
                Samples.size() +
                static_cast<size_t>(Frames) * 2U
        );

        for (
                uint32_t FrameIndex = 0;
                FrameIndex < Frames;
                ++FrameIndex
        )
        {
                const auto Frame =
                        Board.renderSample();

                if (RawStats)
                {
                        ++RawStats->Frames;

                        const int64_t RawLeft =
                                static_cast<int64_t>(
                                        Frame.first
                                );

                        const int64_t RawRight =
                                static_cast<int64_t>(
                                        Frame.second
                                );

                        const int64_t AbsRawLeft =
                                RawLeft < 0
                                        ? -RawLeft
                                        : RawLeft;

                        const int64_t AbsRawRight =
                                RawRight < 0
                                        ? -RawRight
                                        : RawRight;

                        if (AbsRawLeft > RawStats->PeakLeft)
                                RawStats->PeakLeft = AbsRawLeft;

                        if (AbsRawRight > RawStats->PeakRight)
                                RawStats->PeakRight = AbsRawRight;

                        if (RawLeft != 0 || RawRight != 0)
                                ++RawStats->NonZeroFrames;
                }

                const int16_t Left =
                        ConvertDacSample(
                                Frame.first
                        );

                const int16_t Right =
                        ConvertDacSample(
                                Frame.second
                        );

                Samples.push_back(Left);
                Samples.push_back(Right);

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

        constexpr uint32_t SampleRate = 32000;
        constexpr uint32_t BootFrames =
                SampleRate * 10U;

        std::cout
                << "ROM path: "
                << RomPath
                << '\n';

        auto ControlRom = ReadFile(
                RomPath + "/sc88_control.bin",
                emu88Lib::Sc88::RomSize
        );

        if (ControlRom.empty())
        {
                std::cerr
                        << "SC88_CONTROL_ROM=FAIL\n";

                return 1;
        }

        std::array<
                std::vector<uint8_t>,
                emu88Lib::WaveRom::ChipCount
        > WaveChips;

        for (
                size_t Chip = 0;
                Chip < WaveChips.size();
                ++Chip
        )
        {
                const std::string Path =
                        RomPath +
                        "/sc88_wave" +
                        std::to_string(Chip) +
                        ".bin";

                WaveChips[Chip] = ReadFile(
                        Path,
                        emu88Lib::WaveRom::ChipSize
                );

                if (WaveChips[Chip].empty())
                {
                        std::cerr
                                << "SC88_WAVE_ROM_"
                                << Chip
                                << "=FAIL\n";

                        return 2;
                }
        }

        std::cout
                << "Control ROM size: "
                << ControlRom.size()
                << '\n';

        for (
                size_t Chip = 0;
                Chip < WaveChips.size();
                ++Chip
        )
        {
                std::cout
                        << "Wave ROM "
                        << Chip
                        << " size: "
                        << WaveChips[Chip].size()
                        << '\n';
        }

        const auto DecodeStart =
                std::chrono::steady_clock::now();

        emu88Lib::WaveRom WaveRom(
                WaveChips
        );

        const auto DecodeEnd =
                std::chrono::steady_clock::now();

        if (!WaveRom.isValid())
        {
                std::cerr
                        << "SC88_WAVE_DECODE=FAIL\n";

                return 3;
        }

        auto CombinedWaveRom =
                WaveRom.takeData();

        std::cout
                << "Combined Wave ROM size: "
                << CombinedWaveRom.size()
                << '\n';

        if (
                CombinedWaveRom.size() !=
                emu88Lib::WaveRom::Size
        )
        {
                std::cerr
                        << "SC88_COMBINED_WAVE_SIZE=FAIL\n";

                return 4;
        }

        const double DecodeSeconds =
                std::chrono::duration<double>(
                        DecodeEnd - DecodeStart
                ).count();

        std::cout
                << "Wave decode CPU time: "
                << DecodeSeconds
                << " seconds\n";

        const auto CreateStart =
                std::chrono::steady_clock::now();

        emu88Lib::Sc88 Board(
                std::move(ControlRom),
                std::move(CombinedWaveRom),
                emu88Lib::Sc88::Model::Sc88,
                true
        );

        const auto CreateEnd =
                std::chrono::steady_clock::now();

        const double CreateSeconds =
                std::chrono::duration<double>(
                        CreateEnd - CreateStart
                ).count();

        std::cout
                << "Board construction CPU time: "
                << CreateSeconds
                << " seconds\n";

        const auto BootStart =
                std::chrono::steady_clock::now();

        RenderFrames(
                Board,
                BootFrames,
                nullptr
        );

        const auto BootEnd =
                std::chrono::steady_clock::now();

        const double BootSeconds =
                std::chrono::duration<double>(
                        BootEnd - BootStart
                ).count();

        std::cout
                << "Boot frames: "
                << BootFrames
                << '\n'
                << "Boot CPU time: "
                << BootSeconds
                << " seconds\n"
                << "LCD enabled: "
                << (
                        Board.lcdEnabled()
                                ? 1
                                : 0
                )
                << '\n'
                << "Display:\n"
                << ReadLcdText(Board)
                << '\n';

        AudioStats RawStats;

        std::vector<int16_t> Samples;
        PcmStats Pcm;

        Samples.reserve(
                static_cast<size_t>(
                        SampleRate
                ) * 9U
        );

        const auto RenderStart =
                std::chrono::steady_clock::now();

        RenderPcmFrames(
                Board,
                SampleRate / 2U,
                Samples,
                Pcm,
                &RawStats
        );

        SendShortMessage(
                Board,
                0xC0,
                0
        );

        SendShortMessage(
                Board,
                0x90,
                60,
                100
        );

        RenderPcmFrames(
                Board,
                SampleRate * 2U,
                Samples,
                Pcm,
                &RawStats
        );

        SendShortMessage(
                Board,
                0x80,
                60,
                0
        );

        RenderPcmFrames(
                Board,
                SampleRate * 2U,
                Samples,
                Pcm,
                &RawStats
        );

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
                        SampleRate
                );

        std::cout
                << "Raw validation frames: "
                << RawStats.Frames
                << '\n'
                << "Non-zero raw frames: "
                << RawStats.NonZeroFrames
                << '\n'
                << "Raw peak left: "
                << RawStats.PeakLeft
                << '\n'
                << "Raw peak right: "
                << RawStats.PeakRight
                << '\n'
                << "PCM rendered frames: "
                << Pcm.Frames
                << '\n'
                << "Non-zero PCM frames: "
                << Pcm.NonZeroFrames
                << '\n'
                << "PCM peak left: "
                << Pcm.PeakLeft
                << '\n'
                << "PCM peak right: "
                << Pcm.PeakRight
                << '\n'
                << "Render CPU time: "
                << RenderSeconds
                << " seconds\n"
                << "MIDI backlog: "
                << Board.midiInBacklog()
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

        if (RawStats.NonZeroFrames == 0)
        {
                std::cerr
                        << "SC88_MEMORY_RAW_AUDIO=FAIL_SILENCE\n";

                return 5;
        }

        if (Pcm.NonZeroFrames == 0)
        {
                std::cerr
                        << "SC88_MEMORY_PCM_AUDIO=FAIL_SILENCE\n";

                return 6;
        }

        if (!WavOK)
        {
                std::cerr
                        << "SC88_MEMORY_WAV_WRITE=FAIL\n";

                return 7;
        }

        std::cout
                << "SC88_MEMORY_ROM_LOAD=PASS\n"
                << "SC88_MEMORY_BOOT=PASS\n"
                << "SC88_MEMORY_MIDI=PASS\n"
                << "SC88_MEMORY_RAW_AUDIO=PASS\n"
                << "SC88_MEMORY_PCM_AUDIO=PASS\n"
                << "SC88_MEMORY_WAV_WRITE=PASS\n";

        return 0;
}
