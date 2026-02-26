# Crypto Modernization Plan

## Overview

Modernize Sentience's cryptographic implementations to use libsodium's state-of-the-art primitives, replacing legacy and insecure methods.

## Current State Analysis

### Password Hashing (CRITICAL ISSUE)
**Location:** `src/account/auth.c:315-324`

**Current implementation:**
```c
char *hash_password(const char *plaintext) {
    char *hashed = crypt(plaintext, plaintext);  // Using plaintext as salt!
    return str_dup(hashed);
}
```

**Problems:**
1. **Using plaintext as salt** - Completely defeats the purpose of salting
2. `crypt()` defaults to DES on many systems (56-bit key, trivially broken)
3. Even with SHA-256 mode, `crypt()` is not memory-hard (vulnerable to GPU/ASIC brute force)
4. Legacy fallback to custom `sha256_crypt()` (also not memory-hard)

**Password Versions:**
```c
#define PWD_VER_PLAINTEXT 0       // Ancient, should not exist
#define PWD_VER_SHA256_CUSTOM 1   // Legacy custom implementation
#define PWD_VER_CRYPT_SYSTEM 2    // Current, but insecure salt
```

### OTP Key Encryption
**Location:** `src/handler.c:11439-11554`

**Current implementation:**
- AES-256-CBC with OpenSSL EVP
- Random IV per encryption ✅
- Base64 encoding for storage ✅
- Key stored in plaintext file: `data/system/.crypto_key` ⚠️
- **No authenticated encryption** - vulnerable to padding oracle attacks ⚠️

### Crypto Key Management
**Location:** `src/handler.c:11350-11385`

**Current implementation:**
- Single 32-byte key generated from `/dev/urandom`
- Stored in plaintext file with 600 permissions
- No key derivation
- No rotation mechanism

## Implementation Plan

### Phase 1: Core Libsodium Integration

#### 1.1 Create `src/account/auth_sodium.c`
New file with modern crypto primitives:
- `hash_password_v3()` - Argon2id password hashing
- `verify_password_v3()` - Constant-time password verification
- `encrypt_string_v2()` - Authenticated encryption (XSalsa20-Poly1305)
- `decrypt_string_v2()` - Authenticated decryption
- `init_sodium()` - Initialize libsodium library

#### 1.2 Create `src/account/auth_sodium.h`
Public API declarations.

#### 1.3 Update Build Files
- Add `account/auth_sodium.c` to Makefile
- Add `account/auth_sodium.c` to CMakeLists.txt
- Ensure `-lsodium` is in linker flags

### Phase 2: Password Migration System

#### 2.1 Add Password Version
```c
#define PWD_VER_ARGON2ID 3
```

#### 2.2 Update `verify_password()` in `auth.c`
- Detect password version from hash format
- Try verification with appropriate method
- On successful login with old version, trigger rehash

#### 2.3 Create Migration Functions
```c
bool needs_password_upgrade(const char *hash);
char *upgrade_password(const char *plaintext);
```

#### 2.4 Integrate with Login Flow
- In `nanny.c` login path
- Rehash on successful authentication
- Save upgraded hash immediately

### Phase 3: Update Bootstrap

#### 3.1 Modify `bootstrap/bootstrap_account.c`
- Use `hash_password_v3()` for new accounts
- Set `passwd_version = PWD_VER_ARGON2ID`

#### 3.2 Update Comments
Document that bootstrap creates modern Argon2id hashes.

### Phase 4: Staff Migration Command

#### 4.1 Create `do_pwmigrate()` Command
**Location:** New command in `act_wiz.c` or dedicated file

**Features:**
- Check all accounts/characters for old password hashes
- Report migration candidates
- Bulk upgrade with confirmation
- Skip accounts that can't be migrated (need user login)

**Syntax:**
```
pwmigrate check          - List accounts with old password versions
pwmigrate status         - Show migration statistics
```

**Notes:**
- Cannot directly migrate passwords (need plaintext for rehashing)
- Old hashes remain until user logs in
- Command shows which accounts will auto-upgrade on next login

## Technical Details

### Argon2id Parameters

**Interactive (login):**
```c
crypto_pwhash_OPSLIMIT_INTERACTIVE  // 2 operations
crypto_pwhash_MEMLIMIT_INTERACTIVE  // 64 MiB memory
```

**Timing:** ~0.1 seconds on modern hardware
**Security:** Resistant to GPU/ASIC attacks

**Sensitive (high security):**
```c
crypto_pwhash_OPSLIMIT_SENSITIVE    // 4 operations
crypto_pwhash_MEMLIMIT_SENSITIVE    // 1 GiB memory
```

**When to use:** Staff accounts, password changes, optional user setting

### Authenticated Encryption

**XSalsa20-Poly1305 (secretbox):**
- Encryption: XSalsa20 stream cipher
- Authentication: Poly1305 MAC
- Nonce: 24 bytes (random per encryption)
- Key: 32 bytes (existing crypto_key)
- MAC: 16 bytes (prepended to ciphertext)

