#include "system_info.h"
#include <stdbool.h>
#include <stdint.h>
#include "serial.h"
#include "serial_debug.h"
#include "tty.h"
#include "printf.h"
#include "pmm.h"

static void print_cpu_basic_info(const cpu_info_t *info) {
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

static void print_cpu_features(const cpu_info_t *info) {
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

void system_print_cpu_info(const cpu_info_t *cpu) {
    serial_printf(DEBUG_PORT, "\n=== CPU Information ===\n");
    print_cpu_basic_info(cpu);
    print_cpu_features(cpu);
}

void system_print_banner(const cpu_info_t *cpu) {
    tty_set_color(TTY_COLOR_CYAN, TTY_COLOR_BLACK);
    printf("\n=== UESI Kernel ===\n");
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    
    printf("CPU: %s", cpu->vendor);
    if (cpu->brand[0] != '\0') {
        printf(" (%s)", cpu->brand);
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
}