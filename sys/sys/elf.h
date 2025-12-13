#ifndef ELF_H
#define ELF_H

#include <stdint.h>

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

#define EI_NIDENT 16

typedef struct {
	unsigned char e_ident[EI_NIDENT]; /* ELF identification */
	Elf64_Half e_type;                /* Object file type */
	Elf64_Half e_machine;             /* Machine type */
	Elf64_Word e_version;             /* Object file version */
	Elf64_Addr e_entry;               /* Entry point address */
	Elf64_Off e_phoff;                /* Program header offset */
	Elf64_Off e_shoff;                /* Section header offset */
	Elf64_Word e_flags;               /* Processor-specific flags */
	Elf64_Half e_ehsize;              /* ELF header size */
	Elf64_Half e_phentsize;           /* Size of program header entry */
	Elf64_Half e_phnum;               /* Number of program header entries */
	Elf64_Half e_shentsize;           /* Size of section header entry */
	Elf64_Half e_shnum;               /* Number of section header entries */
	Elf64_Half e_shstrndx;            /* Section name string table index */
} Elf64_Ehdr;

#define EI_MAG0 0 /* File identification */
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4      /* File class */
#define EI_DATA 5       /* Data encoding */
#define EI_VERSION 6    /* File version */
#define EI_OSABI 7      /* OS/ABI identification */
#define EI_ABIVERSION 8 /* ABI version */
#define EI_PAD 9        /* Start of padding bytes */

#define ELFMAG0 0x7f
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASSNONE 0 /* Invalid class */
#define ELFCLASS32 1   /* 32-bit objects */
#define ELFCLASS64 2   /* 64-bit objects */

#define ELFDATANONE 0 /* Invalid data encoding */
#define ELFDATA2LSB 1 /* Little-endian */
#define ELFDATA2MSB 2 /* Big-endian */

#define ET_NONE 0 /* No file type */
#define ET_REL 1  /* Relocatable object file */
#define ET_EXEC 2 /* Executable file */
#define ET_DYN 3  /* Shared object file */
#define ET_CORE 4 /* Core file */

#define EM_NONE 0    /* No machine */
#define EM_X86_64 62 /* AMD x86-64 */

#define EV_NONE 0    /* Invalid version */
#define EV_CURRENT 1 /* Current version */

typedef struct {
	Elf64_Word p_type;    /* Type of segment */
	Elf64_Word p_flags;   /* Segment attributes */
	Elf64_Off p_offset;   /* Offset in file */
	Elf64_Addr p_vaddr;   /* Virtual address in memory */
	Elf64_Addr p_paddr;   /* Physical address (not used) */
	Elf64_Xword p_filesz; /* Size of segment in file */
	Elf64_Xword p_memsz;  /* Size of segment in memory */
	Elf64_Xword p_align;  /* Alignment of segment */
} Elf64_Phdr;

#define PT_NULL 0    /* Unused entry */
#define PT_LOAD 1    /* Loadable segment */
#define PT_DYNAMIC 2 /* Dynamic linking information */
#define PT_INTERP 3  /* Interpreter pathname */
#define PT_NOTE 4    /* Note section */
#define PT_SHLIB 5   /* Reserved */
#define PT_PHDR 6    /* Program header table */
#define PT_TLS 7     /* Thread-Local Storage */

#define PF_X 0x1 /* Execute permission */
#define PF_W 0x2 /* Write permission */
#define PF_R 0x4 /* Read permission */

typedef struct {
	Elf64_Word sh_name;       /* Section name */
	Elf64_Word sh_type;       /* Section type */
	Elf64_Xword sh_flags;     /* Section attributes */
	Elf64_Addr sh_addr;       /* Virtual address in memory */
	Elf64_Off sh_offset;      /* Offset in file */
	Elf64_Xword sh_size;      /* Size of section */
	Elf64_Word sh_link;       /* Link to other section */
	Elf64_Word sh_info;       /* Miscellaneous information */
	Elf64_Xword sh_addralign; /* Address alignment boundary */
	Elf64_Xword sh_entsize;   /* Size of entries, if section has table */
} Elf64_Shdr;

#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 0xf)
#define ELF64_ST_INFO(b, t) (((b) << 4) + ((t) & 0xf))

#endif