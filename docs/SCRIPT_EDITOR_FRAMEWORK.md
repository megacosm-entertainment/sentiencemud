# Script Editor Framework & Player-Accessible Scripting

**Author**: AI Analysis  
**Date**: January 28, 2026  
**Status**: Extension of Editor Framework Analysis  
**Related**: EDITOR_FRAMEWORK_ANALYSIS.md

---

## Executive Summary

This document extends the editor framework analysis to cover:
1. **Script editors** (mpedit, opedit, rpedit, tpedit, apedit, ipedit, dpedit) - modernizing to common framework
2. **Script error logging** - displaying compilation errors in editors for troubleshooting
3. **Show commands** (mpshow, opshow, rpshow, etc.) - standalone viewing without entering edit mode
4. **Player-accessible scripting** - safe script templates for home/clan hall decoration

### Current State

**Script Editors Location**: `/sentience/src/editors/scripting/olc_mpcode.c` (1879 lines, all 7 editors in one file)

**Editors Included**:
- `mpedit` - Mobile (NPC) programs
- `opedit` - Object programs
- `rpedit` - Room programs
- `tpedit` - Token programs
- `apedit` - Area programs
- `ipedit` - Instance programs
- `dpedit` - Dungeon programs

**Current Pattern**: Custom dispatcher, not using `process_olc_command()` framework

**Current Show Function**: `scriptedit_show()` - basic display, **no standalone show commands**

**Error Handling**: Compilation errors go to `compile_err_buffer` but **not displayed in editor UI**

---

## Part 1: Current Script Editor Architecture

### 1.1 Existing Implementation

**File**: `src/editors/scripting/olc_mpcode.c`

**Command Tables** (all identical structure):
```c
const struct olc_cmd_type mpedit_table[] = {
    { "commands",  show_commands       },
    { "list",      mpedit_list         },
    { "create",    mpedit_create       },
    { "code",      scriptedit_code     },
    { "show",      scriptedit_show     },
    { "comments",  scriptedit_comments },
    { "compile",   scriptedit_compile  },
    { "name",      scriptedit_name     },
    { "flags",     scriptedit_flags    },
    { "depth",     scriptedit_depth    },
    { "security",  scriptedit_security },
    { "?",         show_help           },
    { NULL,        0                   }
};

// opedit_table, rpedit_table, tpedit_table, apedit_table, ipedit_table, dpedit_table
// All have the same structure, only list/create functions differ
```

**Editor Functions** (custom dispatcher):
```c
void mpedit(CHAR_DATA *ch, char *argument) {
    SCRIPT_DATA *script;
    char command[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int cmd;

    EDIT_MPCODE(ch, script);

    strcpy(arg, argument);
    smash_tilde(argument);
    argument = one_argument(argument, command);

    if (!str_cmp(command, "done")) {
        edit_done(ch);
        return;
    }

    if (command[0] == '\0') {
        scriptedit_show(ch, argument);
        return;
    }

    // Manual command dispatch loop
    for (cmd = 0; mpedit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, mpedit_table[cmd].name)) {
            if ((*mpedit_table[cmd].olc_fun)(ch, argument))
                SET_BIT(script->flags, SCRIPT_CHANGED);
            return;
        }
    }

    // Interpret if not found
    interpret(ch, arg);
    return;
}

// opedit(), rpedit(), tpedit(), apedit(), ipedit(), dpedit() all identical pattern
```

### 1.2 Current Show Function

**Function**: `scriptedit_show()`

**Current Output** (from your example):
```
[INQUIRY] (Sentience Legacy Server [SECURE])
[RpEdit][11001] Room: 11001 (The Beginning) - Tieryo>


Name:       []
Vnum:       [11001]
Call Depth: [Default (15)]
Security:   [0]
Flags       [none]
Code:
room echoaround $n $n bumps into the wall.

room echoat $n You bump into the wall.



-----
Builders' Comments:

-----
```

**Problems**:
1. **No visual hierarchy** - flat text dump
2. **No color coding** - hard to read code sections
3. **No error display** - compilation errors not shown
4. **No last compiled info** - no timestamp, compiler, or error count
5. **No standalone show** - must enter editor to view

### 1.3 Compilation Error System

**Error Functions** (`src/script_comp.c`):
```c
static BUFFER *compile_err_buffer = NULL;
static int compile_current_line = 0;

void compile_error(char *msg) {
    char buf[MSL];
    if (compile_err_buffer) {
        add_buf(compile_err_buffer, msg);
        add_buf(compile_err_buffer, "\n\r");
    }
    sprintf(buf, "SCRIPT ERROR: %s", msg);
    bug(buf, 0);
}

void compile_error_show(char *msg) {
    char buf[MSL];
    if (compile_err_buffer) {
        add_buf(compile_err_buffer, msg);
        add_buf(compile_err_buffer, "\n\r");
    } else {
        sprintf(buf, "SCRIPT ERROR: %s", msg);
        bug(buf, 0);
    }
}
```

**Error Examples**:
```c
// Line number tracking
compile_current_line = line_number;

// Various error messages
"Line %d: Invalid $() field '%s'."
"Line %d: Missing terminating ']'."
"Line %d: Invalid ifcheck '%s'."
"Line %d: Expecting an operator."
"Line %d: Division by zero."
"Line %d: Invalid expression."
"Line %d: Unmatched right parenthesis."
```

**Error Buffer Usage**:
```c
bool compile_script(BUFFER *err_buf, SCRIPT_DATA *script, char *source, int type) {
    compile_err_buffer = err_buf;  // Set global buffer
    
    // ... compilation logic with compile_error_show() calls
    
    if (errors > 0) {
        sprintf(rbuf, "%s(%d) encountered %d error%s.", 
            type_name, script->vnum, errors, (errors == 1 ? "" : "s"));
        compile_error(rbuf);
        return false;
    }
    
    return true;
}
```

**Problem**: Error buffer is used during compilation, but **not persisted or displayed in editor**.

---

## Part 2: Modernized Script Editor Design

### 2.1 Migrate to Common Framework

**Goal**: Use `process_olc_command()` to eliminate duplicate code

**Before** (7 identical functions):
```c
void mpedit(CHAR_DATA *ch, char *argument) { /* 20 lines of dispatch code */ }
void opedit(CHAR_DATA *ch, char *argument) { /* 20 lines of dispatch code */ }
void rpedit(CHAR_DATA *ch, char *argument) { /* 20 lines of dispatch code */ }
void tpedit(CHAR_DATA *ch, char *argument) { /* 20 lines of dispatch code */ }
void apedit(CHAR_DATA *ch, char *argument) { /* 20 lines of dispatch code */ }
void ipedit(CHAR_DATA *ch, char *argument) { /* 20 lines of dispatch code */ }
void dpedit(CHAR_DATA *ch, char *argument) { /* 20 lines of dispatch code */ }
```

**After** (7 one-line wrappers):
```c
void mpedit(CHAR_DATA *ch, char *argument) {
    process_olc_command(ch, argument, mpedit_table, scriptedit_show, 
        (void (*)(void *, bool))mark_script_changed);
}

void opedit(CHAR_DATA *ch, char *argument) {
    process_olc_command(ch, argument, opedit_table, scriptedit_show,
        (void (*)(void *, bool))mark_script_changed);
}

// ... same for rpedit, tpedit, apedit, ipedit, dpedit
```

**Helper Function**:
```c
void mark_script_changed(CHAR_DATA *ch, bool changed) {
    SCRIPT_DATA *script = (SCRIPT_DATA *)ch->desc->pEdit;
    if (changed && script) {
        SET_BIT(script->flags, SCRIPT_CHANGED);
    }
}
```

**Savings**: Removes ~140 lines of duplicate dispatch code

---

### 2.2 Enhanced Show Function

**New Design**: Structured, colorized, error-aware display

