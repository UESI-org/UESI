#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "limine.h"
#include "tty.h"
#include "gdt.h"
#include "isr.h"
#include "idt.h"
#include "pic.h"
#include "serial.h"
#include "serial_debug.h"
#include "scheduler.h"
#include "keyboard.h"
#include "cpuid.h"
#include "pit.h"
#include "pmm.h"
#include "vmm.h"
#include "mmu.h"
#include "paging.h"
#include "printf.h"
#include "kmalloc.h"
#include "kfree.h"

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void keyboard_handler(char c) {
    tty_putchar(c);
}

static void cpuid_serial_print_info(const cpu_info_t *info) {
    if (!info || !info->cpuid_supported) {
        serial_printf(DEBUG_PORT, "CPUID not supported\n");
        return;
    }
    
    serial_printf(DEBUG_PORT, "CPU Information:\n");
    serial_printf(DEBUG_PORT, "  Vendor: %s\n", info->vendor);
    if (info->brand[0]) {
        serial_printf(DEBUG_PORT, "  Brand: %s\n", info->brand);
    }
    serial_printf(DEBUG_PORT, "  Family: %u, Model: %u, Stepping: %u\n",
                  info->display_family, info->display_model, info->stepping);
    serial_printf(DEBUG_PORT, "  Type: %u\n", info->type);
    serial_printf(DEBUG_PORT, "  Cache Line: %u bytes\n", info->cache_line_size);
    serial_printf(DEBUG_PORT, "  Logical CPUs: %u\n", info->logical_processors);
    serial_printf(DEBUG_PORT, "  Initial APIC ID: %u\n", info->initial_apic_id);
    serial_printf(DEBUG_PORT, "  Max Basic Leaf: 0x%08X\n", info->max_basic_leaf);
    serial_printf(DEBUG_PORT, "  Max Ext Leaf: 0x%08X\n", info->max_ext_leaf);
    
    if (info->is_hypervisor) {
        serial_printf(DEBUG_PORT, "  Running under hypervisor\n");
    }
    serial_printf(DEBUG_PORT, "\n");
}

