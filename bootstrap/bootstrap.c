/**
 * bootstrap/bootstrap.c - Main bootstrap orchestration for Sentience MUD
 *
 * Coordinates the bootstrap process: file creation, account setup, verification.
 * Individual operations are handled by specialized modules.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include "../merc.h"
#include "../account/auth_sodium.h"
#include "bootstrap.h"
#include "bootstrap_internal.h"

/* Global variables */
bool bootstrap_mode = false;
bool bootstrap_auto = false;
bool bootstrap_ci_fixtures = false;
char *bootstrap_username = NULL;
char *bootstrap_email = NULL;
char *bootstrap_password = NULL;
char *bootstrap_root = NULL;

/**
 * detect_bootstrap_mode - Check command-line args for bootstrap flags
 *
 * @param argc  Argument count
 * @param argv  Argument vector
 */
void detect_bootstrap_mode(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-bootstrap")) {
            bootstrap_mode = true;
        } else if (!strncmp(argv[i], "--bootstrap-auto", 16)) {
            bootstrap_mode = true;
            bootstrap_auto = true;
        } else if (!strncmp(argv[i], "--bootstrap-username=", 21)) {
            bootstrap_username = argv[i] + 21;
        } else if (!strncmp(argv[i], "--bootstrap-email=", 18)) {
            bootstrap_email = argv[i] + 18;
        } else if (!strncmp(argv[i], "--bootstrap-password=", 21)) {
            bootstrap_password = argv[i] + 21;
        } else if (!strcmp(argv[i], "--bootstrap-fixtures=ci") ||
                   !strcmp(argv[i], "--bootstrap-ci-fixtures")) {
            bootstrap_ci_fixtures = true;
        } else if (!strncmp(argv[i], "--bootstrap-root=", 17)) {
            bootstrap_root = argv[i] + 17;
        }
    }
}

static bool bootstrap_enter_root(void)
{
    if (!bootstrap_root || !bootstrap_root[0]) {
        return true;
    }

    set_runtime_game_root(bootstrap_root);

    struct stat st;
    if (stat(bootstrap_root, &st) != 0) {
        if (mkdir(bootstrap_root, 0755) != 0) {
            fprintf(stderr, "Failed to create bootstrap root '%s': %s\n",
                    bootstrap_root, strerror(errno));
            return false;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Bootstrap root is not a directory: %s\n", bootstrap_root);
        return false;
    }

    if (chdir(bootstrap_root) != 0) {
        fprintf(stderr, "Failed to enter bootstrap root '%s': %s\n",
                bootstrap_root, strerror(errno));
        return false;
    }

    printf("Using bootstrap root: %s\n", bootstrap_root);
    return true;
}

/**
 * check_bootstrap_needed - Determine if bootstrap is needed
 *
 * @return  true if bootstrap should run
 */
bool check_bootstrap_needed(void)
{
    if (!file_exists("data/system/gconfig.rc")) {
        printf("Missing: data/system/gconfig.rc\n");
        return true;
    }

    if (!file_exists("data/world/area.lst")) {
        printf("Missing: data/world/area.lst\n");
        return true;
    }

    if (!file_exists("area/limbo.json")) {
        printf("Missing: area/limbo.json\n");
        return true;
    }

    if (!file_exists("data/system/commands.json")) {
        printf("Missing: data/system/commands.json\n");
        return true;
    }

    /* Check if any race files exist */
    DIR *dir = opendir("data/races/");
    if (!dir) {
        printf("Missing: data/races/ directory\n");
        return true;
    }

    struct dirent *ent;
    int race_count = 0;
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 5 && strcmp(ent->d_name + len - 5, ".json") == 0) {
            race_count++;
        }
    }
    closedir(dir);

    if (race_count == 0) {
        printf("Missing: No race files in data/races/\n");
        return true;
    }

    return false;
}

/**
 * create_minimal_data_files - Create all required minimal files
 *
 * @return  true on success
 */
bool create_minimal_data_files(void)
{
    printf("\nCreating minimal data files...\n");

    if (!ensure_directory_structure()) {
        fprintf(stderr, "Failed to create directory structure\n");
        return false;
    }
    printf("  Directory structure verified.\n");

    /* Create each required file */
    struct {
        const char *name;
        const char *path;
        bool (*create_func)(void);
    } files[] = {
        {"gconfig.rc", "data/system/gconfig.rc", create_gconfig_rc},
        {"area.lst", "data/world/area.lst", create_area_lst},
        {"limbo.json", "area/limbo.json", create_limbo_area},
        {"game_settings.json", "data/system/game_settings.json", create_game_settings},
        {"commands.json", "data/system/commands.json", create_commands_json},
        {NULL, NULL, NULL}
    };

    for (int i = 0; files[i].name; i++) {
        if (file_exists(files[i].path)) {
            printf("  %s already exists... SKIP\n", files[i].name);
        } else {
            printf("  Creating %s... ", files[i].name);
            if (!files[i].create_func()) {
                fprintf(stderr, "FAILED\n");
                return false;
            }
            printf("OK\n");
        }
    }

    /* Race files need special handling */
    printf("  Checking race files... ");
    if (!create_human_race()) {
        fprintf(stderr, "FAILED\n");
        return false;
    }
    printf("OK\n");

    /* Reserved entities - creates objects/mobs in limbo and reserved.json */
    if (!file_exists("data/system/reserved.json")) {
        printf("  Creating reserved entities in limbo... ");
        if (!create_reserved_entities()) {
            fprintf(stderr, "FAILED\n");
            return false;
        }
        printf("OK\n");
    } else {
        printf("  reserved.json already exists... SKIP\n");
    }

    if (bootstrap_ci_fixtures) {
        printf("  Creating CI fixture areas... ");
        if (!create_ci_test_fixture_areas()) {
            fprintf(stderr, "FAILED\n");
            return false;
        }
        printf("OK\n");
    }

    printf("Data files created successfully.\n");
    return true;
}

