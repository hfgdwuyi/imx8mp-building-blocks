#include "bb_update.h"
#include "bb_board.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// OpenSSL for RSA-256 signature (disable with -DBB_UPDATE_NO_CRYPTO)
#ifndef BB_UPDATE_NO_CRYPTO
#if __has_include(<openssl/evp.h>) && __has_include(<openssl/pem.h>) && __has_include(<openssl/err.h>)
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#define BB_UPDATE_HAS_CRYPTO 1
#else
#define BB_UPDATE_HAS_CRYPTO 0
#endif
#else
#define BB_UPDATE_HAS_CRYPTO 0
#endif

// ---------------------------------------------------------------------------
// Signature helpers
// ---------------------------------------------------------------------------

#if BB_UPDATE_HAS_CRYPTO
static int sign_manifest(const bb_update_manifest_t *m, const char *privkey_path,
                         uint8_t sig_out[BB_SIG_MAX])
{
    FILE *kf = fopen(privkey_path, "r");
    if (!kf) {
        fprintf(stderr, "Cannot open private key: %s\n", privkey_path);
        return -1;
    }
    EVP_PKEY *pkey = PEM_read_PrivateKey(kf, NULL, NULL, NULL);
    fclose(kf);
    if (!pkey) {
        fprintf(stderr, "Failed to read private key\n");
        return -1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx || EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, pkey) != 1) {
        fprintf(stderr, "DigestSignInit failed\n");
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    size_t sig_len = BB_SIG_MAX;
    if (EVP_DigestSign(ctx, sig_out, &sig_len, (const uint8_t *)m, sizeof(*m)) != 1) {
        fprintf(stderr, "Signing failed: %s\n",
                ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    // Zero-pad remaining bytes
    if (sig_len < BB_SIG_MAX)
        memset(sig_out + sig_len, 0, BB_SIG_MAX - sig_len);

    EVP_PKEY_free(pkey);
    EVP_MD_CTX_free(ctx);
    return 0;
}

static int verify_signature(const bb_update_manifest_t *m,
                            const uint8_t sig[BB_SIG_MAX],
                            const char *pubkey_path)
{
    EVP_PKEY *pkey = NULL;

    if (pubkey_path) {
        FILE *kf = fopen(pubkey_path, "r");
        if (!kf) {
            fprintf(stderr, "Cannot open public key: %s\n", pubkey_path);
            return -1;
        }
        pkey = PEM_read_PUBKEY(kf, NULL, NULL, NULL);
        fclose(kf);
    }

    if (!pkey) {
        fprintf(stderr, "No public key available for verification\n");
        return -1;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx || EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pkey) != 1) {
        fprintf(stderr, "DigestVerifyInit failed\n");
        EVP_PKEY_free(pkey);
        EVP_MD_CTX_free(ctx);
        return -1;
    }

    // Determine actual signature length (strip trailing zeros)
    size_t sig_len = BB_SIG_MAX;
    while (sig_len > 0 && sig[sig_len - 1] == 0)
        sig_len--;

    int rc = EVP_DigestVerify(ctx, sig, sig_len, (const uint8_t *)m, sizeof(*m));
    EVP_PKEY_free(pkey);
    EVP_MD_CTX_free(ctx);

    if (rc == 1) return 0;
    if (rc == 0) fprintf(stderr, "Signature verification FAILED\n");
    else fprintf(stderr, "Signature verification error\n");
    return -1;
}
#else
static int sign_manifest(const bb_update_manifest_t *m, const char *privkey_path,
                         uint8_t sig_out[BB_SIG_MAX])
{
    (void)m; (void)privkey_path;
    memset(sig_out, 0, BB_SIG_MAX);
    fprintf(stderr, "WARNING: Crypto disabled, package will not be signed\n");
    return 0;
}

static int verify_signature(const bb_update_manifest_t *m,
                            const uint8_t sig[BB_SIG_MAX],
                            const char *pubkey_path)
{
    (void)m; (void)sig; (void)pubkey_path;
    fprintf(stderr, "WARNING: Crypto disabled, skipping signature verification\n");
    return 0;
}
#endif // BB_UPDATE_HAS_CRYPTO

// ---------------------------------------------------------------------------
// Verify
// ---------------------------------------------------------------------------
int bb_update_verify(const char *path, const char *pubkey_path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Cannot open %s\n", path);
        return -1;
    }

    // Read magic
    uint32_t magic;
    if (fread(&magic, 4, 1, f) != 1 || magic != BB_UPDATE_MAGIC) {
        fprintf(stderr, "Invalid update package (bad magic: 0x%08x)\n", magic);
        fclose(f);
        return -1;
    }

    // Read version
    uint32_t ver;
    if (fread(&ver, 4, 1, f) != 1) {
        fprintf(stderr, "Truncated header\n");
        fclose(f);
        return -1;
    }
    printf("Package format version: %u\n", ver);

    // Read manifest
    bb_update_manifest_t m;
    if (fread(&m, sizeof(m), 1, f) != 1) {
        fprintf(stderr, "Truncated manifest\n");
        fclose(f);
        return -1;
    }

    // Read signature
    uint8_t sig[BB_SIG_MAX];
    if (fread(sig, BB_SIG_MAX, 1, f) != 1) {
        fprintf(stderr, "Truncated signature\n");
        fclose(f);
        return -1;
    }
    fclose(f);

    printf("Update: %s -> %s (slot %c)\n", m.from_version, m.to_version, m.target_slot);
    printf("Product: %s\n", m.product);

    // Verify cryptographic signature
    if (verify_signature(&m, sig, pubkey_path) != 0) {
        fprintf(stderr, "Signature verification failed\n");
        return -1;
    }
    printf("Signature: OK\n");
    return 0;
}

