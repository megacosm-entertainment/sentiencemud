/***************************************************************************
 *                                                                         *
 *    Scripting engine rebuilt by Michael Kurtz (Nibelung)                 *
 *    Used with permission.                                                *
 *                                                                         *
 **************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <jansson.h>
#include "merc.h"
#include "recycle.h"
#include "io/json/json_common.h"

// Indexes for lookups of special help files.
int 	motd;
int	imotd;
int 	rules;
int 	wizlist;

// For loading and saving area files
static bool fMatch;

#define HELP_JSON_FORMAT "help_data"
#define HELP_JSON_VERSION 1

void do_help(CHAR_DATA *ch, char *argument)
{
    HELP_DATA *help;
    HELP_CATEGORY *hcat, *hcatnest;
    BUFFER *buffer;
    char buf[2*MSL], buf2[MSL];
    char *p;
    int index;
    int i;

    if (argument[0] == '\0')
        find_help_category_exact("summary", topHelpCat);
        

    // Category lookup - must be exact
    if ((hcat = find_help_category_exact(argument, topHelpCat)) != NULL &&
        get_staff_rank(ch) >= hcat->min_rank &&
        lookup_help_exact(argument, get_staff_rank(ch), topHelpCat) == NULL) {

        buffer = new_buf();

        sprintf(buf2, "{R%s{x", hcat == topHelpCat ? "summary" : hcat->name);

        // Capitalize category name
        for (i = 0; buf2[i] != '\0'; i++) {
            if (buf2[i] == '{')
                i+= 2;

            buf2[i] = UPPER(buf2[i]);
        }

        sprintf(buf, "{R%s{x", buf2);

        // Add on higher-level categories
        hcatnest = hcat;
        while ((hcatnest = hcatnest->up) != NULL && hcatnest != topHelpCat) {
            sprintf(buf2, "{R%s{x", hcatnest->name);
            for (i = 0; buf2[i] != '\0'; i++)
                buf2[i] = UPPER(buf2[i]);

            strcat(buf2, " {r->{R ");
            strcat(buf2, buf);
            sprintf(buf, "%s", buf2);
        }

        sprintf(buf, "{b[{W%s{b]{x\n\r", buf2);
        add_buf(buffer, buf);

        i = 1;
        for (hcatnest = hcat->inside_cats; hcatnest != NULL; hcatnest = hcatnest->next) {
            if (get_staff_rank(ch) >= hcatnest->min_rank) {
                sprintf(buf2, "%s", hcatnest->name);


                p = buf2;
                while (*p != '\0') {
                    *p = UPPER(*p);
                    p++;
                }

//				char buf3[sizeof(buf2)+50];

                sprintf(buf, "{b[{BC{b]{W   \t<send href=\"help %s\">%.36s\t</send>%s %s", buf2, buf2, pad_string(buf2, 36, NULL, NULL), i % 3 == 0 ? "\n\r" : "");
                add_buf(buffer, buf);
                i++;
            }
        }

        for (help = hcat->inside_helps; help != NULL; help = help->next) {
            if (get_staff_rank(ch) >= help->min_rank) {
                sprintf(buf, "{b[{B%-3d{b]{x \t<send href=\"help #%d\">%.20s\t</send>%s %s", help->index, help->index, help->keyword, pad_string(help->keyword, 20, NULL, NULL), i % 3 == 0 ? "\n\r" : "");
                add_buf(buffer, buf);
                i++;
            }
        }

        if ((i - 1) % 3 != 0)
            add_buf(buffer, "\n\r");

        // Only output data and return if we've found some results
        if ((i - 1) > 0) {
            page_to_char(buf_string(buffer), ch);
            free_buf(buffer);

        if (hcat == topHelpCat && argument[0] == '\0')
        {
            send_to_char("\n\r", ch);
            send_to_char("Syntax: help <keyword(s)>\n\r"
                    "        help <category name|summary>\n\r"
                    "        help #<index>\n\r", ch);
        
        }
        return;
        }
    }

    // help #<number> is used to lookup helpfiles by index.
    if (*argument == '#') {
        argument++;

        if ((index = atoi(argument)) < 0 || index > 32000) {
            send_to_char("That help index is out of range.\n\r", ch);
            return;
        } else
            help = lookup_help_index(index, get_staff_rank(ch), topHelpCat);

        if (help == NULL)
            send_to_char("No help found with that index.\n\r", ch);
        else
            show_help_to_ch(ch, help);

        return;
    }

    if (strlen(argument) < 3) {
        send_to_char("You must specify at least 3 letters of a keyword.\n\r", ch);
        return;
    }

    // Lookup by keyword

    // Handle multiple entries w/ same keyword
    if (count_num_helps(argument, get_staff_rank(ch), topHelpCat) > 1) {
        act("{YMultiple entries found with keyword $t:{x", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
        buffer = new_buf();

        lookup_help_multiple(argument, get_staff_rank(ch), topHelpCat, buffer);
        page_to_char(buf_string(buffer), ch);
        free_buf(buffer);
        return;
    }

    help = lookup_help(argument, get_staff_rank(ch), topHelpCat);

    if (help == NULL || help->hCat->min_level > get_staff_rank(ch))
    {
        act("No help or category found with keyword $t.", ch, NULL, NULL, NULL, NULL, argument, NULL, TO_CHAR, NULL, NULL);
        sprintf(buf, "%s attempted to get help for '%s' but no helpfile was found.", ch->name, argument);
        log_string(buf);
        wiznet(buf, ch, NULL, WIZ_HELPS, 0, 0);
    }
    else
        show_help_to_ch(ch, help);
}


void show_help_to_ch(CHAR_DATA *ch, HELP_DATA *help)
{
    char buf[MSL];
    BUFFER *buffer;
    STRING_DATA *topic;
    int i;


    buffer = new_buf();

    sprintf(buf, "{b[{B%-3d {W%-24s{b] ", help->index, help->keyword);
    add_buf(buffer, buf);

    sprintf(buf, "Last updated: {x%s", help->modified == 0 ? "Unknown\n\r" : (char *) ctime(&help->modified));
    add_buf(buffer, buf);

    add_buf(buffer, "{b-------------------------------------------------------------------------------{x\n\r");
    add_buf(buffer, help->text);
    add_buf(buffer, "{b-------------------------------------------------------------------------------{x\n\r");

    if (help->related_topics != NULL)
    add_buf(buffer, "{bRelated topics:{x ");

    i = 0;
    for (topic = help->related_topics; topic != NULL; topic = topic->next) {
        if (lookup_help_exact(topic->string, get_staff_rank(ch), topHelpCat) != NULL)
            sprintf(buf, "\t<send href=\"help #%d\">%s\t</send>{x", lookup_help_exact(topic->string,get_staff_rank(ch),topHelpCat)->index, topic->string);
        else
            sprintf(buf, "{R%s{X", topic->string);
        add_buf(buffer, buf);

        if (topic->next != NULL)
            add_buf(buffer, "{B,{x ");

        i++;

        if (i % 7 == 0)
            add_buf(buffer, "\n\r");
    }

    if (i > 0 && i % 7 != 0)
        add_buf(buffer, "\n\r");

    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}


// Return # of helpfiles in hcat containing keyword.
int count_num_helps(char *keyword, int viewer_level, HELP_CATEGORY *hcat)
{
    HELP_CATEGORY *hcatnest;
    HELP_DATA *help;
    int num = 0;

    for (hcatnest = hcat->inside_cats; hcatnest != NULL; hcatnest = hcatnest->next) {
    num += count_num_helps(keyword, viewer_level, hcatnest);
    }

    for (help = hcat->inside_helps; help != NULL; help = help->next) {
    if (!str_infix(keyword, help->keyword)
    &&  viewer_level >= help->min_level
    &&  viewer_level >= help->hCat->min_level)
        num++;
    }

    return num;
}


// Find a help category with exactly the name specified
HELP_CATEGORY *find_help_category_exact(char *name, HELP_CATEGORY *hcat)
{
    HELP_CATEGORY *hcatnest, *hcatfound;

    if (!str_cmp(name, "summary"))
    return topHelpCat;

    if (!str_cmp(hcat->name, name))
    return hcat;

    for (hcatnest = hcat->inside_cats; hcatnest != NULL; hcatnest = hcatnest->next) {
    if ((hcatfound = find_help_category_exact(name, hcatnest)) != NULL)
        return hcatfound;
    }

    return NULL;
}


// Find a helpfile with a keyword
HELP_DATA *lookup_help(char *keyword, int viewer_level, HELP_CATEGORY *hcat)
{
    HELP_DATA *help;
    HELP_CATEGORY *hcatNest;

    for (hcatNest = hcat->inside_cats; hcatNest != NULL; hcatNest = hcatNest->next) {
    if ((help = lookup_help(keyword, viewer_level, hcatNest)) != NULL)
        return help;
    }

    for (help = hcat->inside_helps; help != NULL; help = help->next) {
    if (!str_infix(keyword, help->keyword)
    &&  viewer_level >= help->min_level
    &&  viewer_level >= help->hCat->min_level)
            return help;
    }

    return NULL;
}


// Find a helpfile with exactly the specified keyword
HELP_DATA *lookup_help_exact(char *keyword, int viewer_level, HELP_CATEGORY *hcat)
{
    HELP_DATA *help;
    HELP_CATEGORY *hcatNest;

    for (hcatNest = hcat->inside_cats; hcatNest != NULL; hcatNest = hcatNest->next) {
    if ((help = lookup_help_exact(keyword, viewer_level, hcatNest)) != NULL)
        return help;
    }

    for (help = hcat->inside_helps; help != NULL; help = help->next) {
    if (!str_cmp(keyword, help->keyword)
    &&  viewer_level >= help->min_level
    &&  viewer_level >= help->hCat->min_level)
            return help;
    }

    return NULL;
}


// Lookup a help by index - always finds exactly one helpfile
HELP_DATA *lookup_help_index(unsigned int index, int viewer_level, HELP_CATEGORY *hcat)
{
    HELP_DATA *help;
    HELP_CATEGORY *hcatNest;

    for (hcatNest = hcat->inside_cats; hcatNest != NULL; hcatNest = hcatNest->next) {
    if ((help = lookup_help_index(index, viewer_level, hcatNest)) != NULL)
        return help;
    }

    for (help = hcat->inside_helps; help != NULL; help = help->next) {
    if (index == help->index
    &&  viewer_level >= help->min_level
    &&  viewer_level >= help->hCat->min_level)
            return help;
    }

    return NULL;
}


// Lookup categories which share the same keyword
void lookup_category_multiple(char *keyword, int viewer_level, HELP_CATEGORY *hcat, BUFFER *buffer)
{
    HELP_CATEGORY *hcatnest;
    char buf[2*MSL], buf2[MSL];
    char *p;

    if (!str_infix(keyword, hcat->name) && viewer_level >= hcat->min_level) {
        sprintf(buf2, hcat->name);

        for (p = buf2; *p != '\0'; p++)
            *p = UPPER(*p);

        sprintf(buf, "{b[{BC  {b] {W%s{x\n\r", buf2);
        add_buf(buffer, buf);
    }

    for (hcatnest = hcat->inside_cats; hcatnest != NULL; hcatnest = hcatnest->next)
        lookup_category_multiple(keyword, viewer_level, hcatnest, buffer);
}


// Lookup a keyword inside a category and output indexes. For keyword matching > 1 helpfile.
void lookup_help_multiple(char *keyword, int viewer_level, HELP_CATEGORY *hcat, BUFFER *buffer)
{
    HELP_CATEGORY *hcatnest;
    HELP_DATA *help;
    char buf[MSL];

    for (hcatnest = hcat->inside_cats; hcatnest != NULL; hcatnest = hcatnest->next)
    lookup_help_multiple(keyword, viewer_level, hcatnest, buffer);

    for (help = hcat->inside_helps; help != NULL; help = help->next) {
    if (!str_infix(keyword, help->keyword)
    &&  viewer_level >= help->min_level
    &&  viewer_level >= help->hCat->min_level)
    {
            sprintf(buf, "{b[{B%-3d{b] \t<send href=\"help #%d\">{W%.24s\t</send>%s{x\n\r", help->index, help->index, help->keyword, pad_string(help->keyword, 24, NULL, NULL));
        add_buf(buffer, buf);
    }
    }
}


// Set up arbitrary help file indexes on a boot.
int index_helpfiles(int index, HELP_CATEGORY *hcat)
{
    HELP_DATA *help;
    HELP_CATEGORY *hcatnest;

    for (hcatnest = hcat->inside_cats; hcatnest != NULL; hcatnest = hcatnest->next)
    index = index_helpfiles(index, hcatnest);

    for (help = hcat->inside_helps; help != NULL; help = help->next)
    {
    help->index = index;

    if (!str_cmp(help->keyword, "MOTD"))
        motd = help->index;
    else if (!str_cmp(help->keyword, "IMOTD"))
        imotd = help->index;
    else if (!str_cmp(help->keyword, "RULES"))
        rules = help->index;
    else if (!str_cmp(help->keyword, "WIZLIST IMMORTALS"))
        wizlist = help->index;

    index++;
    top_help_index++;
    }

    return index;
}


// Find a helpfile anywhere in the db. No security considerations.
HELP_DATA *find_helpfile(char *keyword, HELP_CATEGORY *hcat)
{
    HELP_CATEGORY *hcatnest;
    HELP_DATA *help;

    for (hcatnest = hcat->inside_cats; hcatnest != NULL; hcatnest = hcatnest->next)
    {
    if ((help = find_helpfile(keyword, hcatnest)) != NULL)
        return help;
    }

    for (help = hcat->inside_helps; help != NULL; help = help->next)
    {
    if (!str_prefix(keyword, help->keyword))
        break;
    }

    return help;
}


// Insert help into a list and keep it sorted
void insert_help(HELP_DATA *help, HELP_DATA **list)
{
    HELP_DATA *helpTmp, *helpTmpPrev = NULL;

    for (helpTmp = *list; helpTmp != NULL; helpTmp = helpTmp->next) {
    if (strcmp(help->keyword, helpTmp->keyword) <= 0)
        break;

    helpTmpPrev = helpTmp;
    }

    help->next = helpTmp;
    if (helpTmpPrev != NULL)
    helpTmpPrev->next = help;
    else
    *list = help;
}


// Find a help category in a list
HELP_CATEGORY *find_help_category(char *name, HELP_CATEGORY *list)
{
    HELP_CATEGORY *hcat;

    for (hcat = list; hcat != NULL; hcat = hcat->next)
    {
        if (!str_prefix(name, hcat->name))
        return hcat;
    }

    return NULL;
}


static void normalize_help_category(HELP_CATEGORY *hcat)
{
    if (!hcat)
        return;

    if (!str_cmp(hcat->modified_by, "(null)")) {
        free_string(hcat->modified_by);
        hcat->modified_by = str_dup("Unknown");
    }

    if (!str_cmp(hcat->creator, "(null)")) {
        free_string(hcat->creator);
        hcat->creator = str_dup("Unknown");
    }

    if (!str_cmp(hcat->description, "(null)")) {
        free_string(hcat->description);
        hcat->description = str_dup("None\n\r");
    }

    if (hcat->min_level == 150)
        hcat->min_rank = STAFF_IMMORTAL;
    else if (hcat->min_level == 151 || hcat->min_level == 152)
        hcat->min_rank = STAFF_ASCENDANT;
    else if (hcat->min_level == 153)
        hcat->min_rank = STAFF_SUPREMACY;
    else if (hcat->min_level == 154)
        hcat->min_rank = STAFF_CREATOR;
    else if (hcat->min_level == 155)
        hcat->min_rank = STAFF_IMPLEMENTOR;
}


static void normalize_help(HELP_DATA *help)
{
    if (!help)
        return;

    if (!str_cmp(help->creator, "(null)")) {
        free_string(help->creator);
        help->creator = str_dup("Unknown");
    }

    if (!str_cmp(help->modified_by, "(null)")) {
        free_string(help->modified_by);
        help->modified_by = str_dup("Unknown");
    }

    if (!str_cmp(help->text, "(null)")) {
        free_string(help->text);
        help->text = str_dup("Unknown");
    }

    if (help->min_level == 150)
        help->min_rank = STAFF_IMMORTAL;
    else if (help->min_level == 151 || help->min_level == 152)
        help->min_rank = STAFF_ASCENDANT;
    else if (help->min_level == 153)
        help->min_rank = STAFF_SUPREMACY;
    else if (help->min_level == 154)
        help->min_rank = STAFF_CREATOR;
    else if (help->min_level == 155)
        help->min_rank = STAFF_IMPLEMENTOR;
}


static json_t *serialize_help_topics_json(STRING_DATA *topics)
{
    json_t *array = json_array();
    STRING_DATA *topic;

    for (topic = topics; topic != NULL; topic = topic->next)
        json_array_append_new(array, json_string_safe(topic->string));

    return array;
}


static STRING_DATA *deserialize_help_topics_json(json_t *array)
{
    STRING_DATA *head = NULL;
    STRING_DATA *last = NULL;
    size_t index;
    json_t *entry;

    if (!array || !json_is_array(array))
        return NULL;

    json_array_foreach(array, index, entry) {
        STRING_DATA *topic;

        if (!json_is_string(entry))
            continue;

        topic = new_string_data();
        topic->string = str_dup(json_string_value(entry));
        topic->next = NULL;

        if (!head)
            head = topic;
        else
            last->next = topic;

        last = topic;
    }

    return head;
}


static json_t *serialize_help_json(HELP_DATA *help)
{
    json_t *obj = json_object();

    json_object_set_new(obj, "keyword", json_string_safe(help->keyword));
    json_object_set_new(obj, "creator", json_string_safe(help->creator));
    json_object_set_new(obj, "created", json_integer((json_int_t)help->created));
    json_object_set_new(obj, "modified_by", json_string_safe(help->modified_by));
    json_object_set_new(obj, "modified", json_integer((json_int_t)help->modified));
    json_object_set_new(obj, "builders", json_string_safe(help->builders));
    json_object_set_new(obj, "min_rank", json_integer(help->min_rank));
    json_object_set_new(obj, "security", json_integer(help->security));
    json_object_set_new(obj, "text", json_string_safe(help->text));
    json_object_set_new(obj, "related_topics", serialize_help_topics_json(help->related_topics));

    return obj;
}


static HELP_DATA *deserialize_help_json(json_t *obj)
{
    HELP_DATA *help;

    if (!obj || !json_is_object(obj))
        return NULL;

    help = new_help();
    help->keyword = str_dup(json_get_string(obj, "keyword", ""));
    help->creator = str_dup(json_get_string(obj, "creator", "Unknown"));
    help->created = (time_t)json_get_int(obj, "created", 0);
    help->modified_by = str_dup(json_get_string(obj, "modified_by", "Unknown"));
    help->modified = (time_t)json_get_int(obj, "modified", 0);
    help->builders = str_dup(json_get_string(obj, "builders", "None"));
    help->min_rank = (int)json_get_int(obj, "min_rank", 0);
    help->security = (int)json_get_int(obj, "security", 0);
    help->text = str_dup(json_get_string(obj, "text", "Unknown"));
    help->min_level = (int)json_get_int(obj, "min_level", 0);
    help->related_topics = deserialize_help_topics_json(json_object_get(obj, "related_topics"));

    normalize_help(help);

    return help;
}


static json_t *serialize_help_category_json(HELP_CATEGORY *hcat)
{
    json_t *obj = json_object();
    json_t *categories = json_array();
    json_t *helps = json_array();
    HELP_CATEGORY *hcat_tmp;
    HELP_DATA *help;

    json_object_set_new(obj, "name", json_string_safe(hcat->name));
    json_object_set_new(obj, "description", json_string_safe(hcat->description));
    json_object_set_new(obj, "min_rank", json_integer(hcat->min_rank));
    json_object_set_new(obj, "security", json_integer(hcat->security));
    json_object_set_new(obj, "builders", json_string_safe(hcat->builders));
    json_object_set_new(obj, "creator", json_string_safe(hcat->creator));
    json_object_set_new(obj, "created", json_integer((json_int_t)hcat->created));
    json_object_set_new(obj, "modified_by", json_string_safe(hcat->modified_by));
    json_object_set_new(obj, "modified", json_integer((json_int_t)hcat->modified));

    for (hcat_tmp = hcat->inside_cats; hcat_tmp != NULL; hcat_tmp = hcat_tmp->next)
        json_array_append_new(categories, serialize_help_category_json(hcat_tmp));

    for (help = hcat->inside_helps; help != NULL; help = help->next)
        json_array_append_new(helps, serialize_help_json(help));

    json_object_set_new(obj, "categories", categories);
    json_object_set_new(obj, "helps", helps);

    return obj;
}


static HELP_CATEGORY *deserialize_help_category_json(json_t *obj)
{
    HELP_CATEGORY *hcat;
    size_t index;
    json_t *entry;

    if (!obj || !json_is_object(obj))
        return NULL;

    hcat = new_help_category();
    hcat->name = str_dup(json_get_string(obj, "name", ""));
    hcat->description = str_dup(json_get_string(obj, "description", "None\n\r"));
    hcat->min_rank = (int)json_get_int(obj, "min_rank", 0);
    hcat->security = (int)json_get_int(obj, "security", 9);
    hcat->builders = str_dup(json_get_string(obj, "builders", "None"));
    hcat->creator = str_dup(json_get_string(obj, "creator", "Unknown"));
    hcat->created = (time_t)json_get_int(obj, "created", 0);
    hcat->modified_by = str_dup(json_get_string(obj, "modified_by", "Unknown"));
    hcat->modified = (time_t)json_get_int(obj, "modified", 0);
    hcat->min_level = (int)json_get_int(obj, "min_level", 0);

    {
        json_t *categories = json_object_get(obj, "categories");

        if (categories && json_is_array(categories)) {
        HELP_CATEGORY *tail = NULL;

            json_array_foreach(categories, index, entry) {
                HELP_CATEGORY *child = deserialize_help_category_json(entry);

                if (!child)
                    continue;

                child->up = hcat;

                if (!hcat->inside_cats)
                    hcat->inside_cats = child;
                else
                    tail->next = child;

                tail = child;
            }
        }
    }

    {
        json_t *helps = json_object_get(obj, "helps");

        if (helps && json_is_array(helps)) {
            json_t *help_entry;

            json_array_foreach(helps, index, help_entry) {
                HELP_DATA *help = deserialize_help_json(help_entry);

                if (!help)
                    continue;

                help->hCat = hcat;
                insert_help(help, &hcat->inside_helps);

                if (!str_cmp(help->keyword, "greeting"))
                    help_greeting = help->text;
            }
        }
    }

    normalize_help_category(hcat);

    return hcat;
}


static bool save_helpfiles_json(void)
{
    json_t *root;

    if (!topHelpCat)
        return false;

    root = json_object();
    json_object_set_new(root, "_format", json_string(HELP_JSON_FORMAT));
    json_object_set_new(root, "_version", json_integer(HELP_JSON_VERSION));
    json_object_set_new(root, "top_category", serialize_help_category_json(topHelpCat));

    return json_file_save(root, HELP_JSON_FILE, "save_helpfiles_json",
        JSON_INDENT(2) | JSON_PRESERVE_ORDER);
}


static bool read_helpfiles_json(void)
{
    json_t *root;
    json_t *top;

    root = json_file_load(HELP_JSON_FILE, NULL, NULL, "read_helpfiles_json");
    if (!root)
        return false;

    top = json_object_get(root, "top_category");
    if (!top || !json_is_object(top)) {
        pbugf(LOG_ERROR, "read_helpfiles_json: missing top_category in %s", HELP_JSON_FILE);
        json_decref(root);
        return false;
    }

    topHelpCat = deserialize_help_category_json(top);
    json_decref(root);

    return (topHelpCat != NULL);
}


// Save the helpfiles
void save_helpfiles_new()
{
    FILE *fp;
    char help_file_buf[MAX_INPUT_LENGTH];
    const char *help_file = resolve_game_path(HELP_FILE, help_file_buf, sizeof(help_file_buf));

    if (save_helpfiles_json())
        return;

    if ((fp = fopen(help_file, "w")) == NULL) {
        pbugf(LOG_ERROR, "save_helpfiles_new: couldn't open file for writing");
        return;
    }

    save_help_category_new(fp, topHelpCat);

    fclose(fp);
}


// Read the helpfiles
void read_helpfiles_new()
{
    FILE *fp;
    char *word;
    char help_file_buf[MAX_INPUT_LENGTH];
    const char *help_file = resolve_game_path(HELP_FILE, help_file_buf, sizeof(help_file_buf));

    if (read_helpfiles_json())
        return;

    if ((fp = fopen(help_file, "r")) == NULL) {
        pbugf(LOG_ERROR, "read_helpfiles_new: couldn't open file for reading");
        fp = fopen(help_file, "w");
        if (fp != NULL) {
            fprintf(fp, "#HELPCATEGORY ~\n");
            fprintf(fp, "Description This is the category which holds all of the other categories.\n~");
            fprintf(fp, "MinLevel 0\n");
            fprintf(fp, "Creator System~\n");
        fprintf(fp, "Created %ld\n", (long int)current_time);
            fprintf(fp, "ModifiedBy Nobody~\n");
            fprintf(fp, "Modified 0\n");
            fprintf(fp, "Security 9\n");
            fprintf(fp, "#-HELPCATEGORY\n");
            fclose(fp);
        }
    }

    fp = fopen(help_file, "r");
    if (fp == NULL) {
        topHelpCat = new_help_category();
        free_string(topHelpCat->name);
        topHelpCat->name = str_dup("");
        free_string(topHelpCat->description);
        topHelpCat->description = str_dup("This is the category which holds all of the other categories.\n\r");
        free_string(topHelpCat->builders);
        topHelpCat->builders = str_dup("None");
        free_string(topHelpCat->creator);
        topHelpCat->creator = str_dup("System");
        free_string(topHelpCat->modified_by);
        topHelpCat->modified_by = str_dup("Nobody");
        topHelpCat->created = current_time;
        topHelpCat->modified = 0;
        topHelpCat->security = 9;
        topHelpCat->min_rank = 0;
        save_helpfiles_json();
        return;
    }

    word = fread_word(fp);

    if (!str_cmp(word, "#HELPCATEGORY"))  {
        topHelpCat = read_help_category_new(fp);
        fclose(fp);
        save_helpfiles_json();
    } else {
        fclose(fp);
        pbugf(LOG_ERROR, "read_helpfiles_new: bad format");
        exit(1);
    }
}


// Save a help category and its contents
void save_help_category_new(FILE *fp, HELP_CATEGORY *hcat)
{
    HELP_DATA *help;
    HELP_CATEGORY *hcatTmp;

    fprintf(fp, "#HELPCATEGORY %s~\n", hcat->name);
    fprintf(fp, "Description %s~\n", fix_string(hcat->description));
    fprintf(fp, "Rank %d\n", hcat->min_rank);
    fprintf(fp, "Builders %s~\n", hcat->builders);
    fprintf(fp, "Creator %s~\n", hcat->creator);
    fprintf(fp, "Created %ld\n", (long int)hcat->created);
    fprintf(fp, "Modified %ld\n", (long int)hcat->modified);
    fprintf(fp, "ModifiedBy %s~\n", hcat->modified_by);
    fprintf(fp, "Security %d\n", hcat->security);

    // Recursively save subcategories inside it.
    for (hcatTmp = hcat->inside_cats; hcatTmp != NULL; hcatTmp = hcatTmp->next)
        save_help_category_new(fp, hcatTmp);

    for (help = hcat->inside_helps; help != NULL; help = help->next)
    save_help_new(fp, help);

    fprintf(fp, "#-HELPCATEGORY\n");
}


// Save a helpfile
void save_help_new(FILE *fp, HELP_DATA *help)
{
    STRING_DATA *topic;

    fprintf(fp, "#HELP %s~\n", help->keyword);
    fprintf(fp, "Creator %s~\n", help->creator);
    fprintf(fp, "ModifiedBy %s~\n", help->modified_by);
    fprintf(fp, "Modified %ld\n", (long int)help->modified);
    fprintf(fp, "Builders %s~\n", help->builders);
    fprintf(fp, "Rank %d\n", help->min_rank);
    fprintf(fp, "Security %d\n", help->security);

    if (help->related_topics != NULL) {
    for (topic = help->related_topics; topic != NULL; topic = topic->next) {
        fprintf(fp, "RelatedTopic %s~\n", topic->string);
    }
    }

    fprintf(fp, "Text %s~\n", fix_string(help->text));
    fprintf(fp, "#-HELP\n");
}


// Read a help category and its contents
HELP_CATEGORY *read_help_category_new(FILE *fp)
{
    HELP_CATEGORY *hcat;
    HELP_CATEGORY *hcatNest;
    HELP_CATEGORY *hcatTmp;
    HELP_DATA *help;
    char *word;

    hcat = new_help_category();
    hcat->name = fread_string(fp);

    while (str_cmp((word = fread_word(fp)), "#-HELPCATEGORY"))
    {
    fMatch = false;

    switch (word[0])
    {
        case '#':
            if (!str_cmp(word, "#HELPCATEGORY"))
        {
            hcatNest = read_help_category_new(fp);

            hcatNest->next = NULL;

            if (hcat->inside_cats == NULL)
            hcat->inside_cats = hcatNest;
            else {
            for (hcatTmp = hcat->inside_cats; hcatTmp->next != NULL; hcatTmp = hcatTmp->next)
                ;

            hcatTmp->next = hcatNest;
            }

            hcatNest->up = hcat;

            fMatch = true;
        }

        if (!str_cmp(word, "#HELP")) {
            help = read_help_new(fp);

            help->next = NULL;
            if (hcat->inside_helps == NULL)
            hcat->inside_helps = help;
            else
            insert_help(help, &hcat->inside_helps);

            help->hCat = hcat;

            if (!str_cmp(help->keyword, "greeting"))
            help_greeting = help->text;

            fMatch = true;
        }

        break;

        case 'B':
            KEYS("Builders",	hcat->builders,		fread_string(fp));
        break;

        case 'C':
            KEYS("Creator",	hcat->creator,		fread_string(fp));
        KEY("Created",		hcat->created,		fread_number(fp));
        break;

        case 'D':
            KEYS("Description",	hcat->description,	fread_string(fp));
        break;

        case 'M':
        KEY("MinLevel",	hcat->min_level,	fread_number(fp));
        KEY("Modified",	hcat->modified,		fread_number(fp));
        KEYS("ModifiedBy",	hcat->modified_by,	fread_string(fp));
        break;

        case 'R':
        KEY("Rank",		hcat->min_rank,		fread_number(fp));
        break;

        case 'S':
        KEY("Security",	hcat->security,		fread_number(fp));
        break;
    }

    if (!fMatch) {
        pbugf(LOG_ERROR, "read_help_category_new: no match for word %s", word);
    }
    }


    normalize_help_category(hcat);

    return hcat;
}


// Read a helpfile
HELP_DATA *read_help_new(FILE *fp)
{
    HELP_DATA *help;
    char *word;

    help = new_help();
    help->keyword = fread_string(fp);

    while (str_cmp((word = fread_word(fp)), "#-HELP"))
    {
    fMatch = false;

    switch (word[0])
    {
        case 'B':
            KEYS("Builders",	help->builders,		fread_string(fp));
        break;

        case 'C':
            KEYS("Creator",	help->creator,		fread_string(fp));
        KEY("Created",		help->created,		fread_number(fp));
        break;

        case 'M':
            KEY("MinLevel",	help->min_level,	fread_number(fp));
        KEY("Modified",	help->modified,		fread_number(fp));
        KEYS("ModifiedBy",	help->modified_by,	fread_string(fp));
        break;

            case 'R':
            KEY("Rank",		help->min_rank,		fread_number(fp));
        if (!str_cmp(word, "RelatedTopic")) {
            STRING_DATA *topic, *topic_tmp;

                    topic = new_string_data();

            fMatch = true;

            topic->string = fread_string(fp);

            if (help->related_topics == NULL) {
            topic->next = help->related_topics;
            help->related_topics = topic;
            } else {
            for (topic_tmp = help->related_topics; topic_tmp->next != NULL; topic_tmp = topic_tmp->next)
                ;

            topic_tmp->next = topic;
            topic->next = NULL;
            }
        }

        case 'S':
        KEY("Security",	help->security,		fread_number(fp));
        break;

        case 'T':
        if (!str_cmp(word, "Text")) {
            fMatch = true;

            help->text = fread_string(fp);
        }

        break;
    }

    if (!fMatch) {
        pbugf(LOG_ERROR, "read_help_new: no match for word %s", word);
    
    }
    }

    // Fix up problems here. Mostly from old helpfiles being converted.
    normalize_help(help);

    return help;
}


