/***************************************************************************
 *                                                                         *
 *    In-engine threaded wildgen import using libpng                       *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <pthread.h>
#include <sys/stat.h>
#include <errno.h>

#include "merc.h"
#include "wilds.h"
#include "wilderness_storage.h"

typedef struct wilds_wildgen_color_map WILDS_WILDGEN_COLOR_MAP;
typedef struct wilds_wildgen_job WILDS_WILDGEN_JOB;
typedef struct wilds_wildgen_result WILDS_WILDGEN_RESULT;
typedef struct wilds_wildgen_status_rec WILDS_WILDGEN_STATUS_REC;

struct wilds_wildgen_color_map
{
    uint16_t r;
    uint16_t g;
    uint16_t b;
    char tile;
};

struct wilds_wildgen_job
{
    WILDS_WILDGEN_JOB *next;
    long job_id;
    long wilds_uid;
    int map_size_x;
    int map_size_y;
    char default_tile;
    bool use_grid;
    int grid_rows;
    int grid_cols;
    int expected_tile_width;
    int expected_tile_height;
    bool validate_elevation_grid;
    int source_bit_depth;   /* effective bit depth: 8 or 16 */
    char source_name[MIL];
    char elevation_source_name[MIL];
    char png_path[MSL];
    char elevation_png_path[MSL];
    int color_count;
    WILDS_WILDGEN_COLOR_MAP colors[256];
};

struct wilds_wildgen_result
{
    WILDS_WILDGEN_RESULT *next;
    long job_id;
    long wilds_uid;
    bool success;
    char *tiles;
    int width;
    int height;
    char message[MSL];
};

struct wilds_wildgen_status_rec
{
    WILDS_WILDGEN_STATUS_REC *next;
    long wilds_uid;
    long job_id;
    int state;
    time_t submitted_at;
    time_t started_at;
    time_t finished_at;
    char message[MSL];
};

enum
{
    WILDGEN_STATE_IDLE = 0,
    WILDGEN_STATE_QUEUED,
    WILDGEN_STATE_RUNNING,
    WILDGEN_STATE_APPLYING,
    WILDGEN_STATE_SUCCEEDED,
    WILDGEN_STATE_FAILED
};

static pthread_t wildgen_worker_thread;
static pthread_mutex_t wildgen_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wildgen_cond = PTHREAD_COND_INITIALIZER;
static bool wildgen_running = false;
static bool wildgen_stop_requested = false;
static long wildgen_next_job_id = 0;
static WILDS_WILDGEN_JOB *wildgen_queue_head = NULL;
static WILDS_WILDGEN_JOB *wildgen_queue_tail = NULL;
static WILDS_WILDGEN_RESULT *wildgen_result_head = NULL;
static WILDS_WILDGEN_RESULT *wildgen_result_tail = NULL;

/**
 * wildgen_probe_png_info - Read PNG header (IHDR) to get dimensions and bit depth
 *
 * Opens the file, reads only the IHDR chunk, and returns the image dimensions
 * and native bit depth without loading any pixel data. Useful for choosing
 * between 8-bit and 16-bit reader functions.
 *
 * @param path          Filesystem path to the PNG file
 * @param out_width     Receives image width in pixels (may be NULL)
 * @param out_height    Receives image height in pixels (may be NULL)
 * @param out_bit_depth Receives native bit depth (1, 2, 4, 8, or 16) (may be NULL)
 * @param err           Error message buffer (populated on failure)
 * @param err_size      Size of error buffer
 * @return              true on success, false on failure
 */
static bool wildgen_probe_png_info(const char *path,
    int *out_width, int *out_height, int *out_bit_depth,
    char *err, size_t err_size)
{
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;
    png_uint_32 width, height;
    int bit_depth, color_type;

    fp = fopen(path, "rb");
    if (!fp)
    {
        snprintf(err, err_size, "Failed to open PNG '%s': %s", path, strerror(errno));
        return false;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG read struct for '%s'", path);
        return false;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG info struct for '%s'", path);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "PNG header read error processing '%s'", path);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    if (out_width)
        *out_width = (int)width;
    if (out_height)
        *out_height = (int)height;
    if (out_bit_depth)
        *out_bit_depth = bit_depth;

    return true;
}

/**
 * wildgen_auto_allocate_map - Auto-allocate wilderness map buffers from PNG dimensions
 *
 * If the wilderness has no map allocated (map_size_x < 1 or map_size_y < 1),
 * allocates staticmap and map buffers at the given dimensions, filled with
 * the default terrain character. This allows wildgen import to create the
 * map on the fly without requiring a prior 'wedit create X Y'.
 *
 * @param pWilds    Wilderness to allocate map for
 * @param width     Map width (from PNG dimensions)
 * @param height    Map height (from PNG dimensions)
 * @param err       Error message buffer (populated on failure)
 * @param err_size  Size of error buffer
 * @return          true if map was allocated (or already existed), false on error
 */
static bool wildgen_auto_allocate_map(WILDS_DATA *pWilds, int width, int height,
    char *err, size_t err_size)
{
    size_t map_size;

    if (!pWilds || width < 1 || height < 1)
    {
        snprintf(err, err_size, "Invalid wilderness or dimensions for map allocation");
        return false;
    }

    /* Map already exists at the right size */
    if (pWilds->map_size_x == width && pWilds->map_size_y == height
        && pWilds->staticmap && pWilds->map)
        return true;

    /* Map exists at a different size - caller should handle this */
    if (pWilds->map_size_x > 0 && pWilds->map_size_y > 0)
    {
        snprintf(err, err_size,
            "PNG dimensions %dx%d do not match existing wilderness map %dx%d",
            width, height, pWilds->map_size_x, pWilds->map_size_y);
        return false;
    }

    map_size = (size_t)width * (size_t)height;

    pWilds->staticmap = calloc(1, map_size);
    pWilds->map = calloc(1, map_size);

    if (!pWilds->staticmap || !pWilds->map)
    {
        free(pWilds->staticmap);
        free(pWilds->map);
        pWilds->staticmap = NULL;
        pWilds->map = NULL;
        snprintf(err, err_size, "Out of memory allocating %dx%d map (%zu bytes)",
            width, height, map_size);
        return false;
    }

    memset(pWilds->staticmap, pWilds->cDefaultTerrain, map_size);
    memset(pWilds->map, pWilds->cDefaultTerrain, map_size);
    pWilds->map_size_x = width;
    pWilds->map_size_y = height;

    return true;
}

/**
 * wildgen_read_png_rgb - Read a PNG file into an 8-bit RGB pixel buffer via libpng
 *
 * Opens the file, validates the PNG header, and reads pixel data into a
 * malloc'd buffer of width*height*3 bytes (RGB). Handles palette, grayscale,
 * alpha, and 16-bit source images by applying appropriate libpng transforms
 * to normalize to 8-bit RGB output.
 *
 * @param path       Filesystem path to the PNG file
 * @param out_width  Receives image width in pixels
 * @param out_height Receives image height in pixels
 * @param err        Error message buffer (populated on failure)
 * @param err_size   Size of error buffer
 * @return           malloc'd pixel buffer (caller frees with free()), or NULL on failure
 */
static unsigned char *wildgen_read_png_rgb(const char *path,
    int *out_width, int *out_height,
    char *err, size_t err_size)
{
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;
    unsigned char *pixels = NULL;
    png_uint_32 width, height;
    int bit_depth, color_type;
    int y;

    fp = fopen(path, "rb");
    if (!fp)
    {
        snprintf(err, err_size, "Failed to open PNG '%s': %s", path, strerror(errno));
        return NULL;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG read struct for '%s'", path);
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG info struct for '%s'", path);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        if (pixels) free(pixels);
        if (row_pointers) free(row_pointers);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "PNG read error processing '%s'", path);
        return NULL;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    /* Normalize to 8-bit RGB regardless of source format */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_strip_alpha(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    pixels = malloc((size_t)width * (size_t)height * 3);
    if (!pixels)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Out of memory loading PNG '%s' (%ux%u)", path, width, height);
        return NULL;
    }

    row_pointers = malloc(sizeof(png_bytep) * height);
    if (!row_pointers)
    {
        free(pixels);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Out of memory loading PNG '%s' (%ux%u)", path, width, height);
        return NULL;
    }

    for (y = 0; y < (int)height; y++)
        row_pointers[y] = pixels + ((size_t)y * (size_t)width * 3);

    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, NULL);

    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    *out_width = (int)width;
    *out_height = (int)height;
    return pixels;
}

/**
 * wildgen_read_png_rgb16 - Read a PNG file into a 16-bit RGB pixel buffer via libpng
 *
 * Opens the file, validates the PNG header, and reads pixel data into a
 * malloc'd buffer of width*height*6 bytes (3 channels x 2 bytes, big-endian).
 * Handles palette, grayscale, alpha, and 8-bit source images by applying
 * appropriate libpng transforms to normalize to 16-bit RGB output.
 *
 * Pixel data is stored in network byte order (big-endian) as libpng produces.
 * Use (buf[i*2] << 8 | buf[i*2+1]) to extract host-order uint16 values.
 *
 * @param path       Filesystem path to the PNG file
 * @param out_width  Receives image width in pixels
 * @param out_height Receives image height in pixels
 * @param err        Error message buffer (populated on failure)
 * @param err_size   Size of error buffer
 * @return           malloc'd pixel buffer (caller frees with free()), or NULL on failure
 */