**Proposed Implementation**:
```c
SCRIPTEDIT(scriptedit_show) {
    SCRIPT_DATA *script;
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    const char *type_name;
    int i;
    
    EDIT_SCRIPT(ch, script);
    
    buffer = new_buf();
    
    // Determine script type name
    switch (script->type) {
        case PRG_MPROG:  type_name = "Mobile";   break;
        case PRG_OPROG:  type_name = "Object";   break;
        case PRG_RPROG:  type_name = "Room";     break;
        case PRG_TPROG:  type_name = "Token";    break;
        case PRG_APROG:  type_name = "Area";     break;
        case PRG_IPROG:  type_name = "Instance"; break;
        case PRG_DPROG:  type_name = "Dungeon";  break;
        default:         type_name = "Unknown";  break;
    }
    
    // Header with visual separator
    add_buf(buffer, formatf("{Y╔════════════════════════════════════════════════════════════════════════════╗{x\n\r"));
    add_buf(buffer, formatf("{Y║{W %-74s {Y║{x\n\r", 
        formatf("%s Program: %d", type_name, script->vnum)));
    add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
    
    // Basic info section
    add_buf(buffer, formatf("{Y║{C Name:        {W%-62.62s{Y║{x\n\r", 
        script->name && script->name[0] ? script->name : "{D(unnamed){x"));
    add_buf(buffer, formatf("{Y║{C Vnum:        {W%-62d{Y║{x\n\r", script->vnum));
    add_buf(buffer, formatf("{Y║{C Call Depth:  {W%-62s{Y║{x\n\r", 
        script->call_depth > 0 ? formatf("%d", script->call_depth) : "Default (15)"));
    add_buf(buffer, formatf("{Y║{C Security:    {W%-62d{Y║{x\n\r", script->security));
    
    // Flags
    if (script->flags) {
        add_buf(buffer, formatf("{Y║{C Flags:       {W%-62s{Y║{x\n\r", 
            flag_string(script_flags, script->flags)));
    } else {
        add_buf(buffer, formatf("{Y║{C Flags:       {D%-62s{Y║{x\n\r", "none"));
    }
    
    // Compilation status section
    add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
    add_buf(buffer, formatf("{Y║{W Status Information                                                       {Y║{x\n\r"));
    add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
    
    if (script->code && script->lines > 0) {
        add_buf(buffer, formatf("{Y║{C Status:      {G%-62s{Y║{x\n\r", "Compiled"));
        add_buf(buffer, formatf("{Y║{C Lines:       {W%-62d{Y║{x\n\r", script->lines));
        
        // Last compiled timestamp (if available)
        if (script->last_compiled > 0) {
            char time_buf[80];
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", 
                localtime(&script->last_compiled));
            add_buf(buffer, formatf("{Y║{C Compiled:    {W%-62s{Y║{x\n\r", time_buf));
        }
        
        // Compiled by (if available)
        if (script->compiled_by && script->compiled_by[0]) {
            add_buf(buffer, formatf("{Y║{C Compiled By: {W%-62s{Y║{x\n\r", script->compiled_by));
        }
    } else if (script->source && script->source[0]) {
        add_buf(buffer, formatf("{Y║{C Status:      {Y%-62s{Y║{x\n\r", "Uncompiled"));
    } else {
        add_buf(buffer, formatf("{Y║{C Status:      {D%-62s{Y║{x\n\r", "Blank"));
    }
    
    // Error display section (if compilation failed)
    if (script->last_errors && script->last_errors[0]) {
        add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
        add_buf(buffer, formatf("{Y║{R Compilation Errors                                                       {Y║{x\n\r"));
        add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
        
        // Display errors (word-wrapped to 74 chars)
        char *error_line = script->last_errors;
        char *next_line;
        while (error_line && *error_line) {
            next_line = strchr(error_line, '\n');
            if (next_line) {
                *next_line = '\0';
                add_buf(buffer, formatf("{Y║{R %-74.74s{Y║{x\n\r", error_line));
                error_line = next_line + 1;
                if (*error_line == '\r') error_line++;
            } else {
                add_buf(buffer, formatf("{Y║{R %-74.74s{Y║{x\n\r", error_line));
                break;
            }
        }
    }
    
    // Code section
    add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
    add_buf(buffer, formatf("{Y║{W Code                                                                      {Y║{x\n\r"));
    add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
    
    if (script->source && script->source[0]) {
        // Display source code with line numbers
        char *line = script->source;
        char *next;
        int line_num = 1;
        char line_buf[MAX_STRING_LENGTH];
        
        while (line && *line) {
            next = strchr(line, '\n');
            if (next) {
                int len = next - line;
                if (len > 70) len = 70;  // Truncate long lines
                strncpy(line_buf, line, len);
                line_buf[len] = '\0';
                add_buf(buffer, formatf("{Y║{x {c%3d{x │ {W%-68.68s{Y║{x\n\r", line_num, line_buf));
                line = next + 1;
                if (*line == '\r') line++;
            } else {
                add_buf(buffer, formatf("{Y║{x {c%3d{x │ {W%-68.68s{Y║{x\n\r", line_num, line));
                break;
            }
            line_num++;
        }
    } else {
        add_buf(buffer, formatf("{Y║{D %-74s {Y║{x\n\r", "(no code)"));
    }
    
    // Comments section
    add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
    add_buf(buffer, formatf("{Y║{W Builder Comments                                                         {Y║{x\n\r"));
    add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
    
    if (script->comments && script->comments[0]) {
        char *comment_line = script->comments;
        char *next_comment;
        while (comment_line && *comment_line) {
            next_comment = strchr(comment_line, '\n');
            if (next_comment) {
                *next_comment = '\0';
                add_buf(buffer, formatf("{Y║{x %-74.74s{Y║{x\n\r", comment_line));
                comment_line = next_comment + 1;
                if (*comment_line == '\r') comment_line++;
            } else {
                add_buf(buffer, formatf("{Y║{x %-74.74s{Y║{x\n\r", comment_line));
                break;
            }
        }
    } else {
        add_buf(buffer, formatf("{Y║{D %-74s {Y║{x\n\r", "(no comments)"));
    }
    
    // Footer
    add_buf(buffer, formatf("{Y╚════════════════════════════════════════════════════════════════════════════╝{x\n\r"));
    
    // Usage hint
    add_buf(buffer, "\n\r{CCommands:{x code, compile, name, flags, depth, security, comments\n\r");
    add_buf(buffer, "{CType '{Whelp <command>{x' for syntax, or '{Wcommands{x' for full list.\n\r");
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return false;
}
```

**Visual Example** (new output):
```
╔════════════════════════════════════════════════════════════════════════════╗
║ Room Program: 11001                                                        ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Name:        bump_wall_trigger                                             ║
║ Vnum:        11001                                                         ║
║ Call Depth:  Default (15)                                                  ║
║ Security:    0                                                             ║
║ Flags:       none                                                          ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Status Information                                                         ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Status:      Compiled                                                      ║
║ Lines:       2                                                             ║
║ Compiled:    2026-01-28 14:32:15                                           ║
║ Compiled By: Tieryo                                                        ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Code                                                                       ║
╠════════════════════════════════════════════════════════════════════════════╣
║   1 │ room echoaround $n $n bumps into the wall.                          ║
║   2 │ room echoat $n You bump into the wall.                              ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Builder Comments                                                           ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Triggered when player tries to move into an impassable wall.              ║
╚════════════════════════════════════════════════════════════════════════════╝

Commands: code, compile, name, flags, depth, security, comments
Type 'help <command>' for syntax, or 'commands' for full list.
```

**With Errors**:
```
╠════════════════════════════════════════════════════════════════════════════╣
║ Status Information                                                         ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Status:      Uncompiled                                                    ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Compilation Errors                                                         ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Line 3: Invalid $() field 'targit'. Did you mean 'target'?                ║
║ Line 5: Missing terminating ']' in expression.                            ║
║ room(11001) encountered 2 errors.                                          ║
╠════════════════════════════════════════════════════════════════════════════╣
```

---

### 2.3 Standalone Show Commands

**Purpose**: View script without entering edit mode

**Implementation**:
```c
// Mobile program show
void do_mpshow(CHAR_DATA *ch, char *argument) {
    SCRIPT_DATA *script;
    int vnum;
    
    if (IS_NPC(ch) || argument[0] == '\0') {
        send_to_char("Syntax: mpshow <vnum>\n\r", ch);
        return;
    }
    
    vnum = atoi(argument);
    script = get_mprog_index(vnum);
    
    if (!script) {
        send_to_char("No such mobile program.\n\r", ch);
        return;
    }
    
    // Use shared show function
    olc_show_item(ch, script, scriptedit_show, "");
}

// Object program show
void do_opshow(CHAR_DATA *ch, char *argument) {
    SCRIPT_DATA *script;
    int vnum;
    
    if (IS_NPC(ch) || argument[0] == '\0') {
        send_to_char("Syntax: opshow <vnum>\n\r", ch);
        return;
    }
    
    vnum = atoi(argument);
    script = get_oprog_index(vnum);
    
    if (!script) {
        send_to_char("No such object program.\n\r", ch);
        return;
    }
    
    olc_show_item(ch, script, scriptedit_show, "");
}

// Room program show
void do_rpshow(CHAR_DATA *ch, char *argument) {
    SCRIPT_DATA *script;
    int vnum;
    
    if (IS_NPC(ch) || argument[0] == '\0') {
        send_to_char("Syntax: rpshow <vnum>\n\r", ch);
        return;
    }
    
    vnum = atoi(argument);
    script = get_rprog_index(vnum);
    
    if (!script) {
        send_to_char("No such room program.\n\r", ch);
        return;
    }
    
    olc_show_item(ch, script, scriptedit_show, "");
}

// Token program show
void do_tpshow(CHAR_DATA *ch, char *argument) {
    SCRIPT_DATA *script;
    int vnum;
    
    if (IS_NPC(ch) || argument[0] == '\0') {
        send_to_char("Syntax: tpshow <vnum>\n\r", ch);
        return;
    }
    
    vnum = atoi(argument);
    script = get_tprog_index(vnum);
    
    if (!script) {
        send_to_char("No such token program.\n\r", ch);
        return;
    }
    
    olc_show_item(ch, script, scriptedit_show, "");
}

// Area program show
void do_apshow(CHAR_DATA *ch, char *argument) {
    SCRIPT_DATA *script;
    int vnum;
    
    if (IS_NPC(ch) || argument[0] == '\0') {
        send_to_char("Syntax: apshow <vnum>\n\r", ch);
        return;
    }
    
    vnum = atoi(argument);
    script = get_aprog_index(vnum);
    
    if (!script) {
        send_to_char("No such area program.\n\r", ch);
        return;
    }
    
    olc_show_item(ch, script, scriptedit_show, "");
}

// Instance program show
void do_ipshow(CHAR_DATA *ch, char *argument) {
    SCRIPT_DATA *script;
    int vnum;
    
    if (IS_NPC(ch) || argument[0] == '\0') {
        send_to_char("Syntax: ipshow <vnum>\n\r", ch);
        return;
    }
    
    vnum = atoi(argument);
    script = get_iprog_index(vnum);
    
    if (!script) {
        send_to_char("No such instance program.\n\r", ch);
        return;
    }
    
    olc_show_item(ch, script, scriptedit_show, "");
}

// Dungeon program show
void do_dpshow(CHAR_DATA *ch, char *argument) {
    SCRIPT_DATA *script;
    int vnum;
    
    if (IS_NPC(ch) || argument[0] == '\0') {
        send_to_char("Syntax: dpshow <vnum>\n\r", ch);
        return;
    }
    
    vnum = atoi(argument);
    script = get_dprog_index(vnum);
    
    if (!script) {
        send_to_char("No such dungeon program.\n\r", ch);
        return;
    }
    
    olc_show_item(ch, script, scriptedit_show, "");
}
```