int bb_update_read_manifest(const char *path, bb_update_manifest_t *out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint32_t magic, ver;
    if (fread(&magic, 4, 1, f) != 1 || magic != BB_UPDATE_MAGIC) {
        fclose(f);
        return -1;
    }
    fread(&ver, 4, 1, f);
    fread(out, sizeof(*out), 1, f);
    fclose(f);
    return 0;
}

// ---------------------------------------------------------------------------
// Install (runs on target)
// ---------------------------------------------------------------------------
int bb_update_current_slot(void)
{
    FILE *f = fopen("/proc/cmdline", "r");
    if (!f) return 'a';

    char cmdline[512];
    char slot = 'a';
    if (fgets(cmdline, sizeof(cmdline), f)) {
        char *p = strstr(cmdline, "boot_slot=");
        if (p) slot = p[10];
    }
    fclose(f);
    return slot;
}

void bb_update_current_version(char *buf, size_t len)
{
    FILE *f = fopen("/etc/os-release", "r");
    if (!f) {
        snprintf(buf, len, "unknown");
        return;
    }

    char line[256];
    buf[0] = '\0';
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "BUILDING_BLOCKS_VERSION=", 24) == 0) {
            char *v = line + 24;
            size_t vlen = strlen(v);
            while (vlen > 0 && (v[vlen-1] == '\n' || v[vlen-1] == '"'))
                vlen--;
            snprintf(buf, len, "%.*s", (int)vlen, v);
            break;
        }
    }
    fclose(f);
    if (buf[0] == '\0') snprintf(buf, len, "unknown");
}