static unsigned char *wildgen_read_png_rgb16(const char *path,
    int *out_width, int *out_height,
    char *err, size_t err_size)
{
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;
    unsigned char *pixels = NULL;
    png_uint_32 width, height;
    int bit_depth, color_type;
    int y;

    fp = fopen(path, "rb");
    if (!fp)
    {
        snprintf(err, err_size, "Failed to open PNG '%s': %s", path, strerror(errno));
        return NULL;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG read struct for '%s'", path);
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG info struct for '%s'", path);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        if (pixels) free(pixels);
        if (row_pointers) free(row_pointers);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "PNG read error processing '%s'", path);
        return NULL;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    /* Normalize to 16-bit RGB regardless of source format */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    if (bit_depth < 16)
        png_set_expand_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png_ptr);
    if (color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_strip_alpha(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    pixels = malloc((size_t)width * (size_t)height * 6);
    if (!pixels)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Out of memory loading PNG '%s' (%ux%u)", path, width, height);
        return NULL;
    }

    row_pointers = malloc(sizeof(png_bytep) * height);
    if (!row_pointers)
    {
        free(pixels);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Out of memory loading PNG '%s' (%ux%u)", path, width, height);
        return NULL;
    }

    for (y = 0; y < (int)height; y++)
        row_pointers[y] = pixels + ((size_t)y * (size_t)width * 6);

    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, NULL);

    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    *out_width = (int)width;
    *out_height = (int)height;
    return pixels;
}

/**
 * wildgen_read_png_gray - Read a PNG file into an 8-bit grayscale pixel buffer via libpng
 *
 * Opens the file, validates the PNG header, and reads pixel data into a
 * malloc'd buffer of width*height bytes (single channel). Handles palette,
 * RGB, alpha, and 16-bit source images by applying appropriate libpng
 * transforms to normalize to 8-bit grayscale output.
 *
 * @param path       Filesystem path to the PNG file
 * @param out_width  Receives image width in pixels
 * @param out_height Receives image height in pixels
 * @param err        Error message buffer (populated on failure)
 * @param err_size   Size of error buffer
 * @return           malloc'd pixel buffer (caller frees with free()), or NULL on failure
 */
static unsigned char *wildgen_read_png_gray(const char *path,
    int *out_width, int *out_height,
    char *err, size_t err_size)
{
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;
    unsigned char *pixels = NULL;
    png_uint_32 width, height;
    int bit_depth, color_type;
    int y;

    fp = fopen(path, "rb");
    if (!fp)
    {
        snprintf(err, err_size, "Failed to open PNG '%s': %s", path, strerror(errno));
        return NULL;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG read struct for '%s'", path);
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG info struct for '%s'", path);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        if (pixels) free(pixels);
        if (row_pointers) free(row_pointers);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "PNG read error processing '%s'", path);
        return NULL;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    /* Normalize to 8-bit grayscale regardless of source format */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    if (bit_depth == 16)
        png_set_strip_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_rgb_to_gray_fixed(png_ptr, 1, -1, -1);
    if (color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_strip_alpha(png_ptr);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    pixels = malloc((size_t)width * (size_t)height);
    if (!pixels)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Out of memory loading PNG '%s' (%ux%u)", path, width, height);
        return NULL;
    }

    row_pointers = malloc(sizeof(png_bytep) * height);
    if (!row_pointers)
    {
        free(pixels);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Out of memory loading PNG '%s' (%ux%u)", path, width, height);
        return NULL;
    }

    for (y = 0; y < (int)height; y++)
        row_pointers[y] = pixels + ((size_t)y * (size_t)width);

    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, NULL);

    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    *out_width = (int)width;
    *out_height = (int)height;
    return pixels;
}

/**
 * wildgen_read_png_gray16 - Read a PNG file into a 16-bit grayscale pixel buffer via libpng
 *
 * Opens the file, validates the PNG header, and reads pixel data into a
 * malloc'd buffer of width*height*2 bytes (single channel, big-endian).
 * Handles palette, RGB, alpha, and 8-bit source images by applying
 * appropriate libpng transforms to normalize to 16-bit grayscale output.
 *
 * Pixel data is stored in network byte order (big-endian) as libpng produces.
 * Use (buf[i*2] << 8 | buf[i*2+1]) to extract host-order uint16 values.
 *
 * @param path       Filesystem path to the PNG file
 * @param out_width  Receives image width in pixels
 * @param out_height Receives image height in pixels
 * @param err        Error message buffer (populated on failure)
 * @param err_size   Size of error buffer
 * @return           malloc'd pixel buffer (caller frees with free()), or NULL on failure
 */
static unsigned char *wildgen_read_png_gray16(const char *path,
    int *out_width, int *out_height,
    char *err, size_t err_size)
{
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;
    unsigned char *pixels = NULL;
    png_uint_32 width, height;
    int bit_depth, color_type;
    int y;

    fp = fopen(path, "rb");
    if (!fp)
    {
        snprintf(err, err_size, "Failed to open PNG '%s': %s", path, strerror(errno));
        return NULL;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG read struct for '%s'", path);
        return NULL;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG info struct for '%s'", path);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        if (pixels) free(pixels);
        if (row_pointers) free(row_pointers);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "PNG read error processing '%s'", path);
        return NULL;
    }

    png_init_io(png_ptr, fp);
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    /* Normalize to 16-bit grayscale regardless of source format */
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png_ptr);
    if (bit_depth < 16)
        png_set_expand_16(png_ptr);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_rgb_to_gray_fixed(png_ptr, 1, -1, -1);
    if (color_type == PNG_COLOR_TYPE_RGBA || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_strip_alpha(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    pixels = malloc((size_t)width * (size_t)height * 2);
    if (!pixels)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Out of memory loading PNG '%s' (%ux%u)", path, width, height);
        return NULL;
    }

    row_pointers = malloc(sizeof(png_bytep) * height);
    if (!row_pointers)
    {
        free(pixels);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Out of memory loading PNG '%s' (%ux%u)", path, width, height);
        return NULL;
    }

    for (y = 0; y < (int)height; y++)
        row_pointers[y] = pixels + ((size_t)y * (size_t)width * 2);

    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, NULL);

    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);

    *out_width = (int)width;
    *out_height = (int)height;
    return pixels;
}

/**
 * wildgen_write_png_gray - Write an 8-bit grayscale pixel buffer to a PNG file
 *
 * Creates a PNG file with 8-bit grayscale color type. The pixel buffer must
 * contain width*height bytes (one byte per pixel).
 *
 * @param path       Filesystem path to write
 * @param pixels     Pixel buffer (width*height bytes, 8-bit gray)
 * @param width      Image width in pixels
 * @param height     Image height in pixels
 * @param err        Error message buffer (populated on failure)
 * @param err_size   Size of error buffer
 * @return           true on success, false on failure
 */
static bool wildgen_write_png_gray(const char *path,
    const unsigned char *pixels, int width, int height,
    char *err, size_t err_size)
{
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;
    bool success = false;
    int y;

    fp = fopen(path, "wb");
    if (!fp)
    {
        snprintf(err, err_size, "Failed to open '%s' for writing: %s", path, strerror(errno));
        return false;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG write struct for '%s'", path);
        return false;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG info struct for '%s'", path);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        if (row_pointers) free(row_pointers);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        snprintf(err, err_size, "PNG write error processing '%s'", path);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr,
        (png_uint_32)width, (png_uint_32)height,
        8, PNG_COLOR_TYPE_GRAY,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    row_pointers = malloc(sizeof(png_bytep) * (size_t)height);
    if (!row_pointers)
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        snprintf(err, err_size, "Out of memory writing PNG '%s'", path);
        return false;
    }

    for (y = 0; y < height; y++)
        row_pointers[y] = (png_bytep)(pixels + ((size_t)y * (size_t)width));

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);
    success = true;

    free(row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return success;
}

/**
 * wildgen_write_png_gray16 - Write a 16-bit grayscale pixel buffer to a PNG file
 *
 * Creates a PNG file with 16-bit grayscale color type. The pixel buffer must
 * contain width*height*2 bytes in big-endian (network byte order) as libpng
 * expects.
 *
 * @param path       Filesystem path to write
 * @param pixels     Pixel buffer (width*height*2 bytes, big-endian 16-bit gray)
 * @param width      Image width in pixels
 * @param height     Image height in pixels
 * @param err        Error message buffer (populated on failure)
 * @param err_size   Size of error buffer
 * @return           true on success, false on failure
 */
static bool wildgen_write_png_gray16(const char *path,
    const unsigned char *pixels, int width, int height,
    char *err, size_t err_size)
{
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;
    bool success = false;
    int y;

    fp = fopen(path, "wb");
    if (!fp)
    {
        snprintf(err, err_size, "Failed to open '%s' for writing: %s", path, strerror(errno));
        return false;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG write struct for '%s'", path);
        return false;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG info struct for '%s'", path);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        if (row_pointers) free(row_pointers);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        snprintf(err, err_size, "PNG write error processing '%s'", path);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr,
        (png_uint_32)width, (png_uint_32)height,
        16, PNG_COLOR_TYPE_GRAY,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    row_pointers = malloc(sizeof(png_bytep) * (size_t)height);
    if (!row_pointers)
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        snprintf(err, err_size, "Out of memory writing PNG '%s'", path);
        return false;
    }

    for (y = 0; y < height; y++)
        row_pointers[y] = (png_bytep)(pixels + ((size_t)y * (size_t)width * 2));

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);
    success = true;

    free(row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return success;
}

/**
 * wildgen_write_png_rgb - Write an 8-bit RGB pixel buffer to a PNG file
 *
 * Creates a PNG file with 8-bit RGB color type. The pixel buffer must
 * contain width*height*3 bytes (R, G, B per pixel).
 *
 * @param path       Filesystem path to write
 * @param pixels     Pixel buffer (width*height*3 bytes, 8-bit RGB)
 * @param width      Image width in pixels
 * @param height     Image height in pixels
 * @param err        Error message buffer (populated on failure)
 * @param err_size   Size of error buffer
 * @return           true on success, false on failure
 */