static void cpuid_serial_print_features(const cpu_info_t *info) {
    if (!info || !info->cpuid_supported) {
        return;
    }
    
    serial_printf(DEBUG_PORT, "CPU Features:\n");
    
    serial_printf(DEBUG_PORT, "  Basic: ");
    if (cpuid_has_fpu()) serial_printf(DEBUG_PORT, "FPU ");
    if (cpuid_has_vme()) serial_printf(DEBUG_PORT, "VME ");
    if (cpuid_has_pse()) serial_printf(DEBUG_PORT, "PSE ");
    if (cpuid_has_tsc()) serial_printf(DEBUG_PORT, "TSC ");
    if (cpuid_has_msr()) serial_printf(DEBUG_PORT, "MSR ");
    if (cpuid_has_pae()) serial_printf(DEBUG_PORT, "PAE ");
    if (cpuid_has_mce()) serial_printf(DEBUG_PORT, "MCE ");
    if (cpuid_has_cx8()) serial_printf(DEBUG_PORT, "CX8 ");
    if (cpuid_has_apic()) serial_printf(DEBUG_PORT, "APIC ");
    if (cpuid_has_sep()) serial_printf(DEBUG_PORT, "SEP ");
    if (cpuid_has_mtrr()) serial_printf(DEBUG_PORT, "MTRR ");
    if (cpuid_has_pge()) serial_printf(DEBUG_PORT, "PGE ");
    if (cpuid_has_cmov()) serial_printf(DEBUG_PORT, "CMOV ");
    if (cpuid_has_pat()) serial_printf(DEBUG_PORT, "PAT ");
    if (cpuid_has_clflush()) serial_printf(DEBUG_PORT, "CFLUSH ");
    if (cpuid_has_mmx()) serial_printf(DEBUG_PORT, "MMX ");
    if (cpuid_has_fxsr()) serial_printf(DEBUG_PORT, "FXSR ");
    if (cpuid_has_sse()) serial_printf(DEBUG_PORT, "SSE ");
    if (cpuid_has_sse2()) serial_printf(DEBUG_PORT, "SSE2 ");
    if (cpuid_has_htt()) serial_printf(DEBUG_PORT, "HTT ");
    serial_printf(DEBUG_PORT, "\n");
    
    serial_printf(DEBUG_PORT, "  Extended: ");
    if (cpuid_has_sse3()) serial_printf(DEBUG_PORT, "SSE3 ");
    if (cpuid_has_pclmulqdq()) serial_printf(DEBUG_PORT, "PCLMUL ");
    if (cpuid_has_monitor()) serial_printf(DEBUG_PORT, "MONITOR ");
    if (cpuid_has_ssse3()) serial_printf(DEBUG_PORT, "SSSE3 ");
    if (cpuid_has_fma()) serial_printf(DEBUG_PORT, "FMA ");
    if (cpuid_has_cx16()) serial_printf(DEBUG_PORT, "CX16 ");
    if (cpuid_has_sse4_1()) serial_printf(DEBUG_PORT, "SSE4.1 ");
    if (cpuid_has_sse4_2()) serial_printf(DEBUG_PORT, "SSE4.2 ");
    if (cpuid_has_movbe()) serial_printf(DEBUG_PORT, "MOVBE ");
    if (cpuid_has_popcnt()) serial_printf(DEBUG_PORT, "POPCNT ");
    if (cpuid_has_aes()) serial_printf(DEBUG_PORT, "AES ");
    if (cpuid_has_xsave()) serial_printf(DEBUG_PORT, "XSAVE ");
    if (cpuid_has_osxsave()) serial_printf(DEBUG_PORT, "OSXSAVE ");
    if (cpuid_has_avx()) serial_printf(DEBUG_PORT, "AVX ");
    if (cpuid_has_f16c()) serial_printf(DEBUG_PORT, "F16C ");
    if (cpuid_has_rdrand()) serial_printf(DEBUG_PORT, "RDRAND ");
    serial_printf(DEBUG_PORT, "\n");
    
    serial_printf(DEBUG_PORT, "  Struct Ext: ");
    if (cpuid_has_fsgsbase()) serial_printf(DEBUG_PORT, "FSGSBASE ");
    if (cpuid_has_tsc_adjust()) serial_printf(DEBUG_PORT, "TSC_ADJ ");
    if (cpuid_has_sgx()) serial_printf(DEBUG_PORT, "SGX ");
    if (cpuid_has_bmi1()) serial_printf(DEBUG_PORT, "BMI1 ");
    if (cpuid_has_hle()) serial_printf(DEBUG_PORT, "HLE ");
    if (cpuid_has_avx2()) serial_printf(DEBUG_PORT, "AVX2 ");
    if (cpuid_has_bmi2()) serial_printf(DEBUG_PORT, "BMI2 ");
    if (cpuid_has_erms()) serial_printf(DEBUG_PORT, "ERMS ");
    if (cpuid_has_invpcid()) serial_printf(DEBUG_PORT, "INVPCID ");
    if (cpuid_has_rtm()) serial_printf(DEBUG_PORT, "RTM ");
    if (cpuid_has_mpx()) serial_printf(DEBUG_PORT, "MPX ");
    if (cpuid_has_rdseed()) serial_printf(DEBUG_PORT, "RDSEED ");
    if (cpuid_has_adx()) serial_printf(DEBUG_PORT, "ADX ");
    if (cpuid_has_clflushopt()) serial_printf(DEBUG_PORT, "CLFLUSHOPT ");
    if (cpuid_has_clwb()) serial_printf(DEBUG_PORT, "CLWB ");
    if (cpuid_has_processor_trace()) serial_printf(DEBUG_PORT, "PT ");
    if (cpuid_has_sha()) serial_printf(DEBUG_PORT, "SHA ");
    serial_printf(DEBUG_PORT, "\n");
    
    bool has_avx512 = cpuid_has_avx512f();
    if (has_avx512) {
        serial_printf(DEBUG_PORT, "  AVX-512: F ");
        if (cpuid_has_avx512dq()) serial_printf(DEBUG_PORT, "DQ ");
        if (cpuid_has_avx512_ifma()) serial_printf(DEBUG_PORT, "IFMA ");
        if (cpuid_has_avx512bw()) serial_printf(DEBUG_PORT, "BW ");
        if (cpuid_has_avx512vl()) serial_printf(DEBUG_PORT, "VL ");
        serial_printf(DEBUG_PORT, "\n");
    }
    
    serial_printf(DEBUG_PORT, "  Long Mode: ");
    if (cpuid_has_long_mode()) serial_printf(DEBUG_PORT, "LM ");
    if (cpuid_has_nx()) serial_printf(DEBUG_PORT, "NX ");
    if (cpuid_has_syscall()) serial_printf(DEBUG_PORT, "SYSCALL ");
    if (cpuid_has_rdtscp()) serial_printf(DEBUG_PORT, "RDTSCP ");
    if (cpuid_has_1gb_pages()) serial_printf(DEBUG_PORT, "1GB_PAGES ");
    if (cpuid_has_lahf_lm()) serial_printf(DEBUG_PORT, "LAHF_LM ");
    if (cpuid_has_lzcnt()) serial_printf(DEBUG_PORT, "LZCNT ");
    if (cpuid_has_prefetchw()) serial_printf(DEBUG_PORT, "PREFETCHW ");
    serial_printf(DEBUG_PORT, "\n");
    
    serial_printf(DEBUG_PORT, "  Security: ");
    if (cpuid_has_smep()) serial_printf(DEBUG_PORT, "SMEP ");
    if (cpuid_has_smap()) serial_printf(DEBUG_PORT, "SMAP ");
    if (cpuid_has_umip()) serial_printf(DEBUG_PORT, "UMIP ");
    if (cpuid_has_pku()) serial_printf(DEBUG_PORT, "PKU ");
    if (cpuid_has_ospke()) serial_printf(DEBUG_PORT, "OSPKE ");
    if (cpuid_has_ibrs_ibpb()) serial_printf(DEBUG_PORT, "IBRS/IBPB ");
    if (cpuid_has_stibp()) serial_printf(DEBUG_PORT, "STIBP ");
    if (cpuid_has_l1d_flush()) serial_printf(DEBUG_PORT, "L1D_FLUSH ");
    if (cpuid_has_arch_capabilities()) serial_printf(DEBUG_PORT, "ARCH_CAP ");
    if (cpuid_has_ssbd()) serial_printf(DEBUG_PORT, "SSBD ");
    if (cpuid_has_md_clear()) serial_printf(DEBUG_PORT, "MD_CLEAR ");
    serial_printf(DEBUG_PORT, "\n");
    
    serial_printf(DEBUG_PORT, "  Virtualization: ");
    if (cpuid_has_vmx()) serial_printf(DEBUG_PORT, "VMX ");
    if (cpuid_has_smx()) serial_printf(DEBUG_PORT, "SMX ");
    if (cpuid_has_hypervisor()) serial_printf(DEBUG_PORT, "HYPERVISOR ");
    if (cpuid_has_pcid()) serial_printf(DEBUG_PORT, "PCID ");
    if (cpuid_has_x2apic()) serial_printf(DEBUG_PORT, "X2APIC ");
    serial_printf(DEBUG_PORT, "\n");
    
    serial_printf(DEBUG_PORT, "  Advanced: ");
    if (cpuid_has_waitpkg()) serial_printf(DEBUG_PORT, "WAITPKG ");
    serial_printf(DEBUG_PORT, "\n");
}

