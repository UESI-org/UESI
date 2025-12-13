#include <kdebug.h>
#include <serial.h>
#include <printf.h>
#include <stdarg.h>

static int debug_level = KDEBUG_LEVEL_INFO;
static bool serial_enabled = true;
static uint16_t debug_port = SERIAL_COM1;

#define MAX_BREAKPOINTS 16
static kdebug_breakpoint_t breakpoints[MAX_BREAKPOINTS] = { 0 };

#define MAX_SYMBOLS 128
static struct {
	uint64_t address;
	const char *name;
} symbols[MAX_SYMBOLS] = { 0 };
static size_t symbol_count = 0;

static const char *level_names[] = { "NONE", "ERROR", "WARN",
	                             "INFO", "DEBUG", "TRACE" };

static const char *level_prefixes[] = { "",         "[ERROR] ", "[WARN]  ",
	                                "[INFO]  ", "[DEBUG] ", "[TRACE] " };

void
kdebug_init(void)
{
	serial_enabled = serial_init(debug_port);
	if (serial_enabled) {
		serial_printf(debug_port,
		              "\n=== Kernel Debugger Initialized ===\n");
		serial_printf(
		    debug_port, "Debug level: %s\n", level_names[debug_level]);
	}
}

void
kdebug_set_level(int level)
{
	if (level >= KDEBUG_LEVEL_NONE && level <= KDEBUG_LEVEL_TRACE) {
		debug_level = level;
		if (serial_enabled) {
			serial_printf(debug_port,
			              "Debug level set to: %s\n",
			              level_names[level]);
		}
	}
}

int
kdebug_get_level(void)
{
	return debug_level;
}

void
kdebug_enable_serial(bool enable)
{
	serial_enabled = enable;
}

void
kdebug_log(int level,
           const char *file,
           int line,
           const char *func,
           const char *fmt,
           ...)
{
	if (level > debug_level) {
		return;
	}

	va_list args;

	if (serial_enabled) {
		serial_printf(debug_port,
		              "%s[%s:%d:%s] ",
		              level_prefixes[level],
		              file,
		              line,
		              func);

		va_start(args, fmt);
		// Use serial_printf's va_list version if available,
		// otherwise format separately
		char buffer[256];
		vsnprintf_(buffer, sizeof(buffer), fmt, args);
		serial_printf(debug_port, "%s\n", buffer);
		va_end(args);
	}
}

void
kdebug_dump_registers(registers_t *regs)
{
	if (!regs)
		return;

	printf("\n=== Register Dump ===\n");
	printf("RAX: 0x%016lx  RBX: 0x%016lx\n", regs->rax, regs->rbx);
	printf("RCX: 0x%016lx  RDX: 0x%016lx\n", regs->rcx, regs->rdx);
	printf("RSI: 0x%016lx  RDI: 0x%016lx\n", regs->rsi, regs->rdi);
	printf("RBP: 0x%016lx  RSP: 0x%016lx\n", regs->rbp, regs->rsp);
	printf("R8:  0x%016lx  R9:  0x%016lx\n", regs->r8, regs->r9);
	printf("R10: 0x%016lx  R11: 0x%016lx\n", regs->r10, regs->r11);
	printf("R12: 0x%016lx  R13: 0x%016lx\n", regs->r12, regs->r13);
	printf("R14: 0x%016lx  R15: 0x%016lx\n", regs->r14, regs->r15);
	printf("\nRIP: 0x%016lx  RFLAGS: 0x%016lx\n", regs->rip, regs->rflags);
	printf("CS:  0x%04lx           SS:     0x%04lx\n", regs->cs, regs->ss);
	printf("INT: %lu              ERR:    0x%lx\n",
	       regs->int_no,
	       regs->err_code);

	if (serial_enabled) {
		serial_printf(debug_port, "\n=== Register Dump ===\n");
		serial_printf(debug_port,
		              "RAX: 0x%016lx  RBX: 0x%016lx\n",
		              regs->rax,
		              regs->rbx);
		serial_printf(debug_port,
		              "RCX: 0x%016lx  RDX: 0x%016lx\n",
		              regs->rcx,
		              regs->rdx);
		serial_printf(debug_port,
		              "RSI: 0x%016lx  RDI: 0x%016lx\n",
		              regs->rsi,
		              regs->rdi);
		serial_printf(debug_port,
		              "RBP: 0x%016lx  RSP: 0x%016lx\n",
		              regs->rbp,
		              regs->rsp);
		serial_printf(debug_port,
		              "R8:  0x%016lx  R9:  0x%016lx\n",
		              regs->r8,
		              regs->r9);
		serial_printf(debug_port,
		              "R10: 0x%016lx  R11: 0x%016lx\n",
		              regs->r10,
		              regs->r11);
		serial_printf(debug_port,
		              "R12: 0x%016lx  R13: 0x%016lx\n",
		              regs->r12,
		              regs->r13);
		serial_printf(debug_port,
		              "R14: 0x%016lx  R15: 0x%016lx\n",
		              regs->r14,
		              regs->r15);
		serial_printf(debug_port,
		              "\nRIP: 0x%016lx  RFLAGS: 0x%016lx\n",
		              regs->rip,
		              regs->rflags);
		serial_printf(debug_port,
		              "CS:  0x%04lx           SS:     0x%04lx\n",
		              regs->cs,
		              regs->ss);
		serial_printf(debug_port,
		              "INT: %lu              ERR:    0x%lx\n",
		              regs->int_no,
		              regs->err_code);
	}
}

