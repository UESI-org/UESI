#ifndef BOOT_H
#define BOOT_H

#include <stdbool.h>
#include "limine.h"

void boot_init(void);

bool boot_verify_protocol(void);

struct limine_framebuffer *boot_get_framebuffer(void);

struct limine_memmap_response *boot_get_memmap(void);

struct limine_hhdm_response *boot_get_hhdm(void);

#endif