static bool wildgen_write_png_rgb(const char *path,
    const unsigned char *pixels, int width, int height,
    char *err, size_t err_size)
{
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;
    bool success = false;
    int y;

    fp = fopen(path, "wb");
    if (!fp)
    {
        snprintf(err, err_size, "Failed to open '%s' for writing: %s", path, strerror(errno));
        return false;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG write struct for '%s'", path);
        return false;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG info struct for '%s'", path);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        if (row_pointers) free(row_pointers);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        snprintf(err, err_size, "PNG write error processing '%s'", path);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr,
        (png_uint_32)width, (png_uint_32)height,
        8, PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    row_pointers = malloc(sizeof(png_bytep) * (size_t)height);
    if (!row_pointers)
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        snprintf(err, err_size, "Out of memory writing PNG '%s'", path);
        return false;
    }

    for (y = 0; y < height; y++)
        row_pointers[y] = (png_bytep)(pixels + ((size_t)y * (size_t)width * 3));

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);
    success = true;

    free(row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return success;
}

/**
 * wildgen_write_png_rgb16 - Write a 16-bit RGB pixel buffer to a PNG file
 *
 * Creates a PNG file with 16-bit RGB color type. The pixel buffer must
 * contain width*height*6 bytes (3 channels x 2 bytes) in big-endian
 * (network byte order) as libpng expects.
 *
 * @param path       Filesystem path to write
 * @param pixels     Pixel buffer (width*height*6 bytes, big-endian 16-bit RGB)
 * @param width      Image width in pixels
 * @param height     Image height in pixels
 * @param err        Error message buffer (populated on failure)
 * @param err_size   Size of error buffer
 * @return           true on success, false on failure
 */
static bool wildgen_write_png_rgb16(const char *path,
    const unsigned char *pixels, int width, int height,
    char *err, size_t err_size)
{
    FILE *fp = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *row_pointers = NULL;
    bool success = false;
    int y;

    fp = fopen(path, "wb");
    if (!fp)
    {
        snprintf(err, err_size, "Failed to open '%s' for writing: %s", path, strerror(errno));
        return false;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG write struct for '%s'", path);
        return false;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        snprintf(err, err_size, "Failed to create PNG info struct for '%s'", path);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        if (row_pointers) free(row_pointers);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        snprintf(err, err_size, "PNG write error processing '%s'", path);
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr,
        (png_uint_32)width, (png_uint_32)height,
        16, PNG_COLOR_TYPE_RGB,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    row_pointers = malloc(sizeof(png_bytep) * (size_t)height);
    if (!row_pointers)
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        snprintf(err, err_size, "Out of memory writing PNG '%s'", path);
        return false;
    }

    for (y = 0; y < height; y++)
        row_pointers[y] = (png_bytep)(pixels + ((size_t)y * (size_t)width * 6));

    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);
    success = true;

    free(row_pointers);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return success;
}

/**
 * wildgen_resolve_bit_depth - Determine effective bit depth for a PNG
 *
 * Returns the effective bit depth to use: if the wildgen config forces
 * 8 or 16, that is returned; otherwise the PNG's native bit depth is
 * used (8 for anything <= 8, 16 for 16-bit sources).
 *
 * @param native_depth   The PNG file's IHDR bit depth
 * @param config_depth   The wildgen_bitdepth config (0=auto, 8, or 16)
 * @return               8 or 16
 */
static int wildgen_resolve_bit_depth(int native_depth, int config_depth)
{
    if (config_depth == 16)
        return 16;
    if (config_depth == 8)
        return 8;

    /* auto: use 16 only if the source is natively 16-bit */
    return (native_depth >= 16) ? 16 : 8;
}

static WILDS_WILDGEN_STATUS_REC *wildgen_status_head = NULL;

static const char *wildgen_state_name(int state)
{
    switch (state)
    {
        case WILDGEN_STATE_QUEUED: return "queued";
        case WILDGEN_STATE_RUNNING: return "running";
        case WILDGEN_STATE_APPLYING: return "applying";
        case WILDGEN_STATE_SUCCEEDED: return "succeeded";
        case WILDGEN_STATE_FAILED: return "failed";
        default: return "idle";
    }
}

static WILDS_WILDGEN_STATUS_REC *wildgen_find_status(long wilds_uid, bool create)
{
    WILDS_WILDGEN_STATUS_REC *rec;

    for (rec = wildgen_status_head; rec; rec = rec->next)
        if (rec->wilds_uid == wilds_uid)
            return rec;

    if (!create)
        return NULL;

    rec = calloc(1, sizeof(*rec));
    if (!rec)
        return NULL;

    rec->wilds_uid = wilds_uid;
    rec->state = WILDGEN_STATE_IDLE;
    rec->next = wildgen_status_head;
    wildgen_status_head = rec;
    return rec;
}

static void wildgen_set_status(long wilds_uid, int state, long job_id, const char *message)
{
    WILDS_WILDGEN_STATUS_REC *rec = wildgen_find_status(wilds_uid, true);

    if (!rec)
        return;

    rec->state = state;
    rec->job_id = job_id;

    if (state == WILDGEN_STATE_QUEUED)
        rec->submitted_at = current_time;
    else if (state == WILDGEN_STATE_RUNNING)
        rec->started_at = current_time;
    else if (state == WILDGEN_STATE_SUCCEEDED || state == WILDGEN_STATE_FAILED)
        rec->finished_at = current_time;

    if (message && message[0] != '\0')
        snprintf(rec->message, sizeof(rec->message), "%s", message);
    else
        rec->message[0] = '\0';
}

static char wildgen_lookup_tile(const WILDS_WILDGEN_JOB *job, uint16_t r, uint16_t g, uint16_t b)
{
    int i;

    for (i = 0; i < job->color_count; i++)
    {
        if (job->colors[i].r == r && job->colors[i].g == g && job->colors[i].b == b)
            return job->colors[i].tile;
    }

    return job->default_tile;
}

static bool wildgen_ensure_directory(const char *path, char *err_buf, size_t err_buf_size)
{
    struct stat st;

    if (stat(path, &st) == 0)
    {
        if (S_ISDIR(st.st_mode))
            return true;

        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Path is not a directory: %s", path);
        return false;
    }

    if (mkdir(path, 0775) == 0)
        return true;

    if (errno == EEXIST)
        return true;

    if (err_buf && err_buf_size > 0)
        snprintf(err_buf, err_buf_size, "Failed to create directory %s: %s", path, strerror(errno));
    return false;
}

static bool wildgen_has_png_extension(const char *name)
{
    size_t len;

    if (IS_NULLSTR(name))
        return false;

    len = strlen(name);
    if (len < 4)
        return false;

    return LOWER(name[len - 4]) == '.' &&
           LOWER(name[len - 3]) == 'p' &&
           LOWER(name[len - 2]) == 'n' &&
           LOWER(name[len - 1]) == 'g';
}

static bool wildgen_is_safe_stem(const char *name)
{
    const char *p;

    if (IS_NULLSTR(name))
        return false;

    if (name[0] == '.' || strstr(name, ".."))
        return false;

    for (p = name; *p; p++)
    {
        if (*p == '/' || *p == '\\' || *p == ':')
            return false;

        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-')
            return false;
    }

    return true;
}

static bool wildgen_is_safe_filename(const char *name)
{
    const char *p;

    if (IS_NULLSTR(name))
        return false;

    if (name[0] == '.' || strstr(name, ".."))
        return false;

    for (p = name; *p; p++)
    {
        if (*p == '/' || *p == '\\' || *p == ':')
            return false;
    }

    return wildgen_has_png_extension(name);
}

static bool wildgen_verify_png_signature(const char *path, char *err_buf, size_t err_buf_size)
{
    static const unsigned char expected[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    unsigned char sig[8];
    FILE *fp;

    fp = fopen(path, "rb");
    if (!fp)
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Cannot open image '%s': %s", path, strerror(errno));
        return false;
    }

    if (fread(sig, 1, sizeof(sig), fp) != sizeof(sig))
    {
        fclose(fp);
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Image '%s' is too small to be a valid PNG", path);
        return false;
    }

    fclose(fp);

    if (memcmp(sig, expected, sizeof(expected)) != 0)
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Image '%s' does not have a valid PNG signature", path);
        return false;
    }

    return true;
}

static bool wildgen_build_image_path(WILDS_DATA *pWilds, const char *file_name,
    char *resolved_path, size_t resolved_size, char *err_buf, size_t err_buf_size)
{
    char images_dir[MSL];
    char wilds_dir[MSL];
    char state_path[MSL];
    char *last_slash;

    if (!pWilds || pWilds->uid < 1)
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Wildgen import requires a saved wilderness definition (valid uid)");
        return false;
    }

    if (!wildgen_is_safe_filename(file_name))
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Image name must be a filename ending in .png (no directories)");
        return false;
    }

    if (!wilderness_storage_build_images_dir_path(pWilds, images_dir, sizeof(images_dir)))
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size,
                "Wildgen images path is invalid for wilderness %ld (expected <uid>_<name>/images)",
                pWilds->uid);
        return false;
    }

    if (!wilderness_storage_build_state_path(pWilds, state_path, sizeof(state_path)))
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size,
                "Wildgen state path is invalid for wilderness %ld", pWilds->uid);
        return false;
    }

    snprintf(wilds_dir, sizeof(wilds_dir), "%s", state_path);
    last_slash = strrchr(wilds_dir, '/');
    if (!last_slash)
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size,
                "Wildgen storage path is invalid for wilderness %ld", pWilds->uid);
        return false;
    }
    *last_slash = '\0';

    if (!wildgen_ensure_directory(WORLD_DIR "wilderness_state", err_buf, err_buf_size))
        return false;
    if (!wildgen_ensure_directory(wilds_dir, err_buf, err_buf_size))
        return false;
    if (!wildgen_ensure_directory(images_dir, err_buf, err_buf_size))
        return false;

    if (snprintf(resolved_path, resolved_size, "%s/%s", images_dir, file_name) >= (int)resolved_size)
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Resolved wildgen image path is too long");
        return false;
    }
    return true;
}