void
kdebug_dump_control_registers(void)
{
	uint64_t cr0, cr2, cr3, cr4;

	__asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
	__asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
	__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
	__asm__ volatile("mov %%cr4, %0" : "=r"(cr4));

	printf("\n=== Control Registers ===\n");
	printf("CR0: 0x%016lx  CR2: 0x%016lx\n", cr0, cr2);
	printf("CR3: 0x%016lx  CR4: 0x%016lx\n", cr3, cr4);

	if (serial_enabled) {
		serial_printf(debug_port, "\n=== Control Registers ===\n");
		serial_printf(
		    debug_port, "CR0: 0x%016lx  CR2: 0x%016lx\n", cr0, cr2);
		serial_printf(
		    debug_port, "CR3: 0x%016lx  CR4: 0x%016lx\n", cr3, cr4);
	}
}

void
kdebug_backtrace_from(uint64_t rbp, uint64_t rip)
{
	printf("\n=== Stack Trace ===\n");
	if (serial_enabled) {
		serial_printf(debug_port, "\n=== Stack Trace ===\n");
	}

	stack_frame_t *frame = (stack_frame_t *)rbp;
	int depth = 0;

	// Print current instruction pointer first
	kdebug_symbol_t sym = kdebug_lookup_symbol(rip);
	if (sym.name) {
		printf("#%d  0x%016lx in %s+0x%lx\n",
		       depth,
		       rip,
		       sym.name,
		       sym.offset);
		if (serial_enabled) {
			serial_printf(debug_port,
			              "#%d  0x%016lx in %s+0x%lx\n",
			              depth,
			              rip,
			              sym.name,
			              sym.offset);
		}
	} else {
		printf("#%d  0x%016lx\n", depth, rip);
		if (serial_enabled) {
			serial_printf(
			    debug_port, "#%d  0x%016lx\n", depth, rip);
		}
	}
	depth++;

	// Walk the stack
	while (frame && depth < KDEBUG_MAX_STACK_DEPTH) {
		// Basic validation: check if frame pointer looks reasonable
		if ((uint64_t)frame < 0x1000 || ((uint64_t)frame & 0x7)) {
			break; // Invalid frame pointer
		}

		uint64_t return_addr = frame->rip;
		if (return_addr == 0 || return_addr < 0x1000) {
			break; // Invalid return address
		}

		sym = kdebug_lookup_symbol(return_addr);
		if (sym.name) {
			printf("#%d  0x%016lx in %s+0x%lx\n",
			       depth,
			       return_addr,
			       sym.name,
			       sym.offset);
			if (serial_enabled) {
				serial_printf(debug_port,
				              "#%d  0x%016lx in %s+0x%lx\n",
				              depth,
				              return_addr,
				              sym.name,
				              sym.offset);
			}
		} else {
			printf("#%d  0x%016lx\n", depth, return_addr);
			if (serial_enabled) {
				serial_printf(debug_port,
				              "#%d  0x%016lx\n",
				              depth,
				              return_addr);
			}
		}

		frame = frame->rbp;
		depth++;
	}

	printf("===================\n");
	if (serial_enabled) {
		serial_printf(debug_port, "===================\n");
	}
}