static void test_kmalloc(void) {
    debug_section("Testing kmalloc/kfree");
    
    void *ptr1 = kmalloc(128);
    if (ptr1) {
        debug_success("Allocated 128 bytes");
        strcpy((char *)ptr1, "Hello from kmalloc!");
        serial_printf(DEBUG_PORT, "String: %s\n", (char *)ptr1);
        kfree(ptr1);
        debug_success("Freed 128 bytes");
    } else {
        debug_error("Failed to allocate 128 bytes");
    }
    
    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = kmalloc(64);
        if (ptrs[i]) {
            memset(ptrs[i], i, 64);
        }
    }
    debug_success("Allocated 10x64 bytes");
    
    for (int i = 0; i < 10; i += 2) {
        kfree(ptrs[i]);
    }
    debug_success("Freed 5 allocations");
    
    for (int i = 1; i < 10; i += 2) {
        kfree(ptrs[i]);
    }
    debug_success("Freed remaining allocations");
    
    void *small = kmalloc(16);
    void *medium = kmalloc(256);
    void *large = kmalloc(4096);
    debug_success("Allocated various sizes (16, 256, 4096)");
    
    kfree(small);
    kfree(medium);
    kfree(large);
    debug_success("Freed all sizes");
    
    void *zeroed = kmalloc_flags(512, KMALLOC_ZERO);
    if (zeroed) {
        bool all_zero = true;
        for (int i = 0; i < 512; i++) {
            if (((uint8_t *)zeroed)[i] != 0) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) {
            debug_success("KMALLOC_ZERO works correctly");
        } else {
            debug_error("KMALLOC_ZERO failed to zero memory");
        }
        kfree(zeroed);
    }
    
    if (debug_is_enabled()) {
        kmalloc_stats();
    }
}

