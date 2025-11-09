#ifndef CPUID_H
#define CPUID_H

#include <stdint.h>
#include <stdbool.h>
#include "specialreg.h"

/*
 * CPUID Leaf Numbers
 */
#define CPUID_VENDOR_ID                 0x00000000
#define CPUID_FEATURES                  0x00000001
#define CPUID_CACHE_INFO                0x00000002
#define CPUID_SERIAL_NUMBER             0x00000003
#define CPUID_CACHE_PARAMS              0x00000004
#define CPUID_MONITOR_MWAIT             0x00000005
#define CPUID_THERMAL_POWER             0x00000006
#define CPUID_EXTENDED_FEATURES         0x00000007
#define CPUID_EXTENDED_STATE            0x0000000D
#define CPUID_PROCESSOR_TRACE           0x00000014
#define CPUID_TIME_STAMP_COUNTER        0x00000015

/* Extended CPUID Functions */
#define CPUID_EXT_FUNC_MAX              0x80000000
#define CPUID_EXT_FEATURES              0x80000001
#define CPUID_BRAND_STRING_1            0x80000002
#define CPUID_BRAND_STRING_2            0x80000003
#define CPUID_BRAND_STRING_3            0x80000004
#define CPUID_EXT_CACHE_INFO            0x80000006
#define CPUID_EXT_PROCESSOR_INFO        0x80000008

/*
 * CPU Vendor IDs
 */
#define CPU_VENDOR_INTEL        "GenuineIntel"
#define CPU_VENDOR_AMD          "AuthenticAMD"
#define CPU_VENDOR_VIA          "VIA VIA VIA "
#define CPU_VENDOR_TRANSMETA    "GenuineTMx86"
#define CPU_VENDOR_CYRIX        "CyrixInstead"
#define CPU_VENDOR_CENTAUR      "CentaurHauls"
#define CPU_VENDOR_NEXGEN       "NexGenDriven"
#define CPU_VENDOR_UMC          "UMC UMC UMC "
#define CPU_VENDOR_SIS          "SiS SiS SiS "
#define CPU_VENDOR_NSC          "Geode by NSC"
#define CPU_VENDOR_RISE         "RiseRiseRise"
#define CPU_VENDOR_VORTEX       "Vortex86 SoC"

/*
 * CPU Type (from Family/Model/Stepping)
 */
#define CPU_TYPE_PRIMARY        0
#define CPU_TYPE_OVERDRIVE      1
#define CPU_TYPE_SECONDARY      2
#define CPU_TYPE_RESERVED       3

/*
 * CPUID Register Structure
 */
typedef struct {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} cpuid_regs_t;

/*
 * CPU Information Structure
 */
typedef struct {
    /* Vendor and Brand Information */
    char vendor[13];
    char brand[49];
    
    /* Maximum Supported CPUID Leaves */
    uint32_t max_basic_leaf;
    uint32_t max_ext_leaf;
    
    /* Family, Model, Stepping */
    uint32_t stepping;
    uint32_t model;
    uint32_t family;
    uint32_t type;
    uint32_t ext_model;
    uint32_t ext_family;
    uint32_t display_family;
    uint32_t display_model;
    
    /* Feature Flags (Leaf 0x1) */
    uint32_t features_edx;          /* EDX: Basic features */
    uint32_t features_ecx;          /* ECX: Basic features */
    
    /* Extended Feature Flags (Leaf 0x80000001) */
    uint32_t ext_features_edx;      /* EDX: Extended features */
    uint32_t ext_features_ecx;      /* ECX: Extended features */
    
    /* Structured Extended Features (Leaf 0x7, Subleaf 0) */
    uint32_t ext7_features_ebx;     /* EBX: SEFF features */
    uint32_t ext7_features_ecx;     /* ECX: SEFF features */
    uint32_t ext7_features_edx;     /* EDX: SEFF features */
    
    /* Cache Line Size */
    uint32_t cache_line_size;
    
    /* Logical Processor Count */
    uint32_t logical_processors;
    uint32_t initial_apic_id;
    
    /* Extended State Information (XSAVE) */
    uint32_t xsave_features;
    uint32_t xsave_size;
    uint32_t xsave_user_size;
    
    /* Flags */
    bool cpuid_supported;
    bool is_intel;
    bool is_amd;
    bool is_hypervisor;
} cpu_info_t;

/*
 * Core CPUID Functions
 */
bool cpuid_is_supported(void);
void cpuid_exec(uint32_t leaf, uint32_t subleaf, cpuid_regs_t *regs);
void cpuid_init(cpu_info_t *info);
const cpu_info_t *cpuid_get_info(void);

/*
 * Vendor and Brand Functions
 */
void cpuid_get_vendor(char *vendor);
void cpuid_get_brand(char *brand);
bool cpuid_is_intel(void);
bool cpuid_is_amd(void);

/*
 * Feature Detection Functions
 * These use the constants from specialreg.h
 */
bool cpuid_has_feature(uint32_t feature_flag);
bool cpuid_has_ext_feature(uint32_t feature_flag);
bool cpuid_has_ext7_feature(uint32_t feature_flag);

