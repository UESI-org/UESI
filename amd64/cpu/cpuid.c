#include "cpuid.h"
#include <stddef.h>

static cpu_info_t g_cpu_info = {0};
static bool g_cpu_info_initialized = false;

bool cpuid_is_supported(void) {
    uint64_t flags1, flags2;
    
    __asm__ volatile (
        "pushfq\n"                    // Push RFLAGS onto stack
        "pop %0\n"                    // Pop into flags1
        "mov %0, %1\n"                // Copy to flags2
        "xor $0x200000, %1\n"         // Flip ID bit (bit 21)
        "push %1\n"                   // Push modified value
        "popfq\n"                     // Pop into RFLAGS
        "pushfq\n"                    // Push RFLAGS again
        "pop %1\n"                    // Pop into flags2
        "push %0\n"                   // Restore original RFLAGS
        "popfq\n"
        : "=&r" (flags1), "=&r" (flags2)
        :
        : "cc"
    );
    
    return ((flags1 ^ flags2) & 0x200000) != 0;
}

void cpuid(uint32_t leaf, uint32_t subleaf, cpuid_regs_t *regs) {
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

void cpuid_get_vendor(char *vendor) {
    if (!vendor) {
        return;
    }
    
    cpuid_regs_t regs;
    cpuid(CPUID_VENDOR_ID, 0, &regs);
    
    *((uint32_t *)(vendor + 0)) = regs.ebx;
    *((uint32_t *)(vendor + 4)) = regs.edx;
    *((uint32_t *)(vendor + 8)) = regs.ecx;
    vendor[12] = '\0';
}

void cpuid_get_brand(char *brand) {
    if (!brand) {
        return;
    }
    
    cpuid_regs_t regs;
    uint32_t *brand_ptr = (uint32_t *)brand;
    
    cpuid(CPUID_EXT_FUNC_MAX, 0, &regs);
    if (regs.eax < CPUID_BRAND_STRING_3) {
        brand[0] = '\0';
        return;
    }
    
    // Brand string is in three parts (0x80000002-0x80000004)
    for (uint32_t i = 0; i < 3; i++) {
        cpuid(CPUID_BRAND_STRING_1 + i, 0, &regs);
        brand_ptr[i * 4 + 0] = regs.eax;
        brand_ptr[i * 4 + 1] = regs.ebx;
        brand_ptr[i * 4 + 2] = regs.ecx;
        brand_ptr[i * 4 + 3] = regs.edx;
    }
    
    brand[48] = '\0';
}

bool cpuid_has_feature(uint32_t feature_flag) {
    if (!g_cpu_info_initialized) {
        return false;
    }
    
    // Check if flag is in EDX or ECX based on bit position
    // Flags above bit 31 are not valid for a single register
    if (feature_flag & 0x80000000) {
        return (g_cpu_info.features_ecx & feature_flag) != 0;
    } else {
        return ((g_cpu_info.features_edx & feature_flag) != 0) ||
               ((g_cpu_info.features_ecx & feature_flag) != 0);
    }
}

bool cpuid_has_ext_feature(uint32_t feature_flag) {
    if (!g_cpu_info_initialized) {
        return false;
    }
    
    return ((g_cpu_info.ext_features_edx & feature_flag) != 0) ||
           ((g_cpu_info.ext_features_ecx & feature_flag) != 0);
}

bool cpuid_has_ext7_feature(uint32_t feature_flag) {
    if (!g_cpu_info_initialized) {
        return false;
    }
    
    return ((g_cpu_info.ext7_features_ebx & feature_flag) != 0) ||
           ((g_cpu_info.ext7_features_ecx & feature_flag) != 0) ||
           ((g_cpu_info.ext7_features_edx & feature_flag) != 0);
}

void cpuid_init(cpu_info_t *info) {
    if (!info) {
        return;
    }
    
    cpuid_regs_t regs;
    
    info->cpuid_supported = cpuid_is_supported();
    if (!info->cpuid_supported) {
        return;
    }
    
    cpuid(CPUID_VENDOR_ID, 0, &regs);
    info->max_basic_leaf = regs.eax;
    *((uint32_t *)(info->vendor + 0)) = regs.ebx;
    *((uint32_t *)(info->vendor + 4)) = regs.edx;
    *((uint32_t *)(info->vendor + 8)) = regs.ecx;
    info->vendor[12] = '\0';
    
    if (info->max_basic_leaf >= CPUID_FEATURES) {
        cpuid(CPUID_FEATURES, 0, &regs);
        
        info->stepping = regs.eax & 0xF;
        info->model = (regs.eax >> 4) & 0xF;
        info->family = (regs.eax >> 8) & 0xF;
        info->type = (regs.eax >> 12) & 0x3;
        info->ext_model = (regs.eax >> 16) & 0xF;
        info->ext_family = (regs.eax >> 20) & 0xFF;
        
        if (info->family == 0xF) {
            info->family += info->ext_family;
        }
        if (info->family == 0x6 || info->family == 0xF) {
            info->model += (info->ext_model << 4);
        }
        
        info->features_ecx = regs.ecx;
        info->features_edx = regs.edx;
    }
    
    cpuid(CPUID_EXT_FUNC_MAX, 0, &regs);
    info->max_ext_leaf = regs.eax;
    
    if (info->max_ext_leaf >= CPUID_EXT_FEATURES) {
        cpuid(CPUID_EXT_FEATURES, 0, &regs);
        info->ext_features_ecx = regs.ecx;
        info->ext_features_edx = regs.edx;
    }
    
    // Get brand string if available
    if (info->max_ext_leaf >= CPUID_BRAND_STRING_3) {
        uint32_t *brand_ptr = (uint32_t *)info->brand;
        for (uint32_t i = 0; i < 3; i++) {
            cpuid(CPUID_BRAND_STRING_1 + i, 0, &regs);
            brand_ptr[i * 4 + 0] = regs.eax;
            brand_ptr[i * 4 + 1] = regs.ebx;
            brand_ptr[i * 4 + 2] = regs.ecx;
            brand_ptr[i * 4 + 3] = regs.edx;
        }
        info->brand[48] = '\0';
    } else {
        info->brand[0] = '\0';
    }

    if (info->max_basic_leaf >= CPUID_EXTENDED_FEATURES) {
        cpuid(CPUID_EXTENDED_FEATURES, 0, &regs);
        info->ext7_features_ebx = regs.ebx;
        info->ext7_features_ecx = regs.ecx;
        info->ext7_features_edx = regs.edx;
    }
    
    g_cpu_info = *info;
    g_cpu_info_initialized = true;
}