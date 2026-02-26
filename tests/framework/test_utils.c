#ifdef BUILD_TESTS

#include <string.h>
#include "../../merc.h"
#include "../../recycle.h"
#include "../../protocol.h"
#include "test_utils.h"

bool test_utils_create_fake_player(CHAR_DATA **out_ch, DESCRIPTOR_DATA **out_desc)
{
    CHAR_DATA *ch;
    DESCRIPTOR_DATA *desc;

    if (!out_ch || !out_desc) {
        return false;
    }

    *out_ch = NULL;
    *out_desc = NULL;

    ch = new_char();
    if (!ch) {
        return false;
    }

    if (!ch->pcdata) {
        ch->pcdata = new_pcdata();
        if (!ch->pcdata) {
            free_char(ch);
            return false;
        }
    }

    desc = new_descriptor();
    if (!desc) {
        free_char(ch);
        return false;
    }

    desc->connected = CON_PLAYING;
    desc->showstr_head = NULL;
    desc->showstr_point = NULL;
    desc->outsize = 2000;
    desc->outbuf = alloc_mem(desc->outsize);
    desc->outtop = 0;
    desc->fcommand = false;
    desc->muted = 0;
    desc->pEdit = NULL;
    desc->pString = NULL;
    desc->editor = 0;
    desc->input = false;
    desc->input_var = NULL;
    desc->input_script = 0;
    desc->input_mob = NULL;
    desc->input_obj = NULL;
    desc->input_room = NULL;
    desc->input_tok = NULL;
    desc->input_prompt = NULL;
    desc->inputString = NULL;
    desc->pProtocol = ProtocolCreate();

    if (!desc->outbuf || !desc->pProtocol) {
        if (desc->pProtocol) {
            ProtocolDestroy(desc->pProtocol);
            desc->pProtocol = NULL;
        }
        free_descriptor(desc);
        free_char(ch);
        return false;
    }

    ch->desc = desc;
    desc->character = ch;

    *out_ch = ch;
    *out_desc = desc;
    return true;
}

void test_utils_destroy_fake_player(CHAR_DATA *ch, DESCRIPTOR_DATA *desc)
{
    if (ch) {
        ch->desc = NULL;
    }

    if (desc) {
        if (desc->pProtocol) {
            ProtocolDestroy(desc->pProtocol);
            desc->pProtocol = NULL;
        }

        desc->character = NULL;
        free_descriptor(desc);
    }

    if (ch) {
        free_char(ch);
    }
}

#endif /* BUILD_TESTS */
