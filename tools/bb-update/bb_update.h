#ifndef BB_UPDATE_H
#define BB_UPDATE_H

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Update package (.bbu) constants
// ---------------------------------------------------------------------------
#define BB_UPDATE_MAGIC     0x42554842  // "BUHB" — Building Blocks Update
#define BB_UPDATE_VERSION   1
#define BB_SIG_MAX          512         // RSA-256 signature

// Manifest (contained in update package)
typedef struct {
    char     from_version[32];
    char     to_version[32];
    char     target_slot;              // 'a' or 'b' or '=' (auto: opposite of current)
    char     product[64];              // Must match manufacturing part number
    uint64_t timestamp;
    char     boot_sha256[65];          // SHA-256 of boot.tar.gz
    char     rootfs_sha256[65];        // SHA-256 of rootfs.tar.gz
    char     postinstall_sha256[65];   // SHA-256 of post-install.sh (empty if none)
} bb_update_manifest_t;

// ---------------------------------------------------------------------------
// Verification (runs on target or host)
// ---------------------------------------------------------------------------

// Verify a .bbu package signature and checksums
//   path: path to .bbu file
//   pubkey_path: path to public key PEM file (NULL = use built-in key)
// Returns 0 if valid, -1 on failure
int bb_update_verify(const char *path, const char *pubkey_path);

// Extract manifest from .bbu without full verification
int bb_update_read_manifest(const char *path, bb_update_manifest_t *out);

// ---------------------------------------------------------------------------
// Installation (runs on target only)
// ---------------------------------------------------------------------------

// Install a verified .bbu package
//   1. Determine target slot (auto-select opposite of current)
//   2. Format boot_<target> and rootfs_<target>
//   3. Extract boot.tar.gz → boot partition
//   4. Extract rootfs.tar.gz → rootfs partition
//   5. Run post-install.sh (if present)
//   6. Set U-Boot env: boot_slot=<target>, boot_attempt=boot_limit, boot_ok=0
//   7. Log to /persist/update-log.json
// Returns 0 on success
int bb_update_install(const char *path);

// Get current slot info
int  bb_update_current_slot(void);   // returns 'a' or 'b'
void bb_update_current_version(char *buf, size_t len);

// ---------------------------------------------------------------------------
// Creation (runs on host build machine)
// ---------------------------------------------------------------------------

// Create a .bbu update package
//   output: output .bbu file path
//   manifest: update manifest (caller fills version/product/timestamp)
//   boot_tar: path to boot.tar.gz (or NULL to skip)
//   rootfs_tar: path to rootfs.tar.gz
//   postinstall: path to post-install.sh (or NULL)
//   privkey_path: path to private key PEM for signing
int bb_update_create(const char *output,
                     const bb_update_manifest_t *manifest,
                     const char *boot_tar,
                     const char *rootfs_tar,
                     const char *postinstall,
                     const char *privkey_path);

#endif // BB_UPDATE_H
