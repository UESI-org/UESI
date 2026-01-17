#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if __BSD_VISIBLE

typedef struct {
	uint32_t state[16];
	uint32_t keystream[16];
	size_t available;
	int initialized;
} chacha_ctx;

static chacha_ctx global_rng = {0};

#define ROTL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void
chacha_quarterround(uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d)
{
	*a += *b; *d ^= *a; *d = ROTL32(*d, 16);
	*c += *d; *b ^= *c; *b = ROTL32(*b, 12);
	*a += *b; *d ^= *a; *d = ROTL32(*d, 8);
	*c += *d; *b ^= *c; *b = ROTL32(*b, 7);
}

static void
chacha_block(uint32_t out[16], const uint32_t in[16])
{
	uint32_t x[16];
	memcpy(x, in, sizeof(x));

	/* 10 double-rounds = 20 rounds */
	for (int i = 0; i < 10; i++) {
		chacha_quarterround(&x[0], &x[4], &x[8],  &x[12]);
		chacha_quarterround(&x[1], &x[5], &x[9],  &x[13]);
		chacha_quarterround(&x[2], &x[6], &x[10], &x[14]);
		chacha_quarterround(&x[3], &x[7], &x[11], &x[15]);
		
		chacha_quarterround(&x[0], &x[5], &x[10], &x[15]);
		chacha_quarterround(&x[1], &x[6], &x[11], &x[12]);
		chacha_quarterround(&x[2], &x[7], &x[8],  &x[13]);
		chacha_quarterround(&x[3], &x[4], &x[9],  &x[14]);
	}

	/* Add input to output */
	for (int i = 0; i < 16; i++) {
		out[i] = x[i] + in[i];
	}
}

/* Get entropy from kernel (TODO: implement properly) */
static void
get_entropy(void *buf, size_t len)
{
	/* TEMPORARY: Use RDRAND if available, otherwise use timer */
#ifdef __x86_64__
	uint64_t *p = (uint64_t *)buf;
	size_t i;
	
	/* Try RDRAND first */
	for (i = 0; i + 8 <= len; i += 8) {
		unsigned long long val;
		unsigned char ok;
		
		__asm__ volatile(
			"rdrand %0\n\t"
			"setc %1"
			: "=r"(val), "=qm"(ok)
			:
			: "cc"
		);
		
		if (ok) {
			*p++ = val;
		} else {
			/* Fallback: Use TSC (weak but better than nothing) */
			uint64_t tsc;
			__asm__ volatile("rdtsc" : "=A"(tsc));
			*p++ = tsc;
		}
	}
	
	/* Handle remaining bytes */
	if (i < len) {
		uint64_t val;
		__asm__ volatile("rdtsc" : "=A"(val));
		memcpy((char *)buf + i, &val, len - i);
	}
#else
	/* Generic fallback: very weak! */
	for (size_t i = 0; i < len; i++) {
		((uint8_t *)buf)[i] = rand() ^ (rand() >> 8);
	}
#endif
}

/* Initialize RNG */
static void
chacha_init(chacha_ctx *ctx)
{
	/* ChaCha20 constants "expand 32-byte k" */
	ctx->state[0] = 0x61707865;
	ctx->state[1] = 0x3320646e;
	ctx->state[2] = 0x79622d32;
	ctx->state[3] = 0x6b206574;

	uint32_t key[8];
	get_entropy(key, sizeof(key));
	memcpy(&ctx->state[4], key, sizeof(key));

	ctx->state[12] = 0;
	ctx->state[13] = 0;
	get_entropy(&ctx->state[14], 8);

	ctx->available = 0;
	ctx->initialized = 1;
}

/* Get random bytes */
static void
chacha_getbytes(chacha_ctx *ctx, void *buf, size_t len)
{
	uint8_t *out = (uint8_t *)buf;

	while (len > 0) {
		/* Need more keystream? */
		if (ctx->available == 0) {
			chacha_block(ctx->keystream, ctx->state);
			ctx->available = sizeof(ctx->keystream);
			
			/* Increment counter */
			if (++ctx->state[12] == 0) {
				++ctx->state[13];
			}
		}

		/* Copy from keystream */
		size_t use = (len < ctx->available) ? len : ctx->available;
		size_t offset = sizeof(ctx->keystream) - ctx->available;
		memcpy(out, (uint8_t *)ctx->keystream + offset, use);
		
		out += use;
		len -= use;
		ctx->available -= use;
	}
}

uint32_t
arc4random(void)
{
	if (!global_rng.initialized) {
		chacha_init(&global_rng);
	}

	uint32_t val;
	chacha_getbytes(&global_rng, &val, sizeof(val));
	return val;
}

void
arc4random_buf(void *buf, size_t nbytes)
{
	if (!global_rng.initialized) {
		chacha_init(&global_rng);
	}

	chacha_getbytes(&global_rng, buf, nbytes);
}

uint32_t
arc4random_uniform(uint32_t upper_bound)
{
	if (upper_bound < 2) {
		return 0;
	}

	/* Calculate minimum value to avoid modulo bias */
	uint32_t min = (0xFFFFFFFFU - upper_bound + 1) % upper_bound;
	uint32_t r;

	do {
		r = arc4random();
	} while (r < min);

	return r % upper_bound;
}

#endif /* __BSD_VISIBLE */