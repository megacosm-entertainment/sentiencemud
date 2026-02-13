/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "merc.h"
#include "interp.h"
#include "magic.h"
#include "recycle.h"
#include "tables.h"
#include "db.h"
#include <quickmail.h>

//extern long int   __BUILD_DATE;
extern char __BUILD_DATE;

/**
 * do_showdamage - Toggle display of combat damage numbers
 *
 * Enables/disables showing numerical damage values during combat.
 * By default, only available to immortals or on test port.
 *
 * @param ch        The character toggling the setting
 * @param argument  Unused
 *
 * Requires: IS_IMMORTAL or is_test_port (unless DEBUG_ALLOW_SHOW_DAMAGE)
 *
 * Planned refactor: combat/assess.c (never executed)
 */
void do_showdamage(CHAR_DATA *ch, char *argument)
{
#ifndef DEBUG_ALLOW_SHOW_DAMAGE
    if (!IS_IMMORTAL(ch) && !is_test_port) {
  send_to_char("As a player, you may only see the damages of hits on the testport.\n\r", ch);
  return;
    }
#endif

    if (IS_SET(ch->act[0], PLR_SHOWDAMAGE)) {
    REMOVE_BIT(ch->act[0], PLR_SHOWDAMAGE);
    send_to_char("You will no longer see the damages of hits.\n\r", ch);
    } else {
    SET_BIT(ch->act[0], PLR_SHOWDAMAGE);
    send_to_char("You will now see the damages of hits.\n\r", ch);
    }
}

/**
 * do_autosurvey - Toggle automatic survey when on ships
 *
 * When enabled, automatically performs survey command when moving
 * around on ships.
 *
 * @param ch        The character toggling the setting
 * @param argument  Unused
 *
 * Blocked by: IS_NPC
 *
 * Planned refactor: ship.c (never executed)
 */
void do_autosurvey(CHAR_DATA *ch, char *argument)
{
    if (IS_NPC(ch))
  return;

    if (IS_SET(ch->act[1], PLR_AUTOSURVEY))
    {
  REMOVE_BIT(ch->act[1], PLR_AUTOSURVEY);
  send_to_char("You will no longer automatically survey on ships.\n\r", ch);
    }
    else
    {
  SET_BIT(ch->act[1], PLR_AUTOSURVEY);
  send_to_char("You will now automatically survey on ships.\n\r", ch);
    }
}

/**
 * do_showversion - Display game version and build information
 *
 * Shows the current game version, commit URL, and build date.
 *
 * @param ch        The character viewing version info
 * @param argument  Unused
 */
void do_showversion(CHAR_DATA *ch, char *argument)
{
  char buf[MAX_STRING_LENGTH];
  buf[0] = '\0';
//	time_t  build_date;
//	build_date = (time_t) &__BUILD_DATE;
//	builddate = &__BUILD_DATE)
//	sprintf(buf,"Build Date: %u\n\r",&build_date);
  sprintf(buf,"Version: (\t<a href=\"%s\">%s\t</a>)\n\rCommit URL: %s\n\rBuild Date: %s\n\r", COMMIT, VERSION, COMMIT, BUILD_DATE);
  send_to_char(buf,ch);

}


/**
 * list_attachment_callback - Callback for quickmail attachment listing
 *
 * Debug callback that prints attachment filenames. Used with quickmail
 * library for email functionality.
 *
 * @param mailobj                        The quickmail object
 * @param filename                       Attachment filename
 * @param email_info_attachment_open     Open function pointer
 * @param email_info_attachment_read     Read function pointer
 * @param email_info_attachment_close    Close function pointer
 * @param callbackdata                   Counter pointer for numbering
 */
void list_attachment_callback (quickmail mailobj, const char* filename, quickmail_attachment_open_fn email_info_attachment_open, quickmail_attachment_read_fn email_info_attachment_read, quickmail_attachment_close_fn email_info_attachment_close, void* callbackdata)
{
  printf("[%i]: %s\n", ++*(int*)callbackdata, filename);
}

