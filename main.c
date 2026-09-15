/*
* Copyright (c) 2012 by naehrwert
* This file is released under the GPLv2.
*/
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "types.h"
#include "util.h"
#include "aes_xts.h"

/*! Size of one sector. */
#define SECTOR_SIZE 0x200
/*! Size of one streamed chunk (must be a multiple of SECTOR_SIZE).
    Kept large so the disk sees long sequential reads/writes; the chunk
    is then decrypted across all cores in parallel. */
#define CHUNK_SIZE (64 * 0x100000)

int main(int argc, char **argv)
{
	if (argc < 5) {
		fprintf(stdout, "usage: %s <keys> <input> <output> <ivoffset> [threads]\n", argv[0]);
		fprintf(stdout, "       set OMP_NUM_THREADS or pass [threads] to control crypto parallelism\n");
		return 0;
	}

	/* Read the data and tweak keys once. */
	unsigned char data[16], tweak[16];
	FILE *fk = fopen(argv[1], "rb");
	if (fk == NULL) {
		fprintf(stderr, "error: cannot open key file '%s'\n", argv[1]);
		return 1;
	}
	if (fread(data, 1, sizeof(data), fk) != sizeof(data) ||
	    fread(tweak, 1, sizeof(tweak), fk) != sizeof(tweak)) {
		fprintf(stderr, "error: key file '%s' must be at least 32 bytes\n", argv[1]);
		fclose(fk);
		return 1;
	}
	fclose(fk);

	u64 ivoffset = strtoull(argv[4], NULL, 10);

#ifdef _OPENMP
	if (argc >= 6) {
		int nthreads = atoi(argv[5]);
		if (nthreads > 0)
			omp_set_num_threads(nthreads);
	}
#endif

	FILE *in = fopen(argv[2], "rb");
	if (in == NULL) {
		fprintf(stderr, "error: cannot open input file '%s'\n", argv[2]);
		return 1;
	}
	FILE *out = fopen(argv[3], "wb");
	if (out == NULL) {
		fprintf(stderr, "error: cannot create output file '%s'\n", argv[3]);
		fclose(in);
		return 1;
	}

	unsigned char *buf = (unsigned char *)malloc(CHUNK_SIZE);
	if (buf == NULL) {
		fprintf(stderr, "error: out of memory\n");
		fclose(in);
		fclose(out);
		return 1;
	}

	/* The AES-XTS context is set up once and only read while crypting,
	   so it is shared read-only across all worker threads (the AES
	   tables in aes.c are static const). */
	aes_xts_ctxt_t ctx;
	aes_xts_init(&ctx, AES_DECRYPT, data, tweak, 128);

	/* Stream the file sequentially: read a big chunk, decrypt its sectors
	   in parallel, write it back. Disk I/O stays sequential (fast on a
	   mechanical drive) while all cores are used for the AES-XTS work. */
	u64 sector_index = 0;
	int failed = 0;
	size_t got;

	while ((got = fread(buf, 1, CHUNK_SIZE, in)) > 0) {
		long long nsec = (long long)(got / SECTOR_SIZE);

		#pragma omp parallel for schedule(static)
		for (long long s = 0; s < nsec; s++) {
			aes_xts_crypt(&ctx, ivoffset + sector_index + (u64)s, SECTOR_SIZE,
			              buf + s * SECTOR_SIZE, buf + s * SECTOR_SIZE);
		}

		if (fwrite(buf, 1, got, out) != got) {
			fprintf(stderr, "error: write failed (disk full?)\n");
			failed = 1;
			break;
		}

		sector_index += (u64)nsec;
	}

	if (ferror(in)) {
		fprintf(stderr, "error: read failed\n");
		failed = 1;
	}

	free(buf);
	fclose(in);
	if (fclose(out) != 0) {
		fprintf(stderr, "error: closing output failed\n");
		failed = 1;
	}

	return failed ? 1 : 0;
}
