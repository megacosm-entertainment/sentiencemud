# PLAN: CEdit Conventions (Match Existing OLC Editors)

This document defines non-negotiable implementation conventions for the channel editor (`cedit`).

`cedit` must follow the same framework and UX patterns as existing editors, especially:
- `src/editors/mobiles/medit.c`
- `src/editors/commands/cmdedit.c`

## 1) Editor Architecture (Required)

- Use unified OLC editor framework entry points:
  - `do_cedit(...)` as entry command
  - `cedit(...)` as interpreter
  - `olc_editor_enter(...)` for session start
  - `olc_editor_interp(...)` for command dispatch
- Define a static `OLC_EDITOR_DEF cedit_def` with:
  - `.name`, `.editor_type`, `.cmd_table`, `.show_fn`
  - `.tabs` (if using tabbed display)
  - `.theme`
  - `.perm`
  - `.change_mode`
  - `.mark_changed_fn` (if custom persistence trigger needed)
  - `.audit_changes`

## 2) Command Table Conventions (Required)

- Define a standard `cedit_table[]` using `olc_cmd_type` entries.
- Include conventional commands where applicable:
  - `?`, `commands`, `show`, `create`, `delete`, `comments`
- Keep command naming and behavior consistent with other editors.
- No ad-hoc parser bypassing framework dispatch.

## 3) Display and Tabs (Required)

- Use framework display helpers (`olc_display_*`) for consistent output formatting.
- If tabs are used, implement tab show functions with the same pattern as `medit`:
  - `cedit_show_<tab>_tab(...)`
- Keep labels and alignment consistent with established OLC output style.
- Avoid one-off custom formatting unless no framework helper exists.

## 4) Permission and Security Model (Required)

- Use `olc_editor_check_perm(...)` and `cedit_def.perm` for access control.
- Follow existing conventions for staff-rank and area-security style gating.
- Apply same security logging/denial style as other editors.

## 5) Change Tracking and Persistence (Required)

- Integrate with existing change-tracking conventions:
  - Area-flag change mode or explicit mark-changed callback, as appropriate.
- No hidden save side effects.
- Persistence contract must be explicit (JSON schema and save trigger path documented).

## 6) UX Consistency Requirements

- `do_cedit` behavior mirrors `do_cmdedit` / `do_medit` patterns:
  - Empty arg => helpful usage/default-item message
  - `create` path enters editor on success
  - Existing item selection path enters editor directly
- Error messaging style should match existing OLC voice/tone.

## 7) Scope for Phase 1

For Phase 1 completion, `cedit` must at minimum include:
- Framework wiring (`do_cedit`, `cedit`, `cedit_def`, `cedit_table`)
- `show` implementation with consistent display helpers
- CRUD skeleton for channel definitions
- Permission gates and change tracking integration

Advanced channel behavior (moderation policies, filtering rules, review actions) can be iteratively expanded after the editor foundation is in place.

## 8) Definition of Done (CEdit Conventions)

- [ ] `cedit` uses framework entry/interpreter pattern
- [ ] `cedit` has a proper `OLC_EDITOR_DEF`
- [ ] Command table follows OLC conventions
- [ ] Display uses standard OLC helpers (and tab conventions if tabs enabled)
- [ ] Permissions are framework-gated
- [ ] Change tracking is integrated and explicit
- [ ] No bespoke command parser flow outside OLC framework