/**
 * do_testemail - Send a test email to verify email configuration
 *
 * Sends a test email to the character's registered email address
 * using the game's SMTP settings. Used for debugging email delivery.
 *
 * @param ch        The character sending the test email
 * @param argument  Optional subject line override
 *
 * Requires: Email configuration (host, port, username, password, from_addr)
 */
void do_testemail (CHAR_DATA *ch, char *argument)
{

  extern GAME_SETTINGS_DATA game_settings;

  char buf[MAX_STRING_LENGTH];

  if (IS_NULLSTR(game_settings.email_host) || game_settings.email_port == 0 || IS_NULLSTR(game_settings.email_username) || IS_NULLSTR(game_settings.email_password) || IS_NULLSTR(game_settings.email_from_addr))
  {
    send_to_char("One or more email configuration items is missing.\n\r",ch);
    sprintf(buf, "Host: %s\n\rPort: %d\n\rUsername: %s\n\rPassword: %s\n\rFrom Address: %s\n\r", game_settings.email_host, game_settings.email_port, game_settings.email_username, game_settings.email_password, game_settings.email_from_addr);
    send_to_char(buf,ch);
    return;
  }
    
  char subjline[256];

  quickmail_initialize();

  if (argument[0] != '\0')
    sprintf(subjline, "%s", argument);
  else
    sprintf(subjline, "Test Email");

  quickmail mailobj = quickmail_create(game_settings.email_from_name, game_settings.email_from_addr, subjline);

  quickmail_add_to(mailobj, ch->pcdata->email);
#ifdef TO
  quickmail_add_to(mailobj, ch->pcdata->email);
#endif
#ifdef CC
  quickmail_add_cc(mailobj, CC);
#endif
#ifdef BCC
  quickmail_add_bcc(mailobj, BCC);
#endif
  quickmail_add_header(mailobj, "Importance: Low");
  quickmail_add_header(mailobj, "X-Priority: 5");
  quickmail_add_header(mailobj, "X-MSMail-Priority: Low");
  
  
  quickmail_set_body(mailobj, "This is a test e-mail.\nThis mail was sent using libquickmail.");
  //quickmail_add_body_memory(mailobj, NULL, "This is a test e-mail.\nThis mail was sent using libquickmail.", 64, 0);
  quickmail_add_body_memory(mailobj, "text/html", "This is a <b>test</b> e-mail.<br/>\nThis mail was sent using <u>libquickmail</u>.", 80, 0);

  //quickmail_add_attachment_file(mailobj, "test_quickmail.c", NULL);
  //quickmail_add_attachment_file(mailobj, "test_quickmail.cbp", NULL);
  //quickmail_add_attachment_memory(mailobj, "test.log", NULL, "Test\n123", 8, 0);

//  quickmail_fsave(mailobj, stdout);

//  int i;
//  i = 0;
//  quickmail_list_attachments(mailobj, list_attachment_callback, &i);

//  quickmail_remove_attachment(mailobj, "test_quickmail.cbp");
//  i = 0;
//  quickmail_list_attachments(mailobj, list_attachment_callback, &i);

//  quickmail_destroy(mailobj);
//  return 0;

  const char* errmsg;
  //quickmail_set_debug_log(mailobj, stderr);
  if ((errmsg = quickmail_send(mailobj, game_settings.email_host, game_settings.email_port, game_settings.email_username, game_settings.email_password)) != NULL)
    fprintf(stderr, "Error sending e-mail: %s\n", errmsg);
  quickmail_destroy(mailobj);
  quickmail_cleanup();
}

/**
 * do_raceinfo - Display detailed information about a race
 *
 * Shows the race description, stats, size, and links to help files.
 * Players can view any playable race; immortals can view all races.
 *
 * Syntax: raceinfo <race name>
 */
