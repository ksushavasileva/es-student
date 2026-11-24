
#include "stdio-text-protocol.h"

#include <stddef.h>
#include <stdio.h>

#include "pico/stdlib.h"

static char line[STDIO_TEXT_PROTOCOL_LINE_SIZE];
static uint32_t line_length = 0;
static bool line_truncated = false;
static command_t current_command;

void stdio_text_protocol_init(void)
{
    line_length = 0;
    line_truncated = false;
    current_command.name = NULL;
    current_command.argc = 0;
    current_command.truncated = false;
}

static const command_t *parse_line(void)
{
    line[line_length] = '\0';

    current_command.name = NULL;
    current_command.argc = 0;
    current_command.truncated = line_truncated;

    for (uint32_t i = 0; i < STDIO_TEXT_PROTOCOL_MAX_ARGS; i++)
    {
        current_command.argv[i] = NULL;
    }

    char *cursor = line;

    while (*cursor != '\0')
    {
        while (*cursor == ' ' || *cursor == '\t')
        {
            *cursor = '\0';
            cursor++;
        }

        if (*cursor == '\0')
        {
            break;
        }

        char *word = cursor;

        while (*cursor != '\0' &&
               *cursor != ' ' &&
               *cursor != '\t')
        {
            cursor++;
        }

        if (*cursor != '\0')
        {
            *cursor = '\0';
            cursor++;
        }

        if (current_command.name == NULL)
        {
            current_command.name = word;
        }
        else if (current_command.argc < STDIO_TEXT_PROTOCOL_MAX_ARGS)
        {
            current_command.argv[current_command.argc] = word;
            current_command.argc++;
        }
        else
        {
            current_command.truncated = true;
        }
    }

    return &current_command;
}

const command_t *stdio_text_protocol_handle(void)
{
    int symbol = getchar_timeout_us(0);

    if (symbol == PICO_ERROR_TIMEOUT)
    {
        return NULL;
    }

    if (symbol == '\r' || symbol == '\n')
    {
        if (line_length == 0 && !line_truncated)
        {
            return NULL;
        }

        putchar('\n');

        const command_t *result = parse_line();

        line_length = 0;
        line_truncated = false;

        return result;
    }

    if (symbol == 0x08 || symbol == 0x7f)
    {
        if (line_length > 0)
        {
            line_length--;
            line[line_length] = '\0';
            printf("\b \b");
        }

        return NULL;
    }

    if (symbol >= 32 && symbol <= 126)
    {
        putchar(symbol);

        if (line_length + 1 < STDIO_TEXT_PROTOCOL_LINE_SIZE)
        {
            line[line_length] = (char)symbol;
            line_length++;
        }
        else
        {
            line_truncated = true;
        }
    }

    return NULL;
}
