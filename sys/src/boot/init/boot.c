#include <boot.h>
#include <stddef.h>

__attribute__((
    used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(3);

__attribute__((
    used,
    section(
        ".limine_requests"))) static volatile struct limine_framebuffer_request
    framebuffer_request = { .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0 };

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_module_request
    module_request = { .id = LIMINE_MODULE_REQUEST, .revision = 0 };

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_memmap_request
    memmap_request = { .id = LIMINE_MEMMAP_REQUEST, .revision = 0 };

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_hhdm_request
    hhdm_request = { .id = LIMINE_HHDM_REQUEST, .revision = 0 };

__attribute__((used,
               section(".limine_requests_"
                       "start"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((
    used,
    section(
        ".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER;

void
boot_init(void)
{
	/* Nothing to do here yet, requests are handled by Limine */
}

bool
boot_verify_protocol(void)
{
	return limine_base_revision[2] == 0;
}

struct limine_framebuffer *
boot_get_framebuffer(void)
{
	if (framebuffer_request.response == NULL ||
	    framebuffer_request.response->framebuffer_count < 1) {
		return NULL;
	}

	return framebuffer_request.response->framebuffers[0];
}

struct limine_memmap_response *
boot_get_memmap(void)
{
	return (struct limine_memmap_response *)memmap_request.response;
}

struct limine_hhdm_response *
boot_get_hhdm(void)
{
	return (struct limine_hhdm_response *)hhdm_request.response;
}

struct limine_module_response *
boot_get_modules(void)
{
	return (struct limine_module_response *)module_request.response;
}