/**
 * interactive_account_setup - Interactive staff account creation
 *
 * @return  true on success
 */
static void free_credentials(char *username, char *email, char *password, char *confirm)
{
    if (username) free(username);
    if (email) free(email);
    if (password) {
        memset(password, 0, strlen(password));
        free(password);
    }
    if (confirm) {
        memset(confirm, 0, strlen(confirm));
        free(confirm);
    }
}

bool interactive_account_setup(void)
{
    char *username = NULL;
    char *email = NULL;
    char *password = NULL;
    char *confirm = NULL;

    printf("\n=== First Staff Account Setup ===\n\n");

    username = prompt_username();
    if (!username) {
        fprintf(stderr, "Failed to get username\n");
        free_credentials(username, email, password, confirm);
        return false;
    }

    if (player_exists(username)) {
        fprintf(stderr, "Account '%s' already exists. Bootstrap cannot overwrite existing accounts.\n", username);
        free_credentials(username, email, password, confirm);
        return false;
    }

    email = prompt_email();
    if (!email) {
        fprintf(stderr, "Failed to get email\n");
        free_credentials(username, email, password, confirm);
        return false;
    }

    password = prompt_password("Enter password (8+ characters): ");
    if (!password) {
        fprintf(stderr, "Failed to get password\n");
        free_credentials(username, email, password, confirm);
        return false;
    }

    confirm = prompt_password("Confirm password: ");
    if (!confirm) {
        fprintf(stderr, "Failed to confirm password\n");
        free_credentials(username, email, password, confirm);
        return false;
    }

    if (strcmp(password, confirm) != 0) {
        fprintf(stderr, "\nPasswords do not match. Bootstrap failed.\n");
        free_credentials(username, email, password, confirm);
        return false;
    }

    if (!bootstrap_create_account_and_character(username, email, password)) {
        fprintf(stderr, "Failed to create account\n");
        free_credentials(username, email, password, confirm);
        return false;
    }

    free_credentials(username, email, password, confirm);
    return true;
}

/**
 * verify_bootstrap_success - Verify bootstrap completed successfully
 *
 * @return  true if bootstrap succeeded
 */
bool verify_bootstrap_success(void)
{
    printf("\nVerifying bootstrap...\n");

    if (!file_exists("data/system/gconfig.rc")) {
        fprintf(stderr, "  gconfig.rc not found\n");
        return false;
    }

    if (!file_exists("data/world/area.lst")) {
        fprintf(stderr, "  area.lst not found\n");
        return false;
    }

    if (!file_exists("area/limbo.json")) {
        fprintf(stderr, "  limbo.json not found\n");
        return false;
    }

    if (!file_exists("data/system/commands.json")) {
        fprintf(stderr, "  commands.json not found\n");
        return false;
    }

    DIR *dir = opendir("data/races/");
    if (!dir) {
        fprintf(stderr, "  data/races/ directory not found\n");
        return false;
    }

    struct dirent *ent;
    int race_count = 0;
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len > 5 && strcmp(ent->d_name + len - 5, ".json") == 0) {
            race_count++;
        }
    }
    closedir(dir);

    if (race_count == 0) {
        fprintf(stderr, "  No race files found\n");
        return false;
    }

    printf("Bootstrap verification successful!\n");
    return true;
}

/**
 * run_bootstrap - Main bootstrap orchestrator
 *
 * @return  0 on success, 1 on failure
 */
int run_bootstrap(void)
{
    printf("\n");
    printf("======================================\n");
    printf("  Bootstrap Mode - Sentience MUD\n");
    printf("======================================\n\n");

    if (!bootstrap_enter_root()) {
        return 1;
    }

    if (bootstrap_ci_fixtures) {
        printf("CI fixture generation enabled.\n");
    }

    /* Initialize libsodium for password hashing */
    if (!init_sodium()) {
        fprintf(stderr, "Failed to initialize libsodium\n");
        return 1;
    }

    bool skip_data_creation = false;

    if (!check_bootstrap_needed()) {
        printf("All required files exist. Bootstrap not needed.\n");
        printf("Use -bootstrap to force re-run if desired.\n");

        if (bootstrap_auto) {
            return 0;
        }

        printf("\nCreate a new staff account anyway? (y/n): ");
        char response[10];
        if (!fgets(response, sizeof(response), stdin)) {
            return 1;
        }
        if (response[0] != 'y' && response[0] != 'Y') {
            printf("Bootstrap cancelled.\n");
            return 0;
        }

        skip_data_creation = true;
    }

    if (!skip_data_creation && !create_minimal_data_files()) {
        fprintf(stderr, "\nBootstrap failed during file creation.\n");
        return 1;
    }

    if (!interactive_account_setup()) {
        fprintf(stderr, "\nBootstrap failed during account setup.\n");
        return 1;
    }

    if (!verify_bootstrap_success()) {
        fprintf(stderr, "\nBootstrap verification failed.\n");
        return 1;
    }

    printf("\n");
    printf("======================================\n");
    printf("  Bootstrap Complete!\n");
    printf("======================================\n");
    printf("\nYou can now start the game normally.\n");
    printf("Your implementor account has been created.\n\n");

    if (bootstrap_ci_fixtures) {
        printf("CI fixture areas were generated and added to data/world/area.lst.\n\n");
    }

    return 0;
}
