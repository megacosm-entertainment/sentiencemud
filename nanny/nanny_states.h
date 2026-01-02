#ifndef NANNY_STATES_H
#define NANNY_STATES_H

/*
 * Nanny State Machine Documentation
 *
 * This file documents the connection states used in the nanny (login/character creation) system.
 * The goal is to reduce the 85 states to ~45 by merging redundant states and using context flags.
 *
 * State Categorization:
 * - Core Login: Initial connection and account/character authentication
 * - Character Creation: Race, class, sex, alignment, pronouns, etc.
 * - Account Management: Password changes, email changes, MFA setup
 * - Character Management: Character-specific settings and deletion
 * - Special: MOTD, staff creation, etc.
 */

/*
 * CORE LOGIN STATES (Required)
 */
#define CON_PLAYING                 0   /* In game - playing */
#define CON_GET_NAME                1   /* Initial name entry (legacy character or account) */
#define CON_GET_OLD_PASSWORD        2   /* Legacy character password */
#define CON_GET_ACCOUNT_NAME        25  /* Account name entry */
#define CON_GET_ACCOUNT_PASSWORD    26  /* Account password verification */
#define CON_ACCOUNT_MENU            30  /* Account menu - character selection */
#define CON_CHARACTER_MENU          37  /* Character-specific menu */
#define CON_GET_CHAR_PASSWORD       42  /* Character-level password (override) */
#define CON_GET_CHAR_MFA            43  /* Character-level MFA (override) */

/*
 * CHARACTER CREATION STATES (Required)
 */
#define CON_CONFIRM_NEW_NAME        3   /* Confirm new character name */
#define CON_GET_NEW_PASSWORD        4   /* Set new character password (legacy) */
#define CON_CONFIRM_NEW_PASSWORD    5   /* Confirm new character password (legacy) */
#define CON_GET_NEW_RACE            6   /* Select race */
#define CON_GET_NEW_SEX             7   /* Select biological sex */
#define CON_GET_NEW_CLASS           8   /* Select class */
#define CON_GET_ALIGNMENT           9   /* Select alignment */
#define CON_GET_SUB_CLASS           17  /* Select subclass */
#define CON_GET_ASCII               16  /* View ASCII art / intro */
#define CON_CREATING_NEW_CHAR       31  /* Creating new character (from account menu) */
#define CON_CREATING_NEW_STAFF_CHAR 56  /* Creating new staff character */

/*
 * BODY TYPE AND PRONOUN STATES
 * These 9 states handle custom pronoun entry - can be consolidated
 */
#define CON_GET_NEW_BODY_TYPE               76  /* Select body type */
#define CON_CONFIRM_DEFAULT_PRONOUNS        77  /* Use default pronouns for body type? */
#define CON_SET_CUSTOM_PRONOUN_SUBJ         78  /* he/she/they/etc */
#define CON_SET_CUSTOM_PRONOUN_OBJ          79  /* him/her/them/etc */
#define CON_SET_CUSTOM_PRONOUN_POSS_ADJ     80  /* his/her/their/etc */
#define CON_SET_CUSTOM_PRONOUN_POSS_PRON    81  /* his/hers/theirs/etc */
#define CON_SET_CUSTOM_PRONOUN_REFL         82  /* himself/herself/themself/etc */
#define CON_SET_CUSTOM_VERB_PREF            83  /* is/are */
#define CON_SET_CUSTOM_PRONOUNS_CONFIRM     84  /* Confirm custom pronouns */

/* CONSOLIDATION OPPORTUNITY: Use single CON_SET_CUSTOM_PRONOUNS with d->pronoun_step field */

/*
 * ACCOUNT PASSWORD MANAGEMENT STATES
 * Multiple redundant password states that can be merged
 */
#define CON_NEW_ACCOUNT_PASSWORD            27  /* Setting new account password */
#define CON_CONFIRM_ACCOUNT_PASSWORD        28  /* Confirming new account password */
#define CON_CHANGE_ACCOUNT_PASSWORD         47  /* Changing account password (menu) */
#define CON_CONFIRM_ACCOUNT_PASSWORD_CHANGE 48  /* Confirming password change */
#define CON_VERIFY_ACCOUNT_PASSWORD         51  /* Verify password before operation */
#define CON_CHANGE_PASSWORD                 20  /* Legacy password change */
#define CON_CHANGE_PASSWORD_CONFIRM         21  /* Legacy password change confirm */

/* CONSOLIDATION: Merge to CON_ACCOUNT_PASSWORD_SET, CON_ACCOUNT_PASSWORD_CONFIRM, CON_ACCOUNT_PASSWORD_VERIFY */

/*
 * CHARACTER PASSWORD MANAGEMENT STATES
 */
#define CON_CHARACTER_PASSWORD              38  /* Setting character password */
#define CON_CONFIRM_CHARACTER_PASSWORD      39  /* Confirming character password */
#define CON_SET_UNLINK_PASSWORD             73  /* Setting password for unlink */
#define CON_VERIFY_UNLINK_PASSWORD          71  /* Verify password for unlink */
#define CON_VERIFY_DELETE_PASSWORD          54  /* Verify password for deletion */

/* CONSOLIDATION: Use context flags to distinguish verify vs set operations */

/*
 * MFA (Multi-Factor Authentication) STATES
 * Heavy duplication between account and character MFA
 */
#define CON_GET_MFA                             24  /* Legacy MFA */
#define CON_GET_ACCOUNT_MFA                     49  /* Account MFA verification */
#define CON_GET_ACCOUNT_MFA_FOR_CHAR            53  /* Account MFA for character login */
#define CON_ACCOUNT_MFA_VERIFY                  45  /* Account MFA verify (general) */
#define CON_ACCOUNT_MFA_VERIFY_FOR_SETTINGS     65  /* Account MFA verify for settings */
#define CON_ACCOUNT_MFA_MENU                    52  /* Account MFA setup menu */
#define CON_ACCOUNT_MFA_CONFIRM                 66  /* Confirm account MFA setup */
#define CON_ACCOUNT_MFA_DISABLE_CONFIRM         67  /* Confirm account MFA disable */

