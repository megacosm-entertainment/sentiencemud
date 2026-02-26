# Developer Guides

This directory is for practical, implementation-facing documentation that is not a plan, worklog, or analysis artifact.

Use this directory for stable operational and engineering references such as subsystem guides, API contracts, and maintenance playbooks.

## Guide Taxonomy

### Subsystem References

- [BUFFER System Guide](BUFFER_SYSTEM_GUIDE.md)
- [Event Editor Runtime Reference](EVENT_EDITOR_RUNTIME.md)

### Operational/Admin Guides

- [Event System Admin Guide](EVENT_SYSTEM_ADMIN_GUIDE.md)
- [Channel System Documentation Index](channels/README.md)
- [Channels Admin Guide](channels/CHANNELS_ADMIN_GUIDE.md)
- [Channels Player Guide](channels/CHANNELS_PLAYER_GUIDE.md)

### Feature Implementation Guides

- [RSGEdit Usage and Integration Guide](RSGEDIT_USAGE.md)
- [Adding Skills, Spells, and Commands](SKILL_SPELL_COMMAND_GUIDE.md)
- [Wilds Systems Scripting Guide](WILDS_SYSTEMS_SCRIPTING_GUIDE.md)

## Scope Rules

- Put long-lived how-to/reference docs here.
- Keep planning docs as `PLAN_*` at `src/docs/` root.
- Keep execution logs as `WORKLOG_*` at `src/docs/` root.
- Keep debt tracking in `TECH_DEBT.md`.
- Keep historical audits/analysis docs at `src/docs/` root unless they become evergreen guides.
