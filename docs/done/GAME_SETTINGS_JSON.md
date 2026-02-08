# Game Settings JSON Migration

## Overview

The game settings system has been converted from the legacy `.dat` text format to JSON with environment variable override support. This enables:

1. **Human-readable configuration** - JSON format with proper structure and comments
2. **Environment variable overrides** - Integration with Doppler or other secret management tools
3. **Backward compatibility** - Automatic migration from old `.dat` format
4. **Organized by category** - Settings grouped logically (email, missions, lockers, etc.)

## File Location

- **New format**: `/sentience/data/system/game_settings.json`
- **Old format** (auto-migrated): `/sentience/data/system/game_settings.dat`
- **Backup** (created during migration): `/sentience/data/system/game_settings.dat.backup`

## JSON Structure

The JSON file is organized into 8 categories:

```json
{
  "_version": "1.0",
  "_format": "game_settings",
  "email": {
    "email_enable": true,
    "email_host": "smtp.example.com",
    "email_port": 587,
    ...
  },
  "missions": {
    "max_mission_allowance": 5,
    "inc_missions": 1,
    ...
  },
  "lockers": { ... },
  "vaults": { ... },
  "coffers": { ... },
  "global": {
    "game_name": "SentienceMUD",
    "telnet_port": 9100,
    "tls_port": 9110,
    ...
  },
  "security": {
    "enable_passwd": true,
    "enable_mfa": true,
    "require_email_verif": false,
    ...
  },
  "mssp": { ... }
}
```

## Environment Variable Overrides

Environment variables take precedence over file settings. This is perfect for using with Doppler or other secret managers.

### Naming Convention

Environment variables use the prefix `SENTIENCE_` followed by the setting name in uppercase:

- Setting: `email_host` → Env var: `SENTIENCE_EMAIL_HOST`
- Setting: `telnet_port` → Env var: `SENTIENCE_TELNET_PORT`
- Setting: `enable_mfa` → Env var: `SENTIENCE_ENABLE_MFA`

### Examples

```bash
# Using Doppler
doppler run -- ./sent

# Manual environment variables
export SENTIENCE_EMAIL_HOST="smtp.example.com"
export SENTIENCE_EMAIL_USERNAME="noreply@example.com"
export SENTIENCE_EMAIL_PASSWORD="secret_password"
export SENTIENCE_TELNET_PORT=9100
export SENTIENCE_ENABLE_MFA=true

# SSL certificates from environment (useful for containers)
export SENTIENCE_SSL_CERT_DATA="$(cat /path/to/cert.pem)"
export SENTIENCE_SSL_KEY_DATA="$(cat /path/to/key.pem)"

./sent
```

### Value Formats

**Booleans**: `true`, `false`, `yes`, `no`, `1`, `0` (case-insensitive)
**Integers**: `9100`, `587`, etc.
**Strings**: Any text value
**Floats**: `1.5`, `2.0`, etc.

### Sensitive Settings

The following settings are marked as sensitive and will be logged as `<redacted>`:
- `email_password`
- `email_username`

## Migration Process

The first time you start the server after this update:

1. Server attempts to load `game_settings.json`
2. If not found, automatically migrates from `game_settings.dat`
3. Creates backup: `game_settings.dat.backup`
4. Writes new `game_settings.json`
5. Future startups load from JSON

## Example: Using Doppler for Secret Management

1. Install Doppler CLI:
```bash
# Install Doppler
curl -Ls https://cli.doppler.com/install.sh | sh
```

2. Login and setup:
```bash
# Login to Doppler
doppler login

# Setup project
doppler setup
```

3. Add secrets to Doppler:
```bash
# Email configuration
doppler secrets set SENTIENCE_EMAIL_HOST="smtp.aws.com"
doppler secrets set SENTIENCE_EMAIL_USERNAME="my-username"
doppler secrets set SENTIENCE_EMAIL_PASSWORD="my-secret-password"

# Server configuration
doppler secrets set SENTIENCE_TELNET_PORT="9100"
doppler secrets set SENTIENCE_TLS_PORT="9110"

# SSL Certificates (as multiline secrets)
doppler secrets set SENTIENCE_SSL_CERT_DATA="$(cat /path/to/certificate.pem)"
doppler secrets set SENTIENCE_SSL_KEY_DATA="$(cat /path/to/private-key.pem)"
```

4. Run the MUD with Doppler:
```bash
# Doppler will inject environment variables automatically
doppler run -- ./sent
```

## SSL Certificate Management

You can provide SSL certificates in two ways:

