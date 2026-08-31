// Provides the few symbols sdr_code.c needs that live in other PocketSDR
// translation units (sdr_cmn.c, sdr_func.c). Rewriting them here is cheaper
// than cloning the full libsdr build chain (libfec, libldpc, librtk, fftw3,
// libusb). See scripts/interop/build_pocketsdr.py for the build pipeline.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// sdr_cpx_t is defined in pocket_sdr.h; we include it via the upstream header
// (with our fftw3 stub on the include path so the fftw typedefs resolve to
// our stub). All we need below are the allocation helpers.
#include "pocket_sdr.h"

// From sdr_cmn.c.
void *sdr_malloc(size_t size) {
    void *p = calloc(1, size);
    if (p == NULL) {
        fprintf(stderr, "sdr_malloc: out of memory (%zu bytes)\n", size);
        exit(1);
    }
    return p;
}

void sdr_free(void *p) { free(p); }

// From sdr_func.c. N is in complex samples.
sdr_cpx_t *sdr_cpx_malloc(int N) {
    sdr_cpx_t *p = (sdr_cpx_t *)calloc((size_t)N, sizeof(sdr_cpx_t));
    if (p == NULL) {
        fprintf(stderr, "sdr_cpx_malloc: out of memory (N=%d)\n", N);
        exit(1);
    }
    return p;
}

void sdr_cpx_free(sdr_cpx_t *cpx) { free(cpx); }

// From sdr_func.c — popcount-like parity of a 32-bit word. sdr_code.c uses
// it as the LFSR tap XOR. Copied verbatim from upstream so bit-exact output
// is preserved.
uint8_t sdr_xor_bits(uint32_t X) {
    static const uint8_t xor_8b[] = { // xor of 8 bits
        0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0, 1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1,
        1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1, 0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0,
        1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1, 0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0,
        0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0, 1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1,
        1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1, 0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0,
        0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0, 1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1,
        0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0, 1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1,
        1,0,0,1,0,1,1,0, 0,1,1,0,1,0,0,1, 0,1,1,0,1,0,0,1, 1,0,0,1,0,1,1,0
    };
    return xor_8b[(uint8_t)X] ^ xor_8b[(uint8_t)(X >> 8)] ^
        xor_8b[(uint8_t)(X >> 16)] ^ xor_8b[(uint8_t)(X >> 24)];
}