void kmain(void) {
    debug_init();

    if (limine_base_revision[2] != 0) {
        debug_error("Limine base revision mismatch");
        hcf();
    }
    debug_success("Limine protocol verified");

    debug_section("Initializing Memory Management");
    if (memmap_request.response == NULL || hhdm_request.response == NULL) {
        debug_error("Memory map or HHDM not available");
        hcf();
    }

    pmm_init(memmap_request.response, hhdm_request.response);
    debug_success("PMM initialized");

    kmalloc_init();
    debug_success("kmalloc initialized");

    if (framebuffer_request.response == NULL ||
        framebuffer_request.response->framebuffer_count < 1) {
        debug_error("No framebuffer available");
        hcf();
    }

    struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    debug_framebuffer_info(framebuffer->width, framebuffer->height,
                          framebuffer->pitch, framebuffer->bpp);

    tty_init(framebuffer);
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    debug_success("TTY initialized");

    mmu_init(hhdm_request.response);
    debug_success("MMU initialized");

    vmm_init();
    debug_success("VMM initialized");

    test_kmalloc();

    debug_section("Detecting CPU");
    cpu_info_t cpu;
    cpuid_init(&cpu);

    if (!cpu.cpuid_supported) {
        debug_error("CPUID not supported!");
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: CPUID instruction not supported\n");
        hcf();
    }

    bool all_features_present = true;
    
    if (!cpuid_has_long_mode()) {
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: 64-bit Long Mode not supported\n");
        all_features_present = false;
    }
    
    if (!cpuid_has_pae()) {
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: PAE not supported\n");
        all_features_present = false;
    }
    
    if (!cpuid_has_nx()) {
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: NX bit not supported\n");
        all_features_present = false;
    }

    if (!all_features_present) {
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        hcf();
    }

    debug_success("CPU detection complete");
    
    if (debug_is_enabled()) {
        serial_printf(DEBUG_PORT, "\n=== CPU Information ===\n");
        cpuid_serial_print_info(&cpu);
        cpuid_serial_print_features(&cpu);
    }

    debug_section("Initializing System Components");
    
    gdt_init();
    debug_success("GDT initialized");

    idt_init();
    isr_install();
    debug_success("IDT initialized");

    pic_init();
    debug_success("PIC initialized");

    pit_init(100);
    isr_register_handler(32, (isr_handler_t)pit_handler);
    debug_success("PIT initialized (100 Hz)");

    scheduler_init(100);
    debug_success("Scheduler initialized");

    keyboard_init();
    keyboard_set_callback(keyboard_handler);
    pic_clear_mask(IRQ_KEYBOARD);
    debug_success("Keyboard initialized");

    debug_banner("Kernel Initialization Complete");

    tty_set_color(TTY_COLOR_CYAN, TTY_COLOR_BLACK);
    printf("\n=== UESI Kernel ===\n");
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    
    printf("CPU: %s", cpu.vendor);
    if (cpu.brand[0] != '\0') {
        printf(" (%s)", cpu.brand);
    }
    printf("\n");

    uint64_t total_mem = pmm_get_total_memory();
    uint64_t free_mem = pmm_get_free_memory();
    printf("Memory: %llu MB total, %llu MB free\n",
           (unsigned long long)(total_mem / (1024 * 1024)),
           (unsigned long long)(free_mem / (1024 * 1024)));

    printf("Features: ");
    bool first = true;
    
    if (cpuid_has_sse2()) {
        printf("SSE2");
        first = false;
    }
    if (cpuid_has_sse3()) {
        if (!first) printf(", ");
        printf("SSE3");
        first = false;
    }
    if (cpuid_has_avx()) {
        if (!first) printf(", ");
        printf("AVX");
        first = false;
    }
    if (cpuid_has_1gb_pages()) {
        if (!first) printf(", ");
        printf("1GB Pages");
        first = false;
    }
    printf("\n\n");

    __asm__ volatile("sti");

    tty_set_color(TTY_COLOR_GREEN, TTY_COLOR_BLACK);
    printf("System ready. Type something!\n");
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    printf("> ");

    if (debug_is_enabled()) {
        serial_printf(DEBUG_PORT, "\n=== System Ready ===\n");
        serial_printf(DEBUG_PORT, "Interrupts enabled\n");
        serial_printf(DEBUG_PORT, "Awaiting input...\n");
    }

    for (;;) {
        __asm__ volatile("hlt");
    }
}