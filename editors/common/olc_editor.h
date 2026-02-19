/**
 * @file olc_editor.h
 * @brief Unified OLC Editor Framework
 *
 * Provides a common framework for all OLC editors in the game, eliminating
 * duplicated boilerplate and providing consistent behavior for:
 *   - Editor lifecycle (enter, interpret, exit)
 *   - Tab-based UI with per-editor tab definitions
 *   - Per-editor color themes
 *   - Permissions (staff rank, area security, custom callbacks)
 *   - Audit logging of all changes
 *   - Common display renderers for fields, flags, tables, lists
 *   - Change tracking (dirty flags, explicit save)
 *
 * Migration path: Editors can adopt this framework incrementally.
 * Each editor defines an OLC_EDITOR_DEF and migrates its interpreter
 * function to a one-line call to olc_editor_interp().
 *
 * @see editors/common/olc_editor.c for implementation
 * @see editors/common/olc_display.h for display/rendering helpers
 */

#ifndef __OLC_EDITOR_H__
#define __OLC_EDITOR_H__

#include "../../merc.h"
#include "../../olc.h"

/* =========================================================================
 * Constants
 * ========================================================================= */

/** Maximum tabs per editor */
#define OLC_MAX_EDITOR_TABS     10

/** Maximum audit log message length */
#define OLC_AUDIT_MSG_LEN       256

/** Maximum editor name length */
#define OLC_EDITOR_NAME_LEN     16

/** Maximum history entries per entity (oldest evicted when full) */
#define OLC_MAX_HISTORY_ENTRIES 50

/** Maximum field name length in change history */
#define OLC_HISTORY_FIELD_LEN   64

/** Maximum old/new value display length in change history */
#define OLC_HISTORY_VALUE_LEN   256

/* =========================================================================
 * Forward declarations
 * ========================================================================= */

typedef struct olc_editor_def       OLC_EDITOR_DEF;
typedef struct olc_editor_theme     OLC_EDITOR_THEME;
typedef struct olc_editor_perm      OLC_EDITOR_PERM;
typedef struct olc_editor_tab       OLC_EDITOR_TAB;
typedef struct olc_change_entry     OLC_CHANGE_ENTRY;
typedef struct olc_change_history   OLC_CHANGE_HISTORY;

/* =========================================================================
 * Permission System
 * ========================================================================= */

/**
 * Permission modes for editor access control.
 * These can be combined (bitwise OR) for editors that need multiple checks.
 */
#define OLC_PERM_NONE           0x00  /**< No special permissions (immortal only) */
#define OLC_PERM_STAFF_RANK     0x01  /**< Check staff rank >= min_rank */
#define OLC_PERM_AREA_SECURITY  0x02  /**< Check IS_BUILDER(ch, area) */
#define OLC_PERM_SECURITY_LEVEL 0x04  /**< Check ch->pcdata->security >= min_security */
#define OLC_PERM_CUSTOM         0x08  /**< Use custom permission callback */
#define OLC_PERM_PLAYER         0x10  /**< Available to non-immortal players (housing etc.) */

/**
 * Permission definition for an editor.
 * Multiple checks can be combined; all enabled checks must pass.
 */
struct olc_editor_perm {
    int         flags;              /**< OLC_PERM_* flags */
    int         min_staff_rank;     /**< Minimum staff rank (STAFF_*) if PERM_STAFF_RANK */
    int         min_security;       /**< Minimum security level if PERM_SECURITY_LEVEL */
    /**
     * Custom permission check callback.
     * @param ch    Character attempting access
     * @param pEdit The entity being edited (may be NULL for list/create)
     * @return true if access is allowed
     */
    bool        (*check_fn)(CHAR_DATA *ch, void *pEdit);
};

/* =========================================================================
 * Tab System
 * ========================================================================= */

/**
 * Definition of a single tab in an editor.
 */
struct olc_editor_tab {
    const char  *name;          /**< Full tab name (e.g., "General", "Combat") */
    const char  *short_name;    /**< Abbreviated name for narrow screens (e.g., "Gen") */
    /**
     * Tab-specific show function. Called when this tab is active.
     * @param ch        Character viewing the editor
     * @param ctx       Layout context (screen width, buffer, etc.)
     * @param pEdit     The entity being edited
     */
    void        (*show_fn)(CHAR_DATA *ch, struct olc_layout_ctx *ctx, void *pEdit);
};

