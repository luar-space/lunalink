// Stub fftw3.h for the G-04 minimal extraction of PocketSDR's code
// generators. sdr_code.c only uses fftw3 through sdr_gen_code_fft(), which
// our dump_pocketsdr_codes harness never calls. The stub provides
// ABI-compatible typedefs and no-op prototypes so sdr_code.c compiles and
// links without pulling in libfftw3-dev.
//
// If this stub is ever linked into a program that DOES call
// sdr_gen_code_fft(), the runtime call to fftwf_execute_dft below will
// return silently without producing a valid FFT, and the caller will get
// nonsense. For interop code dumps, that's fine.

#pragma once

#include <stddef.h>  // NULL

#ifdef __cplusplus
extern "C" {
#endif

typedef float fftwf_complex[2];
typedef void *fftwf_plan;

#define FFTW_FORWARD  (-1)
#define FFTW_BACKWARD (+1)
#define FFTW_ESTIMATE (1U << 6)

static inline fftwf_plan fftwf_plan_dft_1d(int n, fftwf_complex *in,
                                           fftwf_complex *out, int sign,
                                           unsigned flags) {
    (void)n; (void)in; (void)out; (void)sign; (void)flags;
    return NULL;
}

static inline void fftwf_execute_dft(fftwf_plan p, fftwf_complex *in,
                                     fftwf_complex *out) {
    (void)p; (void)in; (void)out;
}

static inline void fftwf_destroy_plan(fftwf_plan p) { (void)p; }

#ifdef __cplusplus
}
#endif
