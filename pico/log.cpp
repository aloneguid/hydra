#include "log.h"
#include <stdio.h>
#include "pico/stdlib.h"

bool is_enabled = false;

bool log_enabled() {
    return is_enabled;
}

void log(const char *format, ...) {
    if (!is_enabled) return;

    // convert line_id to string and print it, padding with spaces to 4 characters
    // printf("%d. ", line_id++);
    printf("log: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}