/*
 * Basic Feature Helpers (CPUID leaf 0x1 - specialreg.h: CPUID_*)
 */
static inline bool cpuid_has_fpu(void) {
    return cpuid_has_feature(CPUID_FPU);
}

static inline bool cpuid_has_vme(void) {
    return cpuid_has_feature(CPUID_VME);
}

static inline bool cpuid_has_pse(void) {
    return cpuid_has_feature(CPUID_PSE);
}

static inline bool cpuid_has_tsc(void) {
    return cpuid_has_feature(CPUID_TSC);
}

static inline bool cpuid_has_msr(void) {
    return cpuid_has_feature(CPUID_MSR);
}

static inline bool cpuid_has_pae(void) {
    return cpuid_has_feature(CPUID_PAE);
}

static inline bool cpuid_has_mce(void) {
    return cpuid_has_feature(CPUID_MCE);
}

static inline bool cpuid_has_cx8(void) {
    return cpuid_has_feature(CPUID_CX8);
}

static inline bool cpuid_has_apic(void) {
    return cpuid_has_feature(CPUID_APIC);
}

static inline bool cpuid_has_sep(void) {
    return cpuid_has_feature(CPUID_SEP);
}

static inline bool cpuid_has_mtrr(void) {
    return cpuid_has_feature(CPUID_MTRR);
}

static inline bool cpuid_has_pge(void) {
    return cpuid_has_feature(CPUID_PGE);
}

static inline bool cpuid_has_cmov(void) {
    return cpuid_has_feature(CPUID_CMOV);
}

static inline bool cpuid_has_pat(void) {
    return cpuid_has_feature(CPUID_PAT);
}

static inline bool cpuid_has_clflush(void) {
    return cpuid_has_feature(CPUID_CFLUSH);
}

static inline bool cpuid_has_mmx(void) {
    return cpuid_has_feature(CPUID_MMX);
}

static inline bool cpuid_has_fxsr(void) {
    return cpuid_has_feature(CPUID_FXSR);
}

static inline bool cpuid_has_sse(void) {
    return cpuid_has_feature(CPUID_SSE);
}

static inline bool cpuid_has_sse2(void) {
    return cpuid_has_feature(CPUID_SSE2);
}

static inline bool cpuid_has_htt(void) {
    return cpuid_has_feature(CPUID_HTT);
}

/*
 * Extended Feature Helpers (CPUID leaf 0x1 ECX - specialreg.h: CPUIDECX_*)
 */
static inline bool cpuid_has_sse3(void) {
    return cpuid_has_feature(CPUIDECX_SSE3);
}

static inline bool cpuid_has_pclmulqdq(void) {
    return cpuid_has_feature(CPUIDECX_PCLMUL);
}

static inline bool cpuid_has_monitor(void) {
    return cpuid_has_feature(CPUIDECX_MWAIT);
}

static inline bool cpuid_has_vmx(void) {
    return cpuid_has_feature(CPUIDECX_VMX);
}

static inline bool cpuid_has_smx(void) {
    return cpuid_has_feature(CPUIDECX_SMX);
}

static inline bool cpuid_has_ssse3(void) {
    return cpuid_has_feature(CPUIDECX_SSSE3);
}

static inline bool cpuid_has_fma(void) {
    return cpuid_has_feature(CPUIDECX_FMA3);
}

static inline bool cpuid_has_cx16(void) {
    return cpuid_has_feature(CPUIDECX_CX16);
}

static inline bool cpuid_has_pcid(void) {
    return cpuid_has_feature(CPUIDECX_PCID);
}

static inline bool cpuid_has_sse4_1(void) {
    return cpuid_has_feature(CPUIDECX_SSE41);
}

static inline bool cpuid_has_sse4_2(void) {
    return cpuid_has_feature(CPUIDECX_SSE42);
}

static inline bool cpuid_has_x2apic(void) {
    return cpuid_has_feature(CPUIDECX_X2APIC);
}

static inline bool cpuid_has_movbe(void) {
    return cpuid_has_feature(CPUIDECX_MOVBE);
}

static inline bool cpuid_has_popcnt(void) {
    return cpuid_has_feature(CPUIDECX_POPCNT);
}

static inline bool cpuid_has_aes(void) {
    return cpuid_has_feature(CPUIDECX_AES);
}

static inline bool cpuid_has_xsave(void) {
    return cpuid_has_feature(CPUIDECX_XSAVE);
}

static inline bool cpuid_has_osxsave(void) {
    return cpuid_has_feature(CPUIDECX_OSXSAVE);
}

static inline bool cpuid_has_avx(void) {
    return cpuid_has_feature(CPUIDECX_AVX);
}

static inline bool cpuid_has_f16c(void) {
    return cpuid_has_feature(CPUIDECX_F16C);
}

static inline bool cpuid_has_rdrand(void) {
    return cpuid_has_feature(CPUIDECX_RDRAND);
}

static inline bool cpuid_has_hypervisor(void) {
    return cpuid_has_feature(CPUIDECX_HV);
}