void
kdebug_backtrace(void)
{
	uint64_t rbp, rip;

	// Get current RBP
	__asm__ volatile("mov %%rbp, %0" : "=r"(rbp));

	// Get return address from stack
	rip = *((uint64_t *)(rbp + 8));

	kdebug_backtrace_from(rbp, rip);
}

void
kdebug_hexdump(const void *data, size_t length)
{
	const uint8_t *bytes = (const uint8_t *)data;

	for (size_t i = 0; i < length; i += 16) {
		printf("%016lx: ", (uint64_t)data + i);
		if (serial_enabled) {
			serial_printf(
			    debug_port, "%016lx: ", (uint64_t)data + i);
		}

		// Print hex bytes
		for (size_t j = 0; j < 16; j++) {
			if (i + j < length) {
				printf("%02x ", bytes[i + j]);
				if (serial_enabled) {
					serial_printf(
					    debug_port, "%02x ", bytes[i + j]);
				}
			} else {
				printf("   ");
				if (serial_enabled) {
					serial_printf(debug_port, "   ");
				}
			}
			if (j == 7) {
				printf(" ");
				if (serial_enabled) {
					serial_printf(debug_port, " ");
				}
			}
		}

		printf(" |");
		if (serial_enabled) {
			serial_printf(debug_port, " |");
		}

		// Print ASCII
		for (size_t j = 0; j < 16 && i + j < length; j++) {
			uint8_t c = bytes[i + j];
			char display = (c >= 32 && c <= 126) ? c : '.';
			printf("%c", display);
			if (serial_enabled) {
				serial_printf(debug_port, "%c", display);
			}
		}

		printf("|\n");
		if (serial_enabled) {
			serial_printf(debug_port, "|\n");
		}
	}
}

void
kdebug_dump_memory(uint64_t address, size_t length)
{
	printf("\n=== Memory Dump at 0x%016lx ===\n", address);
	if (serial_enabled) {
		serial_printf(
		    debug_port, "\n=== Memory Dump at 0x%016lx ===\n", address);
	}

	if (kdebug_is_valid_address(address)) {
		kdebug_hexdump((void *)address, length);
	} else {
		printf("Invalid memory address!\n");
		if (serial_enabled) {
			serial_printf(debug_port, "Invalid memory address!\n");
		}
	}
}

void
kdebug_register_symbol(uint64_t address, const char *name)
{
	if (symbol_count < MAX_SYMBOLS) {
		symbols[symbol_count].address = address;
		symbols[symbol_count].name = name;
		symbol_count++;
	}
}

kdebug_symbol_t
kdebug_lookup_symbol(uint64_t address)
{
	kdebug_symbol_t result = { .address = 0, .name = NULL, .offset = 0 };
	uint64_t best_offset = UINT64_MAX;

	for (size_t i = 0; i < symbol_count; i++) {
		if (symbols[i].address <= address) {
			uint64_t offset = address - symbols[i].address;
			if (offset < best_offset) {
				best_offset = offset;
				result.address = symbols[i].address;
				result.name = symbols[i].name;
				result.offset = offset;
			}
		}
	}

	return result;
}

bool
kdebug_is_valid_address(uint64_t address)
{
	// Basic checks for valid kernel addresses
	// This is simplified - you might want to add page table checks
	if (address < 0x1000) {
		return false; // NULL and low memory
	}

	// Check if it's in kernel space (assuming higher half)
	if (address >= 0xFFFF800000000000UL) {
		return true;
	}

	// For user space, would need more validation
	return false;
}

