/**
 * bootstrap_prompts.c - Interactive user input for bootstrap
 *
 * Handles username, email, and password prompts with validation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <unistd.h>
#include "../merc.h"
#include "bootstrap_internal.h"

/* External globals from main bootstrap module */
extern bool bootstrap_auto;
extern char *bootstrap_username;
extern char *bootstrap_email;
extern char *bootstrap_password;

/**
 * validate_username - Check if username is valid
 *
 * Username must be 3-12 alphanumeric characters, start with a letter.
 *
 * @param username  Username to validate
 * @return          true if valid
 */
bool validate_username(const char *username)
{
    size_t len;

    if (!username || !*username)
        return false;

    len = strlen(username);
    if (len < 3 || len > 12)
        return false;

    if (!isalpha(username[0]))
        return false;

    for (size_t i = 0; i < len; i++) {
        if (!isalnum(username[i]))
            return false;
    }

    return true;
}

/**
 * validate_email - Basic email validation
 *
 * @param email  Email to validate
 * @return       true if valid (or empty, since email is optional)
 */
bool validate_email(const char *email)
{
    const char *at_sign;
    const char *dot;

    if (!email || !*email)
        return true;

    at_sign = strchr(email, '@');
    if (!at_sign)
        return false;

    dot = strchr(at_sign, '.');
    if (!dot || dot == at_sign + 1)
        return false;

    return true;
}

/**
 * prompt_username - Interactive username prompt
 *
 * @return  Allocated username string (caller must free), or NULL on failure
 */
char *prompt_username(void)
{
    char buffer[256];

    if (bootstrap_auto && bootstrap_username) {
        if (!validate_username(bootstrap_username)) {
            fprintf(stderr, "Invalid username: %s\n", bootstrap_username);
            return NULL;
        }
        return strdup(bootstrap_username);
    }

    while (true) {
        printf("Enter username for implementor account (3-12 alphanumeric): ");
        fflush(stdout);

        if (!fgets(buffer, sizeof(buffer), stdin))
            return NULL;

        buffer[strcspn(buffer, "\n")] = '\0';

        if (validate_username(buffer)) {
            for (char *p = buffer; *p; p++)
                *p = tolower(*p);

            return strdup(buffer);
        }

        printf("Invalid username. Must be 3-12 alphanumeric characters, starting with a letter.\n");
    }
}

/**
 * prompt_email - Interactive email prompt
 *
 * @return  Allocated email string (caller must free), or NULL on failure
 */
char *prompt_email(void)
{
    char buffer[256];

    if (bootstrap_auto && bootstrap_email) {
        if (!validate_email(bootstrap_email)) {
            fprintf(stderr, "Invalid email: %s\n", bootstrap_email);
            return NULL;
        }
        return strdup(bootstrap_email);
    }

    printf("Enter email address (optional, press Enter to skip): ");
    fflush(stdout);

    if (!fgets(buffer, sizeof(buffer), stdin))
        return NULL;

    buffer[strcspn(buffer, "\n")] = '\0';

    if (!*buffer)
        return strdup("");

    if (!validate_email(buffer)) {
        printf("Invalid email format. Skipping email.\n");
        return strdup("");
    }

    return strdup(buffer);
}

/**
 * prompt_password - Interactive password prompt with hidden input
 *
 * @param prompt  Prompt string to display
 * @return        Allocated password string (caller must free), or NULL on failure
 */
char *prompt_password(const char *prompt)
{
    char buffer[256];
    struct termios old, new;

    if (bootstrap_auto && bootstrap_password) {
        if (strlen(bootstrap_password) < 8) {
            fprintf(stderr, "Password must be at least 8 characters\n");
            return NULL;
        }
        return strdup(bootstrap_password);
    }

    printf("%s", prompt);
    fflush(stdout);

    /* Disable echo */
    if (tcgetattr(STDIN_FILENO, &old) != 0) {
        perror("tcgetattr");
        return NULL;
    }

    new = old;
    new.c_lflag &= ~ECHO;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &new) != 0) {
        perror("tcsetattr");
        return NULL;
    }

    /* Read password */
    if (!fgets(buffer, sizeof(buffer), stdin)) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old);
        printf("\n");
        return NULL;
    }

    /* Restore echo */
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    printf("\n");

    buffer[strcspn(buffer, "\n")] = '\0';

    if (strlen(buffer) < 8) {
        fprintf(stderr, "Password must be at least 8 characters.\n");
        return NULL;
    }

    return strdup(buffer);
}
