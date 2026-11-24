#include "block-task/block-task.h"
#include "platform/platform.h"

static block_task_callback_t callback;
static uint32_t requested_count;

bool block_task_start(uint32_t count, block_task_callback_t on_block)
{
    if (callback != NULL || on_block == NULL)
    {
        return false;
    }
    if (!platform_block_start(count))
    {
        return false;
    }
    requested_count = count;
    callback = on_block;
    return true;
}

void block_task_handle(void)
{
    if (callback == NULL || !platform_block_done())
    {
        return;
    }
    block_task_callback_t done = callback;
    callback = NULL;
    done(platform_block_samples(), requested_count, platform_block_time_us());
}
