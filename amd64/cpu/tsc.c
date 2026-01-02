#include <tsc.h>
#include <cpuid.h>
#include <io.h>
#include <printf.h>
#include <specialreg.h>
#include <timer.h>

static tsc_info_t g_tsc_info = { 0 };
static bool g_tsc_initialized = false;

#define PIT_FREQUENCY 1193182UL
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND 0x43

static bool
tsc_check_invariant(void)
{
	cpuid_regs_t regs;
	const cpu_info_t *cpu_info = cpuid_get_info();

	if (!cpu_info || cpu_info->max_ext_leaf < 0x80000007) {
		return false;
	}

	cpuid_exec(0x80000007, 0, &regs);
	return (regs.edx & CPUIDEDX_ITSC) != 0;
}

bool
tsc_calibrate_cpuid(void)
{
	cpuid_regs_t regs;
	const cpu_info_t *cpu_info = cpuid_get_info();

	if (!cpu_info || cpu_info->max_basic_leaf < CPUID_TIME_STAMP_COUNTER) {
		return false;
	}

	/* Leaf 0x15: TSC/Crystal Clock Information */
	cpuid_exec(CPUID_TIME_STAMP_COUNTER, 0, &regs);

	uint32_t denominator = regs.eax;
	uint32_t numerator = regs.ebx;
	uint32_t crystal_hz = regs.ecx;

	/* Check if data is valid */
	if (denominator == 0 || numerator == 0) {
		return false;
	}

	/* If crystal frequency is provided, calculate TSC frequency */
	if (crystal_hz != 0) {
		g_tsc_info.frequency_hz =
		    ((uint64_t)crystal_hz * numerator) / denominator;
		g_tsc_info.calibration_method = TSC_CALIBRATED_CPUID;
		return true;
	}

	/* Try leaf 0x16 for base frequency (MHz) */
	if (cpu_info->max_basic_leaf >= 0x16) {
		cpuid_exec(0x16, 0, &regs);
		uint32_t base_mhz = regs.eax & 0xFFFF;
		if (base_mhz != 0) {
			/* Approximate TSC frequency from base frequency */
			g_tsc_info.frequency_hz = (uint64_t)base_mhz * 1000000;
			g_tsc_info.calibration_method = TSC_CALIBRATED_CPUID;
			return true;
		}
	}

	return false;
}

bool
tsc_calibrate_pit(uint32_t calibration_ms)
{
	if (calibration_ms == 0 || calibration_ms > 1000) {
		calibration_ms = 100; /* Default to 100ms */
	}

	/* Calculate PIT divisor for desired duration */
	uint32_t pit_ticks = (PIT_FREQUENCY * calibration_ms) / 1000;
	if (pit_ticks > 65535) {
		pit_ticks = 65535;
	}

	/* Set up PIT channel 2 for one-shot mode */
	outb(PIT_COMMAND, 0xB0); /* Channel 2, lo/hi byte, mode 0 */

	/* Read start TSC */
	uint64_t start_tsc = tsc_read();

	/* Program PIT */
	outb(0x42, (uint8_t)(pit_ticks & 0xFF));
	outb(0x42, (uint8_t)((pit_ticks >> 8) & 0xFF));

	/* Start PIT channel 2 */
	uint8_t tmp = inb(0x61);
	outb(0x61, tmp | 0x01);

	/* Wait for PIT to finish (bit 5 of port 0x61 goes high) */
	while (!(inb(0x61) & 0x20))
		__asm__ volatile("pause");

	/* Read end TSC */
	uint64_t end_tsc = tsc_read();

	/* Stop PIT channel 2 */
	outb(0x61, tmp);

	/* Calculate frequency */
	uint64_t tsc_ticks = end_tsc - start_tsc;
	g_tsc_info.frequency_hz = (tsc_ticks * 1000) / calibration_ms;
	g_tsc_info.calibration_method = TSC_CALIBRATED_PIT;
	g_tsc_info.calibration_start_tsc = start_tsc;
	g_tsc_info.calibration_duration_ms = calibration_ms;

	return true;
}