### Option 1: File Paths (Traditional)
Set paths in `game_settings.json`:
```json
{
  "global": {
    "ssl_cert_path": "/etc/ssl/certs/mycert.pem",
    "ssl_key_path": "/etc/ssl/private/mykey.pem"
  }
}
```

Or via environment variables:
```bash
export SENTIENCE_SSL_CERT_PATH="/etc/ssl/certs/mycert.pem"
export SENTIENCE_SSL_KEY_PATH="/etc/ssl/private/mykey.pem"
```

### Option 2: Certificate Data (Environment Variables)
**Priority**: If `SENTIENCE_SSL_CERT_DATA` or `SENTIENCE_SSL_KEY_DATA` are set, they take precedence over file paths.

Load certificates directly from environment (ideal for containers, Kubernetes, Doppler):
```bash
# Load from files into env vars
export SENTIENCE_SSL_CERT_DATA="$(cat /path/to/cert.pem)"
export SENTIENCE_SSL_KEY_DATA="$(cat /path/to/key.pem)"

# Or use Doppler to manage the certificate content
doppler secrets set SENTIENCE_SSL_CERT_DATA="$(cat cert.pem)"
doppler secrets set SENTIENCE_SSL_KEY_DATA="$(cat key.pem)"
```

**Benefits of environment variable approach:**
- No need to mount certificate files in containers
- Easier secret rotation with tools like Doppler or Kubernetes Secrets
- Certificates can be managed alongside other secrets
- Better for immutable infrastructure patterns

## Settings Categories

### Email (8 settings)
Email functionality for password resets, 2FA codes, etc.

### Missions (3 settings)
Mission system configuration

### Lockers (11 settings)
Personal storage configuration and rental settings

### Vaults (11 settings)
Account-wide shared storage configuration

### Coffers (6 settings)
Organization/church storage configuration

### Global (50+ settings)
Server-wide settings including:
- Server identity (name, description)
- Port configuration (telnet, TLS, websocket)
- SSL certificates
- Player limits (max characters, orgs, aliases)
- Timeout settings
- Lock controls (wizlock, new account lock, new char lock)

### Security (10 settings)
Authentication and security settings:
- Password requirements
- MFA/2FA configuration
- Email verification
- Login attempt limits

### MSSP (50+ settings)
MUD Server Status Protocol advertising settings

## Modification

Settings can be modified in three ways:

1. **Edit JSON file directly** - Edit `game_settings.json` and restart
2. **Use gameedit command** - In-game OLC editor (existing functionality preserved)
3. **Environment variables** - Override specific settings at runtime

### Viewing Setting Sources with gameedit

The `gameedit` command now displays the source of each setting value:

**In the list view:**
- Settings overridden by environment variables are marked with a cyan `E` flag
- Legend shows: `E Environment var   * Requires reboot   S Sensitive   X Not settable`

**In the detailed view:**
- Shows "Value Source: Environment Variable" or "Configuration File"
- Displays the exact environment variable name being used (e.g., `SENTIENCE_EMAIL_HOST`)

**When modifying environment-overridden settings:**
- You'll see a warning that the setting is overridden by an environment variable
- Changes will be saved to the config file but won't take effect until the env var is removed
- This helps prevent confusion when debugging configuration issues

Example output:
```
gameedit show email_host

+------------------------------------------------------------------------------+
| Setting: email_host                                                          |
+--------------------+---------------------------------------------------------+
| Category           | Email                                                   |
| Type               | String                                                  |
| OLC Settable       | Yes                                                     |
| Requires Reboot    | No                                                      |
| Sensitive          | No                                                      |
| Value Source       | Environment Variable                                    |
| Env Variable       | SENTIENCE_EMAIL_HOST                                    |
+--------------------+---------------------------------------------------------+
| Description:                                                                 |
| Hostname of the email server                                                 |
+------------------------------------------------------------------------------+
| Current Value: smtp.example.com                                              |
+------------------------------------------------------------------------------+
```

## Implementation Files

- [json_game_settings.h](src/json_game_settings.h) - Header file with function declarations
- [json_game_settings.c](src/json_game_settings.c) - Implementation with JSON loading, env var support, and migration
- [act_wiz.c](src/act_wiz.c) - Updated to use new JSON functions, old .dat functions kept for migration

## Benefits

1. **Better secret management** - No more credentials in version control
2. **Environment-specific config** - Different settings per environment (dev/staging/prod)
3. **Easy CI/CD integration** - Inject secrets via environment variables
4. **Human-readable** - JSON format is easier to read and edit
5. **Backward compatible** - Automatic migration from old format
6. **Documented structure** - Clear organization by category
