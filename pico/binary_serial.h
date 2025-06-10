#pragma once
#include <stdio.h>

/**
 * Provides convenience functions to be able to read binary data from stdin, which in Pico filters out non-printable characters.
 */

 uint8_t stdin_uint8() {
    return getchar();
 }

 uint8_t stdin_uint8_hex() {
    int c0 = getchar();
    int c1 = getchar();

    // c0 is first hex character, c1 is second hex character
    // convert to uint8_t
    if (c0 < '0' || c0 > '9') c0 = c0 - 'A' + 10; // convert A-F to 10-15
    else c0 = c0 - '0'; // convert 0-9 to 0-9
    if (c1 < '0' || c1 > '9') c1 = c1 - 'A' + 10; // convert A-F to 10-15
    else c1 = c1 - '0'; // convert 0-9 to 0-9
    
    return (c0 << 4) | c1; // combine the two nibbles into a byte
 }