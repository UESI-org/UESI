#ifndef TSC_H
#define TSC_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	TSC_UNCALIBRATED = 0,
	TSC_CALIBRATED_PIT = 1,
	TSC_CALIBRATED_CPUID = 2
} tsc_calibration_method_t;

typedef struct {
	uint64_t frequency_hz;
	tsc_calibration_method_t calibration_method;
	bool available;
	bool invariant;
	bool rdtscp_available;
	uint64_t calibration_start_tsc;
	uint64_t calibration_duration_ms;
} tsc_info_t;

void tsc_init(void);

static inline uint64_t tsc_read(void);
static inline uint64_t tsc_read_p(uint32_t *processor_id);
const tsc_info_t *tsc_get_info(void);

uint64_t tsc_to_ns(uint64_t ticks);
uint64_t ns_to_tsc(uint64_t nanoseconds);
uint64_t tsc_get_time_ns(void);
uint64_t tsc_get_time_us(void);
uint64_t tsc_get_time_ms(void);

void tsc_delay_ns(uint64_t nanoseconds);
void tsc_delay_us(uint64_t microseconds);
void tsc_delay_ms(uint64_t milliseconds);

bool tsc_calibrate_pit(uint32_t calibration_ms);
bool tsc_calibrate_cpuid(void);
uint64_t tsc_get_frequency(void);

bool tsc_is_available(void);
bool tsc_is_invariant(void);
bool tsc_has_rdtscp(void);
void tsc_print_info(void);

static inline uint64_t
tsc_read(void)
{
	uint32_t lo, hi;
	__asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t
tsc_read_p(uint32_t *processor_id)
{
	uint32_t lo, hi, aux;
	__asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
	if (processor_id) {
		*processor_id = aux;
	}
	return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t
tsc_read_serialized(void)
{
	uint32_t lo, hi;
	__asm__ volatile("lfence\n\t"
	                 "rdtsc\n\t"
	                 "lfence"
	                 : "=a"(lo), "=d"(hi)
	                 :
	                 : "memory");
	return ((uint64_t)hi << 32) | lo;
}

#endif
