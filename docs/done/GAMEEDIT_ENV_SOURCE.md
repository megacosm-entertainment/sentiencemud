# gameedit Environment Variable Source Display

## Overview

The `gameedit` command now clearly shows which settings are overridden by environment variables, helping administrators understand the effective configuration and debug issues.

## Features

### 1. Visual Indicators in List View

When viewing settings with `gameedit show` or `gameedit show <category>`, environment-overridden settings are marked with a cyan `E` flag in the type column.

Example:
```
+-------------------------+--------------------------------------+------------+
| Setting Name            | Current Value                        | Type       |
+-------------------------+--------------------------------------+------------+
| email_host              | smtp.example.com                     | String  E  |
| email_port              | 587                                  | Integer    |
| email_password          | *****                                | String ES  |
+-------------------------+--------------------------------------+------------+
```

Legend: `E` = Environment var, `*` = Requires reboot, `S` = Sensitive, `X` = Not settable

### 2. Detailed Source Information

When viewing a specific setting with `gameedit show <setting>`, you'll see:

- **Value Source**: Whether the value comes from "Environment Variable" or "Configuration File"
- **Env Variable Name**: The exact environment variable name (e.g., `SENTIENCE_EMAIL_HOST`)

Example:
```
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
```

### 3. Warning When Modifying Override Settings

When you try to modify a setting that's overridden by an environment variable:

```
gameedit set email_host "smtp.newhost.com"

WARNING: This setting is currently overridden by an environment variable.
         The change will be saved to the config file, but will only take effect
         after the environment variable is removed and the server is restarted.

Setting added to pending changes.
```

This prevents confusion when changes appear not to take effect.

## Use Cases

### Debugging Configuration

**Scenario**: You change `telnet_port` in the config file but it doesn't seem to take effect.

**Solution**: Run `gameedit show telnet_port` to see:
```
Value Source       | Environment Variable
Env Variable       | SENTIENCE_TELNET_PORT
```

Now you know an environment variable is overriding your config file change.

### Understanding Effective Configuration

**Scenario**: You need to know the current email configuration for troubleshooting.

**Solution**: Run `gameedit show email` to see all email settings with their sources:
```
| email_host              | smtp.aws.com           | String  E  |
| email_username          | *****                  | String ES  |
| email_password          | *****                  | String ES  |
| email_port              | 587                    | Integer    |
```

Settings marked with `E` are from Doppler/environment; others are from the config file.

### Audit Trail

**Scenario**: During a security audit, you need to document where sensitive settings are stored.

**Solution**: Review all sensitive settings to see which are managed by environment variables (e.g., Doppler) vs config files:
```
gameedit show | grep "S"

| email_username          | *****                  | String ES  |  <- Env var
| email_password          | *****                  | String ES  |  <- Env var
| ssl_key_path            | /etc/ssl/key.pem       | String S   |  <- Config file
```

## Implementation Details

### Functions Added

**In [json_game_settings.c](src/json_game_settings.c):**
- `json_setting_is_env_override(setting_name)` - Checks if a setting has an active env var override
- Returns true if `SENTIENCE_<SETTING_NAME>` environment variable exists and is non-empty

**In [gameedit.c](src/editors/game_settings/gameedit.c):**
- Enhanced `gameedit_display_setting()` to show `E` flag and check override status
- Enhanced individual setting view to show source and env var name
- Added warning in `gameedit_set_value()` when modifying overridden settings

### Color Coding

- **Cyan `E`** - Environment variable override (stands out but not alarming)
- **Red `*`** - Requires reboot (important to notice)
- **Magenta `S`** - Sensitive (security-related)
- **Dark `X`** - Not settable via OLC (informational)

## Best Practices

### 1. Use Environment Variables for Secrets

Environment-overridden settings marked with `E` are perfect for:
- Passwords (`email_password`)
- API keys
- SSL certificates (via `SENTIENCE_SSL_CERT_DATA`)