void
kdebug_panic(const char *message, registers_t *regs)
{
	__asm__ volatile("cli"); // Disable interrupts

	printf("\n\n");
	printf("=====================================\n");
	printf("!!!   KERNEL PANIC   !!!\n");
	printf("=====================================\n");
	printf("%s\n", message);
	printf("=====================================\n");

	if (serial_enabled) {
		serial_printf(debug_port, "\n\n");
		serial_printf(debug_port,
		              "=====================================\n");
		serial_printf(debug_port, "!!!   KERNEL PANIC   !!!\n");
		serial_printf(debug_port,
		              "=====================================\n");
		serial_printf(debug_port, "%s\n", message);
		serial_printf(debug_port,
		              "=====================================\n");
	}

	if (regs) {
		kdebug_dump_registers(regs);
		kdebug_backtrace_from(regs->rbp, regs->rip);
		kdebug_dump_control_registers();
	} else {
		kdebug_backtrace();
	}

	printf("\nSystem halted.\n");
	if (serial_enabled) {
		serial_printf(debug_port, "\nSystem halted.\n");
	}

	// Halt forever
	for (;;) {
		__asm__ volatile("hlt");
	}
}

void
kdebug_assert_failed(const char *expr,
                     const char *file,
                     int line,
                     const char *func)
{
	printf("\n=== ASSERTION FAILED ===\n");
	printf("Expression: %s\n", expr);
	printf("File: %s:%d\n", file, line);
	printf("Function: %s\n", func);

	if (serial_enabled) {
		serial_printf(debug_port, "\n=== ASSERTION FAILED ===\n");
		serial_printf(debug_port, "Expression: %s\n", expr);
		serial_printf(debug_port, "File: %s:%d\n", file, line);
		serial_printf(debug_port, "Function: %s\n", func);
	}

	kdebug_panic("Assertion failed", NULL);
}

void
kdebug_assert_failed_msg(const char *expr,
                         const char *msg,
                         const char *file,
                         int line,
                         const char *func)
{
	printf("\n=== ASSERTION FAILED ===\n");
	printf("Expression: %s\n", expr);
	printf("Message: %s\n", msg);
	printf("File: %s:%d\n", file, line);
	printf("Function: %s\n", func);

	if (serial_enabled) {
		serial_printf(debug_port, "\n=== ASSERTION FAILED ===\n");
		serial_printf(debug_port, "Expression: %s\n", expr);
		serial_printf(debug_port, "Message: %s\n", msg);
		serial_printf(debug_port, "File: %s:%d\n", file, line);
		serial_printf(debug_port, "Function: %s\n", func);
	}

	kdebug_panic("Assertion failed", NULL);
}

bool
kdebug_set_breakpoint(uint64_t address, void (*callback)(registers_t *))
{
	// Find free slot
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (!breakpoints[i].enabled) {
			breakpoints[i].address = address;
			breakpoints[i].original_byte = *(uint8_t *)address;
			breakpoints[i].callback = callback;
			breakpoints[i].enabled = true;

			// Write INT3 instruction (0xCC)
			*(uint8_t *)address = 0xCC;

			if (serial_enabled) {
				serial_printf(debug_port,
				              "Breakpoint set at 0x%016lx\n",
				              address);
			}
			return true;
		}
	}
	return false; // No free slots
}

bool
kdebug_clear_breakpoint(uint64_t address)
{
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (breakpoints[i].enabled &&
		    breakpoints[i].address == address) {
			// Restore original byte
			*(uint8_t *)address = breakpoints[i].original_byte;
			breakpoints[i].enabled = false;

			if (serial_enabled) {
				serial_printf(
				    debug_port,
				    "Breakpoint cleared at 0x%016lx\n",
				    address);
			}
			return true;
		}
	}
	return false;
}

void
kdebug_list_breakpoints(void)
{
	printf("\n=== Active Breakpoints ===\n");
	if (serial_enabled) {
		serial_printf(debug_port, "\n=== Active Breakpoints ===\n");
	}

	bool found = false;
	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (breakpoints[i].enabled) {
			printf("%d: 0x%016lx\n", i, breakpoints[i].address);
			if (serial_enabled) {
				serial_printf(debug_port,
				              "%d: 0x%016lx\n",
				              i,
				              breakpoints[i].address);
			}
			found = true;
		}
	}

	if (!found) {
		printf("No active breakpoints\n");
		if (serial_enabled) {
			serial_printf(debug_port, "No active breakpoints\n");
		}
	}
}

