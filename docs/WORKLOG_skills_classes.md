# Worklog: Skill and Class System Backport Analysis

**Date:** 2026-02-09

## Objective

Analyze the refactored Skill system and the new Class/Job system from the `src_20_dev` branch. Compare them to the current `src` implementation and create a comprehensive backporting plan.

## Work Performed

### Part 1: Skill System Refactor

1.  **Initial Investigation (`src_20_dev`):**
    -   Referenced `PLAN_SKILL_REFACTOR.md` which pointed to `skedit` in `src_20_dev`.
    -   Searched for `skedit` to identify key implementation files: `skills.c` (OLC logic), `olc.c` (command table and interpreter), and `olc.h` (declarations).

2.  **System Analysis (`src_20_dev`):**
    -   Confirmed the existence of a full OLC editor for skills.
    -   Verified that the system is designed to load skill data from external sources (implied to be JSON) and manage them in memory, likely with a hash table for fast lookups.
    -   This implementation completely decouples skill definitions from the compiled C code, allowing for live editing.

3.  **Current State Analysis (`src`):**
    -   Confirmed that `src` still uses the hardcoded `skill_table` array in `const.c` and `gsn_` global variables for skill lookups.

### Part 2: Class/Job System

1.  **Initial Investigation (`src_20_dev`):**
    -   Referenced `PLAN_CLASS_JOB_SYSTEM_BACKPORT.md`.
    -   Searched for the player-facing command `do_setclass` to find the core logic.
    -   Identified `skills.c` as the main implementation file for the class-switching logic.
    -   Located the data structure definitions in `merc.h`.

2.  **System Analysis (`src_20_dev`):**
    -   Examined `merc.h` and found the `CLASS_DATA` and `CLASS_LEVEL` structs.
    -   Confirmed that `PC_DATA` was modified to use a `LLIST *classes` and a `CLASS_LEVEL *current_class` pointer, replacing the old integer-based system. This enables independent leveling for any number of classes.
    -   Noted the plan for a `clsedit` command for OLC management.

3.  **Current State Analysis (`src`):**
    -   Searched for `sub_class_current` and confirmed the legacy system is still in use.
    -   The `pc_data` struct in `src/merc.h` contains multiple integer fields for tracking class and subclass progression, and the code relies on the static `sub_class_table`.

### Part 3: Documentation and Planning

1.  **Synthesized Findings:**
    -   Combined the analysis for both the skill and class systems.
    -   Acknowledged the interconnectedness of the new systems, particularly how the class system is intended to work with the previously analyzed reputation system.

2.  **Created Combined Planning Document:**
    -   Authored `src/docs/PLAN_backport_skills_classes.md`.
    -   This document details the architecture of both new systems as found in `src_20_dev`.
    -   It contrasts them with the legacy systems currently in `src`.
    -   It provides a clear, phased, step-by-step plan for backporting each system, including data structure migration, logic refactoring, persistence changes (to JSON), and OLC integration.

## Next Steps

-   Await user review of the newly created planning document.
-   Continue with further analysis tasks as directed by the user.
