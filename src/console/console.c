/*
 * console.c
 *
 *  Created on: Nov 15, 2025
 *      Author: MrTransistor
 */

#include "console.h"

static ConsoleConfig_t console_config;

// Safe function for copying strings
static inline char* strcopy(char *dst, const char *src, size_t n) {
    if (!n)
        return dst;
    char *d = dst;
    while (--n)
        if ((*d++ = *src++) == 0)
            break;
    if (!n)
        *d = 0;
    return dst;
}

void console_init(const ConsoleConfig_t *config) {
    console_config = *config;
    console_config.write("> ", 2);
}

static const Parameter_t* find_parameter(const char *name) {
    for (uint32_t i = 0; i < console_config.param_count; i++) {
        if (strcmp(console_config.parameters[i].name, name) == 0) {
            return &console_config.parameters[i];
        }
    }
    return NULL;
}

static void print_parameter_value(const Parameter_t *param) {
    switch (param->type) {
        case PARAM_TYPE_UINT32: {
            console_config.write((char*) param->name, strlen(param->name));
            console_config.write(": ", 2);
            // The string for uint32_t is pretty short so can allocate a static amount of bytes
            char str[11];   // Maximum is "4294967296\0" which is 11 bytes
            snprintf(str, sizeof(str), "%ld", *(uint32_t*) param->data_ptr);
            console_config.write(str, strlen(str));
            console_config.write("\r\n", 2);
            break;
        }
        case PARAM_TYPE_INT32: {
            console_config.write((char*) param->name, strlen(param->name));
            console_config.write(": ", 2);
            // The string for int32_t is pretty short so can allocate a static amount of bytes
            char str[12];   // Maximum is "-2147483648\0" which is 12 bytes
            snprintf(str, sizeof(str), "%ld", *(int32_t*) param->data_ptr);
            console_config.write(str, strlen(str));
            console_config.write("\r\n", 2);
            break;
        }
        case PARAM_TYPE_FLOAT: {
            console_config.write((char*) param->name, strlen(param->name));
            console_config.write(": ", 2);
            // The string can be really short or really long so dynamic allocation is better here
            size_t str_len = snprintf(NULL, 0, "%f", *(float*) param->data_ptr);
            char str[str_len];
            snprintf(str, sizeof(str), "%f", *(float*) param->data_ptr);
            console_config.write(str, strlen(str));
            console_config.write("\r\n", 2);
            break;
        }
        case PARAM_TYPE_STRING: {
            console_config.write((char*) param->name, strlen(param->name));
            console_config.write(": ", 2);
            console_config.write((char*) param->data_ptr, strlen((char*) param->data_ptr));
            console_config.write("\r\n", 2);
            break;
        }
    }
}

static void print_parameter_range(const Parameter_t *param, char *str_buf, size_t max_len) {
    // Check if min_val equals to max_val in binary representation
    if (param->min_val.u32 == param->max_val.u32)
        return;

    switch (param->type) {
        case PARAM_TYPE_UINT32:
            snprintf(str_buf, max_len, ", [%lu, %lu]", param->min_val.u32, param->max_val.u32);
            console_config.write(str_buf, strlen(str_buf));
            break;
        case PARAM_TYPE_INT32:
            snprintf(str_buf, max_len, ", [%ld, %ld]", param->min_val.i32, param->max_val.i32);
            console_config.write(str_buf, strlen(str_buf));
            break;
        case PARAM_TYPE_FLOAT:
            snprintf(str_buf, max_len, ", [%f, %f]", param->min_val.f32, param->max_val.f32);
            console_config.write(str_buf, strlen(str_buf));
            break;
        case PARAM_TYPE_STRING:
            snprintf(str_buf, max_len, ", maximum length: %lu", param->max_val.u32);
            console_config.write(str_buf, strlen(str_buf));
            break;
    }
}

static uint8_t set_parameter_value(const Parameter_t *param, const char *value_str) {
    char *endptr;

    switch (param->type) {
        case PARAM_TYPE_UINT32: {
            uint32_t val = strtoul(value_str, &endptr, 10);
            if (endptr == value_str || *endptr != '\0') {
                console_config.write("Error: Invalid value\r\n", 22);
                return 0;
            }
            if (param->min_val.u32 != param->max_val.u32 &&
                    (val < param->min_val.u32 || val > param->max_val.u32)) {
                console_config.write("Error: Value out of range\r\n", 27);
                return 0;
            }
            *(uint32_t*) param->data_ptr = (uint32_t) val;
            break;
        }
        case PARAM_TYPE_INT32: {
            int32_t val = strtol(value_str, &endptr, 10);
            if (endptr == value_str || *endptr != '\0') {
                console_config.write("Error: Invalid value\r\n", 22);
                return 0;
            }
            if (param->min_val.i32 != param->max_val.i32 &&
                    (val < param->min_val.i32 || val > param->max_val.i32)) {
                console_config.write("Error: Value out of range\r\n", 27);
                return 0;
            }
            *(int32_t*) param->data_ptr = (int32_t) val;
            break;
        }
        case PARAM_TYPE_FLOAT: {
            float val = strtof(value_str, &endptr);
            if (endptr == value_str || *endptr != '\0') {
                console_config.write("Error: Invalid value\r\n", 22);
                return 0;
            }
            if (param->min_val.f32 != param->max_val.f32 &&
                    (val < param->min_val.f32 || val > param->max_val.f32)) {
                console_config.write("Error: Value out of range\r\n", 27);
                return 0;
            }
            *(float*) param->data_ptr = (float) val;
            break;
        }
        case PARAM_TYPE_STRING: {
            // Copy the string up to the max_val characters. Errors out if the max_len is set to 0
            if (param->max_val.u32 == 0) {
                console_config.write("Error: String maximum length is zero\r\n", 38);
                return 0;
            }
            strcopy((char*) param->data_ptr, value_str, param->max_val.u32);
            break;
        }
    }

    return 1;
}