static bool wildgen_build_grid_path(WILDS_DATA *pWilds, const char *base_name,
    int row, int col, int rows, int cols,
    char *resolved_path, size_t resolved_size, char *err_buf, size_t err_buf_size)
{
    char file_name[MIL];
    char suffix[64];
    int suffix_len = 0;
    size_t base_len;

    if (!wildgen_is_safe_stem(base_name))
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Grid base must use only letters, numbers, '_' or '-' (no extension/path)");
        return false;
    }

    if (rows > 1)
    {
        if (cols > 1)
            suffix_len = snprintf(suffix, sizeof(suffix), "_%d_%d.png", row, col);
        else
            suffix_len = snprintf(suffix, sizeof(suffix), "_%d.png", row);
    }
    else
    {
        if (cols > 1)
            suffix_len = snprintf(suffix, sizeof(suffix), "_%d.png", col);
        else
            suffix_len = snprintf(suffix, sizeof(suffix), ".png");
    }

    if (suffix_len < 0 || suffix_len >= (int)sizeof(suffix))
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Grid filename suffix is invalid");
        return false;
    }

    base_len = strlcpy(file_name, base_name, sizeof(file_name));
    if (base_len >= sizeof(file_name) || (base_len + (size_t)suffix_len) >= sizeof(file_name))
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Grid filename is too long");
        return false;
    }

    strlcat(file_name, suffix, sizeof(file_name));

    return wildgen_build_image_path(pWilds, file_name, resolved_path, resolved_size, err_buf, err_buf_size);
}

/**
 * wildgen_extract_rgb16 - Extract a pixel as uint16_t R/G/B from either 8-bit or 16-bit buffer
 *
 * For 8-bit buffers (bpp=3): reads 3 bytes, upscales each via (v<<8)|v.
 * For 16-bit buffers (bpp=6): reads 6 bytes, extracts big-endian uint16 pairs.
 *
 * @param pixels  Pixel buffer
 * @param index   Pixel index (0-based, not byte offset)
 * @param bpp     Bytes per pixel (3 for 8-bit RGB, 6 for 16-bit RGB)
 * @param r       Receives red channel as uint16_t
 * @param g       Receives green channel as uint16_t
 * @param b       Receives blue channel as uint16_t
 */
static inline void wildgen_extract_rgb16(const unsigned char *pixels, size_t index,
    int bpp, uint16_t *r, uint16_t *g, uint16_t *b)
{
    if (bpp == 6)
    {
        size_t off = index * 6;
        *r = (uint16_t)(pixels[off + 0] << 8 | pixels[off + 1]);
        *g = (uint16_t)(pixels[off + 2] << 8 | pixels[off + 3]);
        *b = (uint16_t)(pixels[off + 4] << 8 | pixels[off + 5]);
    }
    else
    {
        size_t off = index * 3;
        *r = (uint16_t)(pixels[off + 0] << 8 | pixels[off + 0]);
        *g = (uint16_t)(pixels[off + 1] << 8 | pixels[off + 1]);
        *b = (uint16_t)(pixels[off + 2] << 8 | pixels[off + 2]);
    }
}

static bool wildgen_convert_pixels_to_tiles(const WILDS_WILDGEN_JOB *job,
    const unsigned char *pixels, int width, int height, int bytes_per_pixel,
    char *tiles, int *unknown_pixels)
{
    size_t total = (size_t)width * (size_t)height;
    size_t i;
    int unknown = 0;

    for (i = 0; i < total; i++)
    {
        uint16_t r, g, b;
        char tile;

        wildgen_extract_rgb16(pixels, i, bytes_per_pixel, &r, &g, &b);
        tile = wildgen_lookup_tile(job, r, g, b);

        if (tile == job->default_tile)
        {
            int known = 0;
            int c;
            for (c = 0; c < job->color_count; c++)
            {
                if (job->colors[c].r == r && job->colors[c].g == g && job->colors[c].b == b)
                {
                    known = 1;
                    break;
                }
            }
            if (!known)
                unknown++;
        }

        tiles[i] = tile;
    }

    if (unknown_pixels)
        *unknown_pixels = unknown;

    return true;
}

static int wildgen_collect_colors(WILDS_DATA *pWilds, WILDS_WILDGEN_COLOR_MAP *colors, int max_colors)
{
    WILDS_TERRAIN *terrain;
    int count = 0;

    if (!pWilds || !colors || max_colors < 1)
        return 0;

    for (terrain = pWilds->pTerrain; terrain; terrain = terrain->next)
    {
        if (!terrain->wildgen_has_color)
            continue;

        if (count >= max_colors)
            break;

        colors[count].r = ((uint16_t)terrain->wildgen_r << 8) | (uint16_t)terrain->wildgen_r;
        colors[count].g = ((uint16_t)terrain->wildgen_g << 8) | (uint16_t)terrain->wildgen_g;
        colors[count].b = ((uint16_t)terrain->wildgen_b << 8) | (uint16_t)terrain->wildgen_b;
        colors[count].tile = terrain->mapchar;
        count++;
    }

    return count;
}

bool wilds_wildgen_check_image(WILDS_DATA *pWilds, const char *png_path, char *out_buf, size_t out_buf_size)
{
    typedef struct unmatched_rgb_count UNMATCHED_RGB_COUNT;
    struct unmatched_rgb_count
    {
        uint16_t r;
        uint16_t g;
        uint16_t b;
        size_t count;
    };

    WILDS_WILDGEN_COLOR_MAP colors[256];
    UNMATCHED_RGB_COUNT unmatched_colors[512];
    unsigned char *pixels = NULL;
    char resolved_path[MSL];
    char top_buf[MSL];
    int color_count;
    int width = 0;
    int height = 0;
    int bit_depth = 8;
    int bpp = 3;
    bool is_16bit = false;
    int unmatched_unique = 0;
    int top_k;
    bool unmatched_overflow = false;
    size_t total = 0;
    size_t i;
    size_t unmatched = 0;
    size_t matched = 0;

    if (out_buf && out_buf_size > 0)
        out_buf[0] = '\0';

    if (!pWilds || IS_NULLSTR(png_path))
    {
        if (out_buf && out_buf_size > 0)
            snprintf(out_buf, out_buf_size, "Usage: wildgen check <png_filename.png>");
        return false;
    }

    if (!wildgen_build_image_path(pWilds, png_path, resolved_path, sizeof(resolved_path), out_buf, out_buf_size))
        return false;

    if (!wildgen_verify_png_signature(resolved_path, out_buf, out_buf_size))
        return false;

    color_count = wildgen_collect_colors(pWilds, colors, (int)(sizeof(colors) / sizeof(colors[0])));
    if (color_count < 1)
    {
        if (out_buf && out_buf_size > 0)
            snprintf(out_buf, out_buf_size, "No terrain wildgen colors are configured. Set with 'terrain <token> wildcolor #RRGGBB'.");
        return false;
    }

    {
        char load_err[MSL];
        load_err[0] = '\0';

        if (!wildgen_probe_png_info(resolved_path, NULL, NULL, &bit_depth, load_err, sizeof(load_err)))
        {
            if (out_buf && out_buf_size > 0)
                snprintf(out_buf, out_buf_size, "%s", load_err);
            return false;
        }

        is_16bit = (bit_depth == 16);
        bpp = is_16bit ? 6 : 3;

        if (is_16bit)
            pixels = wildgen_read_png_rgb16(resolved_path, &width, &height, load_err, sizeof(load_err));
        else
            pixels = wildgen_read_png_rgb(resolved_path, &width, &height, load_err, sizeof(load_err));

        if (!pixels)
        {
            if (out_buf && out_buf_size > 0)
                snprintf(out_buf, out_buf_size, "%s", load_err);
            return false;
        }
    }

    if (width != pWilds->map_size_x || height != pWilds->map_size_y)
    {
        free(pixels);
        if (out_buf && out_buf_size > 0)
            snprintf(out_buf, out_buf_size,
                "Check failed: PNG dimensions %dx%d do not match wilds %dx%d",
                width, height, pWilds->map_size_x, pWilds->map_size_y);
        return false;
    }

    total = (size_t)width * (size_t)height;
    memset(unmatched_colors, 0, sizeof(unmatched_colors));
    top_buf[0] = '\0';

    for (i = 0; i < total; i++)
    {
        uint16_t r, g, b;
        bool known = false;
        int c;

        wildgen_extract_rgb16(pixels, i, bpp, &r, &g, &b);

        for (c = 0; c < color_count; c++)
        {
            if (colors[c].r == r && colors[c].g == g && colors[c].b == b)
            {
                known = true;
                break;
            }
        }

        if (known)
        {
            matched++;
        }
        else
        {
            int idx;

            unmatched++;

            for (idx = 0; idx < unmatched_unique; idx++)
            {
                if (unmatched_colors[idx].r == r && unmatched_colors[idx].g == g && unmatched_colors[idx].b == b)
                {
                    unmatched_colors[idx].count++;
                    break;
                }
            }

            if (idx >= unmatched_unique)
            {
                if (unmatched_unique < (int)(sizeof(unmatched_colors) / sizeof(unmatched_colors[0])))
                {
                    unmatched_colors[unmatched_unique].r = r;
                    unmatched_colors[unmatched_unique].g = g;
                    unmatched_colors[unmatched_unique].b = b;
                    unmatched_colors[unmatched_unique].count = 1;
                    unmatched_unique++;
                }
                else
                {
                    unmatched_overflow = true;
                }
            }
        }
    }

    free(pixels);

    if (out_buf && out_buf_size > 0)
    {
        if (unmatched > 0)
        {
            size_t top_used[5] = { 0, 0, 0, 0, 0 };
            double pct = total > 0 ? ((double)unmatched * 100.0 / (double)total) : 0.0;
            size_t top_offset = 0;

            top_k = UMIN(5, unmatched_unique);
            for (int rank = 0; rank < top_k; rank++)
            {
                size_t best_count = 0;
                int best_idx = -1;

                for (int idx = 0; idx < unmatched_unique; idx++)
                {
                    bool already_used = false;

                    for (int used_i = 0; used_i < rank; used_i++)
                    {
                        if (top_used[used_i] == (size_t)idx)
                        {
                            already_used = true;
                            break;
                        }
                    }

                    if (already_used)
                        continue;

                    if (unmatched_colors[idx].count > best_count)
                    {
                        best_count = unmatched_colors[idx].count;
                        best_idx = idx;
                    }
                }

                if (best_idx < 0)
                    break;

                top_used[rank] = (size_t)best_idx;

                {
                    int nearest_idx = -1;
                    int nearest_dist = INT_MAX;

                    for (int c = 0; c < color_count; c++)
                    {
                        int dr = abs((int)unmatched_colors[best_idx].r - (int)colors[c].r);
                        int dg = abs((int)unmatched_colors[best_idx].g - (int)colors[c].g);
                        int db = abs((int)unmatched_colors[best_idx].b - (int)colors[c].b);
                        int dist = dr + dg + db;

                        if (dist < nearest_dist)
                        {
                            nearest_dist = dist;
                            nearest_idx = c;
                        }
                    }

                    if (nearest_idx >= 0)
                    {
                        if (is_16bit)
                            top_offset += snprintf(top_buf + top_offset,
                                sizeof(top_buf) > top_offset ? sizeof(top_buf) - top_offset : 0,
                                "%s#%04X%04X%04X:%zu->#%04X%04X%04X('%c')",
                                rank == 0 ? "" : ", ",
                                unmatched_colors[best_idx].r,
                                unmatched_colors[best_idx].g,
                                unmatched_colors[best_idx].b,
                                unmatched_colors[best_idx].count,
                                colors[nearest_idx].r,
                                colors[nearest_idx].g,
                                colors[nearest_idx].b,
                                colors[nearest_idx].tile);
                        else
                            top_offset += snprintf(top_buf + top_offset,
                                sizeof(top_buf) > top_offset ? sizeof(top_buf) - top_offset : 0,
                                "%s#%02X%02X%02X:%zu->#%02X%02X%02X('%c')",
                                rank == 0 ? "" : ", ",
                                unmatched_colors[best_idx].r >> 8,
                                unmatched_colors[best_idx].g >> 8,
                                unmatched_colors[best_idx].b >> 8,
                                unmatched_colors[best_idx].count,
                                colors[nearest_idx].r >> 8,
                                colors[nearest_idx].g >> 8,
                                colors[nearest_idx].b >> 8,
                                colors[nearest_idx].tile);
                    }
                    else
                    {
                        if (is_16bit)
                            top_offset += snprintf(top_buf + top_offset,
                                sizeof(top_buf) > top_offset ? sizeof(top_buf) - top_offset : 0,
                                "%s#%04X%04X%04X:%zu",
                                rank == 0 ? "" : ", ",
                                unmatched_colors[best_idx].r,
                                unmatched_colors[best_idx].g,
                                unmatched_colors[best_idx].b,
                                unmatched_colors[best_idx].count);
                        else
                            top_offset += snprintf(top_buf + top_offset,
                                sizeof(top_buf) > top_offset ? sizeof(top_buf) - top_offset : 0,
                                "%s#%02X%02X%02X:%zu",
                                rank == 0 ? "" : ", ",
                                unmatched_colors[best_idx].r >> 8,
                                unmatched_colors[best_idx].g >> 8,
                                unmatched_colors[best_idx].b >> 8,
                                unmatched_colors[best_idx].count);
                    }
                }

                if (top_offset >= sizeof(top_buf))
                    break;
            }

            snprintf(out_buf, out_buf_size,
                "Wildgen check: %dx%d, colors configured=%d, matched=%zu unmatched=%zu (%.2f%%). Top unmatched: %s%s",
                width,
                height,
                color_count,
                matched,
                unmatched,
                pct,
                top_buf[0] ? top_buf : "(none)",
                unmatched_overflow ? " (plus additional unmatched colors)" : "");
        }
        else
        {
            snprintf(out_buf, out_buf_size,
                "Wildgen check: %dx%d, colors configured=%d, matched=%zu unmatched=0 (0.00%%). Ready to import.",
                width,
                height,
                color_count,
                matched);
        }
    }

    return true;
}