**Add to Command Table** (`interp.c`):
```c
{ "mpshow",      do_mpshow,      POS_DEAD,  ML, LOG_NORMAL, true, false },
{ "opshow",      do_opshow,      POS_DEAD,  ML, LOG_NORMAL, true, false },
{ "rpshow",      do_rpshow,      POS_DEAD,  ML, LOG_NORMAL, true, false },
{ "tpshow",      do_tpshow,      POS_DEAD,  ML, LOG_NORMAL, true, false },
{ "apshow",      do_apshow,      POS_DEAD,  ML, LOG_NORMAL, true, false },
{ "ipshow",      do_ipshow,      POS_DEAD,  ML, LOG_NORMAL, true, false },
{ "dpshow",      do_dpshow,      POS_DEAD,  ML, LOG_NORMAL, true, false },
```

---

### 2.4 Persistent Error Logging

**Goal**: Store compilation errors in SCRIPT_DATA for display in editor

**Add to SCRIPT_DATA Structure**:
```c
struct script_data {
    // ... existing fields
    char    *source;           // Source code
    int     lines;             // Compiled line count
    SCRIPT_CODE *code;         // Compiled bytecode
    
    // NEW: Error tracking
    char    *last_errors;      // Last compilation errors
    time_t  last_compiled;     // Timestamp of last successful compile
    char    *compiled_by;      // Who compiled it
    int     error_count;       // Number of errors in last compile
};
```

**Modified Compile Function**:
```c
bool compile_script(BUFFER *err_buf, SCRIPT_DATA *script, char *source, int type) {
    compile_err_buffer = err_buf;
    compile_current_line = 0;
    int errors = 0;
    
    // ... compilation logic with error tracking
    
    if (errors > 0) {
        // Store errors in script structure
        if (script->last_errors)
            free_string(script->last_errors);
        script->last_errors = str_dup(buf_string(err_buf));
        script->error_count = errors;
        
        sprintf(rbuf, "%s(%d) encountered %d error%s.", 
            type_name, script->vnum, errors, (errors == 1 ? "" : "s"));
        compile_error(rbuf);
        return false;
    }
    
    // Clear errors on successful compile
    if (script->last_errors) {
        free_string(script->last_errors);
        script->last_errors = NULL;
    }
    script->error_count = 0;
    
    // Update compilation metadata
    script->last_compiled = current_time;
    if (script->compiled_by)
        free_string(script->compiled_by);
    script->compiled_by = str_dup(ch->name);  // Need to pass ch to function
    
    return true;
}
```

**Compile Command Enhancement**:
```c
SCRIPTEDIT(scriptedit_compile) {
    SCRIPT_DATA *script;
    BUFFER *err_buf;
    bool success;
    
    EDIT_SCRIPT(ch, script);
    
    if (!script->source || !script->source[0]) {
        send_to_char("No code to compile.\n\r", ch);
        return false;
    }
    
    // Create error buffer
    err_buf = new_buf();
    
    // Compile with error tracking
    success = compile_script(err_buf, script, script->source, script->type);
    
    if (success) {
        send_to_char("{GScript compiled successfully.{x\n\r", ch);
        SET_BIT(script->flags, SCRIPT_CHANGED);
    } else {
        send_to_char("{RScript compilation failed. Errors:{x\n\r", ch);
        send_to_char(buf_string(err_buf), ch);
        send_to_char("\n\rType '{Wshow{x' to see errors in editor display.\n\r", ch);
    }
    
    free_buf(err_buf);
    return success;
}
```

---

## Part 3: Player-Accessible Scripting

### 3.1 Security Model

**Challenge**: Scripts have full system access (echo commands, teleport, give items, etc.)

**Solution**: Pre-approved **template system** with limited, safe commands

**Template Categories**:
1. **Ambiance** - Atmospheric messages (echoes, weather, lighting)
2. **Decoration** - Visual effects (object descriptions, room extras)
3. **Interaction** - Simple responses (greet, thank, react to keywords)

**Forbidden Commands** (security risk):
- `transfer` - teleportation
- `give` - item creation/duplication
- `force` - force commands on players
- `at` - execute at remote locations
- `goto` - unrestricted movement
- Any command accessing global variables
- Any command modifying areas/vnums

### 3.2 Template Structure

**Template Definition**:
```c
typedef struct script_template {
    char    *name;              // Template name (e.g., "echo_greeting")
    char    *category;          // Category (ambiance, decoration, interaction)
    char    *description;       // What this template does
    char    *source;            // Template source code with placeholders
    int     security_level;     // Required security (0 = players, 1+ = builders)
    char    **allowed_commands; // Whitelist of commands
    char    **parameters;       // List of parameters to fill in
    char    *example;           // Usage example
} SCRIPT_TEMPLATE;
```

**Example Templates**:

**Template 1: Simple Echo**
```c
SCRIPT_TEMPLATE ambiance_echo = {
    .name = "ambiance_echo",
    .category = "Ambiance",
    .description = "Echoes a message to the room at random intervals",
    .source = 
        "if rand(100) < ${CHANCE}\n"
        "  room echoaround * ${MESSAGE}\n"
        "endif",
    .security_level = 0,
    .allowed_commands = (char*[]){"echoaround", NULL},
    .parameters = (char*[]){"CHANCE", "MESSAGE", NULL},
    .example = "CHANCE=5, MESSAGE=A gentle breeze rustles the curtains."
};
```

**Template 2: Greeting**
```c
SCRIPT_TEMPLATE interaction_greet = {
    .name = "interaction_greet",
    .category = "Interaction",
    .description = "Greets a player when they enter the room",
    .source =
        "if ispc($n)\n"
        "  room echoat $n ${MESSAGE}\n"
        "endif",
    .security_level = 0,
    .allowed_commands = (char*[]){"echoat", NULL},
    .parameters = (char*[]){"MESSAGE", NULL},
    .example = "MESSAGE=Welcome to your home, $n!"
};
```

**Template 3: Weather Effect**
```c
SCRIPT_TEMPLATE ambiance_weather = {
    .name = "ambiance_weather",
    .category = "Ambiance",
    .description = "Displays weather-related messages at random",
    .source =
        "if rand(100) < ${CHANCE}\n"
        "  switch $(weather.type)\n"
        "    case 0\n"
        "      room echoaround * ${CLEAR_MESSAGE}\n"
        "    case 1\n"
        "      room echoaround * ${CLOUDY_MESSAGE}\n"
        "    case 2\n"
        "      room echoaround * ${RAIN_MESSAGE}\n"
        "    case 3\n"
        "      room echoaround * ${STORM_MESSAGE}\n"
        "  endswitch\n"
        "endif",
    .security_level = 0,
    .allowed_commands = (char*[]){"echoaround", NULL},
    .parameters = (char*[]){"CHANCE", "CLEAR_MESSAGE", "CLOUDY_MESSAGE", 
        "RAIN_MESSAGE", "STORM_MESSAGE", NULL},
    .example = "CHANCE=10, CLEAR_MESSAGE=Sunlight streams through the windows."
};
```

**Template 4: Time of Day**
```c
SCRIPT_TEMPLATE ambiance_timeofday = {
    .name = "ambiance_timeofday",
    .category = "Ambiance",
    .description = "Different messages based on time of day",
    .source =
        "if rand(100) < ${CHANCE}\n"
        "  if $(time.hour) >= 6 && $(time.hour) < 12\n"
        "    room echoaround * ${MORNING_MESSAGE}\n"
        "  elseif $(time.hour) >= 12 && $(time.hour) < 18\n"
        "    room echoaround * ${AFTERNOON_MESSAGE}\n"
        "  elseif $(time.hour) >= 18 && $(time.hour) < 22\n"
        "    room echoaround * ${EVENING_MESSAGE}\n"
        "  else\n"
        "    room echoaround * ${NIGHT_MESSAGE}\n"
        "  endif\n"
        "endif",
    .security_level = 0,
    .allowed_commands = (char*[]){"echoaround", NULL},
    .parameters = (char*[]){"CHANCE", "MORNING_MESSAGE", "AFTERNOON_MESSAGE",
        "EVENING_MESSAGE", "NIGHT_MESSAGE", NULL},
    .example = "CHANCE=5, MORNING_MESSAGE=Morning light fills the room."
};
```

