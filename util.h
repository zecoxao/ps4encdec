/*
* Copyright (c) 2011-2012 by naehrwert
* This file is released under the GPLv2.
*/

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>

#include "types.h"

/*! Utility functions. */
void _hexdump(FILE *fp, const char *name, u32 offset, u8 *buf, int len, BOOL print_addr);
void _print_align(FILE *fp, const s8 *str, s32 align, s32 len);
u8 *_read_buffer(const s8 *file, u32 *length);
int _write_buffer(const s8 *file, u8 *buffer, u32 length);

#endif
