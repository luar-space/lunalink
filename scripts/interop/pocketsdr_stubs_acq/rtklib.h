// Stub rtklib.h for the G-06.2 PocketSDR `pocket_acq` extraction.
//
// Unlike the G-04 code-generator harness, the acquisition harness pulls in
// sdr_func.c and sdr_cmn.c, which reference a scattering of RTKLIB helpers
// from paths pocket_acq never actually calls:
//
//   - tag file reader/writer (time2str, epoch2time)
//   - stream / log output   (strinit*, strsetopt, stropen, strclose,
//                            strwrite, strstat, and related constants)
//   - least-squares poly fit (mat, lsq)  — used by a polynomial smoother
//                            we don't exercise from the acquisition path
//
// We stub all of them here so sdr_func.c compiles and links cleanly. Any
// call reaching one of these at runtime either no-ops or returns failure;
// the G-06.2 pytest never takes a code path that touches them.
//
// Real fftw3 must be on the include path BEFORE this directory so that
// `#include <fftw3.h>` resolves to the system header rather than to the
// no-op stub in `pocketsdr_stubs/`.

#pragma once

// NOLINTBEGIN(modernize-deprecated-headers) — this is a C header, not C++.
#include <stdint.h>
#include <time.h>
// NOLINTEND(modernize-deprecated-headers)

typedef struct { time_t time; double sec; } gtime_t;

typedef struct obs_t    obs_t;
typedef struct nav_t    nav_t;
typedef struct sol_t    sol_t;
typedef struct ssat_t   ssat_t;
typedef struct rtcm_t   rtcm_t;

// stream_t must have a non-opaque body because sdr_func.c takes
// `sizeof(stream_t)` when allocating one. A single dummy field is enough;
// stropen() below returns 0 so the struct is never actually populated.
typedef struct stream_t { int dummy; } stream_t;

typedef struct libusb_context       libusb_context;
typedef struct libusb_device_handle libusb_device_handle;

// ── Time helpers (RTKLIB) ─────────────────────────────────────────────────
// Referenced from sdr_tag_read / sdr_tag_write. Tag files are skipped by the
// G-06.2 harness (we pass -fmt/-f/-fi on the command line), so the calls
// never need to produce correct output.

static inline void time2str(gtime_t t, char *s, int n) {
    (void)t; (void)n;
    if (s) { s[0] = '\0'; }
}

static inline gtime_t epoch2time(const double *ep) {
    (void)ep;
    gtime_t t = {0};
    return t;
}

// ── Stream / log helpers (RTKLIB) ─────────────────────────────────────────
// Compiled into sdr_func.c's logging path but pocket_acq never calls
// sdr_log_open(), so these remain reachable-but-unreached.

#define STR_TCPSVR  1
#define STR_TCPCLI  2
#define STR_FILE    3
#define STR_MODE_W  1

static inline void strinitcom(void)                                    {}
static inline void strinit(stream_t *s)                                { (void)s; }
static inline void strsetopt(const int *opt)                           { (void)opt; }
static inline int  stropen(stream_t *s, int t, int m, const char *p)   { (void)s; (void)t; (void)m; (void)p; return 0; }
static inline void strclose(stream_t *s)                               { (void)s; }
static inline int  strwrite(stream_t *s, uint8_t *d, int n)            { (void)s; (void)d; (void)n; return 0; }
static inline int  strstat(stream_t *s, char *msg)                     { (void)s; (void)msg; return 0; }

// ── Numerics (RTKLIB) ─────────────────────────────────────────────────────
// `mat()` allocates an n×m double matrix; `lsq()` does a least-squares
// solve. Both are used in sdr_func.c's polynomial-smoothing helper, which
// pocket_acq does not call. Returning NULL from mat() would crash, so we
// allocate a trivial buffer; lsq() returns non-zero (failure), causing the
// caller to fall through.

#include <stdlib.h>  /* NOLINT(modernize-deprecated-headers) */

// NOLINTBEGIN(google-readability-casting,cppcoreguidelines-owning-memory)
static inline double *mat(int n, int m) {
    const size_t sz = (size_t)(n > 0 ? n : 1) * (size_t)(m > 0 ? m : 1);
    return (double *)calloc(sz, sizeof(double));
}
// NOLINTEND(google-readability-casting,cppcoreguidelines-owning-memory)

static inline int lsq(const double *A, const double *y, int n, int m,
                      double *x, double *Q) {
    (void)A; (void)y; (void)n; (void)m; (void)x; (void)Q;
    return -1;
}
