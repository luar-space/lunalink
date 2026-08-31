// G-04 — bit-exact interop harness: dump PRN code chips from PocketSDR's
// sdr_gen_code() for a given signal/PRN range, one uint8 {0,1} byte per
// chip. Consumed by tests/test_interop_pocketsdr_codes.py.
//
// Output format matches scripts/interop/dump_lans_codes.c: {outdir}/{sig_lower}_prn_{prn:03d}.bin
// one uint8 per chip, 0 or 1. PocketSDR returns int8_t {-1, +1}; we convert
// with `(1 - x) / 2` exactly like dump_lans_codes.c does for LANS-AFS-SIM.
//
// Usage:
//   dump_pocketsdr_codes <outdir> <signal> <prn_start> <prn_end>
//
//   <signal>: PocketSDR signal name — "L1CA", "L5I", "L5Q", "E1B", "E1C",
//             "B1CD", "B1CP", etc. Must match the string switch in
//             upstream sdr_code.c.
//
// Exit codes:
//   0 — all PRNs dumped successfully
//   1 — argument parse error
//   2 — sdr_gen_code returned NULL for a PRN (unsupported for this signal)
//   3 — file write error

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pocket_sdr.h"

static void str_tolower(char *dst, const char *src) {
    while (*src) {
        *dst++ = (char)tolower((unsigned char)*src);
        ++src;
    }
    *dst = '\0';
}

static int dump_one(const char *outdir, const char *sig, int prn) {
    int N = 0;
    int8_t *code = sdr_gen_code(sig, prn, &N);
    if (code == NULL || N <= 0) {
        fprintf(stderr, "sdr_gen_code(\"%s\", %d) returned NULL/empty\n",
                sig, prn);
        return 2;
    }

    char sig_lc[32];
    str_tolower(sig_lc, sig);

    char path[512];
    (void)snprintf(path, sizeof(path), "%s/%s_prn_%03d.bin", outdir, sig_lc, prn);

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "fopen %s: failed\n", path);
        return 3;
    }

    // Convert {-1, +1} (signal level) to {0, 1} (logic level) so the on-disk
    // format matches LunaLink's packed-chip outputs. Same convention as
    // dump_lans_codes.c. (+1 → 0, -1 → 1 per standard BPSK mapping).
    for (int i = 0; i < N; ++i) {
        uint8_t chip = (uint8_t)((1 - code[i]) / 2);
        if (fwrite(&chip, 1, 1, fp) != 1) {
            fclose(fp);
            fprintf(stderr, "fwrite %s chip %d: failed\n", path, i);
            return 3;
        }
    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr,
                "Usage: %s <outdir> <signal> <prn_start> <prn_end>\n"
                "  example: %s /tmp/codes L1CA 1 32\n",
                argv[0], argv[0]);
        return 1;
    }

    const char *outdir = argv[1];
    const char *sig = argv[2];
    int prn_start = atoi(argv[3]);
    int prn_end = atoi(argv[4]);

    if (prn_start < 1 || prn_end < prn_start) {
        fprintf(stderr, "invalid PRN range: %d..%d\n", prn_start, prn_end);
        return 1;
    }

    for (int prn = prn_start; prn <= prn_end; ++prn) {
        int rc = dump_one(outdir, sig, prn);
        if (rc != 0) {
            return rc;
        }
    }

    printf("dump_pocketsdr_codes %s PRN %d..%d → %s\n",
           sig, prn_start, prn_end, outdir);
    return 0;
}