/* =========================================================================
 * Color Theme System
 * ========================================================================= */

/**
 * Color theme for an editor. Each editor can have its own color scheme
 * so users can visually distinguish which editor they're in.
 *
 * Color codes use the game's color system (e.g., "{Y" for yellow).
 */
struct olc_editor_theme {
    const char  *label;         /**< Label/heading color (default: "{Y") */
    const char  *value;         /**< Value color (default: "{C") */
    const char  *unset;         /**< Unset/empty value color (default: "{D") */
    const char  *section;       /**< Section divider color (default: "{G") */
    const char  *section_text;  /**< Section title text color (default: "{W") */
    const char  *border;        /**< Table border color (default: "{G") */
    const char  *clickable;     /**< MXP clickable text color (default: "{W") */
    const char  *tab_active;    /**< Active tab color (default: "{Y") */
    const char  *tab_inactive;  /**< Inactive tab color (default: "{g") */
    const char  *header;        /**< Top-line header color (default: "{W") */
    /**
     * Flag display color string (7 chars): label, bracket, stat-set,
     * stat-unset, flag-set, flag-unset, readonly-set.
     * Used by olc_buffer_show_flags_ex().
     */
    const char  *flag_colors;
};

/** Default theme - used when no per-editor theme is specified */
extern const OLC_EDITOR_THEME olc_theme_default;

/** Pre-defined themes for different editor categories */
extern const OLC_EDITOR_THEME olc_theme_world;      /**< Areas, rooms, wilderness */
extern const OLC_EDITOR_THEME olc_theme_entity;     /**< Mobs, objects, tokens */
extern const OLC_EDITOR_THEME olc_theme_data;       /**< Skills, races, classes */
extern const OLC_EDITOR_THEME olc_theme_scripting;  /**< All script editors */
extern const OLC_EDITOR_THEME olc_theme_system;     /**< Commands, settings, socials */
extern const OLC_EDITOR_THEME olc_theme_building;   /**< Blueprints, dungeons */

/* =========================================================================
 * Change Tracking
 * ========================================================================= */

/**
 * Change tracking modes for editors.
 */
typedef enum {
    /** Area-based dirty flag (SET_BIT on area_flags). Legacy editors use this. */
    OLC_CHANGE_AREA_FLAG,

    /** Explicit save command required (skill/race/class/trait editors). */
    OLC_CHANGE_EXPLICIT_SAVE,

    /** Custom callback handles change marking. */
    OLC_CHANGE_CUSTOM,

    /** No change tracking (read-only or handled elsewhere). */
    OLC_CHANGE_NONE
} olc_change_mode_t;

/**
 * Callback to mark an entity as changed.
 * @param ch    Character who made the change
 * @param pEdit Entity that was changed
 */
typedef void (*olc_mark_changed_fn)(CHAR_DATA *ch, void *pEdit);

/**
 * Callback to get the area associated with an edited entity.
 * Required for OLC_CHANGE_AREA_FLAG mode.
 * @param pEdit Entity being edited
 * @return AREA_DATA pointer, or NULL
 */
typedef AREA_DATA *(*olc_get_area_fn)(void *pEdit);

/* =========================================================================
 * Audit Logging
 * ========================================================================= */

/**
 * Audit log entry for an OLC change.
 * Logged via the game's logging infrastructure.
 */
typedef struct olc_audit_entry {
    const char      *editor_name;   /**< Editor name (e.g., "MEdit") */
    const char      *char_name;     /**< Character who made the change */
    const char      *field_name;    /**< Field that was changed (e.g., "name", "level") */
    const char      *old_value;     /**< Previous value (may be NULL) */
    const char      *new_value;     /**< New value */
    long            entity_id;      /**< Entity identifier (vnum, uid, etc.) */
    time_t          timestamp;      /**< When the change was made */
} OLC_AUDIT_ENTRY;

