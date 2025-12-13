#include <tty.h>

void
tty_clear(void)
{
	tty_t *tty = tty_get();

	if (!tty->buffer)
		return;

	tty_fill_rect(0, 0, tty->width, tty->height, tty->bg_color);

	tty->cursor_x = 0;
	tty->cursor_y = 0;

	if (tty->cursor_visible) {
		tty_draw_cursor();
	}
}

void
tty_scroll(void)
{
	tty_t *tty = tty_get();

	if (!tty->buffer)
		return;

	uint64_t line_height = tty->char_height;
	uint64_t pixels_per_row = tty->pitch / 4;
	uint64_t scroll_lines = tty->height - line_height;

	for (uint64_t y = 0; y < scroll_lines; y++) {
		uint32_t *dst = &tty->buffer[y * pixels_per_row];
		uint32_t *src =
		    &tty->buffer[(y + line_height) * pixels_per_row];

		for (uint64_t x = 0; x < tty->width; x++) {
			dst[x] = src[x];
		}
	}

	uint64_t last_line_y = (tty->rows - 1) * tty->char_height;
	tty_fill_rect(0, last_line_y, tty->width, line_height, tty->bg_color);
}

void
tty_clear_to_end(void)
{
	tty_t *tty = tty_get();

	uint64_t px = tty->cursor_x * tty->char_width;
	uint64_t py = tty->cursor_y * tty->char_height;
	tty_fill_rect(px, py, tty->width - px, tty->char_height, tty->bg_color);

	// Clear all lines below current line
	if (tty->cursor_y < tty->rows - 1) {
		uint64_t next_line_y = (tty->cursor_y + 1) * tty->char_height;
		tty_fill_rect(0,
		              next_line_y,
		              tty->width,
		              tty->height - next_line_y,
		              tty->bg_color);
	}
}

void
tty_clear_to_cursor(void)
{
	tty_t *tty = tty_get();

	if (tty->cursor_y > 0) {
		uint64_t height = tty->cursor_y * tty->char_height;
		tty_fill_rect(0, 0, tty->width, height, tty->bg_color);
	}

	// Clear from start of current line to cursor
	uint64_t py = tty->cursor_y * tty->char_height;
	uint64_t px = (tty->cursor_x + 1) * tty->char_width;
	tty_fill_rect(0, py, px, tty->char_height, tty->bg_color);
}