#ifndef SERIAL_DEBUG_H
#define SERIAL_DEBUG_H

#include <stdint.h>
#include <stdbool.h>
#include "serial.h"

#define DEBUG_PORT SERIAL_COM1

bool debug_init(void);

bool debug_is_enabled(void);

void debug_print(const char *str);
void debug_banner(const char *title);
void debug_section(const char *section);
void debug_error(const char *error);
void debug_success(const char *message);

void debug_framebuffer_info(uint32_t width, uint32_t height, 
                           uint32_t pitch, uint32_t bpp);

#endif