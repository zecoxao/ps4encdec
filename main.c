/*
* Copyright (c) 2012 by naehrwert
* This file is released under the GPLv2.
*/
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "aes_xts.h"




/*! Size of one sector. */
#define SECTOR_SIZE 0x200
#define BUFFER_SIZE 0x100000
#define TOTAL_SIZE 0x200000 //0x180000000
#define IVOFFSET 0x1800000000

int main(int argc, char **argv)
{

	unsigned int j=0;
	
	while(j < TOTAL_SIZE){ 
	
		unsigned char data [16];
		unsigned char tweak [16];
	
		FILE * fk = fopen("keys.bin","rb");
		int read = fread(data,0x10,1,fk);
		read = fread(tweak,0x10,1,fk);
		fclose(fk);
	
	
		unsigned char buf [BUFFER_SIZE];
	
	
	
		FILE *fp = fopen("update.bin","rb");
		printf("%08X\n", j);
		fseek(fp,j,SEEK_SET);
		read = fread(buf,BUFFER_SIZE,1,fp);
		fclose(fp);

		aes_xts_ctxt_t ctx;
		aes_xts_init(&ctx, AES_DECRYPT, data, tweak, 128);
		unsigned int i = 0;
		unsigned int k = (i + j) / SECTOR_SIZE;
		printf("%08X\n", k);
		for(i = 0; i < (BUFFER_SIZE / SECTOR_SIZE); i++){
			aes_xts_crypt(&ctx, IVOFFSET + i + k, SECTOR_SIZE, buf + (i *SECTOR_SIZE), buf + (i *SECTOR_SIZE));
		}
		FILE *fx = fopen ("out.bin","ab+");
		fwrite(buf,BUFFER_SIZE,1,fx);
		fclose(fx);
		
		j = j + BUFFER_SIZE;
	}
	
	
	
	
	return 0;
}