int bb_update_install(const char *path)
{
    printf("=== Building Blocks Update ===\n");
    printf("Package: %s\n", path);

    // 1. Verify signature
    if (bb_update_verify(path, NULL) != 0) {
        fprintf(stderr, "Verification failed\n");
        return -1;
    }

    // 2. Read manifest
    bb_update_manifest_t m;
    if (bb_update_read_manifest(path, &m) != 0) {
        fprintf(stderr, "Cannot read manifest\n");
        return -1;
    }

    // 3. Determine target slot
    int current = bb_update_current_slot();
    char target = m.target_slot;
    if (target == '=') {
        target = (current == 'a') ? 'b' : 'a';
    }
    printf("Current slot: %c, Target slot: %c\n", current, target);

    // 4. Determine partition numbers
    char boot_part[32], rootfs_part[32];
    snprintf(boot_part, sizeof(boot_part),
             BB_ROOT_DEV "p%d", (target == 'a') ? BB_PART_BOOT_A : BB_PART_BOOT_B);
    snprintf(rootfs_part, sizeof(rootfs_part),
             BB_ROOT_DEV "p%d", (target == 'a') ? BB_PART_ROOTFS_A : BB_PART_ROOTFS_B);

    // 5. Format target partitions
    printf("Formatting %s ...\n", boot_part);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "mkfs.vfat -F 32 %s 2>/dev/null", boot_part);
    system(cmd);

    printf("Formatting %s ...\n", rootfs_part);
    snprintf(cmd, sizeof(cmd), "mkfs.ext4 -F %s 2>/dev/null", rootfs_part);
    system(cmd);

    // 6. Extract package contents
    printf("Extracting package...\n");

    char tmpdir[] = "/tmp/bb-update-XXXXXX";
    if (!mkdtemp(tmpdir)) {
        fprintf(stderr, "Cannot create temp dir\n");
        return -1;
    }

    // Skip the full header (magic + version + manifest + signature) to reach the tar payload
    snprintf(cmd, sizeof(cmd),
             "tail -c +%zu %s | tar xzf - -C %s 2>/dev/null",
             (size_t)(BB_UPDATE_HEADER_SIZE + 1), path, tmpdir);

    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to extract package\n");
        snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
        system(cmd);
        return -1;
    }

    // 7. Write boot partition
    {
        char boot_tar[256];
        snprintf(boot_tar, sizeof(boot_tar), "%s/boot.tar.gz", tmpdir);

        struct stat st;
        if (stat(boot_tar, &st) == 0) {
            printf("Writing boot partition...\n");
            char mnt[] = "/tmp/bb-boot-XXXXXX";
            if (mkdtemp(mnt)) {
                snprintf(cmd, sizeof(cmd), "mount %s %s", boot_part, mnt);
                system(cmd);
                snprintf(cmd, sizeof(cmd), "tar xzf %s -C %s", boot_tar, mnt);
                system(cmd);
                snprintf(cmd, sizeof(cmd), "umount %s", mnt);
                system(cmd);
                rmdir(mnt);
            }
        }
    }

    // 8. Write rootfs partition
    {
        char rootfs_tar[256];
        snprintf(rootfs_tar, sizeof(rootfs_tar), "%s/rootfs.tar.gz", tmpdir);

        printf("Writing rootfs partition (this may take a while)...\n");
        char mnt[] = "/tmp/bb-rootfs-XXXXXX";
        if (mkdtemp(mnt)) {
            snprintf(cmd, sizeof(cmd), "mount %s %s", rootfs_part, mnt);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "tar xzf %s -C %s", rootfs_tar, mnt);
            system(cmd);

            // 9. Run post-install script (in target rootfs context)
            char postinstall[256];
            snprintf(postinstall, sizeof(postinstall), "%s/post-install.sh", mnt);
            if (access(postinstall, X_OK) == 0) {
                printf("Running post-install script...\n");
                snprintf(cmd, sizeof(cmd), "sh %s", postinstall);
                system(cmd);
            }

            snprintf(cmd, sizeof(cmd), "umount %s", mnt);
            system(cmd);
            rmdir(mnt);
        }
    }

    // Cleanup temp
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);

    // 10. Set U-Boot environment for next boot
    printf("Configuring bootloader for slot %c...\n", target);

    char env_cmd[256];
    snprintf(env_cmd, sizeof(env_cmd),
             "fw_setenv boot_slot %c && "
             "fw_setenv boot_attempt 3 && "
             "fw_setenv boot_ok 0",
             target);
    if (system(env_cmd) != 0) {
        fprintf(stderr, "WARNING: fw_setenv failed -- manual U-Boot env setup required\n");
    }

    // 11. Log update to persist
    {
        FILE *f = fopen("/persist/update-log.json", "a");
        if (f) {
            fprintf(f, "{\"ts\":%lld,\"from\":\"%s\",\"to\":\"%s\",\"slot\":\"%c\",\"ok\":false}\n",
                    (long long)time(NULL), m.from_version, m.to_version, target);
            fclose(f);
        }
    }

    printf("\n========================================\n");
    printf("Update installed to slot %c\n", target);
    printf("Version: %s -> %s\n", m.from_version, m.to_version);
    printf("REBOOT REQUIRED to activate new version.\n");
    printf("Run: systemctl reboot\n");
    printf("========================================\n");

    return 0;
}

