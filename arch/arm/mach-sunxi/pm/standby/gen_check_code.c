/*
 ********************************************************
 * File    : gen_check_code.c
 * By      : weety
 * Data    : 2015-10-10 
 * Descript: Get the source file by reverse engineering.
 * 
 ********************************************************
 */

#include <stdio.h>

#define STAMP_VALUE 0x5F0A6C39

unsigned int gen_check_sum_code(const char *src_filename, const char *dst_filename)
{
	int result;
	int i; 
	FILE *dst_file = NULL;
	FILE *src_file = NULL;
	unsigned int align;
	unsigned int check_default_value;
	unsigned int length;
	unsigned int src_file_len;
	unsigned int file_data;
	unsigned int dst_file_len;
	unsigned int check_sum;
	unsigned int j;
	unsigned char data;

	check_sum = 0;
	check_default_value = STAMP_VALUE;
	src_file = fopen(src_filename, "rb+");
	if ( src_file )
	{
		puts("Source file is open");
		dst_file = fopen(dst_filename, "rb+");
		if ( dst_file )
		{
			puts("Destination file is open.");
			src_file_len = 0;
			for ( i = feof(src_file); !i; i = feof(src_file) )
			{
				data = fgetc(src_file);
				if ( !feof(src_file) )
				{
					fputc(data, dst_file);
					++src_file_len;
				}
			}
			fseek(src_file, 16L, 0);
			fread(&align, 4uL, 1uL, src_file);
			fclose(src_file);
			printf("temp value is %x.\n", align, dst_filename, src_filename);
			if ( (align & 0x80000000) != 0 )
			{
				dst_file_len = align & 0x7FFFFFFF;
			}
			else
			{
				puts("0");
				if ( src_file_len % align )
					dst_file_len = src_file_len + align - src_file_len % align;
				else
					dst_file_len = src_file_len;
			}
			printf("soure_file size is %u. \ndestination_file size is 0x%X.\n", src_file_len, dst_file_len);
			if ( src_file_len <= dst_file_len )
			{
				if ( src_file_len < dst_file_len )
				{
					src_file_len = dst_file_len - src_file_len;
					data = 0xff;
					while ( src_file_len )
					{
						fwrite(&data, 1uL, 1uL, dst_file);
						--src_file_len;
					}
					fclose(dst_file);
					dst_file = fopen(dst_filename, "rb+");
				}
				fseek(dst_file, 16L, 0);
				fwrite(&dst_file_len, 4uL, 1uL, dst_file);
				fclose(dst_file);
				dst_file = fopen(dst_filename, "rb+");
				length = dst_file_len >> 2;
				check_sum = 0;
				for ( j = 0; j < length; ++j )
				{
					fread(&file_data, 4uL, 1uL, dst_file);
					check_sum += file_data;
				}
				printf("check sum generated is 0x%X.\n", check_sum);
				fseek(dst_file, 12L, 0);
				fwrite(&check_sum, 4uL, 1uL, dst_file);
				fclose(dst_file);
				result = 0L;
			}
			else
			{
				puts("ERROR!The expected size of the destination file is smaLer than the size of source file.");
				result = 1L;
			}
		}
		else
		{
			puts("can't open destination file.");
			result = 1L;
		}
	}
	else
	{
		puts("can't open source file");
		result = 1L;
	}
	return result;
}

int main(int argc, const char **argv)
{
	int result;
	FILE *dst_file = NULL;
	FILE *src_file = NULL;

	src_file = fopen(argv[1], "rb+");
	if ( src_file )
	{
		puts("Source file is open");
		fclose(src_file);
		dst_file = fopen(argv[2], "wb+");
		if ( dst_file )
		{
			puts("Destination file is created.");
			fclose(dst_file);
			if ( gen_check_sum_code(argv[1], argv[2]) )
				puts("Open file failed.");
			else
				puts("Everything is ok.");
			result = 0;
		}
		else
		{
			puts("can't create destination file.");
			result = -1;
		}
	}
	else
	{
		puts("can't open source file");
		result = -1;
	}
	return result;
}
