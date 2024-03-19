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
#include <curl/curl.h>


extern GLOBAL_DATA gconfig;


//extern long int   __BUILD_DATE;
extern char __BUILD_DATE;
extern long int   __BUILD_NUMBER;

/* MOVED: combat/assess.c */
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

/* MOVED: ship.c */
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

void do_showversion(CHAR_DATA *ch, char *argument)
{
	char buf[MAX_STRING_LENGTH];
	buf[0] = '\0';
//	time_t  build_date;
//	build_date = (time_t) &__BUILD_DATE;
//	builddate = &__BUILD_DATE)
//	sprintf(buf,"Build Date: %u\n\r",&build_date);
	sprintf(buf,"Build Number: %s (\t<a href=\"%s\">%s\t</a>)\n\rCommit URL: %s\n\rBuild Date: %s\n\r", BUILD_NUMBER, COMMIT, VERSION, COMMIT, BUILD_DATE);
	send_to_char(buf,ch);

}


void list_attachment_callback (quickmail mailobj, const char* filename, quickmail_attachment_open_fn email_info_attachment_open, quickmail_attachment_read_fn email_info_attachment_read, quickmail_attachment_close_fn email_info_attachment_close, void* callbackdata)
{
  printf("[%i]: %s\n", ++*(int*)callbackdata, filename);
}

void do_testemail (CHAR_DATA *ch, char *argument)
{

  char buf[MAX_STRING_LENGTH];

  if (IS_NULLSTR(gconfig.email_host) || gconfig.email_port == 0 || IS_NULLSTR(gconfig.email_username) || IS_NULLSTR(gconfig.email_password) || IS_NULLSTR(gconfig.email_from_addr))
  {
    send_to_char("One or more email configuration items is missing.\n\r",ch);
    sprintf(buf, "Host: %s\n\rPort: %d\n\rUsername: %s\n\rPassword: %s\n\rFrom Address: %s\n\r", gconfig.email_host, gconfig.email_port, gconfig.email_username, gconfig.email_password, gconfig.email_from_addr);
    send_to_char(buf,ch);
    return;
  }
    
  char subjline[256];

  quickmail_initialize();

  if (argument[0] != '\0')
    sprintf(subjline, "%s", argument);
  else
    sprintf(subjline, "Test Email");

  quickmail mailobj = quickmail_create(gconfig.email_from_name, gconfig.email_from_addr, subjline);

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
/**/
  //quickmail_add_attachment_file(mailobj, "test_quickmail.c", NULL);
  //quickmail_add_attachment_file(mailobj, "test_quickmail.cbp", NULL);
  //quickmail_add_attachment_memory(mailobj, "test.log", NULL, "Test\n123", 8, 0);
/**/
/*/
  quickmail_fsave(mailobj, stdout);

  int i;
  i = 0;
  quickmail_list_attachments(mailobj, list_attachment_callback, &i);

  quickmail_remove_attachment(mailobj, "test_quickmail.cbp");
  i = 0;
  quickmail_list_attachments(mailobj, list_attachment_callback, &i);

  quickmail_destroy(mailobj);
  return 0;
/**/

  const char* errmsg;
  //quickmail_set_debug_log(mailobj, stderr);
  if ((errmsg = quickmail_send(mailobj, gconfig.email_host, gconfig.email_port, gconfig.email_username, gconfig.email_password)) != NULL)
    fprintf(stderr, "Error sending e-mail: %s\n", errmsg);
  quickmail_destroy(mailobj);
  quickmail_cleanup();
}