**Template 5: NPC Simple Response**
```c
SCRIPT_TEMPLATE mob_simple_response = {
    .name = "mob_simple_response",
    .category = "Interaction",
    .description = "Mobile responds to a specific keyword",
    .source =
        "if ispc($n)\n"
        "  if strstr(\"${KEYWORD}\", tolower($t))\n"
        "    mob emote ${ACTION}\n"
        "    mob say ${RESPONSE}\n"
        "  endif\n"
        "endif",
    .security_level = 0,
    .allowed_commands = (char*[]){"emote", "say", NULL},
    .parameters = (char*[]){"KEYWORD", "ACTION", "RESPONSE", NULL},
    .example = "KEYWORD=hello, ACTION=smiles warmly, RESPONSE=Good day to you!"
};
```

**Template 6: Object Description Changer**
```c
SCRIPT_TEMPLATE obj_desc_cycle = {
    .name = "obj_desc_cycle",
    .category = "Decoration",
    .description = "Cycles object description between options",
    .source =
        "if rand(100) < ${CHANCE}\n"
        "  setvar cycle $(object.v0)\n"
        "  if $<cycle> == 0\n"
        "    obj extradesc ${KEYWORD} ${DESC1}\n"
        "    setvar cycle 1\n"
        "  elseif $<cycle> == 1\n"
        "    obj extradesc ${KEYWORD} ${DESC2}\n"
        "    setvar cycle 2\n"
        "  else\n"
        "    obj extradesc ${KEYWORD} ${DESC3}\n"
        "    setvar cycle 0\n"
        "  endif\n"
        "  obj v0 $<cycle>\n"
        "endif",
    .security_level = 1,  // Builders only (modifies object)
    .allowed_commands = (char*[]){"extradesc", NULL},
    .parameters = (char*[]){"CHANCE", "KEYWORD", "DESC1", "DESC2", "DESC3", NULL},
    .example = "KEYWORD=fireplace, DESC1=The fire crackles merrily."
};
```

### 3.3 Template System Commands

**Command 1: List Templates**
```c
void do_templates(CHAR_DATA *ch, char *argument) {
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    int i, count = 0;
    
    if (IS_NPC(ch)) return;
    
    buffer = new_buf();
    
    add_buf(buffer, "{W╔════════════════════════════════════════════════════════════════════════════╗{x\n\r");
    add_buf(buffer, "{W║{Y                        Available Script Templates                         {W║{x\n\r");
    add_buf(buffer, "{W╠══════════════════════╤═══════════════╤═══════════════════════════════════╣{x\n\r");
    add_buf(buffer, "{W║{C Name                 {W│{C Category      {W│{C Description                    {W║{x\n\r");
    add_buf(buffer, "{W╠══════════════════════╪═══════════════╪═══════════════════════════════════╣{x\n\r");
    
    for (i = 0; script_template_table[i].name != NULL; i++) {
        SCRIPT_TEMPLATE *tmpl = &script_template_table[i];
        
        // Check security
        if (tmpl->security_level > 0 && get_trust(ch) < ML)
            continue;
        
        sprintf(buf, "{W║{W %-20.20s {W│{G %-13.13s {W│{x %-33.33s {W║{x\n\r",
            tmpl->name, tmpl->category, tmpl->description);
        add_buf(buffer, buf);
        count++;
    }
    
    add_buf(buffer, "{W╚══════════════════════╧═══════════════╧═══════════════════════════════════╝{x\n\r");
    sprintf(buf, "\n\r{CFound %d template%s. Type '{Wtemplate <name>{C' for details.{x\n\r",
        count, count == 1 ? "" : "s");
    add_buf(buffer, buf);
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}
```

**Command 2: View Template**
```c
void do_template(CHAR_DATA *ch, char *argument) {
    SCRIPT_TEMPLATE *tmpl;
    BUFFER *buffer;
    char buf[MAX_STRING_LENGTH];
    int i;
    
    if (IS_NPC(ch) || argument[0] == '\0') {
        send_to_char("Syntax: template <name>\n\r", ch);
        send_to_char("        templates       (list all)\n\r", ch);
        return;
    }
    
    // Find template
    tmpl = NULL;
    for (i = 0; script_template_table[i].name != NULL; i++) {
        if (!str_prefix(argument, script_template_table[i].name)) {
            tmpl = &script_template_table[i];
            break;
        }
    }
    
    if (!tmpl) {
        send_to_char("No such template. Type 'templates' for a list.\n\r", ch);
        return;
    }
    
    // Check security
    if (tmpl->security_level > 0 && get_trust(ch) < ML) {
        send_to_char("That template requires builder access.\n\r", ch);
        return;
    }
    
    buffer = new_buf();
    
    add_buf(buffer, "{Y╔════════════════════════════════════════════════════════════════════════════╗{x\n\r");
    sprintf(buf, "{Y║{W %-74s {Y║{x\n\r", formatf("Template: %s", tmpl->name));
    add_buf(buffer, buf);
    add_buf(buffer, "{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r");
    
    sprintf(buf, "{Y║{C Category:    {W%-62.62s{Y║{x\n\r", tmpl->category);
    add_buf(buffer, buf);
    sprintf(buf, "{Y║{C Description: {W%-62.62s{Y║{x\n\r", tmpl->description);
    add_buf(buffer, buf);
    
    // Parameters
    add_buf(buffer, "{Y║{C Parameters:                                                               {Y║{x\n\r");
    for (i = 0; tmpl->parameters[i] != NULL; i++) {
        sprintf(buf, "{Y║{W   - %-70.70s{Y║{x\n\r", tmpl->parameters[i]);
        add_buf(buffer, buf);
    }
    
    // Example
    add_buf(buffer, "{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r");
    add_buf(buffer, "{Y║{C Example Usage:                                                            {Y║{x\n\r");
    sprintf(buf, "{Y║{W   %-72.72s{Y║{x\n\r", tmpl->example);
    add_buf(buffer, buf);
    
    // Source code
    add_buf(buffer, "{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r");
    add_buf(buffer, "{Y║{C Template Code:                                                            {Y║{x\n\r");
    add_buf(buffer, "{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r");
    
    char *line = tmpl->source;
    char *next;
    while (line && *line) {
        next = strchr(line, '\n');
        if (next) {
            *next = '\0';
            sprintf(buf, "{Y║{x %-74.74s{Y║{x\n\r", line);
            add_buf(buffer, buf);
            line = next + 1;
        } else {
            sprintf(buf, "{Y║{x %-74.74s{Y║{x\n\r", line);
            add_buf(buffer, buf);
            break;
        }
    }
    
    add_buf(buffer, "{Y╚════════════════════════════════════════════════════════════════════════════╝{x\n\r");
    add_buf(buffer, "\n\r{CTo use: '{Wscriptfrom <template> <param1>=<value1> <param2>=<value2> ...{x'\n\r");
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}
```

**Command 3: Create Script from Template**
```c
void do_scriptfrom(CHAR_DATA *ch, char *argument) {
    SCRIPT_TEMPLATE *tmpl;
    SCRIPT_DATA *script;
    char template_name[MAX_INPUT_LENGTH];
    char *source, *param_str;
    char *result_code;
    int i;
    
    if (IS_NPC(ch)) return;
    
    // Parse: scriptfrom <template> <param>=<value> <param>=<value> ...
    argument = one_argument(argument, template_name);
    
    if (template_name[0] == '\0') {
        send_to_char("Syntax: scriptfrom <template> <param>=<value> ...\n\r", ch);
        send_to_char("Example: scriptfrom ambiance_echo CHANCE=5 MESSAGE='The wind howls.'\n\r", ch);
        return;
    }
    
    // Find template
    tmpl = NULL;
    for (i = 0; script_template_table[i].name != NULL; i++) {
        if (!str_prefix(template_name, script_template_table[i].name)) {
            tmpl = &script_template_table[i];
            break;
        }
    }
    
    if (!tmpl) {
        send_to_char("No such template. Type 'templates' for a list.\n\r", ch);
        return;
    }
    
    // Check security
    if (tmpl->security_level > 0 && get_trust(ch) < ML) {
        send_to_char("That template requires builder access.\n\r", ch);
        return;
    }
    
    // Parse parameters
    char param_values[20][MAX_STRING_LENGTH];
    int param_count = 0;
    
    while (argument[0] != '\0' && param_count < 20) {
        char param[MAX_INPUT_LENGTH];
        char value[MAX_STRING_LENGTH];
        char *equals;
        
        argument = one_argument(argument, param);
        
        equals = strchr(param, '=');
        if (!equals) {
            send_to_char("Invalid parameter format. Use <param>=<value>.\n\r", ch);
            return;
        }
        
        *equals = '\0';
        strcpy(value, equals + 1);
        
        // Remove quotes if present
        if (value[0] == '\'' || value[0] == '"') {
            int len = strlen(value);
            if (value[len-1] == value[0]) {
                value[len-1] = '\0';
                memmove(value, value+1, len);
            }
        }
        
        // Store parameter
        sprintf(param_values[param_count], "${%s}=%s", param, value);
        param_count++;
    }
    
    // Validate all required parameters provided
    for (i = 0; tmpl->parameters[i] != NULL; i++) {
        bool found = false;
        for (int j = 0; j < param_count; j++) {
            if (strstr(param_values[j], tmpl->parameters[i])) {
                found = true;
                break;
            }
        }
        if (!found) {
            send_to_char(formatf("Missing required parameter: %s\n\r", tmpl->parameters[i]), ch);
            return;
        }
    }
    
    // Create script code by replacing placeholders
    result_code = str_dup(tmpl->source);
    for (i = 0; i < param_count; i++) {
        char placeholder[MAX_INPUT_LENGTH];
        char replacement[MAX_STRING_LENGTH];
        char *equals = strchr(param_values[i], '=');
        
        strncpy(placeholder, param_values[i], equals - param_values[i]);
        placeholder[equals - param_values[i]] = '\0';
        strcpy(replacement, equals + 1);
        
        // Replace all occurrences of placeholder with value
        result_code = string_replace(result_code, placeholder, replacement);
    }
    
    send_to_char("{GScript created from template:{x\n\r\n\r", ch);
    send_to_char(result_code, ch);
    send_to_char("\n\r{YCopy this code and use '{Wcode{Y' command in your script editor.{x\n\r", ch);
    
    free_string(result_code);
}
```

