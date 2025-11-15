/* This cpuid.c should remain heavily commented! it's a complex software!!!*/

#include <cpuid.h>
#include <printf.h>
#include <stddef.h>
#include <string.h>

/* Global CPU information - initialized once */
static cpu_info_t g_cpu_info = {0};
static bool g_cpu_info_initialized = false;

/*
 * Helper function to compare strings
 */
static inline int str_equal(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

/*
 * Check if CPUID instruction is supported
 * Test by flipping the ID bit (bit 21) in RFLAGS
 */
bool cpuid_is_supported(void) {
    uint64_t flags1, flags2;
    
    __asm__ volatile (
        "pushfq\n"                    /* Save RFLAGS */
        "pop %0\n"                    /* Pop into flags1 */
        "mov %0, %1\n"                /* Copy to flags2 */
        "xor $0x200000, %1\n"         /* Flip ID bit (bit 21) */
        "push %1\n"                   /* Push modified value */
        "popfq\n"                     /* Pop into RFLAGS */
        "pushfq\n"                    /* Push RFLAGS again */
        "pop %1\n"                    /* Pop into flags2 */
        "push %0\n"                   /* Restore original RFLAGS */
        "popfq\n"
        : "=&r" (flags1), "=&r" (flags2)
        :
        : "cc"
    );
    
    /* If ID bit changed, CPUID is supported */
    return ((flags1 ^ flags2) & 0x200000) != 0;
}

/*
 * Execute CPUID instruction with leaf and subleaf
 */
void cpuid_exec(uint32_t leaf, uint32_t subleaf, cpuid_regs_t *regs) {
    if (!regs) {
        return;
    }
    
    __asm__ volatile (
        "cpuid"
        : "=a" (regs->eax),
          "=b" (regs->ebx),
          "=c" (regs->ecx),
          "=d" (regs->edx)
        : "a" (leaf),
          "c" (subleaf)
    );
}

/*
 * Get CPU vendor string
 */
void cpuid_get_vendor(char *vendor) {
    if (!vendor) {
        return;
    }
    
    cpuid_regs_t regs;
    cpuid_exec(CPUID_VENDOR_ID, 0, &regs);
    
    *((uint32_t *)(vendor + 0)) = regs.ebx;
    *((uint32_t *)(vendor + 4)) = regs.edx;
    *((uint32_t *)(vendor + 8)) = regs.ecx;
    vendor[12] = '\0';
}

/*
 * Get CPU brand string (requires extended CPUID support)
 */
void cpuid_get_brand(char *brand) {
    if (!brand) {
        return;
    }
    
    cpuid_regs_t regs;
    uint32_t *brand_ptr = (uint32_t *)brand;
    
    /* Check if extended brand string is available */
    cpuid_exec(CPUID_EXT_FUNC_MAX, 0, &regs);
    if (regs.eax < CPUID_BRAND_STRING_3) {
        brand[0] = '\0';
        return;
    }
    
    /* Brand string is in three parts (0x80000002-0x80000004) */
    for (uint32_t i = 0; i < 3; i++) {
        cpuid_exec(CPUID_BRAND_STRING_1 + i, 0, &regs);
        brand_ptr[i * 4 + 0] = regs.eax;
        brand_ptr[i * 4 + 1] = regs.ebx;
        brand_ptr[i * 4 + 2] = regs.ecx;
        brand_ptr[i * 4 + 3] = regs.edx;
    }
    
    brand[48] = '\0';
    
    /* Trim leading spaces */
    char *src = brand;
    while (*src == ' ') {
        src++;
    }
    if (src != brand) {
        char *dst = brand;
        while ((*dst++ = *src++));
    }
}

/*
 * Check if CPU has a specific feature flag
 * Checks both EDX and ECX feature registers
 */
bool cpuid_has_feature(uint32_t feature_flag) {
    const cpu_info_t *info = &g_cpu_info;
    
    if (!g_cpu_info_initialized || !info->cpuid_supported) {
        return false;
    }
    
    /* Check both EDX and ECX registers */
    return ((info->features_edx & feature_flag) != 0) ||
           ((info->features_ecx & feature_flag) != 0);
}

/*
 * Check if CPU has a specific extended feature flag
 * Checks extended CPUID leaf 0x80000001
 */
bool cpuid_has_ext_feature(uint32_t feature_flag) {
    const cpu_info_t *info = &g_cpu_info;
    
    if (!g_cpu_info_initialized || !info->cpuid_supported) {
        return false;
    }
    
    return ((info->ext_features_edx & feature_flag) != 0) ||
           ((info->ext_features_ecx & feature_flag) != 0);
}

/*
 * Check if CPU has a specific structured extended feature (leaf 0x7)
 */
bool cpuid_has_ext7_feature(uint32_t feature_flag) {
    const cpu_info_t *info = &g_cpu_info;
    
    if (!g_cpu_info_initialized || !info->cpuid_supported) {
        return false;
    }
    
    return ((info->ext7_features_ebx & feature_flag) != 0) ||
           ((info->ext7_features_ecx & feature_flag) != 0) ||
           ((info->ext7_features_edx & feature_flag) != 0);
}

/*
 * Check if CPU is Intel
 */
bool cpuid_is_intel(void) {
    if (!g_cpu_info_initialized) {
        return false;
    }
    return g_cpu_info.is_intel;
}

/*
 * Check if CPU is AMD
 */
bool cpuid_is_amd(void) {
    if (!g_cpu_info_initialized) {
        return false;
    }
    return g_cpu_info.is_amd;
}

/*
 * Get cache line size in bytes
 */
uint32_t cpuid_get_cache_line_size(void) {
    if (!g_cpu_info_initialized) {
        return 0;
    }
    return g_cpu_info.cache_line_size;
}

/*
 * Get the global CPU info structure
 */
const cpu_info_t *cpuid_get_info(void) {
    if (!g_cpu_info_initialized) {
        return NULL;
    }
    return &g_cpu_info;
}

/*
 * Initialize CPU information structure
 * This should be called once during system initialization
 */
void cpuid_init(cpu_info_t *info) {
    cpu_info_t *target_info;
    
    if (info == NULL) {
        /* Initialize global if no structure provided */
        target_info = &g_cpu_info;
    } else {
        target_info = info;
    }
    
    /* Clear the structure */
    memset(target_info, 0, sizeof(cpu_info_t));
    
    /* Check if CPUID is supported */
    target_info->cpuid_supported = cpuid_is_supported();
    if (!target_info->cpuid_supported) {
        /* Always update global state */
        if (info != NULL) {
            g_cpu_info = *target_info;
        }
        g_cpu_info_initialized = true;
        return;
    }
    
    cpuid_regs_t regs;
    
    /*
     * Leaf 0x0: Vendor ID and Maximum Basic Leaf
     */
    cpuid_exec(CPUID_VENDOR_ID, 0, &regs);
    target_info->max_basic_leaf = regs.eax;
    *((uint32_t *)(target_info->vendor + 0)) = regs.ebx;
    *((uint32_t *)(target_info->vendor + 4)) = regs.edx;
    *((uint32_t *)(target_info->vendor + 8)) = regs.ecx;
    target_info->vendor[12] = '\0';
    
    /* Detect vendor */
    target_info->is_intel = str_equal(target_info->vendor, CPU_VENDOR_INTEL);
    target_info->is_amd = str_equal(target_info->vendor, CPU_VENDOR_AMD);
    
    /*
     * Leaf 0x1: Processor Info and Feature Bits
     */
    if (target_info->max_basic_leaf >= CPUID_FEATURES) {
        cpuid_exec(CPUID_FEATURES, 0, &regs);
        
        /* Extract Family, Model, Stepping */
        target_info->stepping = regs.eax & 0xF;
        target_info->model = (regs.eax >> 4) & 0xF;
        target_info->family = (regs.eax >> 8) & 0xF;
        target_info->type = (regs.eax >> 12) & 0x3;
        target_info->ext_model = (regs.eax >> 16) & 0xF;
        target_info->ext_family = (regs.eax >> 20) & 0xFF;
        
        /* Calculate display family and model */
        target_info->display_family = target_info->family;
        target_info->display_model = target_info->model;
        
        if (target_info->family == 0xF) {
            target_info->display_family = target_info->family + target_info->ext_family;
        }
        if (target_info->family == 0x6 || target_info->family == 0xF) {
            target_info->display_model = target_info->model + (target_info->ext_model << 4);
        }
        
        /* Feature flags */
        target_info->features_ecx = regs.ecx;
        target_info->features_edx = regs.edx;
        
        /* Extract additional info */
        target_info->cache_line_size = ((regs.ebx >> 8) & 0xFF) * 8;  /* In bytes */
        target_info->logical_processors = (regs.ebx >> 16) & 0xFF;
        target_info->initial_apic_id = (regs.ebx >> 24) & 0xFF;
        
        /* Check for hypervisor */
        target_info->is_hypervisor = (regs.ecx & CPUIDECX_HV) != 0;
    }
    
    /*
     * Leaf 0x7: Structured Extended Feature Flags
     */
    if (target_info->max_basic_leaf >= CPUID_EXTENDED_FEATURES) {
        cpuid_exec(CPUID_EXTENDED_FEATURES, 0, &regs);
        target_info->ext7_features_ebx = regs.ebx;
        target_info->ext7_features_ecx = regs.ecx;
        target_info->ext7_features_edx = regs.edx;
    }
    
    /*
     * Leaf 0xD: Extended State (XSAVE) Information
     */
    if (target_info->max_basic_leaf >= CPUID_EXTENDED_STATE) {
        /* Subleaf 0: Main XSAVE leaf */
        cpuid_exec(CPUID_EXTENDED_STATE, 0, &regs);
        target_info->xsave_features = regs.eax;
        target_info->xsave_size = regs.ecx;
        
        /* Subleaf 1: XSAVE features */
        cpuid_exec(CPUID_EXTENDED_STATE, 1, &regs);
        target_info->xsave_user_size = regs.ebx;
    }
    
    /*
     * Extended CPUID Functions (0x80000000+)
     */
    cpuid_exec(CPUID_EXT_FUNC_MAX, 0, &regs);
    target_info->max_ext_leaf = regs.eax;
    
    /*
     * Leaf 0x80000001: Extended Feature Flags
     */
    if (target_info->max_ext_leaf >= CPUID_EXT_FEATURES) {
        cpuid_exec(CPUID_EXT_FEATURES, 0, &regs);
        target_info->ext_features_ecx = regs.ecx;
        target_info->ext_features_edx = regs.edx;
    }
    
    /*
     * Leaf 0x80000002-0x80000004: Brand String
     */
    if (target_info->max_ext_leaf >= CPUID_BRAND_STRING_3) {
        uint32_t *brand_ptr = (uint32_t *)target_info->brand;
        for (uint32_t i = 0; i < 3; i++) {
            cpuid_exec(CPUID_BRAND_STRING_1 + i, 0, &regs);
            brand_ptr[i * 4 + 0] = regs.eax;
            brand_ptr[i * 4 + 1] = regs.ebx;
            brand_ptr[i * 4 + 2] = regs.ecx;
            brand_ptr[i * 4 + 3] = regs.edx;
        }
        target_info->brand[48] = '\0';
        
        /* Trim leading spaces */
        char *src = target_info->brand;
        while (*src == ' ') {
            src++;
        }
        if (src != target_info->brand) {
            char *dst = target_info->brand;
            while ((*dst++ = *src++));
        }
    } else {
        target_info->brand[0] = '\0';
    }
    
    /* Always update global CPU info */
    if (info != NULL) {
        g_cpu_info = *target_info;
    }
    g_cpu_info_initialized = true;
}

/*
 * Print CPU information (for debugging)
 * 
 * Note: You need to provide your own printf function.
 * For serial output, create cpuid_serial_print_info() in your kernel code.
 */
void cpuid_print_info(const cpu_info_t *info) {
    if (!info || !info->cpuid_supported) {
        printf("CPUID not supported\n");
        return;
    }
    
    printf("CPU Information:\n");
    printf("  Vendor: %s\n", info->vendor);
    if (info->brand[0]) {
        printf("  Brand: %s\n", info->brand);
    }
    printf("  Family: %u, Model: %u, Stepping: %u\n",
           info->display_family, info->display_model, info->stepping);
    printf("  Type: %u\n", info->type);
    printf("  Cache Line: %u bytes\n", info->cache_line_size);
    printf("  Logical CPUs: %u\n", info->logical_processors);
    printf("  Initial APIC ID: %u\n", info->initial_apic_id);
    printf("  Max Basic Leaf: 0x%08X\n", info->max_basic_leaf);
    printf("  Max Ext Leaf: 0x%08X\n", info->max_ext_leaf);
    
    if (info->is_hypervisor) {
        printf("  Running under hypervisor\n");
    }
    
    printf("\n");
}

void cpuid_print_features(const cpu_info_t *info) {
    if (!info || !info->cpuid_supported) {
        return;
    }
    
    printf("CPU Features:\n");
    
    /* Basic features */
    printf("  Basic: ");
    if (cpuid_has_fpu()) printf("FPU ");
    if (cpuid_has_vme()) printf("VME ");
    if (cpuid_has_pse()) printf("PSE ");
    if (cpuid_has_tsc()) printf("TSC ");
    if (cpuid_has_msr()) printf("MSR ");
    if (cpuid_has_pae()) printf("PAE ");
    if (cpuid_has_mce()) printf("MCE ");
    if (cpuid_has_cx8()) printf("CX8 ");
    if (cpuid_has_apic()) printf("APIC ");
    if (cpuid_has_sep()) printf("SEP ");
    if (cpuid_has_mtrr()) printf("MTRR ");
    if (cpuid_has_pge()) printf("PGE ");
    if (cpuid_has_cmov()) printf("CMOV ");
    if (cpuid_has_pat()) printf("PAT ");
    if (cpuid_has_clflush()) printf("CFLUSH ");
    if (cpuid_has_mmx()) printf("MMX ");
    if (cpuid_has_fxsr()) printf("FXSR ");
    if (cpuid_has_sse()) printf("SSE ");
    if (cpuid_has_sse2()) printf("SSE2 ");
    if (cpuid_has_htt()) printf("HTT ");
    printf("\n");
    
    /* Extended features (SSE3+) */
    printf("  Extended: ");
    if (cpuid_has_sse3()) printf("SSE3 ");
    if (cpuid_has_pclmulqdq()) printf("PCLMUL ");
    if (cpuid_has_monitor()) printf("MONITOR ");
    if (cpuid_has_ssse3()) printf("SSSE3 ");
    if (cpuid_has_fma()) printf("FMA ");
    if (cpuid_has_cx16()) printf("CX16 ");
    if (cpuid_has_sse4_1()) printf("SSE4.1 ");
    if (cpuid_has_sse4_2()) printf("SSE4.2 ");
    if (cpuid_has_movbe()) printf("MOVBE ");
    if (cpuid_has_popcnt()) printf("POPCNT ");
    if (cpuid_has_aes()) printf("AES ");
    if (cpuid_has_xsave()) printf("XSAVE ");
    if (cpuid_has_osxsave()) printf("OSXSAVE ");
    if (cpuid_has_avx()) printf("AVX ");
    if (cpuid_has_f16c()) printf("F16C ");
    if (cpuid_has_rdrand()) printf("RDRAND ");
    printf("\n");
    
    /* Structured extended features (Leaf 0x7) */
    printf("  Struct Ext: ");
    if (cpuid_has_fsgsbase()) printf("FSGSBASE ");
    if (cpuid_has_tsc_adjust()) printf("TSC_ADJ ");
    if (cpuid_has_sgx()) printf("SGX ");
    if (cpuid_has_bmi1()) printf("BMI1 ");
    if (cpuid_has_hle()) printf("HLE ");
    if (cpuid_has_avx2()) printf("AVX2 ");
    if (cpuid_has_bmi2()) printf("BMI2 ");
    if (cpuid_has_erms()) printf("ERMS ");
    if (cpuid_has_invpcid()) printf("INVPCID ");
    if (cpuid_has_rtm()) printf("RTM ");
    if (cpuid_has_mpx()) printf("MPX ");
    if (cpuid_has_rdseed()) printf("RDSEED ");
    if (cpuid_has_adx()) printf("ADX ");
    if (cpuid_has_clflushopt()) printf("CLFLUSHOPT ");
    if (cpuid_has_clwb()) printf("CLWB ");
    if (cpuid_has_processor_trace()) printf("PT ");
    if (cpuid_has_sha()) printf("SHA ");
    printf("\n");
    
    /* AVX-512 features */
    bool has_avx512 = cpuid_has_avx512f();
    if (has_avx512) {
        printf("  AVX-512: F ");
        if (cpuid_has_avx512dq()) printf("DQ ");
        if (cpuid_has_avx512_ifma()) printf("IFMA ");
        if (cpuid_has_avx512bw()) printf("BW ");
        if (cpuid_has_avx512vl()) printf("VL ");
        printf("\n");
    }
    
    /* Long mode features */
    printf("  Long Mode: ");
    if (cpuid_has_long_mode()) printf("LM ");
    if (cpuid_has_nx()) printf("NX ");
    if (cpuid_has_syscall()) printf("SYSCALL ");
    if (cpuid_has_rdtscp()) printf("RDTSCP ");
    if (cpuid_has_1gb_pages()) printf("1GB_PAGES ");
    if (cpuid_has_lahf_lm()) printf("LAHF_LM ");
    if (cpuid_has_lzcnt()) printf("LZCNT ");
    if (cpuid_has_prefetchw()) printf("PREFETCHW ");
    printf("\n");
    
    /* Security features */
    printf("  Security: ");
    if (cpuid_has_smep()) printf("SMEP ");
    if (cpuid_has_smap()) printf("SMAP ");
    if (cpuid_has_umip()) printf("UMIP ");
    if (cpuid_has_pku()) printf("PKU ");
    if (cpuid_has_ospke()) printf("OSPKE ");
    if (cpuid_has_ibrs_ibpb()) printf("IBRS/IBPB ");
    if (cpuid_has_stibp()) printf("STIBP ");
    if (cpuid_has_l1d_flush()) printf("L1D_FLUSH ");
    if (cpuid_has_arch_capabilities()) printf("ARCH_CAP ");
    if (cpuid_has_ssbd()) printf("SSBD ");
    if (cpuid_has_md_clear()) printf("MD_CLEAR ");
    printf("\n");
    
    /* Virtualization */
    printf("  Virtualization: ");
    if (cpuid_has_vmx()) printf("VMX ");
    if (cpuid_has_smx()) printf("SMX ");
    if (cpuid_has_hypervisor()) printf("HYPERVISOR ");
    if (cpuid_has_pcid()) printf("PCID ");
    if (cpuid_has_x2apic()) printf("X2APIC ");
    printf("\n");
    
    /* Advanced features */
    printf("  Advanced: ");
    if (cpuid_has_waitpkg()) printf("WAITPKG ");
    printf("\n");
}