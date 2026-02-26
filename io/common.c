#include <stdio.h>
#include <string.h>
#include <time.h>
#include "common.h" // Assuming you created common.h

// Function to write the standard file header
int write_file_header(FILE *fp, const char *filename, const char *category, int version) {
    if (fp == NULL || filename == NULL || category == NULL) {
        return -1; // Invalid arguments
    }

    time_t now = time(NULL);
    if (now == (time_t)(-1)) {
        perror("Failed to get current time");
        return -1;
    }

    // Format: #BEGINFILE\nFILENAME:<filename>\nCATEGORY:<category>\nVERSION:<version>\nTIMESTAMP:<timestamp>\n
    int written = fprintf(fp, "#BEGINFILE\nFILENAME:%s\nCATEGORY:%s\nVERSION:%d\nTIMESTAMP:%ld\n",
                          filename, category, version, (long)now);

    if (written < 0) {
        perror("Failed to write file header");
        return -1;
    }
    return 0; // Success
}

// Function to write the standard file footer
int write_file_footer(FILE *fp) {
    if (fp == NULL) {
        return -1; // Invalid arguments
    }

    int written = fprintf(fp, "#ENDFILE\n");
    if (written < 0) {
        perror("Failed to write file footer");
        return -1;
    }
    return 0; // Success
}

// Function to read and verify the standard file header
// Returns 0 on success, -1 on error or format mismatch
int read_file_header(FILE *fp, char *out_filename, size_t filename_size,
                     char *out_category, size_t category_size,
                     int *out_version, time_t *out_timestamp) {
    if (fp == NULL || out_filename == NULL || out_category == NULL || out_version == NULL || out_timestamp == NULL) {
        return -1; // Invalid arguments
    }

    char line[MAX_HEADER_LINE_LEN];

    // Read #BEGINFILE
    if (fgets(line, sizeof(line), fp) == NULL || strcmp(line, "#BEGINFILE\n") != 0) {
        fprintf(stderr, "Error: Missing or invalid #BEGINFILE marker.\n");
        return -1;
    }

    // Read FILENAME
    if (fgets(line, sizeof(line), fp) == NULL || sscanf(line, "FILENAME:%255[^\n]", out_filename) != 1) {
        fprintf(stderr, "Error: Missing or invalid FILENAME.\n");
        return -1;
    }
    if (strlen(out_filename) >= filename_size) {
        fprintf(stderr, "Error: Filename too long.\n");
        return -1;
    }


    // Read CATEGORY
    if (fgets(line, sizeof(line), fp) == NULL || sscanf(line, "CATEGORY:%127[^\n]", out_category) != 1) {
        fprintf(stderr, "Error: Missing or invalid CATEGORY.\n");
        return -1;
    }
    if (strlen(out_category) >= category_size) {
        fprintf(stderr, "Error: Category too long.\n");
        return -1;
    }

    // Read VERSION
    if (fgets(line, sizeof(line), fp) == NULL || sscanf(line, "VERSION:%d", out_version) != 1) {
        fprintf(stderr, "Error: Missing or invalid VERSION.\n");
        return -1;
    }

    // Read TIMESTAMP
    long timestamp_long;
    if (fgets(line, sizeof(line), fp) == NULL || sscanf(line, "TIMESTAMP:%ld", &timestamp_long) != 1) {
        fprintf(stderr, "Error: Missing or invalid TIMESTAMP.\n");
        return -1;
    }
    *out_timestamp = (time_t)timestamp_long;

    return 0; // Success
}

// Function to read and verify the standard file footer
// Returns 0 on success, -1 on error or format mismatch
int read_file_footer(FILE *fp) {
    if (fp == NULL) {
        return -1; // Invalid arguments
    }

    char line[MAX_HEADER_LINE_LEN];
    long current_pos = ftell(fp);
    if (current_pos == -1L) {
        perror("ftell before reading footer");
        return -1;
    }

    if (fgets(line, sizeof(line), fp) == NULL) {
        if (feof(fp)) {
            fprintf(stderr, "Error: Reached EOF before #ENDFILE marker.\n");
        } else {
            perror("Error reading for #ENDFILE marker");
        }
        return -1;
    }

    if (strcmp(line, "#ENDFILE\n") != 0) {
        fprintf(stderr, "Error: Missing or invalid #ENDFILE marker. Found: %s", line);
        if (fseek(fp, current_pos, SEEK_SET) != 0) {
            perror("fseek back after incorrect footer");
        }
        return -1;
    }

    return 0; // Success
}