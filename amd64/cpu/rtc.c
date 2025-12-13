/* This rtc.c should remain heavily commented! it's a complex software!!!*/

#include <rtc.h>
#include <io.h>
#include <isr.h>
#include <pic.h>
#include <stddef.h>

/* Static state */
static rtc_handler_t rtc_periodic_handler = NULL;
static uint8_t rtc_century_reg = RTC_REG_CENTURY;
static timezone_t current_timezone = TZ_UTC;

static const timezone_info_t timezone_table[] = {
	{ "UTC", 0, 0, 0 },
	{ "CET", 3600, 7200, 1 },     /* UTC+1, DST: UTC+2 */
	{ "EST", -18000, -14400, 1 }, /* UTC-5, DST: UTC-4 */
	{ "PST", -28800, -25200, 1 }, /* UTC-8, DST: UTC-7 */
	{ "JST", 32400, 32400, 0 },   /* UTC+9, no DST */
	{ "AEST", 36000, 39600, 1 },  /* UTC+10, DST: UTC+11 */
};

uint8_t
rtc_read_register(uint8_t reg)
{
	/* Disable NMI and select register */
	outb(RTC_ADDR_PORT, (RTC_NMI_DISABLE | reg));
	io_wait(); /* Small delay for old hardware */

	/* Read value from data port */
	return inb(RTC_DATA_PORT);
}

void
rtc_write_register(uint8_t reg, uint8_t value)
{
	/* Disable NMI and select register */
	outb(RTC_ADDR_PORT, (RTC_NMI_DISABLE | reg));
	io_wait();

	/* Write value to data port */
	outb(RTC_DATA_PORT, value);
	io_wait();
}

static void
rtc_wait_for_update(void)
{
	/* Wait until UIP flag is clear */
	while (rtc_read_register(RTC_REG_STATUS_A) & RTC_STATUS_A_UIP)
		;
}