/*
 * Extended CPUID Features (leaf 0x80000001 - specialreg.h: CPUID_*)
 */
static inline bool cpuid_has_syscall(void) {
    return cpuid_has_ext_feature(CPUID_SEP);
}

static inline bool cpuid_has_nx(void) {
    return cpuid_has_ext_feature(CPUID_NXE);
}

static inline bool cpuid_has_1gb_pages(void) {
    return cpuid_has_ext_feature(CPUID_PAGE1GB);
}

static inline bool cpuid_has_rdtscp(void) {
    return cpuid_has_ext_feature(CPUID_RDTSCP);
}

static inline bool cpuid_has_long_mode(void) {
    return cpuid_has_ext_feature(CPUID_LONG);
}

static inline bool cpuid_has_lahf_lm(void) {
    return cpuid_has_ext_feature(CPUIDECX_LAHF);
}

static inline bool cpuid_has_lzcnt(void) {
    return cpuid_has_ext_feature(CPUIDECX_ABM);
}

static inline bool cpuid_has_sse4a(void) {
    return cpuid_has_ext_feature(CPUIDECX_SSE4A);
}

static inline bool cpuid_has_prefetchw(void) {
    return cpuid_has_ext_feature(CPUIDECX_3DNOWP);
}

/*
 * Structured Extended Features (leaf 0x7 - specialreg.h: SEFF0EBX_*)
 */
static inline bool cpuid_has_fsgsbase(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_FSGSBASE);
}

static inline bool cpuid_has_tsc_adjust(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_TSC_ADJUST);
}

static inline bool cpuid_has_sgx(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_SGX);
}

static inline bool cpuid_has_bmi1(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_BMI1);
}

static inline bool cpuid_has_hle(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_HLE);
}

static inline bool cpuid_has_avx2(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_AVX2);
}

static inline bool cpuid_has_smep(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_SMEP);
}

static inline bool cpuid_has_bmi2(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_BMI2);
}

static inline bool cpuid_has_erms(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_ERMS);
}

static inline bool cpuid_has_invpcid(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_INVPCID);
}

static inline bool cpuid_has_rtm(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_RTM);
}

static inline bool cpuid_has_mpx(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_MPX);
}

static inline bool cpuid_has_avx512f(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_AVX512F);
}

static inline bool cpuid_has_avx512dq(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_AVX512DQ);
}

static inline bool cpuid_has_rdseed(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_RDSEED);
}

static inline bool cpuid_has_adx(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_ADX);
}

static inline bool cpuid_has_smap(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_SMAP);
}

static inline bool cpuid_has_avx512_ifma(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_AVX512IFMA);
}

static inline bool cpuid_has_clflushopt(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_CLFLUSHOPT);
}

static inline bool cpuid_has_clwb(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_CLWB);
}

static inline bool cpuid_has_processor_trace(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_PT);
}

static inline bool cpuid_has_avx512bw(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_AVX512BW);
}

static inline bool cpuid_has_avx512vl(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_AVX512VL);
}

static inline bool cpuid_has_sha(void) {
    return cpuid_has_ext7_feature(SEFF0EBX_SHA);
}

/*
 * Structured Extended Features ECX (leaf 0x7 - specialreg.h: SEFF0ECX_*)
 */
static inline bool cpuid_has_umip(void) {
    return cpuid_has_ext7_feature(SEFF0ECX_UMIP);
}

static inline bool cpuid_has_pku(void) {
    return cpuid_has_ext7_feature(SEFF0ECX_PKU);
}

static inline bool cpuid_has_ospke(void) {
    return cpuid_has_ext7_feature(SEFF0ECX_OSPKE);
}

static inline bool cpuid_has_waitpkg(void) {
    return cpuid_has_ext7_feature(SEFF0ECX_WAITPKG);
}

/*
 * Structured Extended Features EDX (leaf 0x7 - specialreg.h: SEFF0EDX_*)
 */
static inline bool cpuid_has_md_clear(void) {
    return cpuid_has_ext7_feature(SEFF0EDX_MD_CLEAR);
}

static inline bool cpuid_has_ibrs_ibpb(void) {
    return cpuid_has_ext7_feature(SEFF0EDX_IBRS);
}

static inline bool cpuid_has_stibp(void) {
    return cpuid_has_ext7_feature(SEFF0EDX_STIBP);
}

static inline bool cpuid_has_l1d_flush(void) {
    return cpuid_has_ext7_feature(SEFF0EDX_L1DF);
}

static inline bool cpuid_has_arch_capabilities(void) {
    return cpuid_has_ext7_feature(SEFF0EDX_ARCH_CAP);
}

static inline bool cpuid_has_ssbd(void) {
    return cpuid_has_ext7_feature(SEFF0EDX_SSBD);
}

/*
 * Cache Information
 */
uint32_t cpuid_get_cache_line_size(void);

/*
 * Debug/Utility Functions
 */
void cpuid_print_info(const cpu_info_t *info);
void cpuid_print_features(const cpu_info_t *info);

#endif