### 3.4 Player Script Editor Integration

**Modified Entry Point**:
```c
void do_homeprogram(CHAR_DATA *ch, char *argument) {
    ROOM_INDEX_DATA *room;
    SCRIPT_DATA *script;
    int vnum;
    
    if (IS_NPC(ch)) return;
    
    // Validate player owns this room
    room = ch->in_room;
    
    if (!IS_SET(room->room_flags, ROOM_HOUSE) || 
        !room->owner || str_cmp(room->owner, ch->name)) {
        send_to_char("You can only edit programs in rooms you own.\n\r", ch);
        return;
    }
    
    // Get or create room program
    if (argument[0] == '\0') {
        // List existing programs on this room
        send_to_char("Room programs attached to this room:\n\r", ch);
        // ... list programs
        send_to_char("\n\rSyntax: homeprogram <vnum>        - edit program\n\r", ch);
        send_to_char("        homeprogram create        - create from template\n\r", ch);
        return;
    }
    
    if (!str_cmp(argument, "create")) {
        send_to_char("Use '{Wscriptfrom <template>{x' to create a script from a template.\n\r", ch);
        send_to_char("Then use '{Whomeprogram <vnum>{x' to attach it to this room.\n\r", ch);
        send_to_char("Type '{Wtemplates{x' for a list of available templates.\n\r", ch);
        return;
    }
    
    vnum = atoi(argument);
    script = get_rprog_index(vnum);
    
    if (!script) {
        send_to_char("No such room program.\n\r", ch);
        return;
    }
    
    // Validate script is safe (created from template)
    if (!IS_SET(script->flags, SCRIPT_FROM_TEMPLATE)) {
        send_to_char("You can only edit scripts created from templates.\n\r", ch);
        send_to_char("Use '{Wscriptfrom <template>{x' to create a new script.\n\r", ch);
        return;
    }
    
    // Enter limited editor mode
    ch->desc->pEdit = (void *)script;
    ch->desc->editor = ED_RPCODE;
    ch->desc->player_edit_perm = &rprog_player_permission;  // Set permission
    
    send_to_char("Entering room program editor (limited mode).\n\r", ch);
    scriptedit_show(ch, "");
}
```

**Player Permission Structure**:
```c
PLAYER_EDIT_PERMISSION rprog_player_permission = {
    .editor_type = ED_RPCODE,
    .can_create = false,
    .can_delete = false,
    .allowed_flags = SCRIPT_FROM_TEMPLATE,
    .forbidden_flags = SCRIPT_CHANGED | SCRIPT_DISABLED,
    .validate_func = validate_player_script_edit
};

bool validate_player_script_edit(CHAR_DATA *ch, void *pData) {
    SCRIPT_DATA *script = (SCRIPT_DATA *)pData;
    
    // Must be from template
    if (!IS_SET(script->flags, SCRIPT_FROM_TEMPLATE))
        return false;
    
    // Must not be able to modify dangerous commands
    // ... check script source for forbidden commands
    
    return true;
}
```

---

## Part 4: Runtime Error Reporting

### 4.1 Current Runtime Error System

**Existing Function**: `scriptcmd_bug()` in `scripts.c`

```c
void scriptcmd_bug(SCRIPT_VARINFO *info, char *message)
{
    char buf[2 * MSL];
    
    sprintf(buf, "Script:%ld#%d:Line:%d:%s\n\r",
        info->block->script->area->uid, info->block->script->vnum,
        info->block->line,
        message);
    bug(buf, 0);
}
```

**Current Behavior**:
- Runtime errors logged to `bug()` (logs file only)
- Format: `Script:<area_uid>#<vnum>:Line:<line>:<message>`
- **Not visible to builders in editor**
- **Not captured for troubleshooting**