static uint8_t
bcd_to_binary(uint8_t bcd)
{
	return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

static uint8_t
binary_to_bcd(uint8_t binary)
{
	return ((binary / 10) << 4) | (binary % 10);
}

int
rtc_is_binary_mode(void)
{
	uint8_t status_b = rtc_read_register(RTC_REG_STATUS_B);
	return (status_b & RTC_STATUS_B_DM) ? 1 : 0;
}

int
rtc_is_24hour_mode(void)
{
	uint8_t status_b = rtc_read_register(RTC_REG_STATUS_B);
	return (status_b & RTC_STATUS_B_24H) ? 1 : 0;
}

static void
rtc_interrupt_handler(registers_t *regs __unused)
{
	/* Read Status Register C to clear interrupt flags */
	rtc_read_register(RTC_REG_STATUS_C);

	/* Call user handler if registered */
	if (rtc_periodic_handler != NULL) {
		rtc_periodic_handler();
	}

	/* Send EOI to PIC */
	pic_send_eoi(IRQ_RTC);
}

void
rtc_init(void)
{
	uint8_t status_b;

	/* Read Status Register B */
	status_b = rtc_read_register(RTC_REG_STATUS_B);

	/* Ensure RTC is in a known state:
	 * - 24-hour mode
	 * - Binary mode (easier to work with)
	 * - No interrupts enabled initially
	 */
	status_b |= RTC_STATUS_B_24H; /* 24-hour format */
	status_b |= RTC_STATUS_B_DM;  /* Binary mode */
	status_b &= ~(RTC_STATUS_B_PIE | RTC_STATUS_B_AIE | RTC_STATUS_B_UIE);

	rtc_write_register(RTC_REG_STATUS_B, status_b);

	/* Clear any pending interrupts */
	rtc_read_register(RTC_REG_STATUS_C);

	/* Register IRQ handler but don't enable interrupts yet */
	isr_register_handler(32 + IRQ_RTC, rtc_interrupt_handler);
}

int
rtc_get_time(rtc_time_t *time)
{
	uint8_t second, minute, hour, day, month, year, century;
	uint8_t last_second, last_minute, last_hour, last_day, last_month,
	    last_year;
	int is_binary, is_24h;
	int attempts = 0;

	if (time == NULL)
		return -1;

	is_binary = rtc_is_binary_mode();
	is_24h = rtc_is_24hour_mode();

	/*
	 * Read registers multiple times until we get consistent values.
	 * This handles the case where values change during our read.
	 */
	do {
		/* Wait for update cycle to complete */
		rtc_wait_for_update();

		/* Read all time/date registers */
		second = rtc_read_register(RTC_REG_SECONDS);
		minute = rtc_read_register(RTC_REG_MINUTES);
		hour = rtc_read_register(RTC_REG_HOURS);
		day = rtc_read_register(RTC_REG_DAY);
		month = rtc_read_register(RTC_REG_MONTH);
		year = rtc_read_register(RTC_REG_YEAR);
		century = rtc_read_register(rtc_century_reg);

		/* Wait and read again to verify consistency */
		rtc_wait_for_update();

		last_second = rtc_read_register(RTC_REG_SECONDS);
		last_minute = rtc_read_register(RTC_REG_MINUTES);
		last_hour = rtc_read_register(RTC_REG_HOURS);
		last_day = rtc_read_register(RTC_REG_DAY);
		last_month = rtc_read_register(RTC_REG_MONTH);
		last_year = rtc_read_register(RTC_REG_YEAR);

		/* Check if values are consistent */
		if (second == last_second && minute == last_minute &&
		    hour == last_hour && day == last_day &&
		    month == last_month && year == last_year) {
			break;
		}

		if (++attempts > 100) {
			/* Prevent infinite loop */
			return -1;
		}
	} while (1);

	/* Convert from BCD to binary if necessary */
	if (!is_binary) {
		second = bcd_to_binary(second);
		minute = bcd_to_binary(minute);
		hour = bcd_to_binary(hour);
		day = bcd_to_binary(day);
		month = bcd_to_binary(month);
		year = bcd_to_binary(year);
		century = bcd_to_binary(century);
	}

	/* Handle 12-hour mode conversion */
	if (!is_24h && (hour & 0x80)) {
		hour = ((hour & 0x7F) + 12) % 24;
	}

	/* Fill in the time structure */
	time->second = second;
	time->minute = minute;
	time->hour = hour;
	time->day = day;
	time->month = month;
	time->year = (century * 100) + year;
	time->weekday = rtc_read_register(RTC_REG_WEEKDAY);

	return 0;
}

int
rtc_set_time(const rtc_time_t *time)
{
	uint8_t status_b;
	uint8_t second, minute, hour, day, month, year, century;
	int is_binary;

	if (time == NULL)
		return -1;

	/* Basic validation */
	if (time->second > 59 || time->minute > 59 || time->hour > 23 ||
	    time->day == 0 || time->day > 31 || time->month == 0 ||
	    time->month > 12 || time->year < 1970 || time->year > 2099) {
		return -1;
	}

	is_binary = rtc_is_binary_mode();

	/* Prepare values */
	second = time->second;
	minute = time->minute;
	hour = time->hour;
	day = time->day;
	month = time->month;
	year = time->year % 100;
	century = time->year / 100;

	/* Convert to BCD if necessary */
	if (!is_binary) {
		second = binary_to_bcd(second);
		minute = binary_to_bcd(minute);
		hour = binary_to_bcd(hour);
		day = binary_to_bcd(day);
		month = binary_to_bcd(month);
		year = binary_to_bcd(year);
		century = binary_to_bcd(century);
	}

	/* Disable updates while we write */
	status_b = rtc_read_register(RTC_REG_STATUS_B);
	rtc_write_register(RTC_REG_STATUS_B, status_b | RTC_STATUS_B_SET);

	/* Write all registers */
	rtc_write_register(RTC_REG_SECONDS, second);
	rtc_write_register(RTC_REG_MINUTES, minute);
	rtc_write_register(RTC_REG_HOURS, hour);
	rtc_write_register(RTC_REG_DAY, day);
	rtc_write_register(RTC_REG_MONTH, month);
	rtc_write_register(RTC_REG_YEAR, year);
	rtc_write_register(rtc_century_reg, century);

	/* Re-enable updates */
	rtc_write_register(RTC_REG_STATUS_B, status_b);

	return 0;
}

int
rtc_enable_periodic_interrupt(uint8_t rate, rtc_handler_t handler)
{
	uint8_t status_a, status_b;

	if (rate > 0x0F)
		return -1;

	/* Store handler */
	rtc_periodic_handler = handler;

	/* Set the rate */
	status_a = rtc_read_register(RTC_REG_STATUS_A);
	status_a = (status_a & 0xF0) | rate;
	rtc_write_register(RTC_REG_STATUS_A, status_a);

	/* Enable periodic interrupt */
	status_b = rtc_read_register(RTC_REG_STATUS_B);
	status_b |= RTC_STATUS_B_PIE;
	rtc_write_register(RTC_REG_STATUS_B, status_b);

	/* Clear any pending interrupts */
	rtc_read_register(RTC_REG_STATUS_C);

	/* Unmask IRQ 8 in PIC */
	pic_clear_mask(IRQ_RTC);

	return 0;
}

void
rtc_disable_periodic_interrupt(void)
{
	uint8_t status_b;

	/* Mask IRQ 8 in PIC */
	pic_set_mask(IRQ_RTC);

	/* Disable periodic interrupt in RTC */
	status_b = rtc_read_register(RTC_REG_STATUS_B);
	status_b &= ~RTC_STATUS_B_PIE;
	rtc_write_register(RTC_REG_STATUS_B, status_b);

	/* Clear handler */
	rtc_periodic_handler = NULL;

	/* Clear any pending interrupts */
	rtc_read_register(RTC_REG_STATUS_C);
}

static int
is_leap_year(uint16_t year)
{
	return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static uint8_t
days_in_month(uint8_t month, uint16_t year)
{
	static const uint8_t days[] = { 31, 28, 31, 30, 31, 30,
		                        31, 31, 30, 31, 30, 31 };

	if (month < 1 || month > 12)
		return 0;

	if (month == 2 && is_leap_year(year))
		return 29;

	return days[month - 1];
}

time_t
rtc_get_timestamp(void)
{
	return rtc_get_timestamp_tz(current_timezone);
}

static int
is_dst_europe(rtc_time_t *time)
{
	/* Last Sunday in March to last Sunday in October, 2:00 AM */
	if (time->month < 3 || time->month > 10)
		return 0;
	if (time->month > 3 && time->month < 10)
		return 1;

	/* Need to check last Sunday - simplified for now */
	/* For production, calculate actual last Sunday */
	return (time->month == 3) ? (time->day >= 25) : (time->day < 25);
}

static int
is_dst_us(rtc_time_t *time)
{
	/* Second Sunday in March to first Sunday in November, 2:00 AM */
	if (time->month < 3 || time->month > 11)
		return 0;
	if (time->month > 3 && time->month < 11)
		return 1;

	/* Simplified - for production, calculate actual Sundays */
	return (time->month == 3) ? (time->day >= 8) : (time->day < 7);
}

static int
is_dst_australia(rtc_time_t *time)
{
	/* First Sunday in October to first Sunday in April */
	if (time->month < 4 || time->month > 9)
		return 1;
	if (time->month > 4 && time->month < 10)
		return 0;

	return (time->month == 10) ? (time->day >= 1) : (time->day < 7);
}

static int
needs_dst(timezone_t tz, rtc_time_t *time)
{
	if (!timezone_table[tz].has_dst)
		return 0;

	switch (tz) {
	case TZ_CET:
		return is_dst_europe(time);
	case TZ_EST:
	case TZ_PST:
		return is_dst_us(time);
	case TZ_AEST:
		return is_dst_australia(time);
	default:
		return 0;
	}
}

static int
get_timezone_offset(timezone_t tz, rtc_time_t *time)
{
	if (needs_dst(tz, time))
		return timezone_table[tz].dst_offset_seconds;
	return timezone_table[tz].offset_seconds;
}

void
rtc_set_timezone(timezone_t tz)
{
	if (tz < sizeof(timezone_table) / sizeof(timezone_table[0]))
		current_timezone = tz;
}

timezone_t
rtc_get_timezone(void)
{
	return current_timezone;
}

time_t
rtc_get_timestamp_tz(timezone_t tz)
{
	rtc_time_t time;
	time_t timestamp;
	uint16_t year;
	uint8_t month;

	if (rtc_get_time(&time) != 0)
		return 0;

	/* Calculate days since epoch (1970-01-01) */
	timestamp = 0;

	for (year = 1970; year < time.year; year++) {
		timestamp += is_leap_year(year) ? 366 : 365;
	}

	for (month = 1; month < time.month; month++) {
		timestamp += days_in_month(month, time.year);
	}

	timestamp += time.day - 1;

	timestamp = timestamp * 86400 + time.hour * 3600 + time.minute * 60 +
	            time.second;

	/* Apply timezone offset */
	timestamp += get_timezone_offset(tz, &time);

	return timestamp;
}

int
rtc_get_time_tz(rtc_time_t *time, timezone_t tz)
{
	time_t timestamp;
	int offset;

	/* Get UTC time first */
	if (rtc_get_time(time) != 0)
		return -1;

	/* Get timestamp and apply offset */
	timestamp = rtc_get_timestamp_tz(TZ_UTC);
	offset = get_timezone_offset(tz, time);
	timestamp += offset;

	time->hour = (time->hour + (offset / 3600)) % 24;

	return 0;
}