static bool wildgen_export_image_internal(WILDS_DATA *pWilds, const char *png_path,
    bool use_effective_map, char *out_buf, size_t out_buf_size)
{
    unsigned char tile_r[256];
    unsigned char tile_g[256];
    unsigned char tile_b[256];
    bool tile_has_color[256];
    unsigned char default_r = 0;
    unsigned char default_g = 0;
    unsigned char default_b = 0;
    bool default_has_color = false;
    const char *source_map;
    char resolved_path[MSL];
    unsigned char *pixels = NULL;
    WILDS_TERRAIN *terrain;
    int width;
    int height;
    int write_depth;
    int bpp;
    int x, y;
    size_t total;
    size_t unknown_tiles = 0;
    bool success = false;

    if (out_buf && out_buf_size > 0)
        out_buf[0] = '\0';

    if (!pWilds || IS_NULLSTR(png_path))
    {
        if (out_buf && out_buf_size > 0)
            snprintf(out_buf, out_buf_size, "Usage: wildgen export <png_filename.png>");
        return false;
    }

    if (pWilds->map_size_x < 1 || pWilds->map_size_y < 1)
    {
        if (out_buf && out_buf_size > 0)
            snprintf(out_buf, out_buf_size, "Wilderness dimensions are invalid");
        return false;
    }

    if (use_effective_map)
        source_map = pWilds->map;
    else
        source_map = pWilds->staticmap ? pWilds->staticmap : pWilds->map;

    if (!source_map)
    {
        if (out_buf && out_buf_size > 0)
            snprintf(out_buf, out_buf_size,
                use_effective_map
                    ? "No effective wilderness map data is loaded"
                    : "No wilderness map data is loaded");
        return false;
    }

    if (!wildgen_build_image_path(pWilds, png_path, resolved_path, sizeof(resolved_path), out_buf, out_buf_size))
        return false;

    memset(tile_has_color, 0, sizeof(tile_has_color));
    memset(tile_r, 0, sizeof(tile_r));
    memset(tile_g, 0, sizeof(tile_g));
    memset(tile_b, 0, sizeof(tile_b));

    for (terrain = pWilds->pTerrain; terrain; terrain = terrain->next)
    {
        unsigned char idx = (unsigned char)terrain->mapchar;

        if (!terrain->wildgen_has_color)
            continue;

        tile_has_color[idx] = true;
        tile_r[idx] = terrain->wildgen_r;
        tile_g[idx] = terrain->wildgen_g;
        tile_b[idx] = terrain->wildgen_b;
    }

    {
        unsigned char idx = (unsigned char)pWilds->cDefaultTerrain;
        if (tile_has_color[idx])
        {
            default_has_color = true;
            default_r = tile_r[idx];
            default_g = tile_g[idx];
            default_b = tile_b[idx];
        }
    }

    width = pWilds->map_size_x;
    height = pWilds->map_size_y;
    total = (size_t)width * (size_t)height;

    /* Determine export bit depth from config: 0=auto means 8 for export since
     * terrain colors are defined as 8-bit values */
    write_depth = (pWilds->wildgen_bitdepth == 16) ? 16 : 8;
    bpp = (write_depth == 16) ? 6 : 3;

    pixels = malloc(total * (size_t)bpp);
    if (!pixels)
    {
        if (out_buf && out_buf_size > 0)
            snprintf(out_buf, out_buf_size, "Out of memory preparing exported PNG buffer");
        return false;
    }

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            size_t map_idx = (size_t)y * (size_t)width + (size_t)x;
            size_t pix_idx = map_idx * (size_t)bpp;
            unsigned char tile = (unsigned char)source_map[map_idx];
            unsigned char cr, cg, cb;

            if (tile_has_color[tile])
            {
                cr = tile_r[tile];
                cg = tile_g[tile];
                cb = tile_b[tile];
            }
            else if (default_has_color)
            {
                cr = default_r;
                cg = default_g;
                cb = default_b;
                unknown_tiles++;
            }
            else
            {
                cr = 0;
                cg = 0;
                cb = 0;
                unknown_tiles++;
            }

            if (write_depth == 16)
            {
                /* Upscale 8-bit to 16-bit: (v<<8)|v maps 0->0, 255->65535 */
                pixels[pix_idx + 0] = cr;
                pixels[pix_idx + 1] = cr;
                pixels[pix_idx + 2] = cg;
                pixels[pix_idx + 3] = cg;
                pixels[pix_idx + 4] = cb;
                pixels[pix_idx + 5] = cb;
            }
            else
            {
                pixels[pix_idx + 0] = cr;
                pixels[pix_idx + 1] = cg;
                pixels[pix_idx + 2] = cb;
            }
        }
    }

    if (write_depth == 16)
        success = wildgen_write_png_rgb16(resolved_path, pixels, width, height, out_buf, out_buf_size);
    else
        success = wildgen_write_png_rgb(resolved_path, pixels, width, height, out_buf, out_buf_size);

    if (success && out_buf && out_buf_size > 0)
    {
        double pct = total > 0 ? ((double)unknown_tiles * 100.0 / (double)total) : 0.0;
        snprintf(out_buf, out_buf_size,
            "Wildgen export (%s) wrote %dx%d %d-bit PNG to %s (tiles without explicit color: %zu, %.2f%%)",
            use_effective_map ? "effective map" : "static map",
            width,
            height,
            write_depth,
            png_path,
            unknown_tiles,
            pct);
    }

    free(pixels);
    return success;
}

/**
 * wildgen_export_elevation_internal - Export wilderness elevation as a grayscale PNG
 *
 * Generates a grayscale PNG from the wilderness map. Each tile is mapped to a
 * grayscale value based on its terrain type's configured elevation, or the
 * wilderness default_elevation if no per-terrain elevation is set.
 *
 * In the current implementation, all pixels are set to the wilderness
 * default_elevation value since per-terrain elevation is not yet defined.
 * 8-bit exports clamp the value to 0-255; 16-bit exports use the full range.
 *
 * @param pWilds            Wilderness to export
 * @param png_path          Output PNG filename
 * @param out_buf           Status/error message buffer
 * @param out_buf_size      Size of message buffer
 * @return                  true on success, false on failure
 */
