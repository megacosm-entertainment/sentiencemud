// filepath: /sentience/src/io/common.h
#ifndef IO_COMMON_H
#define IO_COMMON_H

#include <stdio.h>
#include <time.h>

#define MAX_FILENAME_LEN 256
#define MAX_CATEGORY_LEN 128
#define MAX_HEADER_LINE_LEN 512

int write_file_header(FILE *fp, const char *filename, const char *category, int version);
int write_file_footer(FILE *fp);
int read_file_header(FILE *fp, char *out_filename, size_t filename_size,
                     char *out_category, size_t category_size,
                     int *out_version, time_t *out_timestamp);
int read_file_footer(FILE *fp);

#endif // IO_COMMON_H