/* =========================================================================
 * Change History (Per-Entity Audit Trail)
 *
 * Lightweight change tracking modeled on gameedit's history/view commands.
 * Each entity (mob, obj, room, etc.) can have an OLC_CHANGE_HISTORY that
 * records who changed what and when. No rollback — view-only.
 * ========================================================================= */

/**
 * A single field change within an OLC entity's history.
 *
 * Example: If an immortal changes a mob's name from "a troll" to
 * "a cave troll", one OLC_CHANGE_ENTRY is created with:
 *   author = "Nibelung", field = "name",
 *   old_value = "a troll", new_value = "a cave troll"
 */
struct olc_change_entry {
    char            *author;        /**< Character name who made the change */
    time_t          timestamp;      /**< When the change was made */
    int             id;             /**< Sequential ID within this entity's history */
    char            *field;         /**< Field name (e.g., "name", "level", "act_flags") */
    char            *old_value;     /**< Previous value as display string */
    char            *new_value;     /**< New value as display string */
};

/**
 * Change history for a single OLC entity.
 * Stored as a bounded list — oldest entries are evicted when full.
 *
 * Entities that want change tracking add a pointer to this struct:
 *   OLC_CHANGE_HISTORY *change_history;
 *
 * The framework provides `history` and `view` commands automatically
 * for any editor whose OLC_EDITOR_DEF has a get_history_fn set.
 */
struct olc_change_history {
    LLIST           *entries;       /**< List of OLC_CHANGE_ENTRY* (newest first) */
    int             next_id;        /**< Next sequential ID to assign */
    int             max_entries;    /**< Maximum entries to keep (default OLC_MAX_HISTORY_ENTRIES) */
};

/**
 * Callback to get an entity's change history.
 * Used by olc_editor_interp() to support the built-in `history` and `view` commands.
 *
 * @param pEdit Entity being edited
 * @return Entity's change history, or NULL if unavailable
 */
typedef OLC_CHANGE_HISTORY *(*olc_get_history_fn)(void *pEdit);

/* =========================================================================
 * Editor Definition
 * ========================================================================= */

/**
 * Complete definition of an OLC editor.
 *
 * Define one of these as a static const for each editor, then pass it
 * to olc_editor_interp() from your interpreter function.
 *
 * Example usage (in clsedit.c):
 * @code
 * static const OLC_EDITOR_DEF clsedit_def = {
 *     .name           = "ClsEdit",
 *     .editor_type    = ED_CLASS,
 *     .cmd_table      = clsedit_table,
 *     .show_fn        = clsedit_show,
 *     .tabs           = { ... },       // Optional
 *     .theme          = &olc_theme_data,
 *     .perm           = { .flags = OLC_PERM_STAFF_RANK, .min_staff_rank = STAFF_CREATOR },
 *     .change_mode    = OLC_CHANGE_EXPLICIT_SAVE,
 * };
 *
 * void clsedit(CHAR_DATA *ch, char *argument) {
 *     olc_editor_interp(ch, argument, &clsedit_def);
 * }
 * @endcode
 */
struct olc_editor_def {
    /* --- Identity --- */
    const char              *name;              /**< Display name (e.g., "ClsEdit") */
    int                     editor_type;        /**< ED_* constant from olc.h */

    /* --- Command handling --- */
    const struct olc_cmd_type *cmd_table;       /**< Command table (NULL-terminated) */
    OLC_FUN                 *show_fn;           /**< Show/display function */

    /* --- Tab configuration (optional) --- */
    struct {
        int                 count;              /**< Number of tabs (0 = no tabs) */
        OLC_EDITOR_TAB      tabs[OLC_MAX_EDITOR_TABS];  /**< Tab definitions */
    } tabs;

    /* --- Visual theme --- */
    const OLC_EDITOR_THEME  *theme;             /**< Color theme (NULL = default) */

    /* --- Permissions --- */
    OLC_EDITOR_PERM         perm;               /**< Permission requirements */

    /* --- Change tracking --- */
    olc_change_mode_t       change_mode;        /**< How changes are tracked */
    olc_mark_changed_fn     mark_changed_fn;    /**< Custom change callback (CHANGE_CUSTOM) */
    olc_get_area_fn         get_area_fn;        /**< Get area for entity (CHANGE_AREA_FLAG) */

