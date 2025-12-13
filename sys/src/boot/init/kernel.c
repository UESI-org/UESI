#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <tty.h>
#include <tmpfs.h>
#include <gdt.h>
#include <isr.h>
#include <idt.h>
#include <pic.h>
#include <pci.h>
#include <serial_debug.h>
#include <scheduler.h>
#include <syscall.h>
#include <keyboard.h>
#include <kdebug.h>
#include <cpuid.h>
#include <pit.h>
#include <pmm.h>
#include <vmm.h>
#include <vfs.h>
#include <inode.h>
#include <blk.h>
#include <blkalloc.h>
#include <mmu.h>
#include <printf.h>
#include <proc.h>
#include <kmalloc.h>
#include <rtc.h>
#include <boot.h>
#include <tests.h>
#include <system_info.h>
#include <panic.h>
#include <serial.h>
#include <sys/spinlock.h>
#include <sys/sysinfo.h>
#include <sys/ahci.h>

spinlock_t my_lock;
static blk_allocator_t g_block_alloc;

void init_subsystem(void) {
    spinlock_init(&my_lock, "my_subsystem");
}

static void initialize_memory(void) {
    debug_section("Initializing Memory Management");
    
    struct limine_memmap_response *memmap = boot_get_memmap();
    struct limine_hhdm_response *hhdm = boot_get_hhdm();
    
    if (memmap == NULL || hhdm == NULL) {
        panic("Memory map or HHDM not available from bootloader");
    }

    pmm_init(memmap, hhdm);
    debug_success("PMM initialized");

    kmalloc_init();
    debug_success("kmalloc initialized");

    mmu_init(hhdm);
    debug_success("MMU initialized");

    vmm_init();
    debug_success("VMM initialized");
    
    vfs_init();
    debug_success("VFS initialized");
    
    inode_cache_init();
    debug_success("inode cache initialized");

    blk_alloc_init(&g_block_alloc, 4096, 0, BLK_SIZE_4K);
    debug_success("blk alloc initialized");

    blk_buffer_init();
    debug_success("blk buffer initialized");
    
    tmpfs_init();
    debug_success("tmpfs initialized");
    
    int ret = vfs_mount(NULL, "/", "tmpfs", 0, NULL);
    if (ret != 0) {
        printf("vfs_mount returned error code: %d\n", ret);
        debug_error("Failed to mount tmpfs at /");
    } else {
        debug_success("tmpfs mounted at / (root)");
    }
}

static void initialize_cpu(cpu_info_t *cpu) {
    debug_section("Detecting CPU");
    cpuid_init(cpu);

    if (!cpu->cpuid_supported) {
        panic("CPUID instruction not supported - this CPU is too old");
    }

    bool all_features_present = true;
    
    if (!cpuid_has_long_mode()) {
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: 64-bit Long Mode not supported\n");
        all_features_present = false;
    }
    
    if (!cpuid_has_pae()) {
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: PAE not supported\n");
        all_features_present = false;
    }
    
    if (!cpuid_has_nx()) {
        tty_set_color(TTY_COLOR_RED, TTY_COLOR_BLACK);
        printf("FATAL: NX bit not supported\n");
        all_features_present = false;
    }

    if (!all_features_present) {
        tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
        panic("Required CPU features missing (64-bit, PAE, NX)");
    }

    debug_success("CPU detection complete");
    
    if (debug_is_enabled()) {
        system_print_cpu_info(cpu);
    }
}

static void initialize_system_components(void) {
    debug_section("Initializing System Components");
    
    gdt_init();
    debug_success("GDT initialized");

    idt_init();
    isr_install();
    debug_success("IDT initialized");

    kdebug_init();
    isr_register_handler(3, kdebug_breakpoint_handler);  // INT3 for breakpoints
    debug_success("Kernel debugger initialized");

    init_subsystem();

    syscall_init();
    sysinfo_init();

    pic_init();
    debug_success("PIC initialized");

    rtc_init();
    debug_success("RTC initialized");

    pit_init(100);
    isr_register_handler(32, (isr_handler_t)pit_handler);
    debug_success("PIT initialized (100 Hz)");

    scheduler_init(100);
    debug_success("Scheduler initialized");

    keyboard_init();
    pic_clear_mask(IRQ_KEYBOARD);
    debug_success("Keyboard initialized");
}

static void run_tests(void) {
    test_kmalloc();
    test_ahci();
    test_rtc();
    
    struct limine_module_response *modules = boot_get_modules();
    if (modules && modules->module_count > 0) {
        struct limine_file *user_prog = modules->modules[0];
        debug_section("Loading Userland Program");
        printf("Module: %s (%lu bytes)\n", 
               user_prog->path, (unsigned long)user_prog->size);
        userland_load_and_run(user_prog->address, user_prog->size, "test");
    } else {
        printf("No userland modules found\n");
    }
}

void kmain(void) {
    debug_init();

    if (!boot_verify_protocol()) {
        panic("Limine base revision mismatch - incompatible bootloader version");
    }
    debug_success("Limine protocol verified");

    boot_init();

    struct limine_framebuffer *framebuffer = boot_get_framebuffer();
    if (framebuffer == NULL) {
        panic("No framebuffer available from bootloader");
    }

    debug_framebuffer_info(framebuffer->width, framebuffer->height,
                          framebuffer->pitch, framebuffer->bpp);

    tty_init(framebuffer);
    tty_set_color(TTY_COLOR_WHITE, TTY_COLOR_BLACK);
    debug_success("TTY initialized");

    if (!serial_init(SERIAL_COM1)) {
        debug_error("Serial port initialization failed (non-fatal)");
    } else {
        debug_success("Serial port initialized");
    }

    initialize_memory();

    cpu_info_t cpu;
    initialize_cpu(&cpu);

    debug_section("Initializing PCI");
    pci_init();

    debug_section("Initializing AHCI");
    if (ahci_init() == 0) {
        debug_success("AHCI initialized");
    } else {
        debug_error("No AHCI controllers found (non-fatal)");
    }

    initialize_system_components();

    proc_init();
    debug_success("Process subsystem initialized");

    run_tests();

    debug_banner("Kernel Initialization Complete");

    system_print_banner(&cpu);

    __asm__ volatile("sti");

    if (debug_is_enabled()) {
        serial_printf(DEBUG_PORT, "\n=== System Ready ===\n");
        serial_printf(DEBUG_PORT, "Interrupts enabled\n");
        serial_printf(DEBUG_PORT, "Awaiting input...\n");
    }

    for (;;) {
        __asm__ volatile("hlt");
    }
}