**Common Runtime Errors**:
- Invalid entity references (`$target` doesn't exist)
- Type mismatches (expecting string, got number)
- Null pointer access (room/mob/obj destroyed during execution)
- Call depth exceeded (infinite recursion)
- Invalid command parameters (wrong argument types)
- Security violations (restricted commands with insufficient security)

### 4.2 Enhanced Runtime Error Tracking

**Problem**: Builders have no visibility into why scripts fail at runtime

**Solution**: Capture last N runtime errors per script, display in editor

**Add to SCRIPT_DATA Structure**:
```c
struct script_data {
    // Existing fields...
    char    *last_errors;       // Compilation errors
    time_t  last_compiled;      
    char    *compiled_by;       
    int     error_count;        // Compilation error count
    
    // NEW: Runtime error tracking
    SCRIPT_RUNTIME_ERROR *runtime_errors;  // Circular buffer of last 10 errors
    int     runtime_error_head;            // Head index in circular buffer
    int     runtime_error_count;           // Total runtime errors (all time)
    time_t  last_runtime_error;            // Timestamp of most recent error
};

struct script_runtime_error {
    time_t  timestamp;          // When error occurred
    int     line;               // Line number where error occurred
    char    *message;           // Error message
    char    *trigger_type;      // What triggered script (e.g., "entry", "speech")
    long    actor_vnum;         // Who/what triggered (mob/obj/char vnum)
    char    *actor_name;        // Name of triggering entity
};

#define MAX_RUNTIME_ERRORS 10   // Keep last 10 errors
```

**Modified `scriptcmd_bug()` Function**:
```c
void scriptcmd_bug(SCRIPT_VARINFO *info, char *message)
{
    char buf[2 * MSL];
    SCRIPT_DATA *script;
    SCRIPT_RUNTIME_ERROR *err;
    
    // Existing bug logging
    sprintf(buf, "Script:%ld#%d:Line:%d:%s\n\r",
        info->block->script->area->uid, info->block->script->vnum,
        info->block->line,
        message);
    bug(buf, 0);
    
    // NEW: Capture for editor display
    script = info->block->script;
    if (!script) return;
    
    // Initialize runtime error buffer if needed
    if (!script->runtime_errors) {
        script->runtime_errors = calloc(MAX_RUNTIME_ERRORS, sizeof(SCRIPT_RUNTIME_ERROR));
        script->runtime_error_head = 0;
        script->runtime_error_count = 0;
    }
    
    // Get slot in circular buffer
    err = &script->runtime_errors[script->runtime_error_head];
    
    // Free old error data if slot was used
    if (err->message) free_string(err->message);
    if (err->trigger_type) free_string(err->trigger_type);
    if (err->actor_name) free_string(err->actor_name);
    
    // Store new error
    err->timestamp = current_time;
    err->line = info->block->line;
    err->message = str_dup(message);
    err->trigger_type = info->trigger ? str_dup(trigger_name(info->trigger_type)) : NULL;
    
    // Capture actor information
    if (info->mob) {
        err->actor_vnum = info->mob->pIndexData->vnum;
        err->actor_name = str_dup(info->mob->short_descr);
    } else if (info->obj) {
        err->actor_vnum = info->obj->pIndexData->vnum;
        err->actor_name = str_dup(info->obj->short_descr);
    } else if (info->room) {
        err->actor_vnum = info->room->vnum;
        err->actor_name = str_dup(info->room->name);
    } else {
        err->actor_vnum = 0;
        err->actor_name = str_dup("(unknown)");
    }
    
    // Advance circular buffer
    script->runtime_error_head = (script->runtime_error_head + 1) % MAX_RUNTIME_ERRORS;
    script->runtime_error_count++;
    script->last_runtime_error = current_time;
}
```

### 4.3 Runtime Error Display in Editor

**Enhanced `scriptedit_show()` - Runtime Section**:
```c
// Add after compilation status section, before code section

// Runtime error section (if any errors exist)
if (script->runtime_error_count > 0) {
    int display_count = UMIN(script->runtime_error_count, MAX_RUNTIME_ERRORS);
    int i, index;
    
    add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
    add_buf(buffer, formatf("{Y║{R Runtime Errors (last %d)                                                  {Y║{x\n\r", 
        display_count));
    add_buf(buffer, formatf("{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r"));
    
    // Display errors in reverse chronological order (newest first)
    for (i = 0; i < display_count; i++) {
        // Calculate index (walk backwards from head)
        index = (script->runtime_error_head - 1 - i + MAX_RUNTIME_ERRORS) % MAX_RUNTIME_ERRORS;
        SCRIPT_RUNTIME_ERROR *err = &script->runtime_errors[index];
        
        if (!err->message) continue;  // Empty slot
        
        // Format timestamp
        char time_buf[80];
        char *time_ptr = ctime(&err->timestamp);
        // Extract just "Jan 28 14:32" from ctime
        strncpy(time_buf, time_ptr + 4, 12);
        time_buf[12] = '\0';
        
        // Error header
        add_buf(buffer, formatf("{Y║{R Line %3d {D│{c %s {D│{W %-40.40s {Y║{x\n\r",
            err->line, time_buf, 
            err->trigger_type ? err->trigger_type : "manual"));
        
        // Error message (word-wrapped to 72 chars, indented)
        char *msg = err->message;
        char *next;
        while (msg && *msg) {
            next = strchr(msg, '\n');
            if (next) *next = '\0';
            
            add_buf(buffer, formatf("{Y║{x   {R%-72.72s{Y║{x\n\r", msg));
            
            if (!next) break;
            msg = next + 1;
            if (*msg == '\r') msg++;
        }
        
        // Actor info (who triggered the error)
        if (err->actor_name && err->actor_vnum > 0) {
            add_buf(buffer, formatf("{Y║{D   Actor: [%ld] %-60.60s{Y║{x\n\r",
                err->actor_vnum, err->actor_name));
        }
        
        // Separator between errors
        if (i < display_count - 1) {
            add_buf(buffer, formatf("{Y║{D ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ {Y║{x\n\r"));
        }
    }
    
    // Show total error count if > 10
    if (script->runtime_error_count > MAX_RUNTIME_ERRORS) {
        add_buf(buffer, formatf("{Y║{D   (%d total runtime errors, showing last %d)                             {Y║{x\n\r",
            script->runtime_error_count, MAX_RUNTIME_ERRORS));
    }
}
```

**Visual Example** (runtime errors in show):
```
╠════════════════════════════════════════════════════════════════════════════╣
║ Runtime Errors (last 3)                                                    ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Line  15 │ Jan 28 14:32 │ speech                                           ║
║   Invalid $() field 'targit'. Did you mean 'target'?                       ║
║   Actor: [3001] a city guard                                               ║
║ ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ ║
║ Line   8 │ Jan 28 14:15 │ entry                                            ║
║   Room does not exist: 4527                                                ║
║   Actor: [4001] Bob the Builder                                            ║
║ ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ ║
║ Line  22 │ Jan 28 13:58 │ give                                             ║
║   Object [1234] no longer exists                                           ║
║   Actor: [3001] a city guard                                               ║
╠════════════════════════════════════════════════════════════════════════════╣
```

### 4.4 Runtime Error Commands

**Command: Clear Runtime Errors**
```c
SCRIPTEDIT(scriptedit_clearerrors) {
    SCRIPT_DATA *script;
    int i;
    
    EDIT_SCRIPT(ch, script);
    
    if (!script->runtime_errors || script->runtime_error_count == 0) {
        send_to_char("No runtime errors to clear.\n\r", ch);
        return false;
    }
    
    // Free all error data
    for (i = 0; i < MAX_RUNTIME_ERRORS; i++) {
        SCRIPT_RUNTIME_ERROR *err = &script->runtime_errors[i];
        if (err->message) free_string(err->message);
        if (err->trigger_type) free_string(err->trigger_type);
        if (err->actor_name) free_string(err->actor_name);
        memset(err, 0, sizeof(SCRIPT_RUNTIME_ERROR));
    }
    
    script->runtime_error_head = 0;
    script->runtime_error_count = 0;
    script->last_runtime_error = 0;
    
    send_to_char("{GRuntime errors cleared.{x\n\r", ch);
    return false;  // Don't mark script as changed
}
```

**Add to Editor Command Tables**:
```c
const struct olc_cmd_type mpedit_table[] = {
    // ... existing commands
    { "clearerrors", scriptedit_clearerrors },
    // ...
};
// (same for opedit, rpedit, tpedit, apedit, ipedit, dpedit)
```

**Command: View Detailed Runtime Error**
```c
void do_scripterrors(CHAR_DATA *ch, char *argument) {
    SCRIPT_DATA *script;
    char type[MAX_INPUT_LENGTH];
    int vnum, i, index, display_count;
    BUFFER *buffer;
    
    if (IS_NPC(ch)) return;
    
    // Syntax: scripterrors <mp|op|rp|tp|ap|ip|dp> <vnum>
    argument = one_argument(argument, type);
    
    if (!type[0] || !argument[0]) {
        send_to_char("Syntax: scripterrors <type> <vnum>\n\r", ch);
        send_to_char("Types: mp, op, rp, tp, ap, ip, dp\n\r", ch);
        send_to_char("Example: scripterrors rp 11001\n\r", ch);
        return;
    }
    
    vnum = atoi(argument);
    
    // Get script based on type
    if (!str_prefix(type, "mp") || !str_prefix(type, "mobile")) {
        script = get_mprog_index(vnum);
    } else if (!str_prefix(type, "op") || !str_prefix(type, "object")) {
        script = get_oprog_index(vnum);
    } else if (!str_prefix(type, "rp") || !str_prefix(type, "room")) {
        script = get_rprog_index(vnum);
    } else if (!str_prefix(type, "tp") || !str_prefix(type, "token")) {
        script = get_tprog_index(vnum);
    } else if (!str_prefix(type, "ap") || !str_prefix(type, "area")) {
        script = get_aprog_index(vnum);
    } else if (!str_prefix(type, "ip") || !str_prefix(type, "instance")) {
        script = get_iprog_index(vnum);
    } else if (!str_prefix(type, "dp") || !str_prefix(type, "dungeon")) {
        script = get_dprog_index(vnum);
    } else {
        send_to_char("Invalid script type.\n\r", ch);
        return;
    }
    
    if (!script) {
        send_to_char("No such script.\n\r", ch);
        return;
    }
    
    if (!script->runtime_errors || script->runtime_error_count == 0) {
        send_to_char("That script has no runtime errors.\n\r", ch);
        return;
    }
    
    buffer = new_buf();
    
    add_buf(buffer, "{Y╔════════════════════════════════════════════════════════════════════════════╗{x\n\r");
    add_buf(buffer, formatf("{Y║{W Runtime Error Log - %s [%d]                                              {Y║{x\n\r",
        script->name ? script->name : "(unnamed)", script->vnum));
    add_buf(buffer, "{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r");
    add_buf(buffer, formatf("{Y║{C Total Errors: {W%-10d {C│{C Last Error: {W%-35s{Y║{x\n\r",
        script->runtime_error_count,
        ctime(&script->last_runtime_error)));
    add_buf(buffer, "{Y╠════════════════════════════════════════════════════════════════════════════╣{x\n\r");
    
    // Display all errors
    display_count = UMIN(script->runtime_error_count, MAX_RUNTIME_ERRORS);
    
    for (i = 0; i < display_count; i++) {
        index = (script->runtime_error_head - 1 - i + MAX_RUNTIME_ERRORS) % MAX_RUNTIME_ERRORS;
        SCRIPT_RUNTIME_ERROR *err = &script->runtime_errors[index];
        
        if (!err->message) continue;
        
        char time_buf[80];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&err->timestamp));
        
        add_buf(buffer, formatf("{Y║ {C#%-2d {W%s {D│{c Line %3d {D│{W %-28.28s {Y║{x\n\r",
            i + 1, time_buf, err->line,
            err->trigger_type ? err->trigger_type : "manual"));
        
        // Message
        add_buf(buffer, formatf("{Y║{R     %s%-69.69s{Y║{x\n\r", "", err->message));
        
        // Actor
        if (err->actor_name) {
            add_buf(buffer, formatf("{Y║{D     Actor: [%ld] %-58.58s{Y║{x\n\r",
                err->actor_vnum, err->actor_name));
        }
        
        if (i < display_count - 1) {
            add_buf(buffer, "{Y╟────────────────────────────────────────────────────────────────────────────╢{x\n\r");
        }
    }
    
    add_buf(buffer, "{Y╚════════════════════════════════════════════════════════════════════════════╝{x\n\r");
    
    if (script->runtime_error_count > MAX_RUNTIME_ERRORS) {
        add_buf(buffer, formatf("\n\r{D(Showing last %d of %d total errors){x\n\r",
            MAX_RUNTIME_ERRORS, script->runtime_error_count));
    }
    
    add_buf(buffer, "\n\r{CUse '{Wclearerrors{C' in the script editor to clear this log.{x\n\r");
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}
```

**Add to Command Table** (`interp.c`):
```c
{ "scripterrors", do_scripterrors, POS_DEAD, ML, LOG_NORMAL, true, false },
```

### 4.5 Runtime Error Statistics

**Command: Script Error Summary**
```c
void do_scriptstats(CHAR_DATA *ch, char *argument) {
    AREA_DATA *area;
    SCRIPT_DATA *script;
    BUFFER *buffer;
    int total_scripts = 0;
    int scripts_with_errors = 0;
    int total_runtime_errors = 0;
    int i;
    
    if (IS_NPC(ch)) return;
    
    buffer = new_buf();
    
    add_buf(buffer, "{Y╔════════════════════════════════════════════════════════════════════════════╗{x\n\r");
    add_buf(buffer, "{Y║{W                        Script Error Statistics                            {Y║{x\n\r");
    add_buf(buffer, "{Y╠════════╤═══════════╤═══════════╤════════════════════════════════════════╣{x\n\r");
    add_buf(buffer, "{Y║{C Vnum   {W│{C Type      {W│{C Errors    {W│{C Last Error                        {Y║{x\n\r");
    add_buf(buffer, "{Y╠════════╪═══════════╪═══════════╪════════════════════════════════════════╣{x\n\r");
    
    // Iterate through all areas and all script types
    for (area = area_first; area; area = area->next) {
        // Check all script types
        for (i = 0; i < PRG_MAX; i++) {
            // Get first script of this type in area
            // (implementation depends on how scripts are stored)
            // For each script with runtime errors:
            if (script->runtime_error_count > 0) {
                char time_buf[40];
                strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", 
                    localtime(&script->last_runtime_error));
                
                add_buf(buffer, formatf("{Y║{W %6d {W│{c %-9s {W│{R %9d {W│{D %-38s {Y║{x\n\r",
                    script->vnum,
                    script_type_name(script->type),
                    script->runtime_error_count,
                    time_buf));
                
                scripts_with_errors++;
                total_runtime_errors += script->runtime_error_count;
            }
            total_scripts++;
        }
    }
    
    add_buf(buffer, "{Y╠════════╧═══════════╧═══════════╧════════════════════════════════════════╣{x\n\r");
    add_buf(buffer, formatf("{Y║{C Total Scripts: {W%-10d {C│{C Scripts with Errors: {W%-10d               {Y║{x\n\r",
        total_scripts, scripts_with_errors));
    add_buf(buffer, formatf("{Y║{C Total Runtime Errors: {W%-10d                                          {Y║{x\n\r",
        total_runtime_errors));
    add_buf(buffer, "{Y╚════════════════════════════════════════════════════════════════════════════╝{x\n\r");
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
}
```

### 4.6 Integration with Player Scripts

**Security Consideration**: Player-created scripts should have **limited error visibility**

**Player Error Display** (only show their own script errors):
```c
// In homeprogram editor, only show last error (not full log)
if (script->runtime_error_count > 0) {
    SCRIPT_RUNTIME_ERROR *err = &script->runtime_errors[
        (script->runtime_error_head - 1 + MAX_RUNTIME_ERRORS) % MAX_RUNTIME_ERRORS
    ];
    
    send_to_char("\n\r{RLast Runtime Error:{x\n\r", ch);
    send_to_char(formatf("  Line %d: %s\n\r", err->line, err->message), ch);
    send_to_char("  {D(Contact staff if you need help troubleshooting){x\n\r", ch);
}
```

**Staff Error Display** (full access):
```c
// Staff can use scripterrors command to see full log
// Staff can see errors in all scripts via scriptstats
```

---

## Part 5: Implementation Roadmap

### Phase 1: Modernize Script Editors (1 week)

**Week 1**:
- Day 1-2: Migrate 7 script editors to use `process_olc_command()` (remove ~140 lines duplicate code)
- Day 3-4: Implement enhanced `scriptedit_show()` with visual structure, colors, line numbers
- Day 5: Add persistent error logging to SCRIPT_DATA (compilation + runtime)
- Day 6-7: Implement 7 standalone show commands (mpshow, opshow, rpshow, tpshow, apshow, ipshow, dpshow)

**Deliverables**:
- ✅ All script editors use common framework
- ✅ Beautiful, structured show output with error display
- ✅ **Compilation errors** persist in script data for troubleshooting
- ✅ **Runtime errors** captured and displayed (last 10)
- ✅ Standalone show commands for quick viewing

---

### Phase 2: Runtime Error Tracking (3-4 days)

**Days 1-2**:
- Add SCRIPT_RUNTIME_ERROR structure and circular buffer
- Modify `scriptcmd_bug()` to capture runtime errors
- Add runtime error display to `scriptedit_show()`

**Days 3-4**:
- Implement `scriptedit_clearerrors()` command
- Implement `do_scripterrors()` standalone command
- Implement `do_scriptstats()` error statistics

**Deliverables**:
- ✅ Runtime errors stored in circular buffer (last 10)
- ✅ Errors displayed in editor with timestamp, line, trigger, actor
- ✅ Clearerrors command to reset error log
- ✅ Scripterrors command for detailed error viewing
- ✅ Scriptstats for system-wide error overview

---

### Phase 3: Script Template System (1-2 weeks)

**Week 1**:
- Day 1-2: Design SCRIPT_TEMPLATE structure
- Day 3-4: Create 10-15 safe templates (ambiance, decoration, interaction)
- Day 5: Implement `do_templates()` and `do_template()` commands
- Day 6-7: Implement `do_scriptfrom()` with parameter substitution

**Week 2**:
- Day 1-2: Add validation system for template-created scripts
- Day 3-4: Implement `do_homeprogram()` for player script editing
- Day 5-6: Add command whitelist enforcement
- Day 7: Testing and security audit

**Deliverables**:
- ✅ Template library with 10-15 safe scripts
- ✅ Template viewing and parameter system
- ✅ Script generation from templates
- ✅ Player script editor with security constraints
- ✅ Audit logging for player script creation

---

### Phase 4: Builder Script Templates & Prototype Inheritance (2-3 weeks)

**Builder Script Templates** (Week 1):
- Expand template system for builders (not just players)
- More complex templates with advanced commands
- Template categories: combat, quest, puzzle, ambiance, interaction
- Template inheritance (template extends another template)
- Onboarding template library for new builders

**Prototype Inheritance System** (Weeks 2-3):
- Parent/child prototype relationships (guards inherit from base guard)
- Override system (child can override parent fields)
- Multi-level inheritance chains (elite_guard -> city_guard -> guard)
- Cycle detection (prevent circular inheritance)
- Editor display of inherited vs overridden values
- Cascade updates when parent changes

---

### Phase 5: Advanced Features (Optional, 1 week)

- **Script usage search**: Find which entities use a specific script (auditing)
- **Template marketplace**: Players share custom templates (with approval)
- **Script debugger**: Step-through execution with variable inspection
- **Syntax highlighting**: Color-coded script display in editor
- **Auto-complete**: Suggest valid commands/variables
- **Script library**: Browse community-created scripts

---

## Part 5: Summary & Next Steps

### 7.1 Benefits

**Script Editor Modernization**:
- ✅ **140 lines removed** - Eliminate duplicate dispatch code
- ✅ **Consistent UX** - All editors use same framework
- ✅ **Error visibility** - Compilation errors displayed in editor
- ✅ **Runtime debugging** - Last 10 runtime errors with timestamps
- ✅ **Standalone viewing** - Show commands for quick inspection
- ✅ **Troubleshooting** - Persistent error logs with line numbers

**Builder Script Templates**:
- ✅ **Onboarding** - New builders start with working examples
- ✅ **Combat templates** - Guards, patrols, assist behavior
- ✅ **Quest templates** - Item exchange, checkpoints, triggers
- ✅ **Puzzle templates** - Combination locks, sequences, switches
- ✅ **Template inheritance** - Templates extend other templates
- ✅ **Complexity management** - Advanced scripts without starting from scratch

**Prototype Inheritance**:
- ✅ **Reduce duplication** - Define common properties once
- ✅ **Easy updates** - Change parent, all children update
- ✅ **Maintainability** - Centralized definitions
- ✅ **Consistency** - Similar entities behave consistently
- ✅ **Hierarchies** - Multi-level inheritance (base → specialized → elite)
- ✅ **Override system** - Children customize only what's different
- ✅ **Cycle detection** - Prevents circular inheritance
- ✅ **Visual indicators** - Editor shows inherited vs overridden fields

**Player Scripting**:
- ✅ **Safe templates** - Pre-approved scripts for players
- ✅ **Home decoration** - Players add ambiance to personal spaces
- ✅ **Clan customization** - Clan leaders personalize halls
- ✅ **No security risks** - Command whitelist prevents abuse
- ✅ **Easy to use** - Template system abstracts complexity
- ✅ **Limited error display** - Players see last error only

### 7.2 Recommended Order

1. **Modernize script editors** (1 week) - Foundation work, immediate benefit
2. **Add runtime error tracking** (3-4 days) - Critical debugging feature
3. **Add show commands** (2 days) - Quick win, high utility
4. **Builder script templates** (1 week) - Onboarding & productivity
5. **Prototype inheritance** (2-3 weeks) - Major feature, high value
6. **Player templates** (1 week) - Enables player scripting
7. **Player editor** (3 days) - Enabler for templates

**Total Estimated Effort**: 6-8 weeks for complete implementation with all features.

### 7.3 Security Considerations

**Builder Templates**:
- ✅ Full command access (builders are trusted)
- ✅ Template inheritance limited to same security level
- ✅ Audit logging of template usage
- ✅ Templates reviewed by senior staff before inclusion

**Player Templates**:
- ✅ Whitelist commands only (echoaround, echoat, emote, say)
- ✅ No variable modification (except object.v0 for state)
- ✅ No remote execution (at, goto, transfer)
- ✅ No item creation (give, load)
- ✅ No forced commands (force)
- ✅ Template validation on creation
- ✅ Audit logging of all player scripts
- ✅ Staff review queue for new templates

**Prototype Inheritance**:
- ✅ Cycle detection prevents infinite loops
- ✅ Max depth limit (20 levels)
- ✅ Parent changes require area save
- ✅ Override flags prevent accidental inheritance
- ✅ Child can't be parent of its ancestor

**Runtime Protection**:
- ✅ Call depth limits prevent infinite loops
- ✅ Execution time limits prevent DoS
- ✅ Memory limits prevent resource exhaustion
- ✅ Sandbox execution for player scripts
- ✅ Emergency halt command for staff
- ✅ Runtime errors logged but don't crash server

### 7.4 Example Workflows

**Builder Using Combat Template**:
```
> btemplates combat
[Lists combat templates: guard_patrol, assist_allies, flee_wounded, etc.]

> btemplate combat_guard_patrol
[Shows template with parameters]

> scriptfrom combat_guard_patrol START_HOUR=6 END_HOUR=22 PATROL_CHANCE=10
[Generates guard patrol script]

> mped 3001
[MpEdit][3001]> code
[Pastes generated code, customizes as needed]
> compile
Script compiled successfully.
> done
```

**Builder Creating Guard Hierarchy**:
```
> medit 3000
[MEdit][3000] Creating base_guard
> name guard~
> short a guard~
> level 20
> act sentinel npc
> script 3000 greet *
> done

> medit 3001
[MEdit][3001] Creating city_guard
> parent 3000
Parent set to [3000] guard.
Inheritance depth: 1

> show
╔════════════════════════════════════════════════════════════════════════════╗
║ Mobile: [3001] city guard                                                  ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Parent: [3000] guard                                                       ║
║ Inherit Depth: 1                                                           ║
╠════════════════════════════════════════════════════════════════════════════╣
║ Short Desc: (inherited) a guard                                            ║
║ Long Desc:  (inherited) A guard stands here.                               ║
║ Level:      (inherited) 20                                                 ║
║ Scripts:                                                                   ║
║   (inherited) [3000] greet       *                                         ║
╚════════════════════════════════════════════════════════════════════════════╝

> override long A city guard patrols here, watching for criminals.~
Long description overridden.

> override alignment -1000
Alignment overridden.

> done

> medit 3002
[MEdit][3002] Creating palace_guard (inherits from city_guard)
> parent 3001
Parent set to [3001] city guard.
Inheritance depth: 2

> chain
╔════════════════════════════════════════════════════════════════════════════╗
║                        Inheritance Chain                                   ║
╠════════╤═══════════════════════════════════════════════════════════════════╣
║ > [3002] │ palace guard                                                    ║
║        │   ↑ inherits from                                                 ║
║   [3001] │   city guard                                                    ║
║        │     ↑ inherits from                                               ║
║   [3000] │     base guard                                                  ║
╚════════╧═══════════════════════════════════════════════════════════════════╝

> override level 25
Level overridden.

> done
```

**System-Wide Update**:
```
> medit 3000
[MEdit][3000]> level 22
Level set to 22.
> done

# Now city_guard (3001) automatically becomes level 22
# But palace_guard (3002) stays at 25 (overridden)
```

---

## Part 8: Appendices

> scriptfrom ambiance_echo CHANCE=5 MESSAGE='A gentle breeze rustles the curtains.'
[Generates script code]

> rped 11001
[RpEdit][11001]> code
[Pastes generated code]

> compile
Script compiled successfully.

> show
[Displays beautiful formatted output with code]

> done
```

**Player Using Template**:
```
> homeprogram
Room programs attached to this room:
  [11001] ambiance_echo - A gentle breeze rustles the curtains.

Syntax: homeprogram <vnum>    - edit program
        homeprogram create    - create from template

> templates
[Lists player-accessible templates]

> template interaction_greet
[Shows greeting template]

> scriptfrom interaction_greet MESSAGE='Welcome home, $n!'
[Generates script]

> homeprogram 11002
[Limited editor mode]
> code
[Pastes code]
> compile
Script compiled successfully.
> done

Your home now has a greeting script!
```

---

## Appendices

### Appendix A: Script Data Structure Changes

**Add to `merc.h` or `scripts.h`**:
```c
struct script_runtime_error {
    time_t  timestamp;          // When error occurred
    int     line;               // Line number where error occurred
    char    *message;           // Error message
    char    *trigger_type;      // What triggered script (e.g., "entry", "speech")
    long    actor_vnum;         // Who/what triggered (mob/obj/char vnum)
    char    *actor_name;        // Name of triggering entity
};

#define MAX_RUNTIME_ERRORS 10   // Keep last 10 errors

struct script_data {
    // Existing fields
    int             vnum;
    char            *name;
    char            *source;        // Source code
    int             lines;          // Compiled line count
    SCRIPT_CODE     *code;          // Compiled bytecode
    int             call_depth;
    int             security;
    long            flags;
    char            *comments;
    int             type;           // PRG_MPROG, PRG_OPROG, etc.
    AREA_DATA       *area;
    
    // NEW: Compilation error tracking and metadata
    char            *last_errors;   // Compilation error messages
    int             error_count;    // Number of compilation errors
    time_t          last_compiled;  // Timestamp of last compile
    char            *compiled_by;   // Who compiled it
    
    // NEW: Runtime error tracking
    SCRIPT_RUNTIME_ERROR *runtime_errors;  // Circular buffer of last 10 errors
    int             runtime_error_head;    // Head index in circular buffer
    int             runtime_error_count;   // Total runtime errors (all time)
    time_t          last_runtime_error;    // Timestamp of most recent error
    
    // NEW: Template tracking
    char            *template_name; // Source template (if from template)
    char            **template_params; // Parameter values used
};

// Script flags (add to existing flags)
#define SCRIPT_FROM_TEMPLATE    (A)  // Created from safe template
```

### Appendix B: Template Table

**Global Template Table** (`script_templates.c`):
```c
const SCRIPT_TEMPLATE script_template_table[] = {
    // Ambiance templates
    {
        .name = "ambiance_echo",
        .category = "Ambiance",
        .description = "Random atmospheric message",
        .source = "if rand(100) < ${CHANCE}\n  room echoaround * ${MESSAGE}\nendif",
        .security_level = 0,
        .allowed_commands = (char*[]){"echoaround", NULL},
        .parameters = (char*[]){"CHANCE", "MESSAGE", NULL},
        .example = "CHANCE=5 MESSAGE='Wind howls outside.'"
    },
    // ... 10-15 more templates
    { NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL }  // Terminator
};
```

### Appendix C: Script Usage Search (Auditing)
{ "scriptstats",  do_scriptstats,  POS_DEAD, ML, LOG_NORMAL, true, false },

// Template commands
{ "templates",    do_templates,    POS_DEAD, 0,  LOG_NORMAL, true, false },
{ "template",     do_template,     POS_DEAD, 0,  LOG_NORMAL, true, false },
{ "scriptfrom",   do_scriptfrom,   POS_DEAD, 0,  LOG_NORMAL, true, false },

// Player script editing
{ "homeprogram",  do_homeprogram,  POS_DEAD, 0,  LOG_NORMAL, true, false },
```

**Add to Scriptbenefit from both **compilation error tracking** and **runtime error reporting**:

**Compilation Errors** (syntax/structure):
- Caught during `compile` command
- Displayed immediately in editor
- Stored in `last_errors` field
- Prevent script from running

**Runtime Errors** (execution failures):
- Caught during script execution via `scriptcmd_bug()`
- Stored in circular buffer (last 10)
- Displayed in editor with context (timestamp, trigger, actor)
- Help troubleshoot logic errors

Together, these provide complete visibility into script health:
1. **Compile time**: Catch syntax errors before deployment
2. **Run time**: Debug logic errors in production

**Implementation Priority**:
1. ✅ **High**: Modernize script editors (1 week) - Foundation
2. ✅ **High**: Runtime error tracking (3-4 days) - Critical debugging  
3. ✅ **Medium**: Show commands (2 days) - Utility
4. ✅ **Medium**: Template system (1 week) - Player features
5. ✅ **Low**: Player editor (3 days) - Enabler

**Total Effort**: 3-4 weeks for complete implementation with runtime error tracking.

Runtime error reporting is **essential** for production scripts - builders need to see not just compilation issues but also execution failures. The circular buffer approach (last 10 errors) provides enough context for debugging without consuming excessive memory
{ "mpshow",       do_mpshow,       POS_DEAD, ML, LOG_NORMAL, true, false },
{ "opshow",       do_opshow,       POS_DEAD, ML, LOG_NORMAL, true, false },
{ "rpshow",       do_rpshow,       POS_DEAD, ML, LOG_NORMAL, true, false },
{ "tpshow",       do_tpshow,       POS_DEAD, ML, LOG_NORMAL, true, false },
{ "apshow",       do_apshow,       POS_DEAD, ML, LOG_NORMAL, true, false },
{ "ipshow",       do_ipshow,       POS_DEAD, ML, LOG_NORMAL, true, false },
{ "dpshow",       do_dpshow,       POS_DEAD, ML, LOG_NORMAL, true, false },

// Template commands
{ "templates",    do_templates,    POS_DEAD, 0,  LOG_NORMAL, true, false },
{ "template",     do_template,     POS_DEAD, 0,  LOG_NORMAL, true, false },
{ "scriptfrom",   do_scriptfrom,   POS_DEAD, 0,  LOG_NORMAL, true, false },

// Player script editing
{ "homeprogram",  do_homeprogram,  POS_DEAD, 0,  LOG_NORMAL, true, false },
```

---

## Conclusion

Script editors are the final piece of the editor framework unification. With ~140 lines of duplicate code removed, beautiful structured output, persistent error logging, and standalone show commands, they'll match the quality of other modernized editors.

The template system opens up creative opportunities for players while maintaining security through command whitelisting and validation. Players can personalize their homes with ambiance, decorations, and simple interactions without risking system integrity.

**Implementation Priority**:
1. ✅ **High**: Modernize script editors (1 week) - Quality of life improvement
2. ✅ **Medium**: Add show commands (2 days) - Utility enhancement  
3. ✅ **Medium**: Template system (1 week) - Player engagement feature
4. ✅ **Low**: Player editor (3 days) - Enabler for templates

**Total Effort**: 2-3 weeks for complete implementation.
