#include <system_info.h>
#include <stdbool.h>
#include <stdint.h>
#include <serial.h>
#include <serial_debug.h>
#include <tty.h>
#include <printf.h>
#include <pmm.h>

static void
print_cpu_basic_info(const cpu_info_t *info)
{
	if (!info || !info->cpuid_supported) {
		return;
	}

	serial_printf(DEBUG_PORT, "CPU: %s", info->vendor);
	if (info->brand[0]) {
		serial_printf(DEBUG_PORT, " %s", info->brand);
	}
	serial_printf(DEBUG_PORT,
	              " (Family: %u, Model: %u, Stepping: %u)\n",
	              info->display_family,
	              info->display_model,
	              info->stepping);
}

static void
print_cpu_features(const cpu_info_t *info)
{
	if (!info || !info->cpuid_supported) {
		return;
	}

	serial_printf(DEBUG_PORT, "Features: ");

	if (cpuid_has_sse2())
		serial_printf(DEBUG_PORT, "SSE2 ");
	if (cpuid_has_sse3())
		serial_printf(DEBUG_PORT, "SSE3 ");
	if (cpuid_has_ssse3())
		serial_printf(DEBUG_PORT, "SSSE3 ");
	if (cpuid_has_sse4_1())
		serial_printf(DEBUG_PORT, "SSE4.1 ");
	if (cpuid_has_sse4_2())
		serial_printf(DEBUG_PORT, "SSE4.2 ");
	if (cpuid_has_avx())
		serial_printf(DEBUG_PORT, "AVX ");
	if (cpuid_has_avx2())
		serial_printf(DEBUG_PORT, "AVX2 ");
	if (cpuid_has_avx512f())
		serial_printf(DEBUG_PORT, "AVX512 ");
	if (cpuid_has_aes())
		serial_printf(DEBUG_PORT, "AES ");
	if (cpuid_has_long_mode())
		serial_printf(DEBUG_PORT, "x64 ");
	if (cpuid_has_nx())
		serial_printf(DEBUG_PORT, "NX ");
	if (cpuid_has_1gb_pages())
		serial_printf(DEBUG_PORT, "1GB-Pages ");

	serial_printf(DEBUG_PORT, "\n");

	if (cpuid_has_smep() || cpuid_has_smap() || cpuid_has_umip()) {
		serial_printf(DEBUG_PORT, "Security: ");
		if (cpuid_has_smep())
			serial_printf(DEBUG_PORT, "SMEP ");
		if (cpuid_has_smap())
			serial_printf(DEBUG_PORT, "SMAP ");
		if (cpuid_has_umip())
			serial_printf(DEBUG_PORT, "UMIP ");
		if (cpuid_has_ibrs_ibpb())
			serial_printf(DEBUG_PORT, "IBRS/IBPB ");
		serial_printf(DEBUG_PORT, "\n");
	}
}

void
system_print_cpu_info(const cpu_info_t *cpu)
{
	serial_printf(DEBUG_PORT, "\n=== CPU Information ===\n");
	print_cpu_basic_info(cpu);
	print_cpu_features(cpu);
}

void
system_print_banner(const cpu_info_t *cpu)
{
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
		if (!first)
			printf(", ");
		printf("SSE3");
		first = false;
	}
	if (cpuid_has_avx()) {
		if (!first)
			printf(", ");
		printf("AVX");
		first = false;
	}
	if (cpuid_has_1gb_pages()) {
		if (!first)
			printf(", ");
		printf("1GB Pages");
		first = false;
	}
	printf("\n\n");
}