/***************************************************************************
 *  Pfile Migration Utility - Header                                       *
 ***************************************************************************/

#ifndef PFILE_MIGRATE_H
#define PFILE_MIGRATE_H

/* Single file migration */
bool migrate_player(char *name, bool backup);
bool migrate_account(char *name, bool backup, void *stats);

/* Batch migration */
int migrate_all_players(bool backup);
int migrate_all_accounts(bool backup);

/* Command interface */
void do_migrate(CHAR_DATA *ch, char *argument);

#endif /* PFILE_MIGRATE_H */
