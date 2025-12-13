#include <serial_debug.h>
#include <serial.h>

static bool debug_enabled = false;

bool
debug_init(void)
{
	debug_enabled = serial_init(DEBUG_PORT);

	if (debug_enabled) {
		debug_banner("UESI Kernel Boot");
		debug_print("Serial debug initialized (COM1, 115200 baud)\n");
	}

	return debug_enabled;
}

bool
debug_is_enabled(void)
{
	return debug_enabled;
}

void
debug_print(const char *str)
{
	if (debug_enabled) {
		serial_write_string(DEBUG_PORT, str);
	}
}

void
debug_banner(const char *title)
{
	if (debug_enabled) {
		serial_printf(DEBUG_PORT, "\n=== %s ===\n", title);
	}
}

void
debug_section(const char *section)
{
	if (debug_enabled) {
		serial_printf(DEBUG_PORT, "[INFO] %s\n", section);
	}
}

void
debug_error(const char *error)
{
	if (debug_enabled) {
		serial_printf(DEBUG_PORT, "[ERROR] %s\n", error);
	}
}

void
debug_success(const char *message)
{
	if (debug_enabled) {
		serial_printf(DEBUG_PORT, "[OK] %s\n", message);
	}
}

void
debug_framebuffer_info(uint32_t width,
                       uint32_t height,
                       uint32_t pitch,
                       uint32_t bpp)
{
	if (debug_enabled) {
		serial_printf(DEBUG_PORT,
		              "Framebuffer: %ux%u, pitch=%u, bpp=%u\n",
		              width,
		              height,
		              pitch,
		              bpp);
	}
}