static bool wildgen_export_elevation_internal(WILDS_DATA *pWilds, const char *png_path,
    char *out_buf, size_t out_buf_size)
{
    char resolved_path[MSL];
    unsigned char *pixels = NULL;
    int width;
    int height;
    int write_depth;
    int bytes_per_pixel;
    size_t total;
    bool success = false;

    if (out_buf && out_buf_size > 0)
        out_buf[0] = '\0';

    if (!pWilds || IS_NULLSTR(png_path))
    {
        if (out_buf && out_buf_size > 0)
            snprintf(out_buf, out_buf_size, "Usage: wildgen export elevation <png_filename.png>");
        return false;
    }

    if (pWilds->map_size_x < 1 || pWilds->map_size_y < 1)
    {
        if (out_buf && out_buf_size > 0)
            snprintf(out_buf, out_buf_size, "Wilderness dimensions are invalid");
        return false;
    }

    if (!wildgen_build_image_path(pWilds, png_path, resolved_path, sizeof(resolved_path), out_buf, out_buf_size))
        return false;

    width = pWilds->map_size_x;
    height = pWilds->map_size_y;
    total = (size_t)width * (size_t)height;

    /* Determine export bit depth: 16-bit if forced, otherwise 8-bit */
    write_depth = (pWilds->wildgen_bitdepth == 16) ? 16 : 8;
    bytes_per_pixel = (write_depth == 16) ? 2 : 1;

    pixels = malloc(total * (size_t)bytes_per_pixel);
    if (!pixels)
    {
        if (out_buf && out_buf_size > 0)
            snprintf(out_buf, out_buf_size, "Out of memory preparing elevation PNG buffer");
        return false;
    }

    if (write_depth == 16)
    {
        /* Fill with 16-bit big-endian default elevation value */
        int elev = URANGE(0, pWilds->default_elevation, 65535);
        unsigned char hi = (unsigned char)((elev >> 8) & 0xFF);
        unsigned char lo = (unsigned char)(elev & 0xFF);
        size_t i;

        for (i = 0; i < total; i++)
        {
            pixels[i * 2 + 0] = hi;
            pixels[i * 2 + 1] = lo;
        }

        success = wildgen_write_png_gray16(resolved_path, pixels, width, height, out_buf, out_buf_size);
    }
    else
    {
        /* Fill with 8-bit clamped default elevation value */
        unsigned char elev = (unsigned char)URANGE(0, pWilds->default_elevation, 255);

        memset(pixels, elev, total);

        success = wildgen_write_png_gray(resolved_path, pixels, width, height, out_buf, out_buf_size);
    }

    if (success && out_buf && out_buf_size > 0)
    {
        snprintf(out_buf, out_buf_size,
            "Wildgen elevation export wrote %dx%d %d-bit grayscale PNG to %s (base elevation: %d)",
            width, height, write_depth, png_path, pWilds->default_elevation);
    }

    free(pixels);
    return success;
}

bool wilds_wildgen_export_image(WILDS_DATA *pWilds, const char *png_path, char *out_buf, size_t out_buf_size)
{
    return wildgen_export_image_internal(pWilds, png_path, false, out_buf, out_buf_size);
}

bool wilds_wildgen_export_effective_image(WILDS_DATA *pWilds, const char *png_path, char *out_buf, size_t out_buf_size)
{
    return wildgen_export_image_internal(pWilds, png_path, true, out_buf, out_buf_size);
}

bool wilds_wildgen_export_elevation(WILDS_DATA *pWilds, const char *png_path, char *out_buf, size_t out_buf_size)
{
    return wildgen_export_elevation_internal(pWilds, png_path, out_buf, out_buf_size);
}

static bool wildgen_validate_elevation_grid(const WILDS_DATA *pWilds, const WILDS_WILDGEN_JOB *job, char *err, size_t err_size)
{
    int block_w = 0;
    int block_h = 0;
    int row, col;
    int min_elev = 65535;
    int max_elev = 0;

    if (!job->validate_elevation_grid)
        return true;

    if (job->grid_rows < 1 || job->grid_cols < 1)
    {
        snprintf(err, err_size, "Elevation grid requires valid rows/cols");
        return false;
    }

    if (!pWilds)
    {
        snprintf(err, err_size, "Wilderness is not available for elevation validation");
        return false;
    }

    block_w = (job->expected_tile_width > 0) ? job->expected_tile_width : (job->map_size_x / job->grid_cols);
    block_h = (job->expected_tile_height > 0) ? job->expected_tile_height : (job->map_size_y / job->grid_rows);

    for (row = 0; row < job->grid_rows; row++)
    {
        for (col = 0; col < job->grid_cols; col++)
        {
            char elev_path[MSL];
            unsigned char *pixels;
            int width = 0;
            int height = 0;
            int elev_bit_depth = 8;
            size_t total;
            size_t i;

            if (!wildgen_build_grid_path((WILDS_DATA *)pWilds, job->elevation_source_name,
                    row, col, job->grid_rows, job->grid_cols,
                    elev_path, sizeof(elev_path), err, err_size))
                return false;

            if (!wildgen_verify_png_signature(elev_path, err, err_size))
                return false;

            if (!wildgen_probe_png_info(elev_path, NULL, NULL, &elev_bit_depth, err, err_size))
                return false;

            if (elev_bit_depth == 16)
                pixels = wildgen_read_png_gray16(elev_path, &width, &height, err, err_size);
            else
                pixels = wildgen_read_png_gray(elev_path, &width, &height, err, err_size);

            if (!pixels)
                return false;

            if (width != block_w || height != block_h)
            {
                free(pixels);
                snprintf(err, err_size, "Elevation tile dimensions mismatch at (%d,%d): got %dx%d expected %dx%d",
                    row, col, width, height, block_w, block_h);
                return false;
            }

            total = (size_t)width * (size_t)height;
            if (elev_bit_depth == 16)
            {
                for (i = 0; i < total; i++)
                {
                    int v = (int)(pixels[i * 2] << 8 | pixels[i * 2 + 1]);
                    if (v < min_elev) min_elev = v;
                    if (v > max_elev) max_elev = v;
                }
            }
            else
            {
                for (i = 0; i < total; i++)
                {
                    int v = pixels[i];
                    if (v < min_elev) min_elev = v;
                    if (v > max_elev) max_elev = v;
                }
            }

            free(pixels);
        }
    }

    snprintf(err, err_size, "Elevation grid validated (range %d..%d); elevation layer application is not yet enabled", min_elev, max_elev);
    return true;
}

static void wildgen_push_result(WILDS_WILDGEN_RESULT *result)
{
    if (!result)
        return;

    result->next = NULL;
    if (!wildgen_result_tail)
    {
        wildgen_result_head = result;
        wildgen_result_tail = result;
    }
    else
    {
        wildgen_result_tail->next = result;
        wildgen_result_tail = result;
    }
}

