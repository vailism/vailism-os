#ifndef SHELL_H
#define SHELL_H

#include "types.h"

/**
 * Initialize interactive shell and display prompt.
 */
void shell_init(void);

/**
 * Process a single character entered from keyboard or serial.
 */
void shell_handle_char(char c);

/**
 * Execute a raw command line string.
 */
void shell_execute_command(const char *cmdline);

#endif // SHELL_H
