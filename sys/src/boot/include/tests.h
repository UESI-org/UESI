#ifndef TESTS_H
#define TESTS_H

#include <stddef.h>
#include <stdbool.h>

void test_kmalloc(void);
void test_ahci(void);
void test_rtc(void);
bool userland_load_and_run(const void *elf_data,
                           size_t elf_size,
                           const char *name);

#endif