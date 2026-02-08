# Player Changelog

Changes since the account system update (ad9c53d).

## New Features

### Pronoun System
- Full custom pronoun support during character creation and in-game
- Choose from preset options (he/him, she/her, they/them) or define your own
- Set all five pronoun forms: subject, object, possessive adjective, possessive pronoun, and reflexive
- Verb conjugation preference (singular/plural) for custom pronouns
- Use `pronouns` command to view or change your pronouns at any time
- Use `pronouns show <subj> <obj> <poss_adj> <poss_pron> <refl>` to preview how pronouns will appear

### Body Type
- Body type is now separate from pronouns
- Choose your character's physical presentation independently of pronoun choice

### Account Improvements
- Set a default character to automatically select on login
- Tracks your last logged-in character for quick access

### Chat Rooms
- New `show` command for chat rooms to display room information
- Room creators and authorized staff can view chat room passwords

## Bug Fixes

### Stability
- Fixed SSL/TLS connectivity issues that could cause disconnections
- Fixed SIGPIPE errors that could crash SSL connections
- Fixed segfault in object update routines
- Fixed multiple crashes related to caching

### Reconnection
- Fixed lag when reconnecting with characters that have large inventories
- Fixed premature reconnection issues that could corrupt character data
- Fixed bug where accounts could be loaded multiple times, potentially causing data loss

### Characters
- Fixed issues with deleted characters not being handled properly
- Fixed pronoun and body type changes not applying correctly to mobs

## Removed Features

### Healer NPCs
- The automated healer system has been removed (functionality being reworked)

### Inter-MUD Communication
- IMC2 inter-mud chat system has been removed