#define CON_CHARACTER_MFA_VERIFY                44  /* Character MFA verify */
#define CON_CHARACTER_MFA_VERIFY_FOR_SETTINGS   61  /* Character MFA verify for settings */
#define CON_CHARACTER_MFA_MENU                  62  /* Character MFA setup menu */
#define CON_CHARACTER_MFA_CONFIRM               63  /* Confirm character MFA setup */
#define CON_CHARACTER_MFA_DISABLE_CONFIRM       64  /* Confirm character MFA disable */
#define CON_CHARACTER_MFA_TOGGLE                40  /* Toggle character MFA */
#define CON_LINK_CHARACTER_MFA                  36  /* MFA for linking character */
#define CON_VERIFY_DELETE_MFA                   55  /* MFA for deletion */
#define CON_VERIFY_UNLINK_MFA                   72  /* MFA for unlinking */

/* CONSOLIDATION:
 * - Merge verify states: CON_MFA_VERIFY (with is_account flag)
 * - Merge menu states: CON_MFA_MENU (with is_account flag)
 * - Merge confirm states: CON_MFA_CONFIRM (with is_account flag)
 * - Use context for "for settings" vs "for login" vs "for operation"
 */

/*
 * EMAIL MANAGEMENT STATES
 */
#define CON_GET_EMAIL                       22  /* Legacy email */
#define CON_GET_ACCOUNT_EMAIL               29  /* Account email entry */
#define CON_CHANGE_ACCOUNT_EMAIL            50  /* Changing account email */
#define CON_VERIFY_ACCOUNT_EMAIL_CHANGE     68  /* Verify account email change */
#define CON_CONFIRM_EMAIL_FOR_RESET         23  /* Email for password reset */
#define CON_CONFIRM_ACCOUNT_EMAIL_FOR_RESET 46  /* Account email for reset */
#define CON_CHANGE_CHARACTER_EMAIL          69  /* Character email change */
#define CON_VERIFY_CHARACTER_EMAIL_CHANGE   70  /* Verify character email change */
#define CON_GET_STAFF_EMAIL                 57  /* Staff character email */

/* CONSOLIDATION: CON_EMAIL_SET, CON_EMAIL_VERIFY (with is_account/is_staff flags) */

/*
 * CHARACTER MANAGEMENT STATES
 */
#define CON_LINK_CHARACTER_NAME             34  /* Link character - enter name */
#define CON_LINK_CHARACTER_PASSWORD         35  /* Link character - password */
#define CON_CONFIRM_DELETE_CHARACTER        41  /* Confirm character deletion */
#define CON_CHARACTER_DELETE                75  /* Delete character */
#define CON_VERIFY_CHARACTER_DELETE         74  /* Verify character delete */
#define CON_CONFIRM_ACCOUNT_NAME            33  /* Confirm account name */

/* CONSOLIDATION: CON_CHARACTER_DELETE_CONFIRM (merge 41, 74, 75) */

/*
 * SPECIAL STATES
 */
#define CON_READ_IMOTD                  13  /* Reading immortal MOTD */
#define CON_READ_MOTD                   14  /* Reading MOTD */
#define CON_BREAK_CONNECT               15  /* Breaking connection */
#define CON_STAFF_MFA_PROMPT            58  /* Staff MFA prompt */
#define CON_STAFF_PASSWORD              59  /* Staff password */
#define CON_CONFIRM_STAFF_PASSWORD      60  /* Confirm staff password */

/*
 * OBSOLETE/UNUSED STATES
 */
/* #define CON_DEFAULT_CHOICE       10  - Commented out in merc.h */
/* #define CON_GEN_GROUPS           11  - Commented out in merc.h */
/* #define CON_PICK_WEAPON          12  - Commented out in merc.h */
/* #define CON_SELECT_CHARACTER     32  - Commented out in nanny.c case statement */
/* #define CON_OLD_SUBCLASS         18  - May be legacy/unused */
/* #define CON_SUBCLASS_CHOOSE      19  - May be legacy/unused */

#define CON_MAX 85

/*
 * CONSOLIDATION SUMMARY
 *
 * Current: 85 states (3 commented out = 82 active)
 * Target: ~45 states
 *
 * Merges Planned:
 * 1. Custom Pronouns: 9 states → 1 state + step counter (saves 8)
 * 2. MFA Verification: 6 account + 6 character = 12 → 3 states + flags (saves 9)
 * 3. Password Operations: 7 states → 3 states + context (saves 4)
 * 4. Email Operations: 9 states → 3 states + flags (saves 6)
 * 5. Character Deletion: 3 states → 1 state + substep (saves 2)
 * 6. Remove obsolete: 5 states (saves 5)
 *
 * Total savings: 8 + 9 + 4 + 6 + 2 + 5 = 34 states
 * New total: 85 - 34 = 51 states (close to target of ~45)
 *
 * Further optimization may reduce to exactly 45 by examining specific handlers.
 */

/*
 * Context Flags (to be added to DESCRIPTOR_DATA)
 * These allow states to be reused for different contexts:
 *
 * - bool is_account_context;       // Account vs character operation
 * - bool is_verify_context;        // Verify vs set operation
 * - bool is_settings_context;      // Settings menu vs login flow
 * - int pronoun_step;              // Which pronoun field we're on (0-6)
 * - int delete_confirm_step;       // Deletion confirmation substep
 */

#endif /* NANNY_STATES_H */
