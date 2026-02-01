/*
 * console.h
 *
 *  Created on: Nov 15, 2025
 *      Author: MrTransistor
 */

#ifndef CONSOLE_H_
#define CONSOLE_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_STRING_PARAM_LEN    64
#define MAX_CMD_LEN             256
#define MAX_CMD_TOKENS          16

// Parameter types
typedef enum {
    PARAM_TYPE_UINT32,
    PARAM_TYPE_INT32,
    PARAM_TYPE_FLOAT,
    PARAM_TYPE_STRING
} ParameterType_t;

// Parameter descriptor structure
typedef struct {
    const char *name;
    void *data_ptr;
    ParameterType_t type;
    union {
        uint32_t u32;
        int32_t i32;
        float f32;
    } min_val;
    union {
        uint32_t u32;
        int32_t i32;
        float f32;
    } max_val;
    const char *description;
} Parameter_t;

typedef struct ConsoleConfig_t ConsoleConfig_t;

// Custom command descriptor structure
typedef struct {
    const char *name;
    const uint8_t (*func)(ConsoleConfig_t *console, char *tokens[], size_t token_count);
    const char *description;
} Command_t;

// Function pointer types for stream operations
typedef size_t (*stream_read_func)(void *buf, size_t max_len);
typedef size_t (*stream_write_func)(const void *buf, size_t len);

// Parser configuration
typedef struct ConsoleConfig_t {
    stream_read_func read;
    stream_write_func write;
    const Parameter_t *parameters;
    size_t param_count;
    const Command_t *commands;
    size_t command_count;
} ConsoleConfig_t;

// Parser functions
void console_init(const ConsoleConfig_t *config);
void console_process(void);
uint8_t console_execute_command(const char *command);

// Utility functions
void console_print_help(void);
void console_list_parameters(void);

#endif /* CONSOLE_H_ */