    /* --- Audit --- */
    bool                    audit_changes;      /**< Whether to log changes to server audit log */

    /* --- Lifecycle override (optional) --- */
    /**
     * Optional custom handler for the built-in `done` command.
     * If NULL, framework default behavior is `edit_done(ch)`.
     */
    void                    (*done_fn)(CHAR_DATA *ch);

    /* --- Change history (in-game audit trail) --- */
    /**
     * Callback to retrieve an entity's change history.
     * If set, the framework provides built-in `history` and `view` commands.
     * If NULL, history/view are not available for this editor.
     */
    olc_get_history_fn      get_history_fn;
};

/* =========================================================================
 * Editor Lifecycle Functions
 * ========================================================================= */

/**
 * Unified editor interpreter. Replaces all per-editor interpreter boilerplate.
 *
 * Handles:
 *   - Permission checking
 *   - "done" command
 *   - Tab switching (if tabs defined)
 *   - Empty input → show
 *   - Command table lookup
 *   - Change marking
 *   - Audit logging
 *   - Fallback to game interpreter
 *
 * @param ch        Character in the editor
 * @param argument  Raw input from the character
 * @param def       Editor definition (static const, per-editor)
 */
void olc_editor_interp(CHAR_DATA *ch, char *argument, const OLC_EDITOR_DEF *def);

/**
 * Enter an editor for a given entity.
 * Sets up the descriptor state and displays the initial view.
 *
 * @param ch            Character entering the editor
 * @param def           Editor definition
 * @param pEdit         Entity to edit
 * @param show_initial  Whether to display the editor immediately
 */
void olc_editor_enter(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
                      void *pEdit, bool show_initial);

/**
 * Check if a character has permission to use an editor.
 *
 * @param ch    Character to check
 * @param def   Editor definition containing permission requirements
 * @param pEdit Entity being edited (may be NULL for list/create operations)
 * @return true if character has access
 */
bool olc_editor_check_perm(CHAR_DATA *ch, const OLC_EDITOR_DEF *def, void *pEdit);

/**
 * Get the active theme for a character's current editor.
 * Returns the editor-specific theme if set, otherwise the default.
 *
 * @param def   Editor definition (may be NULL)
 * @return Theme to use for rendering
 */
const OLC_EDITOR_THEME *olc_get_theme(const OLC_EDITOR_DEF *def);

/**
 * Determine whether tabbed OLC view is enabled for this character.
 * Uses the staff/player preference key "olctabs" (default ON).
 */
bool olc_tabs_enabled(CHAR_DATA *ch);

/**
 * Determine whether current OLC output should render all tabs on one page.
 * True if tabbed view is disabled by preference or temporarily forced
 * (e.g. from *show commands).
 */
bool olc_show_all_tabs_mode(CHAR_DATA *ch);

/* =========================================================================
 * Audit Functions
 * ========================================================================= */

/**
 * Log an OLC change to the audit system.
 *
 * @param ch        Character who made the change
 * @param def       Editor definition
 * @param field     Field name that was changed
 * @param old_val   Previous value (can be NULL)
 * @param new_val   New value (can be NULL)
 * @param entity_id Entity identifier (vnum, uid, etc.)
 */
void olc_audit_log(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
                   const char *field, const char *old_val,
                   const char *new_val, long entity_id);

/**
 * Log a simple OLC change with just a description.
 *
 * @param ch        Character who made the change
 * @param def       Editor definition
 * @param message   Description of the change
 * @param entity_id Entity identifier
 */
void olc_audit_logf(CHAR_DATA *ch, const OLC_EDITOR_DEF *def,
                    long entity_id, const char *fmt, ...);

/* =========================================================================
 * Change Marking Helpers
 * ========================================================================= */

/**
 * Mark an entity as changed based on the editor definition's change mode.
 * Called automatically by olc_editor_interp() when a command returns true.
 *
 * @param ch    Character who made the change
 * @param def   Editor definition
 */
void olc_mark_changed(CHAR_DATA *ch, const OLC_EDITOR_DEF *def);

/* =========================================================================
 * Editor Registration (for introspection/debugging)
 * ========================================================================= */

