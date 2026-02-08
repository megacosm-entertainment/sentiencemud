# SSL Certificate Environment Variable Support

## Overview

SSL certificates can now be loaded directly from environment variables instead of requiring files on disk. This is particularly useful for:

- **Container deployments** (Docker, Kubernetes)
- **Secret management tools** (Doppler, Vault, AWS Secrets Manager)
- **Immutable infrastructure** patterns
- **Certificate rotation** without filesystem changes

## How It Works

The system checks for certificates in this order:

1. **Environment Variable First**: `SENTIENCE_SSL_CERT_DATA` and `SENTIENCE_SSL_KEY_DATA`
2. **File Path Fallback**: If env vars aren't set, uses paths from game settings

This means you can use environment variables to override file-based certificates without changing your configuration.

## Environment Variables

### `SENTIENCE_SSL_CERT_DATA`
Contains the full PEM-encoded SSL certificate.

### `SENTIENCE_SSL_KEY_DATA`
Contains the full PEM-encoded private key.

## Usage Examples

### Basic Usage

```bash
# Load certificate and key from files into environment
export SENTIENCE_SSL_CERT_DATA="$(cat /path/to/certificate.pem)"
export SENTIENCE_SSL_KEY_DATA="$(cat /path/to/private-key.pem)"

# Start the server
./sent
```

### With Doppler

```bash
# Add certificates to Doppler
doppler secrets set SENTIENCE_SSL_CERT_DATA="$(cat certificate.pem)"
doppler secrets set SENTIENCE_SSL_KEY_DATA="$(cat private-key.pem)"

# Run with Doppler
doppler run -- ./sent
```

### Docker Compose

```yaml
version: '3.8'
services:
  sentience:
    image: sentience:latest
    environment:
      SENTIENCE_SSL_CERT_DATA: |
        -----BEGIN CERTIFICATE-----
        MIIFXzCCBEegAwIBAgIQBLQHKPPJkZbGPDDUhLqw...
        -----END CERTIFICATE-----
      SENTIENCE_SSL_KEY_DATA: |
        -----BEGIN PRIVATE KEY-----
        MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSk...
        -----END PRIVATE KEY-----
```

### Kubernetes Secret

```yaml
apiVersion: v1
kind: Secret
metadata:
  name: sentience-ssl
type: Opaque
stringData:
  cert.pem: |
    -----BEGIN CERTIFICATE-----
    MIIFXzCCBEegAwIBAgIQBLQHKPPJkZbGPDDUhLqw...
    -----END CERTIFICATE-----
  key.pem: |
    -----BEGIN PRIVATE KEY-----
    MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSk...
    -----END PRIVATE KEY-----
---
apiVersion: v1
kind: Pod
metadata:
  name: sentience
spec:
  containers:
  - name: sentience
    image: sentience:latest
    env:
    - name: SENTIENCE_SSL_CERT_DATA
      valueFrom:
        secretKeyRef:
          name: sentience-ssl
          key: cert.pem
    - name: SENTIENCE_SSL_KEY_DATA
      valueFrom:
        secretKeyRef:
          name: sentience-ssl
          key: key.pem
```

### AWS Secrets Manager (via startup script)

```bash
#!/bin/bash

# Fetch certificates from AWS Secrets Manager
export SENTIENCE_SSL_CERT_DATA=$(aws secretsmanager get-secret-value \
  --secret-id sentience/ssl/cert \
  --query SecretString \
  --output text)

export SENTIENCE_SSL_KEY_DATA=$(aws secretsmanager get-secret-value \
  --secret-id sentience/ssl/key \
  --query SecretString \
  --output text)

# Start the server
./sent
```

## Certificate Format

Both environment variables expect PEM format:

**Certificate (`SENTIENCE_SSL_CERT_DATA`):**
```
-----BEGIN CERTIFICATE-----
MIIFXzCCBEegAwIBAgIQBLQHKPPJkZbGPDDUhLqwDDANBgkqhkiG9w0BAQsFADBG
MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRUwEwYDVQQLEwxTZXJ2ZXIg
...
-----END CERTIFICATE-----
```

**Private Key (`SENTIENCE_SSL_KEY_DATA`):**
```
-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDH8NZ0qLPD7KqN
8jW0Yx5tY0cF3a9vQxMj0VjGmGzGRLtYZLXvH8yDqN7aGxKpPvZh7fWqN3L+YmkN
...
-----END PRIVATE KEY-----
```

## Logging

When certificates are loaded, the system logs:
- `"Loading SSL certificate from environment variable"` - When cert is loaded from env
- `"Loading SSL private key from environment variable"` - When key is loaded from env
- If env vars aren't set, it silently falls back to file paths (original behavior)

## Benefits Over File-Based Certificates

1. **No File I/O Required**: Certificates don't need to be on disk
2. **Secret Management Integration**: Works seamlessly with modern secret management
3. **Container-Friendly**: Perfect for ephemeral containers
4. **Easy Rotation**: Update env vars without touching configuration files
5. **Security**: Secrets managed by specialized tools (Doppler, Vault, etc.)
6. **12-Factor App Compliance**: Configuration via environment variables

## Fallback Behavior

If environment variables are not set, the system falls back to the traditional file-based approach using:
- `game_settings.ssl_cert_path`
- `game_settings.ssl_key_path`

You can also override these paths with environment variables:
- `SENTIENCE_SSL_CERT_PATH`
- `SENTIENCE_SSL_KEY_PATH`

## Implementation Details

**Modified File**: [tls.c](src/tls.c) - `configure_context()` function

The function now:
1. Checks for `SENTIENCE_SSL_CERT_DATA` environment variable
2. If found, loads certificate from the variable using OpenSSL BIO
3. If not found, loads from file path (original behavior)
4. Repeats same logic for private key with `SENTIENCE_SSL_KEY_DATA`
5. Validates that private key matches certificate

## Security Considerations

- Environment variables are process-scoped and not visible to other users
- Ensure your secret management tool properly restricts access
- Use encrypted secrets at rest in Kubernetes/Docker Swarm
- Rotate certificates regularly through your secret management system
- Never commit certificates to version control (use `.env` files in `.gitignore`)

## Troubleshooting

**Certificate load fails:**
- Ensure PEM format is correct (including line breaks)
- Check for trailing whitespace or missing headers/footers
- Verify the certificate and key match
- Check server logs for specific OpenSSL errors

**Environment variable not recognized:**
- Confirm variable name is exactly `SENTIENCE_SSL_CERT_DATA` (case-sensitive)
- Verify the variable is exported: `echo $SENTIENCE_SSL_CERT_DATA`
- Check that the process inherits the environment
