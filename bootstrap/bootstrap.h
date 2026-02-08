/**
 * bootstrap.h - Public API for Sentience MUD bootstrap mode
 *
 * Provides functionality to bootstrap a fresh installation with minimal
 * viable data files and create the first staff account interactively.
 */

#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include <stdbool.h>

/* Bootstrap mode flags (set by detect_bootstrap_mode) */
extern bool bootstrap_mode;
extern bool bootstrap_auto;
extern char *bootstrap_username;
extern char *bootstrap_email;
extern char *bootstrap_password;

/* Core bootstrap functions (called from comm.c) */
void detect_bootstrap_mode(int argc, char **argv);
int run_bootstrap(void);

#endif /* BOOTSTRAP_H */
