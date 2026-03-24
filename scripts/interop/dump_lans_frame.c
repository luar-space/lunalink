/*
 * Harness to build an AFS frame using LANS-AFS-SIM's encoding pipeline.
 *
 * Produces a 6000-symbol binary frame with known payload for interop
 * comparison against LunaLink's frame_build().
 *
 * Usage: dump_lans_frame <outdir> <fid> <toi> <pattern>
 *   pattern: "zeros" | "ones" | "alternating"
 *
 * Output: <outdir>/lans_frame_<pattern>.bin  (6000 bytes, uint8 {0,1})
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "afs_nav.h"
#include "pocketsdr.h"

/* Sync pattern per LSIS V1.0 */
static const uint8_t SYNC_BYTES[9] = {
    0xCC, 0x63, 0xF7, 0x45, 0x36, 0xF4, 0x9E, 0x04, 0xA0
};

static void fill_pattern(uint8_t* buf, int len, const char* pattern)
{
    if (strcmp(pattern, "zeros") == 0) {
        memset(buf, 0, len);
    } else if (strcmp(pattern, "ones") == 0) {
        memset(buf, 1, len);
    } else if (strcmp(pattern, "alternating") == 0) {
        for (int i = 0; i < len; i++)
            buf[i] = (uint8_t)(i % 2);
    } else {
        fprintf(stderr, "Unknown pattern: %s\n", pattern);
        exit(1);
    }
}

int main(int argc, char** argv)
{
    if (argc < 5) {
        fprintf(stderr, "Usage: dump_lans_frame <outdir> <fid> <toi> <pattern>\n");
        return 1;
    }

    const char* outdir = argv[1];
    int fid = atoi(argv[2]);
    int toi = atoi(argv[3]);
    const char* pattern = argv[4];

    mkdir(outdir, 0755);

    /* --- Build frame (6000 symbols) --- */
    uint8_t frame[6000];
    memset(frame, 0, sizeof(frame));

    /* Sync pattern: 68 bits */
    sdr_unpack_bits(SYNC_BYTES, 68, frame);

    /* Subframe 1: BCH-encoded FID + TOI (52 bits) */
    uint8_t sb1[52];
    generate_BCH_AFS_SF1(sb1, fid, toi);
    memcpy(frame + 68, sb1, 52);

    /* Subframe 2: 1176 data + 24 CRC = 1200 info → 2400 coded */
    uint8_t sb2_info[1200];
    fill_pattern(sb2_info, 1176, pattern);
    memcpy(sb2_info + 1176, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 24);
    append_CRC24(sb2_info, 1200);

    /* Subframe 3: 846 data + 24 CRC = 870 info → 1740 coded */
    uint8_t sb3_info[870];
    fill_pattern(sb3_info, 846, pattern);
    memset(sb3_info + 846, 0, 24);
    append_CRC24(sb3_info, 870);

    /* Subframe 4: same structure as SF3 */
    uint8_t sb4_info[870];
    fill_pattern(sb4_info, 846, pattern);
    memset(sb4_info + 846, 0, 24);
    append_CRC24(sb4_info, 870);

    /* LDPC encode */
    uint8_t sb234_coded[5880];
    encode_LDPC_AFS_SF2(sb2_info, sb234_coded);          /* → 2400 */
    encode_LDPC_AFS_SF3(sb3_info, sb234_coded + 2400);   /* → 1740 */
    encode_LDPC_AFS_SF3(sb4_info, sb234_coded + 4140);   /* → 1740 */

    /* Interleave */
    uint8_t sb234_interleaved[5880];
    interleave_AFS_SF234(sb234_coded, sb234_interleaved);

    memcpy(frame + 120, sb234_interleaved, 5880);

    /* --- Write output --- */
    char path[512];
    snprintf(path, sizeof(path), "%s/lans_frame_%s_fid%d_toi%d.bin",
             outdir, pattern, fid, toi);

    FILE* fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "Cannot open %s\n", path);
        return 1;
    }
    fwrite(frame, 1, 6000, fp);
    fclose(fp);

    printf("Frame written: %s (6000 bytes)\n", path);
    printf("  FID=%d TOI=%d pattern=%s\n", fid, toi, pattern);

    return 0;
}
