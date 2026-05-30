/*
* Copyright (c) 2011-2012 by naehrwert
* This file is released under the GPLv2.
*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "types.h"
#include "util.h"

void _hexdump(FILE *fp, const char *name, u32 offset, u8 *buf, int len, BOOL print_addr)
{
	int i, j, align = strlen(name) + 1;

	fprintf(fp, "%s ", name);
	if(print_addr == TRUE)
		fprintf(fp, "%08x: ", offset);
	for(i = 0; i < len; i++)
	{
		if(i % 16 == 0 && i != 0)
		{
			fprintf(fp, "\n");
			for(j = 0; j < align; j++)
				putchar(' ');
			if(print_addr == TRUE)
				fprintf(fp, "%08X: ", offset + i);
		}
		fprintf(fp, "%02X ", buf[i]);
	}
	fprintf(fp, "\n");
}

void _print_align(FILE *fp, const s8 *str, s32 align, s32 len)
{
	s32 i, tmp;
	tmp = align - len;
	if(tmp < 0)
			tmp = 0;
	for(i = 0; i < tmp; i++)
		fputs(str, fp);
}

u8 *_read_buffer(const s8 *file, u32 *length)
{
	FILE *fp;
	u32 size;

	if((fp = fopen(file, "rb")) == NULL)
		return NULL;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	u8 *buffer = (u8 *)malloc(sizeof(u8) * size);
	fread(buffer, sizeof(u8), size, fp);

	if(length != NULL)
		*length = size;

	fclose(fp);

	return buffer;
}

int _write_buffer(const s8 *file, u8 *buffer, u32 length)
{
	FILE *fp;

	if((fp = fopen(file, "wb")) == NULL)
		return 0;

	/**/
	while(length > 0)
	{
		u32 wrlen = 1024;
		if(length < 1024)
			wrlen = length;
		fwrite(buffer, sizeof(u8), wrlen, fp);
		length -= wrlen;
		buffer += 1024;
	}
	/**/

	//fwrite(buffer, sizeof(u8), length, fp);

	fclose(fp);

	return 1;
}
