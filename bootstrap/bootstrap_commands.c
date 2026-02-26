/**
 * bootstrap_commands.c - Command bootstrap seeding
 *
 * Creates data/system/commands.json from committed bootstrap_data.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "../merc.h"
#include "bootstrap_internal.h"

static bool copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[8192];
    size_t n;

    if (!in)
        return false;

    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return false;
        }
    }

    fclose(in);
    fclose(out);
    return true;
}

/**
 * create_commands_json - Seed commands.json from committed bootstrap data
 *
 * Searches known bootstrap_data locations for system/commands.json and copies
 * it to data/system/commands.json.
 *
 * @return true on success
 */
bool create_commands_json(void)
{
    const char *workspace = getenv("GITHUB_WORKSPACE");
    char workspace_candidate_a[1024];
    char workspace_candidate_b[1024];
    const char *source = NULL;
    const char *candidates[8];

    workspace_candidate_a[0] = '\0';
    workspace_candidate_b[0] = '\0';

    if (workspace && workspace[0]) {
        snprintf(workspace_candidate_a, sizeof(workspace_candidate_a),
                 "%s/bootstrap/bootstrap_data/system/commands.json", workspace);
        snprintf(workspace_candidate_b, sizeof(workspace_candidate_b),
                 "%s/src/bootstrap/bootstrap_data/system/commands.json", workspace);
    }

    candidates[0] = "bootstrap/bootstrap_data/system/commands.json";
    candidates[1] = "src/bootstrap/bootstrap_data/system/commands.json";
    candidates[2] = workspace_candidate_a[0] ? workspace_candidate_a : NULL;
    candidates[3] = workspace_candidate_b[0] ? workspace_candidate_b : NULL;
    candidates[4] = "/sentience/src/bootstrap/bootstrap_data/system/commands.json";
    candidates[5] = NULL;

    for (int i = 0; candidates[i] != NULL; i++) {
        if (file_exists(candidates[i])) {
            source = candidates[i];
            break;
        }
    }

    if (!source) {
        fprintf(stderr, "create_commands_json: bootstrap commands.json not found\n");
        return false;
    }

    if (!copy_file(source, "data/system/commands.json")) {
        fprintf(stderr, "create_commands_json: failed copying %s -> data/system/commands.json (%s)\n",
                source, strerror(errno));
        return false;
    }

    printf("  Seeded commands.json from %s\n", source);
    return true;
}