/**
 * Register an editor definition so it can be found by type or name.
 * Called during initialization.
 *
 * @param def   Editor definition to register
 */
void olc_register_editor(const OLC_EDITOR_DEF *def);

/**
 * Find an editor definition by ED_* type.
 *
 * @param editor_type   ED_* constant
 * @return Editor definition, or NULL if not registered
 */
const OLC_EDITOR_DEF *olc_find_editor_by_type(int editor_type);

/**
 * Find an editor definition by name.
 *
 * @param name  Editor name (case-insensitive prefix match)
 * @return Editor definition, or NULL if not found
 */
const OLC_EDITOR_DEF *olc_find_editor_by_name(const char *name);

/* =========================================================================
 * Change History Functions
 * ========================================================================= */

/**
 * Create a new change history with default capacity.
 * @return Newly allocated OLC_CHANGE_HISTORY, or NULL on failure
 */
OLC_CHANGE_HISTORY *olc_history_new(void);

/**
 * Free a change history and all its entries.
 * @param history History to free (safe to pass NULL)
 */
void olc_history_free(OLC_CHANGE_HISTORY *history);

/**
 * Record a field change in an entity's history.
 * Automatically evicts the oldest entry if at capacity.
 *
 * @param history   Entity's change history (created if NULL via entity's alloc)
 * @param ch        Character who made the change
 * @param field     Field name that was changed (e.g., "name", "level")
 * @param old_val   Previous value formatted as a display string
 * @param new_val   New value formatted as a display string
 */
void olc_history_record(OLC_CHANGE_HISTORY *history, CHAR_DATA *ch,
                        const char *field, const char *old_val,
                        const char *new_val);

/**
 * Display the history table for an entity (the `history` command).
 * Shows a formatted table of recent changes with ID, author, field, and date.
 *
 * @param ch        Character viewing the history
 * @param history   Entity's change history (may be NULL)
 * @param argument  Optional filter/limit argument (number or field name)
 * @param theme     Color theme for display (uses default if NULL)
 */
void olc_history_show(CHAR_DATA *ch, const OLC_CHANGE_HISTORY *history,
                      const char *argument, const OLC_EDITOR_THEME *theme);

/**
 * Display details of a single change entry (the `view` command).
 * Shows author, date, field, old value, and new value.
 *
 * @param ch        Character viewing the entry
 * @param history   Entity's change history
 * @param id        Change entry ID to view
 * @param theme     Color theme for display (uses default if NULL)
 */
void olc_history_view(CHAR_DATA *ch, const OLC_CHANGE_HISTORY *history,
                      int id, const OLC_EDITOR_THEME *theme);

/* =========================================================================
 * Per-Command / Per-Argument Permission Checks
 * ========================================================================= */

/**
 * Check whether a character has permission for a specific OLC command.
 * Used automatically by olc_editor_interp() for commands that set
 * min_staff_rank, but can also be called manually from within a command
 * handler for argument-level permission gating.
 *
 * Checks only staff rank.  For more complex per-argument checks, use
 * olc_check_perm() which supports the full OLC_EDITOR_PERM model.
 *
 * @param ch              Character to check
 * @param min_staff_rank  Required STAFF_* rank (0 = always passes)
 * @param cmd_name        Command or argument name (for denial message)
 * @return true if the character has permission, false if denied
 */
bool olc_check_command_perm(CHAR_DATA *ch, int min_staff_rank,
                            const char *cmd_name);

/**
 * General-purpose OLC permission check against an OLC_EDITOR_PERM.
 * Suitable for fine-grained argument-level checks: build a temporary
 * OLC_EDITOR_PERM on the stack and call this to validate.
 *
 * When `silent` is false, a denial message is sent to the character.
 *
 * @param ch      Character to check
 * @param perm    Permission definition to test against
 * @param pEdit   Context object (passed to custom check_fn, may be NULL)
 * @param label   Human-readable label for denial messages (e.g. "scripts")
 * @param silent  If true, suppress denial message on failure
 * @return true if all enabled permission checks pass
 */
bool olc_check_perm(CHAR_DATA *ch, const OLC_EDITOR_PERM *perm,
                    void *pEdit, const char *label, bool silent);

#endif /* !def __OLC_EDITOR_H__ */
