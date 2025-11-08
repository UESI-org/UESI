#ifndef CPUID_H
#define CPUID_H

#include <stdint.h>
#include <stdbool.h>

#define CPUID_VENDOR_ID                 0x00000000
#define CPUID_FEATURES                  0x00000001
#define CPUID_CACHE_INFO                0x00000002
#define CPUID_SERIAL_NUMBER             0x00000003
#define CPUID_CACHE_PARAMS              0x00000004
#define CPUID_MONITOR_MWAIT             0x00000005
#define CPUID_THERMAL_POWER             0x00000006
#define CPUID_EXTENDED_FEATURES         0x00000007
#define CPUID_EXTENDED_STATE            0x0000000D
#define CPUID_EXT_FUNC_MAX              0x80000000
#define CPUID_EXT_FEATURES              0x80000001
#define CPUID_BRAND_STRING_1            0x80000002
#define CPUID_BRAND_STRING_2            0x80000003
#define CPUID_BRAND_STRING_3            0x80000004
#define CPUID_EXT_CACHE_INFO            0x80000006
#define CPUID_EXT_PROCESSOR_INFO        0x80000008

#define CPUID_FEAT_EDX_FPU              (1 << 0)
#define CPUID_FEAT_EDX_VME              (1 << 1)
#define CPUID_FEAT_EDX_DE               (1 << 2)
#define CPUID_FEAT_EDX_PSE              (1 << 3)
#define CPUID_FEAT_EDX_TSC              (1 << 4)
#define CPUID_FEAT_EDX_MSR              (1 << 5)
#define CPUID_FEAT_EDX_PAE              (1 << 6)
#define CPUID_FEAT_EDX_MCE              (1 << 7)
#define CPUID_FEAT_EDX_CX8              (1 << 8)
#define CPUID_FEAT_EDX_APIC             (1 << 9)
#define CPUID_FEAT_EDX_SEP              (1 << 11)
#define CPUID_FEAT_EDX_MTRR             (1 << 12)
#define CPUID_FEAT_EDX_PGE              (1 << 13)
#define CPUID_FEAT_EDX_MCA              (1 << 14)
#define CPUID_FEAT_EDX_CMOV             (1 << 15)
#define CPUID_FEAT_EDX_PAT              (1 << 16)
#define CPUID_FEAT_EDX_PSE36            (1 << 17)
#define CPUID_FEAT_EDX_PSN              (1 << 18)
#define CPUID_FEAT_EDX_CLFLUSH          (1 << 19)
#define CPUID_FEAT_EDX_DS               (1 << 21)
#define CPUID_FEAT_EDX_ACPI             (1 << 22)
#define CPUID_FEAT_EDX_MMX              (1 << 23)
#define CPUID_FEAT_EDX_FXSR             (1 << 24)
#define CPUID_FEAT_EDX_SSE              (1 << 25)
#define CPUID_FEAT_EDX_SSE2             (1 << 26)
#define CPUID_FEAT_EDX_SS               (1 << 27)
#define CPUID_FEAT_EDX_HTT              (1 << 28)
#define CPUID_FEAT_EDX_TM               (1 << 29)
#define CPUID_FEAT_EDX_PBE              (1 << 31)

#define CPUID_FEAT_ECX_SSE3             (1 << 0)
#define CPUID_FEAT_ECX_PCLMULQDQ        (1 << 1)
#define CPUID_FEAT_ECX_DTES64           (1 << 2)
#define CPUID_FEAT_ECX_MONITOR          (1 << 3)
#define CPUID_FEAT_ECX_DS_CPL           (1 << 4)
#define CPUID_FEAT_ECX_VMX              (1 << 5)
#define CPUID_FEAT_ECX_SMX              (1 << 6)
#define CPUID_FEAT_ECX_EST              (1 << 7)
#define CPUID_FEAT_ECX_TM2              (1 << 8)
#define CPUID_FEAT_ECX_SSSE3            (1 << 9)
#define CPUID_FEAT_ECX_CNXT_ID          (1 << 10)
#define CPUID_FEAT_ECX_FMA              (1 << 12)
#define CPUID_FEAT_ECX_CX16             (1 << 13)
#define CPUID_FEAT_ECX_XTPR             (1 << 14)
#define CPUID_FEAT_ECX_PDCM             (1 << 15)
#define CPUID_FEAT_ECX_PCID             (1 << 17)
#define CPUID_FEAT_ECX_DCA              (1 << 18)
#define CPUID_FEAT_ECX_SSE4_1           (1 << 19)
#define CPUID_FEAT_ECX_SSE4_2           (1 << 20)
#define CPUID_FEAT_ECX_X2APIC           (1 << 21)
#define CPUID_FEAT_ECX_MOVBE            (1 << 22)
#define CPUID_FEAT_ECX_POPCNT           (1 << 23)
#define CPUID_FEAT_ECX_TSC_DEADLINE     (1 << 24)
#define CPUID_FEAT_ECX_AES              (1 << 25)
#define CPUID_FEAT_ECX_XSAVE            (1 << 26)
#define CPUID_FEAT_ECX_OSXSAVE          (1 << 27)
#define CPUID_FEAT_ECX_AVX              (1 << 28)
#define CPUID_FEAT_ECX_F16C             (1 << 29)
#define CPUID_FEAT_ECX_RDRAND           (1 << 30)
#define CPUID_FEAT_ECX_HYPERVISOR       (1U << 31)

