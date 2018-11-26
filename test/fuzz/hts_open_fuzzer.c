#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "htslib/hfile.h"
#include "htslib/hts.h"
#include "htslib/sam.h"
#include "htslib/vcf.h"

// Duplicated from: htsfile.c
static htsFile *dup_stdout(const char *mode) {
    int fd = dup(STDOUT_FILENO);
    hFILE *hfp = (fd >= 0) ? hdopen(fd, mode) : NULL;
    return hfp ? hts_hopen(hfp, "-", mode) : NULL;
}

static void view_sam(htsFile *in) {
    if (!in) {
        return;
    }
    samFile *out = dup_stdout("w");
    bam_hdr_t *hdr = sam_hdr_read(in);
    if (hdr == NULL) {
        assert(hts_close(out) == 0 && "Error closing duplicated SAM output descriptor.");
        return;
    }

    if (sam_hdr_write(out, hdr) != 0) {
        bam_hdr_destroy(hdr);
        assert(hts_close(out) == 0 && "Error closing duplicated SAM output descriptor.");
        return;
    }
    bam1_t *b = bam_init1();
    while (sam_read1(in, hdr, b) >= 0) {
        if (sam_write1(out, hdr, b) < 0) {
            break;
        }
    }
    bam_destroy1(b);

    bam_hdr_destroy(hdr);
    assert(hts_close(out) == 0 && "Error closing duplicated SAM output descriptor.");
    return;
}

static void view_vcf(htsFile *in) {
    if (!in) {
        return;
    }
    vcfFile *out = dup_stdout("w");
    bcf_hdr_t *hdr = bcf_hdr_read(in);
    if (hdr == NULL) {
        assert(hts_close(out) == 0 && "Error closing duplicated VCF output descriptor.");
        return;
    }

    if (bcf_hdr_write(out, hdr) != 0) {
        bcf_hdr_destroy(hdr);
        assert(hts_close(out) == 0 && "Error closing duplicated VCF output descriptor.");
    }
    bcf1_t *rec = bcf_init();
    while (bcf_read(in, hdr, rec) >= 0) {
        if (bcf_write(out, hdr, rec) < 0) {
            break;
        }
    }
    bcf_destroy(rec);

    bcf_hdr_destroy(hdr);
    assert(hts_close(out) == 0 && "Error closing duplicated VCF output descriptor.");
    return;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    hFILE *memfile;
    uint8_t *copy;
    copy = malloc(size);
    if (copy == NULL) {
        return 0;
    }
    memcpy(copy, data, size);
    memfile = hopen("mem:", "rb:", copy, size);
    if (memfile == NULL) {
        free(copy);
        return 0;
    }

    htsFile *ht_file = hts_hopen(memfile, "data", "rb");
    if (ht_file == NULL) {
        assert(hclose(memfile) == 0 && "Error closing memfile.");
        return 0;
    }
    switch (ht_file->format.category) {
        case sequence_data:
            view_sam(ht_file);
            break;
        case variant_data:
            view_vcf(ht_file);
            break;
        default:
            break;
    }
    hts_close(ht_file);
    return 0;
}