// ---------------------------------------------------------------------------
// Create (runs on host build machine)
// ---------------------------------------------------------------------------
int bb_update_create(const char *output,
                     const bb_update_manifest_t *manifest,
                     const char *boot_tar,
                     const char *rootfs_tar,
                     const char *postinstall,
                     const char *privkey_path)
{
    printf("Creating update package: %s\n", output);
    printf("Version: %s -> %s\n", manifest->from_version, manifest->to_version);

    FILE *out = fopen(output, "wb");
    if (!out) {
        perror("fopen");
        return -1;
    }

    // Write header (magic + version + manifest)
    uint32_t magic = BB_UPDATE_MAGIC;
    uint32_t ver = BB_UPDATE_VERSION;
    fwrite(&magic, 4, 1, out);
    fwrite(&ver, 4, 1, out);
    fwrite(manifest, sizeof(*manifest), 1, out);

    // Sign the manifest and write signature
    uint8_t sig[BB_SIG_MAX];
    memset(sig, 0, sizeof(sig));
    if (privkey_path) {
        if (sign_manifest(manifest, privkey_path, sig) != 0) {
            fprintf(stderr, "Signing failed, package not created\n");
            fclose(out);
            unlink(output);
            return -1;
        }
    }
    fwrite(sig, BB_SIG_MAX, 1, out);

    // Close before appending tar payload via shell
    if (fclose(out) != 0) {
        perror("fclose header");
        return -1;
    }

    // Build tar archive of payloads in a staging directory
    char tmpdir[] = "/tmp/bb-update-create-XXXXXX";
    if (!mkdtemp(tmpdir)) {
        return -1;
    }

    char cmd[512];

    if (boot_tar) {
        snprintf(cmd, sizeof(cmd), "cp %s %s/boot.tar.gz", boot_tar, tmpdir);
        if (system(cmd) != 0) {
            fprintf(stderr, "Failed to copy boot.tar.gz\n");
            snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
            system(cmd);
            return -1;
        }
    }

    snprintf(cmd, sizeof(cmd), "cp %s %s/rootfs.tar.gz", rootfs_tar, tmpdir);
    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to copy rootfs.tar.gz\n");
        snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
        system(cmd);
        return -1;
    }

    if (postinstall && access(postinstall, R_OK) == 0) {
        snprintf(cmd, sizeof(cmd), "cp %s %s/post-install.sh", postinstall, tmpdir);
        system(cmd);
    }

    // Append tar archive payload to the output file (header already written and closed)
    snprintf(cmd, sizeof(cmd),
             "tar czf - -C %s . >> %s 2>/dev/null", tmpdir, output);
    int tar_rc = system(cmd);

    // Cleanup staging directory
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);

    if (tar_rc != 0) {
        fprintf(stderr, "Failed to create tar payload\n");
        return -1;
    }

    printf("Created: %s\n", output);
    return 0;
}