void
kdebug_breakpoint_handler(registers_t *regs)
{
	// INT3 leaves RIP pointing to the instruction AFTER the breakpoint
	// We need to check the previous byte
	uint64_t bp_address = regs->rip - 1;

	for (int i = 0; i < MAX_BREAKPOINTS; i++) {
		if (breakpoints[i].enabled &&
		    breakpoints[i].address == bp_address) {
			printf("\n=== Breakpoint Hit ===\n");
			printf("Address: 0x%016lx\n", bp_address);

			if (serial_enabled) {
				serial_printf(debug_port,
				              "\n=== Breakpoint Hit ===\n");
				serial_printf(debug_port,
				              "Address: 0x%016lx\n",
				              bp_address);
			}

			kdebug_dump_registers(regs);

			if (breakpoints[i].callback) {
				breakpoints[i].callback(regs);
			}

			// Fix RIP to point back to the breakpoint
			regs->rip = bp_address;
			return;
		}
	}

	// Unknown breakpoint
	printf("\n=== Unexpected Breakpoint ===\n");
	printf("Address: 0x%016lx\n", bp_address);
	if (serial_enabled) {
		serial_printf(debug_port, "\n=== Unexpected Breakpoint ===\n");
		serial_printf(debug_port, "Address: 0x%016lx\n", bp_address);
	}
	kdebug_dump_registers(regs);
}

void
kdebug_dump_page_info(uint64_t virtual_address)
{
	uint64_t cr3;
	__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

	printf("\n=== Page Info for 0x%016lx ===\n", virtual_address);
	printf("CR3 (Page Directory Base): 0x%016lx\n", cr3);

	if (serial_enabled) {
		serial_printf(debug_port,
		              "\n=== Page Info for 0x%016lx ===\n",
		              virtual_address);
		serial_printf(
		    debug_port, "CR3 (Page Directory Base): 0x%016lx\n", cr3);
	}

	// Extract page table indices
	uint64_t pml4_idx = (virtual_address >> 39) & 0x1FF;
	uint64_t pdpt_idx = (virtual_address >> 30) & 0x1FF;
	uint64_t pd_idx = (virtual_address >> 21) & 0x1FF;
	uint64_t pt_idx = (virtual_address >> 12) & 0x1FF;
	uint64_t offset = virtual_address & 0xFFF;

	printf("PML4 Index: 0x%lx\n", pml4_idx);
	printf("PDPT Index: 0x%lx\n", pdpt_idx);
	printf("PD Index:   0x%lx\n", pd_idx);
	printf("PT Index:   0x%lx\n", pt_idx);
	printf("Offset:     0x%lx\n", offset);

	if (serial_enabled) {
		serial_printf(debug_port, "PML4 Index: 0x%lx\n", pml4_idx);
		serial_printf(debug_port, "PDPT Index: 0x%lx\n", pdpt_idx);
		serial_printf(debug_port, "PD Index:   0x%lx\n", pd_idx);
		serial_printf(debug_port, "PT Index:   0x%lx\n", pt_idx);
		serial_printf(debug_port, "Offset:     0x%lx\n", offset);
	}
}

void
kdebug_dump_segments(void)
{
	uint16_t cs, ds, es, fs, gs, ss;

	__asm__ volatile("mov %%cs, %0" : "=r"(cs));
	__asm__ volatile("mov %%ds, %0" : "=r"(ds));
	__asm__ volatile("mov %%es, %0" : "=r"(es));
	__asm__ volatile("mov %%fs, %0" : "=r"(fs));
	__asm__ volatile("mov %%gs, %0" : "=r"(gs));
	__asm__ volatile("mov %%ss, %0" : "=r"(ss));

	printf("\n=== Segment Registers ===\n");
	printf("CS: 0x%04x  DS: 0x%04x  ES: 0x%04x\n", cs, ds, es);
	printf("FS: 0x%04x  GS: 0x%04x  SS: 0x%04x\n", fs, gs, ss);

	if (serial_enabled) {
		serial_printf(debug_port, "\n=== Segment Registers ===\n");
		serial_printf(debug_port,
		              "CS: 0x%04x  DS: 0x%04x  ES: 0x%04x\n",
		              cs,
		              ds,
		              es);
		serial_printf(debug_port,
		              "FS: 0x%04x  GS: 0x%04x  SS: 0x%04x\n",
		              fs,
		              gs,
		              ss);
	}
}