uint8_t console_execute_command(const char *command) {
    char cmd[MAX_CMD_LEN];
    char *tokens[MAX_CMD_TOKENS];
    size_t token_count = 0;

    // Make a copy
    strcopy(cmd, command, MAX_CMD_LEN);
    // Tokenize the command
    char *token = strtok(cmd, " \t");
    while (token != NULL && token_count < MAX_CMD_TOKENS) {
        tokens[token_count++] = token;
        token = strtok(NULL, " \t");
    }
    if (token_count == 0) {
        return 0;
    }

    // Handle commands
    if (strcmp(tokens[0], "get") == 0) {
        if (token_count != 2) {
            console_config.write("Usage: get <parameter_name>\r\n", 29);
            return 0;
        }
        const Parameter_t *param = find_parameter(tokens[1]);
        if (param == NULL) {
            console_config.write("Error: Unknown parameter\r\n", 26);
            return 0;
        }
        print_parameter_value(param);
        return 1;
    }
    else if (strcmp(tokens[0], "set") == 0) {
        if (token_count != 3) {
            console_config.write("Usage: set <parameter_name> <value>\r\n", 37);
            return 0;
        }
        const Parameter_t *param = find_parameter(tokens[1]);
        if (param == NULL) {
            console_config.write("Error: Unknown parameter\r\n", 26);
            return 0;
        }
        if (set_parameter_value(param, tokens[2])) {
            print_parameter_value(param);
            return 1;
        }
        return 0;
    }
    else if (strcmp(tokens[0], "help") == 0) {
        console_print_help();
        return 1;
    }
    else if (strcmp(tokens[0], "list") == 0) {
        console_list_parameters();
        return 1;
    }
    else {
        // Search for a custom command with specified name
        for (size_t i = 0; i < console_config.command_count; i++) {
            if (strcmp(tokens[0], console_config.commands[i].name) == 0) {
                if (console_config.commands[i].func(&console_config, tokens, token_count))
                    return 1;
                else
                    return 0;
            }
        }
        console_config.write("Error: Unknown command. Type 'help' for available commands\r\n", 60);
        return 0;
    }
}

void console_print_help(void) {
    const char *help_text = "Available commands:\r\n"
            "  get <param>     - Get parameter value\r\n"
            "  set <param> <value> - Set parameter value\r\n"
            "  list            - List all parameters\r\n"
            "  help            - Show this help\r\n";
    console_config.write(help_text, strlen(help_text));
    for (size_t i = 0; i < console_config.command_count; i++)
        console_config.write(console_config.commands[i].description, strlen(console_config.commands[i].description));
}

void console_list_parameters(void) {
    char str[128];  // TODO: maybe somehow make this dynamic

    console_config.write("Available parameters:\r\n", 23);

    for (uint32_t i = 0; i < console_config.param_count; i++) {
        const Parameter_t *param = &console_config.parameters[i];
        // Print parameter name and type
        const char *type_str = "unknown";
        switch (param->type) {
            case PARAM_TYPE_UINT32:
                type_str = "uint32";
                break;
            case PARAM_TYPE_INT32:
                type_str = "int32";
                break;
            case PARAM_TYPE_FLOAT:
                type_str = "float";
                break;
            case PARAM_TYPE_STRING:
                type_str = "string";
                break;
        }
        snprintf(str, sizeof(str), "  %-16s [%s]", param->name, type_str);
        console_config.write(str, strlen(str));

        // Print description if available
        if (param->description != NULL) {
            console_config.write(" - ", 3);
            console_config.write((char*) param->description, strlen(param->description));
        }

        // Print range if specified
        print_parameter_range(param, str, sizeof(str));

        console_config.write("\r\n", 2);
    }
}

void console_process(void) {
    static char input_buffer[256];
    static uint32_t input_index = 0;

    if (console_config.read == NULL) {
        return;
    }

// Read available characters
    char ch;
    while (console_config.read(&ch, 1) > 0) {
        // Handle backspace and delete
        if (ch == '\b' || ch == 0x7F) {
            if (input_index > 0) {
                input_index--;
                console_config.write("\b \b", 3); // Visual backspace
            }
        }
        // Handle enter/return
        else if (ch == '\r' || ch == '\n') {
            if (input_index > 0) {
                input_buffer[input_index] = '\0';
                console_config.write("\r\n", 2);

                // Execute command
                console_execute_command(input_buffer);

                // Reset buffer
                input_index = 0;

                // Show prompt
                console_config.write("> ", 2);
            }
        }
        // Regular character
        else if (input_index < sizeof(input_buffer) - 1) {
            input_buffer[input_index++] = ch;
            console_config.write(&ch, 1); // Echo character
        }
    }
}