void do_raceinfo(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    RACE_DATA *race;
    BUFFER *buffer;

    one_argument(argument, arg);

    if (arg[0] == '\0') {
        send_to_char("Syntax: raceinfo <race name>\n\r", ch);
        return;
    }

    race = race_lookup_name(arg);
    if (!race) {
        race = race_lookup(arg);
    }
    if (!race) {
        send_to_char("No such race found.\n\r", ch);
        return;
    }

    /* Hide non-playable races from mortals */
    if (!race->playable && !IS_IMMORTAL(ch)) {
        send_to_char("No such race found.\n\r", ch);
        return;
    }

    buffer = new_buf();

    /* Header */
    add_buf(buffer, formatf("{C=== Race: {W%s{C ==={x\n\r", race->name));

    /* Description */
    if (!IS_NULLSTR(race->description)) {
        add_buf(buffer, formatf("\n\r%s{x\n\r", race->description));
    }

    add_buf(buffer, "\n\r");

    /* Availability */
    if (race->starting)
        add_buf(buffer, "{xAvailability: {GStarting race{x\n\r");
    else if (race_is_remort(race))
        add_buf(buffer, "{xAvailability: {YRemort race{x\n\r");
    else if (race->playable)
        add_buf(buffer, "{xAvailability: {YPlayable{x\n\r");
    else
        add_buf(buffer, "{xAvailability: {DNPC only{x\n\r");

    /* Size */
    if (race->min_size == race->max_size)
        add_buf(buffer, formatf("{xSize:         {W%s{x\n\r",
                                size_table[race->min_size].name));
    else
        add_buf(buffer, formatf("{xSize:         {W%s{x to {W%s{x\n\r",
                                size_table[race->min_size].name,
                                size_table[race->max_size].name));

    /* Stats */
    static const char *stat_short[] = {"Str", "Int", "Wis", "Dex", "Con"};
    add_buf(buffer, "{xBase stats:   ");
    for (int i = 0; i < MAX_STATS; i++) {
        add_buf(buffer, formatf("{C%s{x:{W%d{x ", stat_short[i], race->stats[i]));
    }
    add_buf(buffer, "\n\r");

    add_buf(buffer, "{xMax stats:    ");
    for (int i = 0; i < MAX_STATS; i++) {
        add_buf(buffer, formatf("{C%s{x:{W%d{x ", stat_short[i], race->max_stats[i]));
    }
    add_buf(buffer, "\n\r");

    /* Racial skills */
    if (race->skills && list_size(race->skills) > 0) {
        add_buf(buffer, "\n\r{xRacial skills: ");
        ITERATOR it;
        char *skill_name;
        bool first = true;
        iterator_start(&it, race->skills);
        while ((skill_name = (char *)iterator_nextdata(&it))) {
            if (!first) add_buf(buffer, ", ");
            add_buf(buffer, formatf("{W%s{x", skill_name));
            first = false;
        }
        iterator_stop(&it);
        add_buf(buffer, "\n\r");
    }

    /* Remort info */
    if (race->remort_into_id) {
        RACE_DATA *into = race_get_remort_into(race);
        if (into)
            add_buf(buffer, formatf("\n\r{xRemorts into: {W%s{x\n\r", into->name));
    }
    if (race->remort_race_id) {
        RACE_DATA *prereq = race_get_prerequisite(race);
        if (prereq)
            add_buf(buffer, formatf("{xRequires:     {W%s{x\n\r", prereq->name));
    }

    /* Player's race check */
    if (!IS_NPC(ch)) {
        if (ch->race == race)
            add_buf(buffer, "\n\r{YThis is your current race.{x\n\r");
    }

    /* Help file link */
    HELP_DATA *help = lookup_help_exact(race->name, get_staff_rank(ch), topHelpCat);
    if (!help && race->id)
        help = lookup_help_exact(race->id, get_staff_rank(ch), topHelpCat);
    if (help) {
        add_buf(buffer, formatf("\n\r{xHelp:         \t<send href=\"help #%d\">{Whelp %s{x\t</send>\n\r",
                                help->index, race->name));
    }

    page_to_char(buffer->string, ch);
    free_buf(buffer);
}