void
tsc_init(void)
{
	if (g_tsc_initialized) {
		return;
	}

	/* Check if TSC is available */
	if (!cpuid_has_tsc()) {
		g_tsc_info.available = false;
		g_tsc_initialized = true;
		return;
	}

	g_tsc_info.available = true;
	g_tsc_info.invariant = tsc_check_invariant();
	g_tsc_info.rdtscp_available = cpuid_has_rdtscp();

	/* Try CPUID calibration first */
	if (!tsc_calibrate_cpuid()) {
		/* Fall back to PIT calibration */
		tsc_calibrate_pit(100);
	}

	g_tsc_initialized = true;
}

const tsc_info_t *
tsc_get_info(void)
{
	if (!g_tsc_initialized) {
		return NULL;
	}
	return &g_tsc_info;
}

uint64_t
tsc_get_frequency(void)
{
	return g_tsc_info.frequency_hz;
}

bool
tsc_is_available(void)
{
	return g_tsc_initialized && g_tsc_info.available &&
	       g_tsc_info.frequency_hz > 0;
}

bool
tsc_is_invariant(void)
{
	return g_tsc_info.invariant;
}

bool
tsc_has_rdtscp(void)
{
	return g_tsc_info.rdtscp_available;
}

uint64_t
tsc_to_ns(uint64_t ticks)
{
	if (g_tsc_info.frequency_hz == 0) {
		return 0;
	}
	/* ns = (ticks * 1000000000) / freq_hz */
	return (ticks * 1000000000ULL) / g_tsc_info.frequency_hz;
}

uint64_t
ns_to_tsc(uint64_t nanoseconds)
{
	if (g_tsc_info.frequency_hz == 0) {
		return 0;
	}
	/* ticks = (ns * freq_hz) / 1000000000 */
	return (nanoseconds * g_tsc_info.frequency_hz) / 1000000000ULL;
}

uint64_t
tsc_get_time_ns(void)
{
	if (!tsc_is_available()) {
		return 0;
	}
	return tsc_to_ns(tsc_read());
}

uint64_t
tsc_get_time_us(void)
{
	return tsc_get_time_ns() / 1000;
}

uint64_t
tsc_get_time_ms(void)
{
	return tsc_get_time_ns() / 1000000;
}

void
tsc_delay_ns(uint64_t nanoseconds)
{
	if (!tsc_is_available()) {
		return;
	}

	uint64_t ticks = ns_to_tsc(nanoseconds);
	uint64_t end = tsc_read() + ticks;

	while (tsc_read() < end) {
		__asm__ volatile("pause");
	}
}

void
tsc_delay_us(uint64_t microseconds)
{
	tsc_delay_ns(microseconds * 1000);
}

void
tsc_delay_ms(uint64_t milliseconds)
{
	tsc_delay_ns(milliseconds * 1000000);
}

void
tsc_print_info(void)
{
	if (!g_tsc_initialized) {
		printf("TSC: Not initialized\n");
		return;
	}

	if (!g_tsc_info.available) {
		printf("TSC: Not available\n");
		return;
	}

	printf("TSC Information:\n");
	printf("  Frequency: %llu Hz (%llu MHz)\n",
	       g_tsc_info.frequency_hz,
	       g_tsc_info.frequency_hz / 1000000);

	printf("  Calibration: ");
	switch (g_tsc_info.calibration_method) {
	case TSC_CALIBRATED_CPUID:
		printf("CPUID\n");
		break;
	case TSC_CALIBRATED_PIT:
		printf("PIT (%llu ms)\n", g_tsc_info.calibration_duration_ms);
		break;
	default:
		printf("Uncalibrated\n");
		break;
	}

	printf("  Invariant TSC: %s\n", g_tsc_info.invariant ? "Yes" : "No");
	printf("  RDTSCP: %s\n",
	       g_tsc_info.rdtscp_available ? "Available" : "Not available");

	if (!g_tsc_info.invariant) {
		printf("  Warning: TSC may vary with CPU frequency changes\n");
	}
}
