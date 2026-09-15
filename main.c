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
/*! Size of one work block (must be a multiple of SECTOR_SIZE). */
#define BUFFER_SIZE 0x100000

int main(int argc, char **argv)
{
	if (argc < 5) {
		fprintf(stdout, "usage: %s <keys> <input> <output> <ivoffset> [threads]\n", argv[0]);
		fprintf(stdout, "       set OMP_NUM_THREADS or pass [threads] to control parallelism\n");
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

	/* Determine input size (64-bit, so partitions > 4 GiB work). */
	FILE *fs = fopen(argv[2], "rb");
	if (fs == NULL) {
		fprintf(stderr, "error: cannot open input file '%s'\n", argv[2]);
		return 1;
	}
	_fseeki64(fs, 0, SEEK_END);
	s64 size = _ftelli64(fs);
	fclose(fs);
	if (size <= 0) {
		fprintf(stderr, "error: input file '%s' is empty\n", argv[2]);
		return 1;
	}

	/* Create/truncate the output file up front so threads can seek into it. */
	FILE *fo = fopen(argv[3], "wb");
	if (fo == NULL) {
		fprintf(stderr, "error: cannot create output file '%s'\n", argv[3]);
		return 1;
	}

	/* The AES-XTS context is set up once and only read during crypting,
	   so it can be shared read-only across all worker threads. */
	aes_xts_ctxt_t ctx;
	aes_xts_init(&ctx, AES_DECRYPT, data, tweak, 128);

#ifdef _OPENMP
	if (argc >= 6) {
		int nthreads = atoi(argv[5]);
		if (nthreads > 0)
			omp_set_num_threads(nthreads);
	}
#endif

	s64 nblocks = (size + BUFFER_SIZE - 1) / BUFFER_SIZE;
	int failed = 0;

	/* Each block is independent: its tweak depends only on the global
	   sector index, so blocks are processed fully in parallel. */
	#pragma omp parallel
	{
		FILE *in = fopen(argv[2], "rb");
		unsigned char *buf = (unsigned char *)malloc(BUFFER_SIZE);

		if (in == NULL || buf == NULL) {
			#pragma omp atomic write
			failed = 1;
		} else {
			#pragma omp for schedule(static)
			for (s64 b = 0; b < nblocks; b++) {
				s64 off = b * (s64)BUFFER_SIZE;
				s64 remain = size - off;
				size_t want = (remain < BUFFER_SIZE) ? (size_t)remain : BUFFER_SIZE;

				_fseeki64(in, off, SEEK_SET);
				size_t got = fread(buf, 1, want, in);

				/* Decrypt whole sectors; the tweak sequence number is the
				   absolute sector index plus the caller's iv offset. */
				u64 base_sector = (u64)(off / SECTOR_SIZE);
				size_t nsec = got / SECTOR_SIZE;
				for (size_t i = 0; i < nsec; i++) {
					aes_xts_crypt(&ctx, ivoffset + base_sector + i, SECTOR_SIZE,
					              buf + i * SECTOR_SIZE, buf + i * SECTOR_SIZE);
				}

				/* Write this block at its exact offset. Serialized because a
				   single output handle is shared; crypting (the hot path)
				   happens outside the lock. */
				#pragma omp critical(output)
				{
					_fseeki64(fo, off, SEEK_SET);
					if (fwrite(buf, 1, got, fo) != got) {
						#pragma omp atomic write
						failed = 1;
					}
				}
			}
		}

		free(buf);
		if (in != NULL)
			fclose(in);
	}

	fclose(fo);

	if (failed) {
		fprintf(stderr, "error: an I/O or allocation error occurred\n");
		return 1;
	}

	return 0;
}
