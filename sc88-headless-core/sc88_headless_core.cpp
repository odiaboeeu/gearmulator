#include "sc88_headless_core.h"

#include "88lib/boards/sc88.h"
#include "88lib/rom/rom.h"
#include "custom_chips/xp/xp.h"
#include "synthLib/dac.h"
#include "synthLib/midiTypes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

struct sc88_headless_context
{
    std::unique_ptr<emu88Lib::Sc88> board;
};

namespace
{
constexpr uint8_t Sc88DacBits = 18;

constexpr float Sc88DacScale =
    1.0f /
    static_cast<float>(
        xpLib::XP::outputFullScale
    );

bool validateRomSet(
    const sc88_headless_rom_set* roms
)
{
    if (!roms)
        return false;

    if (
        !roms->control ||
        roms->control_size !=
            SC88_HEADLESS_CONTROL_ROM_SIZE
    )
    {
        return false;
    }

    for (
        size_t index = 0;
        index < SC88_HEADLESS_WAVE_ROM_COUNT;
        ++index
    )
    {
        if (
            !roms->wave[index] ||
            roms->wave_size[index] !=
                SC88_HEADLESS_WAVE_ROM_SIZE
        )
        {
            return false;
        }
    }

    return true;
}

int16_t convertDacSample(
    const int32_t sample
)
{
    const float value =
        static_cast<float>(
            synthLib::quantiseDacWord(
                sample,
                Sc88DacBits
            )
        ) * Sc88DacScale;

    return static_cast<int16_t>(
        std::lround(
            std::clamp(
                value,
                -1.0f,
                1.0f
            ) * 32767.0f
        )
    );
}

std::string readDisplayText(
    const emu88Lib::Sc88& board
)
{
    const auto& lcd = board.lcd();

    std::string text;

    for (
        uint32_t line = 0;
        line < lcd.getVisibleLines();
        ++line
    )
    {
        if (line != 0)
            text += '\n';

        for (
            uint32_t column = 0;
            column < lcd.getVisibleColumns();
            ++column
        )
        {
            const auto character =
                lcd.getVisibleCharacter(
                    line,
                    column
                );

            text += character != 0
                ? static_cast<char>(character)
                : ' ';
        }
    }

    return text;
}
}

extern "C"
{
sc88_headless_context* sc88_headless_create(
    const sc88_headless_rom_set* roms
)
{
    if (!validateRomSet(roms))
        return nullptr;

    try
    {
        std::vector<uint8_t> control(
            roms->control,
            roms->control +
                roms->control_size
        );

        std::array<
            std::vector<uint8_t>,
            emu88Lib::WaveRom::ChipCount
        > waveChips;

        for (
            size_t index = 0;
            index < waveChips.size();
            ++index
        )
        {
            waveChips[index].assign(
                roms->wave[index],
                roms->wave[index] +
                    roms->wave_size[index]
            );
        }

        emu88Lib::WaveRom waveRom(
            waveChips
        );

        if (!waveRom.isValid())
            return nullptr;

        auto context =
            std::unique_ptr<sc88_headless_context>(
                new (std::nothrow)
                    sc88_headless_context
            );

        if (!context)
            return nullptr;

        context->board =
            std::make_unique<emu88Lib::Sc88>(
                std::move(control),
                waveRom.takeData(),
                emu88Lib::Model::Sc88,
                true
            );

        if (
            !context->board ||
            !context->board->isValid()
        )
        {
            return nullptr;
        }

        return context.release();
    }
    catch (...)
    {
        return nullptr;
    }
}

void sc88_headless_destroy(
    sc88_headless_context* context
)
{
    delete context;
}

int sc88_headless_is_valid(
    const sc88_headless_context* context
)
{
    return (
        context &&
        context->board &&
        context->board->isValid()
    ) ? 1 : 0;
}

int sc88_headless_boot(
    sc88_headless_context* context
)
{
    if (!sc88_headless_is_valid(context))
        return 0;

    constexpr uint32_t bootFrames =
        SC88_HEADLESS_SAMPLE_RATE * 10U;

    for (
        uint32_t frame = 0;
        frame < bootFrames;
        ++frame
    )
    {
        context->board->renderSample();
    }

    return 1;
}

int sc88_headless_play_short_message(
    sc88_headless_context* context,
    const uint8_t port,
    const uint8_t status,
    const uint8_t data1,
    const uint8_t data2
)
{
    if (!sc88_headless_is_valid(context))
        return 0;

    if (
        port >= 2 ||
        status < 0x80 ||
        status == 0xF0 ||
        status == 0xF7
    )
    {
        return 0;
    }

    const synthLib::SMidiEvent event(
        synthLib::MidiEventSource::Host,
        status,
        data1,
        data2,
        0
    );

    context->board->addMidiEvent(
        event,
        port
    );

    return 1;
}

void sc88_headless_render_int16(
    sc88_headless_context* context,
    int16_t* stereo,
    const uint32_t frames
)
{
    if (!stereo || frames == 0)
        return;

    if (!sc88_headless_is_valid(context))
    {
        std::fill_n(
            stereo,
            static_cast<size_t>(frames) * 2U,
            static_cast<int16_t>(0)
        );

        return;
    }

    for (
        uint32_t frame = 0;
        frame < frames;
        ++frame
    )
    {
        const auto sample =
            context->board->renderSample();

        stereo[
            static_cast<size_t>(frame) * 2U
        ] = convertDacSample(
            sample.first
        );

        stereo[
            static_cast<size_t>(frame) * 2U + 1U
        ] = convertDacSample(
            sample.second
        );
    }
}

uint32_t sc88_headless_sample_rate(void)
{
    return SC88_HEADLESS_SAMPLE_RATE;
}

size_t sc88_headless_get_display_text(
    const sc88_headless_context* context,
    char* output,
    const size_t capacity
)
{
    if (!sc88_headless_is_valid(context))
    {
        if (output && capacity)
            output[0] = '\0';

        return 0;
    }

    const std::string text =
        readDisplayText(
            *context->board
        );

    if (output && capacity)
    {
        const size_t count =
            std::min(
                text.size(),
                capacity - 1U
            );

        std::memcpy(
            output,
            text.data(),
            count
        );

        output[count] = '\0';
    }

    return text.size();
}

size_t sc88_headless_midi_backlog(
    const sc88_headless_context* context
)
{
    if (!sc88_headless_is_valid(context))
        return 0;

    return context->board->midiInBacklog();
}
}
