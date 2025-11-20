#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/elf.h>
#include <proc.h>

bool elf_validate(const void *elf_data, size_t size);
bool elf_load(process_t *proc, const void *elf_data, size_t size, 
              uint64_t *entry_point);

const Elf64_Ehdr *elf_get_header(const void *elf_data);
const Elf64_Phdr *elf_get_program_header(const void *elf_data, int index);
bool elf_check_magic(const Elf64_Ehdr *ehdr);
bool elf_check_supported(const Elf64_Ehdr *ehdr);

#endif