**Benefits over AES-CBC:**
- Authenticated (detects tampering)
- Faster on non-AES-NI systems
- Misuse-resistant
- No padding oracle attacks

### Password Hash Format

**Argon2id hash string:**
```
$argon2id$v=19$m=65536,t=2,p=1$base64salt$base64hash
```

**Detection:**
- Argon2id: starts with `$argon2id$`
- SHA256 crypt: starts with `$5$` or `$6$`
- Custom SHA256: fixed length, no `$` prefix
- DES crypt: 13 characters

### Migration Path

**Automatic migration on login:**
1. User enters password
2. `verify_password()` detects old hash format
3. Verification succeeds with old method
4. Generate new Argon2id hash from plaintext
5. Update account/character with new hash
6. Save immediately
7. User continues login (transparent)

**Manual checks:**
- Staff command shows upgrade status
- No bulk migration possible (need plaintext)
- Old hashes persist until login

## Testing Strategy

### Unit Tests
1. **Argon2id hashing:**
   - Hash generation
   - Verification success
   - Verification failure
   - Constant-time properties

2. **Authenticated encryption:**
   - Encrypt/decrypt round-trip
   - Tamper detection
   - Random nonce uniqueness

3. **Password migration:**
   - Version detection
   - Upgrade triggering
   - Backward compatibility

### Integration Tests
1. **Bootstrap:**
   - New accounts created with v3
   - Verification works
   - Login succeeds

2. **Login with migration:**
   - Old password verifies
   - Automatic upgrade
   - New hash saved
   - Subsequent login uses new hash

3. **Mixed versions:**
   - Old accounts still work
   - New accounts work
   - Staff accounts work

### Security Tests
1. **Timing attacks:**
   - Constant-time verification
   - No early exits

2. **Tampering:**
   - Modified ciphertext rejected
   - MAC verification

## Files to Modify

### New Files
- `src/account/auth_sodium.c` - Libsodium crypto implementations
- `src/account/auth_sodium.h` - Public API

### Modified Files
- `src/account/auth.c` - Add migration logic, version detection
- `src/account/auth.h` - Add PWD_VER_ARGON2ID constant
- `src/bootstrap/bootstrap_account.c` - Use v3 hashing
- `src/nanny.c` - Trigger migration on login
- `src/merc.h` - Add new constants if needed
- `src/Makefile` - Add auth_sodium.c
- `src/CMakeLists.txt` - Add auth_sodium.c

### Optional Files
- `src/act_wiz.c` - Add do_pwmigrate command
- `src/interp.c` - Register pwmigrate command

## Implementation Steps

### Day 1: Core Implementation
1. Create `auth_sodium.c` with Argon2id and authenticated encryption
2. Add to build files
3. Test compilation
4. Unit tests for new functions

### Day 2: Migration System
1. Add version detection to `verify_password()`
2. Implement auto-upgrade on login
3. Update bootstrap to use v3
4. Integration tests

### Day 3: Staff Tools & Documentation
1. Create `do_pwmigrate` command
2. Test migration scenarios
3. Update user documentation
4. Code review and cleanup

## Security Considerations

### Backward Compatibility
- **Keep old verification methods** for existing accounts
- Mark as deprecated in code comments
- Plan removal for major version bump (2-3 releases out)

### Key Storage
- Current: plaintext file with 600 perms
- Future: consider key derivation from passphrase
- Future: consider environment variable support

### Recovery Codes
- Currently stored in plaintext
- Consider hashing (use fast hash, codes are already random)
- Update on migration

### OTP Keys
- Currently: AES-256-CBC (no MAC)
- New: XSalsa20-Poly1305 (authenticated)
- **Can't directly migrate** - need plaintext key
- Add v2 encryption on next MFA setup
- Keep v1 decryption for existing keys

## Future Enhancements

### Phase 5: Key Derivation (Post-MVP)
- Derive crypto_key from passphrase
- Support environment variables
- Enable key rotation

### Phase 6: OTP Key Migration (Post-MVP)
- Detect v1 encrypted keys
- Re-encrypt with v2 on next use
- Remove v1 decryption after migration period

### Phase 7: Recovery Code Hashing (Post-MVP)
- Hash recovery codes instead of plaintext storage
- Update generation and verification

## References

- [libsodium documentation](https://doc.libsodium.org/)
- [Argon2 RFC 9106](https://www.rfc-editor.org/rfc/rfc9106.html)
- [XSalsa20-Poly1305 specification](https://nacl.cr.yp.to/secretbox.html)
- [Password Storage Cheat Sheet (OWASP)](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)

## Success Criteria

- [ ] All new accounts use Argon2id
- [ ] Bootstrap creates secure passwords
- [ ] Existing accounts migrate transparently on login
- [ ] No password verification regressions
- [ ] OTP keys use authenticated encryption
- [ ] Build succeeds on all platforms
- [ ] All tests pass
- [ ] No performance regressions on login
- [ ] Code review complete
- [ ] Documentation updated