#define CPUID_EXT_FEAT_EDX_SYSCALL      (1 << 11)
#define CPUID_EXT_FEAT_EDX_NX           (1 << 20)
#define CPUID_EXT_FEAT_EDX_1GB_PAGE     (1 << 26)
#define CPUID_EXT_FEAT_EDX_RDTSCP       (1 << 27)
#define CPUID_EXT_FEAT_EDX_LM           (1 << 29)

#define CPUID_EXT_FEAT_ECX_LAHF_LM      (1 << 0)
#define CPUID_EXT_FEAT_ECX_SVM          (1 << 2)
#define CPUID_EXT_FEAT_ECX_LZCNT        (1 << 5)
#define CPUID_EXT_FEAT_ECX_SSE4A        (1 << 6)
#define CPUID_EXT_FEAT_ECX_PREFETCHW    (1 << 8)

#define CPUID_EXT7_EBX_FSGSBASE         (1 << 0)
#define CPUID_EXT7_EBX_TSC_ADJUST       (1 << 1)
#define CPUID_EXT7_EBX_BMI1             (1 << 3)
#define CPUID_EXT7_EBX_HLE              (1 << 4)
#define CPUID_EXT7_EBX_AVX2             (1 << 5)
#define CPUID_EXT7_EBX_SMEP             (1 << 7)
#define CPUID_EXT7_EBX_BMI2             (1 << 8)
#define CPUID_EXT7_EBX_ERMS             (1 << 9)
#define CPUID_EXT7_EBX_INVPCID          (1 << 10)
#define CPUID_EXT7_EBX_RTM              (1 << 11)
#define CPUID_EXT7_EBX_RDSEED           (1 << 18)
#define CPUID_EXT7_EBX_ADX              (1 << 19)
#define CPUID_EXT7_EBX_SMAP             (1 << 20)
#define CPUID_EXT7_EBX_CLFLUSHOPT       (1 << 23)
#define CPUID_EXT7_EBX_CLWB             (1 << 24)

typedef struct {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} cpuid_regs_t;

typedef struct {
    char vendor[13];
    char brand[49];
    uint32_t max_basic_leaf;
    uint32_t max_ext_leaf;
    uint32_t stepping;
    uint32_t model;
    uint32_t family;
    uint32_t type;
    uint32_t ext_model;
    uint32_t ext_family;
    uint32_t features_edx;
    uint32_t features_ecx;
    uint32_t ext_features_edx;
    uint32_t ext_features_ecx;
    uint32_t ext7_features_ebx;
    uint32_t ext7_features_ecx;
    uint32_t ext7_features_edx;
    bool cpuid_supported;
} cpu_info_t;

bool cpuid_is_supported(void);
void cpuid(uint32_t leaf, uint32_t subleaf, cpuid_regs_t *regs);
void cpuid_init(cpu_info_t *info);

void cpuid_get_vendor(char *vendor);
void cpuid_get_brand(char *brand);
bool cpuid_has_feature(uint32_t feature_flag);
bool cpuid_has_ext_feature(uint32_t feature_flag);
bool cpuid_has_ext7_feature(uint32_t feature_flag);

static inline bool cpuid_has_sse(void) {
    return cpuid_has_feature(CPUID_FEAT_EDX_SSE);
}

static inline bool cpuid_has_sse2(void) {
    return cpuid_has_feature(CPUID_FEAT_EDX_SSE2);
}

static inline bool cpuid_has_sse3(void) {
    return cpuid_has_feature(CPUID_FEAT_ECX_SSE3);
}

static inline bool cpuid_has_avx(void) {
    return cpuid_has_feature(CPUID_FEAT_ECX_AVX);
}

static inline bool cpuid_has_apic(void) {
    return cpuid_has_feature(CPUID_FEAT_EDX_APIC);
}

static inline bool cpuid_has_msr(void) {
    return cpuid_has_feature(CPUID_FEAT_EDX_MSR);
}

static inline bool cpuid_has_pae(void) {
    return cpuid_has_feature(CPUID_FEAT_EDX_PAE);
}

static inline bool cpuid_has_nx(void) {
    return cpuid_has_ext_feature(CPUID_EXT_FEAT_EDX_NX);
}

static inline bool cpuid_has_long_mode(void) {
    return cpuid_has_ext_feature(CPUID_EXT_FEAT_EDX_LM);
}

static inline bool cpuid_has_1gb_pages(void) {
    return cpuid_has_ext_feature(CPUID_EXT_FEAT_EDX_1GB_PAGE);
}

#endif