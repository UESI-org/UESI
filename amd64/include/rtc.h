#ifndef RTC_H_
#define RTC_H_

#include <cdefs.h>
#include <types.h>

__BEGIN_DECLS

#define RTC_ADDR_PORT       0x70    /* CMOS address/NMI control port */
#define RTC_DATA_PORT       0x71    /* CMOS data port */

#define RTC_REG_SECONDS     0x00    /* Current seconds (0-59) */
#define RTC_REG_MINUTES     0x02    /* Current minutes (0-59) */
#define RTC_REG_HOURS       0x04    /* Current hours (0-23 or 1-12) */
#define RTC_REG_WEEKDAY     0x06    /* Day of week (1-7, Sunday=1) */
#define RTC_REG_DAY         0x07    /* Day of month (1-31) */
#define RTC_REG_MONTH       0x08    /* Month (1-12) */
#define RTC_REG_YEAR        0x09    /* Year (0-99, relative to century) */
#define RTC_REG_CENTURY     0x32    /* Century (BCD, often 19 or 20) */

#define RTC_REG_STATUS_A    0x0A    /* Status Register A */
#define RTC_REG_STATUS_B    0x0B    /* Status Register B */
#define RTC_REG_STATUS_C    0x0C    /* Status Register C (flags) */
#define RTC_REG_STATUS_D    0x0D    /* Status Register D */

#define RTC_STATUS_A_UIP    0x80    /* Update In Progress flag */
#define RTC_STATUS_A_DV_MASK 0x70   /* Divider select bits */
#define RTC_STATUS_A_RS_MASK 0x0F   /* Rate select bits */

#define RTC_STATUS_B_SET    0x80    /* SET bit (inhibit updates) */
#define RTC_STATUS_B_PIE    0x40    /* Periodic Interrupt Enable */
#define RTC_STATUS_B_AIE    0x20    /* Alarm Interrupt Enable */
#define RTC_STATUS_B_UIE    0x10    /* Update-ended Interrupt Enable */
#define RTC_STATUS_B_SQWE   0x08    /* Square Wave Enable */
#define RTC_STATUS_B_DM     0x04    /* Data Mode (0=BCD, 1=Binary) */
#define RTC_STATUS_B_24H    0x02    /* Hour Format (0=12h, 1=24h) */
#define RTC_STATUS_B_DSE    0x01    /* Daylight Savings Enable */

#define RTC_STATUS_C_IRQF   0x80    /* Interrupt Request Flag */
#define RTC_STATUS_C_PF     0x40    /* Periodic Interrupt Flag */
#define RTC_STATUS_C_AF     0x20    /* Alarm Interrupt Flag */
#define RTC_STATUS_C_UF     0x10    /* Update-ended Interrupt Flag */

#define RTC_NMI_DISABLE     0x80    /* Set bit 7 to disable NMI */

#define RTC_RATE_NONE       0x00    /* No periodic interrupt */
#define RTC_RATE_8192HZ     0x03    /* 8192 Hz */
#define RTC_RATE_4096HZ     0x04    /* 4096 Hz */
#define RTC_RATE_2048HZ     0x05    /* 2048 Hz */
#define RTC_RATE_1024HZ     0x06    /* 1024 Hz */
#define RTC_RATE_512HZ      0x07    /* 512 Hz */
#define RTC_RATE_256HZ      0x08    /* 256 Hz */
#define RTC_RATE_128HZ      0x09    /* 128 Hz */
#define RTC_RATE_64HZ       0x0A    /* 64 Hz */
#define RTC_RATE_32HZ       0x0B    /* 32 Hz */
#define RTC_RATE_16HZ       0x0C    /* 16 Hz */
#define RTC_RATE_8HZ        0x0D    /* 8 Hz */
#define RTC_RATE_4HZ        0x0E    /* 4 Hz */
#define RTC_RATE_2HZ        0x0F    /* 2 Hz (default) */

typedef struct rtc_time {
    uint8_t  second;        /* 0-59 */
    uint8_t  minute;        /* 0-59 */
    uint8_t  hour;          /* 0-23 */
    uint8_t  day;           /* 1-31 */
    uint8_t  month;         /* 1-12 */
    uint16_t year;          /* Full year (e.g., 2024) */
    uint8_t  weekday;       /* 1-7 (Sunday=1) */
} rtc_time_t;

typedef enum {
    TZ_UTC = 0,
    TZ_CET,          /* Central European Time (UTC+1/+2) */
    TZ_EST,          /* Eastern Standard Time (UTC-5/-4) */
    TZ_PST,          /* Pacific Standard Time (UTC-8/-7) */
    TZ_JST,          /* Japan Standard Time (UTC+9) */
    TZ_AEST,         /* Australian Eastern Time (UTC+10/+11) */
} timezone_t;

typedef struct {
    const char *name;
    int offset_seconds;      /* Standard time offset from UTC */
    int dst_offset_seconds;  /* DST offset from UTC (0 if no DST) */
    int has_dst;
} timezone_info_t;

typedef void (*rtc_handler_t)(void);
void rtc_init(void);
int rtc_get_time(rtc_time_t *time);
int rtc_set_time(const rtc_time_t *time);
int rtc_enable_periodic_interrupt(uint8_t rate, rtc_handler_t handler);
void rtc_disable_periodic_interrupt(void);
uint8_t rtc_read_register(uint8_t reg);
void rtc_write_register(uint8_t reg, uint8_t value);
int rtc_is_binary_mode(void);
int rtc_is_24hour_mode(void);
time_t rtc_get_timestamp(void);

void rtc_set_timezone(timezone_t tz);
timezone_t rtc_get_timezone(void);
time_t rtc_get_timestamp_tz(timezone_t tz);
int rtc_get_time_tz(rtc_time_t *time, timezone_t tz);

__END_DECLS

#endif