static void *wildgen_worker_func(void *arg)
{
    (void)arg;

    while (true)
    {
        WILDS_WILDGEN_JOB *job = NULL;
        WILDS_WILDGEN_RESULT *result;
        unsigned char *pixels = NULL;
        char *tiles = NULL;
        int width = 0;
        int height = 0;
        int unknown_pixels = 0;
        bool success = false;
        WILDS_DATA *job_wilds = NULL;
        char message[MSL];

        pthread_mutex_lock(&wildgen_mutex);
        while (!wildgen_stop_requested && !wildgen_queue_head)
            pthread_cond_wait(&wildgen_cond, &wildgen_mutex);

        if (wildgen_stop_requested)
        {
            pthread_mutex_unlock(&wildgen_mutex);
            break;
        }

        job = wildgen_queue_head;
        wildgen_queue_head = job->next;
        if (!wildgen_queue_head)
            wildgen_queue_tail = NULL;

        wildgen_set_status(job->wilds_uid, WILDGEN_STATE_RUNNING, job->job_id, "Decoding PNG with libpng");
        pthread_mutex_unlock(&wildgen_mutex);

        job_wilds = get_wilds_from_uid(NULL, job->wilds_uid);
        if (!job_wilds)
        {
            snprintf(message, sizeof(message), "Wilderness no longer loaded (uid %ld)", job->wilds_uid);
            result = calloc(1, sizeof(*result));
            if (result)
            {
                result->job_id = job->job_id;
                result->wilds_uid = job->wilds_uid;
                result->success = false;
                result->tiles = NULL;
                result->width = 0;
                result->height = 0;
                snprintf(result->message, sizeof(result->message), "%s", message);
                pthread_mutex_lock(&wildgen_mutex);
                wildgen_push_result(result);
                pthread_mutex_unlock(&wildgen_mutex);
            }
            free(job);
            continue;
        }

        if (!job->use_grid)
        {
            bool is_16bit = (job->source_bit_depth == 16);
            int bpp = is_16bit ? 6 : 3;

            if (is_16bit)
                pixels = wildgen_read_png_rgb16(job->png_path, &width, &height, message, sizeof(message));
            else
                pixels = wildgen_read_png_rgb(job->png_path, &width, &height, message, sizeof(message));

            if (!pixels)
            {
                /* message already populated by reader */
            }
            else if (width != job->map_size_x || height != job->map_size_y)
            {
                snprintf(message, sizeof(message),
                    "PNG dimensions %dx%d do not match wilds %dx%d",
                    width, height, job->map_size_x, job->map_size_y);
            }
            else
            {
                size_t total = (size_t)width * (size_t)height;
                tiles = malloc(total);
                if (!tiles)
                {
                    snprintf(message, sizeof(message), "Out of memory while preparing imported map tiles");
                }
                else
                {
                    wildgen_convert_pixels_to_tiles(job, pixels, width, height, bpp, tiles, &unknown_pixels);
                    snprintf(message, sizeof(message),
                        "Converted %zu pixels (%d unmatched colors used default tile '%c')",
                        total, unknown_pixels, job->default_tile);
                    success = true;
                }
            }
        }
        else
        {
            bool is_16bit = (job->source_bit_depth == 16);
            int bpp = is_16bit ? 6 : 3;
            int block_w;
            int block_h;
            int row;
            int col;
            size_t total;
            int validated_unknown = 0;

            width = job->map_size_x;
            height = job->map_size_y;

            if (job->grid_rows < 1 || job->grid_cols < 1)
            {
                snprintf(message, sizeof(message), "Invalid grid dimensions");
            }
            else if ((job->map_size_x % job->grid_cols) != 0 || (job->map_size_y % job->grid_rows) != 0)
            {
                snprintf(message, sizeof(message), "Grid does not divide wilderness dimensions evenly");
            }
            else
            {
                block_w = (job->expected_tile_width > 0) ? job->expected_tile_width : (job->map_size_x / job->grid_cols);
                block_h = (job->expected_tile_height > 0) ? job->expected_tile_height : (job->map_size_y / job->grid_rows);

                if ((size_t)block_w * (size_t)job->grid_cols != (size_t)job->map_size_x ||
                    (size_t)block_h * (size_t)job->grid_rows != (size_t)job->map_size_y)
                {
                    snprintf(message, sizeof(message),
                        "Configured grid tile size %dx%d with grid %dx%d does not match wilderness %dx%d",
                        block_w, block_h, job->grid_rows, job->grid_cols, job->map_size_x, job->map_size_y);
                }
                else
                {
                    total = (size_t)width * (size_t)height;
                    tiles = malloc(total);

                    if (!tiles)
                    {
                        snprintf(message, sizeof(message), "Out of memory while preparing grid import tiles");
                    }
                    else
                    {
                        memset(tiles, job->default_tile, total);

                        for (row = 0; row < job->grid_rows && !success; row++)
                        {
                            for (col = 0; col < job->grid_cols; col++)
                            {
                                char part_path[MSL];
                                int pw = 0;
                                int ph = 0;
                                int part_unknown = 0;
                                size_t y;

                                if (!wildgen_build_grid_path(job_wilds, job->source_name,
                                        row, col, job->grid_rows, job->grid_cols,
                                        part_path, sizeof(part_path), message, sizeof(message)))
                                    break;

                                if (!wildgen_verify_png_signature(part_path, message, sizeof(message)))
                                    break;

                                if (is_16bit)
                                    pixels = wildgen_read_png_rgb16(part_path, &pw, &ph, message, sizeof(message));
                                else
                                    pixels = wildgen_read_png_rgb(part_path, &pw, &ph, message, sizeof(message));
                                if (!pixels)
                                    break;

                                if (pw != block_w || ph != block_h)
                                {
                                    free(pixels);
                                    pixels = NULL;
                                    snprintf(message, sizeof(message),
                                        "Grid image has %dx%d, expected %dx%d",
                                        pw, ph, block_w, block_h);
                                    break;
                                }

                                for (y = 0; y < (size_t)ph; y++)
                                {
                                    char *dest = tiles + ((size_t)(row * block_h + (int)y) * (size_t)width) + (size_t)(col * block_w);
                                    const unsigned char *src = pixels + (y * (size_t)pw * (size_t)bpp);
                                    int x;
                                    for (x = 0; x < pw; x++)
                                    {
                                        uint16_t pr, pg, pb;
                                        char tile;

                                        wildgen_extract_rgb16(src, (size_t)x, bpp, &pr, &pg, &pb);
                                        tile = wildgen_lookup_tile(job, pr, pg, pb);
                                        if (tile == job->default_tile)
                                        {
                                            int known = 0;
                                            int c;
                                            for (c = 0; c < job->color_count; c++)
                                            {
                                                if (job->colors[c].r == pr && job->colors[c].g == pg && job->colors[c].b == pb)
                                                {
                                                    known = 1;
                                                    break;
                                                }
                                            }
                                            if (!known)
                                                part_unknown++;
                                        }
                                        dest[x] = tile;
                                    }
                                }

                                validated_unknown += part_unknown;
                                free(pixels);
                                pixels = NULL;
                            }

                            if (row == job->grid_rows - 1)
                                success = true;
                        }

                        if (success)
                        {
                            if (job->validate_elevation_grid)
                            {
                                char elev_message[MSL];
                                if (!wildgen_validate_elevation_grid(job_wilds, job, elev_message, sizeof(elev_message)))
                                {
                                    success = false;
                                    snprintf(message, sizeof(message), "%s", elev_message);
                                }
                                else
                                {
                                    snprintf(message, sizeof(message),
                                        "Converted grid %dx%d (%d unmatched colors used default tile '%c'). %.512s",
                                        job->grid_rows, job->grid_cols, validated_unknown, job->default_tile, elev_message);
                                }
                            }
                            else
                            {
                                snprintf(message, sizeof(message),
                                    "Converted grid %dx%d (%d unmatched colors used default tile '%c')",
                                    job->grid_rows, job->grid_cols, validated_unknown, job->default_tile);
                            }
                        }
                    }
                }
            }
        }

        if (pixels)
            free(pixels);

        result = calloc(1, sizeof(*result));
        if (result)
        {
            result->job_id = job->job_id;
            result->wilds_uid = job->wilds_uid;
            result->success = success;
            result->tiles = success ? tiles : NULL;
            result->width = width;
            result->height = height;
            snprintf(result->message, sizeof(result->message), "%s", message);
        }
        else if (tiles)
        {
            free(tiles);
        }

        pthread_mutex_lock(&wildgen_mutex);
        if (result)
            wildgen_push_result(result);
        else
            wildgen_set_status(job->wilds_uid, WILDGEN_STATE_FAILED, job->job_id, "Failed to allocate job result");
        pthread_mutex_unlock(&wildgen_mutex);

        free(job);
    }

    return NULL;
}

bool wilds_wildgen_init(void)
{
    int result;

    pthread_mutex_lock(&wildgen_mutex);
    if (wildgen_running)
    {
        pthread_mutex_unlock(&wildgen_mutex);
        return true;
    }

    wildgen_stop_requested = false;
    result = pthread_create(&wildgen_worker_thread, NULL, wildgen_worker_func, NULL);
    if (result != 0)
    {
        pthread_mutex_unlock(&wildgen_mutex);
        perrf(LOG_ERROR, "wilds_wildgen_init: pthread_create failed (%d)", result);
        return false;
    }

    wildgen_running = true;
    pthread_mutex_unlock(&wildgen_mutex);
    return true;
}

void wilds_wildgen_shutdown(void)
{
    WILDS_WILDGEN_JOB *job, *job_next;
    WILDS_WILDGEN_RESULT *result, *result_next;
    WILDS_WILDGEN_STATUS_REC *status, *status_next;
    bool should_join;

    pthread_mutex_lock(&wildgen_mutex);
    should_join = wildgen_running;
    wildgen_stop_requested = true;
    pthread_cond_signal(&wildgen_cond);
    pthread_mutex_unlock(&wildgen_mutex);

    if (should_join)
        pthread_join(wildgen_worker_thread, NULL);

    pthread_mutex_lock(&wildgen_mutex);
    for (job = wildgen_queue_head; job; job = job_next)
    {
        job_next = job->next;
        free(job);
    }
    wildgen_queue_head = NULL;
    wildgen_queue_tail = NULL;

    for (result = wildgen_result_head; result; result = result_next)
    {
        result_next = result->next;
        if (result->tiles)
            free(result->tiles);
        free(result);
    }
    wildgen_result_head = NULL;
    wildgen_result_tail = NULL;

    for (status = wildgen_status_head; status; status = status_next)
    {
        status_next = status->next;
        free(status);
    }
    wildgen_status_head = NULL;

    wildgen_running = false;
    wildgen_stop_requested = false;
    pthread_mutex_unlock(&wildgen_mutex);
}

