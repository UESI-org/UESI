#ifndef SYSTEM_INFO_H
#define SYSTEM_INFO_H

#include <cpuid.h>

void system_print_cpu_info(const cpu_info_t *cpu);

void system_print_banner(const cpu_info_t *cpu);

#endif