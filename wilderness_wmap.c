#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "merc.h"
#include "wilds.h"
#include "wilderness_storage.h"
#include "wilderness_wmap.h"

#define WMAP_MAGIC "WMAP"
#define WMAP_CHECKSUM_LEN 64U

static bool wilderness_wmap_ready = false;

static bool wilderness_wmap_file_exists(const char *path)
{
    struct stat st;

    if (!path || path[0] == '\0')
        return false;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool wilderness_wmap_read_exact(FILE *fp, void *dst, size_t len)
{
    return fp && dst && len > 0 && fread(dst, 1, len, fp) == len;
}

static bool wilderness_wmap_write_exact(FILE *fp, const void *src, size_t len)
{
    return fp && src && len > 0 && fwrite(src, 1, len, fp) == len;
}

bool wilderness_wmap_init(void)
{
    wilderness_wmap_ready = true;
    return true;
}

void wilderness_wmap_shutdown(void)
{
    wilderness_wmap_ready = false;
}

bool wilderness_wmap_load(WILDS_DATA *pWilds)
{
    FILE *fp;
    char path[MSL];
    char magic[4];
    uint32_t version;
    int64_t wilds_uid;
    uint32_t width;
    uint32_t height;
    uint32_t terrain_len;
    uint32_t elevation_len;
    char stored_checksum[WMAP_CHECKSUM_LEN + 1];
    char computed_checksum[65];
    char *tiles;

    if (!wilderness_wmap_ready || !pWilds)
        return false;

    if (!wilderness_storage_build_wmap_path(pWilds, path, sizeof(path)))
        return false;

    if (!wilderness_wmap_file_exists(path))
        return true;

    fp = fopen(path, "rb");
    if (!fp)
    {
        perrf(LOG_ERROR, "wilderness_wmap_load: failed to open %s (errno=%d)", path, errno);
        return false;
    }

    if (!wilderness_wmap_read_exact(fp, magic, sizeof(magic))
    || strncmp(magic, WMAP_MAGIC, sizeof(magic)) != 0
    || !wilderness_wmap_read_exact(fp, &version, sizeof(version))
    || !wilderness_wmap_read_exact(fp, &wilds_uid, sizeof(wilds_uid))
    || !wilderness_wmap_read_exact(fp, &width, sizeof(width))
    || !wilderness_wmap_read_exact(fp, &height, sizeof(height))
    || !wilderness_wmap_read_exact(fp, &terrain_len, sizeof(terrain_len))
    || !wilderness_wmap_read_exact(fp, &elevation_len, sizeof(elevation_len))
    || !wilderness_wmap_read_exact(fp, stored_checksum, WMAP_CHECKSUM_LEN))
    {
        fclose(fp);
        perrf(LOG_ERROR, "wilderness_wmap_load: malformed header in %s", path);
        return false;
    }

    stored_checksum[WMAP_CHECKSUM_LEN] = '\0';

    if (version != WILDERNESS_WMAP_VERSION)
    {
        fclose(fp);
        plogf(LOG_WARN,
            "wilderness_wmap_load: unsupported version %u for wilds uid %ld (file=%s)",
            version,
            pWilds->uid,
            path);
        return true;
    }

    if (wilds_uid != (int64_t)pWilds->uid)
    {
        fclose(fp);
        plogf(LOG_WARN,
            "wilderness_wmap_load: uid mismatch in %s (file=%lld runtime=%ld)",
            path,
            (long long)wilds_uid,
            pWilds->uid);
        return true;
    }

    if (width < 1 || height < 1 || terrain_len != (width * height))
    {
        fclose(fp);
        perrf(LOG_ERROR, "wilderness_wmap_load: invalid dimensions in %s", path);
        return false;
    }

    if (pWilds->map_size_x != (int)width || pWilds->map_size_y != (int)height)
    {
        fclose(fp);
        plogf(LOG_WARN,
            "wilderness_wmap_load: dimension mismatch for wilds uid %ld (file=%ux%u runtime=%dx%d), skipping",
            pWilds->uid,
            width,
            height,
            pWilds->map_size_x,
            pWilds->map_size_y);
        return true;
    }

    tiles = calloc(terrain_len, sizeof(char));
    if (!tiles)
    {
        fclose(fp);
        return false;
    }

    if (!wilderness_wmap_read_exact(fp, tiles, terrain_len))
    {
        free(tiles);
        fclose(fp);
        perrf(LOG_ERROR, "wilderness_wmap_load: short read for terrain payload in %s", path);
        return false;
    }

    if (elevation_len > 0 && fseek(fp, (long)elevation_len, SEEK_CUR) != 0)
    {
        free(tiles);
        fclose(fp);
        perrf(LOG_ERROR, "wilderness_wmap_load: failed to skip elevation payload in %s", path);
        return false;
    }

    fclose(fp);

    wilderness_storage_checksum_hex(tiles, terrain_len, computed_checksum);
    if (!IS_NULLSTR(stored_checksum) && str_cmp(stored_checksum, computed_checksum) != 0)
    {
        plogf(LOG_WARN,
            "wilderness_wmap_load: checksum mismatch for wilds uid %ld (stored=%s current=%s)",
            pWilds->uid,
            stored_checksum,
            computed_checksum);
    }

    if (!wilds_apply_static_tiles(pWilds, tiles, (int)width, (int)height))
    {
        free(tiles);
        perrf(LOG_ERROR, "wilderness_wmap_load: failed to apply tiles for wilds uid %ld", pWilds->uid);
        return false;
    }

    free(tiles);
    return true;
}

bool wilderness_wmap_save(WILDS_DATA *pWilds)
{
    FILE *fp;
    char path[MSL];
    uint32_t version = WILDERNESS_WMAP_VERSION;
    int64_t wilds_uid;
    uint32_t width;
    uint32_t height;
    uint32_t terrain_len;
    uint32_t elevation_len = 0;
    char checksum[65];

    if (!wilderness_wmap_ready || !pWilds)
        return false;

    if (!pWilds->staticmap || pWilds->map_size_x < 1 || pWilds->map_size_y < 1)
        return true;

    if (!wilderness_storage_build_wmap_path(pWilds, path, sizeof(path)))
        return false;

    width = (uint32_t)pWilds->map_size_x;
    height = (uint32_t)pWilds->map_size_y;
    terrain_len = width * height;
    wilds_uid = (int64_t)pWilds->uid;

    wilderness_storage_checksum_hex(pWilds->staticmap, terrain_len, checksum);

    fp = fopen(path, "wb");
    if (!fp)
    {
        perrf(LOG_ERROR, "wilderness_wmap_save: failed to open %s for write (errno=%d)", path, errno);
        return false;
    }

    if (!wilderness_wmap_write_exact(fp, WMAP_MAGIC, 4)
    || !wilderness_wmap_write_exact(fp, &version, sizeof(version))
    || !wilderness_wmap_write_exact(fp, &wilds_uid, sizeof(wilds_uid))
    || !wilderness_wmap_write_exact(fp, &width, sizeof(width))
    || !wilderness_wmap_write_exact(fp, &height, sizeof(height))
    || !wilderness_wmap_write_exact(fp, &terrain_len, sizeof(terrain_len))
    || !wilderness_wmap_write_exact(fp, &elevation_len, sizeof(elevation_len))
    || !wilderness_wmap_write_exact(fp, checksum, WMAP_CHECKSUM_LEN)
    || !wilderness_wmap_write_exact(fp, pWilds->staticmap, terrain_len))
    {
        fclose(fp);
        perrf(LOG_ERROR, "wilderness_wmap_save: write failed for %s", path);
        return false;
    }

    fclose(fp);
    return true;
}