bool wilds_wildgen_enqueue(WILDS_DATA *pWilds, const char *png_path, char *err_buf, size_t err_buf_size)
{
    WILDS_WILDGEN_JOB *job;
    WILDS_WILDGEN_STATUS_REC *status;
    int png_width = 0;
    int png_height = 0;
    int png_bit_depth = 8;

    if (err_buf && err_buf_size > 0)
        err_buf[0] = '\0';

    if (!pWilds || IS_NULLSTR(png_path))
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Usage: wildgen import <png_filename.png>");
        return false;
    }

    if (!wilds_wildgen_init())
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Wildgen worker failed to initialize");
        return false;
    }

    job = calloc(1, sizeof(*job));
    if (!job)
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Out of memory creating wildgen job");
        return false;
    }

    snprintf(job->source_name, sizeof(job->source_name), "%s", png_path);

    if (!wildgen_build_image_path(pWilds, png_path, job->png_path, sizeof(job->png_path), err_buf, err_buf_size))
    {
        free(job);
        return false;
    }

    if (!wildgen_verify_png_signature(job->png_path, err_buf, err_buf_size))
    {
        free(job);
        return false;
    }

    /* Probe PNG dimensions and bit depth before committing */
    if (!wildgen_probe_png_info(job->png_path, &png_width, &png_height, &png_bit_depth, err_buf, err_buf_size))
    {
        free(job);
        return false;
    }

    /* Auto-allocate map if wilderness has no map yet */
    if (pWilds->map_size_x < 1 || pWilds->map_size_y < 1)
    {
        if (!wildgen_auto_allocate_map(pWilds, png_width, png_height, err_buf, err_buf_size))
        {
            free(job);
            return false;
        }
    }
    else if (png_width != pWilds->map_size_x || png_height != pWilds->map_size_y)
    {
        free(job);
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size,
                "PNG dimensions %dx%d do not match wilderness map %dx%d",
                png_width, png_height, pWilds->map_size_x, pWilds->map_size_y);
        return false;
    }

    job->wilds_uid = pWilds->uid;
    job->map_size_x = pWilds->map_size_x;
    job->map_size_y = pWilds->map_size_y;
    job->default_tile = pWilds->cDefaultTerrain;
    job->use_grid = false;
    job->grid_rows = 1;
    job->grid_cols = 1;
    job->expected_tile_width = 0;
    job->expected_tile_height = 0;
    job->validate_elevation_grid = false;
    job->source_bit_depth = wildgen_resolve_bit_depth(png_bit_depth, pWilds->wildgen_bitdepth);

    job->color_count = wildgen_collect_colors(pWilds, job->colors, (int)(sizeof(job->colors) / sizeof(job->colors[0])));

    if (job->color_count < 1)
    {
        free(job);
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "No terrain wildgen colors are configured. Set with 'terrain <token> wildcolor #RRGGBB'.");
        return false;
    }

    pthread_mutex_lock(&wildgen_mutex);
    status = wildgen_find_status(pWilds->uid, true);
    if (status && (status->state == WILDGEN_STATE_QUEUED || status->state == WILDGEN_STATE_RUNNING || status->state == WILDGEN_STATE_APPLYING))
    {
        pthread_mutex_unlock(&wildgen_mutex);
        free(job);
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "A wildgen import is already active for this wilderness");
        return false;
    }

    job->job_id = ++wildgen_next_job_id;
    if (!wildgen_queue_tail)
    {
        wildgen_queue_head = job;
        wildgen_queue_tail = job;
    }
    else
    {
        wildgen_queue_tail->next = job;
        wildgen_queue_tail = job;
    }

    wildgen_set_status(pWilds->uid, WILDGEN_STATE_QUEUED, job->job_id, "Queued for threaded wildgen import (locked wilderness images dir)");
    pthread_cond_signal(&wildgen_cond);
    pthread_mutex_unlock(&wildgen_mutex);

    return true;
}

bool wilds_wildgen_enqueue_grid(WILDS_DATA *pWilds, const char *terrain_base, int rows, int cols, const char *elevation_base, char *err_buf, size_t err_buf_size)
{
    WILDS_WILDGEN_JOB *job;
    WILDS_WILDGEN_STATUS_REC *status;
    char sample_path[MSL];
    int png_bit_depth = 8;

    if (err_buf && err_buf_size > 0)
        err_buf[0] = '\0';

    if (!pWilds || IS_NULLSTR(terrain_base) || rows < 1 || cols < 1)
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Usage: wildgen importgrid <terrain_base> <rows> <cols> [elevation_base]");
        return false;
    }

    if (!wilds_wildgen_init())
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Wildgen worker failed to initialize");
        return false;
    }

    job = calloc(1, sizeof(*job));
    if (!job)
    {
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "Out of memory creating wildgen grid job");
        return false;
    }

    job->use_grid = true;
    job->grid_rows = rows;
    job->grid_cols = cols;
    job->expected_tile_width = UMAX(0, pWilds->wildgen_tile_width);
    job->expected_tile_height = UMAX(0, pWilds->wildgen_tile_height);
    snprintf(job->source_name, sizeof(job->source_name), "%s", terrain_base);

    if (!wildgen_build_grid_path(pWilds, terrain_base, 0, 0, rows, cols, sample_path, sizeof(sample_path), err_buf, err_buf_size) ||
        !wildgen_verify_png_signature(sample_path, err_buf, err_buf_size))
    {
        free(job);
        return false;
    }

    /* Probe first tile's bit depth to determine 8-bit vs 16-bit processing */
    {
        int tile_width = 0;
        int tile_height = 0;

        if (!wildgen_probe_png_info(sample_path, &tile_width, &tile_height, &png_bit_depth, err_buf, err_buf_size))
        {
            free(job);
            return false;
        }

        /* Auto-allocate map if wilderness has no map yet, computing from tile * grid */
        if (pWilds->map_size_x < 1 || pWilds->map_size_y < 1)
        {
            int total_width = tile_width * cols;
            int total_height = tile_height * rows;

            if (!wildgen_auto_allocate_map(pWilds, total_width, total_height, err_buf, err_buf_size))
            {
                free(job);
                return false;
            }

            /* Auto-set tile size from probed dimensions */
            if (pWilds->wildgen_tile_width < 1 || pWilds->wildgen_tile_height < 1)
            {
                pWilds->wildgen_tile_width = tile_width;
                pWilds->wildgen_tile_height = tile_height;
                job->expected_tile_width = tile_width;
                job->expected_tile_height = tile_height;
            }
        }
    }

    if (!IS_NULLSTR(elevation_base))
    {
        job->validate_elevation_grid = true;
        snprintf(job->elevation_source_name, sizeof(job->elevation_source_name), "%s", elevation_base);

        if (!wildgen_build_grid_path(pWilds, elevation_base, 0, 0, rows, cols, sample_path, sizeof(sample_path), err_buf, err_buf_size) ||
            !wildgen_verify_png_signature(sample_path, err_buf, err_buf_size))
        {
            free(job);
            return false;
        }
    }

    job->wilds_uid = pWilds->uid;
    job->map_size_x = pWilds->map_size_x;
    job->map_size_y = pWilds->map_size_y;
    job->default_tile = pWilds->cDefaultTerrain;
    job->source_bit_depth = wildgen_resolve_bit_depth(png_bit_depth, pWilds->wildgen_bitdepth);

    job->color_count = wildgen_collect_colors(pWilds, job->colors, (int)(sizeof(job->colors) / sizeof(job->colors[0])));

    if (job->color_count < 1)
    {
        free(job);
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "No terrain wildgen colors are configured. Set with 'terrain <token> wildcolor #RRGGBB'.");
        return false;
    }

    pthread_mutex_lock(&wildgen_mutex);
    status = wildgen_find_status(pWilds->uid, true);
    if (status && (status->state == WILDGEN_STATE_QUEUED || status->state == WILDGEN_STATE_RUNNING || status->state == WILDGEN_STATE_APPLYING))
    {
        pthread_mutex_unlock(&wildgen_mutex);
        free(job);
        if (err_buf && err_buf_size > 0)
            snprintf(err_buf, err_buf_size, "A wildgen import is already active for this wilderness");
        return false;
    }

    job->job_id = ++wildgen_next_job_id;
    if (!wildgen_queue_tail)
    {
        wildgen_queue_head = job;
        wildgen_queue_tail = job;
    }
    else
    {
        wildgen_queue_tail->next = job;
        wildgen_queue_tail = job;
    }

    wildgen_set_status(pWilds->uid, WILDGEN_STATE_QUEUED, job->job_id,
        job->validate_elevation_grid
            ? "Queued threaded terrain grid import + elevation grid validation"
            : "Queued threaded terrain grid import");
    pthread_cond_signal(&wildgen_cond);
    pthread_mutex_unlock(&wildgen_mutex);

    return true;
}

void wilds_wildgen_status(WILDS_DATA *pWilds, char *buf, size_t buf_size)
{
    WILDS_WILDGEN_STATUS_REC *status;

    if (!buf || buf_size < 2)
        return;

    if (!pWilds)
    {
        snprintf(buf, buf_size, "No wilderness selected");
        return;
    }

    pthread_mutex_lock(&wildgen_mutex);
    status = wildgen_find_status(pWilds->uid, false);
    if (!status)
        snprintf(buf, buf_size, "wildgen: idle");
    else if (status->message[0] != '\0')
        snprintf(buf, buf_size, "wildgen: %s (job %ld) - %s", wildgen_state_name(status->state), status->job_id, status->message);
    else
        snprintf(buf, buf_size, "wildgen: %s (job %ld)", wildgen_state_name(status->state), status->job_id);
    pthread_mutex_unlock(&wildgen_mutex);
}

void wilds_wildgen_pulse(void)
{
    WILDS_WILDGEN_RESULT *result;

    while (true)
    {
        pthread_mutex_lock(&wildgen_mutex);
        result = wildgen_result_head;
        if (!result)
        {
            pthread_mutex_unlock(&wildgen_mutex);
            break;
        }

        wildgen_result_head = result->next;
        if (!wildgen_result_head)
            wildgen_result_tail = NULL;

        wildgen_set_status(result->wilds_uid, WILDGEN_STATE_APPLYING, result->job_id, "Applying imported map to wilderness");
        pthread_mutex_unlock(&wildgen_mutex);

        if (result->success)
        {
            WILDS_DATA *pWilds = get_wilds_from_uid(NULL, result->wilds_uid);
            if (!pWilds)
            {
                pthread_mutex_lock(&wildgen_mutex);
                wildgen_set_status(result->wilds_uid, WILDGEN_STATE_FAILED, result->job_id,
                    "Import finished but wilderness is no longer loaded");
                pthread_mutex_unlock(&wildgen_mutex);
            }
            else if (!wilds_apply_static_tiles(pWilds, result->tiles, result->width, result->height))
            {
                pthread_mutex_lock(&wildgen_mutex);
                wildgen_set_status(result->wilds_uid, WILDGEN_STATE_FAILED, result->job_id,
                    "Failed to apply imported tiles to wilderness");
                pthread_mutex_unlock(&wildgen_mutex);
            }
            else
            {
                if (pWilds->pArea)
                    SET_BIT(pWilds->pArea->area_flags, AREA_CHANGED);

                pthread_mutex_lock(&wildgen_mutex);
                wildgen_set_status(result->wilds_uid, WILDGEN_STATE_SUCCEEDED, result->job_id, result->message);
                pthread_mutex_unlock(&wildgen_mutex);
            }
        }
        else
        {
            pthread_mutex_lock(&wildgen_mutex);
            wildgen_set_status(result->wilds_uid, WILDGEN_STATE_FAILED, result->job_id, result->message);
            pthread_mutex_unlock(&wildgen_mutex);
        }

        if (result->tiles)
            free(result->tiles);
        free(result);
    }
}
