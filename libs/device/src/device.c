#include "device/device.h"

#include <stddef.h>

#include "pico.h"
#include "pico/unique_id.h"
#include "pico/version.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sysinfo.h"

#define DEVICE_STRINGIFY_IMPL(value) #value
#define DEVICE_STRINGIFY(value) DEVICE_STRINGIFY_IMPL(value)

void device_get_info(device_info_t *info)
{
    if (info == NULL)
    {
        return;
    }

#ifdef PICO_BOARD
    info->board = DEVICE_STRINGIFY(PICO_BOARD);
#else
    info->board = "pico";
#endif

    pico_get_unique_board_id_string(
        info->serial,
        sizeof(info->serial)
    );

    const volatile uint32_t *chip_id_reg =
        (const volatile uint32_t *)SYSINFO_BASE;

    uint32_t chip_id = *chip_id_reg;

    info->manufacturer =
        (chip_id & SYSINFO_CHIP_ID_MANUFACTURER_BITS)
        >> SYSINFO_CHIP_ID_MANUFACTURER_LSB;

    info->part =
        (chip_id & SYSINFO_CHIP_ID_PART_BITS)
        >> SYSINFO_CHIP_ID_PART_LSB;

    info->revision =
        (chip_id & SYSINFO_CHIP_ID_REVISION_BITS)
        >> SYSINFO_CHIP_ID_REVISION_LSB;

    info->sdk_version = PICO_SDK_VERSION_STRING;
}

