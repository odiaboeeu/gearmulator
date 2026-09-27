#include "88lib/rom/rom.h"

#include "common/romDescramble.h"

namespace emu88Lib
{
std::vector<uint8_t> WaveRom::decodeXpWaveDump(
    const std::vector<uint8_t>& Raw
)
{
    if (Raw.size() % 0x100000 != 0)
        return {};

    std::vector<uint8_t> Decoded(
        Raw.size()
    );

    for (
        size_t Index = 0;
        Index < Raw.size();
        ++Index
    )
    {
        Decoded[
            rLib::rom::Pcm16::physicalAddress(
                Index
            )
        ] = rLib::rom::Pcm16::descrambleByte(
            Raw[Index]
        );
    }

    return Decoded;
}

void WaveRom::unscramble(
    const uint8_t* Raw,
    const size_t RawLength,
    uint8_t* Destination,
    const size_t DestinationCapacity
)
{
    rLib::rom::Pcm16::descramble(
        Raw,
        RawLength,
        Destination,
        DestinationCapacity
    );
}

WaveRom::WaveRom(
    const std::array<
        std::vector<uint8_t>,
        ChipCount
    >& Chips
)
{
    for (
        size_t Index = 0;
        Index < Chips.size();
        ++Index
    )
    {
        if (
            Chips[Index].size() !=
            ChipSize
        )
        {
            return;
        }
    }

    m_data.resize(Size);

    for (
        size_t Bank = 0;
        Bank < ChipCount;
        ++Bank
    )
    {
        unscramble(
            Chips[Bank].data(),
            ChipSize,
            m_data.data() +
                Bank * ChipSize,
            ChipSize
        );
    }
}
}
