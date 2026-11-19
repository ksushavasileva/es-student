
#include <stdlib.h>
#include "command.h"
#include "device.h"

#include "memory.h"

#include <stdio.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/regs/addressmap.h"


int main(void);

uint32_t data_variable = 100;
uint32_t bss_variable;

extern char __flash_binary_start;
extern char __flash_binary_end;
extern char __boot2_start__;
extern char __boot2_end__;
extern char __etext;
extern char __data_start__;
extern char __data_end__;
extern char __bss_start__;
extern char __bss_end__;
extern char __HeapLimit;
extern char __StackBottom;
extern char __StackTop;

static void row(const char *name, uintptr_t start, uintptr_t end)
{
    printf("%-10s 0x%08x 0x%08x %8u\n",
           name,
           (unsigned)start,
           (unsigned)end,
           (unsigned)(end - start));
}

void mem_info(void)
{
    uintptr_t flash_start = XIP_BASE;
    uintptr_t flash_end = flash_start + PICO_FLASH_SIZE_BYTES;

    uintptr_t sram_start = SRAM_BASE;
    uintptr_t sram_end = sram_start + 264u * 1024u;

    uintptr_t rom_start = ROM_BASE;
    uintptr_t rom_end = rom_start + 16u * 1024u;

    uintptr_t image_start = (uintptr_t)&__flash_binary_start;
    uintptr_t image_end = (uintptr_t)&__flash_binary_end;

    uintptr_t boot2_start = (uintptr_t)&__boot2_start__;
    uintptr_t boot2_end = (uintptr_t)&__boot2_end__;

    uintptr_t text_end = (uintptr_t)&__etext;

    uintptr_t data_start = (uintptr_t)&__data_start__;
    uintptr_t data_end = (uintptr_t)&__data_end__;

    uintptr_t bss_start = (uintptr_t)&__bss_start__;
    uintptr_t bss_end = (uintptr_t)&__bss_end__;

    uintptr_t heap_end = (uintptr_t)&__HeapLimit;

    uintptr_t stack_bottom = (uintptr_t)&__StackBottom;
    uintptr_t stack_top = (uintptr_t)&__StackTop;

    unsigned boot2_size = (unsigned)(boot2_end - boot2_start);
    unsigned text_size = (unsigned)(text_end - boot2_end);
    unsigned data_size = (unsigned)(data_end - data_start);
    unsigned bss_size = (unsigned)(bss_end - bss_start);

    unsigned image_size = boot2_size + text_size + data_size;
    unsigned flash_free = (unsigned)(flash_end - image_end);
    unsigned ram_used = data_size + bss_size;
    unsigned heap_size = (unsigned)(heap_end - bss_end);
    unsigned stack_size = (unsigned)(stack_top - stack_bottom);

    printf("area       start      end        size\n");

    row("flash", flash_start, flash_end);
    row("sram", sram_start, sram_end);
    row("rom", rom_start, rom_end);

    row("image", image_start, image_end);
    row("free", image_end, flash_end);

    row("boot2", boot2_start, boot2_end);
    row("text", boot2_end, text_end);

    row("data flash", text_end, text_end + data_size);
    row("data ram", data_start, data_end);

    row("bss", bss_start, bss_end);
    row("heap", bss_end, heap_end);
    row("stack", stack_bottom, stack_top);

    printf("\ntotal\n");

    printf("  flash image %8u = boot2 %u + text %u + data %u\n",
           image_size, boot2_size, text_size, data_size);

    printf("  flash free  %8u of %u\n",
           flash_free, (unsigned)PICO_FLASH_SIZE_BYTES);

    printf("  ram used    %8u = data %u + bss %u\n",
           ram_used, data_size, bss_size);

    printf("  ram free    %8u for heap and %u for stack\n",
           heap_size, stack_size);
}

void fw_info(void)
{
    data_variable++;
    bss_variable++;

    uintptr_t main_address = (uintptr_t)main;
    uintptr_t fw_address = (uintptr_t)fw_info;

    const uint16_t *main_code =
        (const uint16_t *)(main_address & ~1u);

    const uint16_t *fw_code =
        (const uint16_t *)(fw_address & ~1u);

    uint32_t stack_variable = 1946;
    uint32_t *heap_variable = malloc(sizeof(uint32_t));

    if (heap_variable != NULL)
    {
        *heap_variable = 1951;
    }

    printf("object          address     value\n");

    printf("main            0x%08x  0x%04x\n",
           (unsigned)main_address,
           (unsigned)*main_code);

    printf("fw_info         0x%08x  0x%04x\n",
           (unsigned)fw_address,
           (unsigned)*fw_code);

    printf("commands        0x%08x\n",
           (unsigned)(uintptr_t)commands);

    for (uint i = 0; i < command_count; i++)
    {
        printf("- %-13s 0x%08x\n",
               commands[i].name,
               (unsigned)(uintptr_t)commands[i].handler);
    }

    printf("DEVICE_PROJECT  0x%08x  %s\n",
           (unsigned)(uintptr_t)DEVICE_PROJECT,
           DEVICE_PROJECT);

    printf("DEVICE_BOARD    0x%08x  %s\n",
           (unsigned)(uintptr_t)DEVICE_BOARD,
           DEVICE_BOARD);

    printf("data_variable   0x%08x  %u\n",
           (unsigned)(uintptr_t)&data_variable,
           (unsigned)data_variable);

    printf("bss_variable    0x%08x  %u\n",
           (unsigned)(uintptr_t)&bss_variable,
           (unsigned)bss_variable);

    printf("stack_variable  0x%08x  %u\n",
           (unsigned)(uintptr_t)&stack_variable,
           (unsigned)stack_variable);

    if (heap_variable != NULL)
    {
        printf("heap_variable   0x%08x  %u\n",
               (unsigned)(uintptr_t)heap_variable,
               (unsigned)*heap_variable);

        free(heap_variable);
    }
    else
    {
        printf("heap_variable   allocation failed\n");
    }
}