### 2. Use Config Files for Static Settings

Settings without `E` are good for:
- Port numbers (unless varying by environment)
- Feature flags
- Capacity limits

### 3. Document Your Environment Variables

When you see `E` flags, document which tool manages those variables:
- Doppler secrets
- Kubernetes ConfigMaps/Secrets
- Docker environment
- AWS Secrets Manager

### 4. Regular Audits

Periodically review `gameedit show` to ensure:
- Secrets are managed by environment variables (`E` flag present)
- Non-sensitive settings are in config files (no `E` flag)
- No unexpected overrides are active

## Troubleshooting

### Setting Changes Don't Take Effect

**Check for `E` flag:**
```bash
gameedit show <setting>
```

If you see "Value Source: Environment Variable", your change won't take effect until:
1. The environment variable is removed/unset
2. The server is restarted

### Can't Find Where Setting is Defined

**Use gameedit to locate:**
```bash
gameedit show <setting>
```

Shows exactly where the value comes from:
- "Environment Variable" → Check your secret management tool
- "Configuration File" → Check `game_settings.json`

### Environment Variable Not Being Applied

**Verify it's set:**
```bash
echo $SENTIENCE_EMAIL_HOST
```

**Check naming:**
- Must be uppercase: `SENTIENCE_EMAIL_HOST`
- Must use underscores: `SENTIENCE_` prefix
- Setting name converted: `email_host` → `EMAIL_HOST`

**Verify process sees it:**
```bash
# While server is running
ps aux | grep sent
cat /proc/<pid>/environ | tr '\0' '\n' | grep SENTIENCE_
```

## Security Considerations

### Sensitive Settings

Settings marked with both `E` and `S` flags are ideal candidates for secret management:
- Email credentials
- Database passwords
- API tokens

These should always be managed via:
- Doppler
- HashiCorp Vault
- AWS Secrets Manager
- Kubernetes Secrets

### Avoid Logging

When viewing sensitive settings with the `S` flag:
- Values are hidden from non-high-security staff
- Shown as `*****` in output
- This applies to both env var and file-based values

### Environment Variable Audit

Use `gameedit show` to audit which sensitive settings are:
- Properly managed by environment variables (`ES` flags)
- Still in config files (`S` flags only - should migrate to env vars)

## Examples

### Scenario 1: Migrating to Doppler

**Before migration:**
```
gameedit show email

| email_host              | smtp.aws.com           | String     |
| email_username          | *****                  | String S   |
| email_password          | *****                  | String S   |
```

**After adding to Doppler:**
```
gameedit show email

| email_host              | smtp.aws.com           | String  E  |
| email_username          | *****                  | String ES  |
| email_password          | *****                  | String ES  |
```

Now you can see at a glance that email credentials are managed by Doppler.

### Scenario 2: Different Config Per Environment

**Development environment:**
```
| telnet_port             | 9100                   | Integer    |
| tls_port                | 9110                   | Integer    |
```

**Production environment (with overrides):**
```
| telnet_port             | 4000                   | Integer E  |
| tls_port                | 4001                   | Integer E  |
```

The `E` flags clearly show production uses environment-specific port configuration.

### Scenario 3: SSL Certificate Sources

**File-based SSL:**
```
gameedit show ssl_cert_path

Value Source       | Configuration File
Current Value      | /etc/ssl/certs/mycert.pem
```

**Environment-based SSL:**
```
# After setting SENTIENCE_SSL_CERT_DATA
gameedit show ssl_cert_path

Value Source       | Configuration File
Current Value      | /etc/ssl/certs/mycert.pem

Note: SSL certificates are loaded from SENTIENCE_SSL_CERT_DATA/KEY_DATA
      when those environment variables are set (takes precedence over paths)
```

(SSL certificates use separate env vars `SENTIENCE_SSL_CERT_DATA` that don't override the path settings, but do take precedence at runtime)
