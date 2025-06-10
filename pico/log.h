#pragma once
#include <stdio.h>
#include <stdarg.h>

bool log_enabled = false;
int line_id = 0;

void log(const char *format, ...) {
    if (!log_enabled) return;

    // convert line_id to string and print it, padding with spaces to 4 characters
    // printf("%d. ", line_id++);
